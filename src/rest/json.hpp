/*
 *  Copyright (c) 2020, The OpenThread Authors.
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
 *   This file includes json formater definition for RESTful HTTP server.
 */

#ifndef OTBR_REST_JSON_HPP_
#define OTBR_REST_JSON_HPP_

#include <string>
#include <vector>

#include "openthread/netdiag.h"
#include "openthread/thread_ftd.h"

#include "agent/ncp_openthread.hpp"
#include "agent/thread_helper.hpp"
#include "utils/hex.hpp"

namespace otbr {
namespace rest {

struct Node{
    int role;
    int numOfRouter;
    uint16_t rloc16;
    const uint8_t *extPanId;
    const uint8_t *extAddress;
    otIp6Address rlocAddress;
    otLeaderData leaderData;
    const char *networkName;
};

namespace JSON {

std::string Number2JsonString(const uint32_t aNumber);
std::string Bytes2HexJsonString(const uint8_t *aBytes, uint8_t aLength);
std::string CString2JsonString(const char *aBytes);
std::string String2JsonString(std::string aString);
std::string Node2JsonString(Node node);
std::string Diag2JsonString(const std::vector<std::vector<otNetworkDiagTlv>> &aDiagSet);
std::string IpAddr2JsonString(const otIp6Address &aAddress);
std::string Mode2JsonString(const otLinkModeConfig &aMode);
std::string Connectivity2JsonString(const otNetworkDiagConnectivity &aConnectivity);
std::string Route2JsonString(const otNetworkDiagRoute &aRoute);
std::string RouteData2JsonString(const otNetworkDiagRouteData &aRouteData);
std::string LeaderData2JsonString(const otLeaderData &aLeaderData);
std::string Ip6Address2JsonString(const otIp6Address &aAddress);
std::string MacCounters2JsonString(const otNetworkDiagMacCounters &aMacCounters);
std::string ChildTableEntry2JsonString(const otNetworkDiagChildEntry &aChildEntry);

}; // namespace JSON

} // namespace rest
} // namespace otbr

#endif
