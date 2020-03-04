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

#include <assert.h>
#include <byteswap.h>
#include <string.h>

#include "openthread/border_router.h"
#include "openthread/channel_monitor.h"
#include "openthread/instance.h"
#include "openthread/joiner.h"
#include "openthread/link_raw.h"
#include "openthread/netdata.h"
#include "openthread/platform/radio.h"
#include "openthread/thread_ftd.h"

#include "dbus/common/constants.hpp"
#include "dbus/server/dbus_agent.hpp"
#include "dbus/server/dbus_thread_object.hpp"

using std::placeholders::_1;
using std::placeholders::_2;

static std::string GetDeviceRoleName(otDeviceRole aRole)
{
    std::string roleName;

    switch (aRole)
    {
    case OT_DEVICE_ROLE_DISABLED:
        roleName = OTBR_DISABLED_ROLE_NAME;
        break;
    case OT_DEVICE_ROLE_DETACHED:
        roleName = OTBR_DETACHED_ROLE_NAME;
        break;
    case OT_DEVICE_ROLE_CHILD:
        roleName = OTBR_CHILD_ROLE_NAME;
        break;
    case OT_DEVICE_ROLE_ROUTER:
        roleName = OTBR_ROUTER_ROLE_NAME;
        break;
    case OT_DEVICE_ROLE_LEADER:
        roleName = OTBR_LEADER_ROLE_NAME;
        break;
    }

    return roleName;
}

static uint64_t ConvertOpenThreadUint64(const uint8_t *aValue, size_t aSize)
{
    uint64_t val = 0;

    assert(aSize == sizeof(uint64_t));
    for (size_t i = 0; i < sizeof(uint64_t); i++)
    {
        val = (val << 8) | aValue[i];
    }
    return val;
}

namespace otbr {
namespace DBus {

DBusThreadObject::DBusThreadObject(DBusConnection *                 aConnection,
                                   const std::string &              aInterfaceName,
                                   otbr::Ncp::ControllerOpenThread *aNcp)
    : DBusObject(aConnection, OTBR_DBUS_OBJECT_PREFIX + aInterfaceName)
    , mNcp(aNcp)
{
}

otbrError DBusThreadObject::Init(void)
{
    otbrError err          = DBusObject::Init();
    auto      threadHelper = mNcp->GetThreadHelper();

    threadHelper->AddDeviceRoleHandler(std::bind(&DBusThreadObject::DeviceRoleHandler, this, _1));

    RegisterMethod(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_SCAN_METHOD,
                   std::bind(&DBusThreadObject::ScanHandler, this, _1));
    RegisterMethod(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_ATTACH_METHOD,
                   std::bind(&DBusThreadObject::AttachHandler, this, _1));
    RegisterMethod(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_FACTORY_RESET_METHOD,
                   std::bind(&DBusThreadObject::FactoryResetHandler, this, _1));
    RegisterMethod(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_RESET_METHOD,
                   std::bind(&DBusThreadObject::ResetHandler, this, _1));
    RegisterMethod(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_JOINER_START_METHOD,
                   std::bind(&DBusThreadObject::JoinerStartHandler, this, _1));
    RegisterMethod(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_JOINER_STOP_METHOD,
                   std::bind(&DBusThreadObject::JoinerStopHandler, this, _1));
    RegisterMethod(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_ADD_UNSECURE_PORT_METHOD,
                   std::bind(&DBusThreadObject::AddUnsecurePortHandler, this, _1));
    RegisterMethod(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_ADD_ON_MESH_PREFIX_METHOD,
                   std::bind(&DBusThreadObject::AddOnMeshPrefixHandler, this, _1));
    RegisterMethod(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_REMOVE_ON_MESH_PREFIX_METHOD,
                   std::bind(&DBusThreadObject::RemoveOnMeshPrefixHandler, this, _1));

    RegisterSetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_MESH_LOCAL_PREFIX_PROPERTY,
                               std::bind(&DBusThreadObject::SetMeshLocalPrefixHandler, this, _1));
    RegisterSetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_LEGACY_ULA_PREFIX_PROPERTY,
                               std::bind(&DBusThreadObject::SetLegacyUlaPrefixHandler, this, _1));
    RegisterSetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_LINK_MODE_PROPERTY,
                               std::bind(&DBusThreadObject::SetLinkModeHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_LINK_MODE_PROPERTY,
                               std::bind(&DBusThreadObject::GetLinkModeHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_DEVICE_ROLE_PROPERTY,
                               std::bind(&DBusThreadObject::GetDeviceRoleHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_NETWORK_NAME_PROPERTY,
                               std::bind(&DBusThreadObject::GetNetworkNameHandler, this, _1));

    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PANID_PROPERTY,
                               std::bind(&DBusThreadObject::GetPanIdHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_EXTPANID_PROPERTY,
                               std::bind(&DBusThreadObject::GetExtPanIdHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_CHANNEL_PROPERTY,
                               std::bind(&DBusThreadObject::GetChannelHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_MASTER_KEY_PROPERTY,
                               std::bind(&DBusThreadObject::GetMasterKeyHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_CCA_FAILURE_RATE_PROPERTY,
                               std::bind(&DBusThreadObject::GetCcaFailureRateHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_LINK_COUNTERS_PROPERTY,
                               std::bind(&DBusThreadObject::GetLinkCountersHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_IP6_COUNTERS_PROPERTY,
                               std::bind(&DBusThreadObject::GetIp6CountersHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_SUPPORTED_CHANNEL_MASK_PROPERTY,
                               std::bind(&DBusThreadObject::GetSupportedChannelMaskHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_RLOC16_PROPERTY,
                               std::bind(&DBusThreadObject::GetRloc16Handler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_EXTENDED_ADDRESS_PROPERTY,
                               std::bind(&DBusThreadObject::GetExtendedAddressHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_ROUTER_ID_PROPERTY,
                               std::bind(&DBusThreadObject::GetRouterIdHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_LEADER_DATA_PROPERTY,
                               std::bind(&DBusThreadObject::GetLeaderDataHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_NETWORK_DATA_PRPOERTY,
                               std::bind(&DBusThreadObject::GetNetworkDataHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_STABLE_NETWORK_DATA_PRPOERTY,
                               std::bind(&DBusThreadObject::GetStableNetworkDataHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_LOCAL_LEADER_WEIGHT_PROPERTY,
                               std::bind(&DBusThreadObject::GetLocalLeaderWeightHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_CHANNEL_MONITOR_SAMPLE_COUNT_PROPERTY,
                               std::bind(&DBusThreadObject::GetChannelMonitorSampleCountHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_CHANNEL_MONITOR_CHANNEL_QUALITY_MAP_PROPERTY,
                               std::bind(&DBusThreadObject::GetChannelMonitorChannelQualityMapHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_CHILD_TABLE_PROPERTY,
                               std::bind(&DBusThreadObject::GetChildTableHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_NEIGHBOR_TABLE_PROEPRTY,
                               std::bind(&DBusThreadObject::GetNeighborTableHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PARTITION_ID_PROEPRTY,
                               std::bind(&DBusThreadObject::GetPartitionIDHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_INSTANT_RSSI_PROPERTY,
                               std::bind(&DBusThreadObject::GetInstantRssiHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_RADIO_TX_POWER_PROPERTY,
                               std::bind(&DBusThreadObject::GetRadioTxPowerHandler, this, _1));

    return err;
}

void DBusThreadObject::DeviceRoleHandler(otDeviceRole aDeviceRole)
{
    SignalPropertyChanged(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_DEVICE_ROLE_PROPERTY, GetDeviceRoleName(aDeviceRole));
}

void DBusThreadObject::ScanHandler(DBusRequest &aRequest)
{
    auto threadHelper = mNcp->GetThreadHelper();
    threadHelper->Scan(std::bind(&DBusThreadObject::ReplyScanResult, this, aRequest, _1, _2));
}

void DBusThreadObject::ReplyScanResult(DBusRequest &                          aRequest,
                                       otError                                aError,
                                       const std::vector<otActiveScanResult> &aResult)
{
    std::vector<otbrActiveScanResult> results;

    if (aError != OT_ERROR_NONE)
    {
        aRequest.ReplyOtResult(aError);
    }
    else
    {
        for (const auto &r : aResult)
        {
            otbrActiveScanResult result;

            result.mExtAddress    = ConvertOpenThreadUint64(r.mExtAddress.m8, sizeof(r.mExtAddress.m8));
            result.mExtendedPanId = ConvertOpenThreadUint64(r.mExtendedPanId.m8, sizeof(r.mExtendedPanId.m8));
            result.mNetworkName   = r.mNetworkName.m8;
            result.mSteeringData =
                std::vector<uint8_t>(r.mSteeringData.m8, r.mSteeringData.m8 + r.mSteeringData.mLength);
            result.mPanId         = r.mPanId;
            result.mJoinerUdpPort = r.mJoinerUdpPort;
            result.mChannel       = r.mChannel;
            result.mRssi          = r.mRssi;
            result.mLqi           = r.mLqi;
            result.mVersion       = r.mVersion;
            result.mIsNative      = r.mIsNative;
            result.mIsJoinable    = r.mIsJoinable;

            results.emplace_back(result);
        }

        aRequest.Reply(std::tie(results));
    }
}

void DBusThreadObject::AttachHandler(DBusRequest &aRequest)
{
    auto                 threadHelper = mNcp->GetThreadHelper();
    std::string          name;
    uint16_t             panid;
    uint64_t             extPanId;
    std::vector<uint8_t> masterKey;
    std::vector<uint8_t> pskc;
    uint32_t             channelMask;

    auto args = std::tie(masterKey, panid, name, extPanId, pskc, channelMask);

    if (DBusMessageToTuple(*aRequest.GetMessage(), args) != OTBR_ERROR_NONE)
    {
        aRequest.ReplyOtResult(OT_ERROR_INVALID_ARGS);
    }
    else
    {
        threadHelper->Attach(name, panid, extPanId, masterKey, pskc, channelMask,
                             [aRequest](otError aError) mutable { aRequest.ReplyOtResult(aError); });
    }
}

void DBusThreadObject::FactoryResetHandler(DBusRequest &aRequest)
{
    aRequest.ReplyOtResult(OT_ERROR_NONE);
    mNcp->Reset();

    {
        auto threadHelper = mNcp->GetThreadHelper();
        threadHelper->AddDeviceRoleHandler(std::bind(&DBusThreadObject::DeviceRoleHandler, this, _1));
        SignalPropertyChanged(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_DEVICE_ROLE_PROPERTY,
                              GetDeviceRoleName(OT_DEVICE_ROLE_DISABLED));
    }
}

void DBusThreadObject::ResetHandler(DBusRequest &aRequest)
{
    auto threadHelper = mNcp->GetThreadHelper();

    otInstanceReset(threadHelper->GetInstance());
    aRequest.ReplyOtResult(OT_ERROR_NONE);
}

void DBusThreadObject::JoinerStartHandler(DBusRequest &aRequest)
{
    auto        threadHelper = mNcp->GetThreadHelper();
    std::string pskd, provisionUrl, vendorName, vendorModel, vendorSwVersion, vendorData;
    auto        args = std::tie(pskd, provisionUrl, vendorName, vendorModel, vendorSwVersion, vendorData);

    if (DBusMessageToTuple(*aRequest.GetMessage(), args) != OTBR_ERROR_NONE)
    {
        aRequest.ReplyOtResult(OT_ERROR_INVALID_ARGS);
    }
    else
    {
        threadHelper->JoinerStart(pskd, provisionUrl, vendorName, vendorModel, vendorSwVersion, vendorData,
                                  [aRequest](otError aError) mutable { aRequest.ReplyOtResult(aError); });
    }
}

void DBusThreadObject::JoinerStopHandler(DBusRequest &aRequest)
{
    auto threadHelper = mNcp->GetThreadHelper();

    otJoinerStop(threadHelper->GetInstance());
    aRequest.ReplyOtResult(OT_ERROR_NONE);
}

void DBusThreadObject::AddUnsecurePortHandler(DBusRequest &aRequest)
{
    auto     threadHelper = mNcp->GetThreadHelper();
    uint16_t port;
    uint32_t timeout;
    auto     args = std::tie(port, timeout);

    if (DBusMessageToTuple(*aRequest.GetMessage(), args) != OTBR_ERROR_NONE)
    {
        aRequest.ReplyOtResult(OT_ERROR_INVALID_ARGS);
    }
    else
    {
        aRequest.ReplyOtResult(threadHelper->AddUnsecurePort(port, timeout));
    }
}

void DBusThreadObject::AddOnMeshPrefixHandler(DBusRequest &aRequest)
{
    auto             threadHelper = mNcp->GetThreadHelper();
    otbrOnMeshPrefix onMeshPrefix;
    auto             args = std::tie(onMeshPrefix);

    if (DBusMessageToTuple(*aRequest.GetMessage(), args) != OTBR_ERROR_NONE)
    {
        aRequest.ReplyOtResult(OT_ERROR_INVALID_ARGS);
    }
    else
    {
        otBorderRouterConfig cfg;

        // size is guaranteed by parsing
        std::copy(onMeshPrefix.mPrefix.mPrefix.begin(), onMeshPrefix.mPrefix.mPrefix.end(),
                  &cfg.mPrefix.mPrefix.mFields.m8[0]);
        cfg.mPrefix.mLength = onMeshPrefix.mPrefix.mLength;
        cfg.mPreference     = onMeshPrefix.mPreference;
        cfg.mSlaac          = onMeshPrefix.mSlaac;
        cfg.mDhcp           = onMeshPrefix.mDhcp;
        cfg.mConfigure      = onMeshPrefix.mConfigure;
        cfg.mDefaultRoute   = onMeshPrefix.mDefaultRoute;
        cfg.mOnMesh         = onMeshPrefix.mOnMesh;
        cfg.mStable         = onMeshPrefix.mStable;

        aRequest.ReplyOtResult(otBorderRouterAddOnMeshPrefix(threadHelper->GetInstance(), &cfg));
    }
}

void DBusThreadObject::RemoveOnMeshPrefixHandler(DBusRequest &aRequest)
{
    auto          threadHelper = mNcp->GetThreadHelper();
    otbrIp6Prefix onMeshPrefix;
    auto          args = std::tie(onMeshPrefix);

    if (DBusMessageToTuple(*aRequest.GetMessage(), args) != OTBR_ERROR_NONE)
    {
        aRequest.ReplyOtResult(OT_ERROR_INVALID_ARGS);
    }
    else
    {
        otIp6Prefix prefix;
        // size is guaranteed by parsing
        std::copy(onMeshPrefix.mPrefix.begin(), onMeshPrefix.mPrefix.end(), &prefix.mPrefix.mFields.m8[0]);
        prefix.mLength = onMeshPrefix.mLength;

        aRequest.ReplyOtResult(otBorderRouterRemoveOnMeshPrefix(threadHelper->GetInstance(), &prefix));
    }
}

otError DBusThreadObject::SetMeshLocalPrefixHandler(DBusMessageIter &aIter)
{
    auto                                      threadHelper = mNcp->GetThreadHelper();
    otMeshLocalPrefix                         prefix;
    std::array<uint8_t, OTBR_IP6_PREFIX_SIZE> data{};
    otError                                   err = OT_ERROR_NONE;

    VerifyOrExit(DBusMessageExtractFromVariant(&aIter, data) == OTBR_ERROR_NONE, err = OT_ERROR_INVALID_ARGS);
    memcpy(&prefix.m8, &data.front(), sizeof(prefix.m8));
    err = otThreadSetMeshLocalPrefix(threadHelper->GetInstance(), &prefix);
exit:
    return err;
}

#if OPENTHREAD_CONFIG_LEGACY_ENABLE
extern "C" void otNcpHandleDidReceiveNewLegacyUlaPrefix(const uint8_t *aUlaPrefix);
#endif
otError DBusThreadObject::SetLegacyUlaPrefixHandler(DBusMessageIter &aIter)
{
#if OPENTHREAD_CONFIG_LEGACY_ENABLE
    std::array<uint8_t, OTBR_IP6_PREFIX_SIZE> data;
    otError                                   err = OT_ERROR_NONE;

    VerifyOrExit(DBusMessageExtractFromVariant(&aIter, data) == OTBR_ERROR_NONE, err = OT_ERROR_INVALID_ARGS);
    otNcpHandleDidReceiveNewLegacyUlaPrefix(&data[0]);
exit:
    return err;
#else
    return OT_ERROR_NOT_IMPLEMENTED;
#endif // OPENTHREAD_CONFIG_LEGACY_ENABLE
}

otError DBusThreadObject::SetLinkModeHandler(DBusMessageIter &aIter)
{
    auto               threadHelper = mNcp->GetThreadHelper();
    otbrLinkModeConfig cfg;
    otLinkModeConfig   otCfg;
    otError            err = OT_ERROR_NONE;

    VerifyOrExit(DBusMessageExtractFromVariant(&aIter, cfg) == OTBR_ERROR_NONE, err = OT_ERROR_INVALID_ARGS);
    otCfg.mDeviceType         = cfg.mDeviceType;
    otCfg.mNetworkData        = cfg.mNetworkData;
    otCfg.mSecureDataRequests = cfg.mSecureDataRequests;
    otCfg.mRxOnWhenIdle       = cfg.mRxOnWhenIdle;
    err                       = otThreadSetLinkMode(threadHelper->GetInstance(), otCfg);
exit:
    return err;
}

otError DBusThreadObject::GetLinkModeHandler(DBusMessageIter &aIter)
{
    auto               threadHelper = mNcp->GetThreadHelper();
    otLinkModeConfig   otCfg        = otThreadGetLinkMode(threadHelper->GetInstance());
    otbrLinkModeConfig cfg;
    otError            err = OT_ERROR_NONE;

    cfg.mDeviceType         = otCfg.mDeviceType;
    cfg.mNetworkData        = otCfg.mNetworkData;
    cfg.mSecureDataRequests = otCfg.mSecureDataRequests;
    cfg.mRxOnWhenIdle       = otCfg.mRxOnWhenIdle;

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, cfg) == OTBR_ERROR_NONE, err = OT_ERROR_INVALID_ARGS);
exit:
    return err;
}

otError DBusThreadObject::GetDeviceRoleHandler(DBusMessageIter &aIter)
{
    auto         threadHelper = mNcp->GetThreadHelper();
    otDeviceRole role         = otThreadGetDeviceRole(threadHelper->GetInstance());
    std::string  roleName     = GetDeviceRoleName(role);
    ;
    otError err = OT_ERROR_NONE;

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, roleName) == OTBR_ERROR_NONE, err = OT_ERROR_INVALID_ARGS);
exit:
    return err;
}

otError DBusThreadObject::GetNetworkNameHandler(DBusMessageIter &aIter)
{
    auto        threadHelper = mNcp->GetThreadHelper();
    std::string networkName  = otThreadGetNetworkName(threadHelper->GetInstance());
    otError     err          = OT_ERROR_NONE;

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, networkName) == OTBR_ERROR_NONE, err = OT_ERROR_INVALID_ARGS);
exit:
    return err;
}

otError DBusThreadObject::GetPanIdHandler(DBusMessageIter &aIter)
{
    auto     threadHelper = mNcp->GetThreadHelper();
    uint16_t panId        = otLinkGetPanId(threadHelper->GetInstance());
    otError  err          = OT_ERROR_NONE;

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, panId) == OTBR_ERROR_NONE, err = OT_ERROR_INVALID_ARGS);
exit:
    return err;
}

otError DBusThreadObject::GetExtPanIdHandler(DBusMessageIter &aIter)
{
    auto                   threadHelper = mNcp->GetThreadHelper();
    const otExtendedPanId *extPanId     = otThreadGetExtendedPanId(threadHelper->GetInstance());
    uint64_t               extPanIdVal;
    otError                err = OT_ERROR_NONE;

    extPanIdVal = ConvertOpenThreadUint64(extPanId->m8, sizeof(extPanId->m8));

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, extPanIdVal) == OTBR_ERROR_NONE, err = OT_ERROR_INVALID_ARGS);
exit:
    return err;
}

otError DBusThreadObject::GetChannelHandler(DBusMessageIter &aIter)
{
    auto     threadHelper = mNcp->GetThreadHelper();
    uint16_t channel      = otLinkGetChannel(threadHelper->GetInstance());
    otError  err          = OT_ERROR_NONE;

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, channel) == OTBR_ERROR_NONE, err = OT_ERROR_INVALID_ARGS);
exit:
    return err;
}

otError DBusThreadObject::GetMasterKeyHandler(DBusMessageIter &aIter)
{
    auto                 threadHelper = mNcp->GetThreadHelper();
    const otMasterKey *  masterKey    = otThreadGetMasterKey(threadHelper->GetInstance());
    std::vector<uint8_t> keyVal(masterKey->m8, masterKey->m8 + sizeof(masterKey->m8));
    otError              err = OT_ERROR_NONE;

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, keyVal) == OTBR_ERROR_NONE, err = OT_ERROR_INVALID_ARGS);
exit:
    return err;
}

otError DBusThreadObject::GetCcaFailureRateHandler(DBusMessageIter &aIter)
{
    auto     threadHelper = mNcp->GetThreadHelper();
    uint16_t failureRate  = otLinkGetCcaFailureRate(threadHelper->GetInstance());
    otError  err          = OT_ERROR_NONE;

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, failureRate) == OTBR_ERROR_NONE, err = OT_ERROR_INVALID_ARGS);
exit:
    return err;
}

otError DBusThreadObject::GetLinkCountersHandler(DBusMessageIter &aIter)
{
    auto                 threadHelper = mNcp->GetThreadHelper();
    const otMacCounters *otCounters   = otLinkGetCounters(threadHelper->GetInstance());
    otbrMacCounters      counters;
    otError              err = OT_ERROR_NONE;

    counters.mTxTotal              = otCounters->mTxTotal;
    counters.mTxUnicast            = otCounters->mTxUnicast;
    counters.mTxBroadcast          = otCounters->mTxBroadcast;
    counters.mTxAckRequested       = otCounters->mTxAckRequested;
    counters.mTxAcked              = otCounters->mTxAcked;
    counters.mTxNoAckRequested     = otCounters->mTxNoAckRequested;
    counters.mTxData               = otCounters->mTxData;
    counters.mTxDataPoll           = otCounters->mTxDataPoll;
    counters.mTxBeacon             = otCounters->mTxBeacon;
    counters.mTxBeaconRequest      = otCounters->mTxBeaconRequest;
    counters.mTxOther              = otCounters->mTxOther;
    counters.mTxRetry              = otCounters->mTxRetry;
    counters.mTxErrCca             = otCounters->mTxErrCca;
    counters.mTxErrAbort           = otCounters->mTxErrAbort;
    counters.mTxErrBusyChannel     = otCounters->mTxErrBusyChannel;
    counters.mRxTotal              = otCounters->mRxTotal;
    counters.mRxUnicast            = otCounters->mTxUnicast;
    counters.mRxBroadcast          = otCounters->mRxBroadcast;
    counters.mRxData               = otCounters->mRxData;
    counters.mRxDataPoll           = otCounters->mTxDataPoll;
    counters.mRxBeacon             = otCounters->mRxBeacon;
    counters.mRxBeaconRequest      = otCounters->mRxBeaconRequest;
    counters.mRxOther              = otCounters->mRxOther;
    counters.mRxAddressFiltered    = otCounters->mRxAddressFiltered;
    counters.mRxDestAddrFiltered   = otCounters->mRxDestAddrFiltered;
    counters.mRxDuplicated         = otCounters->mRxDuplicated;
    counters.mRxErrNoFrame         = otCounters->mRxErrNoFrame;
    counters.mRxErrUnknownNeighbor = otCounters->mRxErrUnknownNeighbor;
    counters.mRxErrInvalidSrcAddr  = otCounters->mRxErrInvalidSrcAddr;
    counters.mRxErrSec             = otCounters->mRxErrSec;
    counters.mRxErrFcs             = otCounters->mRxErrFcs;
    counters.mRxErrOther           = otCounters->mRxErrOther;

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, counters) == OTBR_ERROR_NONE, err = OT_ERROR_INVALID_ARGS);
exit:
    return err;
}

otError DBusThreadObject::GetIp6CountersHandler(DBusMessageIter &aIter)
{
    auto                threadHelper = mNcp->GetThreadHelper();
    const otIpCounters *otCounters   = otThreadGetIp6Counters(threadHelper->GetInstance());
    otbrIpCounters      counters;
    otError             err = OT_ERROR_NONE;

    counters.mTxSuccess = otCounters->mTxSuccess;
    counters.mTxFailure = otCounters->mTxFailure;
    counters.mRxSuccess = otCounters->mRxSuccess;
    counters.mRxFailure = otCounters->mRxFailure;

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, counters) == OTBR_ERROR_NONE, err = OT_ERROR_INVALID_ARGS);
exit:
    return err;
}

otError DBusThreadObject::GetSupportedChannelMaskHandler(DBusMessageIter &aIter)
{
    auto     threadHelper = mNcp->GetThreadHelper();
    uint32_t channelMask  = otLinkGetSupportedChannelMask(threadHelper->GetInstance());
    otError  err          = OT_ERROR_NONE;

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, channelMask) == OTBR_ERROR_NONE, err = OT_ERROR_INVALID_ARGS);
exit:
    return err;
}

otError DBusThreadObject::GetRloc16Handler(DBusMessageIter &aIter)
{
    auto     threadHelper = mNcp->GetThreadHelper();
    otError  err          = OT_ERROR_NONE;
    uint16_t rloc16       = otThreadGetRloc16(threadHelper->GetInstance());

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, rloc16) == OTBR_ERROR_NONE, err = OT_ERROR_INVALID_ARGS);
exit:
    return err;
}

otError DBusThreadObject::GetExtendedAddressHandler(DBusMessageIter &aIter)
{
    auto                threadHelper    = mNcp->GetThreadHelper();
    otError             err             = OT_ERROR_NONE;
    const otExtAddress *addr            = otLinkGetExtendedAddress(threadHelper->GetInstance());
    uint64_t            extendedAddress = ConvertOpenThreadUint64(addr->m8, sizeof(addr->m8));

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, extendedAddress) == OTBR_ERROR_NONE, err = OT_ERROR_INVALID_ARGS);
exit:
    return err;
}

otError DBusThreadObject::GetRouterIdHandler(DBusMessageIter &aIter)
{
    auto         threadHelper = mNcp->GetThreadHelper();
    otError      err          = OT_ERROR_NONE;
    uint16_t     rloc16       = otThreadGetRloc16(threadHelper->GetInstance());
    otRouterInfo info;

    SuccessOrExit(err = otThreadGetRouterInfo(threadHelper->GetInstance(), rloc16, &info));
    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, info.mRouterId) == OTBR_ERROR_NONE, err = OT_ERROR_INVALID_ARGS);
exit:
    return err;
}

otError DBusThreadObject::GetLeaderDataHandler(DBusMessageIter &aIter)
{
    auto                threadHelper = mNcp->GetThreadHelper();
    otError             err          = OT_ERROR_NONE;
    struct otLeaderData data;
    otbrLeaderData      leaderData;

    SuccessOrExit(err = otThreadGetLeaderData(threadHelper->GetInstance(), &data));
    leaderData.mPartitionId       = data.mPartitionId;
    leaderData.mWeighting         = data.mWeighting;
    leaderData.mDataVersion       = data.mDataVersion;
    leaderData.mStableDataVersion = data.mStableDataVersion;
    leaderData.mLeaderRouterId    = data.mLeaderRouterId;
    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, leaderData) == OTBR_ERROR_NONE, err = OT_ERROR_INVALID_ARGS);
exit:
    return err;
}

otError DBusThreadObject::GetNetworkDataHandler(DBusMessageIter &aIter)
{
    auto                 threadHelper = mNcp->GetThreadHelper();
    otError              err          = OT_ERROR_NONE;
    uint8_t              data[255]; // magic number from openthread/src/cli/cli.cpp:ProcessNetworkDataShow
    uint8_t              len = sizeof(data);
    std::vector<uint8_t> networkData;

    otNetDataGet(threadHelper->GetInstance(), /*stable=*/false, data, &len);
    networkData = std::vector<uint8_t>(&data[0], &data[len]);
    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, networkData) == OTBR_ERROR_NONE, err = OT_ERROR_INVALID_ARGS);
exit:
    return err;
}

otError DBusThreadObject::GetStableNetworkDataHandler(DBusMessageIter &aIter)
{
    auto                 threadHelper = mNcp->GetThreadHelper();
    otError              err          = OT_ERROR_NONE;
    uint8_t              data[255]; // magic number from openthread/src/cli/cli.cpp:ProcessNetworkDataShow
    uint8_t              len = sizeof(data);
    std::vector<uint8_t> networkData;

    otNetDataGet(threadHelper->GetInstance(), /*stable=*/true, data, &len);
    networkData = std::vector<uint8_t>(&data[0], &data[len]);
    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, networkData) == OTBR_ERROR_NONE, err = OT_ERROR_INVALID_ARGS);
exit:
    return err;
}

otError DBusThreadObject::GetLocalLeaderWeightHandler(DBusMessageIter &aIter)
{
    auto    threadHelper = mNcp->GetThreadHelper();
    otError err          = OT_ERROR_NONE;
    uint8_t weight       = otThreadGetLocalLeaderWeight(threadHelper->GetInstance());

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, weight) == OTBR_ERROR_NONE, err = OT_ERROR_INVALID_ARGS);
exit:
    return err;
}

otError DBusThreadObject::GetChannelMonitorSampleCountHandler(DBusMessageIter &aIter)
{
#if OPENTHREAD_CONFIG_CHANNEL_MONITOR_ENABLE
    auto     threadHelper = mNcp->GetThreadHelper();
    otError  err          = OT_ERROR_NONE;
    uint32_t cnt          = otChannelMonitorGetSampleCount(threadHelper->GetInstance());

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, cnt) == OTBR_ERROR_NONE, err = OT_ERROR_INVALID_ARGS);
exit:
    return err;
#else  // OPENTHREAD_CONFIG_CHANNEL_MONITOR_ENABLE
    return OT_ERROR_NOT_IMPLEMENTED;
#endif // OPENTHREAD_CONFIG_CHANNEL_MONITOR_ENABLE
}

otError DBusThreadObject::GetChannelMonitorChannelQualityMapHandler(DBusMessageIter &aIter)
{
#if OPENTHREAD_CONFIG_CHANNEL_MONITOR_ENABLE
    auto                            threadHelper = mNcp->GetThreadHelper();
    otError                         err          = OT_ERROR_NONE;
    uint32_t                        channelMask  = otLinkGetSupportedChannelMask(threadHelper->GetInstance());
    constexpr uint8_t               kNumChannels = sizeof(channelMask) * 8; // 8 bit per byte
    std::vector<otbrChannelQuality> quality;

    for (uint8_t i = 0; i < kNumChannels; i++)
    {
        if (channelMask & (1 << i))
        {
            uint16_t occupancy = otChannelMonitorGetChannelOccupancy(threadHelper->GetInstance(), i);

            quality.emplace_back(otbrChannelQuality{i, occupancy});
        }
    }

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, quality) == OTBR_ERROR_NONE, err = OT_ERROR_INVALID_ARGS);
exit:
    return err;
#else  // OPENTHREAD_CONFIG_CHANNEL_MONITOR_ENABLE
    return OT_ERROR_NOT_IMPLEMENTED;
#endif // OPENTHREAD_CONFIG_CHANNEL_MONITOR_ENABLE
}

otError DBusThreadObject::GetChildTableHandler(DBusMessageIter &aIter)
{
    auto                       threadHelper = mNcp->GetThreadHelper();
    otError                    err          = OT_ERROR_NONE;
    uint16_t                   childIndex   = 0;
    otChildInfo                childInfo;
    std::vector<otbrChildInfo> childTable;

    while (otThreadGetChildInfoByIndex(threadHelper->GetInstance(), childIndex, &childInfo) == OT_ERROR_NONE)
    {
        otbrChildInfo info;

        info.mExtAddress         = ConvertOpenThreadUint64(childInfo.mExtAddress.m8, sizeof(childInfo.mExtAddress.m8));
        info.mTimeout            = childInfo.mTimeout;
        info.mAge                = childInfo.mAge;
        info.mChildId            = childInfo.mChildId;
        info.mNetworkDataVersion = childInfo.mNetworkDataVersion;
        info.mLinkQualityIn      = childInfo.mLinkQualityIn;
        info.mAverageRssi        = childInfo.mAverageRssi;
        info.mLastRssi           = childInfo.mLastRssi;
        info.mFrameErrorRate     = childInfo.mFrameErrorRate;
        info.mMessageErrorRate   = childInfo.mMessageErrorRate;
        info.mRxOnWhenIdle       = childInfo.mRxOnWhenIdle;
        info.mSecureDataRequest  = childInfo.mSecureDataRequest;
        info.mFullThreadDevice   = childInfo.mFullThreadDevice;
        info.mFullNetworkData    = childInfo.mFullNetworkData;
        info.mIsStateRestoring   = childInfo.mIsStateRestoring;
        childTable.push_back(info);
        childIndex++;
    }

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, childTable) == OTBR_ERROR_NONE, err = OT_ERROR_INVALID_ARGS);
exit:
    return err;
}

otError DBusThreadObject::GetNeighborTableHandler(DBusMessageIter &aIter)
{
    auto                          threadHelper = mNcp->GetThreadHelper();
    otError                       err          = OT_ERROR_NONE;
    otNeighborInfoIterator        iter         = OT_NEIGHBOR_INFO_ITERATOR_INIT;
    otNeighborInfo                neighborInfo;
    std::vector<otbrNeighborInfo> neighborTable;

    while (otThreadGetNextNeighborInfo(threadHelper->GetInstance(), &iter, &neighborInfo) == OT_ERROR_NONE)
    {
        otbrNeighborInfo info;

        info.mExtAddress = ConvertOpenThreadUint64(neighborInfo.mExtAddress.m8, sizeof(neighborInfo.mExtAddress.m8));
        info.mAge        = neighborInfo.mAge;
        info.mRloc16     = neighborInfo.mRloc16;
        info.mLinkFrameCounter  = neighborInfo.mLinkFrameCounter;
        info.mMleFrameCounter   = neighborInfo.mMleFrameCounter;
        info.mLinkQualityIn     = neighborInfo.mLinkQualityIn;
        info.mAverageRssi       = neighborInfo.mAverageRssi;
        info.mLastRssi          = neighborInfo.mLastRssi;
        info.mFrameErrorRate    = neighborInfo.mFrameErrorRate;
        info.mMessageErrorRate  = neighborInfo.mMessageErrorRate;
        info.mRxOnWhenIdle      = neighborInfo.mRxOnWhenIdle;
        info.mSecureDataRequest = neighborInfo.mSecureDataRequest;
        info.mFullThreadDevice  = neighborInfo.mFullThreadDevice;
        info.mFullNetworkData   = neighborInfo.mFullNetworkData;
        info.mIsChild           = neighborInfo.mIsChild;
        neighborTable.push_back(info);
    }

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, neighborTable) == OTBR_ERROR_NONE, err = OT_ERROR_INVALID_ARGS);
exit:
    return err;
}

otError DBusThreadObject::GetPartitionIDHandler(DBusMessageIter &aIter)
{
    auto     threadHelper = mNcp->GetThreadHelper();
    otError  err          = OT_ERROR_NONE;
    uint32_t partitionId  = otThreadGetPartitionId(threadHelper->GetInstance());

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, partitionId) == OTBR_ERROR_NONE, err = OT_ERROR_INVALID_ARGS);
exit:
    return err;
}

otError DBusThreadObject::GetInstantRssiHandler(DBusMessageIter &aIter)
{
    auto    threadHelper = mNcp->GetThreadHelper();
    otError err          = OT_ERROR_NONE;
    int8_t  rssi         = otPlatRadioGetRssi(threadHelper->GetInstance());

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, rssi) == OTBR_ERROR_NONE, err = OT_ERROR_INVALID_ARGS);
exit:
    return err;
}

otError DBusThreadObject::GetRadioTxPowerHandler(DBusMessageIter &aIter)
{
    auto    threadHelper = mNcp->GetThreadHelper();
    otError err          = OT_ERROR_NONE;
    int8_t  txPower;

    SuccessOrExit(err = otPlatRadioGetTransmitPower(threadHelper->GetInstance(), &txPower));

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, txPower) == OTBR_ERROR_NONE, err = OT_ERROR_INVALID_ARGS);
exit:
    return err;
}

} // namespace DBus
} // namespace otbr
