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

#define OTBR_LOG_TAG "REST"

#include "rest/resource.hpp"
#include <openthread/commissioner.h>

#define OT_PSKC_MAX_LENGTH 16
#define OT_EXTENDED_PANID_LENGTH 8

#define OT_REST_RESOURCE_PATH_DIAGNOSTICS "/diagnostics"
#define OT_REST_RESOURCE_PATH_NODE "/node"
#define OT_REST_RESOURCE_PATH_NODE_BAID "/node/ba-id"
#define OT_REST_RESOURCE_PATH_NODE_RLOC "/node/rloc"
#define OT_REST_RESOURCE_PATH_NODE_RLOC16 "/node/rloc16"
#define OT_REST_RESOURCE_PATH_NODE_EXTADDRESS "/node/ext-address"
#define OT_REST_RESOURCE_PATH_NODE_STATE "/node/state"
#define OT_REST_RESOURCE_PATH_NODE_NETWORKNAME "/node/network-name"
#define OT_REST_RESOURCE_PATH_NODE_LEADERDATA "/node/leader-data"
#define OT_REST_RESOURCE_PATH_NODE_NUMOFROUTER "/node/num-of-router"
#define OT_REST_RESOURCE_PATH_NODE_EXTPANID "/node/ext-panid"
#define OT_REST_RESOURCE_PATH_NODE_DATASET_ACTIVE "/node/dataset/active"
#define OT_REST_RESOURCE_PATH_NODE_DATASET_PENDING "/node/dataset/pending"
#define OT_REST_RESOURCE_PATH_NODE_COMMISSIONER_STATE "/node/commissioner/state"
#define OT_REST_RESOURCE_PATH_NODE_COMMISSIONER_JOINER "/node/commissioner/joiner"
#define OT_REST_RESOURCE_PATH_NODE_COPROCESSOR "/node/coprocessor"
#define OT_REST_RESOURCE_PATH_NODE_COPROCESSOR_VERSION "/node/coprocessor/version"
#define OT_REST_RESOURCE_PATH_NETWORK "/networks"
#define OT_REST_RESOURCE_PATH_NETWORK_CURRENT "/networks/current"
#define OT_REST_RESOURCE_PATH_NETWORK_CURRENT_COMMISSION "/networks/commission"
#define OT_REST_RESOURCE_PATH_NETWORK_CURRENT_PREFIX "/networks/current/prefix"

#define OT_REST_HTTP_STATUS_200 "200 OK"
#define OT_REST_HTTP_STATUS_201 "201 Created"
#define OT_REST_HTTP_STATUS_204 "204 No Content"
#define OT_REST_HTTP_STATUS_400 "400 Bad Request"
#define OT_REST_HTTP_STATUS_404 "404 Not Found"
#define OT_REST_HTTP_STATUS_405 "405 Method Not Allowed"
#define OT_REST_HTTP_STATUS_408 "408 Request Timeout"
#define OT_REST_HTTP_STATUS_409 "409 Conflict"
#define OT_REST_HTTP_STATUS_500 "500 Internal Server Error"
#define OT_REST_HTTP_STATUS_507 "507 Insufficient Storage"

using std::chrono::duration_cast;
using std::chrono::microseconds;
using std::chrono::steady_clock;

using std::placeholders::_1;
using std::placeholders::_2;

namespace otbr {
namespace rest {

// MulticastAddr
static const char *kMulticastAddrAllRouters = "ff03::2";

// Default TlvTypes for Diagnostic inforamtion
static const uint8_t kAllTlvTypes[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 14, 15, 16, 17, 19};

// Timeout (in Microseconds) for deleting outdated diagnostics
static const uint32_t kDiagResetTimeout = 3000000;

// Timeout (in Microseconds) for collecting diagnostics
static const uint32_t kDiagCollectTimeout = 2000000;

static std::string GetHttpStatus(HttpStatusCode aErrorCode)
{
    std::string httpStatus;

    switch (aErrorCode)
    {
    case HttpStatusCode::kStatusOk:
        httpStatus = OT_REST_HTTP_STATUS_200;
        break;
    case HttpStatusCode::kStatusCreated:
        httpStatus = OT_REST_HTTP_STATUS_201;
        break;
    case HttpStatusCode::kStatusNoContent:
        httpStatus = OT_REST_HTTP_STATUS_204;
        break;
    case HttpStatusCode::kStatusBadRequest:
        httpStatus = OT_REST_HTTP_STATUS_400;
        break;
    case HttpStatusCode::kStatusResourceNotFound:
        httpStatus = OT_REST_HTTP_STATUS_404;
        break;
    case HttpStatusCode::kStatusMethodNotAllowed:
        httpStatus = OT_REST_HTTP_STATUS_405;
        break;
    case HttpStatusCode::kStatusRequestTimeout:
        httpStatus = OT_REST_HTTP_STATUS_408;
        break;
    case HttpStatusCode::kStatusConflict:
        httpStatus = OT_REST_HTTP_STATUS_409;
        break;
    case HttpStatusCode::kStatusInternalServerError:
        httpStatus = OT_REST_HTTP_STATUS_500;
        break;
    case HttpStatusCode::kStatusInsufficientStorage:
        httpStatus = OT_REST_HTTP_STATUS_507;
        break;
    }

    return httpStatus;
}

Resource::Resource(RcpHost *aHost)
    : mInstance(nullptr)
    , mHost(aHost)
{
    // Resource Handler
    mResourceMap.emplace(OT_REST_RESOURCE_PATH_DIAGNOSTICS, &Resource::Diagnostic);
    mResourceMap.emplace(OT_REST_RESOURCE_PATH_NODE, &Resource::NodeInfo);
    mResourceMap.emplace(OT_REST_RESOURCE_PATH_NODE_BAID, &Resource::BaId);
    mResourceMap.emplace(OT_REST_RESOURCE_PATH_NODE_STATE, &Resource::State);
    mResourceMap.emplace(OT_REST_RESOURCE_PATH_NODE_EXTADDRESS, &Resource::ExtendedAddr);
    mResourceMap.emplace(OT_REST_RESOURCE_PATH_NODE_NETWORKNAME, &Resource::NetworkName);
    mResourceMap.emplace(OT_REST_RESOURCE_PATH_NODE_RLOC16, &Resource::Rloc16);
    mResourceMap.emplace(OT_REST_RESOURCE_PATH_NODE_LEADERDATA, &Resource::LeaderData);
    mResourceMap.emplace(OT_REST_RESOURCE_PATH_NODE_NUMOFROUTER, &Resource::NumOfRoute);
    mResourceMap.emplace(OT_REST_RESOURCE_PATH_NODE_EXTPANID, &Resource::ExtendedPanId);
    mResourceMap.emplace(OT_REST_RESOURCE_PATH_NODE_RLOC, &Resource::Rloc);
    mResourceMap.emplace(OT_REST_RESOURCE_PATH_NODE_DATASET_ACTIVE, &Resource::DatasetActive);
    mResourceMap.emplace(OT_REST_RESOURCE_PATH_NODE_DATASET_PENDING, &Resource::DatasetPending);
    mResourceMap.emplace(OT_REST_RESOURCE_PATH_NODE_COMMISSIONER_STATE, &Resource::CommissionerState);
    mResourceMap.emplace(OT_REST_RESOURCE_PATH_NODE_COMMISSIONER_JOINER, &Resource::CommissionerJoiner);
    mResourceMap.emplace(OT_REST_RESOURCE_PATH_NODE_COPROCESSOR_VERSION, &Resource::CoprocessorVersion);

    // Resource callback handler
    mResourceCallbackMap.emplace(OT_REST_RESOURCE_PATH_DIAGNOSTICS, &Resource::HandleDiagnosticCallback);
}

void Resource::Init(void)
{
    mInstance = mHost->GetThreadHelper()->GetInstance();
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
        ErrorHandler(aResponse, HttpStatusCode::kStatusResourceNotFound);
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
        errorCode = GetHttpStatus(HttpStatusCode::kStatusOk);
        aResponse.SetResponsCode(errorCode);
        aResponse.SetBody(body);
        aResponse.SetComplete();
    }
}

void Resource::ErrorHandler(Response &aResponse, HttpStatusCode aErrorCode) const
{
    std::string errorMessage = GetHttpStatus(aErrorCode);
    std::string body         = Json::Error2JsonString(aErrorCode, errorMessage);

    aResponse.SetResponsCode(errorMessage);
    aResponse.SetBody(body);
    aResponse.SetComplete();
}

void Resource::GetNodeInfo(Response &aResponse) const
{
    otbrError       error = OTBR_ERROR_NONE;
    struct NodeInfo node  = {};
    otRouterInfo    routerInfo;
    uint8_t         maxRouterId;
    std::string     body;
    std::string     errorCode;

    VerifyOrExit(otBorderAgentGetId(mInstance, &node.mBaId) == OT_ERROR_NONE, error = OTBR_ERROR_REST);
    (void)otThreadGetLeaderData(mInstance, &node.mLeaderData);

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

    node.mRole        = GetDeviceRoleName(otThreadGetDeviceRole(mInstance));
    node.mExtAddress  = reinterpret_cast<const uint8_t *>(otLinkGetExtendedAddress(mInstance));
    node.mNetworkName = otThreadGetNetworkName(mInstance);
    node.mRloc16      = otThreadGetRloc16(mInstance);
    node.mExtPanId    = reinterpret_cast<const uint8_t *>(otThreadGetExtendedPanId(mInstance));
    node.mRlocAddress = *otThreadGetRloc(mInstance);

    body = Json::Node2JsonString(node);
    aResponse.SetBody(body);

exit:
    if (error == OTBR_ERROR_NONE)
    {
        errorCode = GetHttpStatus(HttpStatusCode::kStatusOk);
        aResponse.SetResponsCode(errorCode);
    }
    else
    {
        ErrorHandler(aResponse, HttpStatusCode::kStatusInternalServerError);
    }
}

void Resource::DeleteNodeInfo(Response &aResponse) const
{
    otbrError   error = OTBR_ERROR_NONE;
    std::string errorCode;

    VerifyOrExit(mHost->GetThreadHelper()->Detach() == OT_ERROR_NONE, error = OTBR_ERROR_INVALID_STATE);
    VerifyOrExit(otInstanceErasePersistentInfo(mInstance) == OT_ERROR_NONE, error = OTBR_ERROR_REST);
    mHost->Reset();

exit:
    if (error == OTBR_ERROR_NONE)
    {
        errorCode = GetHttpStatus(HttpStatusCode::kStatusOk);
        aResponse.SetResponsCode(errorCode);
    }
    else if (error == OTBR_ERROR_INVALID_STATE)
    {
        ErrorHandler(aResponse, HttpStatusCode::kStatusConflict);
    }
    else if (error != OTBR_ERROR_NONE)
    {
        ErrorHandler(aResponse, HttpStatusCode::kStatusInternalServerError);
    }
}

void Resource::NodeInfo(const Request &aRequest, Response &aResponse) const
{
    std::string errorCode;

    switch (aRequest.GetMethod())
    {
    case HttpMethod::kGet:
        GetNodeInfo(aResponse);
        break;
    case HttpMethod::kDelete:
        DeleteNodeInfo(aResponse);
        break;
    default:
        ErrorHandler(aResponse, HttpStatusCode::kStatusMethodNotAllowed);
        break;
    }
}

void Resource::GetDataBaId(Response &aResponse) const
{
    otbrError       error = OTBR_ERROR_NONE;
    otBorderAgentId id;
    std::string     body;
    std::string     errorCode;

    VerifyOrExit(otBorderAgentGetId(mInstance, &id) == OT_ERROR_NONE, error = OTBR_ERROR_REST);

    body = Json::Bytes2HexJsonString(id.mId, sizeof(id));
    aResponse.SetBody(body);

exit:
    if (error == OTBR_ERROR_NONE)
    {
        errorCode = GetHttpStatus(HttpStatusCode::kStatusOk);
        aResponse.SetResponsCode(errorCode);
    }
    else
    {
        ErrorHandler(aResponse, HttpStatusCode::kStatusInternalServerError);
    }
}

void Resource::BaId(const Request &aRequest, Response &aResponse) const
{
    std::string errorCode;

    if (aRequest.GetMethod() == HttpMethod::kGet)
    {
        GetDataBaId(aResponse);
    }
    else
    {
        ErrorHandler(aResponse, HttpStatusCode::kStatusMethodNotAllowed);
    }
}

void Resource::GetDataExtendedAddr(Response &aResponse) const
{
    const uint8_t *extAddress = reinterpret_cast<const uint8_t *>(otLinkGetExtendedAddress(mInstance));
    std::string    errorCode;
    std::string    body = Json::Bytes2HexJsonString(extAddress, OT_EXT_ADDRESS_SIZE);

    aResponse.SetBody(body);
    errorCode = GetHttpStatus(HttpStatusCode::kStatusOk);
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
        ErrorHandler(aResponse, HttpStatusCode::kStatusMethodNotAllowed);
    }
}

void Resource::GetDataState(Response &aResponse) const
{
    std::string  state;
    std::string  errorCode;
    otDeviceRole role;

    role  = otThreadGetDeviceRole(mInstance);
    state = Json::String2JsonString(GetDeviceRoleName(role));
    aResponse.SetBody(state);
    errorCode = GetHttpStatus(HttpStatusCode::kStatusOk);
    aResponse.SetResponsCode(errorCode);
}

void Resource::SetDataState(const Request &aRequest, Response &aResponse) const
{
    otbrError   error = OTBR_ERROR_NONE;
    std::string errorCode;
    std::string body;

    VerifyOrExit(Json::JsonString2String(aRequest.GetBody(), body), error = OTBR_ERROR_INVALID_ARGS);
    if (body == "enable")
    {
        if (!otIp6IsEnabled(mInstance))
        {
            VerifyOrExit(otIp6SetEnabled(mInstance, true) == OT_ERROR_NONE, error = OTBR_ERROR_INVALID_STATE);
        }
        VerifyOrExit(otThreadSetEnabled(mInstance, true) == OT_ERROR_NONE, error = OTBR_ERROR_INVALID_STATE);
    }
    else if (body == "disable")
    {
        VerifyOrExit(otThreadSetEnabled(mInstance, false) == OT_ERROR_NONE, error = OTBR_ERROR_INVALID_STATE);
        VerifyOrExit(otIp6SetEnabled(mInstance, false) == OT_ERROR_NONE, error = OTBR_ERROR_INVALID_STATE);
    }
    else
    {
        ExitNow(error = OTBR_ERROR_INVALID_ARGS);
    }

    errorCode = GetHttpStatus(HttpStatusCode::kStatusOk);
    aResponse.SetResponsCode(errorCode);

exit:
    if (error == OTBR_ERROR_INVALID_STATE)
    {
        ErrorHandler(aResponse, HttpStatusCode::kStatusConflict);
    }
    if (error == OTBR_ERROR_INVALID_ARGS)
    {
        ErrorHandler(aResponse, HttpStatusCode::kStatusBadRequest);
    }
    else if (error != OTBR_ERROR_NONE)
    {
        ErrorHandler(aResponse, HttpStatusCode::kStatusInternalServerError);
    }
}

void Resource::State(const Request &aRequest, Response &aResponse) const
{
    std::string errorCode;

    switch (aRequest.GetMethod())
    {
    case HttpMethod::kGet:
        GetDataState(aResponse);
        break;
    case HttpMethod::kPut:
        SetDataState(aRequest, aResponse);
        break;
    case HttpMethod::kOptions:
        errorCode = GetHttpStatus(HttpStatusCode::kStatusOk);
        aResponse.SetResponsCode(errorCode);
        aResponse.SetComplete();
        break;
    default:
        ErrorHandler(aResponse, HttpStatusCode::kStatusMethodNotAllowed);
        break;
    }
}

void Resource::GetDataNetworkName(Response &aResponse) const
{
    std::string networkName;
    std::string errorCode;

    networkName = otThreadGetNetworkName(mInstance);
    networkName = Json::String2JsonString(networkName);

    aResponse.SetBody(networkName);
    errorCode = GetHttpStatus(HttpStatusCode::kStatusOk);
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
        ErrorHandler(aResponse, HttpStatusCode::kStatusMethodNotAllowed);
    }
}

void Resource::GetDataLeaderData(Response &aResponse) const
{
    otbrError    error = OTBR_ERROR_NONE;
    otLeaderData leaderData;
    std::string  body;
    std::string  errorCode;

    VerifyOrExit(otThreadGetLeaderData(mInstance, &leaderData) == OT_ERROR_NONE, error = OTBR_ERROR_REST);

    body = Json::LeaderData2JsonString(leaderData);

    aResponse.SetBody(body);

exit:
    if (error == OTBR_ERROR_NONE)
    {
        errorCode = GetHttpStatus(HttpStatusCode::kStatusOk);
        aResponse.SetResponsCode(errorCode);
    }
    else
    {
        ErrorHandler(aResponse, HttpStatusCode::kStatusInternalServerError);
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
        ErrorHandler(aResponse, HttpStatusCode::kStatusMethodNotAllowed);
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
    errorCode = GetHttpStatus(HttpStatusCode::kStatusOk);
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
        ErrorHandler(aResponse, HttpStatusCode::kStatusMethodNotAllowed);
    }
}

void Resource::GetDataRloc16(Response &aResponse) const
{
    uint16_t    rloc16 = otThreadGetRloc16(mInstance);
    std::string body;
    std::string errorCode;

    body = Json::Number2JsonString(rloc16);

    aResponse.SetBody(body);
    errorCode = GetHttpStatus(HttpStatusCode::kStatusOk);
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
        ErrorHandler(aResponse, HttpStatusCode::kStatusMethodNotAllowed);
    }
}

void Resource::GetDataExtendedPanId(Response &aResponse) const
{
    const uint8_t *extPanId = reinterpret_cast<const uint8_t *>(otThreadGetExtendedPanId(mInstance));
    std::string    body     = Json::Bytes2HexJsonString(extPanId, OT_EXT_PAN_ID_SIZE);
    std::string    errorCode;

    aResponse.SetBody(body);
    errorCode = GetHttpStatus(HttpStatusCode::kStatusOk);
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
        ErrorHandler(aResponse, HttpStatusCode::kStatusMethodNotAllowed);
    }
}

void Resource::GetDataRloc(Response &aResponse) const
{
    otIp6Address rlocAddress = *otThreadGetRloc(mInstance);
    std::string  body;
    std::string  errorCode;

    body = Json::IpAddr2JsonString(rlocAddress);

    aResponse.SetBody(body);
    errorCode = GetHttpStatus(HttpStatusCode::kStatusOk);
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
        ErrorHandler(aResponse, HttpStatusCode::kStatusMethodNotAllowed);
    }
}

void Resource::GetDataset(DatasetType aDatasetType, const Request &aRequest, Response &aResponse) const
{
    otbrError                error = OTBR_ERROR_NONE;
    struct NodeInfo          node;
    std::string              body;
    std::string              errorCode;
    otOperationalDataset     dataset;
    otOperationalDatasetTlvs datasetTlvs;

    if (aRequest.GetHeaderValue(OT_REST_ACCEPT_HEADER) == OT_REST_CONTENT_TYPE_PLAIN)
    {
        if (aDatasetType == DatasetType::kActive)
        {
            VerifyOrExit(otDatasetGetActiveTlvs(mInstance, &datasetTlvs) == OT_ERROR_NONE,
                         error = OTBR_ERROR_NOT_FOUND);
        }
        else if (aDatasetType == DatasetType::kPending)
        {
            VerifyOrExit(otDatasetGetPendingTlvs(mInstance, &datasetTlvs) == OT_ERROR_NONE,
                         error = OTBR_ERROR_NOT_FOUND);
        }

        aResponse.SetContentType(OT_REST_CONTENT_TYPE_PLAIN);
        body = Utils::Bytes2Hex(datasetTlvs.mTlvs, datasetTlvs.mLength);
    }
    else
    {
        if (aDatasetType == DatasetType::kActive)
        {
            VerifyOrExit(otDatasetGetActive(mInstance, &dataset) == OT_ERROR_NONE, error = OTBR_ERROR_NOT_FOUND);
            body = Json::ActiveDataset2JsonString(dataset);
        }
        else if (aDatasetType == DatasetType::kPending)
        {
            VerifyOrExit(otDatasetGetPending(mInstance, &dataset) == OT_ERROR_NONE, error = OTBR_ERROR_NOT_FOUND);
            body = Json::PendingDataset2JsonString(dataset);
        }
    }

    aResponse.SetBody(body);

exit:
    if (error == OTBR_ERROR_NONE)
    {
        errorCode = GetHttpStatus(HttpStatusCode::kStatusOk);
        aResponse.SetResponsCode(errorCode);
    }
    else if (error == OTBR_ERROR_NOT_FOUND)
    {
        errorCode = GetHttpStatus(HttpStatusCode::kStatusNoContent);
        aResponse.SetResponsCode(errorCode);
    }
    else
    {
        ErrorHandler(aResponse, HttpStatusCode::kStatusInternalServerError);
    }
}

void Resource::SetDataset(DatasetType aDatasetType, const Request &aRequest, Response &aResponse) const
{
    otError                  errorOt = OT_ERROR_NONE;
    otbrError                error   = OTBR_ERROR_NONE;
    struct NodeInfo          node;
    std::string              body;
    std::string              errorCode = GetHttpStatus(HttpStatusCode::kStatusOk);
    otOperationalDataset     dataset   = {};
    otOperationalDatasetTlvs datasetTlvs;
    otOperationalDatasetTlvs datasetUpdateTlvs;
    int                      ret;
    bool                     isTlv;

    if (aDatasetType == DatasetType::kActive)
    {
        VerifyOrExit(otThreadGetDeviceRole(mInstance) == OT_DEVICE_ROLE_DISABLED, error = OTBR_ERROR_INVALID_STATE);
        errorOt = otDatasetGetActiveTlvs(mInstance, &datasetTlvs);
    }
    else if (aDatasetType == DatasetType::kPending)
    {
        errorOt = otDatasetGetPendingTlvs(mInstance, &datasetTlvs);
    }

    // Create a new operational dataset if it doesn't exist.
    if (errorOt == OT_ERROR_NOT_FOUND)
    {
        VerifyOrExit(otDatasetCreateNewNetwork(mInstance, &dataset) == OT_ERROR_NONE, error = OTBR_ERROR_REST);
        otDatasetConvertToTlvs(&dataset, &datasetTlvs);
        errorCode = GetHttpStatus(HttpStatusCode::kStatusCreated);
    }

    isTlv = aRequest.GetHeaderValue(OT_REST_CONTENT_TYPE_HEADER) == OT_REST_CONTENT_TYPE_PLAIN;

    if (isTlv)
    {
        ret = Json::Hex2BytesJsonString(aRequest.GetBody(), datasetUpdateTlvs.mTlvs, OT_OPERATIONAL_DATASET_MAX_LENGTH);
        VerifyOrExit(ret >= 0, error = OTBR_ERROR_INVALID_ARGS);
        datasetUpdateTlvs.mLength = ret;

        VerifyOrExit(otDatasetParseTlvs(&datasetUpdateTlvs, &dataset) == OT_ERROR_NONE, error = OTBR_ERROR_REST);
        VerifyOrExit(otDatasetUpdateTlvs(&dataset, &datasetTlvs) == OT_ERROR_NONE, error = OTBR_ERROR_REST);
    }
    else
    {
        if (aDatasetType == DatasetType::kActive)
        {
            VerifyOrExit(Json::JsonActiveDatasetString2Dataset(aRequest.GetBody(), dataset),
                         error = OTBR_ERROR_INVALID_ARGS);
        }
        else if (aDatasetType == DatasetType::kPending)
        {
            VerifyOrExit(Json::JsonPendingDatasetString2Dataset(aRequest.GetBody(), dataset),
                         error = OTBR_ERROR_INVALID_ARGS);
            VerifyOrExit(dataset.mComponents.mIsDelayPresent, error = OTBR_ERROR_INVALID_ARGS);
        }
        VerifyOrExit(otDatasetUpdateTlvs(&dataset, &datasetTlvs) == OT_ERROR_NONE, error = OTBR_ERROR_REST);
    }

    if (aDatasetType == DatasetType::kActive)
    {
        VerifyOrExit(otDatasetSetActiveTlvs(mInstance, &datasetTlvs) == OT_ERROR_NONE, error = OTBR_ERROR_REST);
    }
    else if (aDatasetType == DatasetType::kPending)
    {
        VerifyOrExit(otDatasetSetPendingTlvs(mInstance, &datasetTlvs) == OT_ERROR_NONE, error = OTBR_ERROR_REST);
    }

    aResponse.SetResponsCode(errorCode);

exit:
    if (error == OTBR_ERROR_INVALID_ARGS)
    {
        ErrorHandler(aResponse, HttpStatusCode::kStatusBadRequest);
    }
    else if (error == OTBR_ERROR_INVALID_STATE)
    {
        ErrorHandler(aResponse, HttpStatusCode::kStatusConflict);
    }
    else if (error != OTBR_ERROR_NONE)
    {
        ErrorHandler(aResponse, HttpStatusCode::kStatusInternalServerError);
    }
}

void Resource::Dataset(DatasetType aDatasetType, const Request &aRequest, Response &aResponse) const
{
    std::string errorCode;

    switch (aRequest.GetMethod())
    {
    case HttpMethod::kGet:
        GetDataset(aDatasetType, aRequest, aResponse);
        break;
    case HttpMethod::kPut:
        SetDataset(aDatasetType, aRequest, aResponse);
        break;
    case HttpMethod::kOptions:
        errorCode = GetHttpStatus(HttpStatusCode::kStatusOk);
        aResponse.SetResponsCode(errorCode);
        aResponse.SetComplete();
        break;
    default:
        ErrorHandler(aResponse, HttpStatusCode::kStatusMethodNotAllowed);
        break;
    }
}

void Resource::DatasetActive(const Request &aRequest, Response &aResponse) const
{
    Dataset(DatasetType::kActive, aRequest, aResponse);
}

void Resource::DatasetPending(const Request &aRequest, Response &aResponse) const
{
    Dataset(DatasetType::kPending, aRequest, aResponse);
}

void Resource::GetCommissionerState(Response &aResponse) const
{
    std::string         state;
    std::string         errorCode;
    otCommissionerState stateCode;

    stateCode = otCommissionerGetState(mInstance);
    state     = Json::String2JsonString(GetCommissionerStateName(stateCode));
    aResponse.SetBody(state);
    errorCode = GetHttpStatus(HttpStatusCode::kStatusOk);
    aResponse.SetResponsCode(errorCode);
}

void Resource::SetCommissionerState(const Request &aRequest, Response &aResponse) const
{
    otbrError   error = OTBR_ERROR_NONE;
    std::string errorCode;
    std::string body;

    VerifyOrExit(Json::JsonString2String(aRequest.GetBody(), body), error = OTBR_ERROR_INVALID_ARGS);
    if (body == "enable")
    {
        VerifyOrExit(otCommissionerGetState(mInstance) == OT_COMMISSIONER_STATE_DISABLED, error = OTBR_ERROR_NONE);
        VerifyOrExit(otCommissionerStart(mInstance, NULL, NULL, NULL) == OT_ERROR_NONE,
                     error = OTBR_ERROR_INVALID_STATE);
    }
    else if (body == "disable")
    {
        VerifyOrExit(otCommissionerGetState(mInstance) != OT_COMMISSIONER_STATE_DISABLED, error = OTBR_ERROR_NONE);
        VerifyOrExit(otCommissionerStop(mInstance) == OT_ERROR_NONE, error = OTBR_ERROR_INVALID_STATE);
    }
    else
    {
        ExitNow(error = OTBR_ERROR_INVALID_ARGS);
    }

exit:
    switch (error)
    {
    case OTBR_ERROR_NONE:
        errorCode = GetHttpStatus(HttpStatusCode::kStatusOk);
        aResponse.SetResponsCode(errorCode);
        break;
    case OTBR_ERROR_INVALID_STATE:
        ErrorHandler(aResponse, HttpStatusCode::kStatusConflict);
        break;
    case OTBR_ERROR_INVALID_ARGS:
        ErrorHandler(aResponse, HttpStatusCode::kStatusBadRequest);
        break;
    default:
        ErrorHandler(aResponse, HttpStatusCode::kStatusInternalServerError);
        break;
    }
}

void Resource::CommissionerState(const Request &aRequest, Response &aResponse) const
{
    std::string errorCode;

    switch (aRequest.GetMethod())
    {
    case HttpMethod::kGet:
        GetCommissionerState(aResponse);
        break;
    case HttpMethod::kPut:
        SetCommissionerState(aRequest, aResponse);
        break;
    case HttpMethod::kOptions:
        errorCode = GetHttpStatus(HttpStatusCode::kStatusOk);
        aResponse.SetResponsCode(errorCode);
        aResponse.SetComplete();
        break;
    default:
        ErrorHandler(aResponse, HttpStatusCode::kStatusMethodNotAllowed);
        break;
    }
}

void Resource::GetJoiners(Response &aResponse) const
{
    uint16_t                  iter = 0;
    otJoinerInfo              joinerInfo;
    std::vector<otJoinerInfo> joinerTable;
    std::string               joinerJson;
    std::string               errorCode;

    while (otCommissionerGetNextJoinerInfo(mInstance, &iter, &joinerInfo) == OT_ERROR_NONE)
    {
        joinerTable.push_back(joinerInfo);
    }

    joinerJson = Json::JoinerTable2JsonString(joinerTable);
    aResponse.SetBody(joinerJson);
    errorCode = GetHttpStatus(HttpStatusCode::kStatusOk);
    aResponse.SetResponsCode(errorCode);
}

void Resource::AddJoiner(const Request &aRequest, Response &aResponse) const
{
    otbrError           error   = OTBR_ERROR_NONE;
    otError             errorOt = OT_ERROR_NONE;
    std::string         errorCode;
    otJoinerInfo        joiner;
    const otExtAddress *addrPtr                         = nullptr;
    const uint8_t       emptyArray[OT_EXT_ADDRESS_SIZE] = {0};

    VerifyOrExit(otCommissionerGetState(mInstance) == OT_COMMISSIONER_STATE_ACTIVE, error = OTBR_ERROR_INVALID_STATE);

    VerifyOrExit(Json::JsonJoinerInfoString2JoinerInfo(aRequest.GetBody(), joiner), error = OTBR_ERROR_INVALID_ARGS);

    addrPtr = &joiner.mSharedId.mEui64;
    if (memcmp(&joiner.mSharedId.mEui64, emptyArray, OT_EXT_ADDRESS_SIZE) == 0)
    {
        addrPtr = nullptr;
    }

    if (joiner.mType == OT_JOINER_INFO_TYPE_DISCERNER)
    {
        errorOt = otCommissionerAddJoinerWithDiscerner(mInstance, &joiner.mSharedId.mDiscerner, joiner.mPskd.m8,
                                                       joiner.mExpirationTime);
    }
    else
    {
        errorOt = otCommissionerAddJoiner(mInstance, addrPtr, joiner.mPskd.m8, joiner.mExpirationTime);
    }
    VerifyOrExit(errorOt == OT_ERROR_NONE, error = OTBR_ERROR_OPENTHREAD);

exit:
    switch (error)
    {
    case OTBR_ERROR_NONE:
        errorCode = GetHttpStatus(HttpStatusCode::kStatusOk);
        aResponse.SetResponsCode(errorCode);
        break;
    case OTBR_ERROR_INVALID_STATE:
        ErrorHandler(aResponse, HttpStatusCode::kStatusConflict);
        break;
    case OTBR_ERROR_INVALID_ARGS:
        ErrorHandler(aResponse, HttpStatusCode::kStatusBadRequest);
        break;
    case OTBR_ERROR_OPENTHREAD:
        switch (errorOt)
        {
        case OT_ERROR_INVALID_ARGS:
            ErrorHandler(aResponse, HttpStatusCode::kStatusBadRequest);
            break;
        case OT_ERROR_NO_BUFS:
            ErrorHandler(aResponse, HttpStatusCode::kStatusInsufficientStorage);
            break;
        default:
            ErrorHandler(aResponse, HttpStatusCode::kStatusInternalServerError);
            break;
        }
        break;
    default:
        ErrorHandler(aResponse, HttpStatusCode::kStatusInternalServerError);
        break;
    }
}

void Resource::RemoveJoiner(const Request &aRequest, Response &aResponse) const
{
    otbrError         error = OTBR_ERROR_NONE;
    std::string       errorCode;
    otExtAddress      eui64;
    otExtAddress     *addrPtr   = nullptr;
    otJoinerDiscerner discerner = {
        .mValue  = 0,
        .mLength = 0,
    };
    std::string body;

    VerifyOrExit(otCommissionerGetState(mInstance) == OT_COMMISSIONER_STATE_ACTIVE, error = OTBR_ERROR_INVALID_STATE);

    VerifyOrExit(Json::JsonString2String(aRequest.GetBody(), body), error = OTBR_ERROR_INVALID_ARGS);
    if (body != "*")
    {
        error = Json::StringDiscerner2Discerner(const_cast<char *>(body.c_str()), discerner);
        if (error == OTBR_ERROR_NOT_FOUND)
        {
            error = OTBR_ERROR_NONE;
            VerifyOrExit(Json::Hex2BytesJsonString(body, eui64.m8, OT_EXT_ADDRESS_SIZE) == OT_EXT_ADDRESS_SIZE,
                         error = OTBR_ERROR_INVALID_ARGS);
            addrPtr = &eui64;
        }
        else if (error != OTBR_ERROR_NONE)
        {
            ExitNow(error = OTBR_ERROR_INVALID_ARGS);
        }
    }

    // These functions should only return OT_ERROR_NONE or OT_ERROR_NOT_FOUND both treated as successful
    if (discerner.mLength == 0)
    {
        (void)otCommissionerRemoveJoiner(mInstance, addrPtr);
    }
    else
    {
        (void)otCommissionerRemoveJoinerWithDiscerner(mInstance, &discerner);
    }

exit:
    switch (error)
    {
    case OTBR_ERROR_NONE:
        errorCode = GetHttpStatus(HttpStatusCode::kStatusOk);
        aResponse.SetResponsCode(errorCode);
        break;
    case OTBR_ERROR_INVALID_STATE:
        ErrorHandler(aResponse, HttpStatusCode::kStatusConflict);
        break;
    case OTBR_ERROR_INVALID_ARGS:
        ErrorHandler(aResponse, HttpStatusCode::kStatusBadRequest);
        break;
    default:
        ErrorHandler(aResponse, HttpStatusCode::kStatusInternalServerError);
        break;
    }
}

void Resource::CommissionerJoiner(const Request &aRequest, Response &aResponse) const
{
    std::string errorCode;

    switch (aRequest.GetMethod())
    {
    case HttpMethod::kGet:
        GetJoiners(aResponse);
        break;
    case HttpMethod::kPost:
        AddJoiner(aRequest, aResponse);
        break;
    case HttpMethod::kDelete:
        RemoveJoiner(aRequest, aResponse);
        break;

    case HttpMethod::kOptions:
        errorCode = GetHttpStatus(HttpStatusCode::kStatusOk);
        aResponse.SetResponsCode(errorCode);
        aResponse.SetComplete();
        break;
    default:
        ErrorHandler(aResponse, HttpStatusCode::kStatusMethodNotAllowed);
        break;
    }
}

void Resource::GetCoprocessorVersion(Response &aResponse) const
{
    std::string coprocessorVersion;
    std::string errorCode;

    coprocessorVersion = mHost->GetCoprocessorVersion();
    coprocessorVersion = Json::String2JsonString(coprocessorVersion);

    aResponse.SetBody(coprocessorVersion);
    errorCode = GetHttpStatus(HttpStatusCode::kStatusOk);
    aResponse.SetResponsCode(errorCode);
}

void Resource::CoprocessorVersion(const Request &aRequest, Response &aResponse) const
{
    std::string errorCode;

    if (aRequest.GetMethod() == HttpMethod::kGet)
    {
        GetCoprocessorVersion(aResponse);
    }
    else
    {
        ErrorHandler(aResponse, HttpStatusCode::kStatusMethodNotAllowed);
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
    otbrError error = OTBR_ERROR_NONE;
    OT_UNUSED_VARIABLE(aRequest);
    struct otIp6Address rloc16address = *otThreadGetRloc(mInstance);
    struct otIp6Address multicastAddress;

    VerifyOrExit(otThreadSendDiagnosticGet(mInstance, &rloc16address, kAllTlvTypes, sizeof(kAllTlvTypes),
                                           &Resource::DiagnosticResponseHandler,
                                           const_cast<Resource *>(this)) == OT_ERROR_NONE,
                 error = OTBR_ERROR_REST);
    VerifyOrExit(otIp6AddressFromString(kMulticastAddrAllRouters, &multicastAddress) == OT_ERROR_NONE,
                 error = OTBR_ERROR_REST);
    VerifyOrExit(otThreadSendDiagnosticGet(mInstance, &multicastAddress, kAllTlvTypes, sizeof(kAllTlvTypes),
                                           &Resource::DiagnosticResponseHandler,
                                           const_cast<Resource *>(this)) == OT_ERROR_NONE,
                 error = OTBR_ERROR_REST);

exit:

    if (error == OTBR_ERROR_NONE)
    {
        aResponse.SetStartTime(steady_clock::now());
        aResponse.SetCallback();
    }
    else
    {
        ErrorHandler(aResponse, HttpStatusCode::kStatusInternalServerError);
    }
}

void Resource::DiagnosticResponseHandler(otError              aError,
                                         otMessage           *aMessage,
                                         const otMessageInfo *aMessageInfo,
                                         void                *aContext)
{
    static_cast<Resource *>(aContext)->DiagnosticResponseHandler(aError, aMessage, aMessageInfo);
}

void Resource::DiagnosticResponseHandler(otError aError, const otMessage *aMessage, const otMessageInfo *aMessageInfo)
{
    std::vector<otNetworkDiagTlv> diagSet;
    otNetworkDiagTlv              diagTlv;
    otNetworkDiagIterator         iterator = OT_NETWORK_DIAGNOSTIC_ITERATOR_INIT;
    otError                       error;
    char                          rloc[7];
    std::string                   keyRloc = "0xffee";

    SuccessOrExit(aError);

    OTBR_UNUSED_VARIABLE(aMessageInfo);

    while ((error = otThreadGetNextDiagnosticTlv(aMessage, &iterator, &diagTlv)) == OT_ERROR_NONE)
    {
        if (diagTlv.mType == OT_NETWORK_DIAGNOSTIC_TLV_SHORT_ADDRESS)
        {
            snprintf(rloc, sizeof(rloc), "0x%04x", diagTlv.mData.mAddr16);
            keyRloc = Json::CString2JsonString(rloc);
        }
        diagSet.push_back(diagTlv);
    }
    UpdateDiag(keyRloc, diagSet);

exit:
    if (aError != OT_ERROR_NONE)
    {
        otbrLogWarning("Failed to get diagnostic data: %s", otThreadErrorToString(aError));
    }
}

} // namespace rest
} // namespace otbr
