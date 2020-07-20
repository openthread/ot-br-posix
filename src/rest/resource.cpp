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

const char *   Resource::kMulticastAddrAllRouters = "ff02::2";
const uint8_t  Resource::kAllTlvTypes[]           = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 14, 15, 16, 17, 18, 19};
const uint32_t Resource::kDiagResetTimeout        = 5000000;

constexpr unsigned int str2int(const char *str, int h = 0)
{
    return !str[h] ? 5381 : (str2int(str, h + 1) * 33) ^ str[h];
}

Resource::Resource(ControllerOpenThread *aNcp)
    : mNcp(aNcp)
    , mInstance(aNcp->GetThreadHelper()->GetInstance())
{
}

void Resource::Init()
{
    otThreadSetReceiveDiagnosticGetCallback(mInstance, &Resource::DiagnosticResponseHandler, this);
}

void Resource::Handle(Request &aRequest, Response &aResponse)
{
    std::string path = aRequest.GetUrl();

    switch (str2int(path.c_str()))
    {
    case str2int(OT_DIAGNOETIC_PATH):
        Diagnostic(aRequest, aResponse);
        break;
    case str2int(OT_NODE_PATH):
        NodeInfo(aRequest, aResponse);
        break;
    case str2int(OT_STATE_PATH):
        State(aRequest, aResponse);
        break;
    case str2int(OT_EXTADDRESS_PATH):
        ExtendedAddr(aRequest, aResponse);
        break;
    case str2int(OT_NETWORKNAME_PATH):
        NetworkName(aRequest, aResponse);
        break;
    case str2int(OT_RLOC16_PATH):
        Rloc16(aRequest, aResponse);
        break;
    case str2int(OT_LEADERDATA_PATH):
        LeaderData(aRequest, aResponse);
        break;
    case str2int(OT_NUMOFROUTER_PATH):
        NumOfRoute(aRequest, aResponse);
        break;
    case str2int(OT_EXTPANID_PATH):
        ExtendedPanId(aRequest, aResponse);
        break;
    case str2int(OT_RLOC_PATH):
        Rloc(aRequest, aResponse);
        break;
    default:
        ErrorHandler(aRequest, aResponse);
        break;
    }
}

void Resource::HandleCallback(Request &aRequest, Response &aResponse)
{
    OT_UNUSED_VARIABLE(aRequest);

    DeleteOutDatedDiag();
    std::string              diagContent;
    std::vector<std::string> diag;

    for (auto it = mDiagMaintainer.begin(); it != mDiagMaintainer.end(); ++it)
    {
        diagContent = it->second->mDiagContent;
        diag.push_back(diagContent);
    }

    std::string body = JSON::Vector2JsonString(diag);
    aResponse.SetBody(body);
}

void Resource::ErrorHandler(Request &aRequest, Response &aResponse)
{
    OT_UNUSED_VARIABLE(aRequest);
    aResponse.SetResponsCode(OT_REST_RESOURCE_NOT_FOUND);
    aResponse.SetBody(OT_REST_RESOURCE_NOT_FOUND);
}

void Resource::NodeInfo(Request &aRequest, Response &aResponse)
{
    OT_UNUSED_VARIABLE(aRequest);

    std::vector<std::string> keyContainer;
    std::vector<std::string> valueContainer;
    std::string              str;

    keyContainer.push_back("state");
    valueContainer.push_back(GetDataState());

    keyContainer.push_back("networkName");
    valueContainer.push_back(GetDataNetworkName());

    keyContainer.push_back("extAddress");
    valueContainer.push_back(GetDataExtendedAddr());

    keyContainer.push_back("rloc16");
    valueContainer.push_back(GetDataRloc16());

    keyContainer.push_back("numOfRouter");
    valueContainer.push_back(GetDataNumOfRoute());

    keyContainer.push_back("leaderData");
    valueContainer.push_back(GetDataLeaderData());

    keyContainer.push_back("extPanId");
    valueContainer.push_back(GetDataExtendedPanId());

    str = JSON::TwoVector2JsonString(keyContainer, valueContainer);

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
    std::string    str        = JSON::Bytes2HexString(extAddress, OT_EXT_ADDRESS_SIZE);
    str                       = JSON::String2JsonString(str);
    return str;
}

std::string Resource::GetDataState()
{
    std::string state;

    switch (otThreadGetDeviceRole(mInstance))
    {
    case OT_DEVICE_ROLE_DISABLED:
        state = "disabled";
        break;
    case OT_DEVICE_ROLE_DETACHED:
        state = "detached";
        break;
    case OT_DEVICE_ROLE_CHILD:
        state = "child";
        break;
    case OT_DEVICE_ROLE_ROUTER:
        state = "router";
        break;
    case OT_DEVICE_ROLE_LEADER:
        state = "leader";
        break;
    default:
        state = "invalid state";
        break;
    }
    state = JSON::String2JsonString(state);
    return state;
}

void Resource::DeleteOutDatedDiag()
{
    // auto eraseIt = mDiagMaintainer.begin();
    // for (eraseIt = mDiagMaintainer.begin(); eraseIt != mDiagMaintainer.end();)
    // {
    //     auto diagInfo = eraseIt->second.get();
    //     auto duration = duration_cast<microseconds>(steady_clock::now() - diagInfo->mStartTime).count();

    //     if (duration >= kDiagResetTimeout)
    //     {
    //         eraseIt = mDiagMaintainer.erase(eraseIt);
    //     }
    //     else
    //     {
    //         eraseIt++;
    //     }
    // }
}

void Resource::UpdateDiag(std::string aKey, std::string aValue)
{
    mDiagMaintainer[aKey] = std::unique_ptr<DiagInfo>(new DiagInfo(steady_clock::now(), aValue));
}

std::string Resource::GetDataNetworkName()
{
    const char *networkName = otThreadGetNetworkName(mInstance);
    std::string str         = networkName;

    str = JSON::String2JsonString(str);
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
    str = std::to_string(count);
    str = JSON::String2JsonString(str);
    return str;
}

std::string Resource::GetDataRloc16()
{
    uint16_t    rloc16 = otThreadGetRloc16(mInstance);
    char        p[7];
    std::string str;
    sprintf(p, "0x%04x", rloc16);
    str = p;
    str = JSON::String2JsonString(str);
    return str;
}

std::string Resource::GetDataExtendedPanId()
{
    const uint8_t *extPanId = reinterpret_cast<const uint8_t *>(otThreadGetExtendedPanId(mInstance));
    std::string    str      = JSON::Bytes2HexString(extPanId, OT_EXT_PAN_ID_SIZE);

    str = JSON::String2JsonString(str);
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
    otNetworkDiagTlv      diagTlv;
    otNetworkDiagIterator iterator = OT_NETWORK_DIAGNOSTIC_ITERATOR_INIT;
    otError               error    = OT_ERROR_NONE;

    std::string              keyRloc;
    std::string              diag;
    std::string              key;
    std::string              value;
    std::vector<std::string> keyContainer;
    std::vector<std::string> valueContainer;

    while ((error = otThreadGetNextDiagnosticTlv(aMessage, &iterator, &diagTlv)) == OT_ERROR_NONE)
    {
        switch (diagTlv.mType)
        {
        case OT_NETWORK_DIAGNOSTIC_TLV_EXT_ADDRESS:
        {
            value = JSON::Bytes2HexString(diagTlv.mData.mExtAddress.m8, OT_EXT_ADDRESS_SIZE);
            keyContainer.push_back("ExtAddress");
            valueContainer.push_back(JSON::String2JsonString(value));
        }
        break;
        case OT_NETWORK_DIAGNOSTIC_TLV_SHORT_ADDRESS:
        {
            char rloc[7];
            sprintf(rloc, "0x%04x", diagTlv.mData.mAddr16);
            value   = rloc;
            keyRloc = value;
            keyContainer.push_back("Rloc16");
            valueContainer.push_back(JSON::String2JsonString(value));
        }
        break;
        case OT_NETWORK_DIAGNOSTIC_TLV_MODE:
        {
            value = JSON::Mode2JsonString(diagTlv.mData.mMode);
            keyContainer.push_back("Mode");
            valueContainer.push_back(value);
        }
        break;
        case OT_NETWORK_DIAGNOSTIC_TLV_TIMEOUT:
        {
            char timeout[11];
            sprintf(timeout, "%d", diagTlv.mData.mTimeout);
            value = timeout;
            keyContainer.push_back("Timeout");
            valueContainer.push_back(JSON::String2JsonString(value));
        }
        break;
        case OT_NETWORK_DIAGNOSTIC_TLV_CONNECTIVITY:
        {
            value = JSON::Connectivity2JsonString(diagTlv.mData.mConnectivity);
            keyContainer.push_back("Connectivity");
            valueContainer.push_back(value);
        }
        break;
        case OT_NETWORK_DIAGNOSTIC_TLV_ROUTE:
        {
            value = JSON::Route2JsonString(diagTlv.mData.mRoute);
            keyContainer.push_back("Route");
            valueContainer.push_back(value);
        }
        break;
        case OT_NETWORK_DIAGNOSTIC_TLV_LEADER_DATA:
        {
            value = JSON::LeaderData2JsonString(diagTlv.mData.mLeaderData);
            keyContainer.push_back("LeaderData");
            valueContainer.push_back(value);
        }
        break;
        case OT_NETWORK_DIAGNOSTIC_TLV_NETWORK_DATA:
        {
            value = JSON::Bytes2HexString(diagTlv.mData.mNetworkData.m8, diagTlv.mData.mNetworkData.mCount);
            keyContainer.push_back("NetworkData");
            valueContainer.push_back(JSON::String2JsonString(value));
        }

        break;
        case OT_NETWORK_DIAGNOSTIC_TLV_IP6_ADDR_LIST:
        {
            std::vector<std::string> Ip6AddressList;
            for (uint16_t i = 0; i < diagTlv.mData.mIp6AddrList.mCount; ++i)
                Ip6AddressList.push_back(JSON::IpAddr2JsonString(diagTlv.mData.mIp6AddrList.mList[i]));
            value = JSON::Vector2JsonString(Ip6AddressList);
            keyContainer.push_back("IP6AddressList");
            valueContainer.push_back(value);
        }
        break;
        case OT_NETWORK_DIAGNOSTIC_TLV_MAC_COUNTERS:
        {
            value = JSON::MacCounters2JsonString(diagTlv.mData.mMacCounters);
            keyContainer.push_back("MACCounters");
            valueContainer.push_back(value);
        }
        break;
        case OT_NETWORK_DIAGNOSTIC_TLV_BATTERY_LEVEL:
        {
            char batteryLevel[9];
            sprintf(batteryLevel, "%d", diagTlv.mData.mBatteryLevel);
            value = batteryLevel;
            keyContainer.push_back("BatteryLevel");
            valueContainer.push_back(JSON::String2JsonString(value));
        }
        break;
        case OT_NETWORK_DIAGNOSTIC_TLV_SUPPLY_VOLTAGE:
        {
            char supplyVoltage[9];
            sprintf(supplyVoltage, "%d", diagTlv.mData.mSupplyVoltage);
            value = supplyVoltage;
            keyContainer.push_back("SupplyVoltage");
            valueContainer.push_back(JSON::String2JsonString(value));
        }

        break;
        case OT_NETWORK_DIAGNOSTIC_TLV_CHILD_TABLE:
        {
            std::vector<std::string> ChildTableList;

            for (uint16_t i = 0; i < diagTlv.mData.mChildTable.mCount; ++i)
                ChildTableList.push_back(JSON::ChildTableEntry2JsonString(diagTlv.mData.mChildTable.mTable[i]));
            value = JSON::Vector2JsonString(ChildTableList);
            keyContainer.push_back("ChildTable");
            valueContainer.push_back(value);
        }
        break;
        case OT_NETWORK_DIAGNOSTIC_TLV_CHANNEL_PAGES:
        {
            value = JSON::Bytes2HexString(diagTlv.mData.mChannelPages.m8, diagTlv.mData.mChannelPages.mCount);
            keyContainer.push_back("ChannelPages");
            valueContainer.push_back(JSON::String2JsonString(value));
        }
        break;
        case OT_NETWORK_DIAGNOSTIC_TLV_MAX_CHILD_TIMEOUT:
        {
            char childTimeout[9];
            sprintf(childTimeout, "%d", diagTlv.mData.mMaxChildTimeout);
            value = childTimeout;
            keyContainer.push_back("MaxChildTimeout");
            valueContainer.push_back(JSON::String2JsonString(value));
        }

        break;
        }
    }

    diag = JSON::TwoVector2JsonString(keyContainer, valueContainer);
    UpdateDiag(keyRloc, diag);
}

} // namespace rest
} // namespace otbr
