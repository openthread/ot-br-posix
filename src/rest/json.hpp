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

#include "cJSON.h"

#include "rest/rest_web_server.hpp"

namespace otbr {
namespace rest {
class JSON
{
public:
    static std::string TwoVectorToJson(const std::vector<std::string> &aKey, const std::vector<std::string> &aValue);
    static std::string VectorToJson(const std::vector<std::string> &aVector);
    static std::string JsonToStringDeleteJson(cJSON *aJson);
    static std::string JsonToStringKeepJson(const cJSON *aJson);
    static std::string CreateMode(const otLinkModeConfig &aMode);
    static std::string CreateConnectivity(const otNetworkDiagConnectivity &aConnectivity);
    static std::string CreateRoute(const otNetworkDiagRoute &aRoute);
    static std::string CreateRouteData(const otNetworkDiagRouteData &aRouteData);
    static std::string CreateLeaderData(const otLeaderData &aLeaderData);
    static std::string CreateIp6Address(const otIp6Address &aAddress);
    static std::string CreateMacCounters(const otNetworkDiagMacCounters &aMacCounters);
    static std::string CreateChildTableEntry(const otNetworkDiagChildEntry &aChildEntry);

private:
    static cJSON *CreateJsonMode(const otLinkModeConfig &aMode);
    static cJSON *CreateJsonConnectivity(const otNetworkDiagConnectivity &aConnectivity);
    static cJSON *CreateJsonRoute(const otNetworkDiagRoute &aRoute);
    static cJSON *CreateJsonRouteData(const otNetworkDiagRouteData &aRouteData);
    static cJSON *CreateJsonLeaderData(const otLeaderData &aLeaderData);
    static cJSON *CreateJsonIp6Address(const otIp6Address &aAddress);
    static cJSON *CreateJsonMacCounters(const otNetworkDiagMacCounters &aMacCounters);
    static cJSON *CreateJsonChildTableEntry(const otNetworkDiagChildEntry &aChildEntry);
};

} // namespace rest
} // namespace otbr

#endif
