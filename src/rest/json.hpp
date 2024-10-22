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

/**
 * @file
 *   This file includes JSON formatter definition for RESTful HTTP server.
 */

#ifndef OTBR_REST_JSON_HPP_
#define OTBR_REST_JSON_HPP_

#include "openthread-br/config.h"

#include "openthread/dataset.h"
#include "openthread/link.h"
#include "openthread/thread_ftd.h"

#include "rest/types.hpp"
#include "utils/hex.hpp"

#include <set>
#include <unordered_map>

// #include "extensions/rest_task_network_diagnostic.hpp"

namespace otbr {
namespace rest {

/**
 * The functions within this namespace provides a tranformation from an object/string/number to a serialized Json
 * string.
 */
namespace Json {

// key names used in json objects
#define KEY_ORIGIN "origin"
#define KEY_COUNT "count"
#define KEY_REPORT "report"
#define KEY_CHANNEL "channel"
#define KEY_MAXRSSI "maxRssi"

#define KEY_EXTADDRESS "extAddress" // 64-bit MAC address
#define KEY_MLEIDIID "mlEidIid"
#define KEY_OMRIPV6 "omrIpv6Address"
#define KEY_EUI64 "eui64" // EUI-64 address
#define KEY_HOSTNAME "hostName"

#define KEY_BORDERAGENTID "baId"
#define KEY_BORDERAGENTSTATE "baState"
#define KEY_STATE "state"
#define KEY_ROLE "role"
#define KEY_ROUTERCOUNT "routerCount"
#define KEY_RLOC16_IPV6ADDRESS "rlocAddress"
#define KEY_NETWORKNAME "networkName"
#define KEY_RLOC16 "rloc16" // 16-bit MAC address
#define KEY_ROUTERID "routerId"
#define KEY_LEADERDATA "leaderData" // Leader data
#define KEY_EXTPANID "extPanId"

#define KEY_IP6ADDRESSLIST "ipv6Addresses"          // List of IPv6 addresses
#define KEY_MACCOUNTERS "macCounters"               // MAC packet/event counters
#define KEY_CHANNELPAGES "channelPages"             // Supported frequency bands
#define KEY_VERSION "version"                       // Thread version
#define KEY_VENDORNAME "vendorName"                 // Vendor name
#define KEY_VENDORMODEL "vendorModel"               // Vendor model
#define KEY_VENDORSWVERSION "vendorSwVersion"       // Vendor software version
#define KEY_THREADSTACKVERSION "threadStackVersion" // Thread stack version
#define KEY_MLECOUNTERS "mleCounters"               // MLE counters
#define KEY_CHILDREN "children"                     // Child table
#define KEY_CHILDRENIP6 "childIpv6Addresses"        // IPv6 addresses of child
#define KEY_NEIGHBORS "routerNeighbor"              // Router neighbor info
#define KEY_BRCOUNTERS "brCounters"
#define KEY_LEADER "isLeader"
#define KEY_SERVICE "hostsService"
#define KEY_PBBR "isPrimaryBBR"
#define KEY_BR "isBorderRouter"

#define KEY_MODE "mode"           // Mode
#define KEY_ISFTD "deviceTypeFTD" // is FullThreadDevice
#define KEY_RXONWHENIDLE "rxOnWhenIdle"
#define KEY_FULLNETWORKDATA "fullNetworkData"
#define KEY_TIMEOUT "timeout"                 // Timeout (max polling time period for SEDs)
#define KEY_CONNECTIVITY "connectivity"       // Connectivity information
#define KEY_ROUTE "route"                     // Route64 information
#define KEY_NETWORKDATA "networkData"         // Network data
#define KEY_BATTERYLEVEL "batteryLevel"       // Battery energy level
#define KEY_SUPPLYVOLTAGE "supplyVoltage"     // Current supply voltage
#define KEY_CHILDTABLE "childTable"           // List of children
#define KEY_MAXCHILDTIMEOUT "maxChildTimeout" // Max child timeout

// unused
#define KEY_LDEVID "lDevIdSubject" // LDevID subject public key info
#define KEY_IDEV "iDevIdCert"      // IDevID certificate

/**
 * This method formats an integer to a Json number and serialize it to a string.
 *
 * @param[in] aNumber  An integer need to be format.
 *
 * @returns A string of serialized Json number.
 */
std::string Number2JsonString(const uint32_t &aNumber);

/**
 * This method formats a Bytes array to a Json string and serialize it to a string.
 *
 * @param[in] aBytes  A Bytes array representing a hex number.
 *
 * @returns A string of serialized Json string.
 */
std::string Bytes2HexJsonString(const uint8_t *aBytes, uint8_t aLength);

/**
 * This method parses a hex string as byte array.
 *
 * @param[in] aHexString String of bytes in hex.
 * @param[in] aBytes     Byte array to write to. Must be at least  @p aMaxLength.
 * @param[in] aMaxLength Maximum length to parse (in bytes).
 *
 * @returns Number of bytes effectively parsed.
 */
int Hex2BytesJsonString(const std::string &aHexString, uint8_t *aBytes, uint8_t aMaxLength);

/**
 * This method formats a C string to a Json string and serialize it to a string.
 *
 * @param[in] aCString  A char pointer pointing to a C string.
 *
 * @returns A string of serialized Json string.
 */
std::string CString2JsonString(const char *aCString);

/**
 * This method formats a string to a Json string and serialize it to a string.
 *
 * @param[in] aString  A string.
 *
 * @returns A string of serialized Json string.
 */
std::string String2JsonString(const std::string &aString);

/**
 * This method parses a Json string and checks its datatype and returns a string if it is a string.
 *
 * @param[in]  aJsonString  A Json string.
 * @param[out] aString      The string.
 *
 * @returns A boolean indicating whether the Json string was indeed a string.
 */
bool JsonString2String(const std::string &aJsonString, std::string &aString);

/**
 * This method formats a Node object to a Json object and serialize it to a string.
 *
 * @param[in] aNode  A Node object.
 *
 * @returns A string of serialized Json object.
 */
std::string Node2JsonString(const NodeInfo &aNode);

/**
 * This method formats a Node object to a Json object and serialize the key-value pairs defined in aFieldset to a
 * string.
 *
 * @param[in] aNode  A Node object.
 * @param[in] aFieldset  A set of strings comprising requested keys. A empty set defaults to any key.
 *
 * @returns A string of serialized Json object.
 *
 */
std::string SparseNode2JsonString(const NodeInfo &aNode, std::set<std::string> aFieldset);

/**
 * This method formats a vector of diagnostic objects to a Json array and serialize it to a string.
 *
 * @param[in] aDiagSet  A vector of diagnostic objects.
 * @param[in] aChildTable  A vector of diagnostic objects.
 * @param[in] aChildIps  A vector of diagnostic objects.
 * @param[in] aNeighbors  A vector of diagnostic objects.
 * @param[in] aDeviceTlvSetExtension  A vector of diagnostic objects.
 *
 * @returns A string of serialized Json array.
 *
 */
std::string DiagSet2JsonString(const std::vector<otNetworkDiagTlv>         &aDiagSet,
                               std::vector<otMeshDiagChildEntry>            aChildTable,
                               std::vector<DeviceIp6Addrs>                  aChildIps,
                               std::vector<otMeshDiagRouterNeighborEntry>   aNeighbors,
                               const std::vector<networkDiagTlvExtensions> &aDeviceTlvSetExtension,
                               std::set<std::string>                        aFieldset);

/**
 * This method formats a vector of diagnostic objects to a Json array and serialize it to a string.
 *
 * @param[in] aDiagSet  A vector of diagnostic objects.
 *
 * @returns A string of serialized Json array.
 */
std::string Diag2JsonString(const std::vector<std::vector<otNetworkDiagTlv>> &aDiagCollection,
                            const std::vector<uint16_t>                      &aUnresponsive);

/**
 * This method formats a pre-formated Json string of attributes to a Json:Api item string.
 *
 * @param[in] aId  A string of a Json:Api compliant Id.
 * @param[in] aType  A string of a Json:Api compliant type name.
 * @param[in] aAttribute  A pre-formated Json string of a attributes.
 *
 * @returns A string of a serialized Json:Api item.
 *
 */
std::string jsonStr2JsonApiItem(std::string aId, const std::string aType, std::string aAttribute);

/**
 * This method formats a pre-formated Json:Api string of data to a Json:Api Collection including meta data.
 *
 * @param[in] data  A string of a Json:Api compliant data.
 * @param[in] meta  A string of a Json:Api compliant meta data.
 *
 * @returns A string of a serialized Json:Api collection.
 *
 */
std::string jsonStr2JsonApiColl(std::string data, std::string meta = "");

/**
 * This method formats a EnergyScanReport to a Json string and serialize it to a string.
 *
 * @param[in] aReport  A EnergyScanReport object.
 *
 * @returns A string of a serialized Json object.
 *
 */
std::string EnergyReport2JsonString(const EnergyScanReport &aReport);

/**
 * This method formats a EnergyScanReport to a Json object and serialize the key-value pairs defined in aFieldset to a
 * string.
 *
 * @param[in] aNode  A EnergyScanReport object.
 * @param[in] aFieldset  A set of strings comprising requested keys. A empty set defaults to any key.
 *
 * @returns A string of a serialized Json object.
 *
 */
std::string SparseEnergyReport2JsonString(const EnergyScanReport &aReport, std::set<std::string> aFieldset);

/**
 * This method formats a device info to a Json object and serialize it to a string.
 *
 * @param[in] aDeviceInfo  A DeviceInfo object.
 *
 * @returns A string of a serialized Json object.
 *
 */
std::string DeviceInfo2JsonString(const DeviceInfo &aDeviceInfo);

/**
 * This method formats a device info to a Json object and serialize the key-value pairs defined in aFieldset to a
 * string.
 *
 * @param[in] aDeviceInfo  A DeviceInfo object.
 * @param[in] aFieldset  A set of strings comprising requested keys. A empty set defaults to any key.
 *
 * @returns A string of a serialized Json object.
 *
 */
std::string SparseDeviceInfo2JsonString(const DeviceInfo &aDeviceInfo, std::set<std::string> aFieldset);

/**
 * This method formats an Ipv6Address to a Json string and serialize it to a string.
 *
 * @param[in] aAddress  An Ip6Address object.
 *
 * @returns A string of serialized Json string.
 */
std::string IpAddr2JsonString(const otIp6Address &aAddress);

/**
 * This method formats a LinkModeConfig object to a Json object and serialize it to a string.
 *
 * @param[in] aMode  A LinkModeConfig object.
 *
 * @returns A string of serialized Json object.
 */
std::string Mode2JsonString(const otLinkModeConfig &aMode);

/**
 * This method formats a Connectivity object to a Json object and serialize it to a string.
 *
 * @param[in] aConnectivity  A Connectivity object.
 *
 * @returns A string of serialized Json object.
 */
std::string Connectivity2JsonString(const otNetworkDiagConnectivity &aConnectivity);

/**
 * This method formats a Route object to a Json object and serialize it to a string.
 *
 * @param[in] aRoute  A Route object.
 *
 * @returns A string of serialized Json object.
 */
std::string Route2JsonString(const otNetworkDiagRoute &aRoute);

/**
 * This method formats a RouteData object to a Json object and serialize it to a string.
 *
 * @param[in] aRouteData  A RouteData object.
 *
 * @returns A string of serialized Json object.
 */
std::string RouteData2JsonString(const otNetworkDiagRouteData &aRouteData);

/**
 * This method formats a LeaderData object to a Json object and serialize it to a string.
 *
 * @param[in] aLeaderData  A LeaderData object.
 *
 * @returns A string of serialized Json object.
 */
std::string LeaderData2JsonString(const otLeaderData &aLeaderData);

/**
 * This method formats a MacCounters object to a Json object and serialize it to a string.
 *
 * @param[in] aMacCounters  A MacCounters object.
 *
 * @returns A string of serialized Json object.
 */
std::string MacCounters2JsonString(const otNetworkDiagMacCounters &aMacCounters);

/**
 * This method formats a ChildEntry object to a Json object and serialize it to a string.
 *
 * @param[in] aChildEntry  A ChildEntry object.
 *
 * @returns A string of serialized Json object.
 */
std::string ChildTableEntry2JsonString(const otNetworkDiagChildEntry &aChildEntry);

/**
 * This method formats an error code and an error message to a Json object and serialize it to a string.
 *
 * @param[in] aErrorCode     An enum HttpStatusCode  such as '404'.
 * @param[in] aErrorMessage  Error message such as '404 Not Found'.
 *
 * @returns A string of serialized Json object.
 */
std::string Error2JsonString(HttpStatusCode aErrorCode, std::string aErrorMessage);

/**
 * This method formats a Json object from an active dataset.
 *
 * @param[in] aDataset  A dataset struct.
 *
 * @returns A string of serialized Json object.
 */
std::string ActiveDataset2JsonString(const otOperationalDataset &aDataset);

/**
 * This method formats a Json object from a pending dataset.
 *
 * @param[in] aDataset  A dataset struct.
 *
 * @returns A string of serialized Json object.
 */
std::string PendingDataset2JsonString(const otOperationalDataset &aPendingDataset);

/**
 * This method parses a Json string and fills the provided dataset. Fields
 * set to null are cleared (set to not present). Non-present fields are left
 * as is.
 *
 * @param[in] aJsonActiveDataset  The Json string to be parsed.
 * @param[in] aDataset            The dataset struct to be filled.
 *
 * @returns If the Json string has been successfully parsed.
 */
bool JsonActiveDatasetString2Dataset(const std::string &aJsonActiveDataset, otOperationalDataset &aDataset);

/**
 * This method parses a Json string and fills the provided dataset. Fields
 * set to null are cleared (set to not present). Non-present fields are left
 * as is.
 *
 * @param[in] aJsonActiveDataset  The Json string to be parsed.
 * @param[in] aDataset            The dataset struct to be filled.
 *
 * @returns If the Json string has been successfully parsed.
 */
bool JsonPendingDatasetString2Dataset(const std::string &aJsonPendingDataset, otOperationalDataset &aDataset);

}; // namespace Json

} // namespace rest
} // namespace otbr

#endif // OTBR_REST_JSON_HPP_
