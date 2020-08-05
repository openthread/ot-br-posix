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
 *   This file includes Handler definition for RESTful HTTP server.
 */

#ifndef OTBR_REST_RESOURCE_HPP_
#define OTBR_REST_RESOURCE_HPP_

#include <unordered_map>

#include <openthread/border_router.h>
#include "rest/json.hpp"
#include "rest/request.hpp"
#include "rest/response.hpp"



using otbr::Ncp::ControllerOpenThread;
using std::chrono::steady_clock;

namespace otbr {
namespace rest {

struct DiagInfo
{
    steady_clock::time_point      mStartTime;
    std::vector<otNetworkDiagTlv> mDiagContent;
};

struct NetworksInfo
{
    steady_clock::time_point mStartTime;
    otActiveScanResult       mNetworkContent;
};

class Resource
{
public:
    Resource(ControllerOpenThread *aNcp);
    void        Init(void);
    void        Handle(Request &aRequest, Response &aResponse);
    void        HandleCallback(Request &aRequest, Response &aResponse);
    void        ErrorHandler(Response &aResponse, int aErrorCode);
    static void DiagnosticResponseHandler(otMessage *aMessage, const otMessageInfo *aMessageInfo, void *aContext);
    void        DiagnosticResponseHandler(otMessage *aMessage, const otMessageInfo);
    void        NetworksResponseHandler(Response *                             aResponse,
                                        otError                                aError,
                                        const std::vector<otActiveScanResult> &aResult);

private:
    typedef void (Resource::*ResourceHandler)(const Request &aRequest, Response &aResponse);
    void NodeInfo(const Request &aRequest, Response &aResponse);
    void ExtendedAddr(const Request &aRequest, Response &aResponse);
    void State(const Request &aRequest, Response &aResponse);
    void NetworkName(const Request &aRequest, Response &aResponse);
    void LeaderData(const Request &aRequest, Response &aResponse);
    void NumOfRoute(const Request &aRequest, Response &aResponse);
    void Rloc16(const Request &aRequest, Response &aResponse);
    void ExtendedPanId(const Request &aRequest, Response &aResponse);
    void Rloc(const Request &aRequest, Response &aResponse);
    void Diagnostic(const Request &aRequest, Response &aResponse);
    void Networks(const Request &aRequest, Response &aResponse);

    void HandleDiagnosticCallback(const Request &aRequest, Response &aResponse);
    void HandleNetworkCallback(const Request &aRequest, Response &aResponse);

    void DeleteOutDatedDiagnostic(void);
    void UpdateDiag(std::string aKey, std::vector<otNetworkDiagTlv> &aDiag);

    void GetNetworks(Response &aResponse);
    void GetNodeInfo(Response &aResponse);
    void GetDataNodeInfo( Response &aResponse);
    void GetDataExtendedAddr(Response &aResponse);
    void GetDataState(Response &aResponse);
    void GetDataNetworkName( Response &aResponse);
    void GetDataLeaderData( Response &aResponse);
    void GetDataNumOfRoute( Response &aResponse);
    void GetDataRloc16( Response &aResponse);
    void GetDataExtendedPanId( Response &aResponse);
    void GetDataRloc( Response &aResponse);

    void PostNetworks(const Request &aRequest, Response &aResponse);

    otInstance *          mInstance;
    ControllerOpenThread *mNcp;

    std::unordered_map<std::string, ResourceHandler> mResourceMap;
    std::unordered_map<std::string, ResourceHandler> mResourceCallbackMap;

    std::unordered_map<int32_t, std::string>      mResponseCodeMap;
    std::unordered_map<std::string, DiagInfo>     mDiagSet;
};

} // namespace rest
} // namespace otbr

#endif // OTBR_REST_RESOURCE_HPP_
