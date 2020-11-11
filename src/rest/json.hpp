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
 *   This file includes JSON formater definition for RESTful HTTP server.
 */

#ifndef OTBR_REST_JSON_HPP_
#define OTBR_REST_JSON_HPP_

#include "openthread/link.h"
#include "openthread/netdata.h"
#include "openthread/thread_ftd.h"

#include "rest/types.hpp"
#include "utils/hex.hpp"

namespace otbr {
namespace Rest {

/**
 * The functions within this namespace provides a tranformation
 * from an object/string/number to a serialized JSON string.
 *
 */
namespace Json {

/**
 * This method formats an integer to a JSON number and serialize it to a string.
 *
 * @param[in]  aNumber  The given integer.
 *
 * @returns The serialized JSON string.
 *
 */
std::string Number2JsonString(const uint32_t &aNumber);

/**
 * This method formats a Bytes array to a JSON string and serialize it to a string.
 *
 * @param[in]  aBytes  The given byte array.
 * @param[in]  aLength The length of the byte array.
 *
 * @returns The serialized JSON string.
 *
 */
std::string Bytes2HexJsonString(const uint8_t *aBytes, uint8_t aLength);

/**
 * This method formats a C string to a JSON string and serialize it to a string.
 *
 * @param[in]  aCString  A char pointer to a C string.
 *
 * @returns The serialized JSON string.
 *
 */
std::string CString2JsonString(const char *aCString);

/**
 * This method formats a string to a JSON string and serialize it to a string.
 *
 * @param[in]  aString  The given string.
 *
 * @returns The serialized JSON string.
 *
 */
std::string String2JsonString(const std::string &aString);

/**
 * This method formats a NodeInfo object to a JSON object and serialize it to a string.
 *
 * @param[in]  aNode  The given NodeInfo object.
 *
 * @returns The serialized JSON string.
 *
 */
std::string Node2JsonString(const NodeInfo &aNode);

/**
 * This method formats diagnostic informations to a JSON array and serialize it to a string.
 *
 * @param[in]  aDiagInfos  The given diagnostic informations.
 *
 * @returns The serialized JSON string.
 *
 */
std::string Diag2JsonString(const std::vector<std::vector<otNetworkDiagTlv>> &aDiagInfos);

/**
 * This method formats an IPv6 address to a JSON string and serialize it to a string.
 *
 * @param[in]  aAddress  An IPv6 address.
 *
 * @returns The serialized JSON string.
 *
 */
std::string IpAddr2JsonString(const otIp6Address &aAddress);

/**
 * This method formats extracts prefixes from a list of config object and format them as JSON Array.
 *
 * @param[in]  aConfigs  An array of Border Router Configurations.
 *
 * @returns The serialized JSON string.
 *
 */
std::string PrefixList2JsonString(const std::vector<otBorderRouterConfig> &aConfigs);

/**
 * This method formats a LinkModeConfig object to a JSON object and serialize it to a string.
 *
 * @param[in]  aMode  A LinkModeConfig object.
 *
 * @returns The serialized JSON string.
 *
 */
std::string Mode2JsonString(const otLinkModeConfig &aMode);

/**
 * This method formats a Connectivity object to a JSON object and serialize it to a string.
 *
 * @param[in]  aConnectivity  A Connectivity object.
 *
 * @returns The serialized JSON string.
 *
 */
std::string Connectivity2JsonString(const otNetworkDiagConnectivity &aConnectivity);

/**
 * This method formats a Route object to a JSON object and serialize it to a string.
 *
 * @param[in]  aRoute  A Route object.
 *
 * @returns The serialized JSON string.
 *
 */
std::string Route2JsonString(const otNetworkDiagRoute &aRoute);

/**
 * This method formats a RouteData object to a JSON object and serialize it to a string.
 *
 * @param[in]  aRouteData  A RouteData object.
 *
 * @returns The serialized JSON string.
 *
 */
std::string RouteData2JsonString(const otNetworkDiagRouteData &aRouteData);

/**
 * This method formats a LeaderData object to a JSON object and serialize it to a string.
 *
 * @param[in]  aLeaderData  A LeaderData object.
 *
 * @returns The serialized JSON string.
 *
 */
std::string LeaderData2JsonString(const otLeaderData &aLeaderData);

/**
 * This method formats a MacCounters object to a JSON object and serialize it to a string.
 *
 * @param[in]  aMacCounters  A MacCounters object.
 *
 * @returns The serialized JSON string.
 *
 */
std::string MacCounters2JsonString(const otNetworkDiagMacCounters &aMacCounters);

/**
 * This method formats a ChildEntry object to a JSON object and serialize it to a string.
 *
 * @param[in]  aChildEntry  A ChildEntry object.
 *
 * @returns The serialized JSON string.
 *
 */
std::string ChildTableEntry2JsonString(const otNetworkDiagChildEntry &aChildEntry);

/**
 * This method formats an array of scan results to a JSON object and serialize it to a string.
 *
 * @param[in]  aResults  An array that contains scan results.
 *
 * @returns The serialized JSON string.
 *
 */
std::string ScanNetworks2JsonString(const std::vector<ActiveScanResult> &aResults);

/**
 * This method formats an array of scan results to a JSON object and serialize it to a string.
 *
 * @param[in]  aResults  An array that contains scan results.
 *
 * @returns The serialized JSON string.
 *
 */
std::string Network2JsonString(const otOperationalDataset &aDataset);

/**
 * This method formats an error code and an error message to a JSON object and serialize it to a string.
 *
 * @param[in]  aErrorCode     An enum HttpStatusCode such as '404'.
 * @param[in]  aErrorMessage  An error message such as '404 Not Found'.
 * @param[in]  aDescription   A detailed error information.
 *
 * @returns The serialized JSON string.
 *
 */
std::string Error2JsonString(HttpStatusCode aErrorCode, std::string aErrorMessage, std::string aDescription);

/**
 * This method decodes a JSON object to a NetworkConfiguration object .
 *
 * @param[in]   aString   A string contains JSON object.
 * @param[out]  aNetwork  A reference to a NetworkConfiguration instance that could be set within this method.
 *
 * @returns A bool value that indicates whether the string could be decoded to the NetworkConfiguration object
 *
 */
bool JsonString2NetworkConfiguration(std::string &aString, NetworkConfiguration &aNetwork);

/**
 * This method decodes a JSON object to a NetworkConfiguration object .
 *
 * @param[in]     aString  A string contains JSON object.
 * @param[in]     aKey     The key that may exist in the JSON object.
 * @param[inout]  aValue   The string that is the value of the key in this JSON object.
 *
 * @returns A bool value that indicates whether the key exist in this JSON object.
 *
 */
bool JsonString2String(std::string aString, std::string aKey, std::string &aValue);

/**
 * This method decodes a JSON object to a bool value .
 *
 * @param[in]     aString  A string contains JSON object.
 * @param[in]     aKey     The key that may exist in the JSON object.
 * @param[inout]  aValue   The bool value that is the value of the key in this JSON object.
 *
 * @returns A bool value that indicates whether the key exist in this JSON object.
 *
 */
bool JsonString2Bool(std::string aString, std::string aKey, bool &aValue);

/**
 * This method decodes a JSON object to a bool value .
 *
 * @param[in]     aString  A string contains JSON object.
 * @param[in]     aKey     The key that may exist in the JSON object.
 * @param[inout]  aValue   The integer value that is the value of the key in this JSON object.
 *
 * @returns A bool value that indicates whether the key exist in this JSON object.
 *
 */
bool JsonString2Int(std::string aString, std::string aKey, int32_t &aValue);

}; // namespace Json

} // namespace Rest
} // namespace otbr

#endif // OTBR_REST_JSON_HPP_
