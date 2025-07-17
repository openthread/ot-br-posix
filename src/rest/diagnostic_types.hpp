/*
 *  Copyright (c) 2025, The OpenThread Authors.
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

#ifndef OTBR_REST_DIAGNOSTIC_TYPES_HPP_
#define OTBR_REST_DIAGNOSTIC_TYPES_HPP_

#include <cstdint>
#include <string>
#include <unordered_map>

#include "openthread/error.h"

namespace otbr {
namespace rest {

class DiagnosticTypes
{
public:
    static constexpr uint32_t kMaxTotalCount      = 26; ///< Total number of recognized types
    static constexpr uint32_t kMaxQueryCount      = 3;  ///< Number of types that require a diag query
    static constexpr uint32_t kMaxResettableCount = 2;  ///< Number of types that can be reset

    static const char *GetJsonKey(uint8_t aTypeId);
    static bool        RequiresQuery(uint8_t aTypeId);
    static bool        CanReset(uint8_t aTypeId);
    static bool        Omittable(uint8_t aTypeId);

    static otError FindId(const char *aJsonKey, uint8_t &aTypeId);

private:
    static constexpr uint32_t kTypeListSize = 35;

    enum PropertyFlags
    {
        kPropertyCanReset  = 1,      ///< Can be reset
        kPropertyQuery     = 1 << 1, ///< Requires a openthread query request
        kPropertyOmittable = 1 << 2, ///< May be omitted in a response
    };

    struct TypeInfo
    {
        const char *mJsonKey;
        uint32_t    mProperties;
    };

    static constexpr TypeInfo kTypeInfos[kTypeListSize] = {
        {"extAddress", 0},                  // 0
        {"rloc16", 0},                      // 1
        {"mode", 0},                        // 2
        {"timeout", kPropertyOmittable},    // 3
        {"connectivity", 0},                // 4
        {"route", 0},                       // 5
        {"leaderData", 0},                  // 6
        {"networkData", 0},                 // 7
        {"ipv6Addresses", 0},               // 8
        {"macCounters", kPropertyCanReset}, // 9
        {nullptr, 0},
        {nullptr, 0},
        {nullptr, 0},
        {nullptr, 0},
        {"batteryLevel", kPropertyOmittable},  // 14
        {"supplyVoltage", kPropertyOmittable}, // 15
        {"childTable", 0},                     // 16
        {"channelPages", 0},                   // 17
        {nullptr, 0},
        {"maxChildTimeout", kPropertyOmittable}, // 19
        {"lDevIdSubject", 0},                    // 20
        {"iDevIdCert", 0},                       // 21
        {nullptr, 0},
        {"eui64", 0},                           // 23
        {"version", 0},                         // 24
        {"vendorName", 0},                      // 25
        {"vendorModel", 0},                     // 26
        {"vendorSwVersion", 0},                 // 27
        {"threadStackVersion", 0},              // 28
        {"children", kPropertyQuery},           // 29
        {"childIpv6Addresses", kPropertyQuery}, // 30
        {"routerNeighbors", kPropertyQuery},    // 31
        {nullptr, 0},
        {nullptr, 0},
        {"mleCounters", kPropertyCanReset} // 34
    };

    static const std::unordered_map<std::string, uint8_t> kKeyMap;
};

} // namespace rest
} // namespace otbr

#endif // OTBR_REST_DIAGNOSTIC_TYPES_HPP_
