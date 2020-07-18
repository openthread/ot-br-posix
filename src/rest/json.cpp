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

namespace otbr {
namespace rest {

static uint16_t HostSwap16(uint16_t v)

{
    return (((v & 0x00ffU) << 8) & 0xff00) | (((v & 0xff00U) >> 8) & 0x00ff);
}

std::string JSON::TwoVectorToJson(const std::vector<std::string> &aKey, const std::vector<std::string> &aValue)
{
    cJSON *     json = cJSON_CreateObject();
    std::string key;
    cJSON *     value;

    for (size_t index = 0; index < aKey.size(); index++)
    {
        key   = aKey[index];
        value = cJSON_Parse(aValue[index].c_str());
        cJSON_AddItemToObject(json, key.c_str(), value);
    }
    return JsonToStringDeleteJson(json);
}

std::string JSON::VectorToJson(const std::vector<std::string> &aVector)
{
    cJSON *json = cJSON_CreateArray();
    cJSON *item;

    for (auto str : aVector)
    {
        item = cJSON_Parse(str.c_str());
        cJSON_AddItemToArray(item, json);
    }

    return JsonToStringDeleteJson(json);
}

std::string JSON::JsonToString(cJSON *aJson)
{
    char *      p   = cJSON_Print(aJson);
    std::string ret = p;
    cJSON_Delete(aJson);
    delete (p);
    return ret;
}

std::string JSON::CreateMode(const otLinkModeConfig &aMode)
{
    return JsonToStringDeleteJson(CreateJsonMode(aMode));
}
std::string JSON::CreateConnectivity(const otNetworkDiagConnectivity &aConnectivity)
{
    return JsonToStringDeleteJson(CreateJsonConnectivity(aConnectivity));
}
std::string JSON::CreateRoute(const otNetworkDiagRoute &aRoute)
{
    return JsonToStringDeleteJson(CreateJsonRoute(aRoute));
}
std::string JSON::CreateRouteData(const otNetworkDiagRouteData &aRouteData)
{
    return JsonToStringDeleteJson(CreateJsonRouteData(aRouteData));
}
std::string JSON::CreateLeaderData(const otLeaderData &aLeaderData)
{
    return JsonToStringDeleteJson(CreateJsonLeaderData(aLeaderData));
}
std::string JSON::CreateIp6Address(const otIp6Address &aAddress)
{
    return JsonToStringDeleteJson(CreateJsonIp6Address(aAddress));
}
std::string JSON::CreateMacCounters(const otNetworkDiagMacCounters &aMacCounters)
{
    return JsonToStringDeleteJson(CreateJsonMacCounters(aMacCounters));
}
std::string JSON::CreateChildTableEntry(const otNetworkDiagChildEntry &aChildEntry)
{
    return JsonToStringDeleteJson(CreateJsonChildTableEntry(aChildEntry));
}

cJSON *JSON::CreateJsonConnectivity(const otNetworkDiagConnectivity &aConnectivity)
{
    cJSON *json = cJSON_CreateObject();

    char tmp[11];

    sprintf(tmp, "%d", aConnectivity.mParentPriority);
    cJSON_AddItemToObject(json, "ParentPriority", cJSON_CreateString(tmp));

    sprintf(tmp, "%u", aConnectivity.mLinkQuality3);
    cJSON_AddItemToObject(json, "LinkQuality3", cJSON_CreateString(tmp));

    sprintf(tmp, "%u", aConnectivity.mLinkQuality2);
    cJSON_AddItemToObject(json, "LinkQuality2", cJSON_CreateString(tmp));

    sprintf(tmp, "%u", aConnectivity.mLinkQuality1);
    cJSON_AddItemToObject(json, "LinkQuality1", cJSON_CreateString(tmp));

    sprintf(tmp, "%u", aConnectivity.mLeaderCost);
    cJSON_AddItemToObject(json, "LeaderCost", cJSON_CreateString(tmp));

    sprintf(tmp, "%u", aConnectivity.mIdSequence);
    cJSON_AddItemToObject(json, "IdSequence", cJSON_CreateString(tmp));

    sprintf(tmp, "%u", aConnectivity.mActiveRouters);
    cJSON_AddItemToObject(json, "ActiveRouters", cJSON_CreateString(tmp));

    sprintf(tmp, "%u", aConnectivity.mSedBufferSize);
    cJSON_AddItemToObject(json, "SedBufferSize", cJSON_CreateString(tmp));

    sprintf(tmp, "%u", aConnectivity.mSedDatagramCount);
    cJSON_AddItemToObject(json, "SedDatagramCount", cJSON_CreateString(tmp));

    return json;
}
cJSON *JSON::CreateJsonMode(const otLinkModeConfig &aMode)
{
    cJSON *json = cJSON_CreateObject();

    char tmp[2];

    sprintf(tmp, "%d", aMode.mRxOnWhenIdle);
    cJSON_AddItemToObject(json, "RxOnWhenIdle", cJSON_CreateString(tmp));

    sprintf(tmp, "%d", aMode.mSecureDataRequests);
    cJSON_AddItemToObject(json, "SecureDataRequests", cJSON_CreateString(tmp));

    sprintf(tmp, "%d", aMode.mDeviceType);
    cJSON_AddItemToObject(json, "DeviceType", cJSON_CreateString(tmp));

    sprintf(tmp, "%d", aMode.mNetworkData);
    cJSON_AddItemToObject(json, "NetworkData", cJSON_CreateString(tmp));

    return json;
}

cJSON *JSON::CreateJsonRoute(const otNetworkDiagRoute &aRoute)
{
    cJSON *json = cJSON_CreateObject();

    char tmp[11];

    sprintf(tmp, "%d", aRoute.mIdSequence);

    cJSON_AddItemToObject(json, "IdSequence", cJSON_CreateString(tmp));

    cJSON *RouteData = cJSON_CreateArray();
    for (uint16_t i = 0; i < aRoute.mRouteCount; ++i)
    {
        cJSON *RouteDatatmp = CreateJsonRouteData(aRoute.mRouteData[i]);

        cJSON_AddItemToArray(RouteData, RouteDatatmp);
    }

    cJSON_AddItemToObject(json, "RouteData", RouteData);

    return json;
}
cJSON *JSON::CreateJsonRouteData(const otNetworkDiagRouteData &aRouteData)
{
    char   tmp[5];
    cJSON *json = cJSON_CreateObject();

    sprintf(tmp, "0x%02x", aRouteData.mRouterId);
    cJSON_AddItemToObject(json, "RouteId", cJSON_CreateString(tmp));

    sprintf(tmp, "%d", aRouteData.mLinkQualityOut);
    cJSON_AddItemToObject(json, "LinkQualityOut", cJSON_CreateString(tmp));

    sprintf(tmp, "%d", aRouteData.mLinkQualityIn);
    cJSON_AddItemToObject(json, "LinkQualityIn", cJSON_CreateString(tmp));

    sprintf(tmp, "%d", aRouteData.mRouteCost);
    cJSON_AddItemToObject(json, "RouteCost", cJSON_CreateString(tmp));

    return json;
}

cJSON *JSON::CreateJsonLeaderData(const otLeaderData &aLeaderData)
{
    char   tmp[11];
    cJSON *json = cJSON_CreateObject();

    sprintf(tmp, "0x%08x", aLeaderData.mPartitionId);
    cJSON_AddItemToObject(json, "PartitionId", cJSON_CreateString(tmp));

    sprintf(tmp, "%d", aLeaderData.mWeighting);
    cJSON_AddItemToObject(json, "Weighting", cJSON_CreateString(tmp));

    sprintf(tmp, "%d", aLeaderData.mDataVersion);
    cJSON_AddItemToObject(json, "DataVersion", cJSON_CreateString(tmp));

    sprintf(tmp, "%d", aLeaderData.mStableDataVersion);
    cJSON_AddItemToObject(json, "StableDataVersion", cJSON_CreateString(tmp));

    sprintf(tmp, "0x%02x", aLeaderData.mLeaderRouterId);
    cJSON_AddItemToObject(json, "LeaderRouterId", cJSON_CreateString(tmp));

    return json;
}
cJSON *JSON::CreateJsonIp6Address(const otIp6Address &aAddress)
{
    char tmp[65];

    sprintf(tmp, "%x:%x:%x:%x:%x:%x:%x:%x", HostSwap16(aAddress.mFields.m16[0]), HostSwap16(aAddress.mFields.m16[1]),
            HostSwap16(aAddress.mFields.m16[2]), HostSwap16(aAddress.mFields.m16[3]),
            HostSwap16(aAddress.mFields.m16[4]), HostSwap16(aAddress.mFields.m16[5]),
            HostSwap16(aAddress.mFields.m16[6]), HostSwap16(aAddress.mFields.m16[7]));

    cJSON *json = cJSON_CreateString(tmp);
    return json;
}

cJSON *JSON::CreateJsonMacCounters(const otNetworkDiagMacCounters &aMacCounters)
{
    char   tmp[9];
    cJSON *json = cJSON_CreateObject();

    sprintf(tmp, "%d", aMacCounters.mIfInUnknownProtos);
    cJSON_AddItemToObject(json, "IfInUnknownProtos", cJSON_CreateString(tmp));

    sprintf(tmp, "%d", aMacCounters.mIfInErrors);
    cJSON_AddItemToObject(json, "IfInErrors", cJSON_CreateString(tmp));

    sprintf(tmp, "%d", aMacCounters.mIfOutErrors);
    cJSON_AddItemToObject(json, "IfOutErrors", cJSON_CreateString(tmp));

    sprintf(tmp, "%d", aMacCounters.mIfInUcastPkts);
    cJSON_AddItemToObject(json, "IfInUcastPkts", cJSON_CreateString(tmp));

    sprintf(tmp, "%d", aMacCounters.mIfInBroadcastPkts);
    cJSON_AddItemToObject(json, "IfInBroadcastPkts", cJSON_CreateString(tmp));

    sprintf(tmp, "%d", aMacCounters.mIfInDiscards);
    cJSON_AddItemToObject(json, "IfInDiscards", cJSON_CreateString(tmp));

    sprintf(tmp, "%d", aMacCounters.mIfOutUcastPkts);
    cJSON_AddItemToObject(json, "IfOutUcastPkts", cJSON_CreateString(tmp));

    sprintf(tmp, "%d", aMacCounters.mIfOutBroadcastPkts);
    cJSON_AddItemToObject(json, "IfOutBroadcastPkts", cJSON_CreateString(tmp));

    sprintf(tmp, "%d", aMacCounters.mIfOutDiscards);
    cJSON_AddItemToObject(json, "IfOutDiscards", cJSON_CreateString(tmp));

    return json;
}

cJSON *JSON::CreateJsonChildTableEntry(const otNetworkDiagChildEntry &aChildEntry)
{
    char   tmp[7];
    cJSON *json = cJSON_CreateObject();

    sprintf(tmp, "0x%04x", aChildEntry.mChildId);
    cJSON_AddItemToObject(json, "ChildId", cJSON_CreateString(tmp));

    sprintf(tmp, "%d", aChildEntry.mTimeout);
    cJSON_AddItemToObject(json, "Timeout", cJSON_CreateString(tmp));

    cJSON *cMode = CreateJsonMode(aChildEntry.mMode);
    cJSON_AddItemToObject(json, "Mode", cMode);

    return json;
}

} // namespace rest
} // namespace otbr
