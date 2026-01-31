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

#include "common/time.hpp"
#define OTBR_LOG_TAG "REST"

#include "rest/rest_web_server.hpp"

#include <chrono>

#include <arpa/inet.h>
#include <fcntl.h>
#include <httplib.h>

#include <openthread/commissioner.h>

#include "common/api_strings.hpp"
#include "rest/json.hpp"

#include "rest/actions_list.hpp" // Actions Collection
#include "rest/commissioner_manager.hpp"
#include "rest/network_diag_handler.hpp"
#include "rest/rest_devices_coll.hpp"     // Devices Collection
#include "rest/rest_diagnostics_coll.hpp" // Diagnostics Collection
#include "rest/services.hpp"
#include "utils/string_utils.hpp"

#include <cJSON.h>

#ifndef OTBR_REST_ACCESS_CONTROL_ALLOW_ORIGIN
#define OTBR_REST_ACCESS_CONTROL_ALLOW_ORIGIN "*"
#endif

#ifndef OTBR_REST_ACCESS_CONTROL_ALLOW_HEADERS
#define OTBR_REST_ACCESS_CONTROL_ALLOW_HEADERS                                        \
    "Origin, Accept, X-Requested-With, Content-Type, Access-Control-Request-Method, " \
    "Access-Control-Request-Headers"
#endif

#ifndef OTBR_REST_ACCESS_CONTROL_ALLOW_METHODS
#define OTBR_REST_ACCESS_CONTROL_ALLOW_METHODS "DELETE, GET, OPTIONS, POST"
#endif

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

// API collection id definitions
#define DEVICEID_REGEX "([0-9a-fA-F]{16})" // Device ID regex pattern matching 16 hex characters of a macaddress
#define UUID_REGEX \
    "([0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12})" // UUID regex pattern matching UUID
                                                                                    // format
                                                                                    // /320ab7c8-9985-4497-8299-6f9b024608a3
#define DEVICES_COLLECTION_NAME "devices"
#define ACTIONS_COLLECTION_NAME "actions"
#define DIAGNOSTICS_COLLECTION_NAME "diagnostics"

// API endpoint path definition ids
#define OT_REST_ROUTE_DEVICES "/api/devices"
#define OT_REST_ROUTE_DEVICES_ID "/api/devices/:id"
#define OT_REST_ROUTE_NODE "/api/node"

#define OT_REST_ROUTE_ACTIONS "/api/actions"
#define OT_REST_ROUTE_ACTIONS_ID "/api/actions/:id"

#define OT_REST_ROUTE_DIAGNOSTICS "/api/diagnostics"
#define OT_REST_ROUTE_DIAGNOSTICS_ID "/api/diagnostics/:id"

using std::chrono::duration_cast;
using std::chrono::microseconds;
using std::chrono::steady_clock;

using namespace httplib;

namespace otbr {
namespace rest {

HttpMethod GetMethod(const Request &aRequest)
{
    if (aRequest.method == "GET")
        return HttpMethod::kGet;
    if (aRequest.method == "POST")
        return HttpMethod::kPost;
    if (aRequest.method == "PUT")
        return HttpMethod::kPut;
    if (aRequest.method == "DELETE")
        return HttpMethod::kDelete;
    if (aRequest.method == "OPTIONS")
        return HttpMethod::kOptions;

    return HttpMethod::kInvalidMethod;
}

RestWebServer::RestWebServer(Host::RcpHost &aHost)
    : mHost(aHost)
{
    mServer.Get(OT_REST_RESOURCE_PATH_NODE, MakeHandler(&RestWebServer::NodeInfo));
    mServer.Delete(OT_REST_RESOURCE_PATH_NODE, MakeHandler(&RestWebServer::NodeInfo));
    mServer.Get(OT_REST_RESOURCE_PATH_NODE_BAID, MakeHandler(&RestWebServer::BaId));
    mServer.Get(OT_REST_RESOURCE_PATH_NODE_STATE, MakeHandler(&RestWebServer::State));
    mServer.Put(OT_REST_RESOURCE_PATH_NODE_STATE, MakeHandler(&RestWebServer::State));
    mServer.Options(OT_REST_RESOURCE_PATH_NODE_STATE, MakeHandler(&RestWebServer::State));
    mServer.Get(OT_REST_RESOURCE_PATH_NODE_EXTADDRESS, MakeHandler(&RestWebServer::ExtendedAddr));
    mServer.Get(OT_REST_RESOURCE_PATH_NODE_NETWORKNAME, MakeHandler(&RestWebServer::NetworkName));
    mServer.Get(OT_REST_RESOURCE_PATH_NODE_RLOC16, MakeHandler(&RestWebServer::Rloc16));
    mServer.Get(OT_REST_RESOURCE_PATH_NODE_LEADERDATA, MakeHandler(&RestWebServer::LeaderData));
    mServer.Get(OT_REST_RESOURCE_PATH_NODE_NUMOFROUTER, MakeHandler(&RestWebServer::NumOfRoute));
    mServer.Get(OT_REST_RESOURCE_PATH_NODE_EXTPANID, MakeHandler(&RestWebServer::ExtendedPanId));
    mServer.Get(OT_REST_RESOURCE_PATH_NODE_RLOC, MakeHandler(&RestWebServer::Rloc));
    mServer.Get(OT_REST_RESOURCE_PATH_NODE_DATASET_ACTIVE, MakeHandler(&RestWebServer::DatasetActive));
    mServer.Put(OT_REST_RESOURCE_PATH_NODE_DATASET_ACTIVE, MakeHandler(&RestWebServer::DatasetActive));
    mServer.Options(OT_REST_RESOURCE_PATH_NODE_DATASET_ACTIVE, MakeHandler(&RestWebServer::DatasetActive));
    mServer.Get(OT_REST_RESOURCE_PATH_NODE_DATASET_PENDING, MakeHandler(&RestWebServer::DatasetPending));
    mServer.Put(OT_REST_RESOURCE_PATH_NODE_DATASET_PENDING, MakeHandler(&RestWebServer::DatasetPending));
    mServer.Options(OT_REST_RESOURCE_PATH_NODE_DATASET_PENDING, MakeHandler(&RestWebServer::DatasetPending));
    mServer.Get(OT_REST_RESOURCE_PATH_NODE_COMMISSIONER_STATE, MakeHandler(&RestWebServer::CommissionerState));
    mServer.Put(OT_REST_RESOURCE_PATH_NODE_COMMISSIONER_STATE, MakeHandler(&RestWebServer::CommissionerState));
    mServer.Options(OT_REST_RESOURCE_PATH_NODE_COMMISSIONER_STATE, MakeHandler(&RestWebServer::CommissionerState));
    mServer.Get(OT_REST_RESOURCE_PATH_NODE_COMMISSIONER_JOINER, MakeHandler(&RestWebServer::CommissionerJoiner));
    mServer.Post(OT_REST_RESOURCE_PATH_NODE_COMMISSIONER_JOINER, MakeHandler(&RestWebServer::CommissionerJoiner));
    mServer.Delete(OT_REST_RESOURCE_PATH_NODE_COMMISSIONER_JOINER, MakeHandler(&RestWebServer::CommissionerJoiner));
    mServer.Options(OT_REST_RESOURCE_PATH_NODE_COMMISSIONER_JOINER, MakeHandler(&RestWebServer::CommissionerJoiner));
    mServer.Get(OT_REST_RESOURCE_PATH_NODE_COPROCESSOR_VERSION, MakeHandler(&RestWebServer::CoprocessorVersion));

    mServer.set_error_handler(MakeHandler(&RestWebServer::RoutingErrorHandler));
    mServer.Get(OT_REST_ROUTE_ACTIONS, MakeHandlerInMainLoop(&RestWebServer::ApiActionsHandler));
    mServer.Get(OT_REST_ROUTE_ACTIONS_ID, MakeHandlerInMainLoop(&RestWebServer::ApiActionsItemGetHandler));
    mServer.Post(OT_REST_ROUTE_ACTIONS, MakeHandlerInMainLoop(&RestWebServer::ApiActionsHandler));
    mServer.Delete(OT_REST_ROUTE_ACTIONS, MakeHandlerInMainLoop(&RestWebServer::ApiActionsHandler));
    mServer.Delete(OT_REST_ROUTE_ACTIONS_ID, MakeHandlerInMainLoop(&RestWebServer::ApiActionsItemDeleteHandler));
    mServer.Options(OT_REST_ROUTE_ACTIONS, MakeHandlerInMainLoop(&RestWebServer::ApiActionsHandler));

    mServer.Get(OT_REST_ROUTE_DEVICES, MakeHandlerInMainLoop(&RestWebServer::ApiDevicesHandler));
    mServer.Get(OT_REST_ROUTE_DEVICES_ID, MakeHandlerInMainLoop(&RestWebServer::ApiDevicesItemGetHandler));
    mServer.Get(OT_REST_ROUTE_NODE, MakeHandlerInMainLoop(&RestWebServer::ApiDevicesSelfGetHandler));
    mServer.Delete(OT_REST_ROUTE_DEVICES, MakeHandlerInMainLoop(&RestWebServer::ApiDevicesHandler));
    mServer.Delete(OT_REST_ROUTE_DEVICES_ID, MakeHandlerInMainLoop(&RestWebServer::ApiDevicesItemDeleteHandler));
    mServer.Options(OT_REST_ROUTE_DEVICES, MakeHandlerInMainLoop(&RestWebServer::ApiDevicesHandler));

    mServer.Get(OT_REST_ROUTE_DIAGNOSTICS, MakeHandlerInMainLoop(&RestWebServer::ApiDiagnosticsHandler));
    mServer.Get(OT_REST_ROUTE_DIAGNOSTICS_ID, MakeHandlerInMainLoop(&RestWebServer::ApiDiagnosticsItemGetHandler));
    mServer.Delete(OT_REST_ROUTE_DIAGNOSTICS, MakeHandlerInMainLoop(&RestWebServer::ApiDiagnosticsHandler));
    mServer.Delete(OT_REST_ROUTE_DIAGNOSTICS_ID,
                   MakeHandlerInMainLoop(&RestWebServer::ApiDiagnosticsItemDeleteHandler));
    mServer.Options(OT_REST_ROUTE_DIAGNOSTICS, MakeHandlerInMainLoop(&RestWebServer::ApiDiagnosticsHandler));
}

RestWebServer::~RestWebServer(void)
{
    if (mServer.is_running())
    {
        mServer.stop();
    }
    if (mServerThread.joinable())
    {
        mServerThread.join();
    }
}

void RestWebServer::ErrorHandler(Response &aResponse, StatusCode aErrorCode) const
{
    std::string errorMessage = status_message(aErrorCode);
    std::string body         = Json::Error2JsonString(aErrorCode, errorMessage);

    aResponse.status = aErrorCode;
    aResponse.set_content(body, OT_REST_CONTENT_TYPE_JSON);
}

void RestWebServer::ErrorHandler(Response &aResponse, StatusCode aErrorCode, std::string aErrorDetails) const
{
    std::string errorMessage = status_message(aErrorCode);
    std::string body         = Json::ErrorDetails2JsonString(aErrorCode, errorMessage, aErrorDetails);

    aResponse.status = aErrorCode;
    aResponse.set_content(body, OT_REST_CONTENT_TYPE_JSON);
}

void RestWebServer::GetNodeInfo(Response &aResponse) const
{
    otbrError   error = OTBR_ERROR_NONE;
    std::string body;

    SuccessOrExit(error = RunInMainLoop([this, &body]() -> otbrError {
                      struct NodeInfo node = {};
                      otRouterInfo    routerInfo;
                      uint8_t         maxRouterId;

                      if (otBorderAgentGetId(GetInstance(), &node.mBaId) != OT_ERROR_NONE)
                          return OTBR_ERROR_REST;

                      (void)otThreadGetLeaderData(GetInstance(), &node.mLeaderData);

                      node.mNumOfRouter = 0;
                      maxRouterId       = otThreadGetMaxRouterId(GetInstance());
                      for (uint8_t i = 0; i <= maxRouterId; ++i)
                      {
                          if (otThreadGetRouterInfo(GetInstance(), i, &routerInfo) != OT_ERROR_NONE)
                          {
                              continue;
                          }
                          ++node.mNumOfRouter;
                      }

                      node.mRole        = GetDeviceRoleName(otThreadGetDeviceRole(GetInstance()));
                      node.mExtAddress  = reinterpret_cast<const uint8_t *>(otLinkGetExtendedAddress(GetInstance()));
                      node.mNetworkName = otThreadGetNetworkName(GetInstance());
                      node.mRloc16      = otThreadGetRloc16(GetInstance());
                      node.mExtPanId    = reinterpret_cast<const uint8_t *>(otThreadGetExtendedPanId(GetInstance()));
                      node.mRlocAddress = *otThreadGetRloc(GetInstance());

                      body = Json::Node2JsonString(node);

                      return OTBR_ERROR_NONE;
                  }));

    aResponse.set_content(body, OT_REST_CONTENT_TYPE_JSON);

exit:
    if (error == OTBR_ERROR_NONE)
    {
        aResponse.status = StatusCode::OK_200;
    }
    else
    {
        ErrorHandler(aResponse, StatusCode::InternalServerError_500);
    }
}

void RestWebServer::DeleteNodeInfo(Response &aResponse) const
{
    otbrError error = OTBR_ERROR_NONE;

    error = RunInMainLoop([this]() {
        VerifyOrReturn(mHost.GetThreadHelper()->Detach() == OT_ERROR_NONE, OTBR_ERROR_INVALID_STATE);
        VerifyOrReturn(otInstanceErasePersistentInfo(GetInstance()) == OT_ERROR_NONE, OTBR_ERROR_REST);
        mHost.Reset();
        return OTBR_ERROR_NONE;
    });

    if (error == OTBR_ERROR_NONE)
    {
        aResponse.status = StatusCode::OK_200;
    }
    else if (error == OTBR_ERROR_INVALID_STATE)
    {
        ErrorHandler(aResponse, StatusCode::Conflict_409);
    }
    else if (error != OTBR_ERROR_NONE)
    {
        ErrorHandler(aResponse, StatusCode::InternalServerError_500);
    }
}

void RestWebServer::NodeInfo(const Request &aRequest, Response &aResponse) const
{
    switch (GetMethod(aRequest))
    {
    case HttpMethod::kGet:
        GetNodeInfo(aResponse);
        break;
    case HttpMethod::kDelete:
        DeleteNodeInfo(aResponse);
        break;
    default:
        ErrorHandler(aResponse, StatusCode::MethodNotAllowed_405);
        break;
    }
}

void RestWebServer::GetDataBaId(Response &aResponse) const
{
    otbrError       error = OTBR_ERROR_NONE;
    otBorderAgentId id;
    std::string     body;

    VerifyOrExit(RunInMainLoop(otBorderAgentGetId, GetInstance(), &id) == OT_ERROR_NONE, error = OTBR_ERROR_REST);

    body = Json::Bytes2HexJsonString(id.mId, sizeof(id));
    aResponse.set_content(body, OT_REST_CONTENT_TYPE_JSON);

exit:
    if (error == OTBR_ERROR_NONE)
    {
        aResponse.status = StatusCode::OK_200;
    }
    else
    {
        ErrorHandler(aResponse, StatusCode::InternalServerError_500);
    }
}

void RestWebServer::BaId(const Request &aRequest, Response &aResponse) const
{
    if (GetMethod(aRequest) == HttpMethod::kGet)
    {
        GetDataBaId(aResponse);
    }
    else
    {
        ErrorHandler(aResponse, StatusCode::MethodNotAllowed_405);
    }
}

void RestWebServer::GetDataExtendedAddr(Response &aResponse) const
{
    const uint8_t *extAddress =
        reinterpret_cast<const uint8_t *>(RunInMainLoop(otLinkGetExtendedAddress, GetInstance()));
    std::string body = Json::Bytes2HexJsonString(extAddress, OT_EXT_ADDRESS_SIZE);

    aResponse.set_content(body, OT_REST_CONTENT_TYPE_JSON);
    aResponse.status = StatusCode::OK_200;
}

void RestWebServer::ExtendedAddr(const Request &aRequest, Response &aResponse) const
{
    if (GetMethod(aRequest) == HttpMethod::kGet)
    {
        GetDataExtendedAddr(aResponse);
    }
    else
    {
        ErrorHandler(aResponse, StatusCode::MethodNotAllowed_405);
    }
}

void RestWebServer::GetDataState(Response &aResponse) const
{
    std::string  state;
    otDeviceRole role;

    role  = RunInMainLoop(otThreadGetDeviceRole, GetInstance());
    state = Json::String2JsonString(GetDeviceRoleName(role));
    aResponse.set_content(state, OT_REST_CONTENT_TYPE_JSON);
    aResponse.status = StatusCode::OK_200;
}

void RestWebServer::SetDataState(const Request &aRequest, Response &aResponse) const
{
    otbrError error = OTBR_ERROR_NONE;

    std::string body;

    VerifyOrExit(Json::JsonString2String(aRequest.body, body), error = OTBR_ERROR_INVALID_ARGS);
    SuccessOrExit(
        error = RunInMainLoop([this, &body]() {
            if (body == "enable")
            {
                if (!otIp6IsEnabled(GetInstance()))
                {
                    VerifyOrReturn(otIp6SetEnabled(GetInstance(), true) == OT_ERROR_NONE, OTBR_ERROR_INVALID_STATE);
                }
                VerifyOrReturn(otThreadSetEnabled(GetInstance(), true) == OT_ERROR_NONE, OTBR_ERROR_INVALID_STATE);
            }
            else if (body == "disable")
            {
                VerifyOrReturn(otThreadSetEnabled(GetInstance(), false) == OT_ERROR_NONE, OTBR_ERROR_INVALID_STATE);
                VerifyOrReturn(otIp6SetEnabled(GetInstance(), false) == OT_ERROR_NONE, OTBR_ERROR_INVALID_STATE);
            }
            else
            {
                return OTBR_ERROR_INVALID_ARGS;
            }
            return OTBR_ERROR_NONE;
        }));

    aResponse.status = StatusCode::OK_200;

exit:
    if (error == OTBR_ERROR_INVALID_STATE)
    {
        ErrorHandler(aResponse, StatusCode::Conflict_409);
    }
    if (error == OTBR_ERROR_INVALID_ARGS)
    {
        ErrorHandler(aResponse, StatusCode::BadRequest_400);
    }
    else if (error != OTBR_ERROR_NONE)
    {
        ErrorHandler(aResponse, StatusCode::InternalServerError_500);
    }
}

void RestWebServer::State(const Request &aRequest, Response &aResponse) const
{
    switch (GetMethod(aRequest))
    {
    case HttpMethod::kGet:
        GetDataState(aResponse);
        break;
    case HttpMethod::kPut:
        SetDataState(aRequest, aResponse);
        break;
    case HttpMethod::kOptions:
        aResponse.status = StatusCode::OK_200;
        break;
    default:
        ErrorHandler(aResponse, StatusCode::MethodNotAllowed_405);
        break;
    }
}

void RestWebServer::GetDataNetworkName(Response &aResponse) const
{
    std::string networkName;

    networkName = RunInMainLoop(otThreadGetNetworkName, GetInstance());
    auto body   = Json::String2JsonString(networkName);

    aResponse.set_content(body, OT_REST_CONTENT_TYPE_JSON);
    aResponse.status = StatusCode::OK_200;
}

void RestWebServer::NetworkName(const Request &aRequest, Response &aResponse) const
{
    if (GetMethod(aRequest) == HttpMethod::kGet)
    {
        GetDataNetworkName(aResponse);
    }
    else
    {
        ErrorHandler(aResponse, StatusCode::MethodNotAllowed_405);
    }
}

void RestWebServer::GetDataLeaderData(Response &aResponse) const
{
    otbrError    error = OTBR_ERROR_NONE;
    otLeaderData leaderData;
    std::string  body;

    VerifyOrExit(RunInMainLoop(otThreadGetLeaderData, GetInstance(), &leaderData) == OT_ERROR_NONE,
                 error = OTBR_ERROR_REST);

    body = Json::LeaderData2JsonString(leaderData);

    aResponse.set_content(body, OT_REST_CONTENT_TYPE_JSON);

exit:
    if (error == OTBR_ERROR_NONE)
    {
        aResponse.status = StatusCode::OK_200;
    }
    else
    {
        ErrorHandler(aResponse, StatusCode::InternalServerError_500);
    }
}

void RestWebServer::LeaderData(const Request &aRequest, Response &aResponse) const
{
    if (GetMethod(aRequest) == HttpMethod::kGet)
    {
        GetDataLeaderData(aResponse);
    }
    else
    {
        ErrorHandler(aResponse, StatusCode::MethodNotAllowed_405);
    }
}

void RestWebServer::GetDataNumOfRoute(Response &aResponse) const
{
    std::string body = RunInMainLoop([this]() {
        uint8_t      count = 0;
        uint8_t      maxRouterId;
        otRouterInfo routerInfo;
        maxRouterId = otThreadGetMaxRouterId(GetInstance());
        for (uint8_t i = 0; i <= maxRouterId; ++i)
        {
            if (otThreadGetRouterInfo(GetInstance(), i, &routerInfo) != OT_ERROR_NONE)
            {
                continue;
            }
            ++count;
        }

        return Json::Number2JsonString(count);
    });

    aResponse.set_content(body, OT_REST_CONTENT_TYPE_JSON);
    aResponse.status = StatusCode::OK_200;
}

void RestWebServer::NumOfRoute(const Request &aRequest, Response &aResponse) const
{
    if (GetMethod(aRequest) == HttpMethod::kGet)
    {
        GetDataNumOfRoute(aResponse);
    }
    else
    {
        ErrorHandler(aResponse, StatusCode::MethodNotAllowed_405);
    }
}

void RestWebServer::GetDataRloc16(Response &aResponse) const
{
    uint16_t    rloc16 = RunInMainLoop(otThreadGetRloc16, GetInstance());
    std::string body;

    body = Json::Number2JsonString(rloc16);

    aResponse.set_content(body, OT_REST_CONTENT_TYPE_JSON);
    aResponse.status = StatusCode::OK_200;
}

void RestWebServer::Rloc16(const Request &aRequest, Response &aResponse) const
{
    if (GetMethod(aRequest) == HttpMethod::kGet)
    {
        GetDataRloc16(aResponse);
    }
    else
    {
        ErrorHandler(aResponse, StatusCode::MethodNotAllowed_405);
    }
}

void RestWebServer::GetDataExtendedPanId(Response &aResponse) const
{
    const uint8_t *extPanId = reinterpret_cast<const uint8_t *>(RunInMainLoop(otThreadGetExtendedPanId, GetInstance()));
    std::string    body     = Json::Bytes2HexJsonString(extPanId, OT_EXT_PAN_ID_SIZE);

    aResponse.set_content(body, OT_REST_CONTENT_TYPE_JSON);
    aResponse.status = StatusCode::OK_200;
}

void RestWebServer::ExtendedPanId(const Request &aRequest, Response &aResponse) const
{
    if (GetMethod(aRequest) == HttpMethod::kGet)
    {
        GetDataExtendedPanId(aResponse);
    }
    else
    {
        ErrorHandler(aResponse, StatusCode::MethodNotAllowed_405);
    }
}

void RestWebServer::GetDataRloc(Response &aResponse) const
{
    otIp6Address rlocAddress = *RunInMainLoop(otThreadGetRloc, GetInstance());
    std::string  body;

    body = Json::IpAddr2JsonString(rlocAddress);

    aResponse.set_content(body, OT_REST_CONTENT_TYPE_JSON);
    aResponse.status = StatusCode::OK_200;
}

void RestWebServer::Rloc(const Request &aRequest, Response &aResponse) const
{
    if (GetMethod(aRequest) == HttpMethod::kGet)
    {
        GetDataRloc(aResponse);
    }
    else
    {
        ErrorHandler(aResponse, StatusCode::MethodNotAllowed_405);
    }
}

void RestWebServer::GetDataset(DatasetType aDatasetType, const Request &aRequest, Response &aResponse) const
{
    otbrError   error = OTBR_ERROR_NONE;
    std::string body;
    std::string contentType = OT_REST_CONTENT_TYPE_JSON;

    SuccessOrExit(
        error = RunInMainLoop([this, aDatasetType, &contentType, &aRequest, &body]() {
            if (aRequest.get_header_value(OT_REST_ACCEPT_HEADER) == OT_REST_CONTENT_TYPE_PLAIN)
            {
                otOperationalDatasetTlvs datasetTlvs;

                if (aDatasetType == DatasetType::kActive)
                {
                    VerifyOrReturn(otDatasetGetActiveTlvs(GetInstance(), &datasetTlvs) == OT_ERROR_NONE,
                                   OTBR_ERROR_NOT_FOUND);
                }
                else if (aDatasetType == DatasetType::kPending)
                {
                    VerifyOrReturn(otDatasetGetPendingTlvs(GetInstance(), &datasetTlvs) == OT_ERROR_NONE,
                                   OTBR_ERROR_NOT_FOUND);
                }

                contentType = OT_REST_CONTENT_TYPE_PLAIN;
                body        = Utils::Bytes2Hex(datasetTlvs.mTlvs, datasetTlvs.mLength);
            }
            else
            {
                otOperationalDataset dataset;

                if (aDatasetType == DatasetType::kActive)
                {
                    VerifyOrReturn(otDatasetGetActive(GetInstance(), &dataset) == OT_ERROR_NONE, OTBR_ERROR_NOT_FOUND);
                    body = Json::ActiveDataset2JsonString(dataset);
                }
                else if (aDatasetType == DatasetType::kPending)
                {
                    VerifyOrReturn(otDatasetGetPending(GetInstance(), &dataset) == OT_ERROR_NONE, OTBR_ERROR_NOT_FOUND);
                    body = Json::PendingDataset2JsonString(dataset);
                }
            }

            return OTBR_ERROR_NONE;
        }));

    aResponse.set_content(body, contentType);

exit:
    if (error == OTBR_ERROR_NONE)
    {
        aResponse.status = StatusCode::OK_200;
    }
    else if (error == OTBR_ERROR_NOT_FOUND)
    {
        aResponse.status = StatusCode::NoContent_204;
    }
    else
    {
        ErrorHandler(aResponse, StatusCode::InternalServerError_500);
    }
}

void RestWebServer::SetDataset(DatasetType aDatasetType, const Request &aRequest, Response &aResponse) const
{
    otbrError       error = OTBR_ERROR_NONE;
    struct NodeInfo node;
    std::string     body;
    StatusCode      errorCode = StatusCode::OK_200;

    SuccessOrExit(
        error = RunInMainLoop([this, aDatasetType, &errorCode, &aRequest]() {
            bool                     isTlv;
            otOperationalDataset     dataset = {};
            otOperationalDatasetTlvs datasetTlvs;
            otError                  errorOt = OT_ERROR_NONE;

            if (aDatasetType == DatasetType::kActive)
            {
                VerifyOrReturn(otThreadGetDeviceRole(GetInstance()) == OT_DEVICE_ROLE_DISABLED,
                               OTBR_ERROR_INVALID_STATE);
                errorOt = otDatasetGetActiveTlvs(GetInstance(), &datasetTlvs);
            }
            else if (aDatasetType == DatasetType::kPending)
            {
                errorOt = otDatasetGetPendingTlvs(GetInstance(), &datasetTlvs);
            }

            // Create a new operational dataset if it doesn't exist.
            if (errorOt == OT_ERROR_NOT_FOUND)
            {
                VerifyOrReturn(otDatasetCreateNewNetwork(GetInstance(), &dataset) == OT_ERROR_NONE, OTBR_ERROR_REST);
                otDatasetConvertToTlvs(&dataset, &datasetTlvs);
                errorCode = StatusCode::Created_201;
            }

            isTlv = aRequest.get_header_value(OT_REST_CONTENT_TYPE_HEADER) == OT_REST_CONTENT_TYPE_PLAIN;

            if (isTlv)
            {
                int                      ret;
                otOperationalDatasetTlvs datasetUpdateTlvs;

                ret = Json::Hex2BytesJsonString(aRequest.body, datasetUpdateTlvs.mTlvs,
                                                OT_OPERATIONAL_DATASET_MAX_LENGTH);
                VerifyOrReturn(ret >= 0, OTBR_ERROR_INVALID_ARGS);
                datasetUpdateTlvs.mLength = ret;

                VerifyOrReturn(otDatasetParseTlvs(&datasetUpdateTlvs, &dataset) == OT_ERROR_NONE, OTBR_ERROR_REST);
                VerifyOrReturn(otDatasetUpdateTlvs(&dataset, &datasetTlvs) == OT_ERROR_NONE, OTBR_ERROR_REST);
            }
            else
            {
                if (aDatasetType == DatasetType::kActive)
                {
                    VerifyOrReturn(Json::JsonActiveDatasetString2Dataset(aRequest.body, dataset),
                                   OTBR_ERROR_INVALID_ARGS);
                }
                else if (aDatasetType == DatasetType::kPending)
                {
                    VerifyOrReturn(Json::JsonPendingDatasetString2Dataset(aRequest.body, dataset),
                                   OTBR_ERROR_INVALID_ARGS);
                    VerifyOrReturn(dataset.mComponents.mIsDelayPresent, OTBR_ERROR_INVALID_ARGS);
                }
                VerifyOrReturn(otDatasetUpdateTlvs(&dataset, &datasetTlvs) == OT_ERROR_NONE, OTBR_ERROR_REST);
            }

            if (aDatasetType == DatasetType::kActive)
            {
                VerifyOrReturn(otDatasetSetActiveTlvs(GetInstance(), &datasetTlvs) == OT_ERROR_NONE, OTBR_ERROR_REST);
            }
            else if (aDatasetType == DatasetType::kPending)
            {
                VerifyOrReturn(otDatasetSetPendingTlvs(GetInstance(), &datasetTlvs) == OT_ERROR_NONE, OTBR_ERROR_REST);
            }
            return OTBR_ERROR_NONE;
        }));

    aResponse.status = errorCode;

exit:
    if (error == OTBR_ERROR_INVALID_ARGS)
    {
        ErrorHandler(aResponse, StatusCode::BadRequest_400);
    }
    else if (error == OTBR_ERROR_INVALID_STATE)
    {
        ErrorHandler(aResponse, StatusCode::Conflict_409);
    }
    else if (error != OTBR_ERROR_NONE)
    {
        ErrorHandler(aResponse, StatusCode::InternalServerError_500);
    }
}

void RestWebServer::Dataset(DatasetType aDatasetType, const Request &aRequest, Response &aResponse) const
{
    switch (GetMethod(aRequest))
    {
    case HttpMethod::kGet:
        GetDataset(aDatasetType, aRequest, aResponse);
        break;
    case HttpMethod::kPut:
        SetDataset(aDatasetType, aRequest, aResponse);
        break;
    case HttpMethod::kOptions:
        aResponse.status = StatusCode::OK_200;
        break;
    default:
        ErrorHandler(aResponse, StatusCode::MethodNotAllowed_405);
        break;
    }
}

void RestWebServer::DatasetActive(const Request &aRequest, Response &aResponse) const
{
    Dataset(DatasetType::kActive, aRequest, aResponse);
}

void RestWebServer::DatasetPending(const Request &aRequest, Response &aResponse) const
{
    Dataset(DatasetType::kPending, aRequest, aResponse);
}

void RestWebServer::GetCommissionerState(Response &aResponse) const
{
    std::string         state;
    otCommissionerState stateCode;

    stateCode = RunInMainLoop(otCommissionerGetState, GetInstance());
    state     = Json::String2JsonString(GetCommissionerStateName(stateCode));
    aResponse.set_content(state, OT_REST_CONTENT_TYPE_JSON);
    aResponse.status = StatusCode::OK_200;
}

void RestWebServer::SetCommissionerState(const Request &aRequest, Response &aResponse) const
{
    otbrError   error = OTBR_ERROR_NONE;
    std::string body;

    VerifyOrExit(Json::JsonString2String(aRequest.body, body), error = OTBR_ERROR_INVALID_ARGS);
    SuccessOrExit(error = RunInMainLoop([this, &body]() {
                      if (body == "enable")
                      {
                          VerifyOrReturn(otCommissionerGetState(GetInstance()) == OT_COMMISSIONER_STATE_DISABLED,
                                         OTBR_ERROR_NONE);
                          VerifyOrReturn(otCommissionerStart(GetInstance(), NULL, NULL, NULL) == OT_ERROR_NONE,
                                         OTBR_ERROR_INVALID_STATE);
                      }
                      else if (body == "disable")
                      {
                          VerifyOrReturn(otCommissionerGetState(GetInstance()) != OT_COMMISSIONER_STATE_DISABLED,
                                         OTBR_ERROR_NONE);
                          VerifyOrReturn(otCommissionerStop(GetInstance()) == OT_ERROR_NONE, OTBR_ERROR_INVALID_STATE);
                      }
                      else
                      {
                          return OTBR_ERROR_INVALID_ARGS;
                      }
                      return OTBR_ERROR_NONE;
                  }));

exit:
    switch (error)
    {
    case OTBR_ERROR_NONE:
        aResponse.status = StatusCode::OK_200;
        break;
    case OTBR_ERROR_INVALID_STATE:
        ErrorHandler(aResponse, StatusCode::Conflict_409);
        break;
    case OTBR_ERROR_INVALID_ARGS:
        ErrorHandler(aResponse, StatusCode::BadRequest_400);
        break;
    default:
        ErrorHandler(aResponse, StatusCode::InternalServerError_500);
        break;
    }
}

void RestWebServer::CommissionerState(const Request &aRequest, Response &aResponse) const
{
    switch (GetMethod(aRequest))
    {
    case HttpMethod::kGet:
        GetCommissionerState(aResponse);
        break;
    case HttpMethod::kPut:
        SetCommissionerState(aRequest, aResponse);
        break;
    case HttpMethod::kOptions:
        aResponse.status = StatusCode::OK_200;
        break;
    default:
        ErrorHandler(aResponse, StatusCode::MethodNotAllowed_405);
        break;
    }
}

void RestWebServer::GetJoiners(Response &aResponse) const
{
    std::string body;

    body = RunInMainLoop([this]() {
        std::vector<otJoinerInfo> joinerTable;
        otJoinerInfo              joinerInfo;
        uint16_t                  iter = 0;

        while (otCommissionerGetNextJoinerInfo(GetInstance(), &iter, &joinerInfo) == OT_ERROR_NONE)
        {
            joinerTable.push_back(joinerInfo);
        }

        return Json::JoinerTable2JsonString(joinerTable);
    });

    aResponse.set_content(body, OT_REST_CONTENT_TYPE_JSON);
    aResponse.status = StatusCode::OK_200;
}

void RestWebServer::AddJoiner(const Request &aRequest, Response &aResponse) const
{
    otbrError error   = OTBR_ERROR_NONE;
    otError   errorOt = OT_ERROR_NONE;

    SuccessOrExit(
        error = RunInMainLoop([this, &errorOt, &aRequest]() {
            otJoinerInfo        joiner;
            const otExtAddress *addrPtr                         = nullptr;
            const uint8_t       emptyArray[OT_EXT_ADDRESS_SIZE] = {0};

            VerifyOrReturn(otCommissionerGetState(GetInstance()) == OT_COMMISSIONER_STATE_ACTIVE,
                           OTBR_ERROR_INVALID_STATE);

            VerifyOrReturn(Json::JsonJoinerInfoString2JoinerInfo(aRequest.body, joiner), OTBR_ERROR_INVALID_ARGS);

            addrPtr = &joiner.mSharedId.mEui64;
            if (memcmp(&joiner.mSharedId.mEui64, emptyArray, OT_EXT_ADDRESS_SIZE) == 0)
            {
                addrPtr = nullptr;
            }

            if (joiner.mType == OT_JOINER_INFO_TYPE_DISCERNER)
            {
                errorOt = otCommissionerAddJoinerWithDiscerner(GetInstance(), &joiner.mSharedId.mDiscerner,
                                                               joiner.mPskd.m8, joiner.mExpirationTime);
            }
            else
            {
                errorOt = otCommissionerAddJoiner(GetInstance(), addrPtr, joiner.mPskd.m8, joiner.mExpirationTime);
            }
            VerifyOrReturn(errorOt == OT_ERROR_NONE, OTBR_ERROR_OPENTHREAD);
            return OTBR_ERROR_NONE;
        }));

exit:
    switch (error)
    {
    case OTBR_ERROR_NONE:
        aResponse.status = StatusCode::OK_200;
        break;
    case OTBR_ERROR_INVALID_STATE:
        ErrorHandler(aResponse, StatusCode::Conflict_409);
        break;
    case OTBR_ERROR_INVALID_ARGS:
        ErrorHandler(aResponse, StatusCode::BadRequest_400);
        break;
    case OTBR_ERROR_OPENTHREAD:
        switch (errorOt)
        {
        case OT_ERROR_INVALID_ARGS:
            ErrorHandler(aResponse, StatusCode::BadRequest_400);
            break;
        case OT_ERROR_NO_BUFS:
            ErrorHandler(aResponse, StatusCode::InsufficientStorage_507);
            break;
        default:
            ErrorHandler(aResponse, StatusCode::InternalServerError_500);
            break;
        }
        break;
    default:
        ErrorHandler(aResponse, StatusCode::InternalServerError_500);
        break;
    }
}

void RestWebServer::RemoveJoiner(const Request &aRequest, Response &aResponse) const
{
    otbrError         error = OTBR_ERROR_NONE;
    otExtAddress      eui64;
    otExtAddress     *addrPtr   = nullptr;
    otJoinerDiscerner discerner = {
        .mValue  = 0,
        .mLength = 0,
    };
    std::string body;

    VerifyOrExit(otCommissionerGetState(GetInstance()) == OT_COMMISSIONER_STATE_ACTIVE,
                 error = OTBR_ERROR_INVALID_STATE);

    VerifyOrExit(Json::JsonString2String(aRequest.body, body), error = OTBR_ERROR_INVALID_ARGS);
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
        (void)otCommissionerRemoveJoiner(GetInstance(), addrPtr);
    }
    else
    {
        (void)otCommissionerRemoveJoinerWithDiscerner(GetInstance(), &discerner);
    }

exit:
    switch (error)
    {
    case OTBR_ERROR_NONE:
        aResponse.status = StatusCode::OK_200;
        break;
    case OTBR_ERROR_INVALID_STATE:
        ErrorHandler(aResponse, StatusCode::Conflict_409);
        break;
    case OTBR_ERROR_INVALID_ARGS:
        ErrorHandler(aResponse, StatusCode::BadRequest_400);
        break;
    default:
        ErrorHandler(aResponse, StatusCode::InternalServerError_500);
        break;
    }
}

void RestWebServer::CommissionerJoiner(const Request &aRequest, Response &aResponse) const
{
    switch (GetMethod(aRequest))
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
        aResponse.status = StatusCode::OK_200;
        break;
    default:
        ErrorHandler(aResponse, StatusCode::MethodNotAllowed_405);
        break;
    }
}

void RestWebServer::GetCoprocessorVersion(Response &aResponse) const
{
    std::string coprocessorVersion;

    coprocessorVersion = mHost.GetCoprocessorVersion();
    coprocessorVersion = Json::String2JsonString(coprocessorVersion);

    aResponse.set_content(coprocessorVersion, OT_REST_CONTENT_TYPE_JSON);
    aResponse.status = StatusCode::OK_200;
}

void RestWebServer::CoprocessorVersion(const Request &aRequest, Response &aResponse) const
{
    if (GetMethod(aRequest) == HttpMethod::kGet)
    {
        GetCoprocessorVersion(aResponse);
    }
    else
    {
        ErrorHandler(aResponse, StatusCode::MethodNotAllowed_405);
    }
}

void RestWebServer::RoutingErrorHandler(const Request &aRequest, Response &aResponse)
{
    httplib::StatusCode error = StatusCode::OK_200;
    std::string         errorDetails;

    // handle methods not used or not supported by cpp-httplib
    switch (GetMethod(aRequest))
    {
    case HttpMethod::kPost:
        // fallthrough
    case HttpMethod::kGet:
        // fallthrough
    case HttpMethod::kDelete:
        // fallthrough
    case HttpMethod::kOptions:
        break;
    default:
        errorDetails = "method not supported";
        error        = StatusCode::MethodNotAllowed_405;
        aResponse.set_header("Allow", "GET, POST, DELETE, OPTIONS");
        break;
    }

    if (error != StatusCode::OK_200 || aResponse.status >= StatusCode::MultipleChoices_300)
    {
        if (error < aResponse.status)
        {
            error = StatusCode(aResponse.status);
        }
        otbrLogWarning("%s:%d Error (%d) - %s", __FILE__, __LINE__, error, errorDetails.c_str());
        ErrorHandler(aResponse, error, errorDetails);
    }
}

std::map<std::string, std::string> RestWebServer::ExtractFieldsQueries(
    const Request               &aRequest,
    const std::set<std::string> &aContainedTypes) const
{
    std::map<std::string, std::string> queries;
    for (const auto &it : aContainedTypes)
    {
        std::string key = "fields[" + it + "]";
        auto        q   = aRequest.params.find(key);
        if (q != aRequest.params.end())
        {
            queries[it] = q->second;
        }
    }
    // If any "fields[...]" query param is present but queries[] remains empty,
    // add a dummy entry to produce an empty response.
    bool hasFieldsQuery = false;
    for (const auto &param : aRequest.params)
    {
        if (param.first.find("fields[") == 0)
        {
            hasFieldsQuery = true;
            break;
        }
    }
    if (hasFieldsQuery && queries.empty())
    {
        queries["None"] = "";
    }
    return queries;
}

void RestWebServer::ApiActionsHandler(const Request &aRequest, Response &aResponse)
{
    StatusCode  statusCode = StatusCode::OK_200;
    std::string errorDetails;
    VerifyOrExit(HasValidChars(aRequest, errorDetails) == OT_ERROR_NONE, statusCode = StatusCode::BadRequest_400);

    switch (GetMethod(aRequest))
    {
    case HttpMethod::kPost:
        ApiActionsPostHandler(aRequest, aResponse);
        break;
    case HttpMethod::kGet:
        ApiActionsGetHandler(aRequest, aResponse);
        break;
    case HttpMethod::kDelete:
        ApiActionsDeleteHandler(aRequest, aResponse);
        break;
    case HttpMethod::kOptions:
        aResponse.status = StatusCode::NoContent_204;
        aResponse.set_header("Allow", "GET, POST, DELETE, OPTIONS");
        break;
    default:
        // aResponse.SetAllowMethods(methods);
        errorDetails = "method not supported";
        statusCode   = StatusCode::MethodNotAllowed_405;
        break;
    }
exit:
    if (statusCode != StatusCode::OK_200)
    {
        otbrLogWarning("%s:%d Error (%d) - %s", __FILE__, __LINE__, statusCode, errorDetails.c_str());
        ErrorHandler(aResponse, statusCode, errorDetails);
    }
}

void RestWebServer::ApiActionsPostHandler(const Request &aRequest, Response &aResponse)
{
    ActionsList &actions = mServices.GetActionsList();
    std::string  responseMessage;
    uint32_t     taskQueueMax;
    std::string  errorCode;
    StatusCode   statusCode = StatusCode::OK_200;
    cJSON       *root       = nullptr;
    cJSON       *dataArray;
    cJSON       *resp_data;
    cJSON       *resp_obj = nullptr;
    cJSON       *datum;
    cJSON       *resp;
    const char  *resp_str;

    std::string aUuid(UUID_STR_LEN, '\0'); // An empty UUID string for obtaining the created actionId

    VerifyOrExit(!aRequest.get_header_value(OT_REST_CONTENT_TYPE_HEADER).empty(),
                 statusCode = StatusCode::NotAcceptable_406);

    VerifyOrExit((aRequest.get_header_value(OT_REST_CONTENT_TYPE_HEADER).compare(OT_REST_CONTENT_TYPE_JSONAPI) == 0),
                 statusCode = StatusCode::UnsupportedMediaType_415);

    root = cJSON_Parse(aRequest.body.c_str());
    VerifyOrExit(root != nullptr, statusCode = StatusCode::BadRequest_400);

    // perform general validation before we attempt to
    // perform any task specific validation
    dataArray = cJSON_GetObjectItemCaseSensitive(root, "data");
    VerifyOrExit((dataArray != nullptr) && cJSON_IsArray(dataArray), statusCode = StatusCode::UnprocessableContent_422);

    // validate the form and arguments of all tasks
    // before we attempt to perform processing on any of the tasks.
    for (int idx = 0; idx < cJSON_GetArraySize(dataArray); idx++)
    {
        // Require all items in the list to be valid Task items with all required attributes;
        // otherwise rejects whole list and returns 422 Unprocessable.
        // Unimplemented tasks counted as failed / invalid tasks
        VerifyOrExit(actions.ValidateRequest(cJSON_GetArrayItem(dataArray, idx)),
                     statusCode = StatusCode::UnprocessableContent_422);
    }

    // Check queueing all tasks does not exceed the max number of tasks we can have queued
    // older completed tasks can be omitted from the queue
    actions.GetPendingOrActive(taskQueueMax);
    taskQueueMax -= actions.GetMaxCollectionSize();
    VerifyOrExit(cJSON_GetArraySize(dataArray) >= 0 &&
                     taskQueueMax >= static_cast<uint32_t>(cJSON_GetArraySize(dataArray)),
                 statusCode = StatusCode::ServiceUnavailable_503);

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

    // prepare response object
    resp = cJSON_CreateObject();
    cJSON_AddItemToObject(resp, "data", resp_data);
    cJSON_AddItemToObject(resp, "meta", Json::CreateMetaCollection(0, 200, cJSON_GetArraySize(resp_data)));

    resp_str = cJSON_PrintUnformatted(resp);
    otbrLogDebug("%s:%d - %s - Sending (%d):\n%s", __FILE__, __LINE__, __func__, strlen(resp_str), resp_str);

    responseMessage = resp_str;
    aResponse.set_content(responseMessage, OT_REST_CONTENT_TYPE_JSONAPI);
    aResponse.status = StatusCode::OK_200;

    cJSON_free((void *)resp_str);
    cJSON_Delete(resp);

    // Clear the 'root' JSON object and release its memory (this should also delete 'data')
    cJSON_Delete(root);
    root = nullptr;

exit:
    if (statusCode != StatusCode::OK_200)
    {
        if (root != nullptr)
        {
            cJSON_Delete(root);
        }
        otbrLogWarning("%s:%d Error (%d)", __FILE__, __LINE__, statusCode);
        ErrorHandler(aResponse, statusCode);
    }
}

void RestWebServer::ApiActionsGetHandler(const Request &aRequest, Response &aResponse)
{
    StatusCode                         statusCode = StatusCode::OK_200;
    std::string                        resp_body;
    std::map<std::string, std::string> queries;

    ActionsList &actions = mServices.GetActionsList();

    std::string acceptHeader = aRequest.get_header_value(OT_REST_ACCEPT_HEADER);
    // Check if acceptHeader was not provided and is the default value ("*/*")
    if (acceptHeader.compare("*/*") == 0)
    {
        acceptHeader = OT_REST_CONTENT_TYPE_JSONAPI;
    }

    VerifyOrExit(((acceptHeader.compare(OT_REST_CONTENT_TYPE_JSONAPI) == 0) ||
                  (acceptHeader.compare(OT_REST_CONTENT_TYPE_JSON) == 0)),
                 statusCode = StatusCode::NotAcceptable_406);

    queries = ExtractFieldsQueries(aRequest, actions.GetContainedTypes());

    if (acceptHeader.compare(OT_REST_CONTENT_TYPE_JSONAPI) == 0)
    {
        // return all items
        resp_body = actions.ToJsonApiColl(queries);
        aResponse.set_content(resp_body, OT_REST_CONTENT_TYPE_JSONAPI);
    }
    else if (acceptHeader.compare(OT_REST_CONTENT_TYPE_JSON) == 0)
    {
        // return all items
        resp_body = actions.ToJsonString();
        aResponse.set_content(resp_body, OT_REST_CONTENT_TYPE_JSON);
    }

exit:
    if (statusCode != StatusCode::OK_200)
    {
        ErrorHandler(aResponse, statusCode);
    }
    else
    {
        aResponse.status = statusCode;
    }
}

void RestWebServer::ApiActionsItemGetHandler(const Request &aRequest, Response &aResponse)
{
    StatusCode                         statusCode = StatusCode::OK_200;
    std::string                        resp_body;
    std::map<std::string, std::string> queries;
    std::string                        itemId = aRequest.path_params.at("id");
    Uuid                               uuid;
    ActionsList                       &actions      = mServices.GetActionsList();
    std::string                        acceptHeader = aRequest.get_header_value(OT_REST_ACCEPT_HEADER);

    // Check if acceptHeader was not provided and is the default value ("*/*")
    if (acceptHeader.compare("*/*") == 0)
    {
        acceptHeader = OT_REST_CONTENT_TYPE_JSONAPI;
    }

    VerifyOrExit(((acceptHeader.compare(OT_REST_CONTENT_TYPE_JSONAPI) == 0) ||
                  (acceptHeader.compare(OT_REST_CONTENT_TYPE_JSON) == 0)),
                 statusCode = StatusCode::NotAcceptable_406);

    VerifyOrExit(uuid.Parse(itemId), statusCode = StatusCode::NotFound_404);

    queries = ExtractFieldsQueries(aRequest, actions.GetContainedTypes());

    if (acceptHeader.compare(OT_REST_CONTENT_TYPE_JSONAPI) == 0)
    {
        // return the item
        resp_body = actions.ToJsonApiItemId(itemId, queries);
        VerifyOrExit(!resp_body.empty(), statusCode = StatusCode::NotFound_404);
        aResponse.set_content(resp_body, OT_REST_CONTENT_TYPE_JSONAPI);
    }
    else if (acceptHeader.compare(OT_REST_CONTENT_TYPE_JSON) == 0)
    {
        // return the item
        resp_body = actions.ToJsonStringItemId(itemId, queries);
        VerifyOrExit(!resp_body.empty(), statusCode = StatusCode::NotFound_404);
        aResponse.set_content(resp_body, OT_REST_CONTENT_TYPE_JSON);
    }

exit:
    if (statusCode != StatusCode::OK_200)
    {
        ErrorHandler(aResponse, statusCode);
    }
    else
    {
        aResponse.status = statusCode;
    }
}

void RestWebServer::ApiActionsDeleteHandler(const Request &aRequest, Response &aResponse)
{
    OT_UNUSED_VARIABLE(aRequest);
    mServices.GetActionsList().DeleteAllActions();
    aResponse.status = StatusCode::NoContent_204;
}

void RestWebServer::ApiActionsItemDeleteHandler(const Request &aRequest, Response &aResponse)
{
    StatusCode  statusCode = StatusCode::NoContent_204;
    std::string itemId     = aRequest.path_params.at("id");
    Uuid        uuid;

    // Check if itemId is a valid UUID
    VerifyOrExit(uuid.Parse(itemId), statusCode = StatusCode::NotFound_404);
    // If itemId is a valid UUID, delete the action with that ID
    VerifyOrExit(mServices.GetActionsList().DeleteAction(uuid) == OT_ERROR_NONE, statusCode = StatusCode::NotFound_404);

exit:
    if (statusCode != StatusCode::NoContent_204)
    {
        ErrorHandler(aResponse, statusCode);
    }
    else
    {
        aResponse.status = statusCode;
    }
}

void RestWebServer::ApiDiagnosticsGetHandler(const Request &aRequest, Response &aResponse)
{
    StatusCode statusCode = StatusCode::OK_200;

    std::string                        resp_body;
    std::map<std::string, std::string> queries;
    std::string                        itemId;

    std::string acceptHeader = aRequest.get_header_value(OT_REST_ACCEPT_HEADER);
    // Check if acceptHeader was not provided and is the default value ("*/*")
    if (acceptHeader == "*/*")
    {
        acceptHeader = OT_REST_CONTENT_TYPE_JSONAPI;
    }

    VerifyOrExit(((acceptHeader.compare(OT_REST_CONTENT_TYPE_JSONAPI) == 0) ||
                  (acceptHeader.compare(OT_REST_CONTENT_TYPE_JSON) == 0)),
                 statusCode = StatusCode::NotAcceptable_406);

    queries = ExtractFieldsQueries(aRequest, mServices.GetDiagnosticsCollection().GetContainedTypes());

    if (acceptHeader.compare(OT_REST_CONTENT_TYPE_JSONAPI) == 0)
    {
        // return all items
        resp_body = mServices.GetDiagnosticsCollection().ToJsonApiColl(queries);
        aResponse.set_content(resp_body, OT_REST_CONTENT_TYPE_JSONAPI);
    }
    else if (acceptHeader.compare(OT_REST_CONTENT_TYPE_JSON) == 0)
    {
        // return all items
        resp_body = mServices.GetDiagnosticsCollection().ToJsonString();
        aResponse.set_content(resp_body, OT_REST_CONTENT_TYPE_JSON);
    }

exit:
    if (statusCode != StatusCode::OK_200)
    {
        ErrorHandler(aResponse, statusCode);
    }
}

void RestWebServer::ApiDiagnosticsItemGetHandler(const Request &aRequest, Response &aResponse)
{
    StatusCode                         statusCode = StatusCode::OK_200;
    std::string                        resp_body;
    std::map<std::string, std::string> queries;
    std::string                        itemId       = aRequest.path_params.at("id");
    std::string                        acceptHeader = aRequest.get_header_value(OT_REST_ACCEPT_HEADER);

    // Check if acceptHeader was not provided and is the default value ("*/*")
    if (acceptHeader == "*/*")
    {
        acceptHeader = OT_REST_CONTENT_TYPE_JSONAPI;
    }

    VerifyOrExit(((acceptHeader.compare(OT_REST_CONTENT_TYPE_JSONAPI) == 0) ||
                  (acceptHeader.compare(OT_REST_CONTENT_TYPE_JSON) == 0)),
                 statusCode = StatusCode::NotAcceptable_406);

    queries = ExtractFieldsQueries(aRequest, mServices.GetDiagnosticsCollection().GetContainedTypes());

    if (acceptHeader.compare(OT_REST_CONTENT_TYPE_JSONAPI) == 0)
    {
        // return the item
        resp_body = mServices.GetDiagnosticsCollection().ToJsonApiItemId(itemId, queries);
        VerifyOrExit(!resp_body.empty(), statusCode = StatusCode::NotFound_404);
        aResponse.set_content(resp_body, OT_REST_CONTENT_TYPE_JSONAPI);
    }
    else if (acceptHeader.compare(OT_REST_CONTENT_TYPE_JSON) == 0)
    {
        // return the item
        resp_body = mServices.GetDiagnosticsCollection().ToJsonStringItemId(itemId, queries);
        VerifyOrExit(!resp_body.empty(), statusCode = StatusCode::NotFound_404);
        aResponse.set_content(resp_body, OT_REST_CONTENT_TYPE_JSON);
    }

exit:
    if (statusCode != StatusCode::OK_200)
    {
        ErrorHandler(aResponse, statusCode);
    }
}

void RestWebServer::ApiDiagnosticsDeleteHandler(const Request &aRequest, Response &aResponse)
{
    OT_UNUSED_VARIABLE(aRequest);
    mServices.GetDiagnosticsCollection().DeleteAll();
    mServices.GetNetworkDiagHandler().Clear();
    aResponse.status = StatusCode::NoContent_204;
}

void RestWebServer::ApiDiagnosticsItemDeleteHandler(const Request &aRequest, Response &aResponse)
{
    StatusCode  statusCode = StatusCode::NoContent_204;
    std::string itemId     = aRequest.path_params.at("id");
    Uuid        uuid;

    // Check if itemId is a valid UUID
    VerifyOrExit(uuid.Parse(itemId), statusCode = StatusCode::NotFound_404);
    // If itemId is a valid UUID, delete the action with that ID
    VerifyOrExit(mServices.GetDiagnosticsCollection().Delete(itemId) == OT_ERROR_NONE,
                 statusCode = StatusCode::NotFound_404);

exit:
    if (statusCode != StatusCode::NoContent_204)
    {
        ErrorHandler(aResponse, statusCode);
    }
    else
    {
        mServices.GetNetworkDiagHandler().Clear();
        aResponse.status = statusCode;
    }
}

void RestWebServer::ApiDiagnosticsHandler(const Request &aRequest, Response &aResponse)
{
    StatusCode  statusCode = StatusCode::OK_200;
    std::string errorDetails;
    VerifyOrExit(HasValidChars(aRequest, errorDetails) == OT_ERROR_NONE, statusCode = StatusCode::BadRequest_400);

    switch (GetMethod(aRequest))
    {
    case HttpMethod::kGet:
        ApiDiagnosticsGetHandler(aRequest, aResponse);
        break;
    case HttpMethod::kDelete:
        ApiDiagnosticsDeleteHandler(aRequest, aResponse);
        break;
    case HttpMethod::kOptions:
        aResponse.status = StatusCode::NoContent_204;
        aResponse.set_header("Allow", "GET, DELETE, OPTIONS");
        break;
    case HttpMethod::kPost:
    default:
        // aResponse.SetAllowMethods(methods);
        errorDetails = "not supported";
        statusCode   = StatusCode::MethodNotAllowed_405;
        break;
    }
exit:
    if (statusCode != StatusCode::OK_200)
    {
        otbrLogWarning("%s:%d Error (%d)", __FILE__, __LINE__, statusCode);
        ErrorHandler(aResponse, statusCode, errorDetails);
    }
}

void RestWebServer::ApiDevicesHandler(const Request &aRequest, Response &aResponse)
{
    StatusCode  statusCode = StatusCode::OK_200;
    std::string errorDetails;
    VerifyOrExit(HasValidChars(aRequest, errorDetails) == OT_ERROR_NONE, statusCode = StatusCode::BadRequest_400);

    switch (GetMethod(aRequest))
    {
    case HttpMethod::kDelete:
        ApiDevicesDeleteHandler(aRequest, aResponse);
        break;
    case HttpMethod::kGet:
        ApiDevicesGetHandler(aRequest, aResponse);
        break;
    case HttpMethod::kOptions:
        aResponse.status = StatusCode::NoContent_204;
        aResponse.set_header("Allow", "GET, DELETE, OPTIONS");
        break;
    default:
        // aResponse.SetAllowMethods(methods);
        errorDetails = "not supported";
        statusCode   = StatusCode::MethodNotAllowed_405;
        break;
    }

exit:
    if (statusCode != StatusCode::OK_200)
    {
        otbrLogWarning("%s:%d Error (%d) - %s", __FILE__, __LINE__, statusCode, errorDetails.c_str());
        ErrorHandler(aResponse, statusCode, errorDetails);
    }
}

void RestWebServer::ApiDevicesDeleteHandler(const Request &aRequest, Response &aResponse)
{
    OT_UNUSED_VARIABLE(aRequest);
    mServices.GetDevicesCollection().DeleteAll();
    aResponse.status = StatusCode::NoContent_204;
}

void RestWebServer::ApiDevicesItemDeleteHandler(const Request &aRequest, Response &aResponse)
{
    StatusCode  statusCode = StatusCode::NoContent_204;
    std::string itemId     = aRequest.path_params.at("id");

    // DeviceCollection expects itemId to be a valid hex string of length 16
    VerifyOrExit((itemId.length() == 16 && itemId.find_first_not_of("0123456789abcdefABCDEF") == std::string::npos),
                 statusCode = StatusCode::NotFound_404);
    // If itemId is present, try to delete the specific device
    VerifyOrExit(mServices.GetDevicesCollection().Delete(itemId), statusCode = StatusCode::NotFound_404);

exit:
    if (statusCode != StatusCode::NoContent_204)
    {
        ErrorHandler(aResponse, statusCode);
    }
    else
    {
        aResponse.status = statusCode;
    }
}

otError RestWebServer::HasValidChars(const Request &aRequest, std::string &aErrorDetails)
{
    otError          error            = OT_ERROR_NONE;
    constexpr size_t kMaxHeaderParams = 32;
    constexpr size_t kMaxQueryParams  = 16;

    // Accept only ASCII alphanumerics and common symbols; percent (%) is allowed only for ASCII hex encoding (%20,
    // etc.)
    const std::string acceptedCharacters =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_ ,.;?-+()[]{}=/*\"\n:%";

    // Helper lambda to check percent-encoding is ASCII hex only
    auto isValidPercentEncoding = [](const std::string &str, const std::string &accepted) -> bool {
        size_t pos = 0;
        while ((pos = str.find('%', pos)) != std::string::npos)
        {
            if (pos + 2 >= str.size())
                return false;
            char c1 = str[pos + 1];
            char c2 = str[pos + 2];
            if (!isxdigit(c1) || !isxdigit(c2))
                return false;
            // Optionally, restrict to ASCII range only (i.e., decoded value <= 0x7F)
            int decoded = std::stoi(str.substr(pos + 1, 2), nullptr, 16);
            // Check if decoded character is in acceptedCharacters
            if (decoded > 0x7F || accepted.find(static_cast<char>(decoded)) == std::string::npos)
                return false;
            pos += 3;
        }
        return true;
    };

    // Limit the number of query parameters to prevent abuse (e.g., max 32)
    aErrorDetails = "Too many query parameters.";
    VerifyOrExit(aRequest.params.size() < kMaxQueryParams, error = OT_ERROR_INVALID_ARGS);
    // Check if the request queries are safe to process
    aErrorDetails = "Invalid characters in query.";
    for (const auto &param : aRequest.params)
    {
        const std::string &val = param.second;
        VerifyOrExit(val.find_first_not_of(acceptedCharacters) == std::string::npos, aErrorDetails += " (" + val + ")",
                     error = OT_ERROR_INVALID_ARGS);
        VerifyOrExit(isValidPercentEncoding(val, acceptedCharacters), error = OT_ERROR_INVALID_ARGS);
    }

    // Limit the number of header parameters to prevent abuse (e.g., max 16)
    aErrorDetails = "Too many header options.";
    VerifyOrExit(aRequest.headers.size() < kMaxHeaderParams, error = OT_ERROR_INVALID_ARGS);
    // Check if the request headers are safe to process
    aErrorDetails = "Invalid characters in headers.";
    for (const auto &header : aRequest.headers)
    {
        const std::string &val = header.second;
        VerifyOrExit((val.find_first_not_of(acceptedCharacters) == std::string::npos),
                     aErrorDetails += " (" + val + ")", error = OT_ERROR_INVALID_ARGS);
        VerifyOrExit(isValidPercentEncoding(val, acceptedCharacters), error = OT_ERROR_INVALID_ARGS);
    }

    // Check if the request body is safe to process
    aErrorDetails = "Invalid characters in body.";
    if (!aRequest.body.empty())
    {
        VerifyOrExit(aRequest.body.find_first_not_of(acceptedCharacters) == std::string::npos,
                     error = OT_ERROR_INVALID_ARGS);
        VerifyOrExit(isValidPercentEncoding(aRequest.body, acceptedCharacters), error = OT_ERROR_INVALID_ARGS);
    }

    // Check if the request path is safe to process
    aErrorDetails = "Invalid characters in path.";
    VerifyOrExit((aRequest.path.find_first_not_of(acceptedCharacters) == std::string::npos),
                 error = OT_ERROR_INVALID_ARGS);
    VerifyOrExit(isValidPercentEncoding(aRequest.path, acceptedCharacters), error = OT_ERROR_INVALID_ARGS);

exit:
    return error;
}

void RestWebServer::ApiDevicesSelfGetHandler(const Request &aRequest, Response &aResponse)
{
    StatusCode                         statusCode   = StatusCode::OK_200;
    std::string                        errorDetails = "None";
    std::string                        resp_body;
    std::map<std::string, std::string> queries;

    // Get this device's ext address as hex string, lowercase
    const otExtAddress *thisextaddr     = otLinkGetExtendedAddress(GetInstance());
    std::string         thisextaddr_str = otbr::Utils::Bytes2Hex(thisextaddr->m8, OT_EXT_ADDRESS_SIZE);
    thisextaddr_str                     = StringUtils::ToLowercase(thisextaddr_str);

    std::string acceptHeader = aRequest.get_header_value(OT_REST_ACCEPT_HEADER);
    // Check if acceptHeader was not provided and is the default value ("*/*")
    if (acceptHeader == "*/*")
    {
        acceptHeader = OT_REST_CONTENT_TYPE_JSONAPI;
    }

    VerifyOrExit(((acceptHeader.compare(OT_REST_CONTENT_TYPE_JSONAPI) == 0) ||
                  (acceptHeader.compare(OT_REST_CONTENT_TYPE_JSON) == 0)),
                 statusCode = StatusCode::NotAcceptable_406);

    VerifyOrExit(HasValidChars(aRequest, errorDetails) == OT_ERROR_NONE, statusCode = StatusCode::BadRequest_400);

    queries = ExtractFieldsQueries(aRequest, mServices.GetDevicesCollection().GetContainedTypes());

    if (acceptHeader.compare(OT_REST_CONTENT_TYPE_JSONAPI) == 0)
    {
        resp_body = mServices.GetDevicesCollection().ToJsonApiItemId(thisextaddr_str, queries);
        VerifyOrExit(!resp_body.empty(), statusCode = StatusCode::NotFound_404);
        aResponse.set_content(resp_body, OT_REST_CONTENT_TYPE_JSONAPI);
    }
    else if (acceptHeader.compare(OT_REST_CONTENT_TYPE_JSON) == 0)
    {
        resp_body = mServices.GetDevicesCollection().ToJsonStringItemId(thisextaddr_str, queries);
        VerifyOrExit(!resp_body.empty(), statusCode = StatusCode::NotFound_404);
        aResponse.set_content(resp_body, OT_REST_CONTENT_TYPE_JSON);
    }

exit:
    if (statusCode != StatusCode::OK_200)
    {
        ErrorHandler(aResponse, statusCode, errorDetails);
    }
}

void RestWebServer::ApiDevicesGetHandler(const Request &aRequest, Response &aResponse)
{
    StatusCode                         statusCode = StatusCode::OK_200;
    std::string                        resp_body;
    std::map<std::string, std::string> queries;

    std::string acceptHeader = aRequest.get_header_value(OT_REST_ACCEPT_HEADER);
    // Check if acceptHeader was not provided and is the default value ("*/*")
    if (acceptHeader == "*/*")
    {
        acceptHeader = OT_REST_CONTENT_TYPE_JSONAPI;
    }

    VerifyOrExit(((acceptHeader.compare(OT_REST_CONTENT_TYPE_JSONAPI) == 0) ||
                  (acceptHeader.compare(OT_REST_CONTENT_TYPE_JSON) == 0)),
                 statusCode = StatusCode::NotAcceptable_406);

    queries = ExtractFieldsQueries(aRequest, mServices.GetDevicesCollection().GetContainedTypes());

    if (acceptHeader.compare(OT_REST_CONTENT_TYPE_JSONAPI) == 0)
    {
        // return all items
        resp_body = mServices.GetDevicesCollection().ToJsonApiColl(queries);
        aResponse.set_content(resp_body, OT_REST_CONTENT_TYPE_JSONAPI);
    }
    else if (acceptHeader.compare(OT_REST_CONTENT_TYPE_JSON) == 0)
    {
        // return all items
        resp_body = mServices.GetDevicesCollection().ToJsonString();
        aResponse.set_content(resp_body, OT_REST_CONTENT_TYPE_JSON);
    }

exit:
    if (statusCode != StatusCode::OK_200)
    {
        ErrorHandler(aResponse, statusCode);
    }
}

void RestWebServer::ApiDevicesItemGetHandler(const Request &aRequest, Response &aResponse)
{
    StatusCode                         statusCode = StatusCode::OK_200;
    std::string                        resp_body;
    std::map<std::string, std::string> queries;
    std::string                        itemId       = aRequest.path_params.at("id");
    std::string                        acceptHeader = aRequest.get_header_value(OT_REST_ACCEPT_HEADER);

    // Check if acceptHeader was not provided and is the default value ("*/*")
    if (acceptHeader == "*/*")
    {
        acceptHeader = OT_REST_CONTENT_TYPE_JSONAPI;
    }

    VerifyOrExit(((acceptHeader.compare(OT_REST_CONTENT_TYPE_JSONAPI) == 0) ||
                  (acceptHeader.compare(OT_REST_CONTENT_TYPE_JSON) == 0)),
                 statusCode = StatusCode::NotAcceptable_406);

    queries = ExtractFieldsQueries(aRequest, mServices.GetDevicesCollection().GetContainedTypes());

    if (acceptHeader.compare(OT_REST_CONTENT_TYPE_JSONAPI) == 0)
    {
        // return the item
        resp_body = mServices.GetDevicesCollection().ToJsonApiItemId(itemId, queries);
        VerifyOrExit(!resp_body.empty(), statusCode = StatusCode::NotFound_404);
        aResponse.set_content(resp_body, OT_REST_CONTENT_TYPE_JSONAPI);
    }
    else if (acceptHeader.compare(OT_REST_CONTENT_TYPE_JSON) == 0)
    {
        // return the item
        resp_body = mServices.GetDevicesCollection().ToJsonStringItemId(itemId, queries);
        VerifyOrExit(!resp_body.empty(), statusCode = StatusCode::NotFound_404);
        aResponse.set_content(resp_body, OT_REST_CONTENT_TYPE_JSON);
    }

exit:
    if (statusCode != StatusCode::OK_200)
    {
        ErrorHandler(aResponse, statusCode);
    }
}

void RestWebServer::ApiDevicesNodeInit()
{
    // Initialize the device collection with the current device info
    DeviceInfo          aDeviceInfo = {};
    const otExtAddress *thisextaddr = otLinkGetExtendedAddress(GetInstance());
    std::string         thisextaddr_str;

    thisextaddr_str = otbr::Utils::Bytes2Hex(thisextaddr->m8, OT_EXT_ADDRESS_SIZE);
    thisextaddr_str = StringUtils::ToLowercase(thisextaddr_str);

    mServices.GetNetworkDiagHandler().SetDeviceItemAttributes(thisextaddr_str, aDeviceInfo);
}

/**
 * @brief Initializes the REST web server and starts the server thread.
 *
 * This function is not thread-safe and should be called only once during initialization.
 * The server thread is started and will run for the lifetime of the RestWebServer object.
 * The thread is joined and cleaned up in the destructor. Concurrent calls to Init are not supported.
 *
 * Note: This function must be called from a context where the RestWebServer object is managed by std::shared_ptr,
 * as it uses shared_from_this(). Calling this function on an object not managed by std::shared_ptr will result in
 * undefined behavior.
 */
void RestWebServer::Init(const std::string &aRestListenAddress, int aRestListenPort)
{
    static bool sInitialized = false;
    if (sInitialized)
    {
        otbrLogWarning("REST server Init called more than once; ignoring subsequent calls.");
        return;
    }
    sInitialized = true;

    mServices.Init(GetInstance());
    // Add current node to device collection for API exposure
    ApiDevicesNodeInit();

    // Use shared_from_this() to get a shared_ptr, then capture a weak_ptr in the thread lambda
    // to avoid undefined behavior if the RestWebServer object is destroyed while the thread is running.
    std::weak_ptr<RestWebServer> weakSelf = shared_from_this();
    mServerThread                         = std::thread([aRestListenAddress, aRestListenPort, weakSelf]() {
        if (auto self = weakSelf.lock())
        {
            otbrLogInfo("RestWebServer listening on %s:%u", aRestListenAddress.c_str(), aRestListenPort);
            self->mServer.set_ipv6_v6only(false);
            self->mServer.set_socket_options([](int sock) {
                int opt = 1;
                // cpp-httplib defaults to SO_REUSEPORT instead of SO_REUSEADDR
                setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
            });
            const httplib::Headers defaultHeaders = {
                {"Access-Control-Allow-Origin", OTBR_REST_ACCESS_CONTROL_ALLOW_ORIGIN},
                {"Access-Control-Allow-Methods", OTBR_REST_ACCESS_CONTROL_ALLOW_METHODS},
                {"Access-Control-Allow-Headers", OTBR_REST_ACCESS_CONTROL_ALLOW_HEADERS}};
            self->mServer.set_default_headers(defaultHeaders);
            if (!self->mServer.listen(aRestListenAddress, aRestListenPort))
            {
                otbrLogWarning("REST server failed to start on %s:%d", aRestListenAddress.c_str(), aRestListenPort);
            }
        }
    });
}

} // namespace rest
} // namespace otbr
