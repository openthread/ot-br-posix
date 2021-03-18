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
 *   This file includes definitions for NCP service.
 */

#ifndef OTBR_AGENT_NCP_OPENTHREAD_HPP_
#define OTBR_AGENT_NCP_OPENTHREAD_HPP_

#include <chrono>
#include <memory>

#include <openthread/backbone_router_ftd.h>
#include <openthread/cli.h>
#include <openthread/instance.h>
#include <openthread/openthread-system.h>

#include "agent/thread_helper.hpp"
#include "common/mainloop.hpp"
#include "common/task_runner.hpp"

namespace otbr {
namespace Ncp {

/**
 * This interface defines NCP Controller functionality.
 *
 */
class ControllerOpenThread : public MainloopProcessor
{
public:
    using ThreadStateChangedCallback = std::function<void(otChangedFlags aFlags)>;

    /**
     * This constructor initializes this object.
     *
     * @param[in]   aInterfaceName          A string of the NCP interface name.
     * @param[in]   aRadioUrl               The URL describes the radio chip.
     * @param[in]   aBackboneInterfaceName  The Backbone network interface name.
     *
     */
    ControllerOpenThread(const char *aInterfaceName, const char *aRadioUrl, const char *aBackboneInterfaceName);

    /**
     * This method initalize the NCP controller.
     *
     * @retval  OTBR_ERROR_NONE     Successfully initialized NCP controller.
     *
     */
    otbrError Init(void);

    /**
     * This method get mInstance pointer.
     *
     * @retval  the pointer of mInstance.
     *
     */
    otInstance *GetInstance(void) { return mInstance; }

    /**
     * This method gets the thread functionality helper.
     *
     * @retval  the pointer to the helper object.
     *
     */
    otbr::agent::ThreadHelper *GetThreadHelper(void) { return mThreadHelper.get(); }

    /**
     * This method updates the mainloop context.
     *
     * @param[inout]  aMainloop  A reference to the mainloop to be updated.
     *
     */
    void Update(MainloopContext &aMainloop) override;

    /**
     * This method processes mainloop events.
     *
     * @param[in]  aMainloop  A reference to the mainloop context.
     *
     */
    void Process(const MainloopContext &aMainloop) override;

    /**
     * This method posts a task to the timer
     *
     * @param[in]   aDelay  The delay in milliseconds before executing the task.
     * @param[in]   aTask   The task function.
     *
     */
    void PostTimerTask(Milliseconds aDelay, TaskRunner::Task<void> aTask);

    /**
     * This method registers a reset handler.
     *
     * @param[in]   aHandler  The handler function.
     *
     */
    void RegisterResetHandler(std::function<void(void)> aHandler);

    void AddThreadStateChangedCallback(ThreadStateChangedCallback aCallback);

    ~ControllerOpenThread(void) override;

private:
    static void HandleStateChanged(otChangedFlags aFlags, void *aContext)
    {
        static_cast<ControllerOpenThread *>(aContext)->HandleStateChanged(aFlags);
    }
    void HandleStateChanged(otChangedFlags aFlags);

    static void HandleBackboneRouterDomainPrefixEvent(void *                            aContext,
                                                      otBackboneRouterDomainPrefixEvent aEvent,
                                                      const otIp6Prefix *               aDomainPrefix);
    void        HandleBackboneRouterDomainPrefixEvent(otBackboneRouterDomainPrefixEvent aEvent,
                                                      const otIp6Prefix *               aDomainPrefix);

#if OTBR_ENABLE_DUA_ROUTING
    static void HandleBackboneRouterNdProxyEvent(void *                       aContext,
                                                 otBackboneRouterNdProxyEvent aEvent,
                                                 const otIp6Address *         aAddress);
    void        HandleBackboneRouterNdProxyEvent(otBackboneRouterNdProxyEvent aEvent, const otIp6Address *aAddress);
#endif

    otInstance *mInstance;

    otPlatformConfig                           mConfig;
    std::unique_ptr<otbr::agent::ThreadHelper> mThreadHelper;
    std::vector<std::function<void(void)>>     mResetHandlers;
    TaskRunner                                 mTaskRunner;
    std::vector<ThreadStateChangedCallback>    mThreadStateChangedCallbacks;
};

} // namespace Ncp
} // namespace otbr

#endif // OTBR_AGENT_NCP_OPENTHREAD_HPP_
