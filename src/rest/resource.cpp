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

#include "rest/types.hpp"
#define OTBR_LOG_TAG "REST"

#include <httplib.h>

#include "rest/resource.hpp"

#include <openthread/commissioner.h>

#include "common/api_strings.hpp"
#include "rest/json.hpp"

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

#define VerifyOrReturn(aCondition, aReturn) \
    do                                      \
    {                                       \
        if (!(aCondition))                  \
        {                                   \
            return (aReturn);               \
        }                                   \
    } while (false)

Server::Handler Resource::MakeHandler(RequestHandler aHandler)
{
    return std::bind(aHandler, this, std::placeholders::_1, std::placeholders::_2);
}

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

    return HttpMethod::kBadMethod;
}

Resource::Resource(Server &aServer, RcpHost *aHost)
    : mInstance(nullptr)
    , mHost(aHost)
{
    // Resource Handler
    aServer.Get(OT_REST_RESOURCE_PATH_DIAGNOSTICS, MakeHandler(&Resource::Diagnostic));
    aServer.Get(OT_REST_RESOURCE_PATH_NODE, MakeHandler(&Resource::NodeInfo));
    aServer.Delete(OT_REST_RESOURCE_PATH_NODE, MakeHandler(&Resource::NodeInfo));
    aServer.Get(OT_REST_RESOURCE_PATH_NODE_BAID, MakeHandler(&Resource::BaId));
    aServer.Get(OT_REST_RESOURCE_PATH_NODE_STATE, MakeHandler(&Resource::State));
    aServer.Put(OT_REST_RESOURCE_PATH_NODE_STATE, MakeHandler(&Resource::State));
    aServer.Options(OT_REST_RESOURCE_PATH_NODE_STATE, MakeHandler(&Resource::State));
    aServer.Get(OT_REST_RESOURCE_PATH_NODE_EXTADDRESS, MakeHandler(&Resource::ExtendedAddr));
    aServer.Get(OT_REST_RESOURCE_PATH_NODE_NETWORKNAME, MakeHandler(&Resource::NetworkName));
    aServer.Get(OT_REST_RESOURCE_PATH_NODE_RLOC16, MakeHandler(&Resource::Rloc16));
    aServer.Get(OT_REST_RESOURCE_PATH_NODE_LEADERDATA, MakeHandler(&Resource::LeaderData));
    aServer.Get(OT_REST_RESOURCE_PATH_NODE_NUMOFROUTER, MakeHandler(&Resource::NumOfRoute));
    aServer.Get(OT_REST_RESOURCE_PATH_NODE_EXTPANID, MakeHandler(&Resource::ExtendedPanId));
    aServer.Get(OT_REST_RESOURCE_PATH_NODE_RLOC, MakeHandler(&Resource::Rloc));
    aServer.Get(OT_REST_RESOURCE_PATH_NODE_DATASET_ACTIVE, MakeHandler(&Resource::DatasetActive));
    aServer.Put(OT_REST_RESOURCE_PATH_NODE_DATASET_ACTIVE, MakeHandler(&Resource::DatasetActive));
    aServer.Options(OT_REST_RESOURCE_PATH_NODE_DATASET_ACTIVE, MakeHandler(&Resource::DatasetActive));
    aServer.Get(OT_REST_RESOURCE_PATH_NODE_DATASET_PENDING, MakeHandler(&Resource::DatasetPending));
    aServer.Put(OT_REST_RESOURCE_PATH_NODE_DATASET_PENDING, MakeHandler(&Resource::DatasetPending));
    aServer.Options(OT_REST_RESOURCE_PATH_NODE_DATASET_PENDING, MakeHandler(&Resource::DatasetPending));
    aServer.Get(OT_REST_RESOURCE_PATH_NODE_COMMISSIONER_STATE, MakeHandler(&Resource::CommissionerState));
    aServer.Put(OT_REST_RESOURCE_PATH_NODE_COMMISSIONER_STATE, MakeHandler(&Resource::CommissionerState));
    aServer.Options(OT_REST_RESOURCE_PATH_NODE_COMMISSIONER_STATE, MakeHandler(&Resource::CommissionerState));
    aServer.Get(OT_REST_RESOURCE_PATH_NODE_COMMISSIONER_JOINER, MakeHandler(&Resource::CommissionerJoiner));
    aServer.Post(OT_REST_RESOURCE_PATH_NODE_COMMISSIONER_JOINER, MakeHandler(&Resource::CommissionerJoiner));
    aServer.Delete(OT_REST_RESOURCE_PATH_NODE_COMMISSIONER_JOINER, MakeHandler(&Resource::CommissionerJoiner));
    aServer.Options(OT_REST_RESOURCE_PATH_NODE_COMMISSIONER_JOINER, MakeHandler(&Resource::CommissionerJoiner));
    aServer.Get(OT_REST_RESOURCE_PATH_NODE_COPROCESSOR_VERSION, MakeHandler(&Resource::CoprocessorVersion));
}

void Resource::Init(void)
{
    mInstance = mHost->GetThreadHelper()->GetInstance();
}

void Resource::ErrorHandler(Response &aResponse, StatusCode aErrorCode) const
{
    std::string errorMessage = status_message(aErrorCode);
    std::string body         = Json::Error2JsonString(aErrorCode, errorMessage);

    aResponse.status = aErrorCode;
    aResponse.set_content(body, "application/json");
}

void Resource::GetNodeInfo(Response &aResponse) const
{
    otbrError   error = OTBR_ERROR_NONE;
    std::string body;

    SuccessOrExit(error = RunInMainLoop([this, &body]() -> otbrError {
                      struct NodeInfo node = {};
                      otRouterInfo    routerInfo;
                      uint8_t         maxRouterId;

                      if (otBorderAgentGetId(mInstance, &node.mBaId) != OT_ERROR_NONE)
                          return OTBR_ERROR_REST;

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

                      return OTBR_ERROR_NONE;
                  }));

    aResponse.set_content(body, "application/json");

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

void Resource::DeleteNodeInfo(Response &aResponse) const
{
    otbrError   error = OTBR_ERROR_NONE;
    std::string errorCode;

    VerifyOrExit(mHost->GetThreadHelper()->Detach() == OT_ERROR_NONE, error = OTBR_ERROR_INVALID_STATE);
    VerifyOrExit(RunInMainLoop(otInstanceErasePersistentInfo, mInstance) == OT_ERROR_NONE, error = OTBR_ERROR_REST);
    mHost->Reset();

exit:
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

void Resource::NodeInfo(const Request &aRequest, Response &aResponse) const
{
    std::string errorCode;

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

void Resource::GetDataBaId(Response &aResponse) const
{
    otbrError       error = OTBR_ERROR_NONE;
    otBorderAgentId id;
    std::string     body;
    std::string     errorCode;

    VerifyOrExit(RunInMainLoop(otBorderAgentGetId, mInstance, &id) == OT_ERROR_NONE, error = OTBR_ERROR_REST);

    body = Json::Bytes2HexJsonString(id.mId, sizeof(id));
    aResponse.set_content(body, "application/json");

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

void Resource::BaId(const Request &aRequest, Response &aResponse) const
{
    std::string errorCode;

    if (GetMethod(aRequest) == HttpMethod::kGet)
    {
        GetDataBaId(aResponse);
    }
    else
    {
        ErrorHandler(aResponse, StatusCode::MethodNotAllowed_405);
    }
}

void Resource::GetDataExtendedAddr(Response &aResponse) const
{
    const uint8_t *extAddress = reinterpret_cast<const uint8_t *>(RunInMainLoop(otLinkGetExtendedAddress, mInstance));
    std::string    body       = Json::Bytes2HexJsonString(extAddress, OT_EXT_ADDRESS_SIZE);

    aResponse.set_content(body, "application/json");
    aResponse.status = StatusCode::OK_200;
}

void Resource::ExtendedAddr(const Request &aRequest, Response &aResponse) const
{
    std::string errorCode;

    if (GetMethod(aRequest) == HttpMethod::kGet)
    {
        GetDataExtendedAddr(aResponse);
    }
    else
    {
        ErrorHandler(aResponse, StatusCode::MethodNotAllowed_405);
    }
}

void Resource::GetDataState(Response &aResponse) const
{
    std::string  state;
    std::string  errorCode;
    otDeviceRole role;

    role  = RunInMainLoop(otThreadGetDeviceRole, mInstance);
    state = Json::String2JsonString(GetDeviceRoleName(role));
    aResponse.set_content(state, "application/json");
    aResponse.status = StatusCode::OK_200;
}

void Resource::SetDataState(const Request &aRequest, Response &aResponse) const
{
    otbrError   error = OTBR_ERROR_NONE;
    std::string errorCode;
    std::string body;

    VerifyOrExit(Json::JsonString2String(aRequest.body, body), error = OTBR_ERROR_INVALID_ARGS);
    SuccessOrExit(
        error = RunInMainLoop([this, &body]() {
            if (body == "enable")
            {
                if (!otIp6IsEnabled(mInstance))
                {
                    VerifyOrReturn(otIp6SetEnabled(mInstance, true) == OT_ERROR_NONE, OTBR_ERROR_INVALID_STATE);
                }
                VerifyOrReturn(otThreadSetEnabled(mInstance, true) == OT_ERROR_NONE, OTBR_ERROR_INVALID_STATE);
            }
            else if (body == "disable")
            {
                VerifyOrReturn(otThreadSetEnabled(mInstance, false) == OT_ERROR_NONE, OTBR_ERROR_INVALID_STATE);
                VerifyOrReturn(otIp6SetEnabled(mInstance, false) == OT_ERROR_NONE, OTBR_ERROR_INVALID_STATE);
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

void Resource::State(const Request &aRequest, Response &aResponse) const
{
    std::string errorCode;

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

void Resource::GetDataNetworkName(Response &aResponse) const
{
    std::string networkName;
    std::string errorCode;

    networkName = RunInMainLoop(otThreadGetNetworkName, mInstance);
    auto body   = Json::String2JsonString(networkName);

    aResponse.set_content(body, "application/json");
    aResponse.status = StatusCode::OK_200;
}

void Resource::NetworkName(const Request &aRequest, Response &aResponse) const
{
    std::string errorCode;

    if (GetMethod(aRequest) == HttpMethod::kGet)
    {
        GetDataNetworkName(aResponse);
    }
    else
    {
        ErrorHandler(aResponse, StatusCode::MethodNotAllowed_405);
    }
}

void Resource::GetDataLeaderData(Response &aResponse) const
{
    otbrError    error = OTBR_ERROR_NONE;
    otLeaderData leaderData;
    std::string  body;
    std::string  errorCode;

    VerifyOrExit(RunInMainLoop(otThreadGetLeaderData, mInstance, &leaderData) == OT_ERROR_NONE,
                 error = OTBR_ERROR_REST);

    body = Json::LeaderData2JsonString(leaderData);

    aResponse.set_content(body, "application/json");

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

void Resource::LeaderData(const Request &aRequest, Response &aResponse) const
{
    std::string errorCode;
    if (GetMethod(aRequest) == HttpMethod::kGet)
    {
        GetDataLeaderData(aResponse);
    }
    else
    {
        ErrorHandler(aResponse, StatusCode::MethodNotAllowed_405);
    }
}

void Resource::GetDataNumOfRoute(Response &aResponse) const
{
    std::string body = RunInMainLoop([this]() {
        uint8_t      count = 0;
        uint8_t      maxRouterId;
        otRouterInfo routerInfo;
        maxRouterId = otThreadGetMaxRouterId(mInstance);
        for (uint8_t i = 0; i <= maxRouterId; ++i)
        {
            if (otThreadGetRouterInfo(mInstance, i, &routerInfo) != OT_ERROR_NONE)
            {
                continue;
            }
            ++count;
        }

        return Json::Number2JsonString(count);
    });

    aResponse.set_content(body, "application/json");
    aResponse.status = StatusCode::OK_200;
}

void Resource::NumOfRoute(const Request &aRequest, Response &aResponse) const
{
    std::string errorCode;

    if (GetMethod(aRequest) == HttpMethod::kGet)
    {
        GetDataNumOfRoute(aResponse);
    }
    else
    {
        ErrorHandler(aResponse, StatusCode::MethodNotAllowed_405);
    }
}

void Resource::GetDataRloc16(Response &aResponse) const
{
    uint16_t    rloc16 = RunInMainLoop(otThreadGetRloc16, mInstance);
    std::string body;

    body = Json::Number2JsonString(rloc16);

    aResponse.set_content(body, "application/json");
    aResponse.status = StatusCode::OK_200;
}

void Resource::Rloc16(const Request &aRequest, Response &aResponse) const
{
    std::string errorCode;

    if (GetMethod(aRequest) == HttpMethod::kGet)
    {
        GetDataRloc16(aResponse);
    }
    else
    {
        ErrorHandler(aResponse, StatusCode::MethodNotAllowed_405);
    }
}

void Resource::GetDataExtendedPanId(Response &aResponse) const
{
    const uint8_t *extPanId = reinterpret_cast<const uint8_t *>(RunInMainLoop(otThreadGetExtendedPanId, mInstance));
    std::string    body     = Json::Bytes2HexJsonString(extPanId, OT_EXT_PAN_ID_SIZE);
    std::string    errorCode;

    aResponse.set_content(body, "application/json");
    aResponse.status = StatusCode::OK_200;
}

void Resource::ExtendedPanId(const Request &aRequest, Response &aResponse) const
{
    std::string errorCode;

    if (GetMethod(aRequest) == HttpMethod::kGet)
    {
        GetDataExtendedPanId(aResponse);
    }
    else
    {
        ErrorHandler(aResponse, StatusCode::MethodNotAllowed_405);
    }
}

void Resource::GetDataRloc(Response &aResponse) const
{
    otIp6Address rlocAddress = *RunInMainLoop(otThreadGetRloc, mInstance);
    std::string  body;

    body = Json::IpAddr2JsonString(rlocAddress);

    aResponse.set_content(body, "application/json");
    aResponse.status = StatusCode::OK_200;
}

void Resource::Rloc(const Request &aRequest, Response &aResponse) const
{
    std::string errorCode;

    if (GetMethod(aRequest) == HttpMethod::kGet)
    {
        GetDataRloc(aResponse);
    }
    else
    {
        ErrorHandler(aResponse, StatusCode::MethodNotAllowed_405);
    }
}

void Resource::GetDataset(DatasetType aDatasetType, const Request &aRequest, Response &aResponse) const
{
    otbrError   error = OTBR_ERROR_NONE;
    std::string body;
    std::string contentType = "application/json";

    SuccessOrExit(
        error = RunInMainLoop([this, aDatasetType, &contentType, &aRequest, &body]() {
            if (aRequest.get_header_value(OT_REST_ACCEPT_HEADER) == OT_REST_CONTENT_TYPE_PLAIN)
            {
                otOperationalDatasetTlvs datasetTlvs;

                if (aDatasetType == DatasetType::kActive)
                {
                    VerifyOrReturn(otDatasetGetActiveTlvs(mInstance, &datasetTlvs) == OT_ERROR_NONE,
                                   OTBR_ERROR_NOT_FOUND);
                }
                else if (aDatasetType == DatasetType::kPending)
                {
                    VerifyOrReturn(otDatasetGetPendingTlvs(mInstance, &datasetTlvs) == OT_ERROR_NONE,
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
                    VerifyOrReturn(otDatasetGetActive(mInstance, &dataset) == OT_ERROR_NONE, OTBR_ERROR_NOT_FOUND);
                    body = Json::ActiveDataset2JsonString(dataset);
                }
                else if (aDatasetType == DatasetType::kPending)
                {
                    VerifyOrReturn(otDatasetGetPending(mInstance, &dataset) == OT_ERROR_NONE, OTBR_ERROR_NOT_FOUND);
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

void Resource::SetDataset(DatasetType aDatasetType, const Request &aRequest, Response &aResponse) const
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
                VerifyOrReturn(otThreadGetDeviceRole(mInstance) == OT_DEVICE_ROLE_DISABLED, OTBR_ERROR_INVALID_STATE);
                errorOt = otDatasetGetActiveTlvs(mInstance, &datasetTlvs);
            }
            else if (aDatasetType == DatasetType::kPending)
            {
                errorOt = otDatasetGetPendingTlvs(mInstance, &datasetTlvs);
            }

            // Create a new operational dataset if it doesn't exist.
            if (errorOt == OT_ERROR_NOT_FOUND)
            {
                VerifyOrReturn(otDatasetCreateNewNetwork(mInstance, &dataset) == OT_ERROR_NONE, OTBR_ERROR_REST);
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
                VerifyOrReturn(otDatasetSetActiveTlvs(mInstance, &datasetTlvs) == OT_ERROR_NONE, OTBR_ERROR_REST);
            }
            else if (aDatasetType == DatasetType::kPending)
            {
                VerifyOrReturn(otDatasetSetPendingTlvs(mInstance, &datasetTlvs) == OT_ERROR_NONE, OTBR_ERROR_REST);
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

void Resource::Dataset(DatasetType aDatasetType, const Request &aRequest, Response &aResponse) const
{
    std::string errorCode;

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
    otCommissionerState stateCode;

    stateCode = RunInMainLoop(otCommissionerGetState, mInstance);
    state     = Json::String2JsonString(GetCommissionerStateName(stateCode));
    aResponse.set_content(state, "application/json");
    aResponse.status = StatusCode::OK_200;
}

void Resource::SetCommissionerState(const Request &aRequest, Response &aResponse) const
{
    otbrError   error = OTBR_ERROR_NONE;
    std::string body;

    VerifyOrExit(Json::JsonString2String(aRequest.body, body), error = OTBR_ERROR_INVALID_ARGS);
    SuccessOrExit(
        error = RunInMainLoop([this, &body]() {
            if (body == "enable")
            {
                VerifyOrReturn(otCommissionerGetState(mInstance) == OT_COMMISSIONER_STATE_DISABLED, OTBR_ERROR_NONE);
                VerifyOrReturn(otCommissionerStart(mInstance, NULL, NULL, NULL) == OT_ERROR_NONE,
                               OTBR_ERROR_INVALID_STATE);
            }
            else if (body == "disable")
            {
                VerifyOrReturn(otCommissionerGetState(mInstance) != OT_COMMISSIONER_STATE_DISABLED, OTBR_ERROR_NONE);
                VerifyOrReturn(otCommissionerStop(mInstance) == OT_ERROR_NONE, OTBR_ERROR_INVALID_STATE);
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

void Resource::CommissionerState(const Request &aRequest, Response &aResponse) const
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

void Resource::GetJoiners(Response &aResponse) const
{
    std::string body;

    body = RunInMainLoop([this]() {
        std::vector<otJoinerInfo> joinerTable;
        otJoinerInfo              joinerInfo;
        uint16_t                  iter = 0;

        while (otCommissionerGetNextJoinerInfo(mInstance, &iter, &joinerInfo) == OT_ERROR_NONE)
        {
            joinerTable.push_back(joinerInfo);
        }

        return Json::JoinerTable2JsonString(joinerTable);
    });

    aResponse.set_content(body, "application/json");
    aResponse.status = StatusCode::OK_200;
}

void Resource::AddJoiner(const Request &aRequest, Response &aResponse) const
{
    otbrError error   = OTBR_ERROR_NONE;
    otError   errorOt = OT_ERROR_NONE;

    SuccessOrExit(
        error = RunInMainLoop([this, &errorOt, &aRequest]() {
            otJoinerInfo        joiner;
            const otExtAddress *addrPtr                         = nullptr;
            const uint8_t       emptyArray[OT_EXT_ADDRESS_SIZE] = {0};

            VerifyOrReturn(otCommissionerGetState(mInstance) == OT_COMMISSIONER_STATE_ACTIVE, OTBR_ERROR_INVALID_STATE);

            VerifyOrReturn(Json::JsonJoinerInfoString2JoinerInfo(aRequest.body, joiner), OTBR_ERROR_INVALID_ARGS);

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

void Resource::CommissionerJoiner(const Request &aRequest, Response &aResponse) const
{
    std::string errorCode;

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

void Resource::GetCoprocessorVersion(Response &aResponse) const
{
    std::string coprocessorVersion;
    std::string errorCode;

    coprocessorVersion = mHost->GetCoprocessorVersion();
    coprocessorVersion = Json::String2JsonString(coprocessorVersion);

    aResponse.set_content(coprocessorVersion, "application/json");
    aResponse.status = StatusCode::OK_200;
}

void Resource::CoprocessorVersion(const Request &aRequest, Response &aResponse) const
{
    std::string errorCode;

    if (GetMethod(aRequest) == HttpMethod::kGet)
    {
        GetCoprocessorVersion(aResponse);
    }
    else
    {
        ErrorHandler(aResponse, StatusCode::MethodNotAllowed_405);
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
        // TODO aResponse.SetStartTime(steady_clock::now());
        // aResponse.SetCallback();
    }
    else
    {
        ErrorHandler(aResponse, StatusCode::InternalServerError_500);
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
