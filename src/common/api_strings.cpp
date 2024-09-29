/*
 *    Copyright (c) 2023, The OpenThread Authors.
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

#include "common/api_strings.hpp"

#include <openthread/instance.h>

std::string GetDeviceRoleName(otDeviceRole aRole)
{
    std::string roleName;

    switch (aRole)
    {
    case OT_DEVICE_ROLE_DISABLED:
        roleName = OTBR_ROLE_NAME_DISABLED;
        break;
    case OT_DEVICE_ROLE_DETACHED:
        roleName = OTBR_ROLE_NAME_DETACHED;
        break;
    case OT_DEVICE_ROLE_CHILD:
        roleName = OTBR_ROLE_NAME_CHILD;
        break;
    case OT_DEVICE_ROLE_ROUTER:
        roleName = OTBR_ROLE_NAME_ROUTER;
        break;
    case OT_DEVICE_ROLE_LEADER:
        roleName = OTBR_ROLE_NAME_LEADER;
        break;
    }

    return roleName;
}

#if OTBR_ENABLE_DHCP6_PD
std::string GetDhcp6PdStateName(otBorderRoutingDhcp6PdState aState)
{
    std::string stateName;

    switch (aState)
    {
    case OT_BORDER_ROUTING_DHCP6_PD_STATE_DISABLED:
        stateName = OTBR_DHCP6_PD_STATE_NAME_DISABLED;
        break;
    case OT_BORDER_ROUTING_DHCP6_PD_STATE_STOPPED:
        stateName = OTBR_DHCP6_PD_STATE_NAME_STOPPED;
        break;
    case OT_BORDER_ROUTING_DHCP6_PD_STATE_RUNNING:
        stateName = OTBR_DHCP6_PD_STATE_NAME_RUNNING;
        break;
#if OPENTHREAD_API_VERSION >= 451
    case OT_BORDER_ROUTING_DHCP6_PD_STATE_IDLE:
        stateName = OTBR_DHCP6_PD_STATE_NAME_IDLE;
        break;
#endif
    }

    return stateName;
}
#endif // OTBR_ENABLE_DHCP6_PD
