/*
 *    Copyright (c) 2018, The OpenThread Authors.
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
 *   The file is the header of net topology data structures and utility functions to
 *   dump them as json
 */

#ifndef OTBR_NET_TOPOLOGY_INFO_HPP_
#define OTBR_NET_TOPOLOGY_INFO_HPP_

#include <vector>

#include <stdint.h>

#include <arpa/inet.h>
#include <errno.h>
#include <jsoncpp/json/json.h>
#include <sys/socket.h>

namespace ot {
namespace BorderRouter {

struct LinkInfo
{
    uint16_t fromRloc16; // in host byte order
    uint16_t toRloc16;   // in host byte order
    uint8_t  routeCost;
    uint8_t  outQualityLevel;
    uint8_t  inQualityLevel;
};

#pragma pack(push, 0)
struct LeaderData
{
    uint32_t partitionID;
    uint8_t  weighting;
    uint8_t  version;
    uint8_t  stateVersion;
    uint8_t  routerID;
};

struct ChildTableEntry
{
    uint8_t  timeOut;
    uint16_t childID;
    uint8_t  mode;
};
#pragma pack(pop)

struct NodeInfo
{
    struct in6_addr mleAddr;
    uint16_t        rloc16; // in host byte order
};

struct NetworkInfo
{
    NodeInfo              leaderNode;
    std::vector<NodeInfo> nodes;
    std::vector<LinkInfo> links;
};

/**
 * This method dumps thread node information to json
 *
 * @param[in]    nodeInfo   thread node information
 *
 * @returns json serialization result
 *
 */
Json::Value DumpNodeInfoToJson(const ot::BorderRouter::NodeInfo &nodeInfo);

/**
 * This method dumps thread link information to json
 *
 * @param[in]    nodeInfo   thread link information
 *
 * @returns json serialization result
 *
 */
Json::Value DumpLinkInfoToJson(const ot::BorderRouter::LinkInfo &linkInfo);

/**
 * This method dumps thread network topology information to json
 *
 * @param[in]    nodeInfo   thread network topology information
 *
 * @returns json serialization result
 *
 */
Json::Value DumpNetworkInfoToJson(const ot::BorderRouter::NetworkInfo &networkInfo);

} // namespace BorderRouter
} // namespace ot

#endif // OTBR_NET_TOPOLOGY_INFO_HPP_
