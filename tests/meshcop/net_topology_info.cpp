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
 *   The file is implements utility functions to dump network topology data structure as json
 */

#include "net_topology_info.hpp"

namespace ot {
namespace BorderRouter {

Json::Value DumpNodeInfoToJson(const ot::BorderRouter::NodeInfo &nodeInfo)
{
    Json::Value nodeValue;
    char        nameBuffer[100];
    nodeValue["rloc16"] = nodeInfo.rloc16;
    inet_ntop(AF_INET6, &nodeInfo.mleAddr, nameBuffer, sizeof(nameBuffer));
    nodeValue["mleAddr"] = nameBuffer;
    return nodeValue;
}

Json::Value DumpLinkInfoToJson(const ot::BorderRouter::LinkInfo &linkInfo)
{
    Json::Value nodeValue;
    nodeValue["fromRloc16"]      = linkInfo.fromRloc16;
    nodeValue["toRloc16"]        = linkInfo.toRloc16;
    nodeValue["routeCost"]       = linkInfo.routeCost;
    nodeValue["inQualityLevel"]  = linkInfo.inQualityLevel;
    nodeValue["outQualityLevel"] = linkInfo.outQualityLevel;
    return nodeValue;
}

Json::Value DumpNetworkInfoToJson(const ot::BorderRouter::NetworkInfo &networkInfo)
{
    Json::Value root;
    Json::Value nodeList;
    Json::Value linkList;
    root["leader"] = DumpNodeInfoToJson(networkInfo.leaderNode);
    for (size_t i = 0; i < networkInfo.nodes.size(); i++)
    {
        nodeList.append(DumpNodeInfoToJson(networkInfo.nodes[i]));
    }
    for (size_t i = 0; i < networkInfo.links.size(); i++)
    {
        linkList.append(DumpLinkInfoToJson(networkInfo.links[i]));
    }
    root["nodes"] = nodeList;
    root["links"] = linkList;
    return root;
}

} // namespace BorderRouter
} // namespace ot
