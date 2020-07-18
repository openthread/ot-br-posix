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

#include "rest/handler.hpp"

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

namespace otbr {
namespace rest {

const char *  Resource::kMulticastAddrAllRouters = "ff02::2";
const uint8_t Resource::kAllTlvTypes[]           = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 14, 15, 16, 17, 18, 19};

static uint16_t HostSwap16(uint16_t v)

{
    return (((v & 0x00ffU) << 8) & 0xff00) | (((v & 0xff00U) >> 8) & 0x00ff);
}

static std::string BytesToHexString(const uint8_t *aBytes, uint8_t aLength)
{
    std::unique_ptr<char> p(new char(aLength + 1));

    char *q = p.get();

    for (int i = 0; i < aLength; ++i)
    {
        snprintf(q, 3, "%02x", aBytes[i]);
        q += 2;
    }
    std::string s(p.get(), aLength);
    return s;
}

void Resource::Init()
{
    otThreadSetReceiveDiagnosticGetCallback(mInstance, &DiagnosticResponseHandler, this);

    mHandlerMap.emplace(OT_DIAGNOETIC_PATH, &GetDiagnostic);
    mHandlerMap.emplace(OT_NODE_PATH, &GetNodeInfo);
    mHandlerMap.emplace(OT_STATE_PATH, &GetState);
    mHandlerMap.emplace(OT_EXTADDRESS_PATH, &GetExtendedAddr);
    mHandlerMap.emplace(OT_NETWORKNAME_PATH, &GetNetworkName);
    mHandlerMap.emplace(OT_RLOC16_PATH, &GetRloc16);
    mHandlerMap.emplace(OT_LEADERDATA_PATH, &GetLeaderData);
    mHandlerMap.emplace(OT_NUMOFROUTER_PATH, &GetNumOfRoute);
    mHandlerMap.emplace(OT_EXTPANID_PATH, &GetExtendedPanId);
    mHandlerMap.emplace(OT_RLOC_PATH, &GetRloc);
}

void Resource::Handle(Request &aRequest, Response &aResponse)
{
    auto it = mHandlerMap.find(aPath);
    if (it == mHandlerMap.end())

        ErrorHandler(aRequest, aResponse);
    else
    {
        auto handler = it->second;
        handler(aRequest, aResponse);
    }
}

void Resource::HandleCallback(Response &aResponse)
{
}

void Resource::ErrorHandler(Request &aRequest, Response &aResponse)
{
    OT_UNUSED_VARIABLE(aRequest);
    aResponse.SetBody("no match handler");
}
void Resource::NodeInfo(Request &aRequest, Response &aResponse)
{
    aRequest.SetCallbackFlag(0);
    std::vector<std::string> keyContainer;
    std::vector<std::string> valueContainer;

    keyContainer.push_back("state");
    valueContainer.push_back(GetDataState(aRequest));

    keyContainer.push_back("networkName");
    valueContainer.push_back(GetDataNetworkName(aRequest));

    keyContainer.push_back("extAddress");
    valueContainer.push_back(GetDataExtendedAddr(aRequest));

    keyContainer.push_back("rloc16");
    valueContainer.push_back(GetDataRloc16(aRequest));

    keyContainer.push_back("numOfRouter");
    valueContainer.push_back(GetDataNumOfRoute(aRequest));

    keyContainer.push_back("leaderData");
    valueContainer.push_back(GetDataLeaderData(aRequest));

    keyContainer.push_back("extPanId");
    valueContainer.push_back(GetDataExtendedPanId(aRequest));

    std::string str = mJsonFormater.TwoVectorToJson(keyContainer, valueContainer);

    aResponse.SetBody(str);
}

void Resource::ExtendedAddr(Request &aRequest, Response &aResponse)
{
    std::string str = GetDataExtendedAddr(aRequest);
    aResponse.SetBody(str);
}

void Resource::State(Request &aRequest, Response &aResponse)
{
    std::string str = GetDataState(aRequest);
    aResponse.SetBody(str);
}

void Resource::NetworkName(Request &aRequest, Response &aResponse)
{
    std::string str = GetDataNetworkName(aRequest);
    aResponse.SetBody(str);
}

void Resource::LeaderData(Request &aRequest, Response &aResponse)
{
    std::string str = GetDataLeaderData(aRequest);
    aResponse.SetBody(str);
}

void Resource::NumOfRoute(Request &aRequest, Response &aResponse)
{
    std::string str = GetDataNumOfRoute(aRequest);
    aResponse.SetBody(str);
}

void Resource::Rloc16(Request &aRequest, Response &aResponse)
{
    std::string str = GetDataRloc16(aRequest);
    aResponse.SetBody(str);
}

void Resource::ExtendedPanId(Request &aRequest, Response &aResponse)
{
    std::string str = GetDataExtendedPanId(aRequest);
    aResponse.SetBody(str);
}

void Resource::Rloc(Request &aRequest, Response &aResponse)
{
    std::string str = GetDataRloc(aRequest);
    aResponse.SetBody(str);
}

std::string Resource::GetDataExtendedAddr()
{
    const uint8_t *extAddress = reinterpret_cast<const uint8_t *>(otLinkGetExtendedAddress(mInstance));
    std::string    str        = BytesToHexString(extAddress, OT_EXT_ADDRESS_SIZE);
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
    return state;
}

std::string Resource::GetDataNetworkName(Request &aRequest)
{
    const char *networkName = otThreadGetNetworkName(mInstance);

    std::string str = networkName;

    return str;
}

std::string Resource::GetDataLeaderData(Request &aRequest)
{
    otLeaderData leaderData;
    otThreadGetLeaderData(mInstance, &leaderData);
    std::string str = mJsonFormater.CreateLeaderData(leaderData);
    return str;
}

std::string Resource::GetDataNumOfRoute(Request &aRequest)
{
    int          count = 0;
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
    std::string str = std::to_string(count);
    return str;
}
std::string Resource::GetDataRloc16(Request &aRequest)
{
    uint16_t rloc16 = otThreadGetRloc16(mInstance);
    char     p[7];
    sprintf(p, "0x%04x", rloc16);
    std::string str = p;
    return str;
}

std::string Resource::GetDataExtendedPanId(Request &aRequest)
{
    const uint8_t *extPanId = reinterpret_cast<const uint8_t *>(otThreadGetExtendedPanId(mInstance));
    std::string    str      = BytesToHexString(extPanId, OT_EXT_PAN_ID_SIZE);
    return str;
}

std::string Resource::GetDataRloc(Request &aRequest)
{
    struct otIp6Address rlocAddress = *otThreadGetRloc(mInstance);
    char                p[65] l

                sprintf(p, "%x:%x:%x:%x:%x:%x:%x:%x", HostSwap16(rlocAddress.mFields.m16[0]),
                HostSwap16(rlocAddress.mFields.m16[1]), HostSwap16(rlocAddress.mFields.m16[2]),
                HostSwap16(rlocAddress.mFields.m16[3]), HostSwap16(rlocAddress.mFields.m16[4]),
                HostSwap16(rlocAddress.mFields.m16[5]), HostSwap16(rlocAddress.mFields.m16[6]),
                HostSwap16(rlocAddress.mFields.m16[7]));
    std::string str = p;
    return str;
}

void Resource::Diagnostic(Request &aRequest, Response &aResponse)
{
    aResponse.SetCallback();

    struct otIp6Address rloc16address = *otThreadGetRloc(mInstance);
    struct otIp6Address multicastAddress;
    otIp6AddressFromString(kMulticastAddrAllRouters, &multicastAddress);
    // char[65] p;

    // sprintf(p, "%x:%x:%x:%x:%x:%x:%x:%x", HostSwap16(rloc16address.mFields.m16[0]),
    //         HostSwap16(rloc16address.mFields.m16[1]), HostSwap16(rloc16address.mFields.m16[2]),
    //         HostSwap16(rloc16address.mFields.m16[3]), HostSwap16(rloc16address.mFields.m16[4]),
    //         HostSwap16(rloc16address.mFields.m16[5]), HostSwap16(rloc16address.mFields.m16[6]),
    //         HostSwap16(rloc16address.mFields.m16[7]));

    uint8_t tlvTypes[OT_NETWORK_DIAGNOSTIC_TYPELIST_MAX_ENTRIES];
    uint8_t count = 0;

    otThreadSendDiagnosticGet(mInstance, &rloc16address, kAllTlvTypes, count);
    otThreadSendDiagnosticGet(mInstance, &multicastAddress, kAllTlvTypes, count);
}

void Resource::DiagnosticResponseHandler(otMessage *aMessage, const otMessageInfo *aMessageInfo, void *aContext)
{
    DiagnosticResponseHandler(aMessage, *static_cast<const otMessageInfo *>(aMessageInfo),
                              static_cast<Resource *>(aContext));
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
            value = BytesToHexString(diagTlv.mData.mExtAddress.m8, OT_EXT_ADDRESS_SIZE);
            keyContainer.push_back("Ext Address");
            valueContainer.push_back(value);
        }
        break;
        case OT_NETWORK_DIAGNOSTIC_TLV_SHORT_ADDRESS:
        {
            char[7] rloc;
            sprintf(rloc, "0x%04x", diagTlv.mData.mAddr16);
            value   = rloc;
            keyRloc = value;
            keyContainer.push_back("Rloc16");
            valueContainer.push_back(value);
        }
        break;
        case OT_NETWORK_DIAGNOSTIC_TLV_MODE:
        {
            value = mJsonFormater.CreateMode(diagTlv.mData.mMode);
            keyContainer.push_back("Mode");
            valueContainer.push_back(value);
        }
        break;
        case OT_NETWORK_DIAGNOSTIC_TLV_TIMEOUT:
        {
            char[11] timeout;
            sprintf(timeout, "%d", diagTlv.mData.mTimeout);
            value = timeout;
            keyContainer.push_back("Timeout");
            valueContainer.push_back(value);
        }
        break;
        case OT_NETWORK_DIAGNOSTIC_TLV_CONNECTIVITY:
        {
            value = mJsonFormater.CreateConnectivity(diagTlv.mData.mConnectivity);
            keyContainer.push_back("Connectivity");
            valueContainer.push_back(value);
        }
        break;
        case OT_NETWORK_DIAGNOSTIC_TLV_ROUTE:
        {
            value = mJsonFormater.CreateRoute(diagTlv.mData.mRoute);
            keyContainer.push_back("Route");
            valueContainer.push_back(value);
        }
        break;
        case OT_NETWORK_DIAGNOSTIC_TLV_LEADER_DATA:
        {
            value = mJsonFormater.CreateLeaderData(diagTlv.mData.mLeaderData);
            keyContainer.push_back("Leader Data");
            valueContainer.push_back(value);
        }
        break;
        case OT_NETWORK_DIAGNOSTIC_TLV_NETWORK_DATA:
        {
            value = BytesToHexString(diagTlv.mData.mNetworkData.m8, diagTlv.mData.mNetworkData.mCount);
            keyContainer.push_back("Network Data");
            valueContainer.push_back(value);
        }

        break;
        case OT_NETWORK_DIAGNOSTIC_TLV_IP6_ADDR_LIST:
        {
            std::vector<std::string> Ip6AddressList;
            for (uint16_t i = 0; i < diagTlv.mData.mIp6AddrList.mCount; ++i)
                Ip6AddressList.push_back(mJsonFormater.CreateIp6Address(diagTlv.mData.mIp6AddrList.mList[i]));
            value = mJsonFormater.VectorToJson(Ip6AddressList);
            keyContainer.push_back("IP6 Address List");
            valueContainer.push_back(value);
        }
        break;
        case OT_NETWORK_DIAGNOSTIC_TLV_MAC_COUNTERS:
        {
            value = mJsonFormater.CreateMacCounters(diagTlv.mData.mMacCounters);
            keyContainer.push_back("MAC Counters");
            valueContainer.push_back(value);
        }
        break;
        case OT_NETWORK_DIAGNOSTIC_TLV_BATTERY_LEVEL:
        {
            char[9] batteryLevel;
            sprintf(batteryLevel, "%d", diagTlv.mData.mBatteryLevel);
            value = batteryLevel;
            keyContainer.push_back("Battery Level");
            valueContainer.push_back(value);
        }
        break;
        case OT_NETWORK_DIAGNOSTIC_TLV_SUPPLY_VOLTAGE:
        {
            char[9] supplyVoltage;
            sprintf(supplyVoltage, "%d", diagTlv.mData.mSupplyVoltage);
            value = supplyVoltage;
            keyContainer.push_back("Supply Voltage");
            valueContainer.push_back(value);
        }

        break;
        case OT_NETWORK_DIAGNOSTIC_TLV_CHILD_TABLE:
        {
            std::vector<std::string> ChildTableList;

            for (uint16_t i = 0; i < diagTlv.mData.mChildTable.mCount; ++i)
                ChildTableList.push_back(mJsonFormater.CreateChildTableEntry(diagTlv.mData.mChildTable.mTable[i]));
            value = mJsonFormater.VectorToJson(ChildTableList);
            keyContainer.push_back("Child Table");
            valueContainer.push_back(value);
        }
        break;
        case OT_NETWORK_DIAGNOSTIC_TLV_CHANNEL_PAGES:
        {
            value = FormatBytes(diagTlv.mData.mChannelPages.m8, diagTlv.mData.mChannelPages.mCount);
            keyContainer.push_back("Channel Pages");
            valueContainer.push_back(value);
        }
        break;
        case OT_NETWORK_DIAGNOSTIC_TLV_MAX_CHILD_TIMEOUT:
        {
            char[9] childTimeout;
            sprintf(childTimeout, "%d", diagTlv.mData.mMaxChildTimeout);
            value = childTimeout;
            keyContainer.push_back("Max Child Timeout");
            valueContainer.push_back(value);
        }

        break;
        }
    }
    diag = mJsonFormater.TwoVectorToJson(keyContainer, valueContainer);
}

} // namespace rest
} // namespace otbr
