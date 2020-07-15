/*
 *  Copyright (c) 2017, The OpenThread Authors.
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

namespace otbr {
namespace rest {

static uint16_t HostSwap16(uint16_t v)

{
    return (((v & 0x00ffU) << 8) & 0xff00) | (((v & 0xff00U) >> 8) & 0x00ff);
}

static std::string FormatBytes(const uint8_t *aBytes, uint8_t aLength)
{
    std::unique_ptr<char> p(new char(aLength + 1));

    char *q = p.get();

    for (int i = 0; i < aLength; i++)
    {
        snprintf(q, 3, "%02x", aBytes[i]);
        q += 2;
    }
    std::string s(p.get(), aLength);
    return s;
}
Handler::Handler()
{
    mHandlerMap.emplace("/diagnostics", &GetDiagnostic);
    mHandlerMap.emplace("/node", &GetNodeInfo);
    mHandlerMap.emplace("/node/state", &GetState);
    mHandlerMap.emplace("/node/ext-address", &GetExtendedAddr);
    mHandlerMap.emplace("/node/network-name", &GetNetworkName);
    mHandlerMap.emplace("/node/rloc16", &GetRloc16);
    mHandlerMap.emplace("/node/leader-data", &GetLeaderData);
    mHandlerMap.emplace("/node/num-of-route", &GetNumOfRoute);
    mHandlerMap.emplace("/node/ext-panid", &GetExtendedPanId);
    mHandlerMap.emplace("/node/rloc", &GetRloc);
}

requestHandler Handler::GetHandler(std::string aPath)
{
    auto it = mHandlerMap.find(aPath);
    if (it == mHandlerMap.end())
        return ErrorHandler;

    else
    {
        auto handler = it->second;
        return handler;
    }
}

void Handler::ErrorHandler(Connection &aConnection, Response &aResponse)
{
    aConnection.SetErrorFlag(1);
    aConnection.SetErrorCode("no match handler");
    aResponse.SetBody("no match handler");
}
void Handler::GetNodeInfo(Connection &aConnection, Response &aResponse)
{
    aConnection.SetCallbackFlag(0);
    std::vector<std::string> keyContainer;
    std::vector<std::string> valueContainer;

    keyContainer.push_back("state");
    valueContainer.push_back(GetDataState(aConnection));

    keyContainer.push_back("networkName");
    valueContainer.push_back(GetDataNetworkName(aConnection));

    keyContainer.push_back("extAddress");
    valueContainer.push_back(GetDataExtendedAddr(aConnection));

    keyContainer.push_back("rloc16");
    valueContainer.push_back(GetDataRloc16(aConnection));

    keyContainer.push_back("numOfRouter");
    valueContainer.push_back(GetDataNumOfRoute(aConnection));

    keyContainer.push_back("leaderData");
    valueContainer.push_back(GetDataLeaderData(aConnection));

    keyContainer.push_back("extPanId");
    valueContainer.push_back(GetDataExtendedPanId(aConnection));

    std::string str = mJsonFormater.TwoVectorToJson(keyContainer, valueContainer);

    aResponse.SetBody(str);
}

void Handler::GetExtendedAddr(Connection &aConnection, Response &aResponse)
{
    std::string str = GetDataExtendedAddr(aConnection);
    aResponse.SetBody(str);
}

void Handler::GetState(Connection &aConnection, Response &aResponse)
{
    std::string str = GetDataState(aConnection);
    aResponse.SetBody(str);
}

void Handler::GetNetworkName(Connection &aConnection, Response &aResponse)
{
    std::string str = GetDataNetworkName(aConnection);
    aResponse.SetBody(str);
}

void Handler::GetLeaderData(Connection &aConnection, Response &aResponse)
{
    std::string str = GetDataLeaderData(aConnection);
    aResponse.SetBody(str);
}

void Handler::GetNumOfRoute(Connection &aConnection, Response &aResponse)
{
    std::string str = GetDataNumOfRoute(aConnection);
    aResponse.SetBody(str);
}

void Handler::GetRloc16(Connection &aConnection, Response &aResponse)
{
    std::string str = GetDataRloc16(aConnection);
    aResponse.SetBody(str);
}

void Handler::GetExtendedPanId(Connection &aConnection, Response &aResponse)
{
    std::string str = GetDataExtendedPanId(aConnection);
    aResponse.SetBody(str);
}

void Handler::GetRloc(Connection &aConnection, Response &aResponse)
{
    std::string str = GetDataRloc(aConnection);
    aResponse.SetBody(str);
}

std::string Handler::GetDataExtendedAddr(Connection &aConnection)
{
    aConnection.SetCallbackFlag(0);

    const uint8_t *extAddress = reinterpret_cast<const uint8_t *>(otLinkGetExtendedAddress(aConnection.GetInstance()));
    std::string    str        = FormatBytes(extAddress, OT_EXT_ADDRESS_SIZE);
    return str;
}

std::string Handler::GetDataState(Connection &aConnection)
{
    aConnection.SetCallbackFlag(0);
    std::string state;

    switch (otThreadGetDeviceRole(aConnection.GetInstance()))
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

std::string Handler::GetDataNetworkName(Connection &aConnection)
{
    aConnection.SetCallbackFlag(0);
    const char *          networkName = otThreadGetNetworkName(aConnection.GetInstance());
    std::unique_ptr<char> p(new char(20));

    sprintf(p.get(), "%s", static_cast<const char *>(networkName));
    std::string str = p.get();

    return str;
}

std::string Handler::GetDataLeaderData(Connection &aConnection)
{
    aConnection.SetCallbackFlag(0);
    otLeaderData leaderData;
    otThreadGetLeaderData(aConnection.GetInstance(), &leaderData);
    std::string str = mJsonFormater.CreateLeaderData(leaderData);
    return str;
}

std::string Handler::GetDataNumOfRoute(Connection &aConnection)
{
    aConnection.SetCallbackFlag(0);
    int          count = 0;
    uint8_t      maxRouterId;
    otRouterInfo routerInfo;
    maxRouterId = otThreadGetMaxRouterId(aConnection.GetInstance());

    for (uint8_t i = 0; i <= maxRouterId; i++)
    {
        if (otThreadGetRouterInfo(aConnection.GetInstance(), i, &routerInfo) != OT_ERROR_NONE)
        {
            continue;
        }
        count++;
    }
    std::string str = std::to_string(count);
    return str;
}
std::string Handler::GetDataRloc16(Connection &aConnection)
{
    aConnection.SetCallbackFlag(0);
    uint16_t              rloc16 = otThreadGetRloc16(aConnection.GetInstance());
    std::unique_ptr<char> p(new char(7));

    sprintf(p.get(), "0x%04x", rloc16);
    std::string str = p.get();
    return str;
}

std::string Handler::GetDataExtendedPanId(Connection &aConnection)
{
    aConnection.SetCallbackFlag(0);
    const uint8_t *extPanId = reinterpret_cast<const uint8_t *>(otThreadGetExtendedPanId(aConnection.GetInstance()));
    std::string    str      = FormatBytes(extPanId, OT_EXT_PAN_ID_SIZE);
    return str;
}

std::string Handler::GetDataRloc(Connection &aConnection)
{
    aConnection.SetCallbackFlag(0);
    struct otIp6Address   rloc16address = *otThreadGetRloc(aConnection.GetInstance());
    std::unique_ptr<char> p(new char(65));

    sprintf(p.get(), "%x:%x:%x:%x:%x:%x:%x:%x", HostSwap16(rloc16address.mFields.m16[0]),
            HostSwap16(rloc16address.mFields.m16[1]), HostSwap16(rloc16address.mFields.m16[2]),
            HostSwap16(rloc16address.mFields.m16[3]), HostSwap16(rloc16address.mFields.m16[4]),
            HostSwap16(rloc16address.mFields.m16[5]), HostSwap16(rloc16address.mFields.m16[6]),
            HostSwap16(rloc16address.mFields.m16[7]));
    std::string str = p.get();
    return str;
}

void Handler::GetDiagnostic(Connection &aConnection, Response &aResponse)
{
    OT_UNUSED_VARIABLE(aResponse);

    aConnection.SetCallbackFlag(1);
    aConnection.ResetDiagInfo();

    struct otIp6Address rloc16address     = *otThreadGetRloc(aConnection.GetInstance());
    char                multicastAddr[10] = "ff02::2";
    struct otIp6Address multicastAddress;
    otIp6AddressFromString(multicastAddr, &multicastAddress);

    std::unique_ptr<char> p(new char(65));

    sprintf(p.get(), "%x:%x:%x:%x:%x:%x:%x:%x", HostSwap16(rloc16address.mFields.m16[0]),
            HostSwap16(rloc16address.mFields.m16[1]), HostSwap16(rloc16address.mFields.m16[2]),
            HostSwap16(rloc16address.mFields.m16[3]), HostSwap16(rloc16address.mFields.m16[4]),
            HostSwap16(rloc16address.mFields.m16[5]), HostSwap16(rloc16address.mFields.m16[6]),
            HostSwap16(rloc16address.mFields.m16[7]));
    uint8_t tlvTypes[OT_NETWORK_DIAGNOSTIC_TYPELIST_MAX_ENTRIES];
    uint8_t count = 0;

    for (long t = 0; t <= 9; t++)
    {
        tlvTypes[count++] = static_cast<uint8_t>(t);
    }
    for (long t = 14; t <= 19; t++)
    {
        tlvTypes[count++] = static_cast<uint8_t>(t);
    }

    otThreadSendDiagnosticGet(aConnection.GetInstance(), &rloc16address, tlvTypes, count);
    otThreadSendDiagnosticGet(aConnection.GetInstance(), &multicastAddress, tlvTypes, count);
}

void Handler::DiagnosticResponseHandler(otMessage *aMessage, const otMessageInfo *aMessageInfo, void *aContext)
{
    DiagnosticResponseHandler(aMessage, *static_cast<const otMessageInfo *>(aMessageInfo),
                              static_cast<RestWebServer *>(aContext));
}

void Handler::DiagnosticResponseHandler(otMessage *aMessage, const otMessageInfo, RestWebServer *aRestWebServer)
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
            value = FormatBytes(diagTlv.mData.mExtAddress.m8, OT_EXT_ADDRESS_SIZE);
            keyContainer.push_back("Ext Address");
            valueContainer.push_back(value);
        }
        break;
        case OT_NETWORK_DIAGNOSTIC_TLV_SHORT_ADDRESS:
        {
            std::unique_ptr<char> rloc(new char(7));
            sprintf(rloc.get(), "0x%04x", diagTlv.mData.mAddr16);
            value   = rloc.get();
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
            std::unique_ptr<char> timeout(new char(11));
            sprintf(timeout.get(), "%d", diagTlv.mData.mTimeout);
            value = timeout.get();
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
            value = FormatBytes(diagTlv.mData.mNetworkData.m8, diagTlv.mData.mNetworkData.mCount);
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
            std::unique_ptr<char> batteryLevel(new char(9));
            sprintf(batteryLevel.get(), "%d", diagTlv.mData.mBatteryLevel);
            value = batteryLevel.get();
            keyContainer.push_back("Battery Level");
            valueContainer.push_back(value);
        }
        break;
        case OT_NETWORK_DIAGNOSTIC_TLV_SUPPLY_VOLTAGE:
        {
            std::unique_ptr<char> supplyVoltage(new char(9));
            sprintf(supplyVoltage.get(), "%d", diagTlv.mData.mSupplyVoltage);
            value = supplyVoltage.get();
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
            std::unique_ptr<char> childTimeout(new char(9));

            sprintf(childTimeout.get(), "%d", diagTlv.mData.mMaxChildTimeout);
            value = childTimeout.get();
            keyContainer.push_back("Max Child Timeout");
            valueContainer.push_back(value);
        }

        break;
        }
    }
    diag = mJsonFormater.TwoVectorToJson(keyContainer, valueContainer);
    aRestWebServer->AddDiag(keyRloc, diag);
}

} // namespace rest
} // namespace otbr
