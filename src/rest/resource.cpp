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

#include "openthread/mesh_diag.h"
#include "openthread/platform/toolchain.h"
#include "rest/json.hpp"
#include "rest/types.hpp"

#ifndef OTBR_LOG_TAG
#define OTBR_LOG_TAG "REST"
#endif

#include <inttypes.h>
#include <set>
#include <openthread/commissioner.h>
#include <openthread/srp_server.h>
#include "common/logging.hpp"
#include "common/task_runner.hpp"
#include "rest/network_diag_handler.hpp"
#include "rest/resource.hpp"
#include "utils/string_utils.hpp"

#include "rest/actions_list.hpp" // Actions Collection
#include "rest/commissioner_manager.hpp"
#include "rest/rest_devices_coll.hpp"     // Devices Collection
#include "rest/rest_diagnostics_coll.hpp" // Diagnostics Collection

#include <cJSON.h>

#define OT_PSKC_MAX_LENGTH 16
#define OT_EXTENDED_PANID_LENGTH 8

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

// API endpoint path definition
#define OT_REST_RESOURCE_PATH_API "/api"
#define OT_REST_RESOURCE_PATH_API_ACTIONS OT_REST_RESOURCE_PATH_API "/actions"
#define OT_REST_RESOURCE_PATH_API_DEVICES OT_REST_RESOURCE_PATH_API "/devices"
#define OT_REST_RESOURCE_PATH_API_DIAGNOSTICS OT_REST_RESOURCE_PATH_API "/diagnostics"
#define OT_REST_RESOURCE_PATH_API_NODE OT_REST_RESOURCE_PATH_API OT_REST_RESOURCE_PATH_NODE
#define OT_REST_RESOURCE_PATH_API_NETWORKS OT_REST_RESOURCE_PATH_API "/networks"

#define OT_REST_HTTP_STATUS_200 "200 OK"
#define OT_REST_HTTP_STATUS_201 "201 Created"
#define OT_REST_HTTP_STATUS_204 "204 No Content"
#define OT_REST_HTTP_STATUS_400 "400 Bad Request"
#define OT_REST_HTTP_STATUS_404 "404 Not Found"
#define OT_REST_HTTP_STATUS_405 "405 Method Not Allowed"
#define OT_REST_HTTP_STATUS_408 "408 Request Timeout"
#define OT_REST_HTTP_STATUS_409 "409 Conflict"
#define OT_REST_HTTP_STATUS_415 "415 Unsupported Media Type"
#define OT_REST_HTTP_STATUS_422 "422 Unprocessable Content"
#define OT_REST_HTTP_STATUS_500 "500 Internal Server Error"
#define OT_REST_HTTP_STATUS_503 "503 Service Unavailable"
#define OT_REST_HTTP_STATUS_507 "507 Insufficient Storage"

using std::chrono::duration_cast;
using std::chrono::microseconds;
using std::chrono::milliseconds;
using std::chrono::steady_clock;

using std::placeholders::_1;
using std::placeholders::_2;

namespace otbr {
namespace rest {

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
    case HttpStatusCode::kStatusUnsupportedMediaType:
        httpStatus = OT_REST_HTTP_STATUS_415;
        break;
    case HttpStatusCode::kStatusUnprocessable:
        httpStatus = OT_REST_HTTP_STATUS_422;
        break;
    case HttpStatusCode::kStatusInternalServerError:
        httpStatus = OT_REST_HTTP_STATUS_500;
        break;
    case HttpStatusCode::kStatusServiceUnavailable:
        httpStatus = OT_REST_HTTP_STATUS_503;
        break;
    case HttpStatusCode::kStatusInsufficientStorage:
        httpStatus = OT_REST_HTTP_STATUS_507;
    }

    return httpStatus;
}

// extract ItemId from request url
std::string getItemIdFromUrl(const Request &aRequest, std::string aCollectionName);

/**
 * Initialize the Resource class with a pointer to the ControllerOpenThread instance.
 */
Resource::Resource(RcpHost *aHost)
    : mInstance(nullptr)
    , mHost(aHost)
    , mServices()
{
    // Resource Handler
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

    // API Resource Handler
    mResourceMap.emplace(OT_REST_RESOURCE_PATH_API_ACTIONS, &Resource::ApiActionHandler);
    mResourceMap.emplace(OT_REST_RESOURCE_PATH_API_DEVICES, &Resource::ApiDeviceHandler);
    mResourceMap.emplace(OT_REST_RESOURCE_PATH_API_DIAGNOSTICS, &Resource::ApiDiagnosticHandler);

    // Resource callback handler
    mResourceCallbackMap.emplace(OT_REST_RESOURCE_PATH_API_DEVICES, &Resource::ApiDevicePostCallbackHandler);
}

void Resource::Init(void)
{
    mInstance = mHost->GetThreadHelper()->GetInstance();
    mServices.Init(mInstance);

    // add node to api/devices
    DeviceInfo          aDeviceInfo = {};
    const otExtAddress *thisextaddr = otLinkGetExtendedAddress(mInstance);
    std::string         thisextaddr_str;
    thisextaddr_str = otbr::Utils::Bytes2Hex(thisextaddr->m8, OT_EXT_ADDRESS_SIZE);
    thisextaddr_str = StringUtils::ToLowercase(thisextaddr_str);
    mServices.GetNetworkDiagHandler().SetDeviceItemAttributes(thisextaddr_str, aDeviceInfo);
}

void Resource::Process(void)
{
    mServices.Process();
}

std::string Resource::redirectToCollection(Request &aRequest)
{
    size_t  endpointSize;
    uint8_t apiPathLength = strlen("/api/");

    std::string url = aRequest.GetUrlPath();

    // redirect OT_REST_RESOURCE_PATH_NODE to /api/devices/{thisDeviceId}
    if ((url == std::string(OT_REST_RESOURCE_PATH_NODE)) || (url == std::string(OT_REST_RESOURCE_PATH_API_NODE)))
    {
        redirectNodeToDeviceItem(aRequest);
        url = aRequest.GetUrlPath();
    }

    VerifyOrExit(url.compare(0, apiPathLength, std::string("/api/")) == 0);
    // check url matches structure /api/{collection}/{itemId}
    endpointSize = url.find('/', apiPathLength);
    // redirect to /api/{collection}
    if (endpointSize != std::string::npos)
    {
        url = url.substr(0, endpointSize);
    }

exit:
    return url;
}

void Resource::Handle(Request &aRequest, Response &aResponse)
{
    std::string url = redirectToCollection(aRequest);

    auto it = mResourceMap.find(url);

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
    std::string url = redirectToCollection(aRequest);

    auto it = mResourceCallbackMap.find(url);

    if (it != mResourceCallbackMap.end())
    {
        ResourceCallbackHandler resourceHandler = it->second;
        (this->*resourceHandler)(aRequest, aResponse);
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

void Resource::redirectNodeToDeviceItem(Request &aRequest)
{
    uint64_t            keyExtaddr = 0;
    const otExtAddress *extAddr;
    char                buffer[18];
    std::string         url;

    extAddr = otLinkGetExtendedAddress(mInstance);

    for (int i = 0; i < OT_EXT_ADDRESS_SIZE; i++)
    {
        keyExtaddr = (keyExtaddr << 8) | extAddr->m8[i];
    }

    std::snprintf(buffer, sizeof(buffer), "/%016" PRIx64, keyExtaddr);

    url = OT_REST_RESOURCE_PATH_API_DEVICES + std::string(buffer);

    aRequest.SetUrlPath(url);
}

void Resource::NodeInfo(const Request &aRequest, Response &aResponse)
{
    std::string errorCode;

    switch (aRequest.GetMethod())
    {
    case HttpMethod::kGet:
        ApiDeviceGetHandler(aRequest, aResponse);
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

void Resource::BaId(const Request &aRequest, Response &aResponse)
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

void Resource::ExtendedAddr(const Request &aRequest, Response &aResponse)
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

void Resource::State(const Request &aRequest, Response &aResponse)
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

void Resource::NetworkName(const Request &aRequest, Response &aResponse)
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

void Resource::LeaderData(const Request &aRequest, Response &aResponse)
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

void Resource::NumOfRoute(const Request &aRequest, Response &aResponse)
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

void Resource::Rloc16(const Request &aRequest, Response &aResponse)
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

void Resource::ExtendedPanId(const Request &aRequest, Response &aResponse)
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

void Resource::Rloc(const Request &aRequest, Response &aResponse)
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

void Resource::Dataset(DatasetType aDatasetType, const Request &aRequest, Response &aResponse)
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

void Resource::DatasetActive(const Request &aRequest, Response &aResponse)
{
    Dataset(DatasetType::kActive, aRequest, aResponse);
}

void Resource::DatasetPending(const Request &aRequest, Response &aResponse)
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

void Resource::CommissionerState(const Request &aRequest, Response &aResponse)
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

void Resource::CommissionerJoiner(const Request &aRequest, Response &aResponse)
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

void Resource::CoprocessorVersion(const Request &aRequest, Response &aResponse)
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

void Resource::ApiActionHandler(const Request &aRequest, Response &aResponse)
{
    std::string errorCode;
    std::string methods = "OPTIONS, GET, POST, DELETE";

    switch (aRequest.GetMethod())
    {
    case HttpMethod::kPost:
        ApiActionPostHandler(aRequest, aResponse);
        break;
    case HttpMethod::kGet:
        ApiActionGetHandler(aRequest, aResponse);
        break;
    case HttpMethod::kDelete:
        ApiActionDeleteHandler(aRequest, aResponse);
        break;
    case HttpMethod::kOptions:
        errorCode = GetHttpStatus(HttpStatusCode::kStatusOk);
        aResponse.SetAllowMethods(methods);
        aResponse.SetResponsCode(errorCode);
        aResponse.SetComplete();
        break;
    default:
        aResponse.SetAllowMethods(methods);
        ErrorHandler(aResponse, HttpStatusCode::kStatusMethodNotAllowed);
        break;
    }
}

void Resource::ApiActionPostHandler(const Request &aRequest, Response &aResponse)
{
    ActionsList   &actions = mServices.GetActionsList();
    std::string    responseMessage;
    std::string    errorCode;
    HttpStatusCode statusCode = HttpStatusCode::kStatusOk;
    cJSON         *root       = nullptr;
    cJSON         *dataArray;
    cJSON         *resp_data;
    cJSON         *resp_obj = nullptr;
    cJSON         *datum;
    cJSON         *resp;
    const char    *resp_str;

    std::string aUuid(UUID_STR_LEN, '\0'); // An empty UUID string for obtaining the created actionId

    VerifyOrExit((aRequest.GetHeaderValue(OT_REST_CONTENT_TYPE_HEADER).compare(OT_REST_CONTENT_TYPE_JSONAPI) == 0),
                 statusCode = HttpStatusCode::kStatusUnsupportedMediaType);

    root = cJSON_Parse(aRequest.GetBody().c_str());
    VerifyOrExit(root != nullptr, statusCode = HttpStatusCode::kStatusBadRequest);

    // perform general validation before we attempt to
    // perform any task specific validation
    dataArray = cJSON_GetObjectItemCaseSensitive(root, "data");
    VerifyOrExit((dataArray != nullptr) && cJSON_IsArray(dataArray), statusCode = HttpStatusCode::kStatusUnprocessable);

    // validate the form and arguments of all tasks
    // before we attempt to perform processing on any of the tasks.
    for (int idx = 0; idx < cJSON_GetArraySize(dataArray); idx++)
    {
        // Require all items in the list to be valid Task items with all required attributes;
        // otherwise rejects whole list and returns 422 Unprocessable.
        // Unimplemented tasks counted as failed / invalid tasks
        VerifyOrExit(actions.ValidateRequest(cJSON_GetArrayItem(dataArray, idx)),
                     statusCode = HttpStatusCode::kStatusUnprocessable);
    }

    // Check queueing all tasks does not exceed the max number of tasks we can have queued
    // TODO
    // VerifyOrExit((TASK_QUEUE_MAX > cJSON_GetArraySize(dataArray)), statusCode = HttpStatusCode::kStatusConflict);
    // VerifyOrExit((TASK_QUEUE_MAX - task_queue_len + can_remove_task_max()) > cJSON_GetArraySize(dataArray),
    //             statusCode = HttpStatusCode::kStatusServiceUnavailable);

    // Queue the tasks and prepare response data
    resp_data = cJSON_CreateArray();
    for (int i = 0; i < cJSON_GetArraySize(dataArray); i++)
    {
        datum = cJSON_GetArrayItem(dataArray, i);
        if (actions.CreateAction(datum, aUuid) == OT_ERROR_NONE)
        {
            resp_obj = cJSON_CreateObject();
            if (resp_obj != nullptr)
            {
                cJSON_AddStringToObject(resp_obj, "id", aUuid.c_str());
                cJSON_AddStringToObject(resp_obj, "type", cJSON_GetObjectItem(datum, "type")->valuestring);

                cJSON_AddItemToObject(resp_obj, "attributes", actions.JsonifyAction(aUuid));
                cJSON_AddItemToArray(resp_data, resp_obj);
            }
        }
    }

    // prepare reponse object
    resp = cJSON_CreateObject();
    cJSON_AddItemToObject(resp, "data", resp_data);
    cJSON_AddItemToObject(resp, "meta", Json::CreateMetaCollection(0, 200, cJSON_GetArraySize(resp_data)));

    resp_str = cJSON_PrintUnformatted(resp);
    otbrLogDebug("%s:%d - %s - Sending (%d):\n%s", __FILE__, __LINE__, __func__, strlen(resp_str), resp_str);

    responseMessage = resp_str;
    aResponse.SetBody(responseMessage);
    aResponse.SetContentType(OT_REST_CONTENT_TYPE_JSONAPI);
    errorCode = GetHttpStatus(HttpStatusCode::kStatusOk);
    aResponse.SetResponsCode(errorCode);
    aResponse.SetComplete();

    free((void *)resp_str);
    cJSON_Delete(resp);

    // Clear the 'root' JSON object and release its memory (this should also delete 'data')
    cJSON_Delete(root);
    root = nullptr;

exit:
    if (statusCode != HttpStatusCode::kStatusOk)
    {
        if (root != nullptr)
        {
            cJSON_Delete(root);
        }
        otbrLogWarning("%s:%d Error (%d)", __FILE__, __LINE__, statusCode);
        ErrorHandler(aResponse, statusCode);
    }
}

/* generic ApiActionGetHandler */
void Resource::ApiActionGetHandler(const Request &aRequest, Response &aResponse)
{
    std::string    errorCode  = GetHttpStatus(HttpStatusCode::kStatusOk);
    HttpStatusCode statusCode = HttpStatusCode::kStatusOk;

    std::string                        resp_body;
    std::map<std::string, std::string> queries;
    std::string                        itemId;

    ActionsList &actions = mServices.GetActionsList();

    VerifyOrExit(((aRequest.GetHeaderValue(OT_REST_ACCEPT_HEADER).compare(OT_REST_CONTENT_TYPE_JSONAPI) == 0) ||
                  (aRequest.GetHeaderValue(OT_REST_ACCEPT_HEADER).compare(OT_REST_CONTENT_TYPE_JSON) == 0)),
                 statusCode = HttpStatusCode::kStatusUnsupportedMediaType);

    itemId = getItemIdFromUrl(aRequest, actions.GetCollectionName());

    if (!itemId.empty())
    {
        UUID uuid;
        VerifyOrExit(uuid.Parse(itemId), statusCode = HttpStatusCode::kStatusBadRequest);
    }

    if (aRequest.GetHeaderValue(OT_REST_ACCEPT_HEADER).compare(OT_REST_CONTENT_TYPE_JSONAPI) == 0)
    {
        aResponse.SetContentType(OT_REST_CONTENT_TYPE_JSONAPI);
        for (auto &it : actions.GetContainedTypes())
        {
            if (aRequest.HasQuery("fields[" + it + "]"))
            {
                queries[it] = aRequest.GetQueryParameter("fields[" + it + "]");
            }
            // TODO: if "fields" in query but fields do not exist and queries[] remains empty
            //       -> add "None" to queries to produce an empty response
        }
        if (!itemId.empty())
        {
            // return the item
            resp_body = actions.ToJsonApiItemId(itemId, queries);
            VerifyOrExit(!resp_body.empty(), statusCode = HttpStatusCode::kStatusResourceNotFound);
        }
        else
        {
            // return all items
            resp_body = actions.ToJsonApiColl(queries);
        }
    }
    else if (aRequest.GetHeaderValue(OT_REST_ACCEPT_HEADER).compare(OT_REST_CONTENT_TYPE_JSON) == 0)
    {
        aResponse.SetContentType(OT_REST_CONTENT_TYPE_JSON);
        if (!itemId.empty())
        {
            // return the item
            resp_body = actions.ToJsonStringItemId(itemId, queries);
            VerifyOrExit(!resp_body.empty(), statusCode = HttpStatusCode::kStatusResourceNotFound);
        }
        else
        {
            // return all items
            resp_body = actions.ToJsonString();
        }
    }

    aResponse.SetBody(resp_body);
    aResponse.SetStartTime(steady_clock::now());
    aResponse.SetResponsCode(errorCode);
    aResponse.SetComplete();

exit:
    if (statusCode != HttpStatusCode::kStatusOk)
    {
        ErrorHandler(aResponse, statusCode);
    }
}

void Resource::ApiActionDeleteHandler(const Request &aRequest, Response &aResponse)
{
    std::string errorCode;

    OTBR_UNUSED_VARIABLE(aRequest);

    mServices.GetActionsList().DeleteAllActions();

    errorCode = GetHttpStatus(HttpStatusCode::kStatusNoContent);
    aResponse.SetResponsCode(errorCode);
    aResponse.SetComplete();
}

void Resource::ApiDiagnosticGetHandler(const Request &aRequest, Response &aResponse)
{
    std::string    errorCode  = GetHttpStatus(HttpStatusCode::kStatusOk);
    HttpStatusCode statusCode = HttpStatusCode::kStatusOk;

    std::string                        resp_body;
    std::map<std::string, std::string> queries;
    std::string                        itemId;

    VerifyOrExit(((aRequest.GetHeaderValue(OT_REST_ACCEPT_HEADER).compare(OT_REST_CONTENT_TYPE_JSONAPI) == 0) ||
                  (aRequest.GetHeaderValue(OT_REST_ACCEPT_HEADER).compare(OT_REST_CONTENT_TYPE_JSON) == 0)),
                 statusCode = HttpStatusCode::kStatusUnsupportedMediaType);

    itemId = getItemIdFromUrl(aRequest, mServices.GetDiagnosticsCollection().GetCollectionName());

    if (aRequest.GetHeaderValue(OT_REST_ACCEPT_HEADER).compare(OT_REST_CONTENT_TYPE_JSONAPI) == 0)
    {
        aResponse.SetContentType(OT_REST_CONTENT_TYPE_JSONAPI);
        for (auto &it : mServices.GetDiagnosticsCollection().GetContainedTypes())
        {
            if (aRequest.HasQuery("fields[" + it + "]"))
            {
                queries[it] = aRequest.GetQueryParameter("fields[" + it + "]");
            }
        }
        if (!itemId.empty())
        {
            // return the item
            resp_body = mServices.GetDiagnosticsCollection().ToJsonApiItemId(itemId, queries);
            VerifyOrExit(!resp_body.empty(), statusCode = HttpStatusCode::kStatusResourceNotFound);
        }
        else
        {
            // return all items
            resp_body = mServices.GetDiagnosticsCollection().ToJsonApiColl(queries);
        }
    }
    else if (aRequest.GetHeaderValue(OT_REST_ACCEPT_HEADER).compare(OT_REST_CONTENT_TYPE_JSON) == 0)
    {
        aResponse.SetContentType(OT_REST_CONTENT_TYPE_JSON);
        if (!itemId.empty())
        {
            // return the item
            resp_body = mServices.GetDiagnosticsCollection().ToJsonStringItemId(itemId, queries);
            VerifyOrExit(!resp_body.empty(), statusCode = HttpStatusCode::kStatusResourceNotFound);
        }
        else
        {
            // return all items
            resp_body = mServices.GetDiagnosticsCollection().ToJsonString();
        }
    }

    aResponse.SetBody(resp_body);
    aResponse.SetStartTime(steady_clock::now());
    aResponse.SetResponsCode(errorCode);
    aResponse.SetComplete();

exit:
    if (statusCode != HttpStatusCode::kStatusOk)
    {
        ErrorHandler(aResponse, statusCode);
    }
}

void Resource::ApiDiagnosticDeleteHandler(const Request &aRequest, Response &aResponse)
{
    std::string errorCode;

    OTBR_UNUSED_VARIABLE(aRequest);

    mServices.GetNetworkDiagHandler().Clear();
    mServices.GetDiagnosticsCollection().Clear();

    errorCode = GetHttpStatus(HttpStatusCode::kStatusNoContent);
    aResponse.SetResponsCode(errorCode);
    aResponse.SetComplete();
}

void Resource::ApiDiagnosticHandler(const Request &aRequest, Response &aResponse)
{
    std::string errorCode;
    std::string methods = "OPTIONS, GET, DELETE";

    switch (aRequest.GetMethod())
    {
    case HttpMethod::kGet:
        ApiDiagnosticGetHandler(aRequest, aResponse);
        break;
    case HttpMethod::kDelete:
        ApiDiagnosticDeleteHandler(aRequest, aResponse);
        break;
    case HttpMethod::kOptions:
        errorCode = GetHttpStatus(HttpStatusCode::kStatusNoContent);
        aResponse.SetAllowMethods(methods);
        aResponse.SetResponsCode(errorCode);
        aResponse.SetComplete();
        break;
    case HttpMethod::kPost:
    default:
        aResponse.SetAllowMethods(methods);
        ErrorHandler(aResponse, HttpStatusCode::kStatusMethodNotAllowed);
        break;
    }
}

void Resource::ApiDeviceHandler(const Request &aRequest, Response &aResponse)
{
    std::string errorCode;

    switch (aRequest.GetMethod())
    {
    case HttpMethod::kDelete:
        ApiDeviceDeleteHandler(aRequest, aResponse);
        break;
    case HttpMethod::kGet:
        ApiDeviceGetHandler(aRequest, aResponse);
        break;
    case HttpMethod::kPost:
        ApiDevicePostHandler(aRequest, aResponse);
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

void Resource::ApiDeviceDeleteHandler(const Request &aRequest, Response &aResponse)
{
    std::string errorCode;

    OTBR_UNUSED_VARIABLE(aRequest);

    mServices.GetDevicesCollection().Clear();

    errorCode = GetHttpStatus(HttpStatusCode::kStatusNoContent);
    aResponse.SetResponsCode(errorCode);
    aResponse.SetComplete();
}

std::string getItemIdFromUrl(const Request &aRequest, std::string aCollectionName)
{
    std::string itemId = "";
    std::string url    = aRequest.GetUrlPath();
    uint8_t     basePathLength =
        strlen(OT_REST_RESOURCE_PATH_API) + aCollectionName.length() + 2; // +2 for '/' before and after aCollectionName
    size_t idSize = url.find('/', basePathLength);

    VerifyOrExit(url.size() >= basePathLength);

    if (idSize != std::string::npos)
    {
        idSize = idSize - basePathLength;
    }

    itemId = url.substr(basePathLength, idSize);

    if (!itemId.empty())
    {
        otbrLogWarning("%s:%d get ItemId %s/%s", __FILE__, __LINE__, aCollectionName.c_str(), itemId.c_str());
    }

exit:
    return itemId;
}

void Resource::ApiDeviceGetHandler(const Request &aRequest, Response &aResponse)
{
    std::string    errorCode  = GetHttpStatus(HttpStatusCode::kStatusOk);
    HttpStatusCode statusCode = HttpStatusCode::kStatusOk;

    std::string                        resp_body;
    std::map<std::string, std::string> queries;

    std::string itemId = getItemIdFromUrl(aRequest, mServices.GetDevicesCollection().GetCollectionName());
    VerifyOrExit(((aRequest.GetHeaderValue(OT_REST_ACCEPT_HEADER).compare(OT_REST_CONTENT_TYPE_JSONAPI) == 0) ||
                  (aRequest.GetHeaderValue(OT_REST_ACCEPT_HEADER).compare(OT_REST_CONTENT_TYPE_JSON) == 0)),
                 statusCode = HttpStatusCode::kStatusUnsupportedMediaType);

    if (aRequest.GetHeaderValue(OT_REST_ACCEPT_HEADER).compare(OT_REST_CONTENT_TYPE_JSONAPI) == 0)
    {
        aResponse.SetContentType(OT_REST_CONTENT_TYPE_JSONAPI);
        for (auto &it : mServices.GetDevicesCollection().GetContainedTypes())
        {
            if (aRequest.HasQuery("fields[" + it + "]"))
            {
                queries[it] = aRequest.GetQueryParameter("fields[" + it + "]");
            }
        }
        if (!itemId.empty())
        {
            // return the item
            resp_body = mServices.GetDevicesCollection().ToJsonApiItemId(itemId, queries);
            VerifyOrExit(!resp_body.empty(), statusCode = HttpStatusCode::kStatusResourceNotFound);
        }
        else
        {
            // return all items
            resp_body = mServices.GetDevicesCollection().ToJsonApiColl(queries);
        }
    }
    else if (aRequest.GetHeaderValue(OT_REST_ACCEPT_HEADER).compare(OT_REST_CONTENT_TYPE_JSON) == 0)
    {
        aResponse.SetContentType(OT_REST_CONTENT_TYPE_JSON);
        for (auto &it : mServices.GetDevicesCollection().GetContainedTypes())
        {
            if (aRequest.HasQuery("fields[" + it + "]"))
            {
                queries[it] = aRequest.GetQueryParameter("fields[" + it + "]");
            }
        }
        if (!itemId.empty())
        {
            // return the item
            resp_body = mServices.GetDevicesCollection().ToJsonStringItemId(itemId, queries);
            VerifyOrExit(!resp_body.empty(), statusCode = HttpStatusCode::kStatusResourceNotFound);
        }
        else
        {
            // return all items
            resp_body = mServices.GetDevicesCollection().ToJsonString();
        }
    }

    aResponse.SetBody(resp_body);
    aResponse.SetStartTime(steady_clock::now());
    aResponse.SetResponsCode(errorCode);
    aResponse.SetComplete();

exit:
    if (statusCode != HttpStatusCode::kStatusOk)
    {
        // otbrLogWarning("%s:%d REST query error %d", __FILE__, __LINE__, error);
        ErrorHandler(aResponse, statusCode);
    }
}

// Discover device in the network and update the device collection.
void Resource::ApiDevicePostHandler(const Request &aRequest, Response &aResponse)
{
    otError     error = OT_ERROR_NONE;
    std::string resp_body;
    std::string errorCode;

    OT_UNUSED_VARIABLE(aRequest);

    aResponse.SetStartTime(steady_clock::now());

    // get handler for network diagnostics
    error = mServices.GetNetworkDiagHandler().HandleNetworkDiscoveryRequest(NETWORKDIAG_REQ_TIMEOUT, NETWORKDIAG_MAXAGE,
                                                                            NETWORKDIAG_REQ_MAX_RETRIES);

    if (error == OT_ERROR_NONE)
    {
        aResponse.SetCallback();
    }
    else if (error == OT_ERROR_INVALID_STATE)
    {
        otbrLogWarning("%s:%d otbr error %s", __FILE__, __LINE__, otThreadErrorToString(error));
        ErrorHandler(aResponse, HttpStatusCode::kStatusServiceUnavailable);
    }
    else
    {
        otbrLogWarning("%s:%d otbr error %s", __FILE__, __LINE__, otThreadErrorToString(error));
        ErrorHandler(aResponse, HttpStatusCode::kStatusInternalServerError);
    }
}

void Resource::ApiDevicePostCallbackHandler(const Request &aRequest, Response &aResponse)
{
    OT_UNUSED_VARIABLE(aRequest);
    otbrError                          error = OTBR_ERROR_NONE;
    std::string                        resp_body;
    std::string                        errorCode;
    std::map<std::string, std::string> queries; // empty query

    VerifyOrExit((error = mServices.GetNetworkDiagHandler().Process()) == OTBR_ERROR_NONE);

exit:
    if (error == OTBR_ERROR_NONE)
    {
        ApiDeviceGetHandler(aRequest, aResponse);
    }
    else if (error == OTBR_ERROR_ABORTED)
    {
        aResponse.SetContentType(OT_REST_CONTENT_TYPE_JSONAPI);

        // return all items collected until timeout
        resp_body = mServices.GetDevicesCollection().ToJsonApiColl(queries);

        aResponse.SetBody(resp_body);
        errorCode = GetHttpStatus(HttpStatusCode::kStatusRequestTimeout);
        aResponse.SetResponsCode(errorCode);
        aResponse.SetComplete();
    }
    else if (error != OTBR_ERROR_ERRNO)
    {
        otbrLogWarning("%s:%d otbr error %s", __FILE__, __LINE__, otbrErrorString(error));
        ErrorHandler(aResponse, HttpStatusCode::kStatusInternalServerError);
    }
}

} // namespace rest
} // namespace otbr
