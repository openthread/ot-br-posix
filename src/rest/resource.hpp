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

#ifndef OTBR_REST_HANDLER_HPP_
#define OTBR_REST_HANDLER_HPP_

#include <string>
#include <unordered_map>

#include "openthread/netdiag.h"
#include "openthread/thread_ftd.h"

#include "agent/ncp_openthread.hpp"
#include "agent/thread_helper.hpp"
#include "rest/json.hpp"
#include "rest/request.hpp"
#include "rest/response.hpp"

using otbr::Ncp::ControllerOpenThread;
using std::chrono::steady_clock;

namespace otbr {
namespace rest {

struct DiagInfo
{
    steady_clock::time_point mStartTime;
    std::string              mDiagContent;

    DiagInfo(steady_clock::time_point aStartTime, std::string aDiagContent)
        : mStartTime(aStartTime)
        , mDiagContent(aDiagContent)
    {
    }
};

class Resource
{
public:
    Resource(ControllerOpenThread *aNcp, int aCallbackTimeout);
    void Init();
    void Handle(Request &aRequest, Response &aResponse);
    void HandleCallback(Request &aRequest, Response &aResponse);

    static void DiagnosticResponseHandler(otMessage *aMessage, const otMessageInfo *aMessageInfo, void *aContext);
    void        DiagnosticResponseHandler(otMessage *aMessage, const otMessageInfo);

private:
    void NodeInfo(Request &aRequest, Response &aResponse);
    void ExtendedAddr(Request &aRequest, Response &aResponse);
    void State(Request &aRequest, Response &aResponse);
    void NetworkName(Request &aRequest, Response &aResponse);
    void LeaderData(Request &aRequest, Response &aResponse);
    void NumOfRoute(Request &aRequest, Response &aResponse);
    void Rloc16(Request &aRequest, Response &aResponse);
    void ExtendedPanId(Request &aRequest, Response &aResponse);
    void Rloc(Request &aRequest, Response &aResponse);
    void Diagnostic(Request &aRequest, Response &aResponse);
    void ErrorHandler(Request &aRequest, Response &aResponse);

    void DeleteOutDatedDiag();
    void UpdateDiag(std::string aKey, std::string aValue);

    std::string GetDataNodeInfo();
    std::string GetDataExtendedAddr();
    std::string GetDataState();
    std::string GetDataNetworkName();
    std::string GetDataLeaderData();
    std::string GetDataNumOfRoute();
    std::string GetDataRloc16();
    std::string GetDataExtendedPanId();
    std::string GetDataRloc();

    otbr::Ncp::ControllerOpenThread *                          mNcp;
    otInstance *                                               mInstance;
    int                                                        mCallbackTimeout;
    std::unordered_map<std::string, std::unique_ptr<DiagInfo>> mDiagMaintainer;

    static const char *  kMulticastAddrAllRouters;
    static const uint8_t kAllTlvTypes[];
    static const uint8_t kTlvTypesCount;
};

} // namespace rest
} // namespace otbr

#endif
