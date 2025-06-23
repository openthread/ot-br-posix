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
#include "rest/types.hpp"

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

using std::chrono::duration_cast;
using std::chrono::microseconds;
using std::chrono::steady_clock;

using namespace httplib;

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
    mServer.Get(OT_REST_RESOURCE_PATH_DIAGNOSTICS, MakeHandler(&RestWebServer::Diagnostic));
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

void RestWebServer::DeleteOutDatedDiagnostic(void)
{
    for (auto eraseIt = mDiagSet.begin(); eraseIt != mDiagSet.end();)
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

void RestWebServer::UpdateDiag(std::string aKey, std::vector<otNetworkDiagTlv> &aDiag)
{
    DiagInfo value;

    value.mStartTime = steady_clock::now();
    value.mDiagContent.assign(aDiag.begin(), aDiag.end());
    mDiagSet[aKey] = value;
}

void RestWebServer::Diagnostic(const Request &aRequest, Response &aResponse)
{
    otbrError error = OTBR_ERROR_NONE;
    OT_UNUSED_VARIABLE(aRequest);
    struct otIp6Address multicastAddress;

    VerifyOrExit(otIp6AddressFromString(kMulticastAddrAllRouters, &multicastAddress) == OT_ERROR_NONE,
                 error = OTBR_ERROR_REST);
    VerifyOrExit(
        RunInMainLoop(
            otThreadSendDiagnosticGet, GetInstance(), &multicastAddress, kAllTlvTypes,
            static_cast<uint8_t>(sizeof(kAllTlvTypes)),
            [](otError aError, otMessage *aMessage, const otMessageInfo *aMessageInfo, void *aContext) -> void {
                static_cast<RestWebServer *>(aContext)->DiagnosticResponseHandler(aError, aMessage, aMessageInfo);
            },
            static_cast<void *>(this)) == OT_ERROR_NONE,
        error = OTBR_ERROR_REST);

    RunInMainLoop(duration_cast<Milliseconds>(Microseconds(kDiagCollectTimeout)), [this] {
        DeleteOutDatedDiagnostic();
        return mDiagSet.size();
    });

exit:

    if (error == OTBR_ERROR_NONE)
    {
        std::vector<std::vector<otNetworkDiagTlv>> diagContentSet;
        std::string                                body;

        for (auto it = mDiagSet.begin(); it != mDiagSet.end(); ++it)
        {
            diagContentSet.push_back(it->second.mDiagContent);
        }

        body             = Json::Diag2JsonString(diagContentSet);
        aResponse.status = StatusCode::OK_200;
        aResponse.set_content(body, OT_REST_CONTENT_TYPE_JSON);
    }
    else
    {
        ErrorHandler(aResponse, StatusCode::InternalServerError_500);
    }
}

void RestWebServer::DiagnosticResponseHandler(otError              aError,
                                              const otMessage     *aMessage,
                                              const otMessageInfo *aMessageInfo)
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

void RestWebServer::Init(const std::string &aRestListenAddress, int aRestListenPort)
{
    mServerThread = std::thread([aRestListenAddress, aRestListenPort, this]() -> void {
        otbrLogInfo("RestWebServer listening on %s:%u", aRestListenAddress.c_str(), aRestListenPort);
        mServer.set_ipv6_v6only(false);
        mServer.listen(aRestListenAddress, aRestListenPort);
    });
}

} // namespace rest
} // namespace otbr
