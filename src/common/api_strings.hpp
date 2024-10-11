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

/**
 * @file
 * This file has helper functions to convert internal state representations
 * to string (useful for APIs).
 */
#ifndef OTBR_COMMON_API_STRINGS_HPP_
#define OTBR_COMMON_API_STRINGS_HPP_

#include "openthread-br/config.h"

#include <string>

#include <openthread/border_routing.h>
#include <openthread/thread.h>

#define OTBR_ROLE_NAME_DISABLED "disabled"
#define OTBR_ROLE_NAME_DETACHED "detached"
#define OTBR_ROLE_NAME_CHILD "child"
#define OTBR_ROLE_NAME_ROUTER "router"
#define OTBR_ROLE_NAME_LEADER "leader"

#if OTBR_ENABLE_DHCP6_PD
#define OTBR_DHCP6_PD_STATE_NAME_DISABLED "disabled"
#define OTBR_DHCP6_PD_STATE_NAME_STOPPED "stopped"
#define OTBR_DHCP6_PD_STATE_NAME_RUNNING "running"
#define OTBR_DHCP6_PD_STATE_NAME_IDLE "idle"
#endif

std::string GetDeviceRoleName(otDeviceRole aRole);

#if OTBR_ENABLE_DHCP6_PD
std::string GetDhcp6PdStateName(otBorderRoutingDhcp6PdState aDhcp6PdState);
#endif // OTBR_ENABLE_DHCP6_PD

#endif // OTBR_COMMON_API_STRINGS_HPP_
