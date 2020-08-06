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
 *   This file includes json formater definition for RESTful HTTP server.
 */

#ifndef OTBR_REST_JSON_HPP_
#define OTBR_REST_JSON_HPP_

#include "openthread/link.h"
#include "openthread/thread_ftd.h"

#include "agent/ncp_openthread.hpp"
#include "agent/thread_helper.hpp"
#include "rest/types.hpp"
#include "utils/hex.hpp"

namespace otbr {
namespace rest {

/**
 * The functions within this namespace provide a tranformation from an object/string/number to a serialized Json string.
 *
 */
namespace JSON {

     /**
     * This method format an integer to a Json number and serialize it to a string.
     *
     * @param[in]   aNumber  an integer need to be format.
     * 
     * @returns     a string serlialized by a Json number.
     *
     */
    std::string Number2JsonString(const uint32_t aNumber);

    /**
     * This method format a Bytes array to a Json string and serialize it to a string.
     *
     * @param[in]   aBytes  a Bytes array representing a hex number.
     * 
     * @returns     a string serlialized by a Json string.
     *
     */
    std::string Bytes2HexJsonString(const uint8_t *aBytes, uint8_t aLength);

    /**
     * This method format a C string to a Json string and serialize it to a string.
     *
     * @param[in]   aCString  a char pointer pointing to a C string.
     * 
     * @returns     a string serlialized by a Json string.
     *
     */
    std::string CString2JsonString(const char *aCString);

    /**
     * This method format a string to a Json string and serialize it to a string.
     *
     * @param[in]   aString  a string.
     * 
     * @returns     a string serlialized by a Json string.
     *
     */
    std::string String2JsonString(std::string aString);

    /**
     * This method format a Node object to a Json object and serialize it to a string.
     *
     * @param[in]   aNode  a Node object.
     * 
     * @returns     a string serlialized by a Json object.
     *
     */
    std::string Node2JsonString(const Node &aNode);

    /**
     * This method format a vector including serveral Diagnostic object to a Json array and serialize it to a string.
     *
     * @param[in]   aDiagSet  a vector including serveral Diagnostic object.
     * 
     * @returns     a string serlialized by a Json array.
     *
     */
    std::string Diag2JsonString(const std::vector<std::vector<otNetworkDiagTlv>> &aDiagSet);

    /**
     * This method format an Ipv6Address to a Json string and serialize it to a string.
     *
     * @param[in]   aAddress  an Ip6Address object.
     * 
     * @returns     a string serlialized by a Json string.
     *
     */
    std::string IpAddr2JsonString(const otIp6Address &aAddress);

    /**
     * This method format a LinkModeConfig object to a Json object and serialize it to a string.
     *
     * @param[in]   aMode  a LinkModeConfig object.
     * 
     * @returns     a string serlialized by a Json object.
     *
     */
    std::string Mode2JsonString(const otLinkModeConfig &aMode);

    /**
     * This method format a Connectivity object to a Json object and serialize it to a string.
     *
     * @param[in]   aConnectivity  a Connectivity object.
     * 
     * @returns     a string serlialized by a Json object.
     *
     */
    std::string Connectivity2JsonString(const otNetworkDiagConnectivity &aConnectivity);

    /**
     * This method format a Route object to a Json object and serialize it to a string.
     *
     * @param[in]   aRoute  a Route object.
     * 
     * @returns     a string serlialized by a Json object.
     *
     */
    std::string Route2JsonString(const otNetworkDiagRoute &aRoute);

    /**
     * This method format a RouteData object to a Json object and serialize it to a string.
     *
     * @param[in]   aRouteData  a RouteData object.
     * 
     * @returns     a string serlialized by a Json object.
     *
     */
    std::string RouteData2JsonString(const otNetworkDiagRouteData &aRouteData);

    /**
     * This method format a LeaderData object to a Json object and serialize it to a string.
     *
     * @param[in]   aLeaderData  a LeaderData object.
     * 
     * @returns     a string serlialized by a Json object.
     *
     */
    std::string LeaderData2JsonString(const otLeaderData &aLeaderData);

    /**
     * This method format a MacCounters object to a Json object and serialize it to a string.
     *
     * @param[in]   aMacCounters  a MacCounters object.
     * 
     * @returns     a string serlialized by a Json object.
     *
     */
    std::string MacCounters2JsonString(const otNetworkDiagMacCounters &aMacCounters);

    /**
     * This method format a ChildEntry object to a Json object and serialize it to a string.
     *
     * @param[in]   aChildEntry  a ChildEntry object.
     * 
     * @returns     a string serlialized by a Json object.
     *
     */
    std::string ChildTableEntry2JsonString(const otNetworkDiagChildEntry &aChildEntry);

    /**
     * This method format an error code and an error message to a Json object and serialize it to a string.
     *
     * @param[in]   aErrorCode  error code such as 404.
     * @param[in]   aErrorMessage  error message such as Not Found.
     * 
     * @returns     a string serlialized by a Json object.
     *
     */
    std::string Error2JsonString(uint32_t aErrorCode, std::string aErrorMessage);

}; // namespace JSON

} // namespace rest
} // namespace otbr

#endif // OTBR_REST_JSON_HPP_
