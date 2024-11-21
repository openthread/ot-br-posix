/*
 *  Copyright (c) 2017, The OpenThread Authors.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 *   This file includes definitions of Thread Controller under RCP mode.
 */

#ifndef OTBR_AGENT_RCP_HOST_HPP_
#define OTBR_AGENT_RCP_HOST_HPP_

#include "openthread-br/config.h"

#include <chrono>
#include <memory>

#include <assert.h>

#include <openthread/backbone_router_ftd.h>
#include <openthread/cli.h>
#include <openthread/instance.h>
#include <openthread/openthread-system.h>

#include "common/mainloop.hpp"
#include "common/task_runner.hpp"
#include "common/types.hpp"
#include "ncp/thread_host.hpp"
#include "utils/thread_helper.hpp"

namespace otbr {
#if OTBR_ENABLE_FEATURE_FLAGS
// Forward declaration of FeatureFlagList proto.
class FeatureFlagList;
#endif

namespace Ncp {

/**
 * This class implements the NetworkProperties for architectures where OT APIs are directly accessible.
 */
class OtNetworkProperties : virtual public NetworkProperties
{
public:
    /**
     * Constructor.
     */
    explicit OtNetworkProperties(void);

    // NetworkProperties methods
    otDeviceRole GetDeviceRole(void) const override;
    bool         Ip6IsEnabled(void) const override;
    uint32_t     GetPartitionId(void) const override;
    void         GetDatasetActiveTlvs(otOperationalDatasetTlvs &aDatasetTlvs) const override;
    void         GetDatasetPendingTlvs(otOperationalDatasetTlvs &aDatasetTlvs) const override;

    // Set the otInstance
    void SetInstance(otInstance *aInstance);

private:
    otInstance *mInstance;
};

/**
 * This interface defines OpenThread Controller under RCP mode.
 */
class RcpHost : public MainloopProcessor, public ThreadHost, public OtNetworkProperties
{
public:
    /**
     * This constructor initializes this object.
     *
     * @param[in]   aInterfaceName          A string of the NCP interface name.
     * @param[in]   aRadioUrls              The radio URLs (can be IEEE802.15.4 or TREL radio).
     * @param[in]   aBackboneInterfaceName  The Backbone network interface name.
     * @param[in]   aDryRun                 TRUE to indicate dry-run mode. FALSE otherwise.
     * @param[in]   aEnableAutoAttach       Whether or not to automatically attach to the saved network.
     */
    RcpHost(const char                      *aInterfaceName,
            const std::vector<const char *> &aRadioUrls,
            const char                      *aBackboneInterfaceName,
            bool                             aDryRun,
            bool                             aEnableAutoAttach);

    /**
     * This method initialize the Thread controller.
     */
    void Init(void) override;

    /**
     * This method deinitialize the Thread controller.
     */
    void Deinit(void) override;

    /**
     * Returns an OpenThread instance.
     *
     * @retval Non-null OpenThread instance if `RcpHost::Init()` has been called.
     *         Otherwise, it's guaranteed to be `null`
     */
    otInstance *GetInstance(void) { return mInstance; }

    /**
     * This method gets the thread functionality helper.
     *
     * @retval The pointer to the helper object.
     */
    otbr::agent::ThreadHelper *GetThreadHelper(void)
    {
        assert(mThreadHelper != nullptr);
        return mThreadHelper.get();
    }

    void Update(MainloopContext &aMainloop) override;
    void Process(const MainloopContext &aMainloop) override;

    /**
     * This method posts a task to the timer
     *
     * @param[in] aDelay  The delay in milliseconds before executing the task.
     * @param[in] aTask   The task function.
     */
    void PostTimerTask(Milliseconds aDelay, TaskRunner::Task<void> aTask);

    /**
     * This method registers a reset handler.
     *
     * @param[in] aHandler  The handler function.
     */
    void RegisterResetHandler(std::function<void(void)> aHandler);

    /**
     * This method resets the OpenThread instance.
     */
    void Reset(void);

    /**
     * This method returns the Thread protocol version as a string.
     *
     * @returns A pointer to the Thread version string.
     */
    static const char *GetThreadVersion(void);

    /**
     * This method returns the Thread network interface name.
     *
     * @returns A pointer to the Thread network interface name string.
     */
    const char *GetInterfaceName(void) const override { return mConfig.mInterfaceName; }

    static otbrLogLevel ConvertToOtbrLogLevel(otLogLevel aLogLevel);

#if OTBR_ENABLE_FEATURE_FLAGS
    /**
     * Apply the feature flag values to OpenThread through OpenThread APIs.
     *
     * @param[in] aFeatureFlagList  The feature flag list to be applied to OpenThread.
     *
     * @returns The error value of underlying OpenThread API calls.
     */
    otError ApplyFeatureFlagList(const FeatureFlagList &aFeatureFlagList);

    /**
     * This method returns the applied FeatureFlagList in ApplyFeatureFlagList call.
     *
     * @returns the applied FeatureFlagList's serialized bytes.
     */
    const std::string &GetAppliedFeatureFlagListBytes(void)
    {
        return mAppliedFeatureFlagListBytes;
    }
#endif

    ~RcpHost(void) override;

    // Thread Control virtual methods
    void Join(const otOperationalDatasetTlvs &aActiveOpDatasetTlvs, const AsyncResultReceiver &aRecevier) override;
    void Leave(const AsyncResultReceiver &aRecevier) override;
    void ScheduleMigration(const otOperationalDatasetTlvs &aPendingOpDatasetTlvs,
                           const AsyncResultReceiver       aReceiver) override;
    void SetThreadEnabled(bool aEnabled, const AsyncResultReceiver aReceiver) override;
    void SetCountryCode(const std::string &aCountryCode, const AsyncResultReceiver &aReceiver) override;
    void GetChannelMasks(const ChannelMasksReceiver &aReceiver, const AsyncResultReceiver &aErrReceiver) override;
#if OTBR_ENABLE_POWER_CALIBRATION
    void SetChannelMaxPowers(const std::vector<ChannelMaxPower> &aChannelMaxPowers,
                             const AsyncResultReceiver          &aReceiver) override;
#endif
    void AddThreadStateChangedCallback(ThreadStateChangedCallback aCallback) override;
    void AddThreadEnabledStateChangedCallback(ThreadEnabledStateCallback aCallback) override;

    CoprocessorType GetCoprocessorType(void) override
    {
        return OT_COPROCESSOR_RCP;
    }

    const char *GetCoprocessorVersion(void) override
    {
        return otPlatRadioGetVersionString(mInstance);
    }

private:
    static void SafeInvokeAndClear(AsyncResultReceiver &aReceiver, otError aError, const std::string &aErrorInfo = "")
    {
        if (aReceiver)
        {
            aReceiver(aError, aErrorInfo);
            aReceiver = nullptr;
        }
    }

    static void HandleStateChanged(otChangedFlags aFlags, void *aContext)
    {
        static_cast<RcpHost *>(aContext)->HandleStateChanged(aFlags);
    }
    void HandleStateChanged(otChangedFlags aFlags);

    static void HandleBackboneRouterDomainPrefixEvent(void                             *aContext,
                                                      otBackboneRouterDomainPrefixEvent aEvent,
                                                      const otIp6Prefix                *aDomainPrefix);
    void        HandleBackboneRouterDomainPrefixEvent(otBackboneRouterDomainPrefixEvent aEvent,
                                                      const otIp6Prefix                *aDomainPrefix);

#if OTBR_ENABLE_DUA_ROUTING
    static void HandleBackboneRouterNdProxyEvent(void                        *aContext,
                                                 otBackboneRouterNdProxyEvent aEvent,
                                                 const otIp6Address          *aAddress);
    void        HandleBackboneRouterNdProxyEvent(otBackboneRouterNdProxyEvent aEvent, const otIp6Address *aAddress);
#endif

    static void DisableThreadAfterDetach(void *aContext);
    void        DisableThreadAfterDetach(void);
    static void SendMgmtPendingSetCallback(otError aError, void *aContext);
    void        SendMgmtPendingSetCallback(otError aError);

    bool IsAutoAttachEnabled(void);
    void DisableAutoAttach(void);

    bool IsAttached(void);

    void UpdateThreadEnabledState(ThreadEnabledState aState);

    otError SetOtbrAndOtLogLevel(otbrLogLevel aLevel);

    otInstance *mInstance;

    otPlatformConfig                           mConfig;
    std::unique_ptr<otbr::agent::ThreadHelper> mThreadHelper;
    std::vector<std::function<void(void)>>     mResetHandlers;
    TaskRunner                                 mTaskRunner;

    std::vector<ThreadStateChangedCallback> mThreadStateChangedCallbacks;
    std::vector<ThreadEnabledStateCallback> mThreadEnabledStateChangedCallbacks;
    bool                                    mEnableAutoAttach = false;
    ThreadEnabledState                      mThreadEnabledState;
    AsyncResultReceiver                     mSetThreadEnabledReceiver;
    AsyncResultReceiver                     mScheduleMigrationReceiver;

#if OTBR_ENABLE_FEATURE_FLAGS
    // The applied FeatureFlagList in ApplyFeatureFlagList call, used for debugging purpose.
    std::string mAppliedFeatureFlagListBytes;
#endif
};

} // namespace Ncp
} // namespace otbr

#endif // OTBR_AGENT_RCP_HOST_HPP_
