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
#include "types.hpp"
#include <map>
#include <set>
#include <sstream>

#include "common/code_utils.hpp"
#include "common/types.hpp"
#include "openthread/mesh_diag.h"
#include "openthread/platform/radio.h"
#include "utils/string_utils.hpp"

extern "C" {
#include <cJSON.h>
}

#ifndef BITS_PER_BYTE
#define BITS_PER_BYTE 8
#endif

namespace otbr {
namespace rest {
namespace Json {

/**
 * @brief Concat str1, '.', and str2.
 *
 * @param str1 A string.
 * @param str2 Another string.
 * @return std::string str1.str2
 */
std::string concat(const char *str1, const char *str2)
{
    return (std::string(str1) + "." + std::string(str2));
}

/**
 * @brief Check if aKey is part of aSet.
 *
 * @param aSet A set of strings representing keys.
 * @param aKey A key
 * @return true  If aSet is empty (default, for return any key),
 *               aKey is in aSet, or both,
 *               aKey = 'atoplevelKey.asecondlevelKey' AND 'atoplevelKey.' are in aSet.
 * @return false Otherwise.
 *
 * the logic in this implementation relates to parsing the query to the set aSet
 * as implemented by BasicDiagnostics::parseQueryFieldValues
 *
 */
static bool hasKey(std::set<std::string> aSet, std::string aKey)
{
    bool        ret = false;
    std::string topKey;
    std::size_t pos;

    if (aSet.empty())
    {
        // defaults to has any key
        ret = true;
        ExitNow();
    }

    // for a second level key in aKey
    // check if the top level key is present as well
    pos = aKey.find(".");
    if (pos != std::string::npos)
    {
        topKey = aKey.substr(0, pos + 1);
    }

    if (!topKey.empty() && aSet.find(aKey) != aSet.end() && aSet.find(topKey) != aSet.end())
    {
        ret = true;
        ExitNow();
    }

    // simple key 'aKey' or
    // comes here when 'atoplevelKey.asecondlevelKey' is not in aSet
    pos = aKey.find(".");
    if (pos != std::string::npos)
    {
        // ignore asecondlevelKey and look for 'atoplevelKey'
        aKey = aKey.substr(0, pos);
    }

    if (aSet.find(aKey) != aSet.end())
    {
        ret = true;
    }

exit:
    return ret;
}

static cJSON *Bytes2HexJson(const uint8_t *aBytes, uint8_t aLength)
{
    char hex[2 * aLength + 1];

    otbr::Utils::Bytes2Hex(aBytes, aLength, hex);
    hex[2 * aLength] = '\0';

    return cJSON_CreateString(StringUtils::ToLowercase(hex).c_str());
}

static cJSON *Number2HexJson(const uint16_t aNumber)
{
    char hex[6 + 1] = {'\0'};

    snprintf(hex, sizeof(hex), "0x%04X", aNumber);

    return cJSON_CreateString(StringUtils::ToLowercase(hex).c_str());
}

std::string String2JsonString(const std::string &aString)
{
    std::string ret;
    cJSON      *json    = nullptr;
    char       *jsonOut = nullptr;

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

bool JsonString2String(const std::string &aJsonString, std::string &aString)
{
    cJSON *jsonString;
    bool   ret = true;

    VerifyOrExit((jsonString = cJSON_Parse(aJsonString.c_str())) != nullptr, ret = false);
    VerifyOrExit(cJSON_IsString(jsonString), ret = false);

    aString = std::string(jsonString->valuestring);

exit:
    cJSON_Delete(jsonString);

    return ret;
}

std::string Json2String(const cJSON *aJson)
{
    std::string ret;
    char       *jsonOut = nullptr;

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

static cJSON *Mode2Json(const otLinkModeConfig &aMode)
{
    cJSON *mode = cJSON_CreateObject();

    cJSON_AddItemToObject(mode, "rxOnWhenIdle", cJSON_CreateBool(aMode.mRxOnWhenIdle));
    cJSON_AddItemToObject(mode, "deviceTypeFTD", cJSON_CreateBool(aMode.mDeviceType));
    cJSON_AddItemToObject(mode, "fullNetworkData", cJSON_CreateBool(aMode.mNetworkData));

    return mode;
}

static cJSON *IpAddr2Json(const otIp6Address &aAddress)
{
    Ip6Address addr(aAddress.mFields.m8);

    return cJSON_CreateString(addr.ToString().c_str());
}

static cJSON *IpPrefix2Json(const otIp6NetworkPrefix &aAddress)
{
    std::stringstream ss;
    otIp6Address      address = {};

    address.mFields.mComponents.mNetworkPrefix = aAddress;
    Ip6Address addr(address.mFields.m8);

    ss << addr.ToString() << "/" << OT_IP6_PREFIX_BITSIZE;

    return cJSON_CreateString(ss.str().c_str());
}

otbrError Json2IpPrefix(const cJSON *aJson, otIp6NetworkPrefix &aIpPrefix)
{
    otbrError          error = OTBR_ERROR_NONE;
    std::istringstream ipPrefixStr(std::string(aJson->valuestring));
    std::string        tmp;
    Ip6Address         addr;

    VerifyOrExit(std::getline(ipPrefixStr, tmp, '/'), error = OTBR_ERROR_INVALID_ARGS);
    VerifyOrExit((error = addr.FromString(tmp.c_str(), addr)) == OTBR_ERROR_NONE);

    memcpy(aIpPrefix.m8, addr.m8, OT_IP6_PREFIX_SIZE);
exit:
    return error;
}

static cJSON *Timestamp2Json(const otTimestamp &aTimestamp)
{
    cJSON *timestamp = cJSON_CreateObject();

    cJSON_AddItemToObject(timestamp, "seconds", cJSON_CreateNumber(aTimestamp.mSeconds));
    cJSON_AddItemToObject(timestamp, "ticks", cJSON_CreateNumber(aTimestamp.mTicks));
    cJSON_AddItemToObject(timestamp, "authoritative", cJSON_CreateBool(aTimestamp.mAuthoritative));

    return timestamp;
}

bool Json2Timestamp(const cJSON *jsonTimestamp, otTimestamp &aTimestamp)
{
    cJSON *value;

    value = cJSON_GetObjectItemCaseSensitive(jsonTimestamp, "seconds");
    if (cJSON_IsNumber(value))
    {
        aTimestamp.mSeconds = static_cast<uint64_t>(value->valuedouble);
    }
    else if (value != nullptr)
    {
        return false;
    }

    value = cJSON_GetObjectItemCaseSensitive(jsonTimestamp, "ticks");
    if (cJSON_IsNumber(value))
    {
        aTimestamp.mTicks = static_cast<uint16_t>(value->valueint);
    }
    else if (value != nullptr)
    {
        return false;
    }

    value                     = cJSON_GetObjectItemCaseSensitive(jsonTimestamp, "authoritative");
    aTimestamp.mAuthoritative = cJSON_IsTrue(value);

    return true;
}

static cJSON *SecurityPolicy2Json(const otSecurityPolicy &aSecurityPolicy)
{
    cJSON *securityPolicy = cJSON_CreateObject();

    cJSON_AddItemToObject(securityPolicy, "rotationTime", cJSON_CreateNumber(aSecurityPolicy.mRotationTime));
    cJSON_AddItemToObject(securityPolicy, "obtainNetworkKey",
                          cJSON_CreateBool(aSecurityPolicy.mObtainNetworkKeyEnabled));
    cJSON_AddItemToObject(securityPolicy, "nativeCommissioning",
                          cJSON_CreateBool(aSecurityPolicy.mNativeCommissioningEnabled));
    cJSON_AddItemToObject(securityPolicy, "Routers", cJSON_CreateBool(aSecurityPolicy.mRoutersEnabled));
    cJSON_AddItemToObject(securityPolicy, "externalCommissioning",
                          cJSON_CreateBool(aSecurityPolicy.mExternalCommissioningEnabled));
    cJSON_AddItemToObject(securityPolicy, "commercialCommissioning",
                          cJSON_CreateBool(aSecurityPolicy.mCommercialCommissioningEnabled));
    cJSON_AddItemToObject(securityPolicy, "autonomousEnrollment",
                          cJSON_CreateBool(aSecurityPolicy.mAutonomousEnrollmentEnabled));
    cJSON_AddItemToObject(securityPolicy, "networkKeyProvisioning",
                          cJSON_CreateBool(aSecurityPolicy.mNetworkKeyProvisioningEnabled));
    cJSON_AddItemToObject(securityPolicy, "tobleLink", cJSON_CreateBool(aSecurityPolicy.mTobleLinkEnabled));
    cJSON_AddItemToObject(securityPolicy, "nonCcmRouters", cJSON_CreateBool(aSecurityPolicy.mNonCcmRoutersEnabled));

    return securityPolicy;
}

bool Json2SecurityPolicy(const cJSON *jsonSecurityPolicy, otSecurityPolicy &aSecurityPolicy)
{
    cJSON *value;

    value = cJSON_GetObjectItemCaseSensitive(jsonSecurityPolicy, "rotationTime");
    if (cJSON_IsNumber(value))
    {
        aSecurityPolicy.mRotationTime = static_cast<uint16_t>(value->valueint);
    }

    value                                    = cJSON_GetObjectItemCaseSensitive(jsonSecurityPolicy, "obtainNetworkKey");
    aSecurityPolicy.mObtainNetworkKeyEnabled = cJSON_IsTrue(value);
    value = cJSON_GetObjectItemCaseSensitive(jsonSecurityPolicy, "nativeCommissioning");
    aSecurityPolicy.mNativeCommissioningEnabled = cJSON_IsTrue(value);
    value                                       = cJSON_GetObjectItemCaseSensitive(jsonSecurityPolicy, "routers");
    aSecurityPolicy.mRoutersEnabled             = cJSON_IsTrue(value);
    value = cJSON_GetObjectItemCaseSensitive(jsonSecurityPolicy, "externalCommissioning");
    aSecurityPolicy.mExternalCommissioningEnabled = cJSON_IsTrue(value);
    value = cJSON_GetObjectItemCaseSensitive(jsonSecurityPolicy, "commercialCommissioning");
    aSecurityPolicy.mCommercialCommissioningEnabled = cJSON_IsTrue(value);
    value = cJSON_GetObjectItemCaseSensitive(jsonSecurityPolicy, "autonomousEnrollment");
    aSecurityPolicy.mAutonomousEnrollmentEnabled = cJSON_IsTrue(value);
    value = cJSON_GetObjectItemCaseSensitive(jsonSecurityPolicy, "networkKeyProvisioning");
    aSecurityPolicy.mNetworkKeyProvisioningEnabled = cJSON_IsTrue(value);
    value                                          = cJSON_GetObjectItemCaseSensitive(jsonSecurityPolicy, "tobleLink");
    aSecurityPolicy.mTobleLinkEnabled              = cJSON_IsTrue(value);
    value                                 = cJSON_GetObjectItemCaseSensitive(jsonSecurityPolicy, "nonCcmRouters");
    aSecurityPolicy.mNonCcmRoutersEnabled = cJSON_IsTrue(value);

    return true;
}

static cJSON *ChildTableEntry2Json(const otNetworkDiagChildEntry &aChildEntry)
{
    cJSON *childEntry = cJSON_CreateObject();

    cJSON_AddItemToObject(childEntry, "childId", cJSON_CreateNumber(aChildEntry.mChildId));
    cJSON_AddItemToObject(childEntry, "timeout", cJSON_CreateNumber(aChildEntry.mTimeout));
    cJSON_AddItemToObject(childEntry, "linkQuality", cJSON_CreateNumber(aChildEntry.mLinkQuality));

    cJSON *mode = Mode2Json(aChildEntry.mMode);
    cJSON_AddItemToObject(childEntry, "mode", mode);

    return childEntry;
}

static cJSON *MacCounters2Json(const otNetworkDiagMacCounters &aMacCounters)
{
    cJSON *macCounters = cJSON_CreateObject();

    cJSON_AddItemToObject(macCounters, "ifInUnknownProtos", cJSON_CreateNumber(aMacCounters.mIfInUnknownProtos));
    cJSON_AddItemToObject(macCounters, "ifInErrors", cJSON_CreateNumber(aMacCounters.mIfInErrors));
    cJSON_AddItemToObject(macCounters, "ifOutErrors", cJSON_CreateNumber(aMacCounters.mIfOutErrors));
    cJSON_AddItemToObject(macCounters, "ifInUcastPkts", cJSON_CreateNumber(aMacCounters.mIfInUcastPkts));
    cJSON_AddItemToObject(macCounters, "ifInBroadcastPkts", cJSON_CreateNumber(aMacCounters.mIfInBroadcastPkts));
    cJSON_AddItemToObject(macCounters, "ifInDiscards", cJSON_CreateNumber(aMacCounters.mIfInDiscards));
    cJSON_AddItemToObject(macCounters, "ifOutUcastPkts", cJSON_CreateNumber(aMacCounters.mIfOutUcastPkts));
    cJSON_AddItemToObject(macCounters, "ifOutBroadcastPkts", cJSON_CreateNumber(aMacCounters.mIfOutBroadcastPkts));
    cJSON_AddItemToObject(macCounters, "ifOutDiscards", cJSON_CreateNumber(aMacCounters.mIfOutDiscards));

    return macCounters;
}

static cJSON *Connectivity2Json(const otNetworkDiagConnectivity &aConnectivity)
{
    cJSON *connectivity = cJSON_CreateObject();

    cJSON_AddItemToObject(connectivity, "parentPriority", cJSON_CreateNumber(aConnectivity.mParentPriority));
    cJSON_AddItemToObject(connectivity, "linkQuality3", cJSON_CreateNumber(aConnectivity.mLinkQuality3));
    cJSON_AddItemToObject(connectivity, "linkQuality2", cJSON_CreateNumber(aConnectivity.mLinkQuality2));
    cJSON_AddItemToObject(connectivity, "linkQuality1", cJSON_CreateNumber(aConnectivity.mLinkQuality1));
    cJSON_AddItemToObject(connectivity, "leaderCost", cJSON_CreateNumber(aConnectivity.mLeaderCost));
    cJSON_AddItemToObject(connectivity, "idSequence", cJSON_CreateNumber(aConnectivity.mIdSequence));
    cJSON_AddItemToObject(connectivity, "activeRouters", cJSON_CreateNumber(aConnectivity.mActiveRouters));
    cJSON_AddItemToObject(connectivity, "sedBufferSize", cJSON_CreateNumber(aConnectivity.mSedBufferSize));
    cJSON_AddItemToObject(connectivity, "sedDatagramCount", cJSON_CreateNumber(aConnectivity.mSedDatagramCount));

    return connectivity;
}

static cJSON *RouteData2Json(const otNetworkDiagRouteData &aRouteData)
{
    cJSON *routeData = cJSON_CreateObject();

    cJSON_AddItemToObject(routeData, "routeId", cJSON_CreateNumber(aRouteData.mRouterId));
    cJSON_AddItemToObject(routeData, "linkQualityOut", cJSON_CreateNumber(aRouteData.mLinkQualityOut));
    cJSON_AddItemToObject(routeData, "linkQualityIn", cJSON_CreateNumber(aRouteData.mLinkQualityIn));
    cJSON_AddItemToObject(routeData, "routeCost", cJSON_CreateNumber(aRouteData.mRouteCost));

    return routeData;
}

static cJSON *Route2Json(const otNetworkDiagRoute &aRoute)
{
    cJSON *route = cJSON_CreateObject();

    cJSON_AddItemToObject(route, "idSequence", cJSON_CreateNumber(aRoute.mIdSequence));

    cJSON *RouteData = cJSON_CreateArray();
    for (uint16_t i = 0; i < aRoute.mRouteCount; ++i)
    {
        cJSON *RouteDatavalue = RouteData2Json(aRoute.mRouteData[i]);
        cJSON_AddItemToArray(RouteData, RouteDatavalue);
    }

    cJSON_AddItemToObject(route, "routeData", RouteData);

    return route;
}

static cJSON *LeaderData2Json(const otLeaderData &aLeaderData)
{
    cJSON *leaderData = cJSON_CreateObject();

    cJSON_AddItemToObject(leaderData, "partitionId", cJSON_CreateNumber(aLeaderData.mPartitionId));
    cJSON_AddItemToObject(leaderData, "weighting", cJSON_CreateNumber(aLeaderData.mWeighting));
    cJSON_AddItemToObject(leaderData, "dataVersion", cJSON_CreateNumber(aLeaderData.mDataVersion));
    cJSON_AddItemToObject(leaderData, "stableDataVersion", cJSON_CreateNumber(aLeaderData.mStableDataVersion));
    cJSON_AddItemToObject(leaderData, "leaderRouterId", cJSON_CreateNumber(aLeaderData.mLeaderRouterId));

    return leaderData;
}

std::string IpAddr2JsonString(const otIp6Address &aAddress)
{
    std::string ret;
    cJSON      *ipAddr = IpAddr2Json(aAddress);

    ret = Json2String(ipAddr);
    cJSON_Delete(ipAddr);

    return ret;
}

std::string Diag2JsonString(const std::vector<std::vector<otNetworkDiagTlv>> &aDiagSet)
{
    cJSON      *diagInfo          = cJSON_CreateArray();
    cJSON      *diagInfoOfOneNode = nullptr;
    cJSON      *addrList          = nullptr;
    cJSON      *tableList         = nullptr;
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

                cJSON_AddItemToObject(diagInfoOfOneNode, "extAddress",
                                      Bytes2HexJson(diagTlv.mData.mExtAddress.m8, OT_EXT_ADDRESS_SIZE));

                break;
            case OT_NETWORK_DIAGNOSTIC_TLV_SHORT_ADDRESS:

                cJSON_AddItemToObject(diagInfoOfOneNode, "rloc16", cJSON_CreateNumber(diagTlv.mData.mAddr16));

                break;
            case OT_NETWORK_DIAGNOSTIC_TLV_MODE:

                cJSON_AddItemToObject(diagInfoOfOneNode, "mode", Mode2Json(diagTlv.mData.mMode));

                break;
            case OT_NETWORK_DIAGNOSTIC_TLV_TIMEOUT:

                timeout = static_cast<uint64_t>(diagTlv.mData.mTimeout);
                cJSON_AddItemToObject(diagInfoOfOneNode, "timeout", cJSON_CreateNumber(timeout));

                break;
            case OT_NETWORK_DIAGNOSTIC_TLV_CONNECTIVITY:

                cJSON_AddItemToObject(diagInfoOfOneNode, "connectivity",
                                      Connectivity2Json(diagTlv.mData.mConnectivity));

                break;
            case OT_NETWORK_DIAGNOSTIC_TLV_ROUTE:

                cJSON_AddItemToObject(diagInfoOfOneNode, "route", Route2Json(diagTlv.mData.mRoute));

                break;
            case OT_NETWORK_DIAGNOSTIC_TLV_LEADER_DATA:

                cJSON_AddItemToObject(diagInfoOfOneNode, "leaderData", LeaderData2Json(diagTlv.mData.mLeaderData));

                break;
            case OT_NETWORK_DIAGNOSTIC_TLV_NETWORK_DATA:

                cJSON_AddItemToObject(diagInfoOfOneNode, "networkData",
                                      Bytes2HexJson(diagTlv.mData.mNetworkData.m8, diagTlv.mData.mNetworkData.mCount));

                break;
            case OT_NETWORK_DIAGNOSTIC_TLV_IP6_ADDR_LIST:

                addrList = cJSON_CreateArray();

                for (uint16_t i = 0; i < diagTlv.mData.mIp6AddrList.mCount; ++i)
                {
                    cJSON_AddItemToArray(addrList, IpAddr2Json(diagTlv.mData.mIp6AddrList.mList[i]));
                }
                cJSON_AddItemToObject(diagInfoOfOneNode, "ip6AddressList", addrList);

                break;
            case OT_NETWORK_DIAGNOSTIC_TLV_MAC_COUNTERS:

                cJSON_AddItemToObject(diagInfoOfOneNode, "macCounters", MacCounters2Json(diagTlv.mData.mMacCounters));

                break;
            case OT_NETWORK_DIAGNOSTIC_TLV_BATTERY_LEVEL:

                cJSON_AddItemToObject(diagInfoOfOneNode, "batteryLevel",
                                      cJSON_CreateNumber(diagTlv.mData.mBatteryLevel));

                break;
            case OT_NETWORK_DIAGNOSTIC_TLV_SUPPLY_VOLTAGE:

                cJSON_AddItemToObject(diagInfoOfOneNode, "supplyVoltage",
                                      cJSON_CreateNumber(diagTlv.mData.mSupplyVoltage));

                break;
            case OT_NETWORK_DIAGNOSTIC_TLV_CHILD_TABLE:

                tableList = cJSON_CreateArray();

                for (uint16_t i = 0; i < diagTlv.mData.mChildTable.mCount; ++i)
                {
                    cJSON_AddItemToArray(tableList, ChildTableEntry2Json(diagTlv.mData.mChildTable.mTable[i]));
                }

                cJSON_AddItemToObject(diagInfoOfOneNode, "childTable", tableList);

                break;
            case OT_NETWORK_DIAGNOSTIC_TLV_CHANNEL_PAGES:

                cJSON_AddItemToObject(
                    diagInfoOfOneNode, "channelPages",
                    Bytes2HexJson(diagTlv.mData.mChannelPages.m8, diagTlv.mData.mChannelPages.mCount));

                break;
            case OT_NETWORK_DIAGNOSTIC_TLV_MAX_CHILD_TIMEOUT:

                cJSON_AddItemToObject(diagInfoOfOneNode, "maxChildTimeout",
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

cJSON *Node2Json(const NodeInfo &aNode, std::set<std::string> aFieldset)
{
    cJSON *node = cJSON_CreateObject();

    if (hasKey(aFieldset, KEY_BORDERAGENTID))
    {
        cJSON_AddItemToObject(node, KEY_BORDERAGENTID, Bytes2HexJson(aNode.mBaId.mId, sizeof(aNode.mBaId)));
    }
    // if (hasKey(aFieldset, KEY_BORDERAGENTSTATE))
    // {
    //     cJSON_AddItemToObject(node, KEY_BORDERAGENTSTATE, cJSON_CreateNumber(aNode.mBaState));
    // }
    if (hasKey(aFieldset, KEY_STATE))
    {
        cJSON_AddItemToObject(node, KEY_STATE, cJSON_CreateString(aNode.mRole.c_str()));
    }
    if (hasKey(aFieldset, KEY_ROUTERCOUNT))
    {
        cJSON_AddItemToObject(node, KEY_ROUTERCOUNT, cJSON_CreateNumber(aNode.mNumOfRouter));
    }
    if (hasKey(aFieldset, KEY_RLOC16_IPV6ADDRESS))
    {
        cJSON_AddItemToObject(node, KEY_RLOC16_IPV6ADDRESS, IpAddr2Json(aNode.mRlocAddress));
    }
    if (hasKey(aFieldset, KEY_EXTADDRESS))
    {
        cJSON_AddItemToObject(node, KEY_EXTADDRESS, Bytes2HexJson(aNode.mExtAddress, OT_EXT_ADDRESS_SIZE));
    }
    if (hasKey(aFieldset, KEY_NETWORKNAME))
    {
        cJSON_AddItemToObject(node, KEY_NETWORKNAME, cJSON_CreateString(aNode.mNetworkName.c_str()));
    }
    if (hasKey(aFieldset, KEY_RLOC16))
    {
        cJSON_AddItemToObject(node, KEY_RLOC16, Number2HexJson(aNode.mRloc16));
    }
    if (hasKey(aFieldset, KEY_ROUTERID))
    {
        if ((aNode.mRloc16 & CHILD_MASK) == 0)
        {
            cJSON_AddItemToObject(node, KEY_ROUTERID, cJSON_CreateNumber(aNode.mRloc16 >> 10));
        }
    }
    if (hasKey(aFieldset, KEY_LEADERDATA))
    {
        cJSON_AddItemToObject(node, KEY_LEADERDATA, LeaderData2Json(aNode.mLeaderData));
    }
    if (hasKey(aFieldset, KEY_EXTPANID))
    {
        cJSON_AddItemToObject(node, KEY_EXTPANID, Bytes2HexJson(aNode.mExtPanId, OT_EXT_PAN_ID_SIZE));
    }

    return node;
}

std::string Node2JsonString(const NodeInfo &aNode)
{
    std::string           ret;
    std::set<std::string> fieldset;
    cJSON                *node = Node2Json(aNode, fieldset);

    ret = Json2String(node);
    cJSON_Delete(node);

    return ret;
}

std::string SparseNode2JsonString(const NodeInfo &aNode, std::set<std::string> aFieldset)
{
    std::string ret;
    cJSON      *node = Node2Json(aNode, aFieldset);

    ret = Json2String(node);
    cJSON_Delete(node);

    return ret;
}

cJSON *brCounter2Json(const otBorderRoutingCounters *aBrCounters)
{
    cJSON *ret = cJSON_CreateObject();

    cJSON_AddNumberToObject(ret, "ifInUcastPkts", aBrCounters->mInboundUnicast.mPackets);
    cJSON_AddNumberToObject(ret, "ifInBroadcastPkts", aBrCounters->mInboundMulticast.mPackets);
    cJSON_AddNumberToObject(ret, "ifOutUcastPkts", aBrCounters->mOutboundUnicast.mPackets);
    cJSON_AddNumberToObject(ret, "ifOutBroadcastPkts", aBrCounters->mOutboundMulticast.mPackets);
    cJSON_AddNumberToObject(ret, "raRx", aBrCounters->mRaRx);
    cJSON_AddNumberToObject(ret, "raTxSuccess", aBrCounters->mRaTxSuccess);
    cJSON_AddNumberToObject(ret, "raTxFailed", aBrCounters->mRaTxFailure);
    cJSON_AddNumberToObject(ret, "rsRx", aBrCounters->mRsRx);
    cJSON_AddNumberToObject(ret, "rsTxSuccess", aBrCounters->mRsTxSuccess);
    cJSON_AddNumberToObject(ret, "rsTxFailed", aBrCounters->mRsTxFailure);

    return ret;
}

std::string jsonStr2JsonApiItem(std::string aId, const std::string aType, std::string aAttribute)
{
    cJSON      *root = cJSON_CreateObject();
    std::string ret;

    cJSON_AddStringToObject(root, "id", aId.c_str());
    cJSON_AddStringToObject(root, "type", aType.c_str());
    cJSON_AddItemToObject(root, "attributes", cJSON_Parse(aAttribute.c_str()));

    ret = Json2String(root);
    cJSON_Delete(root);

    return ret;
}

std::string jsonStr2JsonApiItem(std::string aId, const std::string aType, std::string aAttribute, cJSON *aRelationship)
{
    cJSON      *root = cJSON_CreateObject();
    std::string ret;

    cJSON_AddStringToObject(root, "id", aId.c_str());
    cJSON_AddStringToObject(root, "type", aType.c_str());
    cJSON_AddItemToObject(root, "attributes", cJSON_Parse(aAttribute.c_str()));
    cJSON_AddItemToObject(root, "relationships", cJSON_Duplicate(aRelationship, true));

    ret = Json2String(root);
    cJSON_Delete(root);

    return ret;
}

std::string jsonStr2JsonApiColl(std::string data, std::string meta)
{
    cJSON      *root = cJSON_CreateObject();
    std::string ret;

    cJSON_AddItemToObject(root, "data", cJSON_Parse(data.c_str()));

    if (meta.length() > 0)
    {
        cJSON_AddItemToObject(root, "meta", cJSON_Parse(meta.c_str()));
    }

    ret = Json2String(root);
    cJSON_Delete(root);

    return ret;
}

std::string Bytes2HexJsonString(const uint8_t *aBytes, uint8_t aLength)
{
    cJSON      *hex = Bytes2HexJson(aBytes, aLength);
    std::string ret = Json2String(hex);

    cJSON_Delete(hex);

    return ret;
}

int Hex2BytesJsonString(const std::string &aHexString, uint8_t *aBytes, uint8_t aMaxLength)
{
    return otbr::Utils::Hex2Bytes(aHexString.c_str(), aBytes, aMaxLength);
}

std::string Number2JsonString(const uint32_t &aNumber)
{
    cJSON      *number = cJSON_CreateNumber(aNumber);
    std::string ret    = Json2String(number);

    cJSON_Delete(number);

    return ret;
}

std::string Mode2JsonString(const otLinkModeConfig &aMode)
{
    cJSON      *mode = Mode2Json(aMode);
    std::string ret  = Json2String(mode);

    cJSON_Delete(mode);

    return ret;
}

std::string Connectivity2JsonString(const otNetworkDiagConnectivity &aConnectivity)
{
    cJSON      *connectivity = Connectivity2Json(aConnectivity);
    std::string ret          = Json2String(connectivity);

    cJSON_Delete(connectivity);

    return ret;
}

std::string RouteData2JsonString(const otNetworkDiagRouteData &aRouteData)
{
    cJSON      *routeData = RouteData2Json(aRouteData);
    std::string ret       = Json2String(routeData);

    cJSON_Delete(routeData);

    return ret;
}

std::string Route2JsonString(const otNetworkDiagRoute &aRoute)
{
    cJSON      *route = Route2Json(aRoute);
    std::string ret   = Json2String(route);

    cJSON_Delete(route);

    return ret;
}

std::string LeaderData2JsonString(const otLeaderData &aLeaderData)
{
    cJSON      *leaderData = LeaderData2Json(aLeaderData);
    std::string ret        = Json2String(leaderData);

    cJSON_Delete(leaderData);

    return ret;
}

std::string MacCounters2JsonString(const otNetworkDiagMacCounters &aMacCounters)
{
    cJSON      *macCounters = MacCounters2Json(aMacCounters);
    std::string ret         = Json2String(macCounters);

    cJSON_Delete(macCounters);

    return ret;
}

std::string ChildTableEntry2JsonString(const otNetworkDiagChildEntry &aChildEntry)
{
    cJSON      *childEntry = ChildTableEntry2Json(aChildEntry);
    std::string ret        = Json2String(childEntry);

    cJSON_Delete(childEntry);

    return ret;
}

std::string CString2JsonString(const char *aCString)
{
    cJSON      *cString = CString2Json(aCString);
    std::string ret     = Json2String(cString);

    cJSON_Delete(cString);

    return ret;
}

std::string Error2JsonString(HttpStatusCode aErrorCode, std::string aErrorMessage)
{
    std::string ret;
    cJSON      *error = cJSON_CreateObject();

    cJSON_AddItemToObject(error, "errorCode", cJSON_CreateNumber(static_cast<int16_t>(aErrorCode)));
    cJSON_AddItemToObject(error, "errorMessage", cJSON_CreateString(aErrorMessage.c_str()));

    ret = Json2String(error);

    cJSON_Delete(error);

    return ret;
}

cJSON *ActiveDataset2Json(const otOperationalDataset &aActiveDataset)
{
    cJSON *node = cJSON_CreateObject();

    if (aActiveDataset.mComponents.mIsActiveTimestampPresent)
    {
        cJSON_AddItemToObject(node, "activeTimestamp", Timestamp2Json(aActiveDataset.mActiveTimestamp));
    }
    if (aActiveDataset.mComponents.mIsNetworkKeyPresent)
    {
        cJSON_AddItemToObject(node, "networkKey", Bytes2HexJson(aActiveDataset.mNetworkKey.m8, OT_NETWORK_KEY_SIZE));
    }
    if (aActiveDataset.mComponents.mIsNetworkNamePresent)
    {
        cJSON_AddItemToObject(node, "networkName", cJSON_CreateString(aActiveDataset.mNetworkName.m8));
    }
    if (aActiveDataset.mComponents.mIsExtendedPanIdPresent)
    {
        cJSON_AddItemToObject(node, "extPanId", Bytes2HexJson(aActiveDataset.mExtendedPanId.m8, OT_EXT_PAN_ID_SIZE));
    }
    if (aActiveDataset.mComponents.mIsMeshLocalPrefixPresent)
    {
        cJSON_AddItemToObject(node, "meshLocalPrefix", IpPrefix2Json(aActiveDataset.mMeshLocalPrefix));
    }
    if (aActiveDataset.mComponents.mIsPanIdPresent)
    {
        cJSON_AddItemToObject(node, "panId", cJSON_CreateNumber(aActiveDataset.mPanId));
    }
    if (aActiveDataset.mComponents.mIsChannelPresent)
    {
        cJSON_AddItemToObject(node, "channel", cJSON_CreateNumber(aActiveDataset.mChannel));
    }
    if (aActiveDataset.mComponents.mIsPskcPresent)
    {
        cJSON_AddItemToObject(node, "pskc", Bytes2HexJson(aActiveDataset.mPskc.m8, OT_PSKC_MAX_SIZE));
    }
    if (aActiveDataset.mComponents.mIsSecurityPolicyPresent)
    {
        cJSON_AddItemToObject(node, "securityPolicy", SecurityPolicy2Json(aActiveDataset.mSecurityPolicy));
    }
    if (aActiveDataset.mComponents.mIsChannelMaskPresent)
    {
        cJSON_AddItemToObject(node, "channelMask", cJSON_CreateNumber(aActiveDataset.mChannelMask));
    }

    return node;
}

std::string ActiveDataset2JsonString(const otOperationalDataset &aActiveDataset)
{
    cJSON      *node;
    std::string ret;

    node = ActiveDataset2Json(aActiveDataset);
    ret  = Json2String(node);
    cJSON_Delete(node);

    return ret;
}

std::string PendingDataset2JsonString(const otOperationalDataset &aPendingDataset)
{
    cJSON      *nodeActiveDataset;
    cJSON      *node = cJSON_CreateObject();
    std::string ret;

    nodeActiveDataset = ActiveDataset2Json(aPendingDataset);
    cJSON_AddItemToObject(node, "activeDataset", nodeActiveDataset);
    if (aPendingDataset.mComponents.mIsPendingTimestampPresent)
    {
        cJSON_AddItemToObject(node, "pendingTimestamp", Timestamp2Json(aPendingDataset.mPendingTimestamp));
    }
    if (aPendingDataset.mComponents.mIsDelayPresent)
    {
        cJSON_AddItemToObject(node, "delay", cJSON_CreateNumber(aPendingDataset.mDelay));
    }

    ret = Json2String(node);
    cJSON_Delete(node);

    return ret;
}

bool JsonActiveDataset2Dataset(const cJSON *jsonActiveDataset, otOperationalDataset &aDataset)
{
    cJSON      *value;
    otTimestamp timestamp;
    bool        ret = true;

    value = cJSON_GetObjectItemCaseSensitive(jsonActiveDataset, "activeTimestamp");
    if (cJSON_IsObject(value))
    {
        VerifyOrExit(Json2Timestamp(value, timestamp), ret = false);
        aDataset.mActiveTimestamp                      = timestamp;
        aDataset.mComponents.mIsActiveTimestampPresent = true;
    }
    else if (cJSON_IsNull(value))
    {
        aDataset.mComponents.mIsActiveTimestampPresent = false;
    }
    else if (value != nullptr)
    {
        ExitNow(ret = false);
    }

    value = cJSON_GetObjectItemCaseSensitive(jsonActiveDataset, "networkKey");
    if (cJSON_IsString(value))
    {
        VerifyOrExit(value->valuestring != nullptr, ret = false);
        VerifyOrExit(Hex2BytesJsonString(std::string(value->valuestring), aDataset.mNetworkKey.m8,
                                         OT_NETWORK_KEY_SIZE) == OT_NETWORK_KEY_SIZE,
                     ret = false);
        aDataset.mComponents.mIsNetworkKeyPresent = true;
    }
    else if (cJSON_IsNull(value))
    {
        aDataset.mComponents.mIsNetworkKeyPresent = false;
    }

    value = cJSON_GetObjectItemCaseSensitive(jsonActiveDataset, "networkName");
    if (cJSON_IsString(value))
    {
        VerifyOrExit(value->valuestring != nullptr, ret = false);
        VerifyOrExit(strlen(value->valuestring) <= OT_NETWORK_NAME_MAX_SIZE, ret = false);
        strncpy(aDataset.mNetworkName.m8, value->valuestring, OT_NETWORK_NAME_MAX_SIZE);
        aDataset.mComponents.mIsNetworkNamePresent = true;
    }
    else if (cJSON_IsNull(value))
    {
        aDataset.mComponents.mIsNetworkNamePresent = false;
    }

    value = cJSON_GetObjectItemCaseSensitive(jsonActiveDataset, "extPanId");
    if (cJSON_IsString(value))
    {
        VerifyOrExit(value->valuestring != nullptr, ret = false);
        VerifyOrExit(Hex2BytesJsonString(std::string(value->valuestring), aDataset.mExtendedPanId.m8,
                                         OT_EXT_PAN_ID_SIZE) == OT_EXT_PAN_ID_SIZE,
                     ret = false);
        aDataset.mComponents.mIsExtendedPanIdPresent = true;
    }
    else if (cJSON_IsNull(value))
    {
        aDataset.mComponents.mIsExtendedPanIdPresent = false;
    }

    value = cJSON_GetObjectItemCaseSensitive(jsonActiveDataset, "meshLocalPrefix");
    if (cJSON_IsString(value))
    {
        VerifyOrExit(value->valuestring != nullptr, ret = false);
        VerifyOrExit(Json2IpPrefix(value, aDataset.mMeshLocalPrefix) == OTBR_ERROR_NONE, ret = false);
        aDataset.mComponents.mIsMeshLocalPrefixPresent = true;
    }
    else if (cJSON_IsNull(value))
    {
        aDataset.mComponents.mIsMeshLocalPrefixPresent = false;
    }

    value = cJSON_GetObjectItemCaseSensitive(jsonActiveDataset, "panId");
    if (cJSON_IsNumber(value))
    {
        aDataset.mPanId                      = static_cast<otPanId>(value->valueint);
        aDataset.mComponents.mIsPanIdPresent = true;
    }
    else if (cJSON_IsNull(value))
    {
        aDataset.mComponents.mIsPanIdPresent = false;
    }

    value = cJSON_GetObjectItemCaseSensitive(jsonActiveDataset, "channel");
    if (cJSON_IsNumber(value))
    {
        aDataset.mChannel                      = static_cast<uint16_t>(value->valueint);
        aDataset.mComponents.mIsChannelPresent = true;
    }
    else if (cJSON_IsNull(value))
    {
        aDataset.mComponents.mIsChannelPresent = false;
    }

    value = cJSON_GetObjectItemCaseSensitive(jsonActiveDataset, "pskc");
    if (cJSON_IsString(value))
    {
        VerifyOrExit(value->valuestring != nullptr, ret = false);
        VerifyOrExit(Hex2BytesJsonString(std::string(value->valuestring), aDataset.mPskc.m8, OT_PSKC_MAX_SIZE) ==
                         OT_PSKC_MAX_SIZE,
                     ret = false);
        aDataset.mComponents.mIsPskcPresent = true;
    }
    else if (cJSON_IsNull(value))
    {
        aDataset.mComponents.mIsPskcPresent = false;
    }

    value = cJSON_GetObjectItemCaseSensitive(jsonActiveDataset, "securityPolicy");
    if (cJSON_IsObject(value))
    {
        VerifyOrExit(Json2SecurityPolicy(value, aDataset.mSecurityPolicy), ret = false);
        aDataset.mComponents.mIsSecurityPolicyPresent = true;
    }
    else if (cJSON_IsNull(value))
    {
        aDataset.mComponents.mIsSecurityPolicyPresent = false;
    }

    value = cJSON_GetObjectItemCaseSensitive(jsonActiveDataset, "channelMask");
    if (cJSON_IsNumber(value))
    {
        aDataset.mChannelMask                      = value->valueint;
        aDataset.mComponents.mIsChannelMaskPresent = true;
    }
    else if (cJSON_IsNull(value))
    {
        aDataset.mComponents.mIsChannelMaskPresent = false;
    }

exit:
    return ret;
}

bool JsonActiveDatasetString2Dataset(const std::string &aJsonActiveDataset, otOperationalDataset &aDataset)
{
    cJSON *jsonActiveDataset;
    bool   ret = true;

    VerifyOrExit((jsonActiveDataset = cJSON_Parse(aJsonActiveDataset.c_str())) != nullptr, ret = false);
    VerifyOrExit(cJSON_IsObject(jsonActiveDataset), ret = false);

    ret = JsonActiveDataset2Dataset(jsonActiveDataset, aDataset);

exit:
    cJSON_Delete(jsonActiveDataset);

    return ret;
}

bool JsonPendingDatasetString2Dataset(const std::string &aJsonPendingDataset, otOperationalDataset &aDataset)
{
    cJSON      *value;
    cJSON      *jsonDataset;
    otTimestamp timestamp;
    bool        ret = true;

    VerifyOrExit((jsonDataset = cJSON_Parse(aJsonPendingDataset.c_str())) != nullptr, ret = false);
    VerifyOrExit(cJSON_IsObject(jsonDataset), ret = false);

    value = cJSON_GetObjectItemCaseSensitive(jsonDataset, "activeDataset");
    if (cJSON_IsObject(value))
    {
        VerifyOrExit(JsonActiveDataset2Dataset(value, aDataset), ret = false);
    }
    else if (cJSON_IsString(value))
    {
        otOperationalDatasetTlvs datasetTlvs;
        int                      len;

        len =
            Hex2BytesJsonString(std::string(value->valuestring), datasetTlvs.mTlvs, OT_OPERATIONAL_DATASET_MAX_LENGTH);
        VerifyOrExit(len > 0, ret = false);
        datasetTlvs.mLength = len;

        VerifyOrExit(otDatasetParseTlvs(&datasetTlvs, &aDataset) == OT_ERROR_NONE, ret = false);
    }
    else
    {
        ExitNow(ret = false);
    }

    value = cJSON_GetObjectItemCaseSensitive(jsonDataset, "pendingTimestamp");
    if (cJSON_IsObject(value))
    {
        VerifyOrExit(Json2Timestamp(value, timestamp), ret = false);
        aDataset.mPendingTimestamp                      = timestamp;
        aDataset.mComponents.mIsPendingTimestampPresent = true;
    }
    else if (cJSON_IsNull(value))
    {
        aDataset.mComponents.mIsPendingTimestampPresent = false;
    }
    else if (value != nullptr)
    {
        ExitNow(ret = false);
    }

    value = cJSON_GetObjectItemCaseSensitive(jsonDataset, "delay");
    if (cJSON_IsNumber(value))
    {
        aDataset.mDelay                      = value->valueint;
        aDataset.mComponents.mIsDelayPresent = true;
    }
    else if (cJSON_IsNull(value))
    {
        aDataset.mComponents.mIsDelayPresent = false;
    }

exit:
    cJSON_Delete(jsonDataset);

    return ret;
}

cJSON *JoinerInfo2Json(const otJoinerInfo &aJoinerInfo)
{
    cJSON *node = cJSON_CreateObject();

    cJSON_AddItemToObject(node, "Pskd", cJSON_CreateString(aJoinerInfo.mPskd.m8));
    if (aJoinerInfo.mType == OT_JOINER_INFO_TYPE_EUI64)
    {
        cJSON_AddItemToObject(node, "Eui64", Bytes2HexJson(aJoinerInfo.mSharedId.mEui64.m8, OT_EXT_ADDRESS_SIZE));
    }
    else if (aJoinerInfo.mType == OT_JOINER_INFO_TYPE_DISCERNER)
    {
        char hexValue[((OT_JOINER_MAX_DISCERNER_LENGTH / 8) * 2) + 1]                              = {0};
        char string[sizeof("0x") + ((OT_JOINER_MAX_DISCERNER_LENGTH / 8) * 2) + sizeof("/xx") + 1] = {0};

        otbr::Utils::Long2Hex(aJoinerInfo.mSharedId.mDiscerner.mValue, hexValue);
        snprintf(string, sizeof(string), "0x%s/%d", hexValue, aJoinerInfo.mSharedId.mDiscerner.mLength);
        cJSON_AddItemToObject(node, "Discerner", cJSON_CreateString(string));
    }
    else
    {
        cJSON_AddItemToObject(node, "JoinerId", cJSON_CreateString("*"));
    }
    cJSON_AddItemToObject(node, "Timeout", cJSON_CreateNumber(aJoinerInfo.mExpirationTime));

    return node;
}

std::string JoinerInfo2JsonString(const otJoinerInfo &aJoinerInfo)
{
    cJSON      *node;
    std::string ret;

    node = JoinerInfo2Json(aJoinerInfo);
    ret  = Json2String(node);
    cJSON_Delete(node);

    return ret;
}

static cJSON *AddDevice2Json(const std::string &aEui, const std::string &aPskd, std::set<std::string> aFieldset)
{
    cJSON *root = cJSON_CreateObject();

    if (hasKey(aFieldset, KEY_EUI64))
    {
        cJSON_AddItemToObject(root, KEY_EUI64, cJSON_CreateString(aEui.c_str()));
    }

    if (hasKey(aFieldset, KEY_PSKD))
    {
        cJSON_AddItemToObject(root, KEY_PSKD, cJSON_CreateString(aPskd.c_str()));
    }

    return root;
}

std::string AddDevice2JsonString(const std::string &aEui, const std::string &aPskd)
{
    std::string           ret;
    std::set<std::string> fieldset;
    cJSON                *json = AddDevice2Json(aEui, aPskd, fieldset);

    ret = Json2String(json);
    cJSON_Delete(json);

    return ret;
}

std::string SparseAddDevice2JsonString(const std::string    &aEui,
                                       const std::string    &aPskd,
                                       std::set<std::string> aFieldset)
{
    std::string ret;
    cJSON      *json = AddDevice2Json(aEui, aPskd, aFieldset);

    ret = Json2String(json);
    cJSON_Delete(json);

    return ret;
}

otbrError StringDiscerner2Discerner(char *aString, otJoinerDiscerner &aDiscerner)
{
    otbrError error = OTBR_ERROR_NONE;
    char     *separator;
    uint8_t   byteLength;
    uint8_t   byteSwapBuffer[OT_JOINER_MAX_DISCERNER_LENGTH / BITS_PER_BYTE] = {0};
    uint8_t  *buffer                                                         = (uint8_t *)&aDiscerner.mValue;

    separator = strstr(aString, "/");
    VerifyOrExit(separator != nullptr, error = OTBR_ERROR_NOT_FOUND);
    VerifyOrExit(sscanf(separator + 1, "%hhu", &aDiscerner.mLength) == 1, error = OTBR_ERROR_INVALID_ARGS);
    VerifyOrExit(aDiscerner.mLength > 0 && aDiscerner.mLength <= OT_JOINER_MAX_DISCERNER_LENGTH,
                 error = OTBR_ERROR_INVALID_ARGS);

    if (memcmp(aString, "0x", 2) == 0)
    {
        aString += 2;
    }

    *separator = '\0';
    byteLength = Hex2BytesJsonString(std::string(aString), byteSwapBuffer, OT_JOINER_MAX_DISCERNER_LENGTH);
    VerifyOrExit(byteLength <= (1 + ((aDiscerner.mLength - 1) / BITS_PER_BYTE)), error = OTBR_ERROR_INVALID_ARGS);

    // The discerner is expected to be big endian
    for (uint8_t i = 0; i < byteLength; i++)
    {
        buffer[i] = byteSwapBuffer[byteLength - i - 1];
    }

exit:
    return error;
}

bool JsonJoinerInfo2JoinerInfo(const cJSON *jsonJoinerInfo, otJoinerInfo &aJoinerInfo)
{
    cJSON *value;
    bool   ret = false;

    aJoinerInfo.mType = OT_JOINER_INFO_TYPE_ANY;
    memset(&aJoinerInfo.mSharedId.mEui64, 0, sizeof(aJoinerInfo.mSharedId.mEui64));
    memset(&aJoinerInfo.mPskd.m8, 0, sizeof(aJoinerInfo.mPskd.m8));

    value = cJSON_GetObjectItemCaseSensitive(jsonJoinerInfo, "Pskd");
    if (cJSON_IsString(value))
    {
        VerifyOrExit(value->valuestring != nullptr);
        VerifyOrExit(strlen(value->valuestring) <= OT_JOINER_MAX_PSKD_LENGTH);
        strncpy(aJoinerInfo.mPskd.m8, value->valuestring, OT_JOINER_MAX_PSKD_LENGTH);
    }
    else
    {
        ExitNow();
    }

    value = cJSON_GetObjectItemCaseSensitive(jsonJoinerInfo, "JoinerId");
    if (cJSON_IsString(value))
    {
        VerifyOrExit(aJoinerInfo.mType == OT_JOINER_INFO_TYPE_ANY);
        VerifyOrExit(value->valuestring != nullptr);
        if (strncmp(value->valuestring, "*", 1) != 0)
        {
            otbrError err = StringDiscerner2Discerner(value->valuestring, aJoinerInfo.mSharedId.mDiscerner);
            if (err == OTBR_ERROR_NOT_FOUND)
            {
                VerifyOrExit(Hex2BytesJsonString(std::string(value->valuestring), aJoinerInfo.mSharedId.mEui64.m8,
                                                 OT_EXT_ADDRESS_SIZE) == OT_EXT_ADDRESS_SIZE);
                aJoinerInfo.mType = OT_JOINER_INFO_TYPE_EUI64;
            }
            else
            {
                VerifyOrExit(err == OTBR_ERROR_NONE);
                aJoinerInfo.mType = OT_JOINER_INFO_TYPE_DISCERNER;
            }
        }
    }

    value = cJSON_GetObjectItemCaseSensitive(jsonJoinerInfo, "Discerner");
    if (cJSON_IsString(value))
    {
        VerifyOrExit(aJoinerInfo.mType == OT_JOINER_INFO_TYPE_ANY);
        VerifyOrExit(value->valuestring != nullptr);
        if (strncmp(value->valuestring, "*", 1) != 0)
        {
            VerifyOrExit(StringDiscerner2Discerner(value->valuestring, aJoinerInfo.mSharedId.mDiscerner) ==
                         OTBR_ERROR_NONE);
            aJoinerInfo.mType = OT_JOINER_INFO_TYPE_DISCERNER;
        }
    }

    value = cJSON_GetObjectItemCaseSensitive(jsonJoinerInfo, "Eui64");
    if (cJSON_IsString(value))
    {
        VerifyOrExit(aJoinerInfo.mType == OT_JOINER_INFO_TYPE_ANY);
        VerifyOrExit(value->valuestring != nullptr);
        if (strncmp(value->valuestring, "*", 1) != 0)
        {
            VerifyOrExit(Hex2BytesJsonString(std::string(value->valuestring), aJoinerInfo.mSharedId.mEui64.m8,
                                             OT_EXT_ADDRESS_SIZE) == OT_EXT_ADDRESS_SIZE);
            aJoinerInfo.mType = OT_JOINER_INFO_TYPE_EUI64;
        }
    }

    aJoinerInfo.mExpirationTime = 60;
    value                       = cJSON_GetObjectItemCaseSensitive(jsonJoinerInfo, "Timeout");
    if (cJSON_IsNumber(value))
    {
        aJoinerInfo.mExpirationTime = value->valueint;
    }

    ret = true;
exit:
    return ret;
}

bool JsonJoinerInfoString2JoinerInfo(const std::string &aJsonJoinerInfo, otJoinerInfo &aJoinerInfo)
{
    cJSON *jsonJoinerInfo;
    bool   ret = true;

    VerifyOrExit((jsonJoinerInfo = cJSON_Parse(aJsonJoinerInfo.c_str())) != nullptr, ret = false);
    VerifyOrExit(cJSON_IsObject(jsonJoinerInfo), ret = false);

    ret = JsonJoinerInfo2JoinerInfo(jsonJoinerInfo, aJoinerInfo);

exit:
    cJSON_Delete(jsonJoinerInfo);

    return ret;
}

cJSON *JoinerTable2Json(const std::vector<otJoinerInfo> &aJoinerTable)
{
    cJSON *table = cJSON_CreateArray();

    for (const otJoinerInfo joiner : aJoinerTable)
    {
        cJSON *joinerJson = JoinerInfo2Json(joiner);
        cJSON_AddItemToArray(table, joinerJson);
    }

    return table;
}

std::string JoinerTable2JsonString(const std::vector<otJoinerInfo> &aJoinerTable)
{
    return Json2String(JoinerTable2Json(aJoinerTable));
}

cJSON *CreateMetaCollection(uint32_t aOffset, uint32_t aLimit, uint32_t aTotal)
{
    cJSON *meta            = cJSON_CreateObject();
    cJSON *meta_collection = cJSON_CreateObject();

    cJSON_AddNumberToObject(meta_collection, "offset", aOffset);
    if (aLimit > 0)
    {
        cJSON_AddNumberToObject(meta_collection, "limit", aLimit);
    }
    cJSON_AddNumberToObject(meta_collection, "total", aTotal);
    cJSON_AddItemToObject(meta, "collection", meta_collection);

    return meta;
}

cJSON *CreateMetaCollection(uint32_t aOffset, uint32_t aLimit, uint32_t aTotal, uint32_t aPending)
{
    cJSON *meta            = cJSON_CreateObject();
    cJSON *meta_collection = cJSON_CreateObject();

    cJSON_AddNumberToObject(meta_collection, "offset", aOffset);
    if (aLimit > 0)
    {
        cJSON_AddNumberToObject(meta_collection, "limit", aLimit);
    }
    cJSON_AddNumberToObject(meta_collection, "total", aTotal);
    cJSON_AddItemToObject(meta_collection, "pending", cJSON_CreateNumber(aPending));
    cJSON_AddItemToObject(meta, "collection", meta_collection);

    return meta;
}

} // namespace Json
} // namespace rest
} // namespace otbr
