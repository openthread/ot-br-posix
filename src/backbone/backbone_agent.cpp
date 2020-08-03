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
 *   The file implements the Thread backbone agent.
 */

#include "backbone_agent.hpp"

#include <assert.h>
#include <common/code_utils.hpp>
#include <openthread/backbone_router_ftd.h>

#include "backbone_helper.hpp"
#include "utils/hex.hpp"

namespace otbr {

namespace Backbone {

BackboneAgent::BackboneAgent(otbr::Ncp::ControllerOpenThread *aThread)
    : mThread(*aThread)
    , mBackboneRouterState(OT_BACKBONE_ROUTER_STATE_DISABLED)
{
}

void BackboneAgent::Init(const std::string &aThreadIfName, const std::string &aBackboneIfName)
{
    mSmcrouteManager.Init(aThreadIfName, aBackboneIfName);

    HandleBackboneRouterState();
    HandleBackboneRouterLocal();
}

void BackboneAgent::HandleBackboneRouterState(void)
{
    otBackboneRouterState state = otBackboneRouterGetState(mThread.GetInstance());
    bool                  backboneWasOn, backboneWasPrimary;

    Log(OTBR_LOG_DEBUG, "HandleBackboneRouterState: state=%d, mBackboneRouterState=%d", state, mBackboneRouterState);
    VerifyOrExit(mBackboneRouterState != state);

    backboneWasOn        = (mBackboneRouterState != OT_BACKBONE_ROUTER_STATE_DISABLED);
    backboneWasPrimary   = (mBackboneRouterState == OT_BACKBONE_ROUTER_STATE_PRIMARY);
    mBackboneRouterState = state;

    if (!backboneWasOn && mBackboneRouterState != OT_BACKBONE_ROUTER_STATE_DISABLED)
    {
        BackboneUp();
    }

    if (!backboneWasPrimary && mBackboneRouterState == OT_BACKBONE_ROUTER_STATE_PRIMARY)
    {
        EnterPrimary();
    }
    else if (backboneWasPrimary && mBackboneRouterState != OT_BACKBONE_ROUTER_STATE_PRIMARY)
    {
        ExitPrimary();
    }

    if (backboneWasOn && mBackboneRouterState == OT_BACKBONE_ROUTER_STATE_DISABLED)
    {
        BackboneDown();
    }

exit:
    return;
}

void BackboneAgent::HandleBackboneRouterLocal(void)
{
    otBackboneRouterState state = otBackboneRouterGetState(mThread.GetInstance());

    Log(OTBR_LOG_DEBUG, "HandleBackboneRouterLocal: state=%d", state);
}

void BackboneAgent::Log(int aLevel, const char *aFormat, ...)
{
    va_list ap;

    va_start(ap, aFormat);
    BackboneHelper::Logv(aLevel, "BackboneAgent", aFormat, ap);
    va_end(ap);
}

void BackboneAgent::BackboneUp(void)
{
    Log(OTBR_LOG_INFO, "Backbone turned up!");
}

void BackboneAgent::BackboneDown(void)
{
    Log(OTBR_LOG_INFO, "Backbone turned down!");
}

void BackboneAgent::EnterPrimary(void)
{
    Log(OTBR_LOG_INFO, "Backbone enters primary!");

    mSmcrouteManager.Enable();
}

void BackboneAgent::ExitPrimary(void)
{
    Log(OTBR_LOG_INFO, "Backbone exits primary to %d!", mBackboneRouterState);
    mSmcrouteManager.Disable();
}

void BackboneAgent::HandleBackboneRouterMulticastListenerEvent(otBackboneRouterMulticastListenerEvent aEvent,
                                                               const otIp6Address &                   aAddress)
{
    Log(OTBR_LOG_INFO, "Multicast Listener event: %d, address: %s, IsPrimary: %s", aEvent,
        Ip6Address(aAddress).ToExtendedString().c_str(), IsPrimary() ? "Y" : "N");

    VerifyOrExit(IsPrimary());

    switch (aEvent)
    {
    case OT_BACKBONE_ROUTER_MULTICAST_LISTENER_ADDED:
        mSmcrouteManager.Add(Ip6Address(aAddress));
        break;
    case OT_BACKBONE_ROUTER_MULTICAST_LISTENER_REMOVED:
        mSmcrouteManager.Remove(Ip6Address(aAddress));
        break;
    }

exit:
    return;
}

} // namespace Backbone

} // namespace otbr
