/*
 *    Copyright (c) 2020, The OpenThread Authors.
 *    All rights reserved.
 *
 *    Redistribution and use in source and binary forms, with or without
 *    modification, are permitted provided that the following conditions are met:
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *    3. Neither the name of the copyright holder nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 *    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *    ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *    LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *    CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *    ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *    POSSIBILITY OF SUCH DAMAGE.
 */

#define OTBR_LOG_TAG "UTILS"

#include "utils/thread_helper.hpp"

#include <assert.h>
#include <limits.h>
#include <string.h>
#include <time.h>

#include <string>

#include <openthread/border_router.h>
#include <openthread/channel_manager.h>
#include <openthread/dataset_ftd.h>
#if OTBR_ENABLE_DNSSD_DISCOVERY_PROXY
#include <openthread/dnssd_server.h>
#endif
#include <openthread/jam_detection.h>
#include <openthread/joiner.h>
#if OTBR_ENABLE_NAT64
#include <openthread/crypto.h>
#include <openthread/nat64.h>
#include "utils/sha256.hpp"
#endif
#if OTBR_ENABLE_SRP_ADVERTISING_PROXY
#include <openthread/srp_server.h>
#endif
#include <openthread/thread_ftd.h>
#include <openthread/platform/radio.h>

#include "common/byteswap.hpp"
#include "common/code_utils.hpp"
#include "common/logging.hpp"
#include "common/tlv.hpp"
#include "ncp/ncp_openthread.hpp"

namespace otbr {
namespace agent {
namespace {
const Tlv *FindTlv(uint8_t aTlvType, const uint8_t *aTlvs, int aTlvsSize)
{
    const Tlv *result = nullptr;

    for (const Tlv *tlv = reinterpret_cast<const Tlv *>(aTlvs);
         reinterpret_cast<const uint8_t *>(tlv) + sizeof(Tlv) < aTlvs + aTlvsSize; tlv = tlv->GetNext())
    {
        if (tlv->GetType() == aTlvType)
        {
            ExitNow(result = tlv);
        }
    }

exit:
    return result;
}

#if OTBR_ENABLE_TELEMETRY_DATA_API
static uint32_t TelemetryNodeTypeFromRoleAndLinkMode(const otDeviceRole &aRole, const otLinkModeConfig &aLinkModeCfg)
{
    uint32_t nodeType;

    switch (aRole)
    {
    case OT_DEVICE_ROLE_DISABLED:
        nodeType = threadnetwork::TelemetryData::NODE_TYPE_DISABLED;
        break;
    case OT_DEVICE_ROLE_DETACHED:
        nodeType = threadnetwork::TelemetryData::NODE_TYPE_DETACHED;
        break;
    case OT_DEVICE_ROLE_ROUTER:
        nodeType = threadnetwork::TelemetryData::NODE_TYPE_ROUTER;
        break;
    case OT_DEVICE_ROLE_LEADER:
        nodeType = threadnetwork::TelemetryData::NODE_TYPE_LEADER;
        break;
    case OT_DEVICE_ROLE_CHILD:
        if (!aLinkModeCfg.mRxOnWhenIdle)
        {
            nodeType = threadnetwork::TelemetryData::NODE_TYPE_SLEEPY_END;
        }
        else if (!aLinkModeCfg.mDeviceType)
        {
            // If it's not an FTD, return as minimal end device.
            nodeType = threadnetwork::TelemetryData::NODE_TYPE_MINIMAL_END;
        }
        else
        {
            nodeType = threadnetwork::TelemetryData::NODE_TYPE_END;
        }
        break;
    default:
        nodeType = threadnetwork::TelemetryData::NODE_TYPE_UNSPECIFIED;
    }

    return nodeType;
}

#if OTBR_ENABLE_SRP_ADVERTISING_PROXY
threadnetwork::TelemetryData_SrpServerState SrpServerStateFromOtSrpServerState(otSrpServerState srpServerState)
{
    switch (srpServerState)
    {
    case OT_SRP_SERVER_STATE_DISABLED:
        return threadnetwork::TelemetryData::SRP_SERVER_STATE_DISABLED;
    case OT_SRP_SERVER_STATE_RUNNING:
        return threadnetwork::TelemetryData::SRP_SERVER_STATE_RUNNING;
    case OT_SRP_SERVER_STATE_STOPPED:
        return threadnetwork::TelemetryData::SRP_SERVER_STATE_STOPPED;
    default:
        return threadnetwork::TelemetryData::SRP_SERVER_STATE_UNSPECIFIED;
    }
}

threadnetwork::TelemetryData_SrpServerAddressMode SrpServerAddressModeFromOtSrpServerAddressMode(
    otSrpServerAddressMode srpServerAddressMode)
{
    switch (srpServerAddressMode)
    {
    case OT_SRP_SERVER_ADDRESS_MODE_ANYCAST:
        return threadnetwork::TelemetryData::SRP_SERVER_ADDRESS_MODE_STATE_ANYCAST;
    case OT_SRP_SERVER_ADDRESS_MODE_UNICAST:
        return threadnetwork::TelemetryData::SRP_SERVER_ADDRESS_MODE_UNICAST;
    default:
        return threadnetwork::TelemetryData::SRP_SERVER_ADDRESS_MODE_UNSPECIFIED;
    }
}
#endif // OTBR_ENABLE_SRP_ADVERTISING_PROXY

#if OTBR_ENABLE_NAT64
threadnetwork::TelemetryData_Nat64State Nat64StateFromOtNat64State(otNat64State nat64State)
{
    switch (nat64State)
    {
    case OT_NAT64_STATE_DISABLED:
        return threadnetwork::TelemetryData::NAT64_STATE_DISABLED;
    case OT_NAT64_STATE_NOT_RUNNING:
        return threadnetwork::TelemetryData::NAT64_STATE_NOT_RUNNING;
    case OT_NAT64_STATE_IDLE:
        return threadnetwork::TelemetryData::NAT64_STATE_IDLE;
    case OT_NAT64_STATE_ACTIVE:
        return threadnetwork::TelemetryData::NAT64_STATE_ACTIVE;
    default:
        return threadnetwork::TelemetryData::NAT64_STATE_UNSPECIFIED;
    }
}

void CopyNat64TrafficCounters(const otNat64Counters &from, threadnetwork::TelemetryData_Nat64TrafficCounters *to)
{
    to->set_ipv4_to_ipv6_packets(from.m4To6Packets);
    to->set_ipv4_to_ipv6_bytes(from.m4To6Bytes);
    to->set_ipv6_to_ipv4_packets(from.m6To4Packets);
    to->set_ipv6_to_ipv4_bytes(from.m6To4Bytes);
}
#endif // OTBR_ENABLE_NAT64

void CopyMdnsResponseCounters(const MdnsResponseCounters &from, threadnetwork::TelemetryData_MdnsResponseCounters *to)
{
    to->set_success_count(from.mSuccess);
    to->set_not_found_count(from.mNotFound);
    to->set_invalid_args_count(from.mInvalidArgs);
    to->set_duplicated_count(from.mDuplicated);
    to->set_not_implemented_count(from.mNotImplemented);
    to->set_unknown_error_count(from.mUnknownError);
    to->set_aborted_count(from.mAborted);
    to->set_invalid_state_count(from.mInvalidState);
}
#endif // OTBR_ENABLE_TELEMETRY_DATA_API
} // namespace

ThreadHelper::ThreadHelper(otInstance *aInstance, otbr::Ncp::ControllerOpenThread *aNcp)
    : mInstance(aInstance)
    , mNcp(aNcp)
{
#if OTBR_ENABLE_TELEMETRY_DATA_API && OTBR_ENABLE_NAT64
    otError error;

    SuccessOrExit(error = otPlatCryptoRandomGet(mNat64Ipv6AddressSalt, sizeof(mNat64Ipv6AddressSalt)));

exit:
    if (error != OT_ERROR_NONE)
    {
        otbrLogWarning("Error otPlatCryptoRandomGet: %s", otThreadErrorToString(error));
    }
#endif
}

void ThreadHelper::StateChangedCallback(otChangedFlags aFlags)
{
    if (aFlags & OT_CHANGED_THREAD_ROLE)
    {
        otDeviceRole role = otThreadGetDeviceRole(mInstance);

        for (const auto &handler : mDeviceRoleHandlers)
        {
            handler(role);
        }

        if (role != OT_DEVICE_ROLE_DISABLED && role != OT_DEVICE_ROLE_DETACHED)
        {
            if (mAttachHandler != nullptr)
            {
                if (mWaitingMgmtSetResponse)
                {
                    otbrLogInfo("StateChangedCallback is called during waiting for Mgmt Set Response");
                    ExitNow();
                }
                if (mAttachPendingDatasetTlvs.mLength == 0)
                {
                    AttachHandler handler = mAttachHandler;

                    mAttachHandler = nullptr;
                    handler(OT_ERROR_NONE, mAttachDelayMs);
                }
                else
                {
                    otOperationalDataset emptyDataset = {};
                    otError              error =
                        otDatasetSendMgmtPendingSet(mInstance, &emptyDataset, mAttachPendingDatasetTlvs.mTlvs,
                                                    mAttachPendingDatasetTlvs.mLength, MgmtSetResponseHandler, this);
                    if (error != OT_ERROR_NONE)
                    {
                        AttachHandler handler = mAttachHandler;

                        mAttachHandler            = nullptr;
                        mAttachPendingDatasetTlvs = {};
                        mWaitingMgmtSetResponse   = false;
                        handler(error, 0);
                    }
                    else
                    {
                        mWaitingMgmtSetResponse = true;
                    }
                }
            }
            else if (mJoinerHandler != nullptr)
            {
                mJoinerHandler(OT_ERROR_NONE);
                mJoinerHandler = nullptr;
            }
        }
    }

    if (aFlags & OT_CHANGED_ACTIVE_DATASET)
    {
        ActiveDatasetChangedCallback();
    }

exit:
    return;
}

void ThreadHelper::ActiveDatasetChangedCallback()
{
    otError                  error;
    otOperationalDatasetTlvs datasetTlvs;

    SuccessOrExit(error = otDatasetGetActiveTlvs(mInstance, &datasetTlvs));

    for (const auto &handler : mActiveDatasetChangeHandlers)
    {
        handler(datasetTlvs);
    }

exit:
    if (error != OT_ERROR_NONE)
    {
        otbrLogWarning("Error handling active dataset change: %s", otThreadErrorToString(error));
    }
}

#if OTBR_ENABLE_DBUS_SERVER
void ThreadHelper::OnUpdateMeshCopTxt(std::map<std::string, std::vector<uint8_t>> aUpdate)
{
    if (mUpdateMeshCopTxtHandler)
    {
        mUpdateMeshCopTxtHandler(std::move(aUpdate));
    }
    else
    {
        otbrLogErr("No UpdateMeshCopTxtHandler");
    }
}
#endif

void ThreadHelper::AddDeviceRoleHandler(DeviceRoleHandler aHandler)
{
    mDeviceRoleHandlers.emplace_back(aHandler);
}

void ThreadHelper::Scan(ScanHandler aHandler)
{
    otError error = OT_ERROR_NONE;

    VerifyOrExit(aHandler != nullptr);
    mScanHandler = aHandler;
    mScanResults.clear();

    error =
        otLinkActiveScan(mInstance, /*scanChannels =*/0, /*scanDuration=*/0, &ThreadHelper::ActiveScanHandler, this);

exit:
    if (error != OT_ERROR_NONE)
    {
        if (aHandler)
        {
            mScanHandler(error, {});
        }
        mScanHandler = nullptr;
    }
}

void ThreadHelper::EnergyScan(uint32_t aScanDuration, EnergyScanHandler aHandler)
{
    otError  error             = OT_ERROR_NONE;
    uint32_t preferredChannels = otPlatRadioGetPreferredChannelMask(mInstance);

    VerifyOrExit(aHandler != nullptr, error = OT_ERROR_BUSY);
    VerifyOrExit(aScanDuration < UINT16_MAX, error = OT_ERROR_INVALID_ARGS);
    mEnergyScanHandler = aHandler;
    mEnergyScanResults.clear();

    error = otLinkEnergyScan(mInstance, preferredChannels, static_cast<uint16_t>(aScanDuration),
                             &ThreadHelper::EnergyScanCallback, this);

exit:
    if (error != OT_ERROR_NONE)
    {
        if (aHandler)
        {
            mEnergyScanHandler(error, {});
        }
        mEnergyScanHandler = nullptr;
    }
}

void ThreadHelper::RandomFill(void *aBuf, size_t size)
{
    std::uniform_int_distribution<> dist(0, UINT8_MAX);
    uint8_t                        *buf = static_cast<uint8_t *>(aBuf);

    for (size_t i = 0; i < size; i++)
    {
        buf[i] = static_cast<uint8_t>(dist(mRandomDevice));
    }
}

void ThreadHelper::ActiveScanHandler(otActiveScanResult *aResult, void *aThreadHelper)
{
    ThreadHelper *helper = static_cast<ThreadHelper *>(aThreadHelper);

    helper->ActiveScanHandler(aResult);
}

void ThreadHelper::ActiveScanHandler(otActiveScanResult *aResult)
{
    if (aResult == nullptr)
    {
        if (mScanHandler != nullptr)
        {
            mScanHandler(OT_ERROR_NONE, mScanResults);
        }
    }
    else
    {
        mScanResults.push_back(*aResult);
    }
}

void ThreadHelper::EnergyScanCallback(otEnergyScanResult *aResult, void *aThreadHelper)
{
    ThreadHelper *helper = static_cast<ThreadHelper *>(aThreadHelper);

    helper->EnergyScanCallback(aResult);
}

void ThreadHelper::EnergyScanCallback(otEnergyScanResult *aResult)
{
    if (aResult == nullptr)
    {
        if (mEnergyScanHandler != nullptr)
        {
            mEnergyScanHandler(OT_ERROR_NONE, mEnergyScanResults);
        }
    }
    else
    {
        mEnergyScanResults.push_back(*aResult);
    }
}

uint8_t ThreadHelper::RandomChannelFromChannelMask(uint32_t aChannelMask)
{
    // 8 bit per byte
    constexpr uint8_t kNumChannels = sizeof(aChannelMask) * 8;
    uint8_t           channels[kNumChannels];
    uint8_t           numValidChannels = 0;

    for (uint8_t i = 0; i < kNumChannels; i++)
    {
        if (aChannelMask & (1 << i))
        {
            channels[numValidChannels++] = i;
        }
    }

    return channels[std::uniform_int_distribution<unsigned int>(0, numValidChannels - 1)(mRandomDevice)];
}

static otExtendedPanId ToOtExtendedPanId(uint64_t aExtPanId)
{
    otExtendedPanId extPanId;
    uint64_t        mask = UINT8_MAX;

    for (size_t i = 0; i < sizeof(uint64_t); i++)
    {
        extPanId.m8[i] = static_cast<uint8_t>((aExtPanId >> ((sizeof(uint64_t) - i - 1) * 8)) & mask);
    }

    return extPanId;
}

void ThreadHelper::Attach(const std::string          &aNetworkName,
                          uint16_t                    aPanId,
                          uint64_t                    aExtPanId,
                          const std::vector<uint8_t> &aNetworkKey,
                          const std::vector<uint8_t> &aPSKc,
                          uint32_t                    aChannelMask,
                          AttachHandler               aHandler)

{
    otError              error   = OT_ERROR_NONE;
    otOperationalDataset dataset = {};

    VerifyOrExit(aHandler != nullptr, error = OT_ERROR_INVALID_ARGS);
    VerifyOrExit(mAttachHandler == nullptr && mJoinerHandler == nullptr, error = OT_ERROR_INVALID_STATE);
    VerifyOrExit(aNetworkKey.empty() || aNetworkKey.size() == sizeof(dataset.mNetworkKey.m8),
                 error = OT_ERROR_INVALID_ARGS);
    VerifyOrExit(aPSKc.empty() || aPSKc.size() == sizeof(dataset.mPskc.m8), error = OT_ERROR_INVALID_ARGS);
    VerifyOrExit(aChannelMask != 0, error = OT_ERROR_INVALID_ARGS);

    SuccessOrExit(error = otDatasetCreateNewNetwork(mInstance, &dataset));

    if (aExtPanId != UINT64_MAX)
    {
        dataset.mExtendedPanId = ToOtExtendedPanId(aExtPanId);
    }

    if (!aNetworkKey.empty())
    {
        memcpy(dataset.mNetworkKey.m8, &aNetworkKey[0], sizeof(dataset.mNetworkKey.m8));
    }

    if (aPanId != UINT16_MAX)
    {
        dataset.mPanId = aPanId;
    }

    if (!aPSKc.empty())
    {
        memcpy(dataset.mPskc.m8, &aPSKc[0], sizeof(dataset.mPskc.m8));
    }

    SuccessOrExit(error = otNetworkNameFromString(&dataset.mNetworkName, aNetworkName.c_str()));

    dataset.mChannelMask &= aChannelMask;
    VerifyOrExit(dataset.mChannelMask != 0, otbrLogWarning("Invalid channel mask"), error = OT_ERROR_INVALID_ARGS);

    dataset.mChannel = RandomChannelFromChannelMask(dataset.mChannelMask);

    SuccessOrExit(error = otDatasetSetActive(mInstance, &dataset));

    if (!otIp6IsEnabled(mInstance))
    {
        SuccessOrExit(error = otIp6SetEnabled(mInstance, true));
    }

    SuccessOrExit(error = otThreadSetEnabled(mInstance, true));
    mAttachDelayMs = 0;
    mAttachHandler = aHandler;

exit:
    if (error != OT_ERROR_NONE)
    {
        if (aHandler)
        {
            aHandler(error, 0);
        }
    }
}

void ThreadHelper::Attach(AttachHandler aHandler)
{
    otError error = OT_ERROR_NONE;

    VerifyOrExit(mAttachHandler == nullptr && mJoinerHandler == nullptr, error = OT_ERROR_INVALID_STATE);

    if (!otIp6IsEnabled(mInstance))
    {
        SuccessOrExit(error = otIp6SetEnabled(mInstance, true));
    }
    SuccessOrExit(error = otThreadSetEnabled(mInstance, true));
    mAttachHandler = aHandler;

exit:
    if (error != OT_ERROR_NONE)
    {
        if (aHandler)
        {
            aHandler(error, 0);
        }
    }
}

otError ThreadHelper::Detach(void)
{
    otError error = OT_ERROR_NONE;

    SuccessOrExit(error = otThreadSetEnabled(mInstance, false));
    SuccessOrExit(error = otIp6SetEnabled(mInstance, false));

exit:
    return error;
}

otError ThreadHelper::Reset(void)
{
    mDeviceRoleHandlers.clear();
    otInstanceReset(mInstance);

    return OT_ERROR_NONE;
}

void ThreadHelper::JoinerStart(const std::string &aPskd,
                               const std::string &aProvisioningUrl,
                               const std::string &aVendorName,
                               const std::string &aVendorModel,
                               const std::string &aVendorSwVersion,
                               const std::string &aVendorData,
                               ResultHandler      aHandler)
{
    otError error = OT_ERROR_NONE;

    VerifyOrExit(aHandler != nullptr, error = OT_ERROR_INVALID_ARGS);
    VerifyOrExit(mAttachHandler == nullptr && mJoinerHandler == nullptr, error = OT_ERROR_INVALID_STATE);

    if (!otIp6IsEnabled(mInstance))
    {
        SuccessOrExit(error = otIp6SetEnabled(mInstance, true));
    }
    SuccessOrExit(error = otJoinerStart(mInstance, aPskd.c_str(), aProvisioningUrl.c_str(), aVendorName.c_str(),
                                        aVendorModel.c_str(), aVendorSwVersion.c_str(), aVendorData.c_str(),
                                        JoinerCallback, this));
    mJoinerHandler = aHandler;

exit:
    if (error != OT_ERROR_NONE)
    {
        if (aHandler)
        {
            aHandler(error);
        }
    }
}

void ThreadHelper::JoinerCallback(otError aError, void *aThreadHelper)
{
    ThreadHelper *helper = static_cast<ThreadHelper *>(aThreadHelper);

    helper->JoinerCallback(aError);
}

void ThreadHelper::JoinerCallback(otError aError)
{
    if (aError != OT_ERROR_NONE)
    {
        otbrLogWarning("Failed to join Thread network: %s", otThreadErrorToString(aError));
        mJoinerHandler(aError);
        mJoinerHandler = nullptr;
    }
    else
    {
        LogOpenThreadResult("Start Thread network", otThreadSetEnabled(mInstance, true));
    }
}

otError ThreadHelper::TryResumeNetwork(void)
{
    otError error = OT_ERROR_NONE;

    if (otLinkGetPanId(mInstance) != UINT16_MAX && otThreadGetDeviceRole(mInstance) == OT_DEVICE_ROLE_DISABLED)
    {
        if (!otIp6IsEnabled(mInstance))
        {
            SuccessOrExit(error = otIp6SetEnabled(mInstance, true));
            SuccessOrExit(error = otThreadSetEnabled(mInstance, true));
        }
    }

exit:
    if (error != OT_ERROR_NONE)
    {
        (void)otIp6SetEnabled(mInstance, false);
    }

    return error;
}

void ThreadHelper::LogOpenThreadResult(const char *aAction, otError aError)
{
    if (aError == OT_ERROR_NONE)
    {
        otbrLogInfo("%s: %s", aAction, otThreadErrorToString(aError));
    }
    else
    {
        otbrLogWarning("%s: %s", aAction, otThreadErrorToString(aError));
    }
}

void ThreadHelper::AttachAllNodesTo(const std::vector<uint8_t> &aDatasetTlvs, AttachHandler aHandler)
{
    constexpr uint32_t kDelayTimerMilliseconds = 300 * 1000;

    otError                  error = OT_ERROR_NONE;
    otOperationalDatasetTlvs datasetTlvs;
    otOperationalDataset     dataset;
    otOperationalDataset     emptyDataset{};
    otDeviceRole             role = otThreadGetDeviceRole(mInstance);
    Tlv                     *tlv;
    uint64_t                 pendingTimestamp = 0;
    timespec                 currentTime;

    if (aHandler == nullptr)
    {
        otbrLogWarning("Attach Handler is nullptr");
        ExitNow(error = OT_ERROR_INVALID_ARGS);
    }
    VerifyOrExit(mAttachHandler == nullptr && mJoinerHandler == nullptr, error = OT_ERROR_BUSY);

    VerifyOrExit(aDatasetTlvs.size() <= sizeof(datasetTlvs.mTlvs), error = OT_ERROR_INVALID_ARGS);
    std::copy(aDatasetTlvs.begin(), aDatasetTlvs.end(), datasetTlvs.mTlvs);
    datasetTlvs.mLength = aDatasetTlvs.size();

    SuccessOrExit(error = otDatasetParseTlvs(&datasetTlvs, &dataset));
    VerifyOrExit(dataset.mComponents.mIsActiveTimestampPresent, error = OT_ERROR_INVALID_ARGS);
    VerifyOrExit(dataset.mComponents.mIsNetworkKeyPresent, error = OT_ERROR_INVALID_ARGS);
    VerifyOrExit(dataset.mComponents.mIsNetworkNamePresent, error = OT_ERROR_INVALID_ARGS);
    VerifyOrExit(dataset.mComponents.mIsExtendedPanIdPresent, error = OT_ERROR_INVALID_ARGS);
    VerifyOrExit(dataset.mComponents.mIsMeshLocalPrefixPresent, error = OT_ERROR_INVALID_ARGS);
    VerifyOrExit(dataset.mComponents.mIsPanIdPresent, error = OT_ERROR_INVALID_ARGS);
    VerifyOrExit(dataset.mComponents.mIsChannelPresent, error = OT_ERROR_INVALID_ARGS);
    VerifyOrExit(dataset.mComponents.mIsPskcPresent, error = OT_ERROR_INVALID_ARGS);
    VerifyOrExit(dataset.mComponents.mIsSecurityPolicyPresent, error = OT_ERROR_INVALID_ARGS);
    VerifyOrExit(dataset.mComponents.mIsChannelMaskPresent, error = OT_ERROR_INVALID_ARGS);

    VerifyOrExit(FindTlv(OT_MESHCOP_TLV_PENDINGTIMESTAMP, datasetTlvs.mTlvs, datasetTlvs.mLength) == nullptr &&
                     FindTlv(OT_MESHCOP_TLV_DELAYTIMER, datasetTlvs.mTlvs, datasetTlvs.mLength) == nullptr,
                 error = OT_ERROR_INVALID_ARGS);

    // There must be sufficient space for a Pending Timestamp TLV and a Delay Timer TLV.
    VerifyOrExit(
        static_cast<int>(datasetTlvs.mLength +
                         (sizeof(uint8_t) + sizeof(uint8_t) + sizeof(uint64_t))    // Pending Timestamp TLV (10 bytes)
                         + (sizeof(uint8_t) + sizeof(uint8_t) + sizeof(uint32_t))) // Delay Timer TLV (6 bytes)
            <= int{sizeof(datasetTlvs.mTlvs)},
        error = OT_ERROR_INVALID_ARGS);

    tlv = reinterpret_cast<Tlv *>(datasetTlvs.mTlvs + datasetTlvs.mLength);
    tlv->SetType(OT_MESHCOP_TLV_PENDINGTIMESTAMP);
    clock_gettime(CLOCK_REALTIME, &currentTime);
    pendingTimestamp |= (static_cast<uint64_t>(currentTime.tv_sec) << 16);
    pendingTimestamp |= (((static_cast<uint64_t>(currentTime.tv_nsec) * 32768 / 1000000000) & 0x7fff) << 1);
    tlv->SetValue(pendingTimestamp);

    tlv = tlv->GetNext();
    tlv->SetType(OT_MESHCOP_TLV_DELAYTIMER);
    tlv->SetValue(kDelayTimerMilliseconds);

    datasetTlvs.mLength = reinterpret_cast<uint8_t *>(tlv->GetNext()) - datasetTlvs.mTlvs;

    assert(datasetTlvs.mLength > 0);

    if (role == OT_DEVICE_ROLE_DISABLED || role == OT_DEVICE_ROLE_DETACHED)
    {
        otOperationalDataset existingDataset;
        bool                 hasActiveDataset;

        error = otDatasetGetActive(mInstance, &existingDataset);
        VerifyOrExit(error == OT_ERROR_NONE || error == OT_ERROR_NOT_FOUND);

        hasActiveDataset = (error == OT_ERROR_NONE);

        if (!hasActiveDataset)
        {
            SuccessOrExit(error = otDatasetSetActiveTlvs(mInstance, &datasetTlvs));
        }

        if (!otIp6IsEnabled(mInstance))
        {
            SuccessOrExit(error = otIp6SetEnabled(mInstance, true));
        }
        SuccessOrExit(error = otThreadSetEnabled(mInstance, true));

        if (hasActiveDataset)
        {
            mAttachDelayMs            = kDelayTimerMilliseconds;
            mAttachPendingDatasetTlvs = datasetTlvs;
        }
        else
        {
            mAttachDelayMs            = 0;
            mAttachPendingDatasetTlvs = {};
        }
        mWaitingMgmtSetResponse = false;
        mAttachHandler          = aHandler;
        ExitNow();
    }

    SuccessOrExit(error = otDatasetSendMgmtPendingSet(mInstance, &emptyDataset, datasetTlvs.mTlvs, datasetTlvs.mLength,
                                                      MgmtSetResponseHandler, this));
    mAttachDelayMs          = kDelayTimerMilliseconds;
    mAttachHandler          = aHandler;
    mWaitingMgmtSetResponse = true;

exit:
    if (error != OT_ERROR_NONE)
    {
        aHandler(error, 0);
    }
}

void ThreadHelper::MgmtSetResponseHandler(otError aResult, void *aContext)
{
    static_cast<ThreadHelper *>(aContext)->MgmtSetResponseHandler(aResult);
}

void ThreadHelper::MgmtSetResponseHandler(otError aResult)
{
    AttachHandler handler;
    int64_t       attachDelayMs;

    LogOpenThreadResult("MgmtSetResponseHandler()", aResult);
    mWaitingMgmtSetResponse = false;

    if (mAttachHandler == nullptr)
    {
        otbrLogWarning("mAttachHandler is nullptr");
        mAttachDelayMs            = 0;
        mAttachPendingDatasetTlvs = {};
        ExitNow();
    }

    switch (aResult)
    {
    case OT_ERROR_NONE:
    case OT_ERROR_REJECTED:
        break;
    default:
        aResult = OT_ERROR_FAILED;
        break;
    }

    attachDelayMs             = mAttachDelayMs;
    handler                   = mAttachHandler;
    mAttachDelayMs            = 0;
    mAttachHandler            = nullptr;
    mAttachPendingDatasetTlvs = {};
    if (aResult == OT_ERROR_NONE)
    {
        handler(aResult, attachDelayMs);
    }
    else
    {
        handler(aResult, 0);
    }

exit:
    return;
}

#if OTBR_ENABLE_UNSECURE_JOIN
otError ThreadHelper::PermitUnsecureJoin(uint16_t aPort, uint32_t aSeconds)
{
    otError      error = OT_ERROR_NONE;
    otExtAddress steeringData;

    // 0xff to allow all devices to join
    memset(&steeringData.m8, 0xff, sizeof(steeringData.m8));
    SuccessOrExit(error = otIp6AddUnsecurePort(mInstance, aPort));
    otThreadSetSteeringData(mInstance, &steeringData);

    if (aSeconds > 0)
    {
        auto delay = Milliseconds(aSeconds * 1000);

        ++mUnsecurePortRefCounter[aPort];

        mNcp->PostTimerTask(delay, [this, aPort]() {
            assert(mUnsecurePortRefCounter.find(aPort) != mUnsecurePortRefCounter.end());
            assert(mUnsecurePortRefCounter[aPort] > 0);

            if (--mUnsecurePortRefCounter[aPort] == 0)
            {
                otExtAddress noneAddress;

                // 0 to clean steering data
                memset(&noneAddress.m8, 0, sizeof(noneAddress.m8));
                (void)otIp6RemoveUnsecurePort(mInstance, aPort);
                otThreadSetSteeringData(mInstance, &noneAddress);
                mUnsecurePortRefCounter.erase(aPort);
            }
        });
    }
    else
    {
        otExtAddress noneAddress;

        memset(&noneAddress.m8, 0, sizeof(noneAddress.m8));
        (void)otIp6RemoveUnsecurePort(mInstance, aPort);
        otThreadSetSteeringData(mInstance, &noneAddress);
    }

exit:
    return error;
}
#endif

void ThreadHelper::AddActiveDatasetChangeHandler(DatasetChangeHandler aHandler)
{
    mActiveDatasetChangeHandlers.push_back(std::move(aHandler));
}

void ThreadHelper::DetachGracefully(ResultHandler aHandler)
{
    otError error = OT_ERROR_NONE;

    VerifyOrExit(mDetachGracefullyHandler == nullptr, error = OT_ERROR_BUSY);

    SuccessOrExit(error = otThreadDetachGracefully(mInstance, &ThreadHelper::DetachGracefullyCallback, this));
    mDetachGracefullyHandler = aHandler;

exit:
    if (error != OT_ERROR_NONE)
    {
        aHandler(error);
    }
}

void ThreadHelper::DetachGracefullyCallback(void *aContext)
{
    static_cast<ThreadHelper *>(aContext)->DetachGracefullyCallback();
}

void ThreadHelper::DetachGracefullyCallback(void)
{
    if (mDetachGracefullyHandler != nullptr)
    {
        mDetachGracefullyHandler(OT_ERROR_NONE);
    }
}

#if OTBR_ENABLE_TELEMETRY_DATA_API
otError ThreadHelper::RetrieveTelemetryData(Mdns::Publisher *aPublisher, threadnetwork::TelemetryData &telemetryData)
{
    otError error = OT_ERROR_NONE;

    // Begin of WpanStats section.
    auto wpanStats = telemetryData.mutable_wpan_stats();

    {
        otDeviceRole     role  = otThreadGetDeviceRole(mInstance);
        otLinkModeConfig otCfg = otThreadGetLinkMode(mInstance);

        wpanStats->set_node_type(TelemetryNodeTypeFromRoleAndLinkMode(role, otCfg));
    }

    wpanStats->set_channel(otLinkGetChannel(mInstance));

    {
        uint16_t ccaFailureRate = otLinkGetCcaFailureRate(mInstance);

        wpanStats->set_mac_cca_fail_rate(static_cast<float>(ccaFailureRate) / 0xffff);
    }

    {
        int8_t radioTxPower;

        SuccessOrExit(error = otPlatRadioGetTransmitPower(mInstance, &radioTxPower));
        wpanStats->set_radio_tx_power(radioTxPower);
    }

    {
        const otMacCounters *linkCounters = otLinkGetCounters(mInstance);

        wpanStats->set_phy_rx(linkCounters->mRxTotal);
        wpanStats->set_phy_tx(linkCounters->mTxTotal);
        wpanStats->set_mac_unicast_rx(linkCounters->mRxUnicast);
        wpanStats->set_mac_unicast_tx(linkCounters->mTxUnicast);
        wpanStats->set_mac_broadcast_rx(linkCounters->mRxBroadcast);
        wpanStats->set_mac_broadcast_tx(linkCounters->mTxBroadcast);
        wpanStats->set_mac_tx_ack_req(linkCounters->mTxAckRequested);
        wpanStats->set_mac_tx_no_ack_req(linkCounters->mTxNoAckRequested);
        wpanStats->set_mac_tx_acked(linkCounters->mTxAcked);
        wpanStats->set_mac_tx_data(linkCounters->mTxData);
        wpanStats->set_mac_tx_data_poll(linkCounters->mTxDataPoll);
        wpanStats->set_mac_tx_beacon(linkCounters->mTxBeacon);
        wpanStats->set_mac_tx_beacon_req(linkCounters->mTxBeaconRequest);
        wpanStats->set_mac_tx_other_pkt(linkCounters->mTxOther);
        wpanStats->set_mac_tx_retry(linkCounters->mTxRetry);
        wpanStats->set_mac_rx_data(linkCounters->mRxData);
        wpanStats->set_mac_rx_data_poll(linkCounters->mRxDataPoll);
        wpanStats->set_mac_rx_beacon(linkCounters->mRxBeacon);
        wpanStats->set_mac_rx_beacon_req(linkCounters->mRxBeaconRequest);
        wpanStats->set_mac_rx_other_pkt(linkCounters->mRxOther);
        wpanStats->set_mac_rx_filter_whitelist(linkCounters->mRxAddressFiltered);
        wpanStats->set_mac_rx_filter_dest_addr(linkCounters->mRxDestAddrFiltered);
        wpanStats->set_mac_tx_fail_cca(linkCounters->mTxErrCca);
        wpanStats->set_mac_rx_fail_decrypt(linkCounters->mRxErrSec);
        wpanStats->set_mac_rx_fail_no_frame(linkCounters->mRxErrNoFrame);
        wpanStats->set_mac_rx_fail_unknown_neighbor(linkCounters->mRxErrUnknownNeighbor);
        wpanStats->set_mac_rx_fail_invalid_src_addr(linkCounters->mRxErrInvalidSrcAddr);
        wpanStats->set_mac_rx_fail_fcs(linkCounters->mRxErrFcs);
        wpanStats->set_mac_rx_fail_other(linkCounters->mRxErrOther);
    }

    {
        const otIpCounters *ipCounters = otThreadGetIp6Counters(mInstance);

        wpanStats->set_ip_tx_success(ipCounters->mTxSuccess);
        wpanStats->set_ip_rx_success(ipCounters->mRxSuccess);
        wpanStats->set_ip_tx_failure(ipCounters->mTxFailure);
        wpanStats->set_ip_rx_failure(ipCounters->mRxFailure);
    }
    // End of WpanStats section.

    {
        // Begin of WpanTopoFull section.
        auto     wpanTopoFull = telemetryData.mutable_wpan_topo_full();
        uint16_t rloc16       = otThreadGetRloc16(mInstance);

        wpanTopoFull->set_rloc16(rloc16);

        otRouterInfo info;

        VerifyOrExit(otThreadGetRouterInfo(mInstance, rloc16, &info) == OT_ERROR_NONE, error = OT_ERROR_INVALID_STATE);
        wpanTopoFull->set_router_id(info.mRouterId);

        otNeighborInfoIterator      iter = OT_NEIGHBOR_INFO_ITERATOR_INIT;
        otNeighborInfo              neighborInfo;
        std::vector<otNeighborInfo> neighborTable;

        while (otThreadGetNextNeighborInfo(mInstance, &iter, &neighborInfo) == OT_ERROR_NONE)
        {
            neighborTable.push_back(neighborInfo);
        }
        wpanTopoFull->set_neighbor_table_size(neighborTable.size());

        uint16_t                 childIndex = 0;
        otChildInfo              childInfo;
        std::vector<otChildInfo> childTable;

        while (otThreadGetChildInfoByIndex(mInstance, childIndex, &childInfo) == OT_ERROR_NONE)
        {
            childTable.push_back(childInfo);
            childIndex++;
        }
        wpanTopoFull->set_child_table_size(childTable.size());

        struct otLeaderData leaderData;

        SuccessOrExit(error = otThreadGetLeaderData(mInstance, &leaderData));
        wpanTopoFull->set_leader_router_id(leaderData.mLeaderRouterId);
        wpanTopoFull->set_leader_weight(leaderData.mWeighting);
        wpanTopoFull->set_network_data_version(leaderData.mDataVersion);
        wpanTopoFull->set_stable_network_data_version(leaderData.mStableDataVersion);

        uint8_t weight = otThreadGetLocalLeaderWeight(mInstance);

        wpanTopoFull->set_leader_local_weight(weight);

        uint32_t partitionId = otThreadGetPartitionId(mInstance);

        wpanTopoFull->set_partition_id(partitionId);

        static constexpr size_t kNetworkDataMaxSize = 255;
        {
            uint8_t              data[kNetworkDataMaxSize];
            uint8_t              len = sizeof(data);
            std::vector<uint8_t> networkData;

            SuccessOrExit(error = otNetDataGet(mInstance, /*stable=*/false, data, &len));
            networkData = std::vector<uint8_t>(&data[0], &data[len]);
            wpanTopoFull->set_network_data(std::string(networkData.begin(), networkData.end()));
        }

        {
            uint8_t              data[kNetworkDataMaxSize];
            uint8_t              len = sizeof(data);
            std::vector<uint8_t> networkData;

            SuccessOrExit(error = otNetDataGet(mInstance, /*stable=*/true, data, &len));
            networkData = std::vector<uint8_t>(&data[0], &data[len]);
            wpanTopoFull->set_stable_network_data(std::string(networkData.begin(), networkData.end()));
        }

        int8_t rssi = otPlatRadioGetRssi(mInstance);

        wpanTopoFull->set_instant_rssi(rssi);

        const otExtendedPanId *extPanId = otThreadGetExtendedPanId(mInstance);
        uint64_t               extPanIdVal;

        extPanIdVal = ConvertOpenThreadUint64(extPanId->m8);
        wpanTopoFull->set_extended_pan_id(extPanIdVal);
        // End of WpanTopoFull section.

        // Begin of TopoEntry section.
        std::map<uint16_t, const otChildInfo *> childMap;

        for (const otChildInfo &childInfo : childTable)
        {
            auto pair = childMap.insert({childInfo.mRloc16, &childInfo});
            if (!pair.second)
            {
                // This shouldn't happen, so log an error. It doesn't matter which
                // duplicate is kept.
                otbrLogErr("Children with duplicate RLOC16 found: 0x%04x", static_cast<int>(childInfo.mRloc16));
            }
        }

        for (const otNeighborInfo &neighborInfo : neighborTable)
        {
            auto topoEntry = telemetryData.add_topo_entries();
            topoEntry->set_rloc16(neighborInfo.mRloc16);
            topoEntry->mutable_age()->set_seconds(neighborInfo.mAge);
            topoEntry->set_link_quality_in(neighborInfo.mLinkQualityIn);
            topoEntry->set_average_rssi(neighborInfo.mAverageRssi);
            topoEntry->set_last_rssi(neighborInfo.mLastRssi);
            topoEntry->set_link_frame_counter(neighborInfo.mLinkFrameCounter);
            topoEntry->set_mle_frame_counter(neighborInfo.mMleFrameCounter);
            topoEntry->set_rx_on_when_idle(neighborInfo.mRxOnWhenIdle);
            topoEntry->set_secure_data_request(true);
            topoEntry->set_full_function(neighborInfo.mFullThreadDevice);
            topoEntry->set_full_network_data(neighborInfo.mFullNetworkData);
            topoEntry->set_mac_frame_error_rate(static_cast<float>(neighborInfo.mFrameErrorRate) / 0xffff);
            topoEntry->set_ip_message_error_rate(static_cast<float>(neighborInfo.mMessageErrorRate) / 0xffff);
            topoEntry->set_version(neighborInfo.mVersion);

            if (!neighborInfo.mIsChild)
            {
                continue;
            }

            auto it = childMap.find(neighborInfo.mRloc16);
            if (it == childMap.end())
            {
                otbrLogErr("Neighbor 0x%04x not found in child table", static_cast<int>(neighborInfo.mRloc16));
                continue;
            }
            const otChildInfo *childInfo = it->second;
            topoEntry->set_is_child(true);
            topoEntry->mutable_timeout()->set_seconds(childInfo->mTimeout);
            topoEntry->set_network_data_version(childInfo->mNetworkDataVersion);
        }
        // End of TopoEntry section.
    }

    {
        // Begin of WpanBorderRouter section.
        auto wpanBorderRouter = telemetryData.mutable_wpan_border_router();
        // Begin of BorderRoutingCounters section.
        auto                           borderRoutingCouters    = wpanBorderRouter->mutable_border_routing_counters();
        const otBorderRoutingCounters *otBorderRoutingCounters = otIp6GetBorderRoutingCounters(mInstance);

        borderRoutingCouters->mutable_inbound_unicast()->set_packet_count(
            otBorderRoutingCounters->mInboundUnicast.mPackets);
        borderRoutingCouters->mutable_inbound_unicast()->set_byte_count(
            otBorderRoutingCounters->mInboundUnicast.mBytes);
        borderRoutingCouters->mutable_inbound_multicast()->set_packet_count(
            otBorderRoutingCounters->mInboundMulticast.mPackets);
        borderRoutingCouters->mutable_inbound_multicast()->set_byte_count(
            otBorderRoutingCounters->mInboundMulticast.mBytes);
        borderRoutingCouters->mutable_outbound_unicast()->set_packet_count(
            otBorderRoutingCounters->mOutboundUnicast.mPackets);
        borderRoutingCouters->mutable_outbound_unicast()->set_byte_count(
            otBorderRoutingCounters->mOutboundUnicast.mBytes);
        borderRoutingCouters->mutable_outbound_multicast()->set_packet_count(
            otBorderRoutingCounters->mOutboundMulticast.mPackets);
        borderRoutingCouters->mutable_outbound_multicast()->set_byte_count(
            otBorderRoutingCounters->mOutboundMulticast.mBytes);
        borderRoutingCouters->set_ra_rx(otBorderRoutingCounters->mRaRx);
        borderRoutingCouters->set_ra_tx_success(otBorderRoutingCounters->mRaTxSuccess);
        borderRoutingCouters->set_ra_tx_failure(otBorderRoutingCounters->mRaTxFailure);
        borderRoutingCouters->set_rs_rx(otBorderRoutingCounters->mRsRx);
        borderRoutingCouters->set_rs_tx_success(otBorderRoutingCounters->mRsTxSuccess);
        borderRoutingCouters->set_rs_tx_failure(otBorderRoutingCounters->mRsTxFailure);

#if OTBR_ENABLE_NAT64
        {
            auto nat64IcmpCounters = borderRoutingCouters->mutable_nat64_protocol_counters()->mutable_icmp();
            auto nat64UdpCounters  = borderRoutingCouters->mutable_nat64_protocol_counters()->mutable_udp();
            auto nat64TcpCounters  = borderRoutingCouters->mutable_nat64_protocol_counters()->mutable_tcp();
            otNat64ProtocolCounters otCounters;

            otNat64GetCounters(mInstance, &otCounters);
            nat64IcmpCounters->set_ipv4_to_ipv6_packets(otCounters.mIcmp.m4To6Packets);
            nat64IcmpCounters->set_ipv4_to_ipv6_bytes(otCounters.mIcmp.m4To6Bytes);
            nat64IcmpCounters->set_ipv6_to_ipv4_packets(otCounters.mIcmp.m6To4Packets);
            nat64IcmpCounters->set_ipv6_to_ipv4_bytes(otCounters.mIcmp.m6To4Bytes);
            nat64UdpCounters->set_ipv4_to_ipv6_packets(otCounters.mUdp.m4To6Packets);
            nat64UdpCounters->set_ipv4_to_ipv6_bytes(otCounters.mUdp.m4To6Bytes);
            nat64UdpCounters->set_ipv6_to_ipv4_packets(otCounters.mUdp.m6To4Packets);
            nat64UdpCounters->set_ipv6_to_ipv4_bytes(otCounters.mUdp.m6To4Bytes);
            nat64TcpCounters->set_ipv4_to_ipv6_packets(otCounters.mTcp.m4To6Packets);
            nat64TcpCounters->set_ipv4_to_ipv6_bytes(otCounters.mTcp.m4To6Bytes);
            nat64TcpCounters->set_ipv6_to_ipv4_packets(otCounters.mTcp.m6To4Packets);
            nat64TcpCounters->set_ipv6_to_ipv4_bytes(otCounters.mTcp.m6To4Bytes);
        }

        {
            auto                 errorCounters = borderRoutingCouters->mutable_nat64_error_counters();
            otNat64ErrorCounters otCounters;
            otNat64GetErrorCounters(mInstance, &otCounters);

            errorCounters->mutable_unknown()->set_ipv4_to_ipv6_packets(
                otCounters.mCount4To6[OT_NAT64_DROP_REASON_UNKNOWN]);
            errorCounters->mutable_unknown()->set_ipv6_to_ipv4_packets(
                otCounters.mCount6To4[OT_NAT64_DROP_REASON_UNKNOWN]);
            errorCounters->mutable_illegal_packet()->set_ipv4_to_ipv6_packets(
                otCounters.mCount4To6[OT_NAT64_DROP_REASON_ILLEGAL_PACKET]);
            errorCounters->mutable_illegal_packet()->set_ipv6_to_ipv4_packets(
                otCounters.mCount6To4[OT_NAT64_DROP_REASON_ILLEGAL_PACKET]);
            errorCounters->mutable_unsupported_protocol()->set_ipv4_to_ipv6_packets(
                otCounters.mCount4To6[OT_NAT64_DROP_REASON_UNSUPPORTED_PROTO]);
            errorCounters->mutable_unsupported_protocol()->set_ipv6_to_ipv4_packets(
                otCounters.mCount6To4[OT_NAT64_DROP_REASON_UNSUPPORTED_PROTO]);
            errorCounters->mutable_no_mapping()->set_ipv4_to_ipv6_packets(
                otCounters.mCount4To6[OT_NAT64_DROP_REASON_NO_MAPPING]);
            errorCounters->mutable_no_mapping()->set_ipv6_to_ipv4_packets(
                otCounters.mCount6To4[OT_NAT64_DROP_REASON_NO_MAPPING]);
        }
#endif // OTBR_ENABLE_NAT64
       // End of BorderRoutingCounters section.

#if OTBR_ENABLE_SRP_ADVERTISING_PROXY
        // Begin of SrpServerInfo section.
        {
            auto                               srpServer = wpanBorderRouter->mutable_srp_server();
            otSrpServerLeaseInfo               leaseInfo;
            const otSrpServerHost             *host             = nullptr;
            const otSrpServerResponseCounters *responseCounters = otSrpServerGetResponseCounters(mInstance);

            srpServer->set_state(SrpServerStateFromOtSrpServerState(otSrpServerGetState(mInstance)));
            srpServer->set_port(otSrpServerGetPort(mInstance));
            srpServer->set_address_mode(
                SrpServerAddressModeFromOtSrpServerAddressMode(otSrpServerGetAddressMode(mInstance)));

            auto srpServerHosts            = srpServer->mutable_hosts();
            auto srpServerServices         = srpServer->mutable_services();
            auto srpServerResponseCounters = srpServer->mutable_response_counters();

            while ((host = otSrpServerGetNextHost(mInstance, host)))
            {
                const otSrpServerService *service = nullptr;

                if (otSrpServerHostIsDeleted(host))
                {
                    srpServerHosts->set_deleted_count(srpServerHosts->deleted_count() + 1);
                }
                else
                {
                    srpServerHosts->set_fresh_count(srpServerHosts->fresh_count() + 1);
                    otSrpServerHostGetLeaseInfo(host, &leaseInfo);
                    srpServerHosts->set_lease_time_total_ms(srpServerHosts->lease_time_total_ms() + leaseInfo.mLease);
                    srpServerHosts->set_key_lease_time_total_ms(srpServerHosts->key_lease_time_total_ms() +
                                                                leaseInfo.mKeyLease);
                    srpServerHosts->set_remaining_lease_time_total_ms(srpServerHosts->remaining_lease_time_total_ms() +
                                                                      leaseInfo.mRemainingLease);
                    srpServerHosts->set_remaining_key_lease_time_total_ms(
                        srpServerHosts->remaining_key_lease_time_total_ms() + leaseInfo.mRemainingKeyLease);
                }

                while ((service = otSrpServerHostGetNextService(host, service)))
                {
                    if (otSrpServerServiceIsDeleted(service))
                    {
                        srpServerServices->set_deleted_count(srpServerServices->deleted_count() + 1);
                    }
                    else
                    {
                        srpServerServices->set_fresh_count(srpServerServices->fresh_count() + 1);
                        otSrpServerServiceGetLeaseInfo(service, &leaseInfo);
                        srpServerServices->set_lease_time_total_ms(srpServerServices->lease_time_total_ms() +
                                                                   leaseInfo.mLease);
                        srpServerServices->set_key_lease_time_total_ms(srpServerServices->key_lease_time_total_ms() +
                                                                       leaseInfo.mKeyLease);
                        srpServerServices->set_remaining_lease_time_total_ms(
                            srpServerServices->remaining_lease_time_total_ms() + leaseInfo.mRemainingLease);
                        srpServerServices->set_remaining_key_lease_time_total_ms(
                            srpServerServices->remaining_key_lease_time_total_ms() + leaseInfo.mRemainingKeyLease);
                    }
                }
            }

            srpServerResponseCounters->set_success_count(responseCounters->mSuccess);
            srpServerResponseCounters->set_server_failure_count(responseCounters->mServerFailure);
            srpServerResponseCounters->set_format_error_count(responseCounters->mFormatError);
            srpServerResponseCounters->set_name_exists_count(responseCounters->mNameExists);
            srpServerResponseCounters->set_refused_count(responseCounters->mRefused);
            srpServerResponseCounters->set_other_count(responseCounters->mOther);
        }
        // End of SrpServerInfo section.
#endif // OTBR_ENABLE_SRP_ADVERTISING_PROXY

#if OTBR_ENABLE_DNSSD_DISCOVERY_PROXY
        // Begin of DnsServerInfo section.
        {
            auto            dnsServer                 = wpanBorderRouter->mutable_dns_server();
            auto            dnsServerResponseCounters = dnsServer->mutable_response_counters();
            otDnssdCounters otDnssdCounters           = *otDnssdGetCounters(mInstance);

            dnsServerResponseCounters->set_success_count(otDnssdCounters.mSuccessResponse);
            dnsServerResponseCounters->set_server_failure_count(otDnssdCounters.mServerFailureResponse);
            dnsServerResponseCounters->set_format_error_count(otDnssdCounters.mFormatErrorResponse);
            dnsServerResponseCounters->set_name_error_count(otDnssdCounters.mNameErrorResponse);
            dnsServerResponseCounters->set_not_implemented_count(otDnssdCounters.mNotImplementedResponse);
            dnsServerResponseCounters->set_other_count(otDnssdCounters.mOtherResponse);

            dnsServer->set_resolved_by_local_srp_count(otDnssdCounters.mResolvedBySrp);
        }
        // End of DnsServerInfo section.
#endif // OTBR_ENABLE_DNSSD_DISCOVERY_PROXY

        // Start of MdnsInfo section.
        if (aPublisher != nullptr)
        {
            auto                     mdns     = wpanBorderRouter->mutable_mdns();
            const MdnsTelemetryInfo &mdnsInfo = aPublisher->GetMdnsTelemetryInfo();

            CopyMdnsResponseCounters(mdnsInfo.mHostRegistrations, mdns->mutable_host_registration_responses());
            CopyMdnsResponseCounters(mdnsInfo.mServiceRegistrations, mdns->mutable_service_registration_responses());
            CopyMdnsResponseCounters(mdnsInfo.mHostResolutions, mdns->mutable_host_resolution_responses());
            CopyMdnsResponseCounters(mdnsInfo.mServiceResolutions, mdns->mutable_service_resolution_responses());

            mdns->set_host_registration_ema_latency_ms(mdnsInfo.mHostRegistrationEmaLatency);
            mdns->set_service_registration_ema_latency_ms(mdnsInfo.mServiceRegistrationEmaLatency);
            mdns->set_host_resolution_ema_latency_ms(mdnsInfo.mHostResolutionEmaLatency);
            mdns->set_service_resolution_ema_latency_ms(mdnsInfo.mServiceResolutionEmaLatency);
        }
        // End of MdnsInfo section.

#if OTBR_ENABLE_NAT64
        // Start of BorderRoutingNat64State section.
        {
            auto nat64State = wpanBorderRouter->mutable_nat64_state();

            nat64State->set_prefix_manager_state(Nat64StateFromOtNat64State(otNat64GetPrefixManagerState(mInstance)));
            nat64State->set_translator_state(Nat64StateFromOtNat64State(otNat64GetTranslatorState(mInstance)));
        }
        // End of BorderRoutingNat64State section.

        // Start of Nat64Mapping section.
        {
            otNat64AddressMappingIterator iterator;
            otNat64AddressMapping         otMapping;
            Sha256::Hash                  hash;
            Sha256                        sha256;

            otNat64InitAddressMappingIterator(mInstance, &iterator);
            while (otNat64GetNextAddressMapping(mInstance, &iterator, &otMapping) == OT_ERROR_NONE)
            {
                auto nat64Mapping         = wpanBorderRouter->add_nat64_mappings();
                auto nat64MappingCounters = nat64Mapping->mutable_counters();

                nat64Mapping->set_mapping_id(otMapping.mId);
                CopyNat64TrafficCounters(otMapping.mCounters.mTcp, nat64MappingCounters->mutable_tcp());
                CopyNat64TrafficCounters(otMapping.mCounters.mUdp, nat64MappingCounters->mutable_udp());
                CopyNat64TrafficCounters(otMapping.mCounters.mIcmp, nat64MappingCounters->mutable_icmp());

                {
                    uint8_t ipAddrShaInput[OT_IP6_ADDRESS_SIZE + kNat64SourceAddressHashSaltLength];
                    memcpy(ipAddrShaInput, otMapping.mIp6.mFields.m8, sizeof(otMapping.mIp6.mFields.m8));
                    memcpy(&ipAddrShaInput[sizeof(otMapping.mIp6.mFields.m8)], mNat64Ipv6AddressSalt,
                           sizeof(mNat64Ipv6AddressSalt));

                    sha256.Start();
                    sha256.Update(ipAddrShaInput, sizeof(ipAddrShaInput));
                    sha256.Finish(hash);

                    nat64Mapping->mutable_hashed_ipv6_address()->append(reinterpret_cast<const char *>(hash.GetBytes()),
                                                                        sizeof(hash.GetBytes()));
                    // Remaining time is not included in the telemetry
                }
            }
        }
        // End of Nat64Mapping section.
#endif // OTBR_ENABLE_NAT64

        // End of WpanBorderRouter section.

        // Start of WpanRcp section.
        {
            auto                 wpanRcp                = telemetryData.mutable_wpan_rcp();
            auto                 rcpStabilityStatistics = wpanRcp->mutable_rcp_stability_statistics();
            otRadioSpinelMetrics otRadioSpinelMetrics   = *otSysGetRadioSpinelMetrics();

            rcpStabilityStatistics->set_rcp_timeout_count(otRadioSpinelMetrics.mRcpTimeoutCount);
            rcpStabilityStatistics->set_rcp_reset_count(otRadioSpinelMetrics.mRcpUnexpectedResetCount);
            rcpStabilityStatistics->set_rcp_restoration_count(otRadioSpinelMetrics.mRcpRestorationCount);
            rcpStabilityStatistics->set_spinel_parse_error_count(otRadioSpinelMetrics.mSpinelParseErrorCount);

            // TODO: provide rcp_firmware_update_count info.
            rcpStabilityStatistics->set_thread_stack_uptime(otInstanceGetUptime(mInstance));

            auto                  rcpInterfaceStatistics = wpanRcp->mutable_rcp_interface_statistics();
            otRcpInterfaceMetrics otRcpInterfaceMetrics  = *otSysGetRcpInterfaceMetrics();

            rcpInterfaceStatistics->set_rcp_interface_type(otRcpInterfaceMetrics.mRcpInterfaceType);
            rcpInterfaceStatistics->set_transferred_frames_count(otRcpInterfaceMetrics.mTransferredFrameCount);
            rcpInterfaceStatistics->set_transferred_valid_frames_count(
                otRcpInterfaceMetrics.mTransferredValidFrameCount);
            rcpInterfaceStatistics->set_transferred_garbage_frames_count(
                otRcpInterfaceMetrics.mTransferredGarbageFrameCount);
            rcpInterfaceStatistics->set_rx_frames_count(otRcpInterfaceMetrics.mRxFrameCount);
            rcpInterfaceStatistics->set_rx_bytes_count(otRcpInterfaceMetrics.mRxFrameByteCount);
            rcpInterfaceStatistics->set_tx_frames_count(otRcpInterfaceMetrics.mTxFrameCount);
            rcpInterfaceStatistics->set_tx_bytes_count(otRcpInterfaceMetrics.mTxFrameByteCount);
        }
        // End of WpanRcp section.

        // Start of CoexMetrics section.
        {
            auto               coexMetrics = telemetryData.mutable_coex_metrics();
            otRadioCoexMetrics otRadioCoexMetrics;

            SuccessOrExit(error = otPlatRadioGetCoexMetrics(mInstance, &otRadioCoexMetrics));
            coexMetrics->set_count_tx_request(otRadioCoexMetrics.mNumTxRequest);
            coexMetrics->set_count_tx_grant_immediate(otRadioCoexMetrics.mNumTxGrantImmediate);
            coexMetrics->set_count_tx_grant_wait(otRadioCoexMetrics.mNumTxGrantWait);
            coexMetrics->set_count_tx_grant_wait_activated(otRadioCoexMetrics.mNumTxGrantWaitActivated);
            coexMetrics->set_count_tx_grant_wait_timeout(otRadioCoexMetrics.mNumTxGrantWaitTimeout);
            coexMetrics->set_count_tx_grant_deactivated_during_request(
                otRadioCoexMetrics.mNumTxGrantDeactivatedDuringRequest);
            coexMetrics->set_tx_average_request_to_grant_time_us(otRadioCoexMetrics.mAvgTxRequestToGrantTime);
            coexMetrics->set_count_rx_request(otRadioCoexMetrics.mNumRxRequest);
            coexMetrics->set_count_rx_grant_immediate(otRadioCoexMetrics.mNumRxGrantImmediate);
            coexMetrics->set_count_rx_grant_wait(otRadioCoexMetrics.mNumRxGrantWait);
            coexMetrics->set_count_rx_grant_wait_activated(otRadioCoexMetrics.mNumRxGrantWaitActivated);
            coexMetrics->set_count_rx_grant_wait_timeout(otRadioCoexMetrics.mNumRxGrantWaitTimeout);
            coexMetrics->set_count_rx_grant_deactivated_during_request(
                otRadioCoexMetrics.mNumRxGrantDeactivatedDuringRequest);
            coexMetrics->set_count_rx_grant_none(otRadioCoexMetrics.mNumRxGrantNone);
            coexMetrics->set_rx_average_request_to_grant_time_us(otRadioCoexMetrics.mAvgRxRequestToGrantTime);
        }
        // End of CoexMetrics section.
    }

exit:
    return error;
}
#endif // OTBR_ENABLE_TELEMETRY_DATA_API
} // namespace agent
} // namespace otbr
