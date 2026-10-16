/*
 *    Copyright (c) 2026, The OpenThread Authors.
 *    All rights reserved.
 *
 *    Redistribution and use in source and binary forms, with or without
 *    modification, are permitted provided that the following conditions are met:
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *    3. Neither the name of the copyright holder nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 *    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *    ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *    LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *    CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *    ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *    POSSIBILITY OF SUCH DAMAGE.
 */

#include <cstring>
#include <set>
#include <string>

#include <gtest/gtest.h>

extern "C" {
#include <cJSON.h>
}

#include "rest/diagnostic_types.hpp"
#include "rest/json.hpp"
#include "rest/names.hpp"
#include "rest/rest_diagnostics_coll.hpp"

extern "C" otError otDatasetParseTlvs(const otOperationalDatasetTlvs *aDatasetTlvs, otOperationalDataset *aDataset)
{
    OT_UNUSED_VARIABLE(aDatasetTlvs);
    OT_UNUSED_VARIABLE(aDataset);
    return OT_ERROR_NONE;
}

namespace otbr {
namespace rest {

static std::string ToCompactJson(const std::string &aJsonStr)
{
    cJSON      *json   = cJSON_Parse(aJsonStr.c_str());
    char       *rawStr = nullptr;
    std::string result;

    if (json != nullptr)
    {
        rawStr = cJSON_PrintUnformatted(json);
        if (rawStr != nullptr)
        {
            result = rawStr;
            cJSON_free(rawStr);
        }
        cJSON_Delete(json);
    }

    return result;
}

TEST(RestDiagnosticTypes, EnhancedRouteTlv)
{
    uint8_t id = 0;

    EXPECT_EQ(DiagnosticTypes::FindId(KEY_ENHANCEDROUTE, id), OT_ERROR_NONE);
    EXPECT_EQ(id, static_cast<uint8_t>(OT_NETWORK_DIAGNOSTIC_TLV_ENHANCED_ROUTE));
    EXPECT_STREQ(DiagnosticTypes::GetJsonKey(OT_NETWORK_DIAGNOSTIC_TLV_ENHANCED_ROUTE), KEY_ENHANCEDROUTE);
    EXPECT_TRUE(DiagnosticTypes::Omittable(OT_NETWORK_DIAGNOSTIC_TLV_ENHANCED_ROUTE));
    EXPECT_FALSE(DiagnosticTypes::RequiresQuery(OT_NETWORK_DIAGNOSTIC_TLV_ENHANCED_ROUTE));
    EXPECT_FALSE(DiagnosticTypes::CanReset(OT_NETWORK_DIAGNOSTIC_TLV_ENHANCED_ROUTE));
}

TEST(RestDiagnosticTypes, AllSupportedTlvs)
{
    static const char *const kAllKeys[] = {
        "extAddress",
        "rloc16",
        "mode",
        "timeout",
        "connectivity",
        "route",
        "leaderData",
        "networkData",
        "ipv6Addresses",
        "macCounters",
        "batteryLevel",
        "supplyVoltage",
        "childTable",
        "channelPages",
        "maxChildTimeout",
        "lDevIdSubject",
        "iDevIdCert",
        "eui",
        "threadVersion",
        "vendorName",
        "vendorModel",
        "vendorSwVersion",
        "threadStackVersion",
        "children",
        "childIpv6Addresses",
        "routerNeighbors",
        "mleCounters",
        "enhancedRoute",
    };

    std::set<uint8_t> uniqueIds;

    for (const char *key : kAllKeys)
    {
        uint8_t id = 0;
        EXPECT_EQ(DiagnosticTypes::FindId(key, id), OT_ERROR_NONE) << "key: " << key;
        EXPECT_STREQ(DiagnosticTypes::GetJsonKey(id), key);
        uniqueIds.insert(id);
    }

    EXPECT_EQ(uniqueIds.size(), static_cast<size_t>(DiagnosticTypes::kMaxTotalCount));
}

TEST(RestDiagnosticJson, EnhRouteData2JsonString)
{
    otNetworkDiagEnhRouteData routeData;

    // 1. Self router entry (mIsSelf == true): only routeId and isSelf are emitted.
    memset(&routeData, 0, sizeof(routeData));
    routeData.mRouterId       = 10;
    routeData.mIsSelf         = true;
    routeData.mHasLink        = true;
    routeData.mLinkQualityOut = 3;
    routeData.mLinkQualityIn  = 2;
    routeData.mNextHop        = 5;
    routeData.mNextHopCost    = 1;
    EXPECT_EQ(ToCompactJson(Json::EnhRouteData2JsonString(routeData)), R"({"routeId":10,"isSelf":true})");

    // 2. Neighbor router without next hop (mIsSelf == false, mHasLink == true, mNextHop > OT_NETWORK_MAX_ROUTER_ID).
    memset(&routeData, 0, sizeof(routeData));
    routeData.mRouterId       = 15;
    routeData.mIsSelf         = false;
    routeData.mHasLink        = true;
    routeData.mLinkQualityOut = 3;
    routeData.mLinkQualityIn  = 2;
    routeData.mNextHop        = 63;
    routeData.mNextHopCost    = 0;
    EXPECT_EQ(ToCompactJson(Json::EnhRouteData2JsonString(routeData)),
              R"({"routeId":15,"isSelf":false,"hasLink":true,"linkQualityOut":3,"linkQualityIn":2})");

    // 3. Multi-hop router without direct link (mIsSelf == false, mHasLink == false, mNextHop <=
    // OT_NETWORK_MAX_ROUTER_ID).
    memset(&routeData, 0, sizeof(routeData));
    routeData.mRouterId       = 20;
    routeData.mIsSelf         = false;
    routeData.mHasLink        = false;
    routeData.mLinkQualityOut = 0;
    routeData.mLinkQualityIn  = 0;
    routeData.mNextHop        = 15;
    routeData.mNextHopCost    = 4;
    EXPECT_EQ(ToCompactJson(Json::EnhRouteData2JsonString(routeData)),
              R"({"routeId":20,"isSelf":false,"hasLink":false,"nextHop":15,"nextHopCost":4})");

    // 4. Neighbor router with both direct link and valid next hop (mNextHop == OT_NETWORK_MAX_ROUTER_ID).
    memset(&routeData, 0, sizeof(routeData));
    routeData.mRouterId       = 25;
    routeData.mIsSelf         = false;
    routeData.mHasLink        = true;
    routeData.mLinkQualityOut = 1;
    routeData.mLinkQualityIn  = 3;
    routeData.mNextHop        = OT_NETWORK_MAX_ROUTER_ID;
    routeData.mNextHopCost    = 2;
    EXPECT_EQ(
        ToCompactJson(Json::EnhRouteData2JsonString(routeData)),
        R"({"routeId":25,"isSelf":false,"hasLink":true,"linkQualityOut":1,"linkQualityIn":3,"nextHop":62,"nextHopCost":2})");

    // 5. Unreachable router without direct link or next hop.
    memset(&routeData, 0, sizeof(routeData));
    routeData.mRouterId = 30;
    routeData.mIsSelf   = false;
    routeData.mHasLink  = false;
    routeData.mNextHop  = 63;
    EXPECT_EQ(ToCompactJson(Json::EnhRouteData2JsonString(routeData)),
              R"({"routeId":30,"isSelf":false,"hasLink":false})");
}

TEST(RestDiagnosticJson, EnhRouteTlvInDiagSet)
{
    NetworkDiagnostics diag;
    otNetworkDiagTlv   tlv;

    memset(&tlv, 0, sizeof(tlv));
    tlv.mType                       = OT_NETWORK_DIAGNOSTIC_TLV_ENHANCED_ROUTE;
    tlv.mData.mEnhRoute.mRouteCount = 3;

    // Entry 0: Self
    tlv.mData.mEnhRoute.mRouteData[0].mRouterId = 1;
    tlv.mData.mEnhRoute.mRouteData[0].mIsSelf   = true;

    // Entry 1: Neighbor with direct link and next hop
    tlv.mData.mEnhRoute.mRouteData[1].mRouterId       = 5;
    tlv.mData.mEnhRoute.mRouteData[1].mIsSelf         = false;
    tlv.mData.mEnhRoute.mRouteData[1].mHasLink        = true;
    tlv.mData.mEnhRoute.mRouteData[1].mLinkQualityOut = 3;
    tlv.mData.mEnhRoute.mRouteData[1].mLinkQualityIn  = 3;
    tlv.mData.mEnhRoute.mRouteData[1].mNextHop        = 5;
    tlv.mData.mEnhRoute.mRouteData[1].mNextHopCost    = 1;

    // Entry 2: Multi-hop router via router 5
    tlv.mData.mEnhRoute.mRouteData[2].mRouterId    = 12;
    tlv.mData.mEnhRoute.mRouteData[2].mIsSelf      = false;
    tlv.mData.mEnhRoute.mRouteData[2].mHasLink     = false;
    tlv.mData.mEnhRoute.mRouteData[2].mNextHop     = 5;
    tlv.mData.mEnhRoute.mRouteData[2].mNextHopCost = 2;

    const std::string expectedArray =
        R"([{"routeId":1,"isSelf":true},)"
        R"({"routeId":5,"isSelf":false,"hasLink":true,"linkQualityOut":3,"linkQualityIn":3,"nextHop":5,"nextHopCost":1},)"
        R"({"routeId":12,"isSelf":false,"hasLink":false,"nextHop":5,"nextHopCost":2}])";

    EXPECT_EQ(ToCompactJson(Json::EnhRoute2JsonString(tlv.mData.mEnhRoute)), expectedArray);

    diag.mDeviceTlvSet.push_back(tlv);

    std::string jsonStr = diag.ToJsonString({KEY_ENHANCEDROUTE});
    cJSON      *root    = cJSON_Parse(jsonStr.c_str());
    ASSERT_NE(root, nullptr);

    cJSON *enhRoute = cJSON_GetObjectItemCaseSensitive(root, KEY_ENHANCEDROUTE);
    ASSERT_NE(enhRoute, nullptr);
    EXPECT_TRUE(cJSON_IsArray(enhRoute));
    EXPECT_EQ(cJSON_GetArraySize(enhRoute), 3);

    cJSON_Delete(root);
}

} // namespace rest
} // namespace otbr
