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

#include <set>
#include <openthread/srp_server.h>
#include "common/logging.hpp"
#include "common/task_runner.hpp"
//#include "rest/extensions/rest_task_network_diagnostic.hpp"
#include "rest/network_diag_handler.hpp"
#include "rest/resource.hpp"
#include "utils/string_utils.hpp"

extern "C" {
#include <cJSON.h>
extern task_node_t *task_queue;
extern uint8_t      task_queue_len;
}

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
#define OT_REST_HTTP_STATUS_500 "500 Internal Server Error"
#define OT_REST_HTTP_STATUS_503 "503 Service Unavailable"

using std::chrono::duration_cast;
using std::chrono::microseconds;
using std::chrono::milliseconds;
using std::chrono::steady_clock;

using std::placeholders::_1;
using std::placeholders::_2;

namespace otbr {
namespace rest {

// Default TlvTypes for Diagnostic information
static const uint8_t kAllTlvTypes[] = {
    OT_NETWORK_DIAGNOSTIC_TLV_EXT_ADDRESS,          ///< MAC Extended Address TLV
    OT_NETWORK_DIAGNOSTIC_TLV_SHORT_ADDRESS,        ///< Address16 TLV
    OT_NETWORK_DIAGNOSTIC_TLV_MODE,                 ///< Mode TLV
    OT_NETWORK_DIAGNOSTIC_TLV_TIMEOUT,              ///< Timeout TLV (max polling time period for SEDs)
    OT_NETWORK_DIAGNOSTIC_TLV_CONNECTIVITY,         ///< Connectivity TLV
    OT_NETWORK_DIAGNOSTIC_TLV_ROUTE,                ///< Route64 TLV
    OT_NETWORK_DIAGNOSTIC_TLV_LEADER_DATA,          ///< Leader Data TLV
    OT_NETWORK_DIAGNOSTIC_TLV_NETWORK_DATA,         ///< Network Data TLV
    OT_NETWORK_DIAGNOSTIC_TLV_IP6_ADDR_LIST,        ///< IPv6 Address List TLV
    OT_NETWORK_DIAGNOSTIC_TLV_MAC_COUNTERS,         ///< MAC Counters TLV
    OT_NETWORK_DIAGNOSTIC_TLV_BATTERY_LEVEL,        ///< Battery Level TLV
    OT_NETWORK_DIAGNOSTIC_TLV_SUPPLY_VOLTAGE,       ///< Supply Voltage TLV
    OT_NETWORK_DIAGNOSTIC_TLV_CHILD_TABLE,          ///< Child Table TLV
    OT_NETWORK_DIAGNOSTIC_TLV_CHANNEL_PAGES,        ///< Channel Pages TLV
    OT_NETWORK_DIAGNOSTIC_TLV_MAX_CHILD_TIMEOUT,    ///< Max Child Timeout TLV
    OT_NETWORK_DIAGNOSTIC_TLV_EUI64,                ///< EUI64 TLV
    OT_NETWORK_DIAGNOSTIC_TLV_VERSION,              ///< Thread Version TLV
    OT_NETWORK_DIAGNOSTIC_TLV_VENDOR_NAME,          ///< Vendor Name TLV
    OT_NETWORK_DIAGNOSTIC_TLV_VENDOR_MODEL,         ///< Vendor Model TLV
    OT_NETWORK_DIAGNOSTIC_TLV_VENDOR_SW_VERSION,    ///< Vendor SW Version TLV
    OT_NETWORK_DIAGNOSTIC_TLV_THREAD_STACK_VERSION, ///< Thread Stack Version TLV (codebase/commit version)
    OT_NETWORK_DIAGNOSTIC_TLV_CHILD,                ///< Child TLV
    OT_NETWORK_DIAGNOSTIC_TLV_CHILD_IP6_ADDR_LIST,  ///< Child IPv6 Address List TLV
    OT_NETWORK_DIAGNOSTIC_TLV_ROUTER_NEIGHBOR,      ///< Router Neighbor TLV
    OT_NETWORK_DIAGNOSTIC_TLV_MLE_COUNTERS          ///< MLE Counters TLV
};

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
    case HttpStatusCode::kStatusInternalServerError:
        httpStatus = OT_REST_HTTP_STATUS_500;
        break;
    case HttpStatusCode::kStatusServiceUnavailable:
        httpStatus = OT_REST_HTTP_STATUS_503;
        break;
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
    // prepare to handle commissioner state changes
    mHost->AddThreadStateChangedCallback((Ncp::RcpHost::ThreadStateChangedCallback)HandleThreadStateChanges);
    // start task runner for api/actions
    ApiActionRepeatedTaskRunner(2000);
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

// calls rest_task_queue_handler after delay_ms
void Resource::ApiActionRepeatedTaskRunner(uint16_t delay_ms)
{
    // TODO: post repeatedly
    mHost->PostTimerTask(milliseconds(delay_ms), rest_task_queue_handle);
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

    std::sprintf(buffer, "/%016lx", keyExtaddr);

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

void Resource::HandleThreadStateChanges(otChangedFlags aFlags)
{
    if (aFlags & OT_CHANGED_COMMISSIONER_STATE)
    {
        otbrLogDebug("%s:%d - %s - commissioner state change.", __FILE__, __LINE__, __func__);
        rest_task_queue_handle();
    }
}

void Resource::ApiActionPostHandler(const Request &aRequest, Response &aResponse)
{
    std::string    responseMessage;
    std::string    errorCode;
    HttpStatusCode statusCode = HttpStatusCode::kStatusOk;
    cJSON         *root;
    cJSON         *dataArray;
    cJSON         *resp_data;
    cJSON         *datum;
    uuid_t         task_id;
    task_node_t   *task_node;
    cJSON         *resp;
    const char    *resp_str;

    VerifyOrExit((aRequest.GetHeaderValue(OT_REST_CONTENT_TYPE_HEADER).compare(OT_REST_CONTENT_TYPE_JSONAPI) == 0),
                 statusCode = HttpStatusCode::kStatusUnsupportedMediaType);

    root = cJSON_Parse(aRequest.GetBody().c_str());
    VerifyOrExit(root != NULL, statusCode = HttpStatusCode::kStatusBadRequest);

    // perform general validation before we attempt to
    // perform any task specific validation
    dataArray = cJSON_GetObjectItemCaseSensitive(root, "data");
    VerifyOrExit((dataArray != NULL) && cJSON_IsArray(dataArray), statusCode = HttpStatusCode::kStatusConflict);

    // validate the form and arguments of all tasks
    // before we attempt to perform processing on any of the tasks.
    for (int idx = 0; idx < cJSON_GetArraySize(dataArray); idx++)
    {
        // Require all items in the list to be valid Task items with all required attributes;
        // otherwise rejects whole list and returns 409 Conflict.
        // Unimplemented tasks counted as failed / invalid tasks
        VerifyOrExit(ACTIONS_TASK_VALID == validate_task(cJSON_GetArrayItem(dataArray, idx)),
                     statusCode = HttpStatusCode::kStatusConflict);
    }

    // Check queueing all tasks does not exceed the max number of tasks we can have queued
    VerifyOrExit((TASK_QUEUE_MAX - task_queue_len + can_remove_task_max()) > cJSON_GetArraySize(dataArray),
                 statusCode = HttpStatusCode::kStatusConflict);

    // Queue the tasks and prepare response data
    resp_data = cJSON_CreateArray();
    for (int i = 0; i < cJSON_GetArraySize(dataArray); i++)
    {
        datum = cJSON_GetArrayItem(dataArray, i);
        if (queue_task(datum, &task_id))
        {
            task_node = task_node_find_by_id(task_id);
            cJSON_AddItemToArray(resp_data, task_to_json(task_node));
            // attempt first process of task
            rest_task_queue_handle();
            // and another attempt after 2s
            ApiActionRepeatedTaskRunner(2000);
        }
    }

    // prepare reponse object
    resp = cJSON_CreateObject();
    cJSON_AddItemToObject(resp, "data", resp_data);
    cJSON_AddItemToObject(resp, "meta", jsonCreateTaskMetaCollection(0, TASK_QUEUE_MAX, cJSON_GetArraySize(resp_data)));

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
    root = NULL;

exit:
    if (statusCode != HttpStatusCode::kStatusOk)
    {
        if (root != NULL)
        {
            cJSON_Delete(root);
        }
        otbrLogWarning("Error (%d)", statusCode);
        ErrorHandler(aResponse, statusCode);
    }
}

void Resource::ApiActionGetHandler(const Request &aRequest, Response &aResponse)
{
    std::string    resp_body;
    std::string    errorCode;
    HttpStatusCode statusCode = HttpStatusCode::kStatusOk;
    task_node_t   *task_node;
    cJSON         *resp;
    cJSON         *resp_data;

    std::string itemId = getItemIdFromUrl(aRequest, "actions");

    VerifyOrExit(aRequest.GetHeaderValue(OT_REST_ACCEPT_HEADER).compare(OT_REST_CONTENT_TYPE_JSONAPI) == 0,
                 statusCode = HttpStatusCode::kStatusUnsupportedMediaType);

    // update the task status in the queue
    rest_task_queue_handle();
    // and another attempt after 2s
    ApiActionRepeatedTaskRunner(2000);

    if (aRequest.GetHeaderValue(OT_REST_ACCEPT_HEADER).compare(OT_REST_CONTENT_TYPE_JSONAPI) == 0)
    {
        aResponse.SetContentType(OT_REST_CONTENT_TYPE_JSONAPI);

        if (!itemId.empty())
        {
            // return the item
            task_node = task_queue;
            while (std::string(task_node->id_str) != itemId)
            {
                VerifyOrExit(task_node->next != nullptr, statusCode = HttpStatusCode::kStatusResourceNotFound);

                task_node = task_node->next;
            }
            evaluate_task(task_node);

            resp = cJSON_CreateObject();
            cJSON_AddItemToObject(resp, "data", task_to_json(task_node));
            resp_body = std::string(cJSON_PrintUnformatted(resp));
        }
        else
        {
            // return all items
            task_node = task_queue;
            resp      = cJSON_CreateObject();

            resp_data = cJSON_CreateArray();
            while (task_node != NULL)
            {
                evaluate_task(task_node);
                cJSON_AddItemToObject(resp_data, "data", task_to_json(task_node));
                task_node = task_node->next;
            }

            cJSON_AddItemToObject(resp, "data", resp_data);
            cJSON_AddItemToObject(resp, "meta",
                                  jsonCreateTaskMetaCollection(0, TASK_QUEUE_MAX, cJSON_GetArraySize(resp_data)));

            resp_body = std::string(cJSON_PrintUnformatted(resp));
        }
    }

    aResponse.SetBody(resp_body);
    cJSON_Delete(resp);

    errorCode = GetHttpStatus(statusCode);
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

    remove_all_task();
    rest_task_queue_handle();

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

    itemId = getItemIdFromUrl(aRequest, otbr::rest::gDiagnosticsCollection.getCollectionName());

    if (aRequest.GetHeaderValue(OT_REST_ACCEPT_HEADER).compare(OT_REST_CONTENT_TYPE_JSONAPI) == 0)
    {
        aResponse.SetContentType(OT_REST_CONTENT_TYPE_JSONAPI);
        for (auto &it : otbr::rest::gDiagnosticsCollection.getContainedTypes())
        {
            if (aRequest.HasQuery("fields[" + it + "]"))
            {
                queries[it] = aRequest.GetQueryParameter("fields[" + it + "]");
            }
        }
        if (!itemId.empty())
        {
            // return the item
            resp_body = otbr::rest::gDiagnosticsCollection.toJsonApiItemId(itemId, queries);
            VerifyOrExit(!resp_body.empty(), statusCode = HttpStatusCode::kStatusResourceNotFound);
        }
        else
        {
            // return all items
            resp_body = otbr::rest::gDiagnosticsCollection.toJsonApiColl(queries);
        }
    }
    else if (aRequest.GetHeaderValue(OT_REST_ACCEPT_HEADER).compare(OT_REST_CONTENT_TYPE_JSON) == 0)
    {
        aResponse.SetContentType(OT_REST_CONTENT_TYPE_JSON);
        if (!itemId.empty())
        {
            // return the item
            resp_body = otbr::rest::gDiagnosticsCollection.toJsonStringItemId(itemId, queries);
            VerifyOrExit(!resp_body.empty(), statusCode = HttpStatusCode::kStatusResourceNotFound);
        }
        else
        {
            // return all items
            resp_body = otbr::rest::gDiagnosticsCollection.toJsonString();
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

    // get handler for network diagnostics
    NetworkDiagHandler &netDiagHandler = NetworkDiagHandler::getInstance(mInstance);
    netDiagHandler.clear();

    otbr::rest::gDiagnosticsCollection.clear();

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

    gDevicesCollection.clear();

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

    std::string itemId = getItemIdFromUrl(aRequest, otbr::rest::gDevicesCollection.getCollectionName());

    VerifyOrExit(((aRequest.GetHeaderValue(OT_REST_ACCEPT_HEADER).compare(OT_REST_CONTENT_TYPE_JSONAPI) == 0) ||
                  (aRequest.GetHeaderValue(OT_REST_ACCEPT_HEADER).compare(OT_REST_CONTENT_TYPE_JSON) == 0)),
                 statusCode = HttpStatusCode::kStatusUnsupportedMediaType);

    if (aRequest.GetHeaderValue(OT_REST_ACCEPT_HEADER).compare(OT_REST_CONTENT_TYPE_JSONAPI) == 0)
    {
        aResponse.SetContentType(OT_REST_CONTENT_TYPE_JSONAPI);
        for (auto &it : otbr::rest::gDevicesCollection.getContainedTypes())
        {
            if (aRequest.HasQuery("fields[" + it + "]"))
            {
                queries[it] = aRequest.GetQueryParameter("fields[" + it + "]");
            }
        }
        if (!itemId.empty())
        {
            // return the item
            resp_body = otbr::rest::gDevicesCollection.toJsonApiItemId(itemId, queries);
            VerifyOrExit(!resp_body.empty(), statusCode = HttpStatusCode::kStatusResourceNotFound);
        }
        else
        {
            // return all items
            resp_body = otbr::rest::gDevicesCollection.toJsonApiColl(queries);
        }
    }
    else if (aRequest.GetHeaderValue(OT_REST_ACCEPT_HEADER).compare(OT_REST_CONTENT_TYPE_JSON) == 0)
    {
        aResponse.SetContentType(OT_REST_CONTENT_TYPE_JSON);
        for (auto &it : otbr::rest::gDevicesCollection.getContainedTypes())
        {
            if (aRequest.HasQuery("fields[" + it + "]"))
            {
                queries[it] = aRequest.GetQueryParameter("fields[" + it + "]");
            }
        }
        if (!itemId.empty())
        {
            // return the item
            resp_body = otbr::rest::gDevicesCollection.toJsonStringItemId(itemId, queries);
            VerifyOrExit(!resp_body.empty(), statusCode = HttpStatusCode::kStatusResourceNotFound);
        }
        else
        {
            // return all items
            resp_body = otbr::rest::gDevicesCollection.toJsonString();
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
    otbrError   error = OTBR_ERROR_NONE;
    std::string resp_body;
    std::string errorCode;

    OT_UNUSED_VARIABLE(aRequest);

    aResponse.SetStartTime(steady_clock::now());

    // get handler for network diagnostics
    NetworkDiagHandler &netDiagHandler = NetworkDiagHandler::getInstance(mInstance);
    VerifyOrExit((error = netDiagHandler.configRequest(10000, 30000, 1, nullptr)) == OTBR_ERROR_NONE);
    VerifyOrExit((error = netDiagHandler.handleNetworkDiscoveryRequest("", gDevicesCollection.getCollectionName())) ==
                 OTBR_ERROR_NONE);

exit:
    if (error == OTBR_ERROR_NONE)
    {
        aResponse.SetCallback();
    }
    else if (error == OTBR_ERROR_INVALID_STATE)
    {
        otbrLogWarning("%s:%d otbr error %s", __FILE__, __LINE__, otbrErrorString(error));
        ErrorHandler(aResponse, HttpStatusCode::kStatusServiceUnavailable);
    }
    else
    {
        otbrLogWarning("%s:%d otbr error %s", __FILE__, __LINE__, otbrErrorString(error));
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

    // get handler for network diagnostics
    NetworkDiagHandler &netDiagHandler = NetworkDiagHandler::getInstance(mInstance);

    VerifyOrExit((error = netDiagHandler.continueHandleRequest()) == OTBR_ERROR_NONE);

exit:
    if (error == OTBR_ERROR_NONE)
    {
        ApiDeviceGetHandler(aRequest, aResponse);
    }
    else if (error == OTBR_ERROR_ABORTED)
    {
        aResponse.SetContentType(OT_REST_CONTENT_TYPE_JSONAPI);

        // return all items collected until timeout
        resp_body = otbr::rest::gDevicesCollection.toJsonApiColl(queries);

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
