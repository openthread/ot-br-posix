/*
 *    Copyright (c) 2021, The OpenThread Authors.
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

#include "hidl/1.0/hidl_thread.hpp"

#include <hidl/HidlTransportSupport.h>
#include <openthread/border_router.h>
#include <openthread/thread_ftd.h>

#include "common/code_utils.hpp"
#include "common/logging.hpp"
#include "common/ot_utils.hpp"
#include "common/types.hpp"

#if OTBR_ENABLE_LEGACY
#include <ot-legacy-pairing-ext.h>
#endif

namespace otbr {

namespace Hidl {

using android::hardware::handleTransportPoll;
using android::hardware::setupTransportPolling;

using std::placeholders::_1;
using std::placeholders::_2;

HidlThread::HidlThread(otbr::Ncp::ControllerOpenThread *aNcp)
    : mNcp(aNcp)
    , mThreadCallback(nullptr)
{
    VerifyOrDie(setupTransportPolling() >= 0, "Setup HIDL transport for use with (e)poll failed");
}

void HidlThread::Init(void)
{
    otbrLog(OTBR_LOG_INFO, "Register HIDL Thread service");
    VerifyOrDie(registerAsService() == android::NO_ERROR, "Register HIDL Thread service failed");

    mDeathRecipient = new ClientDeathRecipient(sClientDeathCallback, this);
    VerifyOrDie(mDeathRecipient != nullptr, "Create client death reciptient failed");
}

Return<void> HidlThread::initialize(const sp<IThreadCallback> &aCallback)
{
    auto threadHelper = mNcp->GetThreadHelper();

    mThreadCallback = aCallback;

    threadHelper->AddDeviceRoleHandler(std::bind(&HidlThread::DeviceRoleHandler, this, _1));
    mNcp->RegisterResetHandler(std::bind(&HidlThread::NcpResetHandler, this));

    mDeathRecipient->SetClientHasDied(false);
    mThreadCallback->linkToDeath(mDeathRecipient, 1);

    otbrLog(OTBR_LOG_INFO, "HIDL Thread interface initialized");

    return Void();
}

Return<void> HidlThread::deinitialize(void)
{
    if ((!mDeathRecipient->GetClientHasDied()) && (mThreadCallback != nullptr))
    {
        mThreadCallback->unlinkToDeath(mDeathRecipient);
        mDeathRecipient->SetClientHasDied(true);
    }

    mThreadCallback = nullptr;
    otbrLog(OTBR_LOG_INFO, "HIDL Thread interface deinitialized");

    return Void();
}

void HidlThread::DeviceRoleHandler(otDeviceRole aDeviceRole)
{
    if (mThreadCallback != nullptr)
    {
        mThreadCallback->onAddDeviceRole(static_cast<::android::hardware::thread::V1_0::DeviceRole>(aDeviceRole));
    }
}

void HidlThread::NcpResetHandler(void)
{
    mNcp->GetThreadHelper()->AddDeviceRoleHandler(std::bind(&HidlThread::DeviceRoleHandler, this, _1));

    if (mThreadCallback != nullptr)
    {
        mThreadCallback->onAddDeviceRole(
            static_cast<::android::hardware::thread::V1_0::DeviceRole>(OT_DEVICE_ROLE_DISABLED));
    }
}

Return<ThreadError> HidlThread::permitUnsecureJoin(uint16_t aPort, uint32_t aSeconds)
{
    OTBR_UNUSED_VARIABLE(aPort);
    OTBR_UNUSED_VARIABLE(aSeconds);

    ThreadError error = ThreadError::OT_ERROR_NOT_IMPLEMENTED;

#ifdef OTBR_ENABLE_UNSECURE_JOIN
    auto threadHelper = mNcp->GetThreadHelper();

    error = static_cast<ThreadError>(threadHelper->PermitUnsecureJoin(aPort, aSeconds));

    otbrLog(OTBR_LOG_INFO, "permitUnsecureJoin: Port:%d, Seconds:%d", aPort, aSeconds);
#endif

    return error;
}

Return<ThreadError> HidlThread::scan()
{
    auto threadHelper = mNcp->GetThreadHelper();

    threadHelper->Scan(std::bind(&HidlThread::ScanResultHandler, this, _1, _2));
    otbrLog(OTBR_LOG_INFO, "Scan");

    return ThreadError::ERROR_NONE;
}

void HidlThread::ScanResultHandler(otError aError, const std::vector<otActiveScanResult> &aResult)
{
    std::vector<::android::hardware::thread::V1_0::ActiveScanResult> results;

    otbrLog(OTBR_LOG_INFO, "ScanResultHandler: Error:%d", aError);

    VerifyOrExit(aError == OT_ERROR_NONE);
    for (const auto &r : aResult)
    {
        ::android::hardware::thread::V1_0::ActiveScanResult result;

        std::copy(r.mSteeringData.m8, r.mSteeringData.m8 + r.mSteeringData.mLength, result.mSteeringData.begin());

        result.mExtAddress    = ArrayToUint64(r.mExtAddress.m8);
        result.mExtendedPanId = ArrayToUint64(r.mExtendedPanId.m8);
        result.mNetworkName   = r.mNetworkName.m8;
        result.mPanId         = r.mPanId;
        result.mJoinerUdpPort = r.mJoinerUdpPort;
        result.mChannel       = r.mChannel;
        result.mRssi          = r.mRssi;
        result.mLqi           = r.mLqi;
        result.mVersion       = r.mVersion;
        result.mIsNative      = r.mIsNative;
        result.mIsJoinable    = r.mIsJoinable;

        otbrLog(OTBR_LOG_INFO,
                "IsJoinable:%u, NetworkName:%-16s, ExtPanId:0x%s, PanId:0x%04x, ExtAddress:%s, Channel:%2u: "
                "Rssi:%3u, Lqi:%3u",
                r.mIsJoinable, r.mNetworkName.m8, otbr::ExtPanId(r.mExtendedPanId).ToString().c_str(), r.mPanId,
                otbr::ExtAddress(r.mExtAddress).ToString().c_str(), r.mChannel, r.mRssi, r.mLqi);

        results.emplace_back(result);
    }

exit:
    if (mThreadCallback != nullptr)
    {
        mThreadCallback->onScan(static_cast<ThreadError>(aError), std::move(results));
    }
}

Return<ThreadError> HidlThread::attach(const hidl_string &      aNetworkName,
                                       uint16_t                 aPanId,
                                       uint64_t                 aExtPanId,
                                       const hidl_vec<uint8_t> &aMasterKey,
                                       const hidl_vec<uint8_t> &aPSKc,
                                       uint32_t                 aChannelMask)
{
    auto threadHelper = mNcp->GetThreadHelper();

    otbrLog(OTBR_LOG_INFO,
            "Attach: NetworkName:%s, PanId:0x%04x, ExtPanId:0x%s, MaskerKey:[Hiden], Pskc:[Hiden], "
            "ChannelMask:0x%08x",
            aNetworkName.c_str(), aPanId, otbr::ExtPanId(Uint64ToOtExtendedPanId(aExtPanId)).ToString().c_str(),
            aChannelMask);

    threadHelper->Attach(aNetworkName, aPanId, aExtPanId, aMasterKey, aPSKc, aChannelMask, [this](otError aError) {
        if (mThreadCallback != nullptr)
        {
            otbrLog(OTBR_LOG_INFO, "onAttach: error=%d", aError);
            mThreadCallback->onAttach(static_cast<ThreadError>(aError));
        }
    });

    return ThreadError::ERROR_NONE;
}

Return<ThreadError> HidlThread::attachActiveDataset()
{
    auto threadHelper = mNcp->GetThreadHelper();

    otbrLog(OTBR_LOG_INFO, "AttachActiveDataset");

    threadHelper->Attach([this](otError aError) {
        if (mThreadCallback != nullptr)
        {
            otbrLog(OTBR_LOG_INFO, "onAttach: error=%d", aError);
            mThreadCallback->onAttach(static_cast<ThreadError>(aError));
        }
    });

    return ThreadError::ERROR_NONE;
}

Return<ThreadError> HidlThread::factoryReset()
{
    otbrLog(OTBR_LOG_INFO, "FactoryReset");
    otInstanceFactoryReset(mNcp->GetThreadHelper()->GetInstance());
    return ThreadError::ERROR_NONE;
}

Return<ThreadError> HidlThread::reset()
{
    otbrLog(OTBR_LOG_INFO, "Reset");
    mNcp->Reset();
    return ThreadError::ERROR_NONE;
}

Return<ThreadError> HidlThread::joinerStart(const hidl_string &aPskd,
                                            const hidl_string &aProvisioningUrl,
                                            const hidl_string &aVendorName,
                                            const hidl_string &aVendorModel,
                                            const hidl_string &aVendorSwVersion,
                                            const hidl_string &aVendorData)
{
    auto threadHelper = mNcp->GetThreadHelper();

    otbrLog(OTBR_LOG_INFO,
            "JoinerStart: Pskd:[Hiden], ProvisioningUrl:%s, VendorName:%s, VendorModel:%s, "
            "VendorSwVersion:%s, VendorData:%s",
            aProvisioningUrl.c_str(), aVendorName.c_str(), aVendorModel.c_str(), aVendorSwVersion.c_str(),
            aVendorData.c_str());

    threadHelper->JoinerStart(aPskd, aProvisioningUrl, aVendorName, aVendorModel, aVendorSwVersion, aVendorData,
                              [this](otError aError) {
                                  if (mThreadCallback != nullptr)
                                  {
                                      mThreadCallback->onJoinerStart(static_cast<ThreadError>(aError));
                                  }
                              });
    return ThreadError::ERROR_NONE;
}

Return<ThreadError> HidlThread::joinerStop()
{
    auto threadHelper = mNcp->GetThreadHelper();

    otJoinerStop(threadHelper->GetInstance());
    otbrLog(OTBR_LOG_INFO, "JoinerStop");

    return ThreadError::ERROR_NONE;
}

Return<ThreadError> HidlThread::addOnMeshPrefix(const ::android::hardware::thread::V1_0::OnMeshPrefix &aPrefix)
{
    auto                 threadHelper = mNcp->GetThreadHelper();
    otError              error        = OT_ERROR_NONE;
    otBorderRouterConfig config;

    VerifyOrExit(aPrefix.mPrefix.mPrefix.size() <= OT_IP6_ADDRESS_SIZE, error = OT_ERROR_INVALID_ARGS);

    std::copy(aPrefix.mPrefix.mPrefix.begin(), aPrefix.mPrefix.mPrefix.end(), &config.mPrefix.mPrefix.mFields.m8[0]);
    config.mPrefix.mLength = aPrefix.mPrefix.mLength;
    config.mPreference     = aPrefix.mPreference;
    config.mSlaac          = aPrefix.mSlaac;
    config.mDhcp           = aPrefix.mDhcp;
    config.mConfigure      = aPrefix.mConfigure;
    config.mDefaultRoute   = aPrefix.mDefaultRoute;
    config.mOnMesh         = aPrefix.mOnMesh;
    config.mStable         = aPrefix.mStable;

    SuccessOrExit(error = otBorderRouterAddOnMeshPrefix(threadHelper->GetInstance(), &config));
    SuccessOrExit(error = otBorderRouterRegister(threadHelper->GetInstance()));

    otbrLog(OTBR_LOG_INFO,
            "AddOnMeshPrefix: Prefix:%s, Preference:%u, Slaac:%u, Dhcp:%u, Configure:%u, DefaultRoute:%u, OnMesh:%u, "
            "Stable:%u",
            otbr::Ip6Prefix(config.mPrefix).ToString().c_str(), config.mPreference, config.mSlaac, config.mDhcp,
            config.mConfigure, config.mDefaultRoute, config.mOnMesh, config.mStable);

exit:
    return static_cast<ThreadError>(error);
}

Return<ThreadError> HidlThread::removeOnMeshPrefix(const ::android::hardware::thread::V1_0::Ip6Prefix &aPrefix)
{
    auto        threadHelper = mNcp->GetThreadHelper();
    otError     error        = OT_ERROR_NONE;
    otIp6Prefix ip6Prefix;

    VerifyOrExit(aPrefix.mPrefix.size() <= OT_IP6_ADDRESS_SIZE, error = OT_ERROR_INVALID_ARGS);

    std::copy(aPrefix.mPrefix.begin(), aPrefix.mPrefix.end(), &ip6Prefix.mPrefix.mFields.m8[0]);
    ip6Prefix.mLength = aPrefix.mLength;

    SuccessOrExit(error = otBorderRouterRemoveOnMeshPrefix(threadHelper->GetInstance(), &ip6Prefix));
    SuccessOrExit(error = otBorderRouterRegister(threadHelper->GetInstance()));

    otbrLog(OTBR_LOG_INFO, "RemoveOnMeshPrefix: Prefix:%s", otbr::Ip6Prefix(ip6Prefix).ToString().c_str());

exit:
    return static_cast<ThreadError>(error);
}

Return<ThreadError> HidlThread::addExternalRoute(const ::android::hardware::thread::V1_0::ExternalRoute &aExternalRoute)
{
    auto                  threadHelper = mNcp->GetThreadHelper();
    otError               error        = OT_ERROR_NONE;
    otExternalRouteConfig otRoute;
    otIp6Prefix &         prefix = otRoute.mPrefix;

    VerifyOrExit(aExternalRoute.mPrefix.mPrefix.size() <= OT_IP6_ADDRESS_SIZE, error = OT_ERROR_INVALID_ARGS);

    std::copy(aExternalRoute.mPrefix.mPrefix.begin(), aExternalRoute.mPrefix.mPrefix.end(),
              &prefix.mPrefix.mFields.m8[0]);
    prefix.mLength      = aExternalRoute.mPrefix.mLength;
    otRoute.mPreference = aExternalRoute.mPreference;
    otRoute.mStable     = aExternalRoute.mStable;

    SuccessOrExit(error = otBorderRouterAddRoute(threadHelper->GetInstance(), &otRoute));
    if (aExternalRoute.mStable)
    {
        SuccessOrExit(error = otBorderRouterRegister(threadHelper->GetInstance()));
    }

    otbrLog(OTBR_LOG_INFO, "AddExternalRoute: Prefix:%s, Preference:%u, Stable:%u",
            otbr::Ip6Prefix(prefix).ToString().c_str(), otRoute.mPreference, otRoute.mStable);

exit:
    return static_cast<ThreadError>(error);
}

Return<ThreadError> HidlThread::removeExternalRoute(const ::android::hardware::thread::V1_0::Ip6Prefix &aPrefix)
{
    auto        threadHelper = mNcp->GetThreadHelper();
    otError     error        = OT_ERROR_NONE;
    otIp6Prefix ip6Prefix;

    VerifyOrExit(aPrefix.mPrefix.size() <= OT_IP6_ADDRESS_SIZE, error = OT_ERROR_INVALID_ARGS);

    std::copy(aPrefix.mPrefix.begin(), aPrefix.mPrefix.end(), &ip6Prefix.mPrefix.mFields.m8[0]);
    ip6Prefix.mLength = aPrefix.mLength;

    SuccessOrExit(error = otBorderRouterRemoveRoute(threadHelper->GetInstance(), &ip6Prefix));
    SuccessOrExit(error = otBorderRouterRegister(threadHelper->GetInstance()));

    otbrLog(OTBR_LOG_INFO, "RemoveExternalRoute: Prefix:%s", otbr::Ip6Prefix(ip6Prefix).ToString().c_str());

exit:
    return static_cast<ThreadError>(error);
}

Return<ThreadError> HidlThread::setMeshLocalPrefix(const hidl_array<uint8_t, 8> &aPrefix)
{
    auto              threadHelper = mNcp->GetThreadHelper();
    otMeshLocalPrefix networkPrefix;

    std::copy(&aPrefix[0], &aPrefix[OT_IP6_PREFIX_SIZE], std::begin(networkPrefix.m8));

    otbrLog(OTBR_LOG_INFO, "SetMeshLocalPrefix: Prefix:%s",
            otbr::Ip6NetworkPrefix(networkPrefix.m8).ToString().c_str());

    return static_cast<ThreadError>(otThreadSetMeshLocalPrefix(threadHelper->GetInstance(), &networkPrefix));
}

Return<ThreadError> HidlThread::setLegacyUlaPrefix(const hidl_array<uint8_t, 8> &aPrefix)
{
    OTBR_UNUSED_VARIABLE(aPrefix);

#if OTBR_ENABLE_LEGACY
    otbrLog(OTBR_LOG_INFO, "SetLegacyUlaPrefix: Prefix:%s", otbr::Ip6NetworkPrefix(&aPrefix[0]).ToString().c_str());
    otSetLegacyUlaPrefix(&aPrefix[0]);
#endif

    return ThreadError::ERROR_NONE;
}

Return<ThreadError> HidlThread::setLinkMode(const ::android::hardware::thread::V1_0::LinkModeConfig &aConfig)
{
    auto             threadHelper = mNcp->GetThreadHelper();
    otError          error        = OT_ERROR_NONE;
    otLinkModeConfig otCfg;

    otCfg.mDeviceType   = aConfig.mDeviceType;
    otCfg.mNetworkData  = aConfig.mNetworkData;
    otCfg.mRxOnWhenIdle = aConfig.mRxOnWhenIdle;

    VerifyOrExit((error = otThreadSetLinkMode(threadHelper->GetInstance(), otCfg)) == OT_ERROR_NONE);

    otbrLog(OTBR_LOG_INFO, "SetLinkMode: DeviceType:%u, NetworkData:%u, RxOnWhenIdle:%u", otCfg.mDeviceType,
            otCfg.mNetworkData, otCfg.mRxOnWhenIdle);

exit:
    return static_cast<ThreadError>(error);
}

Return<ThreadError> HidlThread::setRadioRegion(const hidl_string &region)
{
    auto     threadHelper = mNcp->GetThreadHelper();
    uint16_t regionCode;
    otError  error = OT_ERROR_NONE;

    VerifyOrExit(region.size() == sizeof(uint16_t), error = OT_ERROR_INVALID_ARGS);
    regionCode = region.c_str()[0] << 8 | region.c_str()[1];

    SuccessOrExit(error = otPlatRadioSetRegion(threadHelper->GetInstance(), regionCode));

    otbrLog(OTBR_LOG_INFO, "SetRegion: Region:%s", region.c_str());

exit:
    return static_cast<ThreadError>(error);
}

Return<void> HidlThread::getLinkMode(getLinkMode_cb _hidl_cb)
{
    auto                                              threadHelper = mNcp->GetThreadHelper();
    otLinkModeConfig                                  otCfg        = otThreadGetLinkMode(threadHelper->GetInstance());
    ::android::hardware::thread::V1_0::LinkModeConfig cfg;

    cfg.mDeviceType   = otCfg.mDeviceType;
    cfg.mNetworkData  = otCfg.mNetworkData;
    cfg.mRxOnWhenIdle = otCfg.mRxOnWhenIdle;

    otbrLog(OTBR_LOG_INFO, "GetLinkMode: DeviceType:%u, NetworkData:%u, RxOnWhenIdle:%u", otCfg.mDeviceType,
            otCfg.mNetworkData, otCfg.mRxOnWhenIdle);

    _hidl_cb(ThreadError::ERROR_NONE, cfg);

    return Void();
}

Return<void> HidlThread::getDeviceRole(getDeviceRole_cb _hidl_cb)
{
    auto         threadHelper = mNcp->GetThreadHelper();
    otDeviceRole role         = otThreadGetDeviceRole(threadHelper->GetInstance());

    otbrLog(OTBR_LOG_INFO, "GetDeviceRole: Role:%s", DeviceRoleToString(role).c_str());

    _hidl_cb(ThreadError::ERROR_NONE, static_cast<::android::hardware::thread::V1_0::DeviceRole>(role));

    return Void();
}

Return<void> HidlThread::getNetworkName(getNetworkName_cb _hidl_cb)
{
    auto        threadHelper = mNcp->GetThreadHelper();
    std::string networkName  = otThreadGetNetworkName(threadHelper->GetInstance());

    otbrLog(OTBR_LOG_INFO, "GetNetworkName: NetworkName:%s", networkName.c_str());

    _hidl_cb(ThreadError::ERROR_NONE, networkName);

    return Void();
}

Return<void> HidlThread::getPanId(getPanId_cb _hidl_cb)
{
    auto     threadHelper = mNcp->GetThreadHelper();
    uint16_t panId        = otLinkGetPanId(threadHelper->GetInstance());

    otbrLog(OTBR_LOG_INFO, "GetPanId: PanId:0x%04x", panId);

    _hidl_cb(ThreadError::ERROR_NONE, panId);

    return Void();
}

Return<void> HidlThread::getExtPanId(getExtPanId_cb _hidl_cb)
{
    auto                   threadHelper = mNcp->GetThreadHelper();
    const otExtendedPanId *extPanId     = otThreadGetExtendedPanId(threadHelper->GetInstance());
    uint64_t               extPanIdVal  = ArrayToUint64(extPanId->m8);

    otbrLog(OTBR_LOG_INFO, "GetExtPanId: ExtPanId:0x%s", otbr::ExtPanId(*extPanId).ToString().c_str());
    _hidl_cb(ThreadError::ERROR_NONE, extPanIdVal);

    return Void();
}

Return<void> HidlThread::getChannel(getChannel_cb _hidl_cb)
{
    auto     threadHelper = mNcp->GetThreadHelper();
    uint16_t channel      = otLinkGetChannel(threadHelper->GetInstance());

    otbrLog(OTBR_LOG_INFO, "GetChannel: Channel:%u", channel);
    _hidl_cb(ThreadError::ERROR_NONE, channel);

    return Void();
}

Return<void> HidlThread::getMasterKey(getMasterKey_cb _hidl_cb)
{
    auto                 threadHelper = mNcp->GetThreadHelper();
    const otMasterKey *  masterKey    = otThreadGetMasterKey(threadHelper->GetInstance());
    std::vector<uint8_t> keyVal(masterKey->m8, masterKey->m8 + sizeof(masterKey->m8));

    otbrLog(OTBR_LOG_INFO, "GetMasterKey: MasterKey:[Hiden]");
    _hidl_cb(ThreadError::ERROR_NONE, keyVal);

    return Void();
}

Return<void> HidlThread::getCcaFailureRate(getCcaFailureRate_cb _hidl_cb)
{
    auto     threadHelper = mNcp->GetThreadHelper();
    uint16_t failureRate  = otLinkGetCcaFailureRate(threadHelper->GetInstance());

    otbrLog(OTBR_LOG_INFO, "GetCcaFailureRate: FailureRate:%u", failureRate);
    _hidl_cb(ThreadError::ERROR_NONE, failureRate);

    return Void();
}

Return<void> HidlThread::getLinkCounters(getLinkCounters_cb _hidl_cb)
{
    auto                                           threadHelper = mNcp->GetThreadHelper();
    const otMacCounters *                          otCounters   = otLinkGetCounters(threadHelper->GetInstance());
    ::android::hardware::thread::V1_0::MacCounters counters;

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

    otbrLog(
        OTBR_LOG_INFO,
        "TxTotal:%u, TxUnicast:%u, TxBroadcast:%u, TxAckRequested:%u, TxAcked:%u, TxNoAckRequested: %u, TxData:%u, "
        "TxDataPoll:%u, TxBeacon:%u, TxBeaconRequest:%u, TxOther:%u, TxRetry:%u, TxErrCca:%u, TxErrAbort:%u, "
        "TxErrBusyChannel:%u, RxTotal:%u, RxUnicast:%u, RxBroadcast:%u, RxData:%u, RxDataPoll:%u, RxBeacon:%u, "
        "RxBeaconRequest:%u, RxOther:%u, RxAddressFiltered:%u, RxDestAddrFiltered:%u, RxDuplicated:%u, "
        "RxErrNoFrame:%u, RxErrNoUnknownNeighbor:%u, RxErrInvalidSrcAddr:%u, RxErrSec:%u, RxErrFcs:%u, RxErrOther:%u",
        otCounters->mTxTotal, otCounters->mTxUnicast, otCounters->mTxBroadcast, otCounters->mTxAckRequested,
        otCounters->mTxAcked, otCounters->mTxNoAckRequested, otCounters->mTxData, otCounters->mTxDataPoll,
        otCounters->mTxBeacon, otCounters->mTxBeaconRequest, otCounters->mTxOther, otCounters->mTxRetry,
        otCounters->mTxErrCca, otCounters->mTxErrAbort, otCounters->mTxErrBusyChannel, otCounters->mRxTotal,
        otCounters->mTxUnicast, otCounters->mRxBroadcast, otCounters->mRxData, otCounters->mTxDataPoll,
        otCounters->mRxBeacon, otCounters->mRxBeaconRequest, otCounters->mRxOther, otCounters->mRxAddressFiltered,
        otCounters->mRxDestAddrFiltered, otCounters->mRxDuplicated, otCounters->mRxErrNoFrame,
        otCounters->mRxErrUnknownNeighbor, otCounters->mRxErrInvalidSrcAddr, otCounters->mRxErrSec,
        otCounters->mRxErrFcs, otCounters->mRxErrOther);

    _hidl_cb(ThreadError::ERROR_NONE, counters);

    return Void();
}

Return<void> HidlThread::getIp6Counters(getIp6Counters_cb _hidl_cb)
{
    auto                                          threadHelper = mNcp->GetThreadHelper();
    const otIpCounters *                          otCounters   = otThreadGetIp6Counters(threadHelper->GetInstance());
    ::android::hardware::thread::V1_0::IpCounters counters;

    counters.mTxSuccess = otCounters->mTxSuccess;
    counters.mTxFailure = otCounters->mTxFailure;
    counters.mRxSuccess = otCounters->mRxSuccess;
    counters.mRxFailure = otCounters->mRxFailure;

    otbrLog(OTBR_LOG_INFO, "GetIp6Counters: TxSuccess:%u, TxFailure:%u, RxSuccess:%u, RxFailure:%u",
            otCounters->mTxSuccess, otCounters->mTxFailure, otCounters->mRxSuccess, otCounters->mRxFailure);

    _hidl_cb(ThreadError::ERROR_NONE, counters);

    return Void();
}

Return<void> HidlThread::getSupportedChannelMask(getSupportedChannelMask_cb _hidl_cb)
{
    auto     threadHelper = mNcp->GetThreadHelper();
    uint32_t channelMask  = otLinkGetSupportedChannelMask(threadHelper->GetInstance());

    otbrLog(OTBR_LOG_INFO, "GetSupportedChannelMask: ChannelMask:0x%08x", channelMask);

    _hidl_cb(ThreadError::ERROR_NONE, channelMask);

    return Void();
}

Return<void> HidlThread::getRloc16(getRloc16_cb _hidl_cb)
{
    auto     threadHelper = mNcp->GetThreadHelper();
    uint16_t rloc16       = otThreadGetRloc16(threadHelper->GetInstance());

    otbrLog(OTBR_LOG_INFO, "GetRloc16: Rloc16:0x%04x", rloc16);

    _hidl_cb(ThreadError::ERROR_NONE, rloc16);

    return Void();
}

Return<void> HidlThread::getExtendedAddress(getExtendedAddress_cb _hidl_cb)
{
    auto                threadHelper    = mNcp->GetThreadHelper();
    const otExtAddress *addr            = otLinkGetExtendedAddress(threadHelper->GetInstance());
    uint64_t            extendedAddress = ArrayToUint64(addr->m8);

    otbrLog(OTBR_LOG_INFO, "GetExtendedAddress: ExtAddr:%s", otbr::ExtAddress(*addr).ToString().c_str());

    _hidl_cb(ThreadError::ERROR_NONE, extendedAddress);

    return Void();
}

Return<void> HidlThread::getRouterId(getRouterId_cb _hidl_cb)
{
    otError      error        = OT_ERROR_NONE;
    auto         threadHelper = mNcp->GetThreadHelper();
    uint16_t     rloc16       = otThreadGetRloc16(threadHelper->GetInstance());
    otRouterInfo info;

    SuccessOrExit(error = otThreadGetRouterInfo(threadHelper->GetInstance(), rloc16, &info));

    otbrLog(OTBR_LOG_INFO, "GetRouterId: RouterId:0x%02x", info.mRouterId);

exit:
    _hidl_cb(static_cast<ThreadError>(error), info.mRouterId);

    return Void();
}

Return<void> HidlThread::getLeaderData(getLeaderData_cb _hidl_cb)
{
    otError                                       error        = OT_ERROR_NONE;
    auto                                          threadHelper = mNcp->GetThreadHelper();
    struct otLeaderData                           data;
    ::android::hardware::thread::V1_0::LeaderData leaderData;

    SuccessOrExit(error = otThreadGetLeaderData(threadHelper->GetInstance(), &data));
    leaderData.mPartitionId       = data.mPartitionId;
    leaderData.mWeighting         = data.mWeighting;
    leaderData.mDataVersion       = data.mDataVersion;
    leaderData.mStableDataVersion = data.mStableDataVersion;
    leaderData.mLeaderRouterId    = data.mLeaderRouterId;

    otbrLog(OTBR_LOG_INFO,
            "GetLeaderData: PartitionId:%u, Weighting:%u, DataVersion:%u, StableDataVersion:%u, LeaderRouterId:%u",
            data.mPartitionId, data.mWeighting, data.mDataVersion, data.mStableDataVersion, data.mLeaderRouterId);

exit:
    _hidl_cb(static_cast<ThreadError>(error), leaderData);

    return Void();
}

Return<void> HidlThread::getNetworkData(getNetworkData_cb _hidl_cb)
{
    otError                 error               = OT_ERROR_NONE;
    static constexpr size_t kNetworkDataMaxSize = 255;
    auto                    threadHelper        = mNcp->GetThreadHelper();
    uint8_t                 data[kNetworkDataMaxSize];
    uint8_t                 len = sizeof(data);
    std::vector<uint8_t>    networkData;

    SuccessOrExit(error = otNetDataGet(threadHelper->GetInstance(), /*stable=*/false, data, &len));
    networkData = std::vector<uint8_t>(&data[0], &data[len]);

    otbrLog(OTBR_LOG_INFO, "GetNetworkData");

exit:
    _hidl_cb(static_cast<ThreadError>(error), networkData);

    return Void();
}

Return<void> HidlThread::getStableNetworkData(getStableNetworkData_cb _hidl_cb)
{
    otError                 error               = OT_ERROR_NONE;
    static constexpr size_t kNetworkDataMaxSize = 255;
    auto                    threadHelper        = mNcp->GetThreadHelper();
    uint8_t                 data[kNetworkDataMaxSize];
    uint8_t                 len = sizeof(data);
    std::vector<uint8_t>    networkData;

    SuccessOrExit(error = otNetDataGet(threadHelper->GetInstance(), /*stable=*/true, data, &len));
    networkData = std::vector<uint8_t>(&data[0], &data[len]);

    otbrLog(OTBR_LOG_INFO, "GetStableNetworkData");

exit:
    _hidl_cb(static_cast<ThreadError>(error), networkData);

    return Void();
}

Return<void> HidlThread::getLocalLeaderWeight(getLocalLeaderWeight_cb _hidl_cb)
{
    auto    threadHelper = mNcp->GetThreadHelper();
    uint8_t weight       = otThreadGetLocalLeaderWeight(threadHelper->GetInstance());

    otbrLog(OTBR_LOG_INFO, "GetLocalLeaderWeight: Weight:%u", weight);

    _hidl_cb(ThreadError::ERROR_NONE, weight);

    return Void();
}

Return<void> HidlThread::getChannelMonitorSampleCount(getChannelMonitorSampleCount_cb _hidl_cb)
{
#if OPENTHREAD_CONFIG_CHANNEL_MONITOR_ENABLE
    auto     threadHelper = mNcp->GetThreadHelper();
    uint32_t cnt          = otChannelMonitorGetSampleCount(threadHelper->GetInstance());

    otbrLog(OTBR_LOG_INFO, "GetChannelMonitorSampleCount: Count:%u", cnt);

    _hidl_cb(ThreadError::ERROR_NONE, cnt);
#else
    _hidl_cb(ThreadError::OT_ERROR_NOT_IMPLEMENTED, 0);
#endif

    return Void();
}

Return<void> HidlThread::getChannelMonitorAllChannelQualities(getChannelMonitorAllChannelQualities_cb _hidl_cb)
{
#if OPENTHREAD_CONFIG_CHANNEL_MONITOR_ENABLE
    auto              threadHelper = mNcp->GetThreadHelper();
    uint32_t          channelMask  = otLinkGetSupportedChannelMask(threadHelper->GetInstance());
    constexpr uint8_t kNumChannels = sizeof(channelMask) * 8; // 8 bit per byte
    std::vector<::android::hardware::thread::V1_0::ChannelQuality> quality;

    otbrLog(OTBR_LOG_INFO, "GetChannelMonitorAllChannelQualities: ChannelMask:0x%08x", channelMask);
    for (uint8_t i = 0; i < kNumChannels; i++)
    {
        if (channelMask & (1U << i))
        {
            uint16_t occupancy = otChannelMonitorGetChannelOccupancy(threadHelper->GetInstance(), i);

            quality.emplace_back(ChannelQuality{i, occupancy});
            otbrLog(OTBR_LOG_INFO, "Channel: %d, Occupancy: %d", i, occupancy);
        }
    }

    _hidl_cb(ThreadError::ERROR_NONE, quality);
#else
    _hidl_cb(ThreadError::OT_ERROR_NOT_IMPLEMENTED, std::vector<::android::hardware::thread::V1_0::ChannelQuality>());
#endif

    return Void();
}

Return<void> HidlThread::getChildTable(getChildTable_cb _hidl_cb)
{
    auto                                                      threadHelper = mNcp->GetThreadHelper();
    uint16_t                                                  childIndex   = 0;
    otChildInfo                                               childInfo;
    std::vector<::android::hardware::thread::V1_0::ChildInfo> childTable;

    otbrLog(OTBR_LOG_INFO, "GetChildTable:");

    while (otThreadGetChildInfoByIndex(threadHelper->GetInstance(), childIndex, &childInfo) == OT_ERROR_NONE)
    {
        ::android::hardware::thread::V1_0::ChildInfo info;

        info.mExtAddress         = ArrayToUint64(childInfo.mExtAddress.m8);
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
        info.mFullThreadDevice   = childInfo.mFullThreadDevice;
        info.mFullNetworkData    = childInfo.mFullNetworkData;
        info.mIsStateRestoring   = childInfo.mIsStateRestoring;
        childTable.push_back(info);
        childIndex++;

        otbrLog(
            OTBR_LOG_INFO,
            "%d: ExtAddress:%s, Timeout:%u, Age:%u, ChildId:0x%04x, NetworkDataVersion:%u, "
            "LinkQualityIn:%u, AverageRssi:%d, LastRssi:%d, FrameErrorRate:%u, MessageErrorRate:%u, RxOnWhenIdle:%u, "
            "FullThreadDevice:%u, FullNetworkData:%u, IsStateRestoring:%u",
            childIndex, otbr::ExtAddress(childInfo.mExtAddress).ToString().c_str(), childInfo.mTimeout, childInfo.mAge,
            childInfo.mChildId, childInfo.mNetworkDataVersion, childInfo.mLinkQualityIn, childInfo.mAverageRssi,
            childInfo.mLastRssi, childInfo.mFrameErrorRate, childInfo.mMessageErrorRate, childInfo.mRxOnWhenIdle,
            childInfo.mFullThreadDevice, childInfo.mFullNetworkData, childInfo.mIsStateRestoring);
    }

    _hidl_cb(ThreadError::ERROR_NONE, childTable);

    return Void();
}

Return<void> HidlThread::getNeighborTable(getNeighborTable_cb _hidl_cb)
{
    auto                                                         threadHelper = mNcp->GetThreadHelper();
    otNeighborInfoIterator                                       iter         = OT_NEIGHBOR_INFO_ITERATOR_INIT;
    otNeighborInfo                                               neighborInfo;
    std::vector<::android::hardware::thread::V1_0::NeighborInfo> neighborTable;

    otbrLog(OTBR_LOG_INFO, "GetNeighborTable:");

    while (otThreadGetNextNeighborInfo(threadHelper->GetInstance(), &iter, &neighborInfo) == OT_ERROR_NONE)
    {
        ::android::hardware::thread::V1_0::NeighborInfo info;

        info.mExtAddress       = ArrayToUint64(neighborInfo.mExtAddress.m8);
        info.mAge              = neighborInfo.mAge;
        info.mRloc16           = neighborInfo.mRloc16;
        info.mLinkFrameCounter = neighborInfo.mLinkFrameCounter;
        info.mMleFrameCounter  = neighborInfo.mMleFrameCounter;
        info.mLinkQualityIn    = neighborInfo.mLinkQualityIn;
        info.mAverageRssi      = neighborInfo.mAverageRssi;
        info.mLastRssi         = neighborInfo.mLastRssi;
        info.mFrameErrorRate   = neighborInfo.mFrameErrorRate;
        info.mMessageErrorRate = neighborInfo.mMessageErrorRate;
        info.mRxOnWhenIdle     = neighborInfo.mRxOnWhenIdle;
        info.mFullThreadDevice = neighborInfo.mFullThreadDevice;
        info.mFullNetworkData  = neighborInfo.mFullNetworkData;
        info.mIsChild          = neighborInfo.mIsChild;
        neighborTable.push_back(info);

        otbrLog(
            OTBR_LOG_INFO,
            "ExtAddress:%s, Age:%u, Rloc16:0x%04x, LinkFrameCounter:%u, MleFrameCounter:%u"
            "LinkQualityIn:%u, AverageRssi:%d, LastRssi:%d, FrameErrorRate:%u, MessageErrorRate:%u, RxOnWhenIdle:%u, "
            "FullThreadDevice:%u, FullNetworkData:%u, IsChild:%u",
            otbr::ExtAddress(neighborInfo.mExtAddress).ToString().c_str(), neighborInfo.mAge, neighborInfo.mRloc16,
            neighborInfo.mLinkFrameCounter, neighborInfo.mMleFrameCounter, neighborInfo.mLinkQualityIn,
            neighborInfo.mAverageRssi, neighborInfo.mLastRssi, neighborInfo.mFrameErrorRate,
            neighborInfo.mMessageErrorRate, neighborInfo.mRxOnWhenIdle, neighborInfo.mFullThreadDevice,
            neighborInfo.mFullNetworkData, neighborInfo.mIsChild);
    }

    _hidl_cb(ThreadError::ERROR_NONE, neighborTable);

    return Void();
}

Return<void> HidlThread::getPartitionId(getPartitionId_cb _hidl_cb)
{
    auto     threadHelper = mNcp->GetThreadHelper();
    uint32_t partitionId  = otThreadGetPartitionId(threadHelper->GetInstance());

    otbrLog(OTBR_LOG_INFO, "GetPartitionId: PartitionId:%u", partitionId);

    _hidl_cb(ThreadError::ERROR_NONE, partitionId);

    return Void();
}

Return<void> HidlThread::getInstantRssi(getInstantRssi_cb _hidl_cb)
{
    auto   threadHelper = mNcp->GetThreadHelper();
    int8_t rssi         = otPlatRadioGetRssi(threadHelper->GetInstance());

    otbrLog(OTBR_LOG_INFO, "GetInstantRssi: Rssi:%d", rssi);
    _hidl_cb(ThreadError::ERROR_NONE, rssi);

    return Void();
}

Return<void> HidlThread::getRadioTxPower(getRadioTxPower_cb _hidl_cb)
{
    auto    threadHelper = mNcp->GetThreadHelper();
    otError error        = OT_ERROR_NONE;
    int8_t  txPower;

    SuccessOrExit(error = otPlatRadioGetTransmitPower(threadHelper->GetInstance(), &txPower));
    otbrLog(OTBR_LOG_INFO, "GetRadioTxPower: TxPower:%d", txPower);

exit:
    _hidl_cb(static_cast<ThreadError>(error), txPower);

    return Void();
}

Return<void> HidlThread::getExternalRoutes(getExternalRoutes_cb _hidl_cb)
{
    otError                                                       error        = OT_ERROR_NONE;
    auto                                                          threadHelper = mNcp->GetThreadHelper();
    otNetworkDataIterator                                         iter         = OT_NETWORK_DATA_ITERATOR_INIT;
    otExternalRouteConfig                                         config;
    std::vector<::android::hardware::thread::V1_0::ExternalRoute> externalRouteTable;

    otbrLog(OTBR_LOG_INFO, "GetExternalRoutes:");
    while (otBorderRouterGetNextRoute(threadHelper->GetInstance(), &iter, &config) == OT_ERROR_NONE)
    {
        ::android::hardware::thread::V1_0::ExternalRoute route;

        route.mPrefix.mPrefix.resize(OTBR_IP6_PREFIX_SIZE);
        std::copy(&config.mPrefix.mPrefix.mFields.m8[0], &config.mPrefix.mPrefix.mFields.m8[OTBR_IP6_PREFIX_SIZE],
                  std::begin(route.mPrefix.mPrefix));

        route.mPrefix.mLength      = config.mPrefix.mLength;
        route.mRloc16              = config.mRloc16;
        route.mPreference          = config.mPreference;
        route.mStable              = config.mStable;
        route.mNextHopIsThisDevice = config.mNextHopIsThisDevice;
        externalRouteTable.push_back(route);

        otbrLog(OTBR_LOG_INFO, "Prefix:%s, Rloc16:0x%04x, Preference:%u, Stable:%u, NextHopIsThisDevice:%u",
                otbr::Ip6Prefix(config.mPrefix).ToString().c_str(), config.mRloc16, config.mPreference, config.mStable,
                config.mNextHopIsThisDevice);
    }

    _hidl_cb(static_cast<ThreadError>(error), externalRouteTable);

    return Void();
}

Return<void> HidlThread::getRadioRegion(getRadioRegion_cb _hidl_cb)
{
    auto        threadHelper = mNcp->GetThreadHelper();
    otError     error        = OT_ERROR_NONE;
    std::string radioRegion;
    uint16_t    regionCode;

    SuccessOrExit(error = otPlatRadioGetRegion(threadHelper->GetInstance(), &regionCode));
    radioRegion.resize(sizeof(uint16_t), '\0');
    radioRegion[0] = static_cast<char>((regionCode >> 8) & 0xff);
    radioRegion[1] = static_cast<char>(regionCode & 0xff);

    otbrLog(OTBR_LOG_INFO, "GetRegion: Region:%s", radioRegion.c_str());

exit:
    _hidl_cb(static_cast<ThreadError>(error), radioRegion);

    return Void();
}

Return<ThreadError> HidlThread::setActiveDatasetTlvs(
    const ::android::hardware::thread::V1_0::OperationalDatasetTlvs &aDataset)
{
    auto                     threadHelper = mNcp->GetThreadHelper();
    otOperationalDatasetTlvs datasetTlvs;
    otError                  error = OT_ERROR_NONE;

    VerifyOrExit(aDataset.size() <= sizeof(datasetTlvs.mTlvs));
    std::copy(std::begin(aDataset), std::end(aDataset), std::begin(datasetTlvs.mTlvs));
    datasetTlvs.mLength = aDataset.size();
    error               = otDatasetSetActiveTlvs(threadHelper->GetInstance(), &datasetTlvs);

exit:
    return static_cast<ThreadError>(error);
}

Return<void> HidlThread::getActiveDatasetTlvs(getActiveDatasetTlvs_cb _hidl_cb)
{
    ::android::hardware::thread::V1_0::OperationalDatasetTlvs dataset;
    auto                                                      threadHelper = mNcp->GetThreadHelper();
    otError                                                   error        = OT_ERROR_NONE;
    otOperationalDatasetTlvs                                  datasetTlvs;

    SuccessOrExit(error = otDatasetGetActiveTlvs(threadHelper->GetInstance(), &datasetTlvs));
    dataset = hidl_vec<uint8_t>{std::begin(datasetTlvs.mTlvs), std::begin(datasetTlvs.mTlvs) + datasetTlvs.mLength};

exit:
    _hidl_cb(static_cast<ThreadError>(error), dataset);

    return Void();
}

} // namespace Hidl
} // namespace otbr
