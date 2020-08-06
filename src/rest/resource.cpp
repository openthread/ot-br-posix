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

#include "rest/resource.hpp"

#include "string.h"

#include "utils/pskc.hpp"

#define OT_PSKC_MAX_LENGTH 16
#define OT_EXTENDED_PANID_LENGTH 8
#define OT_DIAGNOETIC_PATH "/diagnostics"
#define OT_NODE_PATH "/node"
#define OT_RLOC_PATH "/node/rloc"
#define OT_RLOC16_PATH "/node/rloc16"
#define OT_EXTADDRESS_PATH "/node/ext-address"
#define OT_STATE_PATH "/node/state"
#define OT_NETWORKNAME_PATH "/node/network-name"
#define OT_LEADERDATA_PATH "/node/leader-data"
#define OT_NUMOFROUTER_PATH "/node/num-of-router"
#define OT_EXTPANID_PATH "/node/ext-panid"
#define OT_NETWORK_PATH "/networks"
#define OT_NETWORK_CURRENT_PATH "/networks/current"
#define OT_NETWORK_CURRENT_COMMISSION_PATH "/networks/commission"
#define OT_NETWORK_CURRENT_PREFIX_PATH "/networks/current/prefix"
#define OT_REST_RESOURCE_200 "200 OK"
#define OT_REST_RESOURCE_201 "201 Created"
#define OT_REST_RESOURCE_202 "202 Accepted"
#define OT_REST_RESOURCE_204 "204 No Content"
#define OT_REST_RESOURCE_400 "400 Bad Request"
#define OT_REST_RESOURCE_404 "404 Not Found"
#define OT_REST_RESOURCE_405 "405 Method Not Allowed"
#define OT_REST_RESOURCE_408 "408 Request Timeout"
#define OT_REST_RESOURCE_411 "411 Length Required"
#define OT_REST_RESOURCE_415 "415 Unsupported Media Type"
#define OT_REST_RESOURCE_500 "500 Internal Server Error"
#define OT_REST_RESOURCE_501 "501 Not Implemented"
#define OT_REST_RESOURCE_505 "505 HTTP Version Not Supported"

using std::chrono::duration_cast;
using std::chrono::microseconds;
using std::chrono::steady_clock;

using std::placeholders::_1;
using std::placeholders::_2;

namespace otbr {
namespace rest {

// MulticastAddr
static const char *kMulticastAddrAllRouters = "ff02::2";
// Default TlvTypes for Diagnostic inforamtion
static const uint8_t kAllTlvTypes[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 14, 15, 16, 17, 18, 19};
//  Timeout(in Microseconds) for deleting outdated diagnostics
static const uint32_t kDiagResetTimeout = 3000000;
// Timeout(in Microseconds) for collecting diagnostics
static const uint32_t kDiagCollectTimeout = 2000000;
// Waittime after perform a factory reset.
static const uint32_t kResetSleepInterval = 1000000;

Resource::Resource(ControllerOpenThread *aNcp)
    : mInstance(aNcp->GetThreadHelper()->GetInstance())
    , mNcp(aNcp)
{
    // Resource Handler
    mResourceMap.emplace(OT_DIAGNOETIC_PATH, &Resource::Diagnostic);
    mResourceMap.emplace(OT_NODE_PATH, &Resource::NodeInfo);
    mResourceMap.emplace(OT_STATE_PATH, &Resource::State);
    mResourceMap.emplace(OT_EXTADDRESS_PATH, &Resource::ExtendedAddr);
    mResourceMap.emplace(OT_NETWORKNAME_PATH, &Resource::NetworkName);
    mResourceMap.emplace(OT_RLOC16_PATH, &Resource::Rloc16);
    mResourceMap.emplace(OT_LEADERDATA_PATH, &Resource::LeaderData);
    mResourceMap.emplace(OT_NUMOFROUTER_PATH, &Resource::NumOfRoute);
    mResourceMap.emplace(OT_EXTPANID_PATH, &Resource::ExtendedPanId);
    mResourceMap.emplace(OT_RLOC_PATH, &Resource::Rloc);

    // Resource callback handler
    mResourceCallbackMap.emplace(OT_DIAGNOETIC_PATH, &Resource::HandleDiagnosticCallback);

    // HTTP Response code
    mResponseCodeMap.emplace(200, OT_REST_RESOURCE_200);
    mResponseCodeMap.emplace(201, OT_REST_RESOURCE_201);
    mResponseCodeMap.emplace(202, OT_REST_RESOURCE_202);
    mResponseCodeMap.emplace(204, OT_REST_RESOURCE_204);
    mResponseCodeMap.emplace(400, OT_REST_RESOURCE_400);
    mResponseCodeMap.emplace(404, OT_REST_RESOURCE_404);
    mResponseCodeMap.emplace(405, OT_REST_RESOURCE_405);
    mResponseCodeMap.emplace(408, OT_REST_RESOURCE_408);
    mResponseCodeMap.emplace(411, OT_REST_RESOURCE_411);
    mResponseCodeMap.emplace(415, OT_REST_RESOURCE_415);
    mResponseCodeMap.emplace(500, OT_REST_RESOURCE_500);
    mResponseCodeMap.emplace(501, OT_REST_RESOURCE_501);
    mResponseCodeMap.emplace(505, OT_REST_RESOURCE_505);
}

void Resource::Init(void)
{
    otThreadSetReceiveDiagnosticGetCallback(mInstance, &Resource::DiagnosticResponseHandler, this);
}

void Resource::Handle(Request &aRequest, Response &aResponse)
{
    std::string url = aRequest.GetUrl();
    auto        it  = mResourceMap.find(url);

    if (it != mResourceMap.end())
    {
        ResourceHandler resourceHandler = it->second;
        (this->*resourceHandler)(aRequest, aResponse);
    }
    else
    {
        ErrorHandler(aResponse, 404);
    }
}

void Resource::HandleCallback(Request &aRequest, Response &aResponse)
{
    std::string url = aRequest.GetUrl();
    auto        it  = mResourceCallbackMap.find(url);

    if (it != mResourceCallbackMap.end())
    {
        ResourceHandler resourceHandler = it->second;
        (this->*resourceHandler)(aRequest, aResponse);
    }
}

void Resource::HandleDiagnosticCallback(const Request &aRequest, Response &aResponse)
{
    OT_UNUSED_VARIABLE(aRequest);
    std::vector<std::vector<otNetworkDiagTlv>> diagContentSet;
    std::string                                body;
    std::string                                errorCode;

    auto duration = duration_cast<microseconds>(steady_clock::now() - aResponse.GetStartTime()).count();
    if (duration >= kDiagCollectTimeout)
    {
        DeleteOutDatedDiagnostic();

        for (auto it = mDiagSet.begin(); it != mDiagSet.end(); ++it)
        {
            diagContentSet.push_back(it->second.mDiagContent);
        }

        body      = JSON::Diag2JsonString(diagContentSet);
        errorCode = mResponseCodeMap[200];
        aResponse.SetResponsCode(errorCode);
        aResponse.SetBody(body);
        aResponse.SetComplete();
    }
}

void Resource::ErrorHandler(Response &aResponse, int aErrorCode)
{
    std::string errorMessage = mResponseCodeMap[aErrorCode];
    std::string body         = JSON::Error2JsonString(aErrorCode, errorMessage);

    aResponse.SetResponsCode(errorMessage);
    aResponse.SetBody(body);
    aResponse.SetComplete();
}

void Resource::GetNodeInfo(Response &aResponse)
{
    otbrError    error = OTBR_ERROR_NONE;
    Node         node;
    otRouterInfo routerInfo;
    uint8_t      maxRouterId;
    std::string  body;
    std::string  errorCode;

    VerifyOrExit(otThreadGetLeaderData(mInstance, &node.mLeaderData) == OT_ERROR_NONE, error = OTBR_ERROR_REST);

    node.mNumOfRouter = 0;
    maxRouterId       = otThreadGetMaxRouterId(mInstance);
    for (uint8_t i = 0; i <= maxRouterId; ++i)
    {
        if (otThreadGetRouterInfo(mInstance, i, &routerInfo) != OT_ERROR_NONE)
        {
            continue;
        }
        ++node.mNumOfRouter;
    }

    node.mRole        = otThreadGetDeviceRole(mInstance);
    node.mExtAddress  = reinterpret_cast<const uint8_t *>(otLinkGetExtendedAddress(mInstance));
    node.mNetworkName = otThreadGetNetworkName(mInstance);
    node.mRloc16      = otThreadGetRloc16(mInstance);
    node.mExtPanId    = reinterpret_cast<const uint8_t *>(otThreadGetExtendedPanId(mInstance));
    node.mRlocAddress = *otThreadGetRloc(mInstance);

    body = JSON::Node2JsonString(node);
    aResponse.SetBody(body);

exit:
    if (error == OTBR_ERROR_NONE)
    {
        errorCode = mResponseCodeMap[200];
        aResponse.SetResponsCode(errorCode);
    }
    else
    {
        ErrorHandler(aResponse, 500);
    }
}

void Resource::NodeInfo(const Request &aRequest, Response &aResponse)
{
    std::string errorCode;
    if (aRequest.GetMethod() == OTBR_REST_METHOD_GET)
    {
        GetNodeInfo(aResponse);
    }
    else
    {
        ErrorHandler(aResponse, 405);
    }
}

void Resource::GetDataExtendedAddr(Response &aResponse)
{
    const uint8_t *extAddress = reinterpret_cast<const uint8_t *>(otLinkGetExtendedAddress(mInstance));
    std::string    errorCode;
    std::string    body = JSON::Bytes2HexJsonString(extAddress, OT_EXT_ADDRESS_SIZE);

    aResponse.SetBody(body);
    errorCode = mResponseCodeMap[200];
    aResponse.SetResponsCode(errorCode);
}

void Resource::ExtendedAddr(const Request &aRequest, Response &aResponse)
{
    std::string errorCode;

    if (aRequest.GetMethod() == OTBR_REST_METHOD_GET)
    {
        GetDataExtendedAddr(aResponse);
    }
    else
    {
        ErrorHandler(aResponse, 405);
    }
}

void Resource::GetDataState(Response &aResponse)
{
    std::string state;
    std::string errorCode;
    uint8_t     role;
    // 0 : disabled
    // 1 : detached
    // 2 : child
    // 3 : router
    // 4 : leader

    role  = otThreadGetDeviceRole(mInstance);
    state = JSON::Number2JsonString(role);
    aResponse.SetBody(state);
    errorCode = mResponseCodeMap[200];
    aResponse.SetResponsCode(errorCode);
}

void Resource::State(const Request &aRequest, Response &aResponse)
{
    std::string errorCode;

    if (aRequest.GetMethod() == OTBR_REST_METHOD_GET)
    {
        GetDataState(aResponse);
    }
    else
    {
        ErrorHandler(aResponse, 405);
    }
}

void Resource::GetDataNetworkName(Response &aResponse)
{
    std::string networkName;
    std::string errorCode;

    networkName = otThreadGetNetworkName(mInstance);
    networkName = JSON::String2JsonString(networkName);

    aResponse.SetBody(networkName);
    errorCode = mResponseCodeMap[200];
    aResponse.SetResponsCode(errorCode);
}

void Resource::NetworkName(const Request &aRequest, Response &aResponse)
{
    std::string errorCode;

    if (aRequest.GetMethod() == OTBR_REST_METHOD_GET)
    {
        GetDataNetworkName(aResponse);
    }
    else
    {
        ErrorHandler(aResponse, 405);
    }
}

void Resource::GetDataLeaderData(Response &aResponse)
{
    otbrError    error = OTBR_ERROR_NONE;
    otLeaderData leaderData;
    std::string  body;
    std::string  errorCode;

    VerifyOrExit(otThreadGetLeaderData(mInstance, &leaderData) == OT_ERROR_NONE, error = OTBR_ERROR_REST);

    body = JSON::LeaderData2JsonString(leaderData);

    aResponse.SetBody(body);

exit:
    if (error == OTBR_ERROR_NONE)
    {
        errorCode = mResponseCodeMap[200];
        aResponse.SetResponsCode(errorCode);
    }
    else
    {
        ErrorHandler(aResponse, 500);
    }
}

void Resource::LeaderData(const Request &aRequest, Response &aResponse)
{
    std::string errorCode;
    if (aRequest.GetMethod() == OTBR_REST_METHOD_GET)
    {
        GetDataLeaderData(aResponse);
    }
    else
    {
        ErrorHandler(aResponse, 405);
    }
}

void Resource::GetDataNumOfRoute(Response &aResponse)
{
    uint8_t      count = 0;
    uint8_t      maxRouterId;
    otRouterInfo routerInfo;
    maxRouterId = otThreadGetMaxRouterId(mInstance);
    std::string body;
    std::string errorCode;

    for (uint8_t i = 0; i <= maxRouterId; ++i)
    {
        if (otThreadGetRouterInfo(mInstance, i, &routerInfo) != OT_ERROR_NONE)
        {
            continue;
        }
        ++count;
    }

    body = JSON::Number2JsonString(count);

    aResponse.SetBody(body);
    errorCode = mResponseCodeMap[200];
    aResponse.SetResponsCode(errorCode);
}

void Resource::NumOfRoute(const Request &aRequest, Response &aResponse)
{
    std::string errorCode;

    if (aRequest.GetMethod() == OTBR_REST_METHOD_GET)
    {
        GetDataNumOfRoute(aResponse);
    }
    else
    {
        ErrorHandler(aResponse, 405);
    }
}

void Resource::GetDataRloc16(Response &aResponse)
{
    uint16_t    rloc16 = otThreadGetRloc16(mInstance);
    std::string body;
    std::string errorCode;

    body = JSON::Number2JsonString(rloc16);

    aResponse.SetBody(body);
    errorCode = mResponseCodeMap[200];
    aResponse.SetResponsCode(errorCode);
}

void Resource::Rloc16(const Request &aRequest, Response &aResponse)
{
    std::string errorCode;

    if (aRequest.GetMethod() == OTBR_REST_METHOD_GET)
    {
        GetDataRloc16(aResponse);
    }
    else
    {
        ErrorHandler(aResponse, 405);
    }
}

void Resource::GetDataExtendedPanId(Response &aResponse)
{
    const uint8_t *extPanId = reinterpret_cast<const uint8_t *>(otThreadGetExtendedPanId(mInstance));
    std::string    body     = JSON::Bytes2HexJsonString(extPanId, OT_EXT_PAN_ID_SIZE);
    std::string    errorCode;

    aResponse.SetBody(body);
    errorCode = mResponseCodeMap[200];
    aResponse.SetResponsCode(errorCode);
}

void Resource::ExtendedPanId(const Request &aRequest, Response &aResponse)
{
    std::string errorCode;

    if (aRequest.GetMethod() == OTBR_REST_METHOD_GET)
    {
        GetDataExtendedPanId(aResponse);
    }
    else
    {
        ErrorHandler(aResponse, 405);
    }
}

void Resource::GetDataRloc(Response &aResponse)
{
    otIp6Address rlocAddress = *otThreadGetRloc(mInstance);
    std::string  body;
    std::string  errorCode;

    body = JSON::IpAddr2JsonString(rlocAddress);

    aResponse.SetBody(body);
    errorCode = mResponseCodeMap[200];
    aResponse.SetResponsCode(errorCode);
}

void Resource::Rloc(const Request &aRequest, Response &aResponse)
{
    std::string errorCode;

    if (aRequest.GetMethod() == OTBR_REST_METHOD_GET)
    {
        GetDataRloc(aResponse);
    }
    else
    {
        ErrorHandler(aResponse, 405);
    }
}

void Resource::DeleteOutDatedDiagnostic(void)
{
    auto eraseIt = mDiagSet.begin();
    for (eraseIt = mDiagSet.begin(); eraseIt != mDiagSet.end();)
    {
        auto diagInfo = eraseIt->second;
        auto duration = duration_cast<microseconds>(steady_clock::now() - diagInfo.mStartTime).count();

        if (duration >= kDiagResetTimeout)
        {
            eraseIt = mDiagSet.erase(eraseIt);
        }
        else
        {
            eraseIt++;
        }
    }
}

void Resource::UpdateDiag(std::string aKey, std::vector<otNetworkDiagTlv> &aDiag)
{
    DiagInfo value;

    value.mStartTime = steady_clock::now();
    value.mDiagContent.assign(aDiag.begin(), aDiag.end());
    mDiagSet[aKey] = value;
}

void Resource::Diagnostic(const Request &aRequest, Response &aResponse)
{
    otbrError error = OTBR_ERROR_NONE;
    OT_UNUSED_VARIABLE(aRequest);
    struct otIp6Address rloc16address = *otThreadGetRloc(mInstance);
    struct otIp6Address multicastAddress;

    VerifyOrExit(otThreadSendDiagnosticGet(mInstance, &rloc16address, kAllTlvTypes, sizeof(kAllTlvTypes)) ==
                     OT_ERROR_NONE,
                 error = OTBR_ERROR_REST);
    VerifyOrExit(otIp6AddressFromString(kMulticastAddrAllRouters, &multicastAddress) == OT_ERROR_NONE,
                 error = OTBR_ERROR_REST);
    VerifyOrExit(otThreadSendDiagnosticGet(mInstance, &multicastAddress, kAllTlvTypes, sizeof(kAllTlvTypes)) ==
                     OT_ERROR_NONE,
                 error = OTBR_ERROR_REST);

exit:

    if (error == OTBR_ERROR_NONE)
    {
        aResponse.SetStartTime(steady_clock::now());
        aResponse.SetCallback();
    }
    else
    {
        ErrorHandler(aResponse, 500);
    }
}

void Resource::DiagnosticResponseHandler(otMessage *aMessage, const otMessageInfo *aMessageInfo, void *aContext)
{
    static_cast<Resource *>(aContext)->DiagnosticResponseHandler(aMessage,
                                                                 *static_cast<const otMessageInfo *>(aMessageInfo));
}

void Resource::DiagnosticResponseHandler(otMessage *aMessage, const otMessageInfo)
{
    std::vector<otNetworkDiagTlv> diagSet;
    otNetworkDiagTlv              diagTlv;
    otNetworkDiagIterator         iterator = OT_NETWORK_DIAGNOSTIC_ITERATOR_INIT;
    otError                       error;
    char                          rloc[7];
    std::string                   keyRloc = "0xffee";

    while ((error = otThreadGetNextDiagnosticTlv(aMessage, &iterator, &diagTlv)) == OT_ERROR_NONE)
    {
        if (diagTlv.mType == OT_NETWORK_DIAGNOSTIC_TLV_SHORT_ADDRESS)
        {
            sprintf(rloc, "0x%04x", diagTlv.mData.mAddr16);
            keyRloc = JSON::CString2JsonString(rloc);
        }
        diagSet.push_back(diagTlv);
    }
    UpdateDiag(keyRloc, diagSet);
}

} // namespace rest
} // namespace otbr
