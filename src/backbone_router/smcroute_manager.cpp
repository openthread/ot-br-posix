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
 *   The file implements the SMCRoute manager.
 */

#include "backbone_router/smcroute_manager.hpp"

#include <assert.h>
#include <unistd.h>

#include <openthread/backbone_router_ftd.h>

#include "common/code_utils.hpp"
#include "utils/system_utils.hpp"

namespace otbr {

namespace BackboneRouter {

void SMCRouteManager::Init(void)
{
    assert(!mEnabled);

    StartSMCRouteService();
}

void SMCRouteManager::Enable(void)
{
    otbrError error = OTBR_ERROR_NONE;

    VerifyOrExit(!mEnabled);

    mEnabled = true;

    Flush();

    // Add mroute rules: 65520 (0xfff0) allow outbound for MA scope >= admin (4)
    SuccessOrExit(error = AllowOutboundMulticast());

    // Add mroute rules for the current Multicast Listeners Table
    for (const Ip6Address &address : mListenerSet)
    {
        SuccessOrExit(error = AddRoute(address));
    }

exit:
    otbrLogResult(error, "SMCRouteManager: %s", __FUNCTION__);
}

void SMCRouteManager::Disable(void)
{
    otbrError error = OTBR_ERROR_NONE;

    VerifyOrExit(mEnabled);

    mEnabled = false;

    Flush();

    // Remove mroute rules for the current Multicast Listeners Table
    for (const Ip6Address &address : mListenerSet)
    {
        SuccessOrExit(error = DeleteRoute(address));
    }

    // Remove mroute rules: forbid outbound Multicast traffic
    SuccessOrExit(error = ForbidOutboundMulticast());

exit:
    otbrLogResult(error, "SMCRouteManager: %s", __FUNCTION__);
}

void SMCRouteManager::StartSMCRouteService(void)
{
    otbrError                             error = OTBR_ERROR_NONE;
    std::chrono::system_clock::time_point deadline;

    VerifyOrExit(SystemUtils::ExecuteCommand("smcroutectl kill || true") == 0, error = OTBR_ERROR_SMCROUTE);
    VerifyOrExit(SystemUtils::ExecuteCommand("smcrouted") == 0, error = OTBR_ERROR_SMCROUTE);

    deadline = std::chrono::system_clock::now() + std::chrono::seconds(10);

    while (std::chrono::system_clock::now() < deadline)
    {
        usleep(10000);

        VerifyOrExit((error = Flush()) != OTBR_ERROR_NONE);
    }

exit:
    SuccessOrDie(error, "Failed to start SMCRoute service");
}

void SMCRouteManager::Add(const Ip6Address &aAddress)
{
    otbrError error = OTBR_ERROR_NONE;

    assert(mListenerSet.find(aAddress) == mListenerSet.end());
    mListenerSet.insert(aAddress);

    VerifyOrExit(mEnabled);

    Flush();
    error = AddRoute(aAddress);

exit:
    otbrLogResult(error, "SMCRouteManager: AddRoute %s", aAddress.ToString().c_str());
}

void SMCRouteManager::Remove(const Ip6Address &aAddress)
{
    otbrError error = OTBR_ERROR_NONE;

    assert(mListenerSet.find(aAddress) != mListenerSet.end());
    mListenerSet.erase(aAddress);

    VerifyOrExit(mEnabled);

    Flush();
    error = DeleteRoute(aAddress);
exit:
    otbrLogResult(error, "SMCRouteManager: RemoveRoute %s", aAddress.ToString().c_str());
}

otbrError SMCRouteManager::AllowOutboundMulticast(void)
{
    return SystemUtils::ExecuteCommand("smcroutectl add %s :: :: 65520 %s", InstanceParams::Get().GetThreadIfName(),
                                       InstanceParams::Get().GetBackboneIfName()) == 0
               ? OTBR_ERROR_NONE
               : OTBR_ERROR_SMCROUTE;
}

otbrError SMCRouteManager::ForbidOutboundMulticast(void)
{
    return SystemUtils::ExecuteCommand("smcroutectl remove %s :: :: 65520 %s", InstanceParams::Get().GetThreadIfName(),
                                       InstanceParams::Get().GetBackboneIfName()) == 0
               ? OTBR_ERROR_NONE
               : OTBR_ERROR_SMCROUTE;
}

otbrError SMCRouteManager::AddRoute(const Ip6Address &aAddress)
{
    return SystemUtils::ExecuteCommand("smcroutectl add %s :: %s %s", InstanceParams::Get().GetBackboneIfName(),
                                       aAddress.ToString().c_str(), InstanceParams::Get().GetThreadIfName()) == 0
               ? OTBR_ERROR_NONE
               : OTBR_ERROR_SMCROUTE;
}

otbrError SMCRouteManager::DeleteRoute(const Ip6Address &aAddress)
{
    return SystemUtils::ExecuteCommand("smcroutectl del %s :: %s %s", InstanceParams::Get().GetBackboneIfName(),
                                       aAddress.ToString().c_str(), InstanceParams::Get().GetThreadIfName()) == 0
               ? OTBR_ERROR_NONE
               : OTBR_ERROR_SMCROUTE;
}

otbrError SMCRouteManager::Flush(void)
{
    return SystemUtils::ExecuteCommand("smcroutectl flush") == 0 ? OTBR_ERROR_NONE : OTBR_ERROR_SMCROUTE;
}

} // namespace BackboneRouter

} // namespace otbr
