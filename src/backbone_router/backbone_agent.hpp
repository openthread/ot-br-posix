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

/**
 * @file
 *   This file includes definition for Thread Backbone agent.
 */

#ifndef BACKBONE_ROUTER_BACKBONE_AGENT_HPP_
#define BACKBONE_ROUTER_BACKBONE_AGENT_HPP_

#include <openthread/backbone_router_ftd.h>

#include "agent/instance_params.hpp"
#include "agent/ncp_openthread.hpp"
#include "backbone_router/nd_proxy.hpp"

namespace otbr {
namespace BackboneRouter {

/**
 * @addtogroup border-router-backbone
 *
 * @brief
 *   This module includes definition for Thread Backbone agent.
 *
 * @{
 */

/**
 * This class implements Thread Backbone agent functionality.
 *
 */
class BackboneAgent
{
public:
    /**
     * This constructor intiializes the `BackboneAgent` instance.
     *
     * @param[in] aNcp  The Thread instance.
     *
     */
    BackboneAgent(otbr::Ncp::ControllerOpenThread &aNcp);

    /**
     * This method initializes the Backbone agent.
     *
     */
    void Init(void);

    /**
     * This method updates the fd_set and timeout for mainloop.
     *
     * @param[inout]    aReadFdSet      A reference to fd_set for polling read.
     * @param[inout]    aWriteFdSet     A reference to fd_set for polling read.
     * @param[inout]    aErrorFdSet     A reference to fd_set for polling error.
     * @param[inout]    aMaxFd          A reference to the current max fd in @p aReadFdSet and @p aWriteFdSet.
     * @param[inout]    aTimeout        A reference to the timeout.
     *
     */
    void UpdateFdSet(fd_set & aReadFdSet,
                     fd_set & aWriteFdSet,
                     fd_set & aErrorFdSet,
                     int &    aMaxFd,
                     timeval &aTimeout) const;

    /**
     * This method performs border agent processing.
     *
     * @param[in]   aReadFdSet   A reference to read file descriptors.
     * @param[in]   aWriteFdSet  A reference to write file descriptors.
     * @param[in]   aErrorFdSet  A reference to error file descriptors.
     *
     */
    void Process(const fd_set &aReadFdSet, const fd_set &aWriteFdSet, const fd_set &aErrorFdSet);

private:
    void        OnBecomePrimary(void);
    void        OnResignPrimary(void);
    bool        IsPrimary(void) const { return mBackboneRouterState == OT_BACKBONE_ROUTER_STATE_PRIMARY; }
    static void HandleBackboneRouterState(void *aContext, int aEvent, va_list aArguments);
    void        HandleBackboneRouterState(void);
    static void HandleBackboneRouterDomainPrefixEvent(void *aContext, int aEvent, va_list aArguments);
    void        HandleBackboneRouterDomainPrefixEvent(otBackboneRouterDomainPrefixEvent aEvent,
                                                      const otIp6Prefix *               aDomainPrefix);
    static void HandleBackboneRouterNdProxyEvent(void *aContext, int aEvent, va_list aArguments);
    void        HandleBackboneRouterNdProxyEvent(otBackboneRouterNdProxyEvent aEvent, const otIp6Address *aAddress);

    static const char *StateToString(otBackboneRouterState aState);

    otbr::Ncp::ControllerOpenThread &mNcp;
    otBackboneRouterState            mBackboneRouterState;
    NdProxyManager                   mNdProxyManager;
    Ip6Prefix                        mDomainPrefix;
};

/**
 * @}
 */

} // namespace BackboneRouter
} // namespace otbr

#endif // BACKBONE_ROUTER_BACKBONE_AGENT_HPP_
