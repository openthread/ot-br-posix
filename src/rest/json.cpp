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
#include <cJSON.h>
}

namespace otbr {
namespace Rest {
namespace Json {

static cJSON *Bytes2HexJson(const uint8_t *aBytes, uint8_t aLength)
{
    char hex[2 * aLength + 1];

    otbr::Utils::Bytes2Hex(aBytes, aLength, hex);
    for (auto &ch : hex)
    {
        ch = tolower(ch);
    }

    return cJSON_CreateString(hex);
}

static cJSON *Prefix2Json(const otIp6Prefix &aPrefix)
{
    std::string serilizedPrefix;
    uint16_t    twoBytes;
    char        prefix[5];

    for (size_t i = 0; i < 8; ++i)
    {
        if (i % 2 == 1)
        {
            twoBytes = (aPrefix.mPrefix.mFields.m8[i - 1] << 8) + aPrefix.mPrefix.mFields.m8[i];
            sprintf(prefix, "%x", twoBytes);
            if (i > 1)
            {
                serilizedPrefix += ":";
            }
            serilizedPrefix += std::string(prefix);
        }
    }

    return cJSON_CreateString(serilizedPrefix.c_str());
}

std::string String2JsonString(const std::string &aString)
{
    std::string ret;
    cJSON *     json    = nullptr;
    char *      jsonOut = nullptr;

    VerifyOrExit(aString.size() > 0);

    json    = cJSON_CreateString(aString.c_str());
    jsonOut = cJSON_Print(json);
    ret     = jsonOut;
    if (jsonOut != nullptr)
    {
        cJSON_free(jsonOut);
        jsonOut = nullptr;
    }

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
    if (jsonOut != nullptr)
    {
        cJSON_free(jsonOut);
        jsonOut = nullptr;
    }

exit:
    return ret;
}

static cJSON *CString2Json(const char *aString)
{
    return cJSON_CreateString(aString);
}

static cJSON *ScanNetworks2Json(const std::vector<ActiveScanResult> &aResults)
{
    cJSON *networks = cJSON_CreateArray();
    for (const auto &result : aResults)
    {
        cJSON *network = cJSON_CreateObject();

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
static cJSON *Mode2Json(const otLinkModeConfig &aMode)
{
    cJSON *mode = cJSON_CreateObject();

    cJSON_AddItemToObject(mode, "RxOnWhenIdle", cJSON_CreateNumber(aMode.mRxOnWhenIdle));
    cJSON_AddItemToObject(mode, "DeviceType", cJSON_CreateNumber(aMode.mDeviceType));
    cJSON_AddItemToObject(mode, "NetworkData", cJSON_CreateNumber(aMode.mNetworkData));

    return mode;
}

static cJSON *Addr2Json(const uint8_t *aAddress, size_t aSize)
{
    std::string serilizedAddr;
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

    return cJSON_CreateString(serilizedAddr.c_str());
}

static cJSON *IpAddr2Json(const otIp6Address &aAddress)
{
    std::string serilizedIpAddr;
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

    return cJSON_CreateString(serilizedIpAddr.c_str());
}

static cJSON *ChildTableEntry2Json(const otNetworkDiagChildEntry &aChildEntry)
{
    cJSON *childEntry = cJSON_CreateObject();

    cJSON_AddItemToObject(childEntry, "ChildId", cJSON_CreateNumber(aChildEntry.mChildId));
    cJSON_AddItemToObject(childEntry, "Timeout", cJSON_CreateNumber(aChildEntry.mTimeout));

    cJSON *mode = Mode2Json(aChildEntry.mMode);
    cJSON_AddItemToObject(childEntry, "Mode", mode);

    return childEntry;
}

static cJSON *MacCounters2Json(const otNetworkDiagMacCounters &aMacCounters)
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

static cJSON *Connectivity2Json(const otNetworkDiagConnectivity &aConnectivity)
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

static cJSON *RouteData2Json(const otNetworkDiagRouteData &aRouteData)
{
    cJSON *routeData = cJSON_CreateObject();

    cJSON_AddItemToObject(routeData, "RouteId", cJSON_CreateNumber(aRouteData.mRouterId));
    cJSON_AddItemToObject(routeData, "LinkQualityOut", cJSON_CreateNumber(aRouteData.mLinkQualityOut));
    cJSON_AddItemToObject(routeData, "LinkQualityIn", cJSON_CreateNumber(aRouteData.mLinkQualityIn));
    cJSON_AddItemToObject(routeData, "RouteCost", cJSON_CreateNumber(aRouteData.mRouteCost));

    return routeData;
}

static cJSON *Route2Json(const otNetworkDiagRoute &aRoute)
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

static cJSON *LeaderData2Json(const otLeaderData &aLeaderData)
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

std::string ScanNetworks2JsonString(const std::vector<ActiveScanResult> &aResults)
{
    std::string ret;
    cJSON *     networks = ScanNetworks2Json(aResults);

    ret = Json2String(networks);
    cJSON_Delete(networks);

    return ret;
}

std::string Network2JsonString(const otOperationalDataset &aDataset)
{
    cJSON *     network = cJSON_CreateObject();
    std::string ret;
    cJSON *     value = nullptr;

    cJSON_AddItemToObject(network, "MeshLocalPrefix",
                          Addr2Json(aDataset.mMeshLocalPrefix.m8, OT_MESH_LOCAL_PREFIX_SIZE));
    cJSON_AddItemToObject(network, "MasterKey", Bytes2HexJson(aDataset.mMasterKey.m8, OT_MASTER_KEY_SIZE));
    cJSON_AddItemToObject(network, "PanId", cJSON_CreateNumber(aDataset.mPanId));
    cJSON_AddItemToObject(network, "ChannelMask", cJSON_CreateNumber(aDataset.mChannelMask));
    cJSON_AddItemToObject(network, "Channel", cJSON_CreateNumber(aDataset.mChannel));

    value = cJSON_CreateArray();
    cJSON_AddItemToArray(value, cJSON_CreateNumber(aDataset.mSecurityPolicy.mRotationTime));
    cJSON_AddItemToArray(value, cJSON_CreateNumber(aDataset.mSecurityPolicy.mFlags));

    cJSON_AddItemToObject(network, "SecurityPolicy", value);
    cJSON_AddItemToObject(network, "PSKc", Bytes2HexJson(aDataset.mPskc.m8, OT_PSKC_MAX_SIZE));
    cJSON_AddItemToObject(network, "NetworkName", cJSON_CreateString(aDataset.mNetworkName.m8));
    cJSON_AddItemToObject(network, "ExtPanId", Bytes2HexJson(aDataset.mExtendedPanId.m8, OT_EXT_PAN_ID_SIZE));

    ret = Json2String(network);

    cJSON_Delete(network);

    return ret;
}

std::string Node2JsonString(const NodeInfo &aNode)
{
    cJSON *     node = cJSON_CreateObject();
    std::string ret;

    cJSON_AddItemToObject(node, "WPAN service", cJSON_CreateString(aNode.mWpanService.c_str()));
    cJSON_AddItemToObject(node, "NCP:State", cJSON_CreateNumber(aNode.mRole));
    cJSON_AddItemToObject(node, "NCP:Version", cJSON_CreateString(aNode.mVersion.c_str()));
    cJSON_AddItemToObject(node, "NCP:HardwareAddress", Bytes2HexJson(aNode.mEui64.m8, OT_EXT_ADDRESS_SIZE));
    cJSON_AddItemToObject(node, "NCP:Channel", cJSON_CreateNumber(aNode.mChannel));
    cJSON_AddItemToObject(node, "Network:NodeType", cJSON_CreateNumber(aNode.mRole));
    cJSON_AddItemToObject(node, "Network:Name", cJSON_CreateString(aNode.mNetworkName.c_str()));
    cJSON_AddItemToObject(node, "Network:XPANID", Bytes2HexJson(aNode.mExtPanId, OT_EXT_PAN_ID_SIZE));
    cJSON_AddItemToObject(node, "Network:PANID", cJSON_CreateNumber(aNode.mPanId));
    cJSON_AddItemToObject(node, "IPv6:MeshLocalPrefix", Addr2Json(aNode.mMeshLocalPrefix, OT_MESH_LOCAL_PREFIX_SIZE));
    cJSON_AddItemToObject(node, "IPv6:MeshLocalAddress", IpAddr2Json(aNode.mMeshLocalAddress));

    ret = Json2String(node);

    cJSON_Delete(node);

    return ret;
}

std::string Diag2JsonString(const std::vector<std::vector<otNetworkDiagTlv>> &aDiagSet)
{
    cJSON *     diagInfo          = cJSON_CreateArray();
    cJSON *     diagInfoOfOneNode = nullptr;
    cJSON *     addrList          = nullptr;
    cJSON *     tableList         = nullptr;
    std::string ret;
    uint64_t    timeout;

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

                cJSON_AddItemToObject(diagInfoOfOneNode, "Rloc16", cJSON_CreateNumber(diagTlv.mData.mAddr16));

                break;
            case OT_NETWORK_DIAGNOSTIC_TLV_MODE:

                cJSON_AddItemToObject(diagInfoOfOneNode, "Mode", Mode2Json(diagTlv.mData.mMode));

                break;
            case OT_NETWORK_DIAGNOSTIC_TLV_TIMEOUT:

                timeout = static_cast<uint64_t>(diagTlv.mData.mTimeout);
                cJSON_AddItemToObject(diagInfoOfOneNode, "Timeout", cJSON_CreateNumber(timeout));

                break;
            case OT_NETWORK_DIAGNOSTIC_TLV_CONNECTIVITY:

                cJSON_AddItemToObject(diagInfoOfOneNode, "Connectivity",
                                      Connectivity2Json(diagTlv.mData.mConnectivity));

                break;
            case OT_NETWORK_DIAGNOSTIC_TLV_ROUTE:

                cJSON_AddItemToObject(diagInfoOfOneNode, "Route", Route2Json(diagTlv.mData.mRoute));

                break;
            case OT_NETWORK_DIAGNOSTIC_TLV_LEADER_DATA:

                cJSON_AddItemToObject(diagInfoOfOneNode, "LeaderData", LeaderData2Json(diagTlv.mData.mLeaderData));

                break;
            case OT_NETWORK_DIAGNOSTIC_TLV_NETWORK_DATA:

                cJSON_AddItemToObject(diagInfoOfOneNode, "NetworkData",
                                      Bytes2HexJson(diagTlv.mData.mNetworkData.m8, diagTlv.mData.mNetworkData.mCount));

                break;
            case OT_NETWORK_DIAGNOSTIC_TLV_IP6_ADDR_LIST:

                addrList = cJSON_CreateArray();

                for (uint16_t i = 0; i < diagTlv.mData.mIp6AddrList.mCount; ++i)
                {
                    cJSON_AddItemToArray(addrList, IpAddr2Json(diagTlv.mData.mIp6AddrList.mList[i]));
                }
                cJSON_AddItemToObject(diagInfoOfOneNode, "IP6AddressList", addrList);

                break;
            case OT_NETWORK_DIAGNOSTIC_TLV_MAC_COUNTERS:

                cJSON_AddItemToObject(diagInfoOfOneNode, "MACCounters", MacCounters2Json(diagTlv.mData.mMacCounters));

                break;
            case OT_NETWORK_DIAGNOSTIC_TLV_BATTERY_LEVEL:

                cJSON_AddItemToObject(diagInfoOfOneNode, "BatteryLevel",
                                      cJSON_CreateNumber(diagTlv.mData.mBatteryLevel));

                break;
            case OT_NETWORK_DIAGNOSTIC_TLV_SUPPLY_VOLTAGE:

                cJSON_AddItemToObject(diagInfoOfOneNode, "SupplyVoltage",
                                      cJSON_CreateNumber(diagTlv.mData.mSupplyVoltage));

                break;
            case OT_NETWORK_DIAGNOSTIC_TLV_CHILD_TABLE:

                tableList = cJSON_CreateArray();

                for (uint16_t i = 0; i < diagTlv.mData.mChildTable.mCount; ++i)
                {
                    cJSON_AddItemToArray(tableList, ChildTableEntry2Json(diagTlv.mData.mChildTable.mTable[i]));
                }

                cJSON_AddItemToObject(diagInfoOfOneNode, "ChildTable", tableList);

                break;
            case OT_NETWORK_DIAGNOSTIC_TLV_CHANNEL_PAGES:

                cJSON_AddItemToObject(
                    diagInfoOfOneNode, "ChannelPages",
                    Bytes2HexJson(diagTlv.mData.mChannelPages.m8, diagTlv.mData.mChannelPages.mCount));

                break;
            case OT_NETWORK_DIAGNOSTIC_TLV_MAX_CHILD_TIMEOUT:

                cJSON_AddItemToObject(diagInfoOfOneNode, "MaxChildTimeout",
                                      cJSON_CreateNumber(diagTlv.mData.mMaxChildTimeout));

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

std::string Number2JsonString(const uint32_t &aNumber)
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

std::string PrefixList2JsonString(const std::vector<otBorderRouterConfig> &aConfig)
{
    std::string ret;

    cJSON *prefixList = cJSON_CreateArray();

    for (size_t i = 0; i < aConfig.size(); i++)
    {
        cJSON_AddItemToArray(prefixList, Prefix2Json(aConfig[i].mPrefix));
    }
    ret = Json2String(prefixList);

    cJSON_Delete(prefixList);

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

std::string CString2JsonString(const char *aCString)
{
    cJSON *     cString = CString2Json(aCString);
    std::string ret     = Json2String(cString);

    cJSON_Delete(cString);

    return ret;
}

std::string Error2JsonString(HttpStatusCode aErrorCode, std::string aErrorMessage, std::string aDescription)
{
    std::string ret;
    cJSON *     error = cJSON_CreateObject();

    cJSON_AddItemToObject(error, "ErrorCode", cJSON_CreateNumber(static_cast<int16_t>(aErrorCode)));
    cJSON_AddItemToObject(error, "ErrorMessage", cJSON_CreateString(aErrorMessage.c_str()));
    cJSON_AddItemToObject(error, "ErrorDescription", cJSON_CreateString(aDescription.c_str()));

    ret = Json2String(error);

    cJSON_Delete(error);

    return ret;
}
bool JsonString2NetworkConfiguration(std::string &aString, NetworkConfiguration &aNetwork)
{
    cJSON *value;
    cJSON *jsonOut;
    bool   ret = true;

    jsonOut = cJSON_Parse(aString.c_str());
    value   = cJSON_GetObjectItemCaseSensitive(jsonOut, "masterKey");
    VerifyOrExit(cJSON_IsString(value) && (value->valuestring != nullptr), ret = false);
    aNetwork.mMasterKey = std::string(value->valuestring);

    value = cJSON_GetObjectItemCaseSensitive(jsonOut, "prefix");
    VerifyOrExit(cJSON_IsString(value) && (value->valuestring != nullptr), ret = false);
    aNetwork.mPrefix = std::string(value->valuestring);

    value = cJSON_GetObjectItemCaseSensitive(jsonOut, "channel");
    VerifyOrExit(cJSON_IsNumber(value), ret = false);
    aNetwork.mChannel = static_cast<uint32_t>(value->valueint);

    value = cJSON_GetObjectItemCaseSensitive(jsonOut, "networkName");
    VerifyOrExit(cJSON_IsString(value) && (value->valuestring != nullptr), ret = false);
    aNetwork.mNetworkName = std::string(value->valuestring);

    value = cJSON_GetObjectItemCaseSensitive(jsonOut, "passphrase");
    VerifyOrExit(cJSON_IsString(value) && (value->valuestring != nullptr), ret = false);
    aNetwork.mPassphrase = std::string(value->valuestring);

    value = cJSON_GetObjectItemCaseSensitive(jsonOut, "panId");
    VerifyOrExit(cJSON_IsString(value) && (value->valuestring != nullptr), ret = false);
    aNetwork.mPanId = std::string(value->valuestring);

    value = cJSON_GetObjectItemCaseSensitive(jsonOut, "extPanId");
    VerifyOrExit(cJSON_IsString(value) && (value->valuestring != nullptr), ret = false);
    aNetwork.mExtPanId = std::string(value->valuestring);

    aNetwork.mDefaultRoute = false;
    value                  = cJSON_GetObjectItemCaseSensitive(jsonOut, "defaultRoute");
    VerifyOrExit(cJSON_IsBool(value), ret = false);
    if (cJSON_IsTrue(value))
    {
        aNetwork.mDefaultRoute = true;
    }

exit:

    cJSON_Delete(jsonOut);

    return ret;
}

bool JsonString2String(std::string aString, std::string aKey, std::string &aValue)
{
    cJSON *value;
    cJSON *jsonOut;
    bool   ret = true;

    jsonOut = cJSON_Parse(aString.c_str());

    value = cJSON_GetObjectItemCaseSensitive(jsonOut, aKey.c_str());
    if (cJSON_IsString(value) && (value->valuestring != nullptr))
    {
        aValue = std::string(value->valuestring);
    }
    else
    {
        ret = false;
    }

    cJSON_Delete(jsonOut);

    return ret;
}
bool JsonString2Bool(std::string aString, std::string aKey, bool &aValue)
{
    bool   ret     = true;
    cJSON *value   = nullptr;
    cJSON *jsonOut = nullptr;
    aValue         = false;
    jsonOut        = cJSON_Parse(aString.c_str());
    value          = cJSON_GetObjectItemCaseSensitive(jsonOut, aKey.c_str());
    VerifyOrExit(cJSON_IsBool(value), ret = false);
    if (cJSON_IsTrue(value))
    {
        aValue = true;
    }
    else
    {
        aValue = false;
    }

exit:

    cJSON_Delete(jsonOut);

    return ret;
}

bool JsonString2Int(std::string aString, std::string aKey, int32_t &aValue)
{
    bool   ret     = true;
    cJSON *value   = nullptr;
    cJSON *jsonOut = nullptr;

    jsonOut = cJSON_Parse(aString.c_str());
    value   = cJSON_GetObjectItemCaseSensitive(jsonOut, aKey.c_str());

    if (cJSON_IsNumber(value))
    {
        aValue = static_cast<int32_t>(value->valueint);
    }
    else
    {
        ret = false;
    }

    cJSON_Delete(jsonOut);

    return ret;
}

} // namespace Json
} // namespace Rest
} // namespace otbr
