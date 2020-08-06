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

#include "rest/json.hpp"

#include "common/code_utils.hpp"

extern "C" {
#include "cJSON.h"
}

namespace otbr {
namespace rest {
namespace JSON {

cJSON *Bytes2HexJson(const uint8_t *aBytes, uint8_t aLength)
{
    char   hex[2 * aLength + 1];
    cJSON *ret = nullptr;

    otbr::Utils::Bytes2Hex(aBytes, aLength, hex);
    hex[2 * aLength] = '\0';
    ret              = cJSON_CreateString(hex);

    return ret;
}

std::string String2JsonString(std::string aString)
{
    std::string ret;
    cJSON *     json    = nullptr;
    char *      jsonOut = nullptr;

    VerifyOrExit(aString.size() > 0);

    json    = cJSON_CreateString(aString.c_str());
    jsonOut = cJSON_Print(json);
    ret     = jsonOut;
    delete (jsonOut);
    jsonOut = nullptr;
    cJSON_Delete(json);

exit:
    return ret;
}

std::string Json2String(const cJSON *aJson)
{
    std::string ret;
    char *      jsonOut = nullptr;

    VerifyOrExit(aJson != nullptr);

    jsonOut = cJSON_Print(aJson);
    ret     = jsonOut;
    delete (jsonOut);
    jsonOut = nullptr;

exit:
    return ret;
}
cJSON *CString2Json(const char *aString)
{
    cJSON *jsonStr = cJSON_CreateString(aString);

    return jsonStr;
}

cJSON *Networks2Json(const std::vector<ActiveScanResult> aResults)
{
    cJSON *networks = cJSON_CreateArray();
    cJSON *network  = nullptr;
    for (const auto result : aResults)
    {
        network = cJSON_CreateObject();
        
        cJSON_AddItemToObject(network, "IsJoinable", cJSON_CreateNumber(result.mIsJoinable));

        
        cJSON_AddItemToObject(network, "NetworkName", cJSON_CreateString(result.mNetworkName.c_str()));

        cJSON_AddItemToObject(network, "ExtPanId", Bytes2HexJson(result.mExtendedPanId, OT_EXT_PAN_ID_SIZE));

        cJSON_AddItemToObject(network, "PanId", cJSON_CreateNumber(result.mPanId));

        cJSON_AddItemToObject(network, "MacAddress", Bytes2HexJson(result.mExtAddress, OT_EXT_ADDRESS_SIZE));

        cJSON_AddItemToObject(network, "Channel", cJSON_CreateNumber(result.mChannel));

        cJSON_AddItemToObject(network, "Rssi", cJSON_CreateNumber(result.mRssi));

        cJSON_AddItemToObject(network, "LQi", cJSON_CreateNumber(result.mLqi));

        cJSON_AddItemToArray(networks, network);
    }

    return networks;
}
cJSON *Mode2Json(const otLinkModeConfig &aMode)
{
    cJSON *mode = cJSON_CreateObject();

    cJSON_AddItemToObject(mode, "RxOnWhenIdle", cJSON_CreateNumber(aMode.mRxOnWhenIdle));

    cJSON_AddItemToObject(mode, "SecureDataRequests", cJSON_CreateNumber(aMode.mSecureDataRequests));

    cJSON_AddItemToObject(mode, "DeviceType", cJSON_CreateNumber(aMode.mDeviceType));

    cJSON_AddItemToObject(mode, "NetworkData", cJSON_CreateNumber(aMode.mNetworkData));

    return mode;
}

cJSON *Addr2Json(const uint8_t *aAddress, size_t aSize)
{
    cJSON *     addr          = nullptr;
    std::string serilizedAddr = "";
    uint16_t    twoBytes;
    char        AddrField[5];

    for (size_t i = 0; i < aSize; ++i)
    {
        if (i % 2 == 1)
        {
            twoBytes = (aAddress[i - 1] << 8) + aAddress[i];
            sprintf(AddrField, "%x", twoBytes);
            if (i > 1)
            {
                serilizedAddr += ":";
            }
            serilizedAddr += std::string(AddrField);
        }
    }

    addr = cJSON_CreateString(serilizedAddr.c_str());

    return addr;
}


cJSON *IpAddr2Json(const otIp6Address &aAddress)
{
    cJSON *     ipAddr          = nullptr;
    std::string serilizedIpAddr = "";
    uint16_t    twoBytes;
    char        ipAddrField[5];

    for (size_t i = 0; i < 16; ++i)
    {
        if (i % 2 == 1)
        {
            twoBytes = (aAddress.mFields.m8[i - 1] << 8) + aAddress.mFields.m8[i];
            sprintf(ipAddrField, "%x", twoBytes);
            if (i > 1)
            {
                serilizedIpAddr += ":";
            }
            serilizedIpAddr += std::string(ipAddrField);
        }
    }

    ipAddr = cJSON_CreateString(serilizedIpAddr.c_str());

    return ipAddr;
}

cJSON *ChildTableEntry2Json(const otNetworkDiagChildEntry &aChildEntry)
{
    cJSON *childEntry = cJSON_CreateObject();

    cJSON_AddItemToObject(childEntry, "ChildId", cJSON_CreateNumber(aChildEntry.mChildId));

    cJSON_AddItemToObject(childEntry, "Timeout", cJSON_CreateNumber(aChildEntry.mTimeout));

    cJSON *mode = Mode2Json(aChildEntry.mMode);

    cJSON_AddItemToObject(childEntry, "Mode", mode);

    return childEntry;
}

cJSON *MacCounters2Json(const otNetworkDiagMacCounters &aMacCounters)
{
    cJSON *macCounters = cJSON_CreateObject();

    cJSON_AddItemToObject(macCounters, "IfInUnknownProtos", cJSON_CreateNumber(aMacCounters.mIfInUnknownProtos));

    cJSON_AddItemToObject(macCounters, "IfInErrors", cJSON_CreateNumber(aMacCounters.mIfInErrors));

    cJSON_AddItemToObject(macCounters, "IfOutErrors", cJSON_CreateNumber(aMacCounters.mIfOutErrors));

    cJSON_AddItemToObject(macCounters, "IfInUcastPkts", cJSON_CreateNumber(aMacCounters.mIfInUcastPkts));

    cJSON_AddItemToObject(macCounters, "IfInBroadcastPkts", cJSON_CreateNumber(aMacCounters.mIfInBroadcastPkts));

    cJSON_AddItemToObject(macCounters, "IfInDiscards", cJSON_CreateNumber(aMacCounters.mIfInDiscards));

    cJSON_AddItemToObject(macCounters, "IfOutUcastPkts", cJSON_CreateNumber(aMacCounters.mIfOutUcastPkts));

    cJSON_AddItemToObject(macCounters, "IfOutBroadcastPkts", cJSON_CreateNumber(aMacCounters.mIfOutBroadcastPkts));

    cJSON_AddItemToObject(macCounters, "IfOutDiscards", cJSON_CreateNumber(aMacCounters.mIfOutDiscards));

    return macCounters;
}

cJSON *Connectivity2Json(const otNetworkDiagConnectivity &aConnectivity)
{
    cJSON *connectivity = cJSON_CreateObject();

    cJSON_AddItemToObject(connectivity, "ParentPriority", cJSON_CreateNumber(aConnectivity.mParentPriority));

    cJSON_AddItemToObject(connectivity, "LinkQuality3", cJSON_CreateNumber(aConnectivity.mLinkQuality3));

    cJSON_AddItemToObject(connectivity, "LinkQuality2", cJSON_CreateNumber(aConnectivity.mLinkQuality2));

    cJSON_AddItemToObject(connectivity, "LinkQuality1", cJSON_CreateNumber(aConnectivity.mLinkQuality1));

    cJSON_AddItemToObject(connectivity, "LeaderCost", cJSON_CreateNumber(aConnectivity.mLeaderCost));

    cJSON_AddItemToObject(connectivity, "IdSequence", cJSON_CreateNumber(aConnectivity.mIdSequence));

    cJSON_AddItemToObject(connectivity, "ActiveRouters", cJSON_CreateNumber(aConnectivity.mActiveRouters));

    cJSON_AddItemToObject(connectivity, "SedBufferSize", cJSON_CreateNumber(aConnectivity.mSedBufferSize));

    cJSON_AddItemToObject(connectivity, "SedDatagramCount", cJSON_CreateNumber(aConnectivity.mSedDatagramCount));

    return connectivity;
}

cJSON *RouteData2Json(const otNetworkDiagRouteData &aRouteData)
{
    cJSON *routeData = cJSON_CreateObject();

    cJSON_AddItemToObject(routeData, "RouteId", cJSON_CreateNumber(aRouteData.mRouterId));

    cJSON_AddItemToObject(routeData, "LinkQualityOut", cJSON_CreateNumber(aRouteData.mLinkQualityOut));

    cJSON_AddItemToObject(routeData, "LinkQualityIn", cJSON_CreateNumber(aRouteData.mLinkQualityIn));

    cJSON_AddItemToObject(routeData, "RouteCost", cJSON_CreateNumber(aRouteData.mRouteCost));

    return routeData;
}

cJSON *Route2Json(const otNetworkDiagRoute &aRoute)
{
    cJSON *route = cJSON_CreateObject();

    cJSON_AddItemToObject(route, "IdSequence", cJSON_CreateNumber(aRoute.mIdSequence));

    cJSON *RouteData = cJSON_CreateArray();
    for (uint16_t i = 0; i < aRoute.mRouteCount; ++i)
    {
        cJSON *RouteDatavalue = RouteData2Json(aRoute.mRouteData[i]);

        cJSON_AddItemToArray(RouteData, RouteDatavalue);
    }

    cJSON_AddItemToObject(route, "RouteData", RouteData);

    return route;
}

cJSON *LeaderData2Json(const otLeaderData &aLeaderData)
{
    cJSON *leaderData = cJSON_CreateObject();

    cJSON_AddItemToObject(leaderData, "PartitionId", cJSON_CreateNumber(aLeaderData.mPartitionId));

    cJSON_AddItemToObject(leaderData, "Weighting", cJSON_CreateNumber(aLeaderData.mWeighting));

    cJSON_AddItemToObject(leaderData, "DataVersion", cJSON_CreateNumber(aLeaderData.mDataVersion));

    cJSON_AddItemToObject(leaderData, "StableDataVersion", cJSON_CreateNumber(aLeaderData.mStableDataVersion));

    cJSON_AddItemToObject(leaderData, "LeaderRouterId", cJSON_CreateNumber(aLeaderData.mLeaderRouterId));

    return leaderData;
}

std::string IpAddr2JsonString(const otIp6Address &aAddress)
{
    std::string ret;
    cJSON *     ipAddr = IpAddr2Json(aAddress);

    ret = Json2String(ipAddr);
    cJSON_Delete(ipAddr);

    return ret;
}

std::string Networks2JsonString(const std::vector<ActiveScanResult> aResults)
{
    std::string ret;
    cJSON *     networks = Networks2Json(aResults);

    ret = Json2String(networks);
    cJSON_Delete(networks);

    return ret;
}

std::string Node2JsonString(const Node &aNode)
{
    cJSON *     node = cJSON_CreateObject();
    std::string ret;
    cJSON_AddItemToObject(node, "MeshLocalAddress", IpAddr2Json(aNode.meshLocalAddress));
    cJSON_AddItemToObject(node, "MeshLocalPrefix", Addr2Json(aNode.meshLocalPrefix,8));
    cJSON_AddItemToObject(node, "PanId", cJSON_CreateNumber(aNode.panId));
     
    cJSON_AddItemToObject(node, "Channel", cJSON_CreateNumber(aNode.channel));
    
    cJSON_AddItemToObject(node, "Version", cJSON_CreateString(aNode.version));
    
    cJSON_AddItemToObject(node, "State", cJSON_CreateNumber(aNode.role));

    cJSON_AddItemToObject(node, "NumOfRouter", cJSON_CreateNumber(aNode.numOfRouter));

    cJSON_AddItemToObject(node, "RlocAddress", IpAddr2Json(aNode.rlocAddress));
    
    cJSON_AddItemToObject(node, "ExtAddress", Bytes2HexJson(aNode.extAddress, OT_EXT_ADDRESS_SIZE));
    
    cJSON_AddItemToObject(node, "Eui64", Bytes2HexJson(aNode.eui64.m8, OT_EXT_ADDRESS_SIZE));
    
    cJSON_AddItemToObject(node, "NetworkName", cJSON_CreateString(aNode.networkName.c_str()));

    cJSON_AddItemToObject(node, "Rloc16", cJSON_CreateNumber(aNode.rloc16));

    cJSON_AddItemToObject(node, "LeaderData", LeaderData2Json(aNode.leaderData));

    cJSON_AddItemToObject(node, "ExtPanId", Bytes2HexJson(aNode.extPanId, OT_EXT_PAN_ID_SIZE));

    ret = Json2String(node);

    cJSON_Delete(node);

    return ret;
}

std::string Diag2JsonString(const std::vector<std::vector<otNetworkDiagTlv>> &aDiagSet)
{
    cJSON *     diagInfo          = cJSON_CreateArray();
    cJSON *     diagInfoOfOneNode = nullptr;
    std::string ret;

    for (auto diagItem : aDiagSet)
    {
        diagInfoOfOneNode = cJSON_CreateObject();
        for (auto diagTlv : diagItem)
        {
            switch (diagTlv.mType)
            {
            case OT_NETWORK_DIAGNOSTIC_TLV_EXT_ADDRESS:

                cJSON_AddItemToObject(diagInfoOfOneNode, "ExtAddress",
                                      Bytes2HexJson(diagTlv.mData.mExtAddress.m8, OT_EXT_ADDRESS_SIZE));

                break;
            case OT_NETWORK_DIAGNOSTIC_TLV_SHORT_ADDRESS:
            {
                cJSON_AddItemToObject(diagInfoOfOneNode, "Rloc16", cJSON_CreateNumber(diagTlv.mData.mAddr16));
            }
            break;
            case OT_NETWORK_DIAGNOSTIC_TLV_MODE:
            {
                cJSON_AddItemToObject(diagInfoOfOneNode, "Mode", Mode2Json(diagTlv.mData.mMode));
            }
            break;
            case OT_NETWORK_DIAGNOSTIC_TLV_TIMEOUT:
            {
                uint64_t timeout = static_cast<uint64_t>(diagTlv.mData.mTimeout);
                cJSON_AddItemToObject(diagInfoOfOneNode, "Timeout", cJSON_CreateNumber(timeout));
            }
            break;
            case OT_NETWORK_DIAGNOSTIC_TLV_CONNECTIVITY:
            {
                cJSON_AddItemToObject(diagInfoOfOneNode, "Connectivity",
                                      Connectivity2Json(diagTlv.mData.mConnectivity));
            }
            break;
            case OT_NETWORK_DIAGNOSTIC_TLV_ROUTE:
            {
                cJSON_AddItemToObject(diagInfoOfOneNode, "Route", Route2Json(diagTlv.mData.mRoute));
            }
            break;
            case OT_NETWORK_DIAGNOSTIC_TLV_LEADER_DATA:
            {
                cJSON_AddItemToObject(diagInfoOfOneNode, "LeaderData", LeaderData2Json(diagTlv.mData.mLeaderData));
            }
            break;
            case OT_NETWORK_DIAGNOSTIC_TLV_NETWORK_DATA:
            {
                cJSON_AddItemToObject(diagInfoOfOneNode, "NetworkData",
                                      Bytes2HexJson(diagTlv.mData.mNetworkData.m8, diagTlv.mData.mNetworkData.mCount));
            }
            break;
            case OT_NETWORK_DIAGNOSTIC_TLV_IP6_ADDR_LIST:
            {
                cJSON *addrList = cJSON_CreateArray();

                for (uint16_t i = 0; i < diagTlv.mData.mIp6AddrList.mCount; ++i)
                {
                    cJSON_AddItemToArray(addrList, IpAddr2Json(diagTlv.mData.mIp6AddrList.mList[i]));
                }
                cJSON_AddItemToObject(diagInfoOfOneNode, "IP6AddressList", addrList);
            }
            break;
            case OT_NETWORK_DIAGNOSTIC_TLV_MAC_COUNTERS:
            {
                cJSON_AddItemToObject(diagInfoOfOneNode, "MACCounters", MacCounters2Json(diagTlv.mData.mMacCounters));
            }
            break;
            case OT_NETWORK_DIAGNOSTIC_TLV_BATTERY_LEVEL:
            {
                cJSON_AddItemToObject(diagInfoOfOneNode, "BatteryLevel",
                                      cJSON_CreateNumber(diagTlv.mData.mBatteryLevel));
            }
            break;
            case OT_NETWORK_DIAGNOSTIC_TLV_SUPPLY_VOLTAGE:
            {
                cJSON_AddItemToObject(diagInfoOfOneNode, "SupplyVoltage",
                                      cJSON_CreateNumber(diagTlv.mData.mSupplyVoltage));
            }

            break;
            case OT_NETWORK_DIAGNOSTIC_TLV_CHILD_TABLE:
            {
                cJSON *tableList = cJSON_CreateArray();

                for (uint16_t i = 0; i < diagTlv.mData.mChildTable.mCount; ++i)
                {
                    cJSON_AddItemToArray(tableList, ChildTableEntry2Json(diagTlv.mData.mChildTable.mTable[i]));
                }

                cJSON_AddItemToObject(diagInfoOfOneNode, "ChildTable", tableList);
            }
            break;
            case OT_NETWORK_DIAGNOSTIC_TLV_CHANNEL_PAGES:
            {
                cJSON_AddItemToObject(
                    diagInfoOfOneNode, "ChannelPages",
                    Bytes2HexJson(diagTlv.mData.mChannelPages.m8, diagTlv.mData.mChannelPages.mCount));
            }
            break;
            case OT_NETWORK_DIAGNOSTIC_TLV_MAX_CHILD_TIMEOUT:
            {
                cJSON_AddItemToObject(diagInfoOfOneNode, "MaxChildTimeout",
                                      cJSON_CreateNumber(diagTlv.mData.mMaxChildTimeout));
            }

            break;
            default:
                break;
            }
        }
        cJSON_AddItemToArray(diagInfo, diagInfoOfOneNode);
    }

    ret = Json2String(diagInfo);

    cJSON_Delete(diagInfo);

    return ret;
}

std::string Bytes2HexJsonString(const uint8_t *aBytes, uint8_t aLength)
{
    cJSON *     hex = Bytes2HexJson(aBytes, aLength);
    std::string ret = Json2String(hex);

    cJSON_Delete(hex);

    return ret;
}

std::string Number2JsonString(const uint32_t aNumber)
{
    cJSON *     number = cJSON_CreateNumber(aNumber);
    std::string ret    = Json2String(number);

    cJSON_Delete(number);

    return ret;
}

std::string Mode2JsonString(const otLinkModeConfig &aMode)
{
    cJSON *     mode = Mode2Json(aMode);
    std::string ret  = Json2String(mode);

    cJSON_Delete(mode);

    return ret;
}

std::string Connectivity2JsonString(const otNetworkDiagConnectivity &aConnectivity)
{
    cJSON *     connectivity = Connectivity2Json(aConnectivity);
    std::string ret          = Json2String(connectivity);

    cJSON_Delete(connectivity);

    return ret;
}

std::string RouteData2JsonString(const otNetworkDiagRouteData &aRouteData)
{
    cJSON *     routeData = RouteData2Json(aRouteData);
    std::string ret       = Json2String(routeData);

    cJSON_Delete(routeData);

    return ret;
}

std::string Route2JsonString(const otNetworkDiagRoute &aRoute)
{
    cJSON *     route = Route2Json(aRoute);
    std::string ret   = Json2String(route);

    cJSON_Delete(route);

    return ret;
}

std::string LeaderData2JsonString(const otLeaderData &aLeaderData)
{
    cJSON *     leaderData = LeaderData2Json(aLeaderData);
    std::string ret        = Json2String(leaderData);

    cJSON_Delete(leaderData);

    return ret;
}

std::string MacCounters2JsonString(const otNetworkDiagMacCounters &aMacCounters)
{
    cJSON *     macCounters = MacCounters2Json(aMacCounters);
    std::string ret         = Json2String(macCounters);

    cJSON_Delete(macCounters);

    return ret;
}

std::string ChildTableEntry2JsonString(const otNetworkDiagChildEntry &aChildEntry)
{
    cJSON *     childEntry = ChildTableEntry2Json(aChildEntry);
    std::string ret        = Json2String(childEntry);

    cJSON_Delete(childEntry);

    return ret;
}

std::string CString2JsonString(const char *aString)
{
    cJSON *     cString = CString2Json(aString);
    std::string ret     = Json2String(cString);

    cJSON_Delete(cString);

    return ret;
}

std::string Error2JsonString(uint32_t aErrorCode, std::string aErrorMessage)
{
    char        code[5];
    std::string ret;
    cJSON *     error = cJSON_CreateObject();

    snprintf(code, 5, "%d", aErrorCode);

    cJSON_AddItemToObject(error, "ErrorCode", cJSON_CreateString(code));

    cJSON_AddItemToObject(error, "ErrorMessage", cJSON_CreateString(aErrorMessage.c_str()));

    ret = Json2String(error);

    cJSON_Delete(error);

    return ret;
}

Network JsonString2Network(std::string aString)
{
    cJSON* value;
    cJSON* jsonOut;
    Network network;
    otbrLog(OTBR_LOG_ERR, "start  %s", aString.c_str());
    
    jsonOut  = cJSON_Parse(aString.c_str());
    value = cJSON_GetObjectItemCaseSensitive(jsonOut, "networkKey");
    if (cJSON_IsString(value) && (value->valuestring != nullptr))
    {   
        network.networkKey = std::string(value->valuestring);
        otbrLog(OTBR_LOG_ERR, "network key", network.networkKey.c_str());
    }

    value = cJSON_GetObjectItemCaseSensitive(jsonOut, "prefix");
    if (cJSON_IsString(value) && (value->valuestring != nullptr))
    {   
        network.prefix = std::string(value->valuestring);
        // if (network.prefix.find('/') == std::string::npos)
        // {
        //     network.prefix += "/64";
        // }
    }
    
    value = cJSON_GetObjectItemCaseSensitive(jsonOut, "channel");

    if (cJSON_IsNumber(value))
    {   
        network.channel = static_cast<uint32_t>(value->valueint);
    }
    
    value = cJSON_GetObjectItemCaseSensitive(jsonOut, "networkName");
    
    if (cJSON_IsString(value) && (value->valuestring != nullptr))
    {   
        network.networkName = std::string(value->valuestring);
        otbrLog(OTBR_LOG_ERR, "panid %s", network.networkName.c_str());
    }


    value = cJSON_GetObjectItemCaseSensitive(jsonOut, "passphrase");

    if (cJSON_IsString(value) && (value->valuestring != nullptr))
    {   
        network.passphrase = std::string(value->valuestring);
    }


    value = cJSON_GetObjectItemCaseSensitive(jsonOut, "panId");

    if (cJSON_IsString(value) && (value->valuestring != nullptr))
    {   
        network.panId = std::string(value->valuestring);
        otbrLog(OTBR_LOG_ERR, "panid %s", network.panId.c_str());
    }

    value = cJSON_GetObjectItemCaseSensitive(jsonOut, "extPanId");

    if (cJSON_IsString(value) && (value->valuestring != nullptr))
    {   
        network.extPanId = std::string(value->valuestring);
    }

    value = cJSON_GetObjectItemCaseSensitive(jsonOut, "defaultRoute");
    if (cJSON_IsTrue(value))
    {   
        network.defaultRoute = true;
    }
    else
    {
        network.defaultRoute = false;
    }
    otbrLog(OTBR_LOG_ERR, "finish");
    return network;

    // otbr::Utils::Hex2Bytes(network.extPanId.c_str(), extPanIdBytes, OT_EXTENDED_PANID_LENGTH);
    // otbr::Utils::Bytes2Hex(psk.ComputePskc(network.extPanIdBytes, network.networkName.c_str(), network.passphrase.c_str()), OT_PSKC_MAX_LENGTH,
    //                        network.pskcStr);
}

std::string JsonString2String(std::string aString, std::string aKey)
{
    cJSON* value;
    cJSON* jsonOut;
    std::string ret;

    jsonOut  = cJSON_Parse(aString.c_str());

    value = cJSON_GetObjectItemCaseSensitive(jsonOut, aKey.c_str());
    if (cJSON_IsString(value) && (value->valuestring != nullptr))
    {   
        ret = std::string(value->valuestring);
    }

    return ret;


}
bool JsonString2Bool(std::string aString, std::string aKey)
{
    cJSON* value;
    bool ret = false;
    jsonOut  = cJSON_Parse(aString.c_str());
    value = cJSON_GetObjectItemCaseSensitive(jsonOut, aKey.c_str());
    if (cJSON_IsTrue(value)))
    {   
        ret = true;
    }
    return ret;
}

} // namespace JSON
} // namespace rest
} // namespace otbr
