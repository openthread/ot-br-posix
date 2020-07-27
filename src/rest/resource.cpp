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

#include <chrono>
#include <string>

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
#define OT_REST_RESOURCE_NOT_FOUND "404 Not Found"

using std::chrono::duration_cast;
using std::chrono::microseconds;
using std::chrono::steady_clock;
namespace otbr {
namespace rest {

static const char *   kMulticastAddrAllRouters = "ff02::2";
static const uint8_t  kAllTlvTypes[]           = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 14, 15, 16, 17, 18, 19};
//  Timeout for delete outdated diagnostics in Microseconds 
static const uint32_t kDiagResetTimeout        = 5000000;

Resource::Resource(ControllerOpenThread *aNcp)
    : mNcp(aNcp)
    , mInstance(aNcp->GetThreadHelper()->GetInstance())
{
    mResourceMap.insert( std::make_pair( OT_DIAGNOETIC_PATH, &Resource::Diagnostic ));
    mResourceMap.insert( std::make_pair( OT_NODE_PATH, &Resource::NodeInfo ));
    mResourceMap.insert( std::make_pair( OT_STATE_PATH, &Resource::State ));
    mResourceMap.insert( std::make_pair( OT_EXTADDRESS_PATH, &Resource::ExtendedAddr ));
    mResourceMap.insert( std::make_pair( OT_NETWORKNAME_PATH, &Resource::NetworkName ));
    mResourceMap.insert( std::make_pair( OT_RLOC16_PATH, &Resource::Rloc16 ));
    mResourceMap.insert( std::make_pair( OT_LEADERDATA_PATH, &Resource::LeaderData ));
    mResourceMap.insert( std::make_pair( OT_NUMOFROUTER_PATH, &Resource::NumOfRoute ));
    mResourceMap.insert( std::make_pair( OT_EXTPANID_PATH, &Resource::ExtendedPanId ));
    mResourceMap.insert( std::make_pair( OT_RLOC_PATH, &Resource::Rloc ));
}

void Resource::Init()
{   
    otThreadSetReceiveDiagnosticGetCallback(mInstance, &Resource::DiagnosticResponseHandler, this);
}

void Resource::Handle(Request &aRequest, Response &aResponse)
{
    std::string url = aRequest.GetUrl();
    auto it = mResourceMap.find(url);
    if (it != mResourceMap.end())
    {

        ResourceHandler  resourceHandler= it->second;
        (this->*resourceHandler)(aRequest,aResponse);
    }
    else
    {
        ErrorHandler(aRequest,aResponse);
    }
}

void Resource::HandleCallback(Request &aRequest, Response &aResponse)
{
    OT_UNUSED_VARIABLE(aRequest);
    std::vector<std::vector<otNetworkDiagTlv>> diagTestSet;

    DeleteOutDatedDiag();
    
    for (auto it = mDiagSet.begin(); it != mDiagSet.end(); ++it){
        diagTestSet.push_back(it->second->mDiagContent);
    }

    
    std::string bodyTest = JSON::Diag2JsonString(diagTestSet);
    
    aResponse.SetBody(bodyTest);
}

void Resource::ErrorHandler(Request &aRequest, Response &aResponse)
{
    OT_UNUSED_VARIABLE(aRequest);
    std::string str = JSON::String2JsonString(aRequest.GetUrl());
    aResponse.SetBody(str);
}

void Resource::NodeInfo(Request &aRequest, Response &aResponse)
{
    OT_UNUSED_VARIABLE(aRequest);
    
    Node node;
    otRouterInfo routerInfo;
    int maxRouterId;
    std::string str;

    node.role = otThreadGetDeviceRole(mInstance);
    node.extAddress = reinterpret_cast<const uint8_t *>(otLinkGetExtendedAddress(mInstance));
    node.networkName = otThreadGetNetworkName(mInstance);
    otThreadGetLeaderData(mInstance, &node.leaderData);
    node.rloc16 =  otThreadGetRloc16(mInstance);
    node.extPanId = reinterpret_cast<const uint8_t *>(otThreadGetExtendedPanId(mInstance));
    node.rlocAddress = *otThreadGetRloc(mInstance);
    node.numOfRouter = 0;
    maxRouterId = otThreadGetMaxRouterId(mInstance);
    

    for (uint8_t i = 0; i <= maxRouterId; ++i)
    {
        if (otThreadGetRouterInfo(mInstance, i, &routerInfo) != OT_ERROR_NONE)
        {
            continue;
        }
        ++node.numOfRouter;
    }

    str = JSON::Node2JsonString(node);

    aResponse.SetBody(str);
}

void Resource::ExtendedAddr(Request &aRequest, Response &aResponse)
{
    OT_UNUSED_VARIABLE(aRequest);
    std::string str = GetDataExtendedAddr();
    aResponse.SetBody(str);
}

void Resource::State(Request &aRequest, Response &aResponse)
{
    OT_UNUSED_VARIABLE(aRequest);
    std::string str = GetDataState();
    aResponse.SetBody(str);
}

void Resource::NetworkName(Request &aRequest, Response &aResponse)
{
    OT_UNUSED_VARIABLE(aRequest);
    std::string str = GetDataNetworkName();
    aResponse.SetBody(str);
}

void Resource::LeaderData(Request &aRequest, Response &aResponse)
{
    OT_UNUSED_VARIABLE(aRequest);
    std::string str = GetDataLeaderData();
    aResponse.SetBody(str);
}

void Resource::NumOfRoute(Request &aRequest, Response &aResponse)
{
    OT_UNUSED_VARIABLE(aRequest);
    std::string str = GetDataNumOfRoute();
    aResponse.SetBody(str);
}

void Resource::Rloc16(Request &aRequest, Response &aResponse)
{
    OT_UNUSED_VARIABLE(aRequest);
    std::string str = GetDataRloc16();
    aResponse.SetBody(str);
}

void Resource::ExtendedPanId(Request &aRequest, Response &aResponse)
{
    OT_UNUSED_VARIABLE(aRequest);
    std::string str = GetDataExtendedPanId();
    aResponse.SetBody(str);
}

void Resource::Rloc(Request &aRequest, Response &aResponse)
{
    OT_UNUSED_VARIABLE(aRequest);
    std::string str = GetDataRloc();
    aResponse.SetBody(str);
}

std::string Resource::GetDataExtendedAddr()
{
    const uint8_t *extAddress = reinterpret_cast<const uint8_t *>(otLinkGetExtendedAddress(mInstance));
    
    std::string    str        = JSON::Bytes2HexJsonString(extAddress, OT_EXT_ADDRESS_SIZE);
    
    return str;
}

std::string Resource::GetDataState()
{
    std::string state;

    // 0 : disabled
    // 1 : detached
    // 2 : child
    // 3 : router
    // 4 : leader  
    
    auto role = otThreadGetDeviceRole(mInstance);

    state  = JSON::Number2JsonString(role);

    return state;
}

void Resource::DeleteOutDatedDiag()
{
    auto eraseIt  = mDiagSet.begin();
    for (eraseIt = mDiagSet.begin(); eraseIt != mDiagSet.end();)
    {
        auto diagInfo = eraseIt->second.get();
        auto duration = duration_cast<microseconds>(steady_clock::now() - diagInfo->mStartTime).count();

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

void Resource::UpdateDiag(std::string aKey, std::vector<otNetworkDiagTlv> aDiag)
{   
    mDiagSet[aKey] = std::unique_ptr<DiagInfo>(new DiagInfo(steady_clock::now(),aDiag));
}

std::string Resource::GetDataNetworkName()
{   
    std::string str;
    const char *networkName = otThreadGetNetworkName(mInstance);
    
    str = JSON::CString2JsonString(networkName);
    return str;
}

std::string Resource::GetDataLeaderData()
{
    otLeaderData leaderData;
    std::string  str;
    
    otThreadGetLeaderData(mInstance, &leaderData);
    str = JSON::LeaderData2JsonString(leaderData);
    
    return str;
}

std::string Resource::GetDataNumOfRoute()
{
    int          count = 0;
    uint8_t      maxRouterId;
    otRouterInfo routerInfo;
    maxRouterId = otThreadGetMaxRouterId(mInstance);
    std::string str;

    for (uint8_t i = 0; i <= maxRouterId; ++i)
    {
        if (otThreadGetRouterInfo(mInstance, i, &routerInfo) != OT_ERROR_NONE)
        {
            continue;
        }
        ++count;
    }
    
    str = JSON::Number2JsonString(count);
    return str;
}

std::string Resource::GetDataRloc16()
{
    uint16_t    rloc16 = otThreadGetRloc16(mInstance);
    std::string str;
    
    str = JSON::Number2JsonString(rloc16);
    
    return str;
}

std::string Resource::GetDataExtendedPanId()
{
    const uint8_t *extPanId = reinterpret_cast<const uint8_t *>(otThreadGetExtendedPanId(mInstance));
    
    std::string    str      = JSON::Bytes2HexJsonString(extPanId, OT_EXT_PAN_ID_SIZE);

    return str;
}

std::string Resource::GetDataRloc()
{
    struct otIp6Address rlocAddress = *otThreadGetRloc(mInstance);
   
    std::string         str;
    
    str = JSON::IpAddr2JsonString(rlocAddress);
    
    return str;
}

void Resource::Diagnostic(Request &aRequest, Response &aResponse)
{
    OT_UNUSED_VARIABLE(aRequest);
    aResponse.SetCallback();

    struct otIp6Address rloc16address = *otThreadGetRloc(mInstance);
    struct otIp6Address multicastAddress;
    otIp6AddressFromString(kMulticastAddrAllRouters, &multicastAddress);

    otThreadSendDiagnosticGet(mInstance, &rloc16address, kAllTlvTypes, sizeof(kAllTlvTypes));
    otThreadSendDiagnosticGet(mInstance, &multicastAddress, kAllTlvTypes, sizeof(kAllTlvTypes));
}

void Resource::DiagnosticResponseHandler(otMessage *aMessage, const otMessageInfo *aMessageInfo, void *aContext)
{
    static_cast<Resource *>(aContext)->DiagnosticResponseHandler(aMessage,
                                                                 *static_cast<const otMessageInfo *>(aMessageInfo));
}

void Resource::DiagnosticResponseHandler(otMessage *aMessage, const otMessageInfo)
{   
    std::vector<otNetworkDiagTlv> diagSet;
    otNetworkDiagTlv      diagTlv;
    otNetworkDiagIterator iterator = OT_NETWORK_DIAGNOSTIC_ITERATOR_INIT;
    otError               error    = OT_ERROR_NONE;
    char rloc[7];
    std::string              keyRloc = "0xffee";
    
    while ((error = otThreadGetNextDiagnosticTlv(aMessage, &iterator, &diagTlv)) == OT_ERROR_NONE)
    {   if (diagTlv.mType == OT_NETWORK_DIAGNOSTIC_TLV_SHORT_ADDRESS)
        {
            sprintf(rloc, "0x%04x", diagTlv.mData.mAddr16);
            keyRloc   = JSON::CString2JsonString(rloc);
        }
        diagSet.push_back(diagTlv);
    }
        
    UpdateDiag(keyRloc,diagSet);

}

} // namespace rest
} // namespace otbr
