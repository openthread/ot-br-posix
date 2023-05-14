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
#include <sstream>

#include "common/code_utils.hpp"
#include "common/types.hpp"

extern "C" {
#include <cJSON.h>
}

namespace otbr {
namespace rest {
namespace Json {

static cJSON *Bytes2HexJson(const uint8_t *aBytes, uint8_t aLength)
{
    char hex[2 * aLength + 1];

    otbr::Utils::Bytes2Hex(aBytes, aLength, hex);
    hex[2 * aLength] = '\0';

    return cJSON_CreateString(hex);
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

    cJSON_AddItemToObject(mode, "RxOnWhenIdle", cJSON_CreateNumber(aMode.mRxOnWhenIdle));
    cJSON_AddItemToObject(mode, "DeviceType", cJSON_CreateNumber(aMode.mDeviceType));
    cJSON_AddItemToObject(mode, "NetworkData", cJSON_CreateNumber(aMode.mNetworkData));

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

    cJSON_AddItemToObject(timestamp, "Seconds", cJSON_CreateNumber(aTimestamp.mSeconds));
    cJSON_AddItemToObject(timestamp, "Ticks", cJSON_CreateNumber(aTimestamp.mTicks));
    cJSON_AddItemToObject(timestamp, "Authoritative", cJSON_CreateBool(aTimestamp.mAuthoritative));

    return timestamp;
}

bool Json2Timestamp(const cJSON *jsonTimestamp, otTimestamp &aTimestamp)
{
    cJSON *value;

    value = cJSON_GetObjectItemCaseSensitive(jsonTimestamp, "Seconds");
    if (cJSON_IsNumber(value))
    {
        aTimestamp.mSeconds = static_cast<uint64_t>(value->valuedouble);
    }
    else if (value != nullptr)
    {
        return false;
    }

    value = cJSON_GetObjectItemCaseSensitive(jsonTimestamp, "Ticks");
    if (cJSON_IsNumber(value))
    {
        aTimestamp.mTicks = static_cast<uint16_t>(value->valueint);
    }
    else if (value != nullptr)
    {
        return false;
    }

    value                     = cJSON_GetObjectItemCaseSensitive(jsonTimestamp, "Authoritative");
    aTimestamp.mAuthoritative = cJSON_IsTrue(value);

    return true;
}

static cJSON *SecurityPolicy2Json(const otSecurityPolicy &aSecurityPolicy)
{
    cJSON *securityPolicy = cJSON_CreateObject();

    cJSON_AddItemToObject(securityPolicy, "RotationTime", cJSON_CreateNumber(aSecurityPolicy.mRotationTime));
    cJSON_AddItemToObject(securityPolicy, "ObtainNetworkKey",
                          cJSON_CreateBool(aSecurityPolicy.mObtainNetworkKeyEnabled));
    cJSON_AddItemToObject(securityPolicy, "NativeCommissioning",
                          cJSON_CreateBool(aSecurityPolicy.mNativeCommissioningEnabled));
    cJSON_AddItemToObject(securityPolicy, "Routers", cJSON_CreateBool(aSecurityPolicy.mRoutersEnabled));
    cJSON_AddItemToObject(securityPolicy, "ExternalCommissioning",
                          cJSON_CreateBool(aSecurityPolicy.mExternalCommissioningEnabled));
    cJSON_AddItemToObject(securityPolicy, "CommercialCommissioning",
                          cJSON_CreateBool(aSecurityPolicy.mCommercialCommissioningEnabled));
    cJSON_AddItemToObject(securityPolicy, "AutonomousEnrollment",
                          cJSON_CreateBool(aSecurityPolicy.mAutonomousEnrollmentEnabled));
    cJSON_AddItemToObject(securityPolicy, "NetworkKeyProvisioning",
                          cJSON_CreateBool(aSecurityPolicy.mNetworkKeyProvisioningEnabled));
    cJSON_AddItemToObject(securityPolicy, "TobleLink", cJSON_CreateBool(aSecurityPolicy.mTobleLinkEnabled));
    cJSON_AddItemToObject(securityPolicy, "NonCcmRouters", cJSON_CreateBool(aSecurityPolicy.mNonCcmRoutersEnabled));

    return securityPolicy;
}

bool Json2SecurityPolicy(const cJSON *jsonSecurityPolicy, otSecurityPolicy &aSecurityPolicy)
{
    cJSON *value;

    value = cJSON_GetObjectItemCaseSensitive(jsonSecurityPolicy, "RotationTime");
    if (cJSON_IsNumber(value))
    {
        aSecurityPolicy.mRotationTime = static_cast<uint16_t>(value->valueint);
    }

    value                                    = cJSON_GetObjectItemCaseSensitive(jsonSecurityPolicy, "ObtainNetworkKey");
    aSecurityPolicy.mObtainNetworkKeyEnabled = cJSON_IsTrue(value);
    value = cJSON_GetObjectItemCaseSensitive(jsonSecurityPolicy, "NativeCommissioning");
    aSecurityPolicy.mNativeCommissioningEnabled = cJSON_IsTrue(value);
    value                                       = cJSON_GetObjectItemCaseSensitive(jsonSecurityPolicy, "Routers");
    aSecurityPolicy.mRoutersEnabled             = cJSON_IsTrue(value);
    value = cJSON_GetObjectItemCaseSensitive(jsonSecurityPolicy, "ExternalCommissioning");
    aSecurityPolicy.mExternalCommissioningEnabled = cJSON_IsTrue(value);
    value = cJSON_GetObjectItemCaseSensitive(jsonSecurityPolicy, "CommercialCommissioning");
    aSecurityPolicy.mCommercialCommissioningEnabled = cJSON_IsTrue(value);
    value = cJSON_GetObjectItemCaseSensitive(jsonSecurityPolicy, "AutonomousEnrollment");
    aSecurityPolicy.mAutonomousEnrollmentEnabled = cJSON_IsTrue(value);
    value = cJSON_GetObjectItemCaseSensitive(jsonSecurityPolicy, "NetworkKeyProvisioning");
    aSecurityPolicy.mNetworkKeyProvisioningEnabled = cJSON_IsTrue(value);
    value                                          = cJSON_GetObjectItemCaseSensitive(jsonSecurityPolicy, "TobleLink");
    aSecurityPolicy.mTobleLinkEnabled              = cJSON_IsTrue(value);
    value                                 = cJSON_GetObjectItemCaseSensitive(jsonSecurityPolicy, "NonCcmRouters");
    aSecurityPolicy.mNonCcmRoutersEnabled = cJSON_IsTrue(value);

    return true;
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
    cJSON      *ipAddr = IpAddr2Json(aAddress);

    ret = Json2String(ipAddr);
    cJSON_Delete(ipAddr);

    return ret;
}

std::string Node2JsonString(const NodeInfo &aNode)
{
    cJSON      *node = cJSON_CreateObject();
    std::string ret;

    cJSON_AddItemToObject(node, "BaId", Bytes2HexJson(aNode.mBaId.mId, sizeof(aNode.mBaId)));
    cJSON_AddItemToObject(node, "State", cJSON_CreateString(aNode.mRole.c_str()));
    cJSON_AddItemToObject(node, "NumOfRouter", cJSON_CreateNumber(aNode.mNumOfRouter));
    cJSON_AddItemToObject(node, "RlocAddress", IpAddr2Json(aNode.mRlocAddress));
    cJSON_AddItemToObject(node, "ExtAddress", Bytes2HexJson(aNode.mExtAddress, OT_EXT_ADDRESS_SIZE));
    cJSON_AddItemToObject(node, "NetworkName", cJSON_CreateString(aNode.mNetworkName.c_str()));
    cJSON_AddItemToObject(node, "Rloc16", cJSON_CreateNumber(aNode.mRloc16));
    cJSON_AddItemToObject(node, "LeaderData", LeaderData2Json(aNode.mLeaderData));
    cJSON_AddItemToObject(node, "ExtPanId", Bytes2HexJson(aNode.mExtPanId, OT_EXT_PAN_ID_SIZE));

    ret = Json2String(node);
    cJSON_Delete(node);

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

    cJSON_AddItemToObject(error, "ErrorCode", cJSON_CreateNumber(static_cast<int16_t>(aErrorCode)));
    cJSON_AddItemToObject(error, "ErrorMessage", cJSON_CreateString(aErrorMessage.c_str()));

    ret = Json2String(error);

    cJSON_Delete(error);

    return ret;
}

cJSON *ActiveDataset2Json(const otOperationalDataset &aActiveDataset)
{
    cJSON *node = cJSON_CreateObject();

    if (aActiveDataset.mComponents.mIsActiveTimestampPresent)
    {
        cJSON_AddItemToObject(node, "ActiveTimestamp", Timestamp2Json(aActiveDataset.mActiveTimestamp));
    }
    if (aActiveDataset.mComponents.mIsNetworkKeyPresent)
    {
        cJSON_AddItemToObject(node, "NetworkKey", Bytes2HexJson(aActiveDataset.mNetworkKey.m8, OT_NETWORK_KEY_SIZE));
    }
    if (aActiveDataset.mComponents.mIsNetworkNamePresent)
    {
        cJSON_AddItemToObject(node, "NetworkName", cJSON_CreateString(aActiveDataset.mNetworkName.m8));
    }
    if (aActiveDataset.mComponents.mIsExtendedPanIdPresent)
    {
        cJSON_AddItemToObject(node, "ExtPanId", Bytes2HexJson(aActiveDataset.mExtendedPanId.m8, OT_EXT_PAN_ID_SIZE));
    }
    if (aActiveDataset.mComponents.mIsMeshLocalPrefixPresent)
    {
        cJSON_AddItemToObject(node, "MeshLocalPrefix", IpPrefix2Json(aActiveDataset.mMeshLocalPrefix));
    }
    if (aActiveDataset.mComponents.mIsPanIdPresent)
    {
        cJSON_AddItemToObject(node, "PanId", cJSON_CreateNumber(aActiveDataset.mPanId));
    }
    if (aActiveDataset.mComponents.mIsChannelPresent)
    {
        cJSON_AddItemToObject(node, "Channel", cJSON_CreateNumber(aActiveDataset.mChannel));
    }
    if (aActiveDataset.mComponents.mIsPskcPresent)
    {
        cJSON_AddItemToObject(node, "PSKc", Bytes2HexJson(aActiveDataset.mPskc.m8, OT_PSKC_MAX_SIZE));
    }
    if (aActiveDataset.mComponents.mIsSecurityPolicyPresent)
    {
        cJSON_AddItemToObject(node, "SecurityPolicy", SecurityPolicy2Json(aActiveDataset.mSecurityPolicy));
    }
    if (aActiveDataset.mComponents.mIsChannelMaskPresent)
    {
        cJSON_AddItemToObject(node, "ChannelMask", cJSON_CreateNumber(aActiveDataset.mChannelMask));
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
    cJSON_AddItemToObject(node, "ActiveDataset", nodeActiveDataset);
    if (aPendingDataset.mComponents.mIsPendingTimestampPresent)
    {
        cJSON_AddItemToObject(node, "PendingTimestamp", Timestamp2Json(aPendingDataset.mPendingTimestamp));
    }
    if (aPendingDataset.mComponents.mIsDelayPresent)
    {
        cJSON_AddItemToObject(node, "Delay", cJSON_CreateNumber(aPendingDataset.mDelay));
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

    value = cJSON_GetObjectItemCaseSensitive(jsonActiveDataset, "ActiveTimestamp");
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

    value = cJSON_GetObjectItemCaseSensitive(jsonActiveDataset, "NetworkKey");
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

    value = cJSON_GetObjectItemCaseSensitive(jsonActiveDataset, "NetworkName");
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

    value = cJSON_GetObjectItemCaseSensitive(jsonActiveDataset, "ExtPanId");
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

    value = cJSON_GetObjectItemCaseSensitive(jsonActiveDataset, "MeshLocalPrefix");
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

    value = cJSON_GetObjectItemCaseSensitive(jsonActiveDataset, "PanId");
    if (cJSON_IsNumber(value))
    {
        aDataset.mPanId                      = static_cast<otPanId>(value->valueint);
        aDataset.mComponents.mIsPanIdPresent = true;
    }
    else if (cJSON_IsNull(value))
    {
        aDataset.mComponents.mIsPanIdPresent = false;
    }

    value = cJSON_GetObjectItemCaseSensitive(jsonActiveDataset, "Channel");
    if (cJSON_IsNumber(value))
    {
        aDataset.mChannel                      = static_cast<uint16_t>(value->valueint);
        aDataset.mComponents.mIsChannelPresent = true;
    }
    else if (cJSON_IsNull(value))
    {
        aDataset.mComponents.mIsChannelPresent = false;
    }

    value = cJSON_GetObjectItemCaseSensitive(jsonActiveDataset, "PSKc");
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

    value = cJSON_GetObjectItemCaseSensitive(jsonActiveDataset, "SecurityPolicy");
    if (cJSON_IsObject(value))
    {
        VerifyOrExit(Json2SecurityPolicy(value, aDataset.mSecurityPolicy), ret = false);
        aDataset.mComponents.mIsSecurityPolicyPresent = true;
    }
    else if (cJSON_IsNull(value))
    {
        aDataset.mComponents.mIsSecurityPolicyPresent = false;
    }

    value = cJSON_GetObjectItemCaseSensitive(jsonActiveDataset, "ChannelMask");
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

    value = cJSON_GetObjectItemCaseSensitive(jsonDataset, "ActiveDataset");
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

    value = cJSON_GetObjectItemCaseSensitive(jsonDataset, "PendingTimestamp");
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

    value = cJSON_GetObjectItemCaseSensitive(jsonDataset, "Delay");
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

} // namespace Json
} // namespace rest
} // namespace otbr
