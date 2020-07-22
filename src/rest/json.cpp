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
cJSON * CString2Json(const char *aString){
    cJSON * json = cJSON_CreateString(aString);
    return json;
}

cJSON *Mode2Json(const otLinkModeConfig &aMode){
    cJSON *json = cJSON_CreateObject();

    cJSON_AddItemToObject(json, "RxOnWhenIdle", cJSON_CreateNumber(aMode.mRxOnWhenIdle));

    cJSON_AddItemToObject(json, "SecureDataRequests", cJSON_CreateNumber(aMode.mSecureDataRequests));

    cJSON_AddItemToObject(json, "DeviceType", cJSON_CreateNumber(aMode.mDeviceType));

    cJSON_AddItemToObject(json, "NetworkData", cJSON_CreateNumber(aMode.mNetworkData));

    return json;
}

cJSON * IpAddr2Json(const otIp6Address &aAddress)
{   cJSON * json;
    std::string str;
    uint16_t    value;
    char        p[5];
    
    for (int i = 0; i < 16; ++i)
    {
        value = aAddress.mFields.m8[i];
        if(i % 2 ==1){
            value = (aAddress.mFields.m8[i-1] << 8) + aAddress.mFields.m8[i]; 
            sprintf(p, "%x", value);
            if( i > 1){
                str += ":";
            }
            str += std::string(p);
        }
    }

    json = cJSON_CreateString(str.c_str());

    return json;
}

cJSON* ChildTableEntry2Json(const otNetworkDiagChildEntry &aChildEntry)
{
    char   value[7];
    cJSON *json = cJSON_CreateObject();

    sprintf(value, "0x%04x", aChildEntry.mChildId);
    cJSON_AddItemToObject(json, "ChildId", cJSON_CreateString(value));

    
    cJSON_AddItemToObject(json, "Timeout", cJSON_CreateNumber(aChildEntry.mTimeout));

    cJSON *mode = Mode2Json(aChildEntry.mMode);
    cJSON_AddItemToObject(json, "Mode", mode);

    return json;
}

cJSON* MacCounters2Json(const otNetworkDiagMacCounters &aMacCounters)
{
    cJSON *json = cJSON_CreateObject();

   
    cJSON_AddItemToObject(json, "IfInUnknownProtos", cJSON_CreateNumber(aMacCounters.mIfInUnknownProtos));

    
    cJSON_AddItemToObject(json, "IfInErrors", cJSON_CreateNumber(aMacCounters.mIfInErrors));

    
    cJSON_AddItemToObject(json, "IfOutErrors", cJSON_CreateNumber(aMacCounters.mIfOutErrors));

    
    cJSON_AddItemToObject(json, "IfInUcastPkts", cJSON_CreateNumber(aMacCounters.mIfInUcastPkts));

    
    cJSON_AddItemToObject(json, "IfInBroadcastPkts", cJSON_CreateNumber(aMacCounters.mIfInBroadcastPkts));

    
    cJSON_AddItemToObject(json, "IfInDiscards", cJSON_CreateNumber(aMacCounters.mIfInDiscards));

    
    cJSON_AddItemToObject(json, "IfOutUcastPkts", cJSON_CreateNumber(aMacCounters.mIfOutUcastPkts));

   
    cJSON_AddItemToObject(json, "IfOutBroadcastPkts", cJSON_CreateNumber(aMacCounters.mIfOutBroadcastPkts));

    
    cJSON_AddItemToObject(json, "IfOutDiscards", cJSON_CreateNumber(aMacCounters.mIfOutDiscards));

    return json;
}

cJSON * Connectivity2Json(const otNetworkDiagConnectivity &aConnectivity){
    
    cJSON *json = cJSON_CreateObject();

    cJSON_AddItemToObject(json, "ParentPriority", cJSON_CreateNumber(aConnectivity.mParentPriority));

    cJSON_AddItemToObject(json, "LinkQuality3", cJSON_CreateNumber(aConnectivity.mLinkQuality3));

    cJSON_AddItemToObject(json, "LinkQuality2", cJSON_CreateNumber(aConnectivity.mLinkQuality2));

    cJSON_AddItemToObject(json, "LinkQuality1", cJSON_CreateNumber(aConnectivity.mLinkQuality1));

    cJSON_AddItemToObject(json, "LeaderCost", cJSON_CreateNumber(aConnectivity.mLeaderCost));

    cJSON_AddItemToObject(json, "IdSequence", cJSON_CreateNumber(aConnectivity.mIdSequence));

    cJSON_AddItemToObject(json, "ActiveRouters", cJSON_CreateNumber(aConnectivity.mActiveRouters));

    cJSON_AddItemToObject(json, "SedBufferSize", cJSON_CreateNumber(aConnectivity.mSedBufferSize));

    cJSON_AddItemToObject(json, "SedDatagramCount", cJSON_CreateNumber(aConnectivity.mSedDatagramCount));

    return json;
}

cJSON *RouteData2Json(const otNetworkDiagRouteData &aRouteData)
{
    char   value[5];
    cJSON *json = cJSON_CreateObject();

    sprintf(value, "0x%02x", aRouteData.mRouterId);
    cJSON_AddItemToObject(json, "RouteId", cJSON_CreateString(value));

    
    cJSON_AddItemToObject(json, "LinkQualityOut", cJSON_CreateNumber(aRouteData.mLinkQualityOut));

   
    cJSON_AddItemToObject(json, "LinkQualityIn", cJSON_CreateNumber(aRouteData.mLinkQualityIn));

    
    cJSON_AddItemToObject(json, "RouteCost", cJSON_CreateNumber(aRouteData.mRouteCost));

    return json;
}

cJSON * Route2Json(const otNetworkDiagRoute &aRoute){

    cJSON *json = cJSON_CreateObject();

    cJSON_AddItemToObject(json, "IdSequence", cJSON_CreateNumber(aRoute.mIdSequence));

    cJSON *RouteData = cJSON_CreateArray();
    for (uint16_t i = 0; i < aRoute.mRouteCount; ++i)
    {
        cJSON *RouteDatavalue = RouteData2Json(aRoute.mRouteData[i]);

        cJSON_AddItemToArray(RouteData, RouteDatavalue);
    }

    cJSON_AddItemToObject(json, "RouteData", RouteData);

    return json;

}

cJSON * LeaderData2Json(const otLeaderData &aLeaderData)
{
    char   value[11];
    cJSON *json = cJSON_CreateObject();

    sprintf(value, "0x%08x", aLeaderData.mPartitionId);
    cJSON_AddItemToObject(json, "PartitionId", cJSON_CreateString(value));

    
    cJSON_AddItemToObject(json, "Weighting", cJSON_CreateNumber(aLeaderData.mWeighting));

    
    cJSON_AddItemToObject(json, "DataVersion", cJSON_CreateNumber(aLeaderData.mDataVersion));

    
    cJSON_AddItemToObject(json, "StableDataVersion", cJSON_CreateNumber(aLeaderData.mStableDataVersion));

    sprintf(value, "0x%02x", aLeaderData.mLeaderRouterId);
    cJSON_AddItemToObject(json, "LeaderRouterId", cJSON_CreateString(value));
    
    return json;
   
}

std::string IpAddr2JsonString(const otIp6Address &aAddress)
{
    std::string str;
    uint16_t    value;
    
    char        p[5];
    
    for (int i = 0; i < 16; ++i)
    {
        value = aAddress.mFields.m8[i];
        if(i % 2 ==1){
            value = (aAddress.mFields.m8[i-1] << 8) + aAddress.mFields.m8[i]; 
            sprintf(p, "%x", value);
            if( i > 1){
                str += ":";
            }
            str += std::string(p);
        }
    }
    return String2JsonString(str);
}

cJSON * Bytes2HexJson(const uint8_t *aBytes, uint8_t aLength){
    
    char hex[2*aLength+1];
    cJSON * json;
    
    auto size = otbr::Utils::Bytes2Hex(aBytes, aLength, hex);
    
    if (static_cast<size_t>(2*aLength+1) >= size){
        json = cJSON_CreateString(hex);
    }
    
    return json;
}

std::string Node2JsonString(Node node){
    cJSON *    json = cJSON_CreateObject();
    cJSON*     value;
    char        p[7];

    value =  cJSON_CreateNumber(node.role);
    cJSON_AddItemToObject(json,"state",value);
    
    value =  cJSON_CreateNumber(node.numOfRouter);
    cJSON_AddItemToObject(json,"numOfRouter",value);
    
    value = IpAddr2Json(node.rlocAddress);
    cJSON_AddItemToObject(json,"rlocAddress",value);
    
    value = Bytes2HexJson(node.extAddress,OT_EXT_ADDRESS_SIZE);
    cJSON_AddItemToObject(json,"extAddress",value);
    
    value = CString2Json(node.networkName);
    cJSON_AddItemToObject(json,"networkName",value);
    
    sprintf(p, "0x%04x", node.rloc16);
    value = CString2Json(p);
    cJSON_AddItemToObject(json,"rloc16",value);
    
    value = LeaderData2Json(node.leaderData);
    cJSON_AddItemToObject(json,"leaderData",value);
    
    value = Bytes2HexJson(node.extPanId,OT_EXT_PAN_ID_SIZE);
    cJSON_AddItemToObject(json,"extPanId",value);

    return Json2String(json);

}

std::string Diag2JsonString(const std::vector<std::vector<otNetworkDiagTlv>> &aDiagSet){
    cJSON *    diagArray = cJSON_CreateArray();
    cJSON *    value;
    cJSON *   diagObject;
    std::string str;
    for( auto diag : aDiagSet  )
    {   diagObject = cJSON_CreateObject();
        for(auto diagTlv : diag)
        {
            switch (diagTlv.mType)
            {
            case OT_NETWORK_DIAGNOSTIC_TLV_EXT_ADDRESS:
            {   
                value = Bytes2HexJson(diagTlv.mData.mExtAddress.m8, OT_EXT_ADDRESS_SIZE);
                cJSON_AddItemToObject(diagObject,"ExtAddress",value);
           
            }
            break;
            case OT_NETWORK_DIAGNOSTIC_TLV_SHORT_ADDRESS:
            {
                char rloc[7];
                sprintf(rloc, "0x%04x", diagTlv.mData.mAddr16);
                value   = CString2Json(rloc);
                cJSON_AddItemToObject(diagObject,"Rloc16",value);
            }
            break;
            case OT_NETWORK_DIAGNOSTIC_TLV_MODE:
            {
                value = Mode2Json(diagTlv.mData.mMode);
                cJSON_AddItemToObject(diagObject,"Mode",value);
            }
            break;
            case OT_NETWORK_DIAGNOSTIC_TLV_TIMEOUT:
            {   
                double timeout = static_cast<double>(diagTlv.mData.mTimeout);
                value = cJSON_CreateNumber(timeout);
                cJSON_AddItemToObject(diagObject,"Timeout",value);
            }
            break;
            case OT_NETWORK_DIAGNOSTIC_TLV_CONNECTIVITY:
            {
                value = Connectivity2Json(diagTlv.mData.mConnectivity);
                cJSON_AddItemToObject(diagObject,"Connectivity",value);
            }
            break;
            case OT_NETWORK_DIAGNOSTIC_TLV_ROUTE:
            {
                value = Route2Json(diagTlv.mData.mRoute);
                cJSON_AddItemToObject(diagObject,"Route",value);
            }
            break;
            case OT_NETWORK_DIAGNOSTIC_TLV_LEADER_DATA:
            {
            
                value = LeaderData2Json(diagTlv.mData.mLeaderData);
                cJSON_AddItemToObject(diagObject,"LeaderData",value);
            
            }
            break;
            case OT_NETWORK_DIAGNOSTIC_TLV_NETWORK_DATA:
            {
                value = Bytes2HexJson(diagTlv.mData.mNetworkData.m8, diagTlv.mData.mNetworkData.mCount);
                cJSON_AddItemToObject(diagObject,"NetworkData",value);
                
            }
            break;
            case OT_NETWORK_DIAGNOSTIC_TLV_IP6_ADDR_LIST:
            {    
                cJSON* addrList = cJSON_CreateArray();
                
                for (uint16_t i = 0; i < diagTlv.mData.mIp6AddrList.mCount; ++i)
                {   
                    cJSON_AddItemToArray(addrList,IpAddr2Json(diagTlv.mData.mIp6AddrList.mList[i]));
                }
                cJSON_AddItemToObject(diagObject,"IP6AddressList",addrList);
                
            }
            break;
            case OT_NETWORK_DIAGNOSTIC_TLV_MAC_COUNTERS:
            {
                value = MacCounters2Json(diagTlv.mData.mMacCounters);
                cJSON_AddItemToObject(diagObject,"MACCounters",value);
            }
            break;
            case OT_NETWORK_DIAGNOSTIC_TLV_BATTERY_LEVEL:
            {
                value = cJSON_CreateNumber(diagTlv.mData.mBatteryLevel);
                cJSON_AddItemToObject(diagObject,"BatteryLevel",value);
            }
            break;
            case OT_NETWORK_DIAGNOSTIC_TLV_SUPPLY_VOLTAGE:
            {
                value = cJSON_CreateNumber(diagTlv.mData.mSupplyVoltage);
                cJSON_AddItemToObject(diagObject,"SupplyVoltage",value);
                
            }

            break;
            case OT_NETWORK_DIAGNOSTIC_TLV_CHILD_TABLE:
            {
                cJSON* tableList = cJSON_CreateArray();

                for (uint16_t i = 0; i < diagTlv.mData.mChildTable.mCount; ++i)
                {
                    cJSON_AddItemToArray(tableList,ChildTableEntry2Json(diagTlv.mData.mChildTable.mTable[i]));
                }
                
                cJSON_AddItemToObject(diagObject,"ChildTable",tableList);
           
            }
            break;
            case OT_NETWORK_DIAGNOSTIC_TLV_CHANNEL_PAGES:
            {
                value = Bytes2HexJson(diagTlv.mData.mChannelPages.m8, diagTlv.mData.mChannelPages.mCount);
            
                cJSON_AddItemToObject(diagObject,"ChannelPages",value);
            
            }
            break;
            case OT_NETWORK_DIAGNOSTIC_TLV_MAX_CHILD_TIMEOUT:
            {   
                value = cJSON_CreateNumber(diagTlv.mData.mMaxChildTimeout);
                cJSON_AddItemToObject(diagObject,"MaxChildTimeout",value);
            }

            break;
            default: 
            break;
            }
        
        }
        cJSON_AddItemToArray(diagArray,diagObject);
    }
    return Json2String(diagArray);

}

std::string Bytes2HexJsonString(const uint8_t *aBytes, uint8_t aLength){
    cJSON *    json = Bytes2HexJson(aBytes,aLength);
    
    return Json2String(json);
}

std::string Number2JsonString(const uint32_t aNumber){
    cJSON *    json = cJSON_CreateNumber(aNumber);

    return Json2String(json);
}

std::string Mode2JsonString(const otLinkModeConfig &aMode)
{
    cJSON *json = Mode2Json(aMode);

    return Json2String(json);
}

std::string Connectivity2JsonString(const otNetworkDiagConnectivity &aConnectivity)
{
    cJSON *json = Connectivity2Json(aConnectivity);
    return Json2String(json);
}

std::string RouteData2JsonString(const otNetworkDiagRouteData &aRouteData)
{
    
    cJSON *json = RouteData2Json(aRouteData);

   return Json2String(json);
}

std::string Route2JsonString(const otNetworkDiagRoute &aRoute)
{
    cJSON *json = Route2Json(aRoute);

    return Json2String(json);
}

std::string LeaderData2JsonString(const otLeaderData &aLeaderData)
{
    
    cJSON *json = LeaderData2Json(aLeaderData);
    return Json2String(json);
}

std::string MacCounters2JsonString(const otNetworkDiagMacCounters &aMacCounters)
{
    cJSON* json = MacCounters2Json(aMacCounters);

    return Json2String(json);
}

std::string ChildTableEntry2JsonString(const otNetworkDiagChildEntry &aChildEntry)
{
    cJSON *json = ChildTableEntry2Json(aChildEntry);

    return Json2String(json);
}

std::string CString2JsonString(const char *aString){
    
    cJSON *json = CString2Json(aString);
    
    return Json2String(json);
}
} // namespace JSON
} // namespace rest
} // namespace otbr
