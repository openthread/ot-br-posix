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

#include "cJSON.h"

namespace otbr {
namespace rest {
namespace JSON {

std::string String2JsonString(std::string aString)
{
    cJSON *     json = cJSON_CreateString(aString.c_str());
    char *      out  = cJSON_Print(json);
    std::string ret  = out;
    cJSON_Delete(json);
    return ret;
}

std::string Json2String(cJSON *aJson)
{
    char *      out = cJSON_Print(aJson);
    std::string ret;
    if (out != nullptr)
    {
        ret = out;
        cJSON_Delete(aJson);
        delete (out);
    }
    return ret;
}

bool is_big_endian(void)
{
    union
    {
        uint32_t i;
        char     c[4];
    } bint = {0x01020304};

    return bint.c[0] == 1;
}

std::string IpAddr2JsonString(const otIp6Address &aAddress)
{
    std::string str;
    uint16_t    value;
    char        p[5];

    for (int i = 0; i < 8; ++i)
    {
        if (!is_big_endian())
            value = (((aAddress.mFields.m16[i] & 0x00ffU) << 8) & 0xff00) |
                    (((aAddress.mFields.m16[i] & 0xff00U) >> 8) & 0x00ff);
        else
            value = aAddress.mFields.m16[i];
        sprintf(p, "%x", value);
        str += ":" + std::string(p);
    }
    return String2JsonString(str);
}

std::string Bytes2HexString(const uint8_t *aBytes, uint8_t aLength)
{
    std::string str;
    char        p[3];

    for (int i = 0; i < aLength; ++i)
    {
        snprintf(p, 3, "%02x", aBytes[i]);
        str += std::string(p, 2);
    }

    return str;
}

std::string TwoVector2JsonString(const std::vector<std::string> &aKey, const std::vector<std::string> &aValue)
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
    return Json2String(json);
}

std::string Vector2JsonString(const std::vector<std::string> &aVector)
{
    cJSON *json = cJSON_CreateArray();
    cJSON *item;

    for (auto str : aVector)
    {
        item = cJSON_Parse(str.c_str());
        cJSON_AddItemToArray(json, item);
    }

    return Json2String(json);
}

std::string Mode2JsonString(const otLinkModeConfig &aMode)
{
    cJSON *json = cJSON_CreateObject();

    char value[2];

    sprintf(value, "%d", aMode.mRxOnWhenIdle);
    cJSON_AddItemToObject(json, "RxOnWhenIdle", cJSON_CreateString(value));

    sprintf(value, "%d", aMode.mSecureDataRequests);
    cJSON_AddItemToObject(json, "SecureDataRequests", cJSON_CreateString(value));

    sprintf(value, "%d", aMode.mDeviceType);
    cJSON_AddItemToObject(json, "DeviceType", cJSON_CreateString(value));

    sprintf(value, "%d", aMode.mNetworkData);
    cJSON_AddItemToObject(json, "NetworkData", cJSON_CreateString(value));

    return Json2String(json);
}

std::string Connectivity2JsonString(const otNetworkDiagConnectivity &aConnectivity)
{
    cJSON *json = cJSON_CreateObject();

    char value[11];

    sprintf(value, "%d", aConnectivity.mParentPriority);
    cJSON_AddItemToObject(json, "ParentPriority", cJSON_CreateString(value));

    sprintf(value, "%u", aConnectivity.mLinkQuality3);
    cJSON_AddItemToObject(json, "LinkQuality3", cJSON_CreateString(value));

    sprintf(value, "%u", aConnectivity.mLinkQuality2);
    cJSON_AddItemToObject(json, "LinkQuality2", cJSON_CreateString(value));

    sprintf(value, "%u", aConnectivity.mLinkQuality1);
    cJSON_AddItemToObject(json, "LinkQuality1", cJSON_CreateString(value));

    sprintf(value, "%u", aConnectivity.mLeaderCost);
    cJSON_AddItemToObject(json, "LeaderCost", cJSON_CreateString(value));

    sprintf(value, "%u", aConnectivity.mIdSequence);
    cJSON_AddItemToObject(json, "IdSequence", cJSON_CreateString(value));

    sprintf(value, "%u", aConnectivity.mActiveRouters);
    cJSON_AddItemToObject(json, "ActiveRouters", cJSON_CreateString(value));

    sprintf(value, "%u", aConnectivity.mSedBufferSize);
    cJSON_AddItemToObject(json, "SedBufferSize", cJSON_CreateString(value));

    sprintf(value, "%u", aConnectivity.mSedDatagramCount);
    cJSON_AddItemToObject(json, "SedDatagramCount", cJSON_CreateString(value));

    return Json2String(json);
}

std::string RouteData2JsonString(const otNetworkDiagRouteData &aRouteData)
{
    char   value[5];
    cJSON *json = cJSON_CreateObject();

    sprintf(value, "0x%02x", aRouteData.mRouterId);
    cJSON_AddItemToObject(json, "RouteId", cJSON_CreateString(value));

    sprintf(value, "%d", aRouteData.mLinkQualityOut);
    cJSON_AddItemToObject(json, "LinkQualityOut", cJSON_CreateString(value));

    sprintf(value, "%d", aRouteData.mLinkQualityIn);
    cJSON_AddItemToObject(json, "LinkQualityIn", cJSON_CreateString(value));

    sprintf(value, "%d", aRouteData.mRouteCost);
    cJSON_AddItemToObject(json, "RouteCost", cJSON_CreateString(value));

    return Json2String(json);
}

std::string Route2JsonString(const otNetworkDiagRoute &aRoute)
{
    cJSON *json = cJSON_CreateObject();

    char value[11];

    sprintf(value, "%d", aRoute.mIdSequence);

    cJSON_AddItemToObject(json, "IdSequence", cJSON_CreateString(value));

    cJSON *RouteData = cJSON_CreateArray();
    for (uint16_t i = 0; i < aRoute.mRouteCount; ++i)
    {
        cJSON *RouteDatavalue = cJSON_Parse(RouteData2JsonString(aRoute.mRouteData[i]).c_str());

        cJSON_AddItemToArray(RouteData, RouteDatavalue);
    }

    cJSON_AddItemToObject(json, "RouteData", RouteData);

    return Json2String(json);
}

std::string LeaderData2JsonString(const otLeaderData &aLeaderData)
{
    char   value[11];
    cJSON *json = cJSON_CreateObject();

    sprintf(value, "0x%08x", aLeaderData.mPartitionId);
    cJSON_AddItemToObject(json, "PartitionId", cJSON_CreateString(value));

    sprintf(value, "%d", aLeaderData.mWeighting);
    cJSON_AddItemToObject(json, "Weighting", cJSON_CreateString(value));

    sprintf(value, "%d", aLeaderData.mDataVersion);
    cJSON_AddItemToObject(json, "DataVersion", cJSON_CreateString(value));

    sprintf(value, "%d", aLeaderData.mStableDataVersion);
    cJSON_AddItemToObject(json, "StableDataVersion", cJSON_CreateString(value));

    sprintf(value, "0x%02x", aLeaderData.mLeaderRouterId);
    cJSON_AddItemToObject(json, "LeaderRouterId", cJSON_CreateString(value));

    return Json2String(json);
}

std::string MacCounters2JsonString(const otNetworkDiagMacCounters &aMacCounters)
{
    char   value[9];
    cJSON *json = cJSON_CreateObject();

    sprintf(value, "%d", aMacCounters.mIfInUnknownProtos);
    cJSON_AddItemToObject(json, "IfInUnknownProtos", cJSON_CreateString(value));

    sprintf(value, "%d", aMacCounters.mIfInErrors);
    cJSON_AddItemToObject(json, "IfInErrors", cJSON_CreateString(value));

    sprintf(value, "%d", aMacCounters.mIfOutErrors);
    cJSON_AddItemToObject(json, "IfOutErrors", cJSON_CreateString(value));

    sprintf(value, "%d", aMacCounters.mIfInUcastPkts);
    cJSON_AddItemToObject(json, "IfInUcastPkts", cJSON_CreateString(value));

    sprintf(value, "%d", aMacCounters.mIfInBroadcastPkts);
    cJSON_AddItemToObject(json, "IfInBroadcastPkts", cJSON_CreateString(value));

    sprintf(value, "%d", aMacCounters.mIfInDiscards);
    cJSON_AddItemToObject(json, "IfInDiscards", cJSON_CreateString(value));

    sprintf(value, "%d", aMacCounters.mIfOutUcastPkts);
    cJSON_AddItemToObject(json, "IfOutUcastPkts", cJSON_CreateString(value));

    sprintf(value, "%d", aMacCounters.mIfOutBroadcastPkts);
    cJSON_AddItemToObject(json, "IfOutBroadcastPkts", cJSON_CreateString(value));

    sprintf(value, "%d", aMacCounters.mIfOutDiscards);
    cJSON_AddItemToObject(json, "IfOutDiscards", cJSON_CreateString(value));

    return Json2String(json);
}

std::string ChildTableEntry2JsonString(const otNetworkDiagChildEntry &aChildEntry)
{
    char   value[7];
    cJSON *json = cJSON_CreateObject();

    sprintf(value, "0x%04x", aChildEntry.mChildId);
    cJSON_AddItemToObject(json, "ChildId", cJSON_CreateString(value));

    sprintf(value, "%d", aChildEntry.mTimeout);
    cJSON_AddItemToObject(json, "Timeout", cJSON_CreateString(value));

    cJSON *cMode = cJSON_Parse(Mode2JsonString(aChildEntry.mMode).c_str());
    cJSON_AddItemToObject(json, "Mode", cMode);

    return Json2String(json);
}
} // namespace JSON
} // namespace rest
} // namespace otbr
