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
 *   This file includes definition for SMCRoute manager.
 */

#ifndef BACKBONE_ROUTER_SMCROUTE_MANAGER_HPP_
#define BACKBONE_ROUTER_SMCROUTE_MANAGER_HPP_

#include <set>
#include <openthread/backbone_router_ftd.h>

#include "agent/instance_params.hpp"
#include "agent/ncp_openthread.hpp"
#include "utils/system_utils.hpp"

namespace otbr {
namespace BackboneRouter {

/**
 * @addtogroup border-router-backbone
 *
 * @brief
 *   This module includes definition for SMCRoute manager.
 *
 * @{
 */

/**
 * This class implements SMCRoute manager functionality.
 *
 */
class SMCRouteManager
{
public:
    /**
     * This constructor initializes a SMCRoute manager instance.
     *
     */
    explicit SMCRouteManager()
        : mEnabled(false)
    {
    }

    /**
     * This method initializes a SMCRoute manager instance.
     *
     */
    void Init(void);

    /**
     * This method enables the SMCRoute manager.
     *
     */
    void Enable(void);

    /**
     * This method disables the SMCRoute manager.
     *
     */
    void Disable(void);

    /**
     * This method adds a multicast route.
     *
     * NOTE: Multicast routes are only effective when the SMCRoute manager is enabled.
     *
     * @param[in] aAddress  The multicast address.
     *
     */
    void Add(const Ip6Address &aAddress);

    /**
     * This method removes a multicast route.
     *
     * @param[in] aAddress  The multicast address.
     *
     */
    void Remove(const Ip6Address &aAddress);

private:
    void      StartSMCRouteService(void);
    otbrError Flush(void);
    otbrError ForbidOutboundMulticast(void);
    otbrError AllowOutboundMulticast(void);
    otbrError AddRoute(const Ip6Address &aAddress);
    otbrError DeleteRoute(const Ip6Address &aAddress);

    std::set<Ip6Address> mListenerSet;
    bool                 mEnabled : 1;
};

/**
 * @}
 */

} // namespace BackboneRouter
} // namespace otbr

#endif // BACKBONE_ROUTER_SMCROUTE_MANAGER_HPP_
