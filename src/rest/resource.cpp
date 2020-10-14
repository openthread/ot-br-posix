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

#include <string.h>

#include "utils/pskc.hpp"

#define OT_REST_RESOURCE_VERSION "/v1"

#define OT_REST_RESOURCE_PATH_DIAGNOSTIC OT_REST_RESOURCE_VERSION "/diagnostics"

#define OT_REST_RESOURCE_PATH_NODE OT_REST_RESOURCE_VERSION "/node"
#define OT_REST_RESOURCE_PATH_NODE_RLOC OT_REST_RESOURCE_PATH_NODE "/rloc"
#define OT_REST_RESOURCE_PATH_NODE_RLOC16 OT_REST_RESOURCE_PATH_NODE "/rloc16"
#define OT_REST_RESOURCE_PATH_NODE_EXTADDRESS OT_REST_RESOURCE_PATH_NODE "/ext-address"
#define OT_REST_RESOURCE_PATH_NODE_STATE OT_REST_RESOURCE_PATH_NODE "/state"
#define OT_REST_RESOURCE_PATH_NODE_NETWORKNAME OT_REST_RESOURCE_PATH_NODE "/network-name"
#define OT_REST_RESOURCE_PATH_NODE_LEADERDATA OT_REST_RESOURCE_PATH_NODE "/leader-data"
#define OT_REST_RESOURCE_PATH_NODE_NUMOFROUTER OT_REST_RESOURCE_PATH_NODE "/num-of-router"
#define OT_REST_RESOURCE_PATH_NODE_EXTPANID OT_REST_RESOURCE_PATH_NODE "/ext-panid"

#define OT_REST_RESOURCE_PATH_NETWORKS OT_REST_RESOURCE_VERSION "/networks"
#define OT_REST_RESOURCE_PATH_NETWORKS_CURRENT OT_REST_RESOURCE_PATH_NETWORKS "/current"
#define OT_REST_RESOURCE_PATH_NETWORKS_CURRENT_COMMISSION OT_REST_RESOURCE_PATH_NETWORKS_CURRENT "/commission"
#define OT_REST_RESOURCE_PATH_NETWORKS_CURRENT_PREFIX OT_REST_RESOURCE_PATH_NETWORKS_CURRENT "/prefix"

#define OT_REST_HTTP_STATUS_200 "200 OK"
#define OT_REST_HTTP_STATUS_201 "201 Created"
#define OT_REST_HTTP_STATUS_202 "202 Accepted"
#define OT_REST_HTTP_STATUS_204 "204 No Content"
#define OT_REST_HTTP_STATUS_400 "400 Bad Request"
#define OT_REST_HTTP_STATUS_404 "404 Not Found"
#define OT_REST_HTTP_STATUS_405 "405 Method Not Allowed"
#define OT_REST_HTTP_STATUS_408 "408 Request Timeout"
#define OT_REST_HTTP_STATUS_411 "411 Length Required"
#define OT_REST_HTTP_STATUS_415 "415 Unsupported Media Type"
#define OT_REST_HTTP_STATUS_500 "500 Internal Server Error"
#define OT_REST_HTTP_STATUS_501 "501 Not Implemented"
#define OT_REST_HTTP_STATUS_505 "505 HTTP Version Not Supported"

#define OT_REST_405_DESCRIPTION "This method is not allowed"
#define OT_REST_404_DESCRIPTION "Resource is not available, please check the URL"
#define OT_REST_EMPTY_DESCRIPTION ""

using otbr::Ncp::ControllerOpenThread;
using std::chrono::duration_cast;
using std::chrono::microseconds;
using std::chrono::steady_clock;

using std::placeholders::_1;
using std::placeholders::_2;

namespace otbr {
namespace Rest {

// MulticastAddr
static const char *kMulticastAddrAllRouters = "ff02::2";

// Timeout (in Microseconds) for deleting outdated diagnostics
static const uint32_t kDiagResetTimeout = 3000000;

// Timeout (in Microseconds) for collecting diagnostics
static const uint32_t kDiagCollectTimeout = 2000000;

// Interval (in Microseconds) should wait after a factory reset
static const uint32_t kResetSleepInterval = 1000000;

// Default timeout (in Seconds) for Joiners
static const uint32_t kDefaultJoinerTimeout = 120;

Resource::Resource(ControllerOpenThread *aNcp)
    : mNcp(aNcp)
{
    mInstance = mNcp->GetThreadHelper()->GetInstance();

    // Resource Handler
    mResourceMap.emplace(OT_REST_RESOURCE_PATH_DIAGNOSTIC, &Resource::Diagnostic);
    mResourceMap.emplace(OT_REST_RESOURCE_PATH_NODE, &Resource::NodeInfo);
    mResourceMap.emplace(OT_REST_RESOURCE_PATH_NODE_STATE, &Resource::State);
    mResourceMap.emplace(OT_REST_RESOURCE_PATH_NODE_EXTADDRESS, &Resource::ExtendedAddr);
    mResourceMap.emplace(OT_REST_RESOURCE_PATH_NODE_NETWORKNAME, &Resource::NetworkName);
    mResourceMap.emplace(OT_REST_RESOURCE_PATH_NODE_RLOC16, &Resource::Rloc16);
    mResourceMap.emplace(OT_REST_RESOURCE_PATH_NODE_LEADERDATA, &Resource::LeaderData);
    mResourceMap.emplace(OT_REST_RESOURCE_PATH_NODE_NUMOFROUTER, &Resource::NumOfRoute);
    mResourceMap.emplace(OT_REST_RESOURCE_PATH_NODE_EXTPANID, &Resource::ExtendedPanId);
    mResourceMap.emplace(OT_REST_RESOURCE_PATH_NODE_RLOC, &Resource::Rloc);
    mResourceMap.emplace(OT_REST_RESOURCE_PATH_NETWORKS, &Resource::Networks);
    mResourceMap.emplace(OT_REST_RESOURCE_PATH_NETWORKS_CURRENT, &Resource::CurrentNetwork);
    mResourceMap.emplace(OT_REST_RESOURCE_PATH_NETWORKS_CURRENT_PREFIX, &Resource::CurrentNetworkPrefix);
    mResourceMap.emplace(OT_REST_RESOURCE_PATH_NETWORKS_CURRENT_COMMISSION, &Resource::CurrentNetworkCommission);

    // Callback Handler
    mResourceCallbackMap.emplace(OT_REST_RESOURCE_PATH_DIAGNOSTIC, &Resource::HandleDiagnosticCallback);
    mResourceCallbackMap.emplace(OT_REST_RESOURCE_PATH_NETWORKS, &Resource::PostNetworksCallback);
    mResourceCallbackMap.emplace(OT_REST_RESOURCE_PATH_NETWORKS_CURRENT, &Resource::PutCurrentNetworkCallback);
    mResourceCallbackMap.emplace(OT_REST_RESOURCE_PATH_NETWORKS_CURRENT_COMMISSION,
                                 &Resource::CurrentNetworkCommissionCallback);

    // HTTP Response code
    mResponseCodeMap.emplace(HttpStatusCode::kStatusOk, OT_REST_HTTP_STATUS_200);
    mResponseCodeMap.emplace(HttpStatusCode::kStatusCreated, OT_REST_HTTP_STATUS_201);
    mResponseCodeMap.emplace(HttpStatusCode::kStatusAccepted, OT_REST_HTTP_STATUS_202);
    mResponseCodeMap.emplace(HttpStatusCode::kStatusNoContent, OT_REST_HTTP_STATUS_204);
    mResponseCodeMap.emplace(HttpStatusCode::kStatusBadRequest, OT_REST_HTTP_STATUS_400);
    mResponseCodeMap.emplace(HttpStatusCode::kStatusResourceNotFound, OT_REST_HTTP_STATUS_404);
    mResponseCodeMap.emplace(HttpStatusCode::kStatusMethodNotAllowed, OT_REST_HTTP_STATUS_405);
    mResponseCodeMap.emplace(HttpStatusCode::kStatusRequestTimeout, OT_REST_HTTP_STATUS_408);
    mResponseCodeMap.emplace(HttpStatusCode::kStatusLengthRequired, OT_REST_HTTP_STATUS_411);
    mResponseCodeMap.emplace(HttpStatusCode::kStatusUnsupportedMediaType, OT_REST_HTTP_STATUS_415);
    mResponseCodeMap.emplace(HttpStatusCode::kStatusInternalServerError, OT_REST_HTTP_STATUS_500);
    mResponseCodeMap.emplace(HttpStatusCode::kStatusNotImplemented, OT_REST_HTTP_STATUS_501);
    mResponseCodeMap.emplace(HttpStatusCode::kStatusHttpVersionNotSupported, OT_REST_HTTP_STATUS_505);
}

void Resource::Init(void)
{
    otThreadSetReceiveDiagnosticGetCallback(mInstance, &Resource::DiagnosticResponseHandler, this);
}

void Resource::Handle(Request &aRequest, Response &aResponse) const
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
        ErrorHandler(aResponse, HttpStatusCode::kStatusResourceNotFound, OT_REST_404_DESCRIPTION);
    }
}

void Resource::HandleCallback(Request &aRequest, Response &aResponse)
{
    std::string url = aRequest.GetUrl();
    auto        it  = mResourceCallbackMap.find(url);

    if (it != mResourceCallbackMap.end())
    {
        ResourceCallbackHandler resourceHandler = it->second;
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

        body      = Json::Diag2JsonString(diagContentSet);
        errorCode = mResponseCodeMap.at(HttpStatusCode::kStatusOk);
        aResponse.SetResponsCode(errorCode);
        aResponse.SetBody(body);
        aResponse.SetComplete();
    }
}

void Resource::ErrorHandler(Response &aResponse, HttpStatusCode aErrorCode, std::string aErrorDescription) const
{
    std::string errorMessage = mResponseCodeMap.at(aErrorCode);
    std::string body         = Json::Error2JsonString(aErrorCode, errorMessage, aErrorDescription);

    aResponse.SetResponsCode(errorMessage);
    aResponse.SetBody(body);
    aResponse.SetComplete();
}

void Resource::CurrentNetworkPrefix(const Request &aRequest, Response &aResponse) const
{
    if (aRequest.GetMethod() == HttpMethod::kPost)
    {
        PostCurrentNetworkPrefix(aRequest, aResponse);
    }
    else if (aRequest.GetMethod() == HttpMethod::kGet)
    {
        GetCurrentNetworkPrefix(aRequest, aResponse);
    }
    else if (aRequest.GetMethod() == HttpMethod::kDelete)
    {
        DeleteCurrentNetworkPrefix(aRequest, aResponse);
    }
    else if (aRequest.GetMethod() == HttpMethod::kOptions)
    {
        ErrorHandler(aResponse, HttpStatusCode::kStatusOk, OT_REST_EMPTY_DESCRIPTION);
    }
    else
    {
        ErrorHandler(aResponse, HttpStatusCode::kStatusMethodNotAllowed, OT_REST_405_DESCRIPTION);
    }
}

void Resource::GetCurrentNetworkPrefix(const Request &aRequest, Response &aResponse) const
{
    OT_UNUSED_VARIABLE(aRequest);
    std::string                       errorCode;
    std::string                       errorDescription;
    std::string                       body;
    otBorderRouterConfig              config;
    std::vector<otBorderRouterConfig> configList;
    otNetworkDataIterator             iterator = OT_NETWORK_DATA_ITERATOR_INIT;

    while (otBorderRouterGetNextOnMeshPrefix(mInstance, &iterator, &config) == OT_ERROR_NONE)
    {
        configList.push_back(config);
    }
    body = Json::PrefixList2JsonString(configList);

    aResponse.SetBody(body);
    errorCode = mResponseCodeMap.at(HttpStatusCode::kStatusOk);
    aResponse.SetResponsCode(errorCode);
    aResponse.SetComplete();
}

void Resource::PostCurrentNetworkPrefix(const Request &aRequest, Response &aResponse) const
{
    otError              err = OT_ERROR_NONE;
    std::string          errorCode;
    std::string          errorDescription;
    PostError            error = PostError::kPostErrorNone;
    std::string          requestBody;
    std::string          prefix;
    bool                 defaultRoute;
    otBorderRouterConfig config;

    requestBody = aRequest.GetBody();

    VerifyOrExit(Json::JsonString2Bool(requestBody, std::string("defaultRoute"), defaultRoute),
                 error = PostError::kPostBadRequest, errorDescription = "Failed at decode json : defaultRoute");
    VerifyOrExit(Json::JsonString2String(requestBody, std::string("prefix"), prefix),
                 error = PostError::kPostBadRequest, errorDescription = "Failed at decode json : prefix");

    // Add prefix
    memset(&config, 0, sizeof(otBorderRouterConfig));

    err = otIp6AddressFromString(prefix.c_str(), &config.mPrefix.mPrefix);
    VerifyOrExit(err == OT_ERROR_NONE, error = PostError::kPostBadRequest,
                 errorDescription = "Failed at encode prefix :" + std::string(otThreadErrorToString(err)));

    config.mPreferred      = true;
    config.mSlaac          = true;
    config.mStable         = true;
    config.mOnMesh         = true;
    config.mDefaultRoute   = defaultRoute;
    config.mPrefix.mLength = 64;

    err = otBorderRouterAddOnMeshPrefix(mInstance, &config);
    VerifyOrExit(err == OT_ERROR_NONE, error = PostError::kPostSetFail,
                 errorDescription = "Failed at add prefix :" + std::string(otThreadErrorToString(err)));

exit:

    if (error == PostError::kPostErrorNone)
    {
        aResponse.SetBody(requestBody);
        errorCode = mResponseCodeMap.at(HttpStatusCode::kStatusOk);
        aResponse.SetResponsCode(errorCode);
        aResponse.SetComplete();
    }
    else if (error == PostError::kPostSetFail)
    {
        ErrorHandler(aResponse, HttpStatusCode::kStatusInternalServerError, errorDescription);
    }
    else
    {
        ErrorHandler(aResponse, HttpStatusCode::kStatusBadRequest, errorDescription);
    }
}

void Resource::DeleteCurrentNetworkPrefix(const Request &aRequest, Response &aResponse) const
{
    otError            err = OT_ERROR_NONE;
    std::string        errorCode;
    PostError          error = PostError::kPostErrorNone;
    std::string        errorDescription;
    std::string        requestBody;
    std::string        value;
    struct otIp6Prefix prefix;

    memset(&prefix, 0, sizeof(otIp6Prefix));

    requestBody = aRequest.GetBody();

    VerifyOrExit(Json::JsonString2String(requestBody, std::string("prefix"), value), error = PostError::kPostBadRequest,
                 errorDescription = "Failed at decode json : prefix");

    err = otIp6AddressFromString(value.c_str(), &prefix.mPrefix);
    VerifyOrExit(err == OT_ERROR_NONE, error = PostError::kPostBadRequest,
                 errorDescription = "Failed at encode prefix :" + std::string(otThreadErrorToString(err)));

    prefix.mLength = 64;

    err = otBorderRouterRemoveOnMeshPrefix(mInstance, &prefix);
    VerifyOrExit(err == OT_ERROR_NONE, error = PostError::kPostSetFail,
                 errorDescription = "Failed at delete prefix :" + std::string(otThreadErrorToString(err)));

exit:

    if (error == PostError::kPostErrorNone)
    {
        aResponse.SetBody(requestBody);
        errorCode = mResponseCodeMap.at(HttpStatusCode::kStatusOk);
        aResponse.SetResponsCode(errorCode);
        aResponse.SetComplete();
    }
    else if (error == PostError::kPostSetFail)
    {
        ErrorHandler(aResponse, HttpStatusCode::kStatusInternalServerError, errorDescription);
    }
    else
    {
        ErrorHandler(aResponse, HttpStatusCode::kStatusBadRequest, errorDescription);
    }
}

void Resource::CurrentNetworkCommission(const Request &aRequest, Response &aResponse) const
{
    std::string errorCode;

    if (aRequest.GetMethod() == HttpMethod::kPost)
    {
        PostCurrentNetworkCommission(aRequest, aResponse);
    }

    else if (aRequest.GetMethod() == HttpMethod::kOptions)
    {
        ErrorHandler(aResponse, HttpStatusCode::kStatusOk, OT_REST_EMPTY_DESCRIPTION);
    }
    else
    {
        ErrorHandler(aResponse, HttpStatusCode::kStatusMethodNotAllowed, OT_REST_405_DESCRIPTION);
    }
}

void Resource::CurrentNetworkCommissionCallback(const Request &aRequest, Response &aResponse)
{
    otError             err = OT_ERROR_NONE;
    std::string         errorDescription;
    bool                complete = true;
    std::string         errorCode;
    PostError           error = PostError::kPostErrorNone;
    std::string         requestBody;
    std::string         pskd;
    unsigned long       timeout = kDefaultJoinerTimeout;
    const otExtAddress *addrPtr = nullptr;

    auto duration = duration_cast<microseconds>(steady_clock::now() - aResponse.GetStartTime()).count();

    VerifyOrExit(duration >= kResetSleepInterval, complete = false);

    requestBody = aRequest.GetBody();

    VerifyOrExit(Json::JsonString2String(requestBody, std::string("pskd"), pskd), error = PostError::kPostBadRequest,
                 errorDescription = "Failed at decode json: pskd");

    err = otCommissionerAddJoiner(mInstance, addrPtr, pskd.c_str(), static_cast<uint32_t>(timeout));
    VerifyOrExit(err == OT_ERROR_NONE, error = PostError::kPostBadRequest,
                 errorDescription = "Failed at commissioner add joiner: " + std::string(otThreadErrorToString(err)));

exit:
    if (complete)
    {
        if (error == PostError::kPostErrorNone)
        {
            aResponse.SetBody(requestBody);
            errorCode = mResponseCodeMap.at(HttpStatusCode::kStatusOk);
            aResponse.SetResponsCode(errorCode);
            aResponse.SetComplete();
        }
        else
        {
            ErrorHandler(aResponse, HttpStatusCode::kStatusBadRequest, errorDescription);
        }
    }
}

void Resource::PostCurrentNetworkCommission(const Request &aRequest, Response &aResponse) const
{
    OT_UNUSED_VARIABLE(aRequest);
    otError     err = OT_ERROR_NONE;
    std::string errorDescription;
    PostError   error = PostError::kPostErrorNone;

    err = otCommissionerStart(mInstance, &Resource::HandleCommissionerStateChanged, &Resource::HandleJoinerEvent,
                              const_cast<Resource *>(this));
    VerifyOrExit(err == OT_ERROR_NONE, error = PostError::kPostSetFail,
                 errorDescription = "Failed at commissioner start: " + std::string(otThreadErrorToString(err)));

exit:
    if (error == PostError::kPostErrorNone)
    {
        aResponse.SetCallback();

        aResponse.SetStartTime(steady_clock::now());
    }
    else
    {
        ErrorHandler(aResponse, HttpStatusCode::kStatusInternalServerError, errorDescription);
    }
}

void Resource::CurrentNetwork(const Request &aRequest, Response &aResponse) const
{
    std::string errorCode;

    if (aRequest.GetMethod() == HttpMethod::kGet)
    {
        GetCurrentNetwork(aResponse);
    }
    else if (aRequest.GetMethod() == HttpMethod::kPut)
    {
        PutCurrentNetwork(aResponse);
    }
    else if (aRequest.GetMethod() == HttpMethod::kOptions)
    {
        ErrorHandler(aResponse, HttpStatusCode::kStatusOk, OT_REST_EMPTY_DESCRIPTION);
    }
    else
    {
        ErrorHandler(aResponse, HttpStatusCode::kStatusMethodNotAllowed, OT_REST_405_DESCRIPTION);
    }
}

void Resource::PutCurrentNetwork(Response &aResponse) const
{
    otInstanceFactoryReset(mInstance);
    aResponse.SetCallback();
    aResponse.SetStartTime(steady_clock::now());
}

void Resource::GetCurrentNetwork(Response &aResponse) const
{
    otError              err = OT_ERROR_NONE;
    std::string          errorDescription;
    otbrError            error = OTBR_ERROR_NONE;
    std::string          body;
    otOperationalDataset dataset;
    std::string          errorCode;

    err = otDatasetGetActive(mInstance, &dataset);
    VerifyOrExit(err == OT_ERROR_NONE, error = OTBR_ERROR_REST,
                 errorDescription = "Failed at get active dataset: " + std::string(otThreadErrorToString(err)));

    body = Json::Network2JsonString(dataset);

    aResponse.SetBody(body);

exit:

    if (error == OTBR_ERROR_NONE)
    {
        errorCode = mResponseCodeMap.at(HttpStatusCode::kStatusOk);
        aResponse.SetResponsCode(errorCode);
    }
    else
    {
        ErrorHandler(aResponse, HttpStatusCode::kStatusInternalServerError, errorDescription);
    }
}

void Resource::PutCurrentNetworkCallback(const Request &aRequest, Response &aResponse)
{
    otError         err = OT_ERROR_NONE;
    std::string     errorDescription;
    std::string     errorCode;
    bool            complete = true;
    PostError       error    = PostError::kPostErrorNone;
    otMasterKey     masterKey;
    std::string     requestBody;
    otExtendedPanId extPanid;
    long            panid;
    std::string     masterkey;
    std::string     prefix;
    std::string     extPanId;
    std::string     networkName;
    std::string     panId;
    int32_t         channel;
    char *          endptr;
    bool            defaultRoute;

    otBorderRouterConfig config;

    auto duration = duration_cast<microseconds>(steady_clock::now() - aResponse.GetStartTime()).count();

    VerifyOrExit(aRequest.GetMethod() == HttpMethod::kPut, complete = false);

    VerifyOrExit(duration >= kResetSleepInterval, complete = false);

    requestBody = aRequest.GetBody();

    VerifyOrExit(Json::JsonString2String(requestBody, std::string("masterKey"), masterkey),
                 error = PostError::kPostBadRequest, errorDescription = "Failed at decode json : masterKey");
    VerifyOrExit(Json::JsonString2Bool(requestBody, std::string("defaultRoute"), defaultRoute),
                 error = PostError::kPostBadRequest, errorDescription = "Failed at decode json : defaultRoute");
    VerifyOrExit(Json::JsonString2String(requestBody, std::string("prefix"), prefix),
                 error = PostError::kPostBadRequest, errorDescription = "Failed at decode json : prefix");
    VerifyOrExit(Json::JsonString2String(requestBody, std::string("extPanId"), extPanId),
                 error = PostError::kPostBadRequest, errorDescription = "Failed at decode json : extPanId");
    VerifyOrExit(Json::JsonString2String(requestBody, std::string("networkName"), networkName),
                 error = PostError::kPostBadRequest, errorDescription = "Failed at decode json : networkName");
    VerifyOrExit(Json::JsonString2Int(requestBody, std::string("channel"), channel), error = PostError::kPostBadRequest,
                 errorDescription = "Failed at decode json : channel");
    VerifyOrExit(Json::JsonString2String(requestBody, std::string("panId"), panId), error = PostError::kPostBadRequest,
                 errorDescription = "Failed at decode json : panId");
    VerifyOrExit(otbr::Utils::Hex2Bytes(masterkey.c_str(), masterKey.m8, sizeof(masterKey.m8)) == OT_MASTER_KEY_SIZE,
                 error = PostError::kPostBadRequest, errorDescription = "Failed at encode masterkey : not valid");

    // Set Master Key
    err = otThreadSetMasterKey(mInstance, &masterKey);
    VerifyOrExit(err == OT_ERROR_NONE, error = PostError::kPostSetFail,
                 errorDescription = "Failed at set masterkey: " + std::string(otThreadErrorToString(err)));

    // Set Network Name
    err = otThreadSetNetworkName(mInstance, networkName.c_str());
    VerifyOrExit(err == OT_ERROR_NONE, error = PostError::kPostSetFail,
                 errorDescription = "Failed at set networkname: " + std::string(otThreadErrorToString(err)));

    // Set Channel
    err = otLinkSetChannel(mInstance, channel);
    VerifyOrExit(err == OT_ERROR_NONE, error = PostError::kPostSetFail,
                 errorDescription = "Failed at set channel: " + std::string(otThreadErrorToString(err)));

    // Set ExtPanId
    VerifyOrExit(otbr::Utils::Hex2Bytes(extPanId.c_str(), extPanid.m8, sizeof(extPanid)) == sizeof(extPanid),
                 error = PostError::kPostBadRequest, errorDescription = "Failed at encode extpanid: not valid");

    err = otThreadSetExtendedPanId(mInstance, &extPanid);
    VerifyOrExit(err == OT_ERROR_NONE, error = PostError::kPostSetFail,
                 errorDescription = "Failed at set extpanid: " + std::string(otThreadErrorToString(err)));

    // Set PanId
    panid = strtol(panId.c_str(), &endptr, 0);
    VerifyOrExit(*endptr == '\0', error = PostError::kPostBadRequest,
                 errorDescription = "Failed at check panid: not valid");
    err = otLinkSetPanId(mInstance, static_cast<otPanId>(panid));
    VerifyOrExit(err == OT_ERROR_NONE, error = PostError::kPostSetFail,
                 errorDescription = "Failed at set panid: " + std::string(otThreadErrorToString(err)));

    // IfConfig Up
    err = otIp6SetEnabled(mInstance, true);
    VerifyOrExit(err == OT_ERROR_NONE, error = PostError::kPostSetFail,
                 errorDescription = "Failed at enable Ip6: " + std::string(otThreadErrorToString(err)));
    // Thread start
    err = otThreadSetEnabled(mInstance, true);
    VerifyOrExit(err == OT_ERROR_NONE, error = PostError::kPostSetFail,
                 errorDescription = "Failed at enable thread : " + std::string(otThreadErrorToString(err)));

    // Add prefix
    memset(&config, 0, sizeof(otBorderRouterConfig));

    err = otIp6AddressFromString(prefix.c_str(), &config.mPrefix.mPrefix);
    VerifyOrExit(err == OT_ERROR_NONE, error = PostError::kPostBadRequest,
                 errorDescription = "Failed at encode prefix : " + std::string(otThreadErrorToString(err)));

    config.mPreferred      = true;
    config.mSlaac          = true;
    config.mStable         = true;
    config.mOnMesh         = true;
    config.mDefaultRoute   = defaultRoute;
    config.mPrefix.mLength = 64;

    err = otBorderRouterAddOnMeshPrefix(mInstance, &config);
    VerifyOrExit(err == OT_ERROR_NONE, error = PostError::kPostSetFail,
                 errorDescription = "Failed at add prefix : " + std::string(otThreadErrorToString(err)));

exit:
    if (complete)
    {
        if (error == PostError::kPostErrorNone)
        {
            aResponse.SetBody(requestBody);
            errorCode = mResponseCodeMap.at(HttpStatusCode::kStatusOk);
            aResponse.SetResponsCode(errorCode);
            aResponse.SetComplete();
        }
        else if (error == PostError::kPostSetFail)
        {
            ErrorHandler(aResponse, HttpStatusCode::kStatusInternalServerError, errorDescription);
        }
        else
        {
            ErrorHandler(aResponse, HttpStatusCode::kStatusBadRequest, errorDescription);
        }
    }
}

void Resource::GetNodeInfo(Response &aResponse) const
{
    otError              err = OT_ERROR_NONE;
    std::string          errorDescription;
    otbrError            error = OTBR_ERROR_NONE;
    struct NodeInfo      node;
    std::string          body;
    otOperationalDataset dataset;
    std::string          errorCode;

    node.mRole        = otThreadGetDeviceRole(mInstance);
    node.mWpanService = "uninitialized";
    if (node.mRole == OT_DEVICE_ROLE_DISABLED)
    {
        node.mWpanService = "offline";
    }
    else if (node.mRole == OT_DEVICE_ROLE_DETACHED)
    {
        node.mWpanService = "associating";
    }
    else
    {
        node.mWpanService = "associated";
    }

    // If state is not valid
    VerifyOrExit(node.mWpanService == "associated", errorDescription = "Fail at check node state : Unsupported state");

    //  Eui64
    otLinkGetFactoryAssignedIeeeEui64(mInstance, &node.mEui64);

    // MeshLocalAddress
    node.mMeshLocalAddress = *otThreadGetMeshLocalEid(mInstance);

    // Version
    node.mVersion = otGetVersionString();

    // MeshLocalPrefix
    err = otDatasetGetActive(mInstance, &dataset);
    VerifyOrExit(err == OT_ERROR_NONE, error = OTBR_ERROR_REST,
                 errorDescription = "Failed at get active dataset: " + std::string(otThreadErrorToString(err)));
    node.mMeshLocalPrefix = dataset.mMeshLocalPrefix.m8;

    // PanId
    node.mPanId = otLinkGetPanId(mInstance);

    // Channel
    node.mChannel = otLinkGetChannel(mInstance);

    // Network Name
    node.mNetworkName = otThreadGetNetworkName(mInstance);

    // ExtPanId
    node.mExtPanId = reinterpret_cast<const uint8_t *>(otThreadGetExtendedPanId(mInstance));

    body = Json::Node2JsonString(node);

    aResponse.SetBody(body);

exit:
    if (error == OTBR_ERROR_NONE)
    {
        if (node.mWpanService != "associated")
        {
            ErrorHandler(aResponse, HttpStatusCode::kStatusBadRequest, errorDescription);
        }
        else
        {
            errorCode = mResponseCodeMap.at(HttpStatusCode::kStatusOk);
            aResponse.SetResponsCode(errorCode);
            aResponse.SetComplete();
        }
    }
    else
    {
        ErrorHandler(aResponse, HttpStatusCode::kStatusInternalServerError, errorDescription);
    }
}

void Resource::NodeInfo(const Request &aRequest, Response &aResponse) const
{
    std::string errorCode;
    if (aRequest.GetMethod() == HttpMethod::kGet)
    {
        GetNodeInfo(aResponse);
    }
    else
    {
        ErrorHandler(aResponse, HttpStatusCode::kStatusMethodNotAllowed, OT_REST_405_DESCRIPTION);
    }
}

void Resource::GetDataExtendedAddr(Response &aResponse) const
{
    const uint8_t *extAddress = reinterpret_cast<const uint8_t *>(otLinkGetExtendedAddress(mInstance));
    std::string    errorCode;
    std::string    body = Json::Bytes2HexJsonString(extAddress, OT_EXT_ADDRESS_SIZE);

    aResponse.SetBody(body);
    errorCode = mResponseCodeMap.at(HttpStatusCode::kStatusOk);
    aResponse.SetResponsCode(errorCode);
}

void Resource::ExtendedAddr(const Request &aRequest, Response &aResponse) const
{
    std::string errorCode;

    if (aRequest.GetMethod() == HttpMethod::kGet)
    {
        GetDataExtendedAddr(aResponse);
    }
    else
    {
        ErrorHandler(aResponse, HttpStatusCode::kStatusMethodNotAllowed, OT_REST_405_DESCRIPTION);
    }
}

void Resource::GetDataState(Response &aResponse) const
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
    state = Json::Number2JsonString(role);
    aResponse.SetBody(state);
    errorCode = mResponseCodeMap.at(HttpStatusCode::kStatusOk);
    aResponse.SetResponsCode(errorCode);
}

void Resource::State(const Request &aRequest, Response &aResponse) const
{
    std::string errorCode;

    if (aRequest.GetMethod() == HttpMethod::kGet)
    {
        GetDataState(aResponse);
    }
    else
    {
        ErrorHandler(aResponse, HttpStatusCode::kStatusMethodNotAllowed, OT_REST_405_DESCRIPTION);
    }
}

void Resource::GetDataNetworkName(Response &aResponse) const
{
    std::string networkName;
    std::string errorCode;

    networkName = otThreadGetNetworkName(mInstance);
    networkName = Json::String2JsonString(networkName);

    aResponse.SetBody(networkName);
    errorCode = mResponseCodeMap.at(HttpStatusCode::kStatusOk);
    aResponse.SetResponsCode(errorCode);
}

void Resource::NetworkName(const Request &aRequest, Response &aResponse) const
{
    std::string errorCode;

    if (aRequest.GetMethod() == HttpMethod::kGet)
    {
        GetDataNetworkName(aResponse);
    }
    else
    {
        ErrorHandler(aResponse, HttpStatusCode::kStatusMethodNotAllowed, OT_REST_405_DESCRIPTION);
    }
}

void Resource::GetDataLeaderData(Response &aResponse) const
{
    otError      err = OT_ERROR_NONE;
    std::string  errorDescription;
    otbrError    error = OTBR_ERROR_NONE;
    otLeaderData leaderData;
    std::string  body;
    std::string  errorCode;

    err = otThreadGetLeaderData(mInstance, &leaderData);

    VerifyOrExit(err == OT_ERROR_NONE, error = OTBR_ERROR_REST,
                 errorDescription = "Failed at get leader data: " + std::string(otThreadErrorToString(err)));

    body = Json::LeaderData2JsonString(leaderData);

    aResponse.SetBody(body);

exit:
    if (error == OTBR_ERROR_NONE)
    {
        errorCode = mResponseCodeMap.at(HttpStatusCode::kStatusOk);
        aResponse.SetResponsCode(errorCode);
    }
    else
    {
        ErrorHandler(aResponse, HttpStatusCode::kStatusInternalServerError, errorDescription);
    }
}

void Resource::LeaderData(const Request &aRequest, Response &aResponse) const
{
    std::string errorCode;
    if (aRequest.GetMethod() == HttpMethod::kGet)
    {
        GetDataLeaderData(aResponse);
    }
    else
    {
        ErrorHandler(aResponse, HttpStatusCode::kStatusMethodNotAllowed, OT_REST_405_DESCRIPTION);
    }
}

void Resource::GetDataNumOfRoute(Response &aResponse) const
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

    body = Json::Number2JsonString(count);

    aResponse.SetBody(body);
    errorCode = mResponseCodeMap.at(HttpStatusCode::kStatusOk);
    aResponse.SetResponsCode(errorCode);
}

void Resource::NumOfRoute(const Request &aRequest, Response &aResponse) const
{
    std::string errorCode;

    if (aRequest.GetMethod() == HttpMethod::kGet)
    {
        GetDataNumOfRoute(aResponse);
    }
    else
    {
        ErrorHandler(aResponse, HttpStatusCode::kStatusMethodNotAllowed, OT_REST_405_DESCRIPTION);
    }
}

void Resource::GetDataRloc16(Response &aResponse) const
{
    uint16_t    rloc16 = otThreadGetRloc16(mInstance);
    std::string body;
    std::string errorCode;

    body = Json::Number2JsonString(rloc16);

    aResponse.SetBody(body);
    errorCode = mResponseCodeMap.at(HttpStatusCode::kStatusOk);
    aResponse.SetResponsCode(errorCode);
}

void Resource::Rloc16(const Request &aRequest, Response &aResponse) const
{
    std::string errorCode;

    if (aRequest.GetMethod() == HttpMethod::kGet)
    {
        GetDataRloc16(aResponse);
    }
    else
    {
        ErrorHandler(aResponse, HttpStatusCode::kStatusMethodNotAllowed, OT_REST_405_DESCRIPTION);
    }
}

void Resource::GetDataExtendedPanId(Response &aResponse) const
{
    const uint8_t *extPanId = reinterpret_cast<const uint8_t *>(otThreadGetExtendedPanId(mInstance));
    std::string    body     = Json::Bytes2HexJsonString(extPanId, OT_EXTENDED_PAN_ID_LENGTH);
    std::string    errorCode;

    aResponse.SetBody(body);
    errorCode = mResponseCodeMap.at(HttpStatusCode::kStatusOk);
    aResponse.SetResponsCode(errorCode);
}

void Resource::ExtendedPanId(const Request &aRequest, Response &aResponse) const
{
    std::string errorCode;

    if (aRequest.GetMethod() == HttpMethod::kGet)
    {
        GetDataExtendedPanId(aResponse);
    }
    else
    {
        ErrorHandler(aResponse, HttpStatusCode::kStatusMethodNotAllowed, OT_REST_405_DESCRIPTION);
    }
}

void Resource::GetDataRloc(Response &aResponse) const
{
    otIp6Address rlocAddress = *otThreadGetRloc(mInstance);
    std::string  body;
    std::string  errorCode;

    body = Json::IpAddr2JsonString(rlocAddress);

    aResponse.SetBody(body);
    errorCode = mResponseCodeMap.at(HttpStatusCode::kStatusOk);
    aResponse.SetResponsCode(errorCode);
}

void Resource::Rloc(const Request &aRequest, Response &aResponse) const
{
    std::string errorCode;

    if (aRequest.GetMethod() == HttpMethod::kGet)
    {
        GetDataRloc(aResponse);
    }
    else
    {
        ErrorHandler(aResponse, HttpStatusCode::kStatusMethodNotAllowed, OT_REST_405_DESCRIPTION);
    }
}

void Resource::Networks(const Request &aRequest, Response &aResponse) const
{
    std::string errorCode;

    if (aRequest.GetMethod() == HttpMethod::kGet)
    {
        GetNetworks(aResponse);
    }
    else if (aRequest.GetMethod() == HttpMethod::kPost)
    {
        PostNetworks(aResponse);
    }
    else if (aRequest.GetMethod() == HttpMethod::kOptions)
    {
        ErrorHandler(aResponse, HttpStatusCode::kStatusOk, OT_REST_EMPTY_DESCRIPTION);
    }
}

void Resource::PostNetworks(Response &aResponse) const
{
    otInstanceFactoryReset(mInstance);
    aResponse.SetCallback();
    aResponse.SetStartTime(steady_clock::now());
}

void Resource::PostNetworksCallback(const Request &aRequest, Response &aResponse)
{
    otError              err = OT_ERROR_NONE;
    std::string          errorDescription;
    std::string          errorCode;
    bool                 complete = true;
    PostError            error    = PostError::kPostErrorNone;
    NetworkConfiguration network;
    char *               endptr;
    long                 panId;
    otPskc               pskc;
    otMasterKey          masterKey;
    otExtendedPanId      extPanId;
    std::string          requestBody;
    otbr::Psk::Pskc      psk;
    otBorderRouterConfig config;
    char                 pskcStr[OT_PSKC_LENGTH * 2 + 1];
    uint8_t              extPanIdBytes[OT_EXTENDED_PAN_ID_LENGTH];

    auto duration = duration_cast<microseconds>(steady_clock::now() - aResponse.GetStartTime()).count();

    VerifyOrExit(aRequest.GetMethod() == HttpMethod::kPost, complete = false);
    VerifyOrExit(duration >= kResetSleepInterval, complete = false);

    requestBody                 = aRequest.GetBody();
    pskcStr[OT_PSKC_LENGTH * 2] = '\0';

    VerifyOrExit(Json::JsonString2NetworkConfiguration(requestBody, network), error = PostError::kPostBadRequest,
                 errorDescription = "Failed at decode json : not valid");

    otbr::Utils::Hex2Bytes(network.mExtPanId.c_str(), extPanIdBytes, OT_EXTENDED_PAN_ID_LENGTH);
    otbr::Utils::Bytes2Hex(psk.ComputePskc(extPanIdBytes, network.mNetworkName.c_str(), network.mPassphrase.c_str()),
                           OT_PSKC_LENGTH, pskcStr);

    VerifyOrExit(otbr::Utils::Hex2Bytes(network.mMasterKey.c_str(), masterKey.m8, sizeof(masterKey.m8)) ==
                     OT_MASTER_KEY_SIZE,
                 error = PostError::kPostBadRequest, errorDescription = "Failed at encode masterkey : not valid");
    // Set Master Key
    err = otThreadSetMasterKey(mInstance, &masterKey);
    VerifyOrExit(err == OT_ERROR_NONE, error = PostError::kPostSetFail,
                 errorDescription = "Failed at set masterkey: " + std::string(otThreadErrorToString(err)));

    // NetworkName
    err = otThreadSetNetworkName(mInstance, network.mNetworkName.c_str());
    VerifyOrExit(err == OT_ERROR_NONE, error = PostError::kPostSetFail,
                 errorDescription = "Failed at set networkname: " + std::string(otThreadErrorToString(err)));

    // Channel
    err = otLinkSetChannel(mInstance, network.mChannel);
    VerifyOrExit(err == OT_ERROR_NONE, error = PostError::kPostSetFail,
                 errorDescription = "Failed at set channel: " + std::string(otThreadErrorToString(err)));

    // ExtPanId

    VerifyOrExit(otbr::Utils::Hex2Bytes(network.mExtPanId.c_str(), extPanId.m8, sizeof(extPanId)) == sizeof(extPanId),
                 error = PostError::kPostBadRequest, errorDescription = "Failed at encode extpanid: not valid");
    err = otThreadSetExtendedPanId(mInstance, &extPanId);
    VerifyOrExit(err == OT_ERROR_NONE, error = PostError::kPostSetFail,
                 errorDescription = "Failed at set extpanid: " + std::string(otThreadErrorToString(err)));

    // PanId
    panId = strtol(network.mPanId.c_str(), &endptr, 0);
    VerifyOrExit(*endptr == '\0', error = PostError::kPostBadRequest,
                 errorDescription = "Failed at check panid: not valid");
    err = otLinkSetPanId(mInstance, static_cast<otPanId>(panId));
    VerifyOrExit(err == OT_ERROR_NONE, error = PostError::kPostSetFail,
                 errorDescription = "Failed at set panid: " + std::string(otThreadErrorToString(err)));

    // pskc
    VerifyOrExit(otbr::Utils::Hex2Bytes(pskcStr, pskc.m8, sizeof(pskc)) == sizeof(pskc),
                 error = PostError::kPostBadRequest, errorDescription = "Failed at encode pskc: not valid");
    err = otThreadSetPskc(mInstance, &pskc);
    VerifyOrExit(err == OT_ERROR_NONE, error = PostError::kPostSetFail,
                 errorDescription = "Failed at set pskc: " + std::string(otThreadErrorToString(err)));

    // IfConfig Up
    err = otIp6SetEnabled(mInstance, true);
    VerifyOrExit(err == OT_ERROR_NONE, error = PostError::kPostSetFail,
                 errorDescription = "Failed at enable Ip6: " + std::string(otThreadErrorToString(err)));

    // Thread start
    err = otThreadSetEnabled(mInstance, true);
    VerifyOrExit(err == OT_ERROR_NONE, error = PostError::kPostSetFail,
                 errorDescription = "Failed at enable thread: " + std::string(otThreadErrorToString(err)));

    // Add prefix
    memset(&config, 0, sizeof(otBorderRouterConfig));
    err = otIp6AddressFromString(network.mPrefix.c_str(), &config.mPrefix.mPrefix);
    VerifyOrExit(err == OT_ERROR_NONE, error = PostError::kPostBadRequest,
                 errorDescription = "Failed at encode prefix: " + std::string(otThreadErrorToString(err)));

    config.mPreferred      = true;
    config.mSlaac          = true;
    config.mStable         = true;
    config.mOnMesh         = true;
    config.mDefaultRoute   = network.mDefaultRoute;
    config.mPrefix.mLength = 64;
    err                    = otBorderRouterAddOnMeshPrefix(mInstance, &config);
    VerifyOrExit(err == OT_ERROR_NONE, error = PostError::kPostSetFail,
                 errorDescription = "Failed at add prefix: " + std::string(otThreadErrorToString(err)));

exit:
    if (complete)
    {
        if (error == PostError::kPostErrorNone)
        {
            aResponse.SetBody(requestBody);
            errorCode = mResponseCodeMap.at(HttpStatusCode::kStatusOk);
            aResponse.SetResponsCode(errorCode);
            aResponse.SetComplete();
        }
        else if (error == PostError::kPostSetFail)
        {
            ErrorHandler(aResponse, HttpStatusCode::kStatusInternalServerError, errorDescription);
        }
        else
        {
            ErrorHandler(aResponse, HttpStatusCode::kStatusBadRequest, errorDescription);
        }
    }
}

void Resource::GetNetworks(Response &aResponse) const
{
    aResponse.SetCallback();
    aResponse.SetStartTime(steady_clock::now());
    auto threadHelper = mNcp->GetThreadHelper();
    threadHelper->Scan(std::bind(&Resource::NetworksResponseHandler, const_cast<Resource *>(this), &aResponse, _1, _2));
}

void Resource::NetworksResponseHandler(Response *                             aResponse,
                                       otError                                aError,
                                       const std::vector<otActiveScanResult> &aResult)
{
    std::vector<ActiveScanResult> results;
    std::string                   body;
    std::string                   errorCode;

    if (aError != OT_ERROR_NONE)
    {
        ErrorHandler(*aResponse, HttpStatusCode::kStatusInternalServerError, "Scan occurs internal error");
    }
    else
    {
        for (const auto &r : aResult)
        {
            ActiveScanResult result;

            for (int i = 0; i < OT_EXT_ADDRESS_SIZE; ++i)
            {
                result.mExtAddress[i] = r.mExtAddress.m8[i];
            }
            for (int i = 0; i < OT_EXTENDED_PAN_ID_LENGTH; ++i)
            {
                result.mExtendedPanId[i] = r.mExtendedPanId.m8[i];
            }

            result.mNetworkName = r.mNetworkName.m8;
            result.mSteeringData =
                std::vector<uint8_t>(r.mSteeringData.m8, r.mSteeringData.m8 + r.mSteeringData.mLength);
            result.mPanId         = r.mPanId;
            result.mJoinerUdpPort = r.mJoinerUdpPort;
            result.mChannel       = r.mChannel;
            result.mRssi          = r.mRssi;
            result.mLqi           = r.mLqi;
            result.mVersion       = r.mVersion;
            result.mIsNative      = r.mIsNative;
            result.mIsJoinable    = r.mIsJoinable;

            results.emplace_back(result);
        }

        body      = Json::ScanNetworks2JsonString(results);
        errorCode = mResponseCodeMap.at(HttpStatusCode::kStatusOk);
        aResponse->SetResponsCode(errorCode);
        aResponse->SetBody(body);
        aResponse->SetComplete();
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

void Resource::Diagnostic(const Request &aRequest, Response &aResponse) const
{
    static const uint8_t kAllTlvTypes[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 14, 15, 16, 17, 18, 19};

    OT_UNUSED_VARIABLE(aRequest);
    otError             err = OT_ERROR_NONE;
    std::string         errorDescription;
    otbrError           error         = OTBR_ERROR_NONE;
    struct otIp6Address rloc16address = *otThreadGetRloc(mInstance);
    struct otIp6Address multicastAddress;

    otThreadSetReceiveDiagnosticGetCallback(mInstance, &Resource::DiagnosticResponseHandler,
                                            const_cast<Resource *>(this));
    err = otThreadSendDiagnosticGet(mInstance, &rloc16address, kAllTlvTypes, sizeof(kAllTlvTypes));
    VerifyOrExit(err == OT_ERROR_NONE, error = OTBR_ERROR_REST,
                 errorDescription =
                     "Failed at send self diagnostic message: " + std::string(otThreadErrorToString(err)));

    err = otIp6AddressFromString(kMulticastAddrAllRouters, &multicastAddress);
    VerifyOrExit(err == OT_ERROR_NONE, error = OTBR_ERROR_REST,
                 errorDescription =
                     "Failed at get encode multicast address: " + std::string(otThreadErrorToString(err)));

    err = otThreadSendDiagnosticGet(mInstance, &multicastAddress, kAllTlvTypes, sizeof(kAllTlvTypes));
    VerifyOrExit(err == OT_ERROR_NONE, error = OTBR_ERROR_REST,
                 errorDescription =
                     "Failed at send multicast diagnostic message: " + std::string(otThreadErrorToString(err)));

exit:

    if (error == OTBR_ERROR_NONE)
    {
        aResponse.SetStartTime(steady_clock::now());
        aResponse.SetCallback();
    }
    else
    {
        ErrorHandler(aResponse, HttpStatusCode::kStatusInternalServerError, errorDescription);
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
            keyRloc = Json::CString2JsonString(rloc);
        }
        diagSet.push_back(diagTlv);
    }
    UpdateDiag(keyRloc, diagSet);
}

void Resource::HandleCommissionerStateChanged(otCommissionerState aState, void *aContext)
{
    static_cast<Resource *>(aContext)->HandleCommissionerStateChanged(aState);
}

void Resource::HandleCommissionerStateChanged(otCommissionerState aState) const
{
    switch (aState)
    {
    case OT_COMMISSIONER_STATE_DISABLED:
        otbrLog(OTBR_LOG_INFO, "OTBR-REST: commissioner state disabled");
        break;
    case OT_COMMISSIONER_STATE_ACTIVE:
        otbrLog(OTBR_LOG_INFO, "OTBR-REST: commissioner state active");
        break;
    case OT_COMMISSIONER_STATE_PETITION:
        otbrLog(OTBR_LOG_INFO, "OTBR-REST: commissioner state petition");
        break;
    }
}

void Resource::HandleJoinerEvent(otCommissionerJoinerEvent aEvent,
                                 const otJoinerInfo *      aJoinerInfo,
                                 const otExtAddress *      aJoinerId,
                                 void *                    aContext)
{
    static_cast<Resource *>(aContext)->HandleJoinerEvent(aEvent, aJoinerInfo, aJoinerId);
}

void Resource::HandleJoinerEvent(otCommissionerJoinerEvent aEvent,
                                 const otJoinerInfo *      aJoinerInfo,
                                 const otExtAddress *      aJoinerId) const
{
    OT_UNUSED_VARIABLE(aJoinerInfo);
    OT_UNUSED_VARIABLE(aJoinerId);

    switch (aEvent)
    {
    case OT_COMMISSIONER_JOINER_START:
        otbrLog(OTBR_LOG_INFO, "OTBR-REST: joiner start");
        break;
    case OT_COMMISSIONER_JOINER_CONNECTED:
        otbrLog(OTBR_LOG_INFO, "OTBR-REST: joiner connected");
        break;
    case OT_COMMISSIONER_JOINER_FINALIZE:
        otbrLog(OTBR_LOG_INFO, "OTBR-REST: joiner finalize");
        break;
    case OT_COMMISSIONER_JOINER_END:
        otbrLog(OTBR_LOG_INFO, "OTBR-REST: joiner end");
        break;
    case OT_COMMISSIONER_JOINER_REMOVED:
        otbrLog(OTBR_LOG_INFO, "OTBR-REST: joiner remove");
        break;
    }
}

} // namespace Rest
} // namespace otbr
