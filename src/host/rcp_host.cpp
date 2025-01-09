/*
 *    Copyright (c) 2019, The OpenThread Authors.
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

#define OTBR_LOG_TAG "RCP_HOST"

#include "host/rcp_host.hpp"

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include <openthread/backbone_router_ftd.h>
#include <openthread/border_routing.h>
#include <openthread/dataset.h>
#include <openthread/dnssd_server.h>
#include <openthread/link_metrics.h>
#include <openthread/logging.h>
#include <openthread/nat64.h>
#include <openthread/srp_server.h>
#include <openthread/tasklet.h>
#include <openthread/thread.h>
#include <openthread/thread_ftd.h>
#include <openthread/trel.h>
#include <openthread/platform/logging.h>
#include <openthread/platform/misc.h>
#include <openthread/platform/radio.h>
#include <openthread/platform/settings.h>

#include "common/code_utils.hpp"
#include "common/logging.hpp"
#include "common/types.hpp"
#if OTBR_ENABLE_FEATURE_FLAGS
#include "proto/feature_flag.pb.h"
#endif

namespace otbr {
namespace Host {

static const uint16_t kThreadVersion11 = 2; ///< Thread Version 1.1
static const uint16_t kThreadVersion12 = 3; ///< Thread Version 1.2
static const uint16_t kThreadVersion13 = 4; ///< Thread Version 1.3
static const uint16_t kThreadVersion14 = 5; ///< Thread Version 1.4

// =============================== OtNetworkProperties ===============================

OtNetworkProperties::OtNetworkProperties(void)
    : mInstance(nullptr)
{
}

otDeviceRole OtNetworkProperties::GetDeviceRole(void) const
{
    return otThreadGetDeviceRole(mInstance);
}

bool OtNetworkProperties::Ip6IsEnabled(void) const
{
    return otIp6IsEnabled(mInstance);
}

uint32_t OtNetworkProperties::GetPartitionId(void) const
{
    return otThreadGetPartitionId(mInstance);
}

void OtNetworkProperties::GetDatasetActiveTlvs(otOperationalDatasetTlvs &aDatasetTlvs) const
{
    otError error = otDatasetGetActiveTlvs(mInstance, &aDatasetTlvs);

    if (error != OT_ERROR_NONE)
    {
        aDatasetTlvs.mLength = 0;
        memset(aDatasetTlvs.mTlvs, 0, sizeof(aDatasetTlvs.mTlvs));
    }
}

void OtNetworkProperties::GetDatasetPendingTlvs(otOperationalDatasetTlvs &aDatasetTlvs) const
{
    otError error = otDatasetGetPendingTlvs(mInstance, &aDatasetTlvs);

    if (error != OT_ERROR_NONE)
    {
        aDatasetTlvs.mLength = 0;
        memset(aDatasetTlvs.mTlvs, 0, sizeof(aDatasetTlvs.mTlvs));
    }
}

void OtNetworkProperties::SetInstance(otInstance *aInstance)
{
    mInstance = aInstance;
}

// =============================== RcpHost ===============================

RcpHost::RcpHost(const char                      *aInterfaceName,
                 const std::vector<const char *> &aRadioUrls,
                 const char                      *aBackboneInterfaceName,
                 bool                             aDryRun,
                 bool                             aEnableAutoAttach)
    : mInstance(nullptr)
    , mEnableAutoAttach(aEnableAutoAttach)
    , mThreadEnabledState(ThreadEnabledState::kStateDisabled)
{
    VerifyOrDie(aRadioUrls.size() <= OT_PLATFORM_CONFIG_MAX_RADIO_URLS, "Too many Radio URLs!");

    memset(&mConfig, 0, sizeof(mConfig));

    mConfig.mInterfaceName         = aInterfaceName;
    mConfig.mBackboneInterfaceName = aBackboneInterfaceName;
    mConfig.mDryRun                = aDryRun;

    for (const char *url : aRadioUrls)
    {
        mConfig.mCoprocessorUrls.mUrls[mConfig.mCoprocessorUrls.mNum++] = url;
    }
    mConfig.mSpeedUpFactor = 1;
}

RcpHost::~RcpHost(void)
{
    // Make sure OpenThread Instance was gracefully de-initialized.
    assert(mInstance == nullptr);
}

otbrLogLevel RcpHost::ConvertToOtbrLogLevel(otLogLevel aLogLevel)
{
    otbrLogLevel otbrLogLevel;

    switch (aLogLevel)
    {
    case OT_LOG_LEVEL_NONE:
        otbrLogLevel = OTBR_LOG_EMERG;
        break;
    case OT_LOG_LEVEL_CRIT:
        otbrLogLevel = OTBR_LOG_CRIT;
        break;
    case OT_LOG_LEVEL_WARN:
        otbrLogLevel = OTBR_LOG_WARNING;
        break;
    case OT_LOG_LEVEL_NOTE:
        otbrLogLevel = OTBR_LOG_NOTICE;
        break;
    case OT_LOG_LEVEL_INFO:
        otbrLogLevel = OTBR_LOG_INFO;
        break;
    case OT_LOG_LEVEL_DEBG:
    default:
        otbrLogLevel = OTBR_LOG_DEBUG;
        break;
    }

    return otbrLogLevel;
}

#if OTBR_ENABLE_FEATURE_FLAGS
/* Converts ProtoLogLevel to otbrLogLevel */
otbrLogLevel ConvertProtoToOtbrLogLevel(ProtoLogLevel aProtoLogLevel)
{
    otbrLogLevel otbrLogLevel;

    switch (aProtoLogLevel)
    {
    case PROTO_LOG_EMERG:
        otbrLogLevel = OTBR_LOG_EMERG;
        break;
    case PROTO_LOG_ALERT:
        otbrLogLevel = OTBR_LOG_ALERT;
        break;
    case PROTO_LOG_CRIT:
        otbrLogLevel = OTBR_LOG_CRIT;
        break;
    case PROTO_LOG_ERR:
        otbrLogLevel = OTBR_LOG_ERR;
        break;
    case PROTO_LOG_WARNING:
        otbrLogLevel = OTBR_LOG_WARNING;
        break;
    case PROTO_LOG_NOTICE:
        otbrLogLevel = OTBR_LOG_NOTICE;
        break;
    case PROTO_LOG_INFO:
        otbrLogLevel = OTBR_LOG_INFO;
        break;
    case PROTO_LOG_DEBUG:
    default:
        otbrLogLevel = OTBR_LOG_DEBUG;
        break;
    }

    return otbrLogLevel;
}
#endif

otError RcpHost::SetOtbrAndOtLogLevel(otbrLogLevel aLevel)
{
    otError error = OT_ERROR_NONE;
    otbrLogSetLevel(aLevel);
    error = otLoggingSetLevel(ConvertToOtLogLevel(aLevel));
    return error;
}

void RcpHost::Init(void)
{
    otbrError  error = OTBR_ERROR_NONE;
    otLogLevel level = ConvertToOtLogLevel(otbrLogGetLevel());

#if OTBR_ENABLE_FEATURE_FLAGS && OTBR_ENABLE_TREL
    FeatureFlagList featureFlagList;
#endif

    VerifyOrExit(otLoggingSetLevel(level) == OT_ERROR_NONE, error = OTBR_ERROR_OPENTHREAD);

    mInstance = otSysInit(&mConfig);
    assert(mInstance != nullptr);

    {
        otError result = otSetStateChangedCallback(mInstance, &RcpHost::HandleStateChanged, this);

        agent::ThreadHelper::LogOpenThreadResult("Set state callback", result);
        VerifyOrExit(result == OT_ERROR_NONE, error = OTBR_ERROR_OPENTHREAD);
    }

#if OTBR_ENABLE_FEATURE_FLAGS && OTBR_ENABLE_TREL
    // Enable/Disable trel according to feature flag default value.
    otTrelSetEnabled(mInstance, featureFlagList.enable_trel());
#endif

#if OTBR_ENABLE_SRP_ADVERTISING_PROXY
#if OTBR_ENABLE_SRP_SERVER_AUTO_ENABLE_MODE
    // Let SRP server use auto-enable mode. The auto-enable mode delegates the control of SRP server to the Border
    // Routing Manager. SRP server automatically starts when bi-directional connectivity is ready.
    otSrpServerSetAutoEnableMode(mInstance, /* aEnabled */ true);
#else
    otSrpServerSetEnabled(mInstance, /* aEnabled */ true);
#endif
#endif

#if !OTBR_ENABLE_FEATURE_FLAGS
    // Bring up all features when feature flags is not supported.
#if OTBR_ENABLE_NAT64
    otNat64SetEnabled(mInstance, /* aEnabled */ true);
#endif
#if OTBR_ENABLE_DNS_UPSTREAM_QUERY
    otDnssdUpstreamQuerySetEnabled(mInstance, /* aEnabled */ true);
#endif
#if OTBR_ENABLE_DHCP6_PD && OTBR_ENABLE_BORDER_ROUTING
    otBorderRoutingDhcp6PdSetEnabled(mInstance, /* aEnabled */ true);
#endif
#endif // OTBR_ENABLE_FEATURE_FLAGS

    mThreadHelper = MakeUnique<otbr::agent::ThreadHelper>(mInstance, this);

    OtNetworkProperties::SetInstance(mInstance);

exit:
    SuccessOrDie(error, "Failed to initialize the RCP Host!");
}

#if OTBR_ENABLE_FEATURE_FLAGS
otError RcpHost::ApplyFeatureFlagList(const FeatureFlagList &aFeatureFlagList)
{
    otError error = OT_ERROR_NONE;
    // Save a cached copy of feature flags for debugging purpose.
    mAppliedFeatureFlagListBytes = aFeatureFlagList.SerializeAsString();

#if OTBR_ENABLE_NAT64
    otNat64SetEnabled(mInstance, aFeatureFlagList.enable_nat64());
#endif

    if (aFeatureFlagList.enable_detailed_logging())
    {
        error = SetOtbrAndOtLogLevel(ConvertProtoToOtbrLogLevel(aFeatureFlagList.detailed_logging_level()));
    }
    else
    {
        error = SetOtbrAndOtLogLevel(otbrLogGetDefaultLevel());
    }

#if OTBR_ENABLE_TREL
    otTrelSetEnabled(mInstance, aFeatureFlagList.enable_trel());
#endif
#if OTBR_ENABLE_DNS_UPSTREAM_QUERY
    otDnssdUpstreamQuerySetEnabled(mInstance, aFeatureFlagList.enable_dns_upstream_query());
#endif
#if OTBR_ENABLE_DHCP6_PD
    otBorderRoutingDhcp6PdSetEnabled(mInstance, aFeatureFlagList.enable_dhcp6_pd());
#endif
#if OTBR_ENABLE_LINK_METRICS_TELEMETRY
    otLinkMetricsManagerSetEnabled(mInstance, aFeatureFlagList.enable_link_metrics_manager());
#endif

    return error;
}
#endif

void RcpHost::Deinit(void)
{
    assert(mInstance != nullptr);

    otSysDeinit();
    mInstance = nullptr;

    OtNetworkProperties::SetInstance(nullptr);
    mThreadStateChangedCallbacks.clear();
    mThreadEnabledStateChangedCallbacks.clear();
    mResetHandlers.clear();

    mJoinReceiver              = nullptr;
    mSetThreadEnabledReceiver  = nullptr;
    mScheduleMigrationReceiver = nullptr;
    mDetachGracefullyCallbacks.clear();
}

void RcpHost::HandleStateChanged(otChangedFlags aFlags)
{
    for (auto &stateCallback : mThreadStateChangedCallbacks)
    {
        stateCallback(aFlags);
    }

    mThreadHelper->StateChangedCallback(aFlags);

    if ((aFlags & OT_CHANGED_THREAD_ROLE) && IsAttached() && mJoinReceiver != nullptr)
    {
        otbrLogInfo("Join succeeded");
        SafeInvokeAndClear(mJoinReceiver, OT_ERROR_NONE, "Join succeeded");
    }
}

void RcpHost::Update(MainloopContext &aMainloop)
{
    if (otTaskletsArePending(mInstance))
    {
        aMainloop.mTimeout = ToTimeval(Microseconds::zero());
    }

    otSysMainloopUpdate(mInstance, &aMainloop);
}

void RcpHost::Process(const MainloopContext &aMainloop)
{
    otTaskletsProcess(mInstance);

    otSysMainloopProcess(mInstance, &aMainloop);

    if (IsAutoAttachEnabled() && mThreadHelper->TryResumeNetwork() == OT_ERROR_NONE)
    {
        DisableAutoAttach();
    }
}

bool RcpHost::IsAutoAttachEnabled(void)
{
    return mEnableAutoAttach;
}

void RcpHost::DisableAutoAttach(void)
{
    mEnableAutoAttach = false;
}

void RcpHost::PostTimerTask(Milliseconds aDelay, TaskRunner::Task<void> aTask)
{
    mTaskRunner.Post(std::move(aDelay), std::move(aTask));
}

void RcpHost::RegisterResetHandler(std::function<void(void)> aHandler)
{
    mResetHandlers.emplace_back(std::move(aHandler));
}

void RcpHost::AddThreadStateChangedCallback(ThreadStateChangedCallback aCallback)
{
    mThreadStateChangedCallbacks.emplace_back(std::move(aCallback));
}

void RcpHost::AddThreadEnabledStateChangedCallback(ThreadEnabledStateCallback aCallback)
{
    mThreadEnabledStateChangedCallbacks.push_back(aCallback);
}

void RcpHost::Reset(void)
{
    gPlatResetReason = OT_PLAT_RESET_REASON_SOFTWARE;

    otSysDeinit();
    mInstance = nullptr;

    Init();
    for (auto &handler : mResetHandlers)
    {
        handler();
    }
    mEnableAutoAttach = true;
}

const char *RcpHost::GetThreadVersion(void)
{
    const char *version;

    switch (otThreadGetVersion())
    {
    case kThreadVersion11:
        version = "1.1.1";
        break;
    case kThreadVersion12:
        version = "1.2.0";
        break;
    case kThreadVersion13:
        version = "1.3.0";
        break;
    case kThreadVersion14:
        version = "1.4.0";
        break;
    default:
        otbrLogEmerg("Unexpected thread version %hu", otThreadGetVersion());
        exit(-1);
    }
    return version;
}

static bool noNeedRejoin(const otOperationalDatasetTlvs &aLhs, const otOperationalDatasetTlvs &aRhs)
{
    bool result = false;

    otOperationalDataset lhsDataset;
    otOperationalDataset rhsDataset;

    SuccessOrExit(otDatasetParseTlvs(&aLhs, &lhsDataset));
    SuccessOrExit(otDatasetParseTlvs(&aRhs, &rhsDataset));

    result =
        (lhsDataset.mChannel == rhsDataset.mChannel) &&
        (memcmp(lhsDataset.mNetworkKey.m8, rhsDataset.mNetworkKey.m8, sizeof(lhsDataset.mNetworkKey)) == 0) &&
        (memcmp(lhsDataset.mExtendedPanId.m8, rhsDataset.mExtendedPanId.m8, sizeof(lhsDataset.mExtendedPanId)) == 0);

exit:
    return result;
}

void RcpHost::Join(const otOperationalDatasetTlvs &aActiveOpDatasetTlvs, const AsyncResultReceiver &aReceiver)
{
    otError                  error = OT_ERROR_NONE;
    std::string              errorMsg;
    bool                     receiveResultHere = true;
    otOperationalDatasetTlvs curDatasetTlvs;

    VerifyOrExit(mInstance != nullptr, error = OT_ERROR_INVALID_STATE, errorMsg = "OT is not initialized");
    VerifyOrExit(mThreadEnabledState != ThreadEnabledState::kStateDisabling, error = OT_ERROR_BUSY,
                 errorMsg = "Thread is disabling");
    VerifyOrExit(mThreadEnabledState == ThreadEnabledState::kStateEnabled, error = OT_ERROR_INVALID_STATE,
                 errorMsg = "Thread is not enabled");

    otbrLogInfo("Start joining...");

    error = otDatasetGetActiveTlvs(mInstance, &curDatasetTlvs);
    if (error == OT_ERROR_NONE && noNeedRejoin(aActiveOpDatasetTlvs, curDatasetTlvs) && IsAttached())
    {
        // Do not leave and re-join if this device has already joined the same network. This can help elimilate
        // unnecessary connectivity and topology disruption and save the time for re-joining. It's more useful for use
        // cases where Thread networks are dynamically brought up and torn down (e.g. Thread on mobile phones).
        SuccessOrExit(error    = otDatasetSetActiveTlvs(mInstance, &aActiveOpDatasetTlvs),
                      errorMsg = "Failed to set Active Operational Dataset");
        errorMsg = "Already Joined the target network";
        ExitNow();
    }

    if (GetDeviceRole() != OT_DEVICE_ROLE_DISABLED)
    {
        ThreadDetachGracefully([aActiveOpDatasetTlvs, aReceiver, this] {
            ConditionalErasePersistentInfo(true);
            Join(aActiveOpDatasetTlvs, aReceiver);
        });
        receiveResultHere = false;
        ExitNow();
    }

    SuccessOrExit(error    = otDatasetSetActiveTlvs(mInstance, &aActiveOpDatasetTlvs),
                  errorMsg = "Failed to set Active Operational Dataset");

    // TODO(b/273160198): check how we can implement join as a child
    SuccessOrExit(error = otIp6SetEnabled(mInstance, true), errorMsg = "Failed to bring up Thread interface");
    SuccessOrExit(error = otThreadSetEnabled(mInstance, true), errorMsg = "Failed to bring up Thread stack");

    // Abort an ongoing join()
    if (mJoinReceiver != nullptr)
    {
        SafeInvoke(mJoinReceiver, OT_ERROR_ABORT, "Join() is aborted");
    }
    mJoinReceiver     = aReceiver;
    receiveResultHere = false;

exit:
    if (receiveResultHere)
    {
        mTaskRunner.Post([aReceiver, error, errorMsg](void) { aReceiver(error, errorMsg); });
    }
}

void RcpHost::Leave(bool aEraseDataset, const AsyncResultReceiver &aReceiver)
{
    otError     error = OT_ERROR_NONE;
    std::string errorMsg;
    bool        receiveResultHere = true;

    VerifyOrExit(mInstance != nullptr, error = OT_ERROR_INVALID_STATE, errorMsg = "OT is not initialized");
    VerifyOrExit(mThreadEnabledState != ThreadEnabledState::kStateDisabling, error = OT_ERROR_BUSY,
                 errorMsg = "Thread is disabling");

    if (mThreadEnabledState == ThreadEnabledState::kStateDisabled)
    {
        ConditionalErasePersistentInfo(aEraseDataset);
        ExitNow();
    }

    ThreadDetachGracefully([aEraseDataset, aReceiver, this] {
        ConditionalErasePersistentInfo(aEraseDataset);
        if (aReceiver)
        {
            aReceiver(OT_ERROR_NONE, "");
        }
    });

exit:
    if (receiveResultHere)
    {
        mTaskRunner.Post([aReceiver, error, errorMsg](void) { aReceiver(error, errorMsg); });
    }
}

void RcpHost::ScheduleMigration(const otOperationalDatasetTlvs &aPendingOpDatasetTlvs,
                                const AsyncResultReceiver       aReceiver)
{
    otError              error = OT_ERROR_NONE;
    std::string          errorMsg;
    otOperationalDataset emptyDataset;

    VerifyOrExit(mInstance != nullptr, error = OT_ERROR_INVALID_STATE, errorMsg = "OT is not initialized");

    VerifyOrExit(mThreadEnabledState != ThreadEnabledState::kStateDisabling, error = OT_ERROR_BUSY,
                 errorMsg = "Thread is disabling");
    VerifyOrExit(mThreadEnabledState == ThreadEnabledState::kStateEnabled, error = OT_ERROR_INVALID_STATE,
                 errorMsg = "Thread is disabled");

    VerifyOrExit(IsAttached(), error = OT_ERROR_INVALID_STATE, errorMsg = "Device is detached");

    // TODO: check supported channel mask

    SuccessOrExit(error    = otDatasetSendMgmtPendingSet(mInstance, &emptyDataset, aPendingOpDatasetTlvs.mTlvs,
                                                         static_cast<uint8_t>(aPendingOpDatasetTlvs.mLength),
                                                         SendMgmtPendingSetCallback, this),
                  errorMsg = "Failed to send MGMT_PENDING_SET.req");

exit:
    if (error != OT_ERROR_NONE)
    {
        mTaskRunner.Post([aReceiver, error, errorMsg](void) { aReceiver(error, errorMsg); });
    }
    else
    {
        // otDatasetSendMgmtPendingSet() returns OT_ERROR_BUSY if it has already been called before but the
        // callback hasn't been invoked. So we can guarantee that mMigrationReceiver is always nullptr here
        assert(mScheduleMigrationReceiver == nullptr);
        mScheduleMigrationReceiver = aReceiver;
    }
}

void RcpHost::SendMgmtPendingSetCallback(otError aError, void *aContext)
{
    static_cast<RcpHost *>(aContext)->SendMgmtPendingSetCallback(aError);
}

void RcpHost::SendMgmtPendingSetCallback(otError aError)
{
    SafeInvokeAndClear(mScheduleMigrationReceiver, aError, "");
}

void RcpHost::SetThreadEnabled(bool aEnabled, const AsyncResultReceiver aReceiver)
{
    otError     error             = OT_ERROR_NONE;
    std::string errorMsg          = "";
    bool        receiveResultHere = true;

    VerifyOrExit(mInstance != nullptr, error = OT_ERROR_INVALID_STATE, errorMsg = "OT is not initialized");
    VerifyOrExit(mThreadEnabledState != ThreadEnabledState::kStateDisabling, error = OT_ERROR_BUSY,
                 errorMsg = "Thread is disabling");

    if (aEnabled)
    {
        otOperationalDatasetTlvs datasetTlvs;

        if (mThreadEnabledState == ThreadEnabledState::kStateEnabled)
        {
            ExitNow();
        }

        if (otDatasetGetActiveTlvs(mInstance, &datasetTlvs) != OT_ERROR_NOT_FOUND && datasetTlvs.mLength > 0 &&
            otThreadGetDeviceRole(mInstance) == OT_DEVICE_ROLE_DISABLED)
        {
            SuccessOrExit(error = otIp6SetEnabled(mInstance, true));
            SuccessOrExit(error = otThreadSetEnabled(mInstance, true));
        }
        UpdateThreadEnabledState(ThreadEnabledState::kStateEnabled);
    }
    else
    {
        UpdateThreadEnabledState(ThreadEnabledState::kStateDisabling);

        ThreadDetachGracefully([this](void) { DisableThreadAfterDetach(); });
        mSetThreadEnabledReceiver = aReceiver;
        receiveResultHere         = false;
    }

exit:
    if (receiveResultHere)
    {
        mTaskRunner.Post([aReceiver, error, errorMsg](void) { SafeInvoke(aReceiver, error, errorMsg); });
    }
}

void RcpHost::GetChannelMasks(const ChannelMasksReceiver &aReceiver, const AsyncResultReceiver &aErrReceiver)
{
    otError  error = OT_ERROR_NONE;
    uint32_t supportedChannelMask;
    uint32_t preferredChannelMask;

    VerifyOrExit(mInstance != nullptr, error = OT_ERROR_INVALID_STATE);

    supportedChannelMask = otLinkGetSupportedChannelMask(mInstance);
    preferredChannelMask = otPlatRadioGetPreferredChannelMask(mInstance);

exit:
    if (error == OT_ERROR_NONE)
    {
        mTaskRunner.Post([aReceiver, supportedChannelMask, preferredChannelMask](void) {
            aReceiver(supportedChannelMask, preferredChannelMask);
        });
    }
    else
    {
        mTaskRunner.Post([aErrReceiver, error](void) { aErrReceiver(error, "OT is not initialized"); });
    }
}

#if OTBR_ENABLE_POWER_CALIBRATION
void RcpHost::SetChannelMaxPowers(const std::vector<ChannelMaxPower> &aChannelMaxPowers,
                                  const AsyncResultReceiver          &aReceiver)
{
    otError     error = OT_ERROR_NONE;
    std::string errorMsg;

    VerifyOrExit(mInstance != nullptr, error = OT_ERROR_INVALID_STATE, errorMsg = "OT is not initialized");

    for (ChannelMaxPower channelMaxPower : aChannelMaxPowers)
    {
        VerifyOrExit((channelMaxPower.mChannel >= OT_RADIO_2P4GHZ_OQPSK_CHANNEL_MIN) &&
                         (channelMaxPower.mChannel <= OT_RADIO_2P4GHZ_OQPSK_CHANNEL_MAX),
                     error = OT_ERROR_INVALID_ARGS, errorMsg = "The channel is invalid");
    }

    for (ChannelMaxPower channelMaxPower : aChannelMaxPowers)
    {
        otbrLogInfo("Set channel max power: channel=%u, maxPower=%u", static_cast<uint32_t>(channelMaxPower.mChannel),
                    static_cast<uint32_t>(channelMaxPower.mMaxPower));
        SuccessOrExit(error = otPlatRadioSetChannelTargetPower(
                          mInstance, static_cast<uint8_t>(channelMaxPower.mChannel), channelMaxPower.mMaxPower),
                      errorMsg = "Failed to set channel max power");
    }

exit:
    mTaskRunner.Post([aReceiver, error, errorMsg](void) { aReceiver(error, errorMsg); });
}
#endif // OTBR_ENABLE_POWER_CALIBRATION

void RcpHost::ThreadDetachGracefully(const DetachGracefullyCallback &aCallback)
{
    mDetachGracefullyCallbacks.push_back(aCallback);

    // Ignores the OT_ERROR_BUSY error if a detach has already been requested
    OT_UNUSED_VARIABLE(otThreadDetachGracefully(mInstance, ThreadDetachGracefullyCallback, this));
}

void RcpHost::ThreadDetachGracefullyCallback(void *aContext)
{
    static_cast<RcpHost *>(aContext)->ThreadDetachGracefullyCallback();
}

void RcpHost::ThreadDetachGracefullyCallback(void)
{
    SafeInvokeAndClear(mJoinReceiver, OT_ERROR_ABORT, "Aborted by leave/disable operation");
    SafeInvokeAndClear(mScheduleMigrationReceiver, OT_ERROR_ABORT, "Aborted by leave/disable operation");

    for (auto &callback : mDetachGracefullyCallbacks)
    {
        callback();
    }
    mDetachGracefullyCallbacks.clear();
}

void RcpHost::ConditionalErasePersistentInfo(bool aErase)
{
    if (aErase)
    {
        OT_UNUSED_VARIABLE(otInstanceErasePersistentInfo(mInstance));
    }
}

void RcpHost::DisableThreadAfterDetach(void)
{
    otError     error = OT_ERROR_NONE;
    std::string errorMsg;

    SuccessOrExit(error = otThreadSetEnabled(mInstance, false), errorMsg = "Failed to disable Thread stack");
    SuccessOrExit(error = otIp6SetEnabled(mInstance, false), errorMsg = "Failed to disable Thread interface");

    UpdateThreadEnabledState(ThreadEnabledState::kStateDisabled);

exit:
    SafeInvokeAndClear(mSetThreadEnabledReceiver, error, errorMsg);
}

void RcpHost::SetCountryCode(const std::string &aCountryCode, const AsyncResultReceiver &aReceiver)
{
    static constexpr int kCountryCodeLength = 2;
    otError              error              = OT_ERROR_NONE;
    std::string          errorMsg;
    uint16_t             countryCode;

    VerifyOrExit((aCountryCode.length() == kCountryCodeLength) && isalpha(aCountryCode[0]) && isalpha(aCountryCode[1]),
                 error = OT_ERROR_INVALID_ARGS, errorMsg = "The country code is invalid");

    otbrLogInfo("Set country code: %c%c", aCountryCode[0], aCountryCode[1]);
    VerifyOrExit(mInstance != nullptr, error = OT_ERROR_INVALID_STATE, errorMsg = "OT is not initialized");

    countryCode = static_cast<uint16_t>((aCountryCode[0] << 8) | aCountryCode[1]);
    SuccessOrExit(error = otLinkSetRegion(mInstance, countryCode), errorMsg = "Failed to set the country code");

exit:
    mTaskRunner.Post([aReceiver, error, errorMsg](void) { aReceiver(error, errorMsg); });
}

bool RcpHost::IsAttached(void)
{
    otDeviceRole role = GetDeviceRole();

    return role == OT_DEVICE_ROLE_CHILD || role == OT_DEVICE_ROLE_ROUTER || role == OT_DEVICE_ROLE_LEADER;
}

void RcpHost::UpdateThreadEnabledState(ThreadEnabledState aState)
{
    mThreadEnabledState = aState;

    for (auto &callback : mThreadEnabledStateChangedCallbacks)
    {
        callback(mThreadEnabledState);
    }
}

/*
 * Provide, if required an "otPlatLog()" function
 */
extern "C" void otPlatLog(otLogLevel aLogLevel, otLogRegion aLogRegion, const char *aFormat, ...)
{
    OT_UNUSED_VARIABLE(aLogRegion);

    otbrLogLevel otbrLogLevel = RcpHost::ConvertToOtbrLogLevel(aLogLevel);

    va_list ap;
    va_start(ap, aFormat);
    otbrLogvNoFilter(otbrLogLevel, aFormat, ap);
    va_end(ap);
}

extern "C" void otPlatLogHandleLevelChanged(otLogLevel aLogLevel)
{
    otbrLogSetLevel(RcpHost::ConvertToOtbrLogLevel(aLogLevel));
    otbrLogInfo("OpenThread log level changed to %d", aLogLevel);
}

} // namespace Host
} // namespace otbr
