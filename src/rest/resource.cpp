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

static const char *  kMulticastAddrAllRouters = "ff02::2";
static const uint8_t kAllTlvTypes[]           = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 14, 15, 16, 17, 18, 19};
//  Timeout(in Microseconds) for delete outdated diagnostics
static const uint32_t kDiagResetTimeout = 3000000;
static const uint32_t kDiagCollectTimeout = 2000000;
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
    mResourceMap.emplace(OT_NETWORK_PATH, &Resource::Networks);
    mResourceMap.emplace(OT_NETWORK_CURRENT_PATH, &Resource::CurrentNetwork);
    mResourceMap.emplace(OT_NETWORK_CURRENT_PREFIX_PATH, &Resource::CurrentNetworkPrefix);
    mResourceMap.emplace(OT_NETWORK_CURRENT_COMMISSION_PATH, &Resource::CurrentNetworkCommission);
    

    mResourceCallbackMap.emplace(OT_DIAGNOETIC_PATH, &Resource::HandleDiagnosticCallback);
    mResourceCallbackMap.emplace(OT_NETWORK_PATH, &Resource::PostNetworksCallback);

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
    // else
    // {   
    //     // otbrLog(OTBR_LOG_ERR, "body %s", aResponse.GetBody().c_str() );
    //     if ( !aResponse.IsComplete() )
    //     {
    //        ErrorHandler(aResponse, 404);
            
    //     }
        
    // }
}

void Resource::HandleDiagnosticCallback(const Request &aRequest, Response &aResponse)
{
    
    OT_UNUSED_VARIABLE(aRequest);
    std::vector<std::vector<otNetworkDiagTlv>> diagContentSet;
    std::string                                body;
    std::string                                errorCode;

    auto  duration = duration_cast<microseconds>(steady_clock::now() - aResponse.GetStartTime()).count();
    if (duration >= kDiagCollectTimeout )
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

void Resource::CurrentNetworkPrefix(const Request &aRequest, Response &aResponse)
{
    std::string errorCode;
   
    if (aRequest.GetMethod() ==  OTBR_REST_METHOD_POST)
    {
        PostCurrentNetwork(aResponse);
    }
    else if (aRequest.GetMethod() ==  OTBR_REST_METHOD_DELETE)
    {
        DeleteCurrentNetwork(aResponse);
    }

    else if (aRequest.GetMethod() ==  OTBR_REST_METHOD_OPTIONS)
    {
        ErrorHandler(aResponse, 200);
    }
    else
    {
        ErrorHandler(aResponse, 405);
    }

}

void Resource::PostCurrentNetworkPrefix(Response &aResponse)
{
    std::string errorCode;
    
    PostError error = OTBR_REST_POST_ERROR_NONE;
    
    std::string requestBody;
    
    std::string prefix;
    bool defaultRoute;
    
    otBorderRouterConfig config;
    
    
    requestBody = aRequest.GetBody();
    
    defaultRoute = JSON::JsonString2Bool(requestBody, std::string("defaultRoute"));
    prefix = JSON::JsonString2String(requestBody, std::string("prefix"));

    // Add prefix
    memset(&config, 0, sizeof(otBorderRouterConfig));

    VerifyOrExit(otIp6AddressFromString(prefix.c_str(), &config.mPrefix.mPrefix) == OT_ERROR_NONE, error = OTBR_REST_POST_BAD_REQUEST);
    
    config.mPreferred = true;
    config.mSlaac = true;
    config.mStable = true;
    config.mOnMesh = true;
    config.mDefaultRoute = defaultRoute;
    config.mPrefix.mLength = 64;

    VerifyOrExit(otBorderRouterAddOnMeshPrefix(mInstance, &config) == OT_ERROR_NONE, error = OTBR_REST_POST_SET_FAIL);

    
   

exit:
    
    if (  error == OTBR_REST_POST_ERROR_NONE)
    {   
            aResponse.SetBody(requestBody);
            errorCode = mResponseCodeMap[200];
            aResponse.SetResponsCode(errorCode);
            aResponse.SetComplete();
    }
    else if (error == OTBR_REST_POST_SET_FAIL)
    {
        ErrorHandler(aResponse , 500);
    }
    else 
    {  
        ErrorHandler(aResponse , 400);
    }

}

void Resource::DeleteCurrentNetworkPrefix(Response &aResponse)
{
    std::string errorCode;
    
    PostError error = OTBR_REST_POST_ERROR_NONE;
    
    std::string requestBody;
    
    std::string value;
    
    struct otIp6Prefix prefix;
    
    memset(&prefix, 0, sizeof(otIp6Prefix));
    
    requestBody = aRequest.GetBody();
    
    value = JSON::JsonString2String(requestBody, std::string("prefix"));

    VerifyOrExit(otIp6AddressFromString(value.c_str(), &prefix.mPrefix) == OT_ERROR_NONE, error = OTBR_REST_POST_BAD_REQUEST);
    
    prefix.mLength = 64;

    VerifyOrExit(otBorderRouterRemoveOnMeshPrefix(mInstance, &prefix) == OT_ERROR_NONE, error = OTBR_REST_POST_SET_FAIL);

exit:
    
    if (  error == OTBR_REST_POST_ERROR_NONE)
    {   
            aResponse.SetBody(requestBody);
            errorCode = mResponseCodeMap[200];
            aResponse.SetResponsCode(errorCode);
            aResponse.SetComplete();
    }
    else if (error == OTBR_REST_POST_SET_FAIL)
    {
        ErrorHandler(aResponse , 500);
    }
    else 
    {  
        ErrorHandler(aResponse , 400);
    }

}

void CurrentNetworkCommission(const Request &aRequest, Response &aResponse)
{
    std::string errorCode;
    if (aRequest.GetMethod() ==  OTBR_REST_METHOD_POST)
    {
        PostCurrentNetworkCommission(aResponse);
    }
    
    else if (aRequest.GetMethod() ==  OTBR_REST_METHOD_OPTIONS)
    {
        ErrorHandler(aResponse, 200);
    }
    else
    {
        ErrorHandler(aResponse, 405);
    }


    
}

void PostCurrentNetworkCommission( Response &aResponse){

    std::string errorCode;
    PostError error = OTBR_REST_POST_ERROR_NONE;
    std::string requestBody;
    std::string pskd;
    unsigned long timeout = kDefaultJoinerTimeout;
    
    const otExtAddress *addrPtr = nullptr;
    
    requestBody = aRequest.GetBody();
    
    pskd = JSON::JsonString2String(requestBody, std::string("pskd"));

    // otCommissionerStart(mInterpreter.mInstance, &Commissioner::HandleStateChanged,
    //                            &Commissioner::HandleJoinerEvent, this);
   

    VerifyOrExit(otCommissionerAddJoiner(mInstance, addrPtr, pskd.c_str() , static_cast<uint32_t>(timeout)) == OT_ERROR_NONE);

    
    

    
exit:
    
    if (  error == OTBR_REST_POST_ERROR_NONE)
    {   
            aResponse.SetBody(requestBody);
            errorCode = mResponseCodeMap[200];
            aResponse.SetResponsCode(errorCode);
            aResponse.SetComplete();
    }
    else if (error == OTBR_REST_POST_SET_FAIL)
    {
        ErrorHandler(aResponse , 500);
    }
    else 
    {  
        ErrorHandler(aResponse , 400);
    }


}
    

void Resource::CurrentNetwork(const Request &aRequest, Response &aResponse)
{
    std::string errorCode;
    if (aRequest.GetMethod() ==  OTBR_REST_METHOD_GET)
    {
        GetCurrentNetwork(aResponse);
    }
    else if (aRequest.GetMethod() ==  OTBR_REST_METHOD_PUT)
    {
        PutCurrentNetwork(aResponse);
        

    }
    else if (aRequest.GetMethod() ==  OTBR_REST_METHOD_OPTIONS)
    {
        ErrorHandler(aResponse, 200);
    }
    else
    {
        ErrorHandler(aResponse, 405);
    }



}


void Resource::PutCurrentNetwork(Response &aResponse)
{
    
    otInstanceFactoryReset(mInstance);
    aResponse.SetCallback();
    aResponse.SetStartTime(steady_clock::now());


}

void Resource::PutCurrentNetworkCallback(const Request &aRequest, Response &aResponse)
{
    
    std::string errorCode;
    bool complete = true;
    PostError error = OTBR_REST_POST_ERROR_NONE;
    otMasterKey     masterKey;
    std::string requestBody;
    std::string masterKey;
    std::string prefix;
    bool defaultRoute;
    
    otBorderRouterConfig config;
    
    auto  duration = duration_cast<microseconds>(steady_clock::now() - aResponse.GetStartTime()).count();
    
    VerifyOrExit(aRequest.GetMethod() == OTBR_REST_METHOD_PUT, complete = false);
    
    VerifyOrExit(duration >= kResetSleepInterval, complete = false);
    
    requestBody = aRequest.GetBody();
    
    masterKey = JSON::JsonString2String(requestBody, std::string("networkKey"));
    defaultRoute = JSON::JsonString2Bool(requestBody, std::string("defaultRoute"));
    prefix = JSON::JsonString2String(requestBody, std::string("prefix"));

    
    VerifyOrExit(otbr::Utils::Hex2Bytes(masterKey.c_str(),masterKey.m8,sizeof(masterKey.m8)) == OT_MASTER_KEY_SIZE, error = OTBR_REST_POST_BAD_REQUEST);
    // Set Master Key
    VerifyOrExit(otThreadSetMasterKey(mInstance, &masterKey) == OT_ERROR_NONE, error = OTBR_REST_POST_SET_FAIL );
    
    // IfConfig Up
    VerifyOrExit(otIp6SetEnabled(mInstance, true) == OT_ERROR_NONE, error = OTBR_REST_POST_SET_FAIL);
    // Thread start 
    VerifyOrExit(otThreadSetEnabled(mInstance, true) == OT_ERROR_NONE, error = OTBR_REST_POST_SET_FAIL);
   
    
    // Add prefix
    memset(&config, 0, sizeof(otBorderRouterConfig));

    VerifyOrExit(otIp6AddressFromString(prefix .c_str(), &config.mPrefix.mPrefix) == OT_ERROR_NONE, error = OTBR_REST_POST_BAD_REQUEST);
    
    config.mPreferred = true;
    config.mSlaac = true;
    config.mStable = true;
    config.mOnMesh = true;
    config.mDefaultRoute = defaultRoute;
    config.mPrefix.mLength = 64;

    VerifyOrExit(otBorderRouterAddOnMeshPrefix(mInstance, &config) == OT_ERROR_NONE, error = OTBR_REST_POST_SET_FAIL);

    
    otbrLog(OTBR_LOG_ERR, "post body %s", requestBody.c_str());

exit:
    if (complete)
    {
        if (  error == OTBR_REST_POST_ERROR_NONE)
        {   
            aResponse.SetBody(requestBody);
            errorCode = mResponseCodeMap[200];
            aResponse.SetResponsCode(errorCode);
            aResponse.SetComplete();
        }
        else if (error == OTBR_REST_POST_SET_FAIL)
        {
            ErrorHandler(aResponse , 500);

        }
        else 
        {  
            ErrorHandler(aResponse , 400);
        }

    }
    
    


}


void  Resource::GetNodeInfo(Response &aResponse)
{
    otbrError    error = OTBR_ERROR_NONE;
    Node         node;
    otRouterInfo routerInfo;
    uint8_t      maxRouterId;
    std::string  body;
    otOperationalDataset dataset;
    std::string errorCode;
    
    node.meshLocalAddress = *otThreadGetMeshLocalEid(mInstance);
    
    VerifyOrExit(otDatasetGetActive(mInstance, &dataset) == OT_ERROR_NONE, error = OTBR_ERROR_REST);
    node.meshLocalPrefix = dataset.mMeshLocalPrefix.m8;
    
    VerifyOrExit(otThreadGetLeaderData(mInstance, &node.leaderData) == OT_ERROR_NONE, error = OTBR_ERROR_REST);

    maxRouterId      = otThreadGetMaxRouterId(mInstance);

    for (uint8_t i = 0; i <= maxRouterId; ++i)
    {
        if (otThreadGetRouterInfo(mInstance, i, &routerInfo) != OT_ERROR_NONE)
        {
            continue;
        }
        ++node.numOfRouter;
    }

    node.panId = otLinkGetPanId(mInstance);
    node.channel = otLinkGetChannel(mInstance);
    otLinkGetFactoryAssignedIeeeEui64(mInstance, &node.eui64);
    node.version = otGetVersionString();
    node.role        = otThreadGetDeviceRole(mInstance);
    node.extAddress  = reinterpret_cast<const uint8_t *>(otLinkGetExtendedAddress(mInstance));
    node.networkName = otThreadGetNetworkName(mInstance);
    node.rloc16      = otThreadGetRloc16(mInstance);
    node.extPanId    = reinterpret_cast<const uint8_t *>(otThreadGetExtendedPanId(mInstance));
    node.rlocAddress = *otThreadGetRloc(mInstance);
    node.numOfRouter = 0;
    
    body       = JSON::Node2JsonString(node);
    
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
    if (aRequest.GetMethod() ==  OTBR_REST_METHOD_GET)
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
    std::string errorCode;
    std::string body = JSON::Bytes2HexJsonString(extAddress, OT_EXT_ADDRESS_SIZE);

    aResponse.SetBody(body);
    errorCode = mResponseCodeMap[200];
    aResponse.SetResponsCode(errorCode);

}

void Resource::ExtendedAddr(const Request &aRequest, Response &aResponse)
{
    std::string errorCode;

    if (aRequest.GetMethod() ==  OTBR_REST_METHOD_GET)
    {
        GetDataExtendedAddr(aResponse);
       
    }
    else
    {
        ErrorHandler(aResponse, 405);
    }


}

void  Resource::GetDataState(Response &aResponse)
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

    if (aRequest.GetMethod() ==  OTBR_REST_METHOD_GET)
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

    if (aRequest.GetMethod() ==  OTBR_REST_METHOD_GET)
    {
        GetDataNetworkName(aResponse);
    }
    else
    {
        ErrorHandler(aResponse, 405);
    }

}

void  Resource::GetDataLeaderData(Response &aResponse)
{
    otbrError   error = OTBR_ERROR_NONE;
    otLeaderData leaderData;
    std::string  body;
    std::string errorCode;

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
    if (aRequest.GetMethod() ==  OTBR_REST_METHOD_GET)
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

    if (aRequest.GetMethod() ==  OTBR_REST_METHOD_GET)
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

    if (aRequest.GetMethod() ==  OTBR_REST_METHOD_GET)
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
    std::string    body      = JSON::Bytes2HexJsonString(extPanId, OT_EXT_PAN_ID_SIZE);
    std::string errorCode;
    aResponse.SetBody(body);
    errorCode = mResponseCodeMap[200];
    aResponse.SetResponsCode(errorCode);

}


void Resource::ExtendedPanId(const Request &aRequest, Response &aResponse)
{
    std::string errorCode;

    if (aRequest.GetMethod() ==  OTBR_REST_METHOD_GET)
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

    otIp6Address  rlocAddress = *otThreadGetRloc(mInstance);
    std::string body;
    std::string errorCode;

    body = JSON::IpAddr2JsonString(rlocAddress);

    aResponse.SetBody(body);
    errorCode = mResponseCodeMap[200];
    aResponse.SetResponsCode(errorCode);

}

void Resource::Rloc(const Request &aRequest, Response &aResponse)
{
    std::string errorCode;

    if (aRequest.GetMethod() ==  OTBR_REST_METHOD_GET)
    {
        GetDataRloc(aResponse);
    }
    else
    {
        ErrorHandler(aResponse, 405);
    }
}

void Resource::Networks( const Request &aRequest, Response &aResponse)
{   
    std::string errorCode;
    int32_t method = aRequest.GetMethod();
    if (method == OTBR_REST_METHOD_GET)
    {   
        GetNetworks(aResponse);
    }
    else if (method == OTBR_REST_METHOD_POST)
    {
        PostNetworks(aRequest,aResponse);
    }
    else if (method == OTBR_REST_METHOD_OPTIONS)
    {
        ErrorHandler(aResponse, 200);

    }
    
    
}

void Resource::PostNetworks( const Request &aRequest, Response &aResponse)
{
    OT_UNUSED_VARIABLE(aRequest);
    otInstanceFactoryReset(mInstance);
    aResponse.SetCallback();
    aResponse.SetStartTime(steady_clock::now());

}

void Resource::PostNetworksCallback( const Request &aRequest, Response &aResponse)
{

    
    std::string errorCode;
    bool complete = true;
    PostError error = OTBR_REST_POST_ERROR_NONE;
    Network network;
    char *endptr;
    long panId;
    otPskc pskc;
    otMasterKey     masterKey;
    otExtendedPanId extPanId;
    std::string requestBody;
    otbr::Psk::Pskc      psk;
    otBorderRouterConfig config;
    char                        pskcStr[OT_PSKC_MAX_LENGTH * 2 + 1];
    uint8_t                     extPanIdBytes[OT_EXTENDED_PANID_LENGTH];
   
    auto  duration = duration_cast<microseconds>(steady_clock::now() - aResponse.GetStartTime()).count();
    
    VerifyOrExit(aRequest.GetMethod() == OTBR_REST_METHOD_POST, complete = false);
    VerifyOrExit(duration >= kResetSleepInterval, complete = false);
    
    requestBody = aRequest.GetBody();
    pskcStr[OT_PSKC_MAX_LENGTH * 2] = '\0';
    
    network = JSON::JsonString2Network(requestBody);
    otbr::Utils::Hex2Bytes(network.extPanId.c_str(), extPanIdBytes, OT_EXTENDED_PANID_LENGTH);
    otbr::Utils::Bytes2Hex(psk.ComputePskc(extPanIdBytes, network.networkName.c_str(), network.passphrase.c_str()), OT_PSKC_MAX_LENGTH,
                           pskcStr);
    otbrLog(OTBR_LOG_ERR, "finish2");
    //FactoryReset
   
    // sleep(1);
    // Format network Key
    otbrLog(OTBR_LOG_ERR, "finish3");
    
    VerifyOrExit(otbr::Utils::Hex2Bytes(network.networkKey.c_str(),masterKey.m8,sizeof(masterKey.m8)) == OT_MASTER_KEY_SIZE, error = OTBR_REST_POST_BAD_REQUEST);
    // Set Master Key
    VerifyOrExit(otThreadSetMasterKey(mInstance, &masterKey) == OT_ERROR_NONE, error = OTBR_REST_POST_SET_FAIL );
     otbrLog(OTBR_LOG_ERR, "finish4");
    // NetworkName
    VerifyOrExit(otThreadSetNetworkName(mInstance, network.networkName.c_str()) == OT_ERROR_NONE, error = OTBR_REST_POST_SET_FAIL);

    // Channel
    VerifyOrExit(otLinkSetChannel(mInstance, network.channel) == OT_ERROR_NONE, error = OTBR_REST_POST_SET_FAIL);
otbrLog(OTBR_LOG_ERR, "finish5");
    // ExtPanId
   
    
    VerifyOrExit(otbr::Utils::Hex2Bytes(network.extPanId.c_str(),extPanId.m8,sizeof(extPanId)) == sizeof(extPanId), error = OTBR_REST_POST_BAD_REQUEST);
   
    VerifyOrExit(otThreadSetExtendedPanId(mInstance, &extPanId) == OT_ERROR_NONE, error = OTBR_REST_POST_SET_FAIL);
otbrLog(OTBR_LOG_ERR, "finish6");
    // PanId
    panId = strtol(network.panId.c_str(), &endptr, 0);
    VerifyOrExit(*endptr == '\0', error = OTBR_REST_POST_BAD_REQUEST); 
    VerifyOrExit(otLinkSetPanId(mInstance, static_cast<otPanId>(panId)) == OT_ERROR_NONE, error = OTBR_REST_POST_SET_FAIL);
otbrLog(OTBR_LOG_ERR, "finish7");
    // pskc
    
    VerifyOrExit(otbr::Utils::Hex2Bytes(pskcStr, pskc.m8, sizeof(pskc)) == sizeof(pskc), error = OTBR_REST_POST_BAD_REQUEST);
    VerifyOrExit(otThreadSetPskc(mInstance, &pskc) == OT_ERROR_NONE, error = OTBR_REST_POST_SET_FAIL);
otbrLog(OTBR_LOG_ERR, "finish8");
    // IfConfig Up
    VerifyOrExit(otIp6SetEnabled(mInstance, true) == OT_ERROR_NONE, error = OTBR_REST_POST_SET_FAIL);
    // Thread start 
    VerifyOrExit(otThreadSetEnabled(mInstance, true) == OT_ERROR_NONE, error = OTBR_REST_POST_SET_FAIL);
    otbrLog(OTBR_LOG_ERR, "finish9");
    
    // Add prefix
    memset(&config, 0, sizeof(otBorderRouterConfig));
otbrLog(OTBR_LOG_ERR, "finish9 %s", network.prefix.c_str());
    VerifyOrExit(otIp6AddressFromString(network.prefix.c_str(), &config.mPrefix.mPrefix) == OT_ERROR_NONE, error = OTBR_REST_POST_BAD_REQUEST);
    otbrLog(OTBR_LOG_ERR, "finish10");
    config.mPreferred = true;
    config.mSlaac = true;
    config.mStable = true;
    config.mOnMesh = true;
    config.mDefaultRoute = network.defaultRoute;
    config.mPrefix.mLength = 64;

    VerifyOrExit(otBorderRouterAddOnMeshPrefix(mInstance, &config) == OT_ERROR_NONE, error = OTBR_REST_POST_SET_FAIL);

    
    otbrLog(OTBR_LOG_ERR, "post body %s", requestBody.c_str());

exit:
    if (complete)
    {
        if (  error == OTBR_REST_POST_ERROR_NONE)
        {   
            aResponse.SetBody(requestBody);
            errorCode = mResponseCodeMap[200];
            aResponse.SetResponsCode(errorCode);
            aResponse.SetComplete();
        }
        else if (error == OTBR_REST_POST_SET_FAIL)
        {
            ErrorHandler(aResponse , 500);

        }
        else 
        {  
            ErrorHandler(aResponse , 400);
        }

    }
    
    
    
}

void Resource::GetNetworks(Response &aResponse)
{
    aResponse.SetCallback();
    aResponse.SetStartTime(steady_clock::now());
    auto threadHelper = mNcp->GetThreadHelper();
    threadHelper->Scan(std::bind(&Resource::NetworksResponseHandler, this, &aResponse, _1, _2));
    
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
        otbrLog(OTBR_LOG_ERR, "error  here" );
        ErrorHandler(*aResponse, 500);
    }
    else
    {
        
        for (const auto &r : aResult)
        {
            ActiveScanResult result;
            strncpy(reinterpret_cast<char*>(result.mExtAddress), reinterpret_cast<const char*>(r.mExtAddress.m8),OT_EXT_ADDRESS_SIZE );
            strncpy(reinterpret_cast<char*>(result.mExtendedPanId), reinterpret_cast<const char*>(r.mExtendedPanId.m8),OT_EXT_PAN_ID_SIZE );
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

        body      = JSON::Networks2JsonString(results);
        errorCode = mResponseCodeMap[200];
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

void Resource::Diagnostic(const Request &aRequest, Response &aResponse)
{   
    otbrError error = OTBR_ERROR_NONE;
    OT_UNUSED_VARIABLE(aRequest);
    struct otIp6Address rloc16address = *otThreadGetRloc(mInstance);
    struct otIp6Address multicastAddress;

    VerifyOrExit(otThreadSendDiagnosticGet(mInstance, &rloc16address, kAllTlvTypes, sizeof(kAllTlvTypes)) == OT_ERROR_NONE, error = OTBR_ERROR_REST);
    VerifyOrExit(otIp6AddressFromString(kMulticastAddrAllRouters, &multicastAddress) == OT_ERROR_NONE, error = OTBR_ERROR_REST);
    VerifyOrExit(otThreadSendDiagnosticGet(mInstance, &multicastAddress, kAllTlvTypes, sizeof(kAllTlvTypes)) ==
            OT_ERROR_NONE, error = OTBR_ERROR_REST);
    
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
    otError                       error ;
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
