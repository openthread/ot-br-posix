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
#include "rest/names.hpp"

namespace otbr {
namespace rest {

class DiagnosticTypes
{
public:
    static constexpr uint32_t kMaxTotalCount      = 27; ///< Total number of recognized types
    static constexpr uint32_t kMaxQueryCount      = 3;  ///< Number of types that require a diag query
    static constexpr uint32_t kMaxResettableCount = 2;  ///< Number of types that can be reset

    static constexpr uint32_t CountTotalTypes(uint32_t aIndex)
    {
        return (aIndex >= kTypeListSize)
                   ? 0
                   : ((kTypeInfos[aIndex].mJsonKey != nullptr) ? 1 : 0) + CountTotalTypes(aIndex + 1);
    }

    static constexpr uint32_t CountQueryTypes(uint32_t aIndex)
    {
        return (aIndex >= kTypeListSize)
                   ? 0
                   : (((kTypeInfos[aIndex].mJsonKey != nullptr) && (kTypeInfos[aIndex].mProperties & kPropertyQuery))
                          ? 1
                          : 0) +
                         CountQueryTypes(aIndex + 1);
    }

    static constexpr uint32_t CountResettableTypes(uint32_t aIndex)
    {
        return (aIndex >= kTypeListSize)
                   ? 0
                   : (((kTypeInfos[aIndex].mJsonKey != nullptr) && (kTypeInfos[aIndex].mProperties & kPropertyCanReset))
                          ? 1
                          : 0) +
                         CountResettableTypes(aIndex + 1);
    }

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
        {KEY_EXTADDRESS, 0},                  // 0
        {KEY_RLOC16, 0},                      // 1
        {KEY_MODE, 0},                        // 2
        {KEY_TIMEOUT, kPropertyOmittable},    // 3
        {KEY_CONNECTIVITY, 0},                // 4
        {KEY_ROUTE, 0},                       // 5
        {KEY_LEADERDATA, 0},                  // 6
        {KEY_NETWORKDATA, 0},                 // 7
        {KEY_IP6ADDRESSLIST, 0},              // 8
        {KEY_MACCOUNTERS, kPropertyCanReset}, // 9
        {nullptr, 0},
        {nullptr, 0},
        {nullptr, 0},
        {nullptr, 0},
        {KEY_BATTERYLEVEL, kPropertyOmittable},  // 14
        {KEY_SUPPLYVOLTAGE, kPropertyOmittable}, // 15
        {KEY_CHILDTABLE, 0},                     // 16
        {KEY_CHANNELPAGES, 0},                   // 17
        {nullptr, 0},
        {KEY_MAXCHILDTIMEOUT, kPropertyOmittable}, // 19
        {KEY_LDEVIDSUBJECT, 0},                    // 20
        {KEY_IDEVIDCERT, 0},                       // 21
        {nullptr, 0},
        {KEY_EUI, 0},                             // 23
        {KEY_THREADVERSION, 0},                   // 24
        {KEY_VENDORNAME, 0},                      // 25
        {KEY_VENDORMODEL, 0},                     // 26
        {KEY_VENDORSWVERSION, 0},                 // 27
        {KEY_THREADSTACKVERSION, 0},              // 28
        {KEY_CHILDREN, kPropertyQuery},           // 29
        {KEY_CHILDIPV6ADDRESSES, kPropertyQuery}, // 30
        {KEY_ROUTERNEIGHBORS, kPropertyQuery},    // 31
        {nullptr, 0},
        {nullptr, 0},
        {KEY_MLECOUNTERS, kPropertyCanReset} // 34
    };

    static const std::unordered_map<std::string, uint8_t> kKeyMap;
};

static_assert(DiagnosticTypes::kMaxTotalCount == DiagnosticTypes::CountTotalTypes(0),
              "kMaxTotalCount must match the number of recognized types");
static_assert(DiagnosticTypes::kMaxQueryCount == DiagnosticTypes::CountQueryTypes(0),
              "kMaxQueryCount must match the number of query types");
static_assert(DiagnosticTypes::kMaxResettableCount == DiagnosticTypes::CountResettableTypes(0),
              "kMaxResettableCount must match the number of resettable types");

} // namespace rest
} // namespace otbr

#endif // OTBR_REST_DIAGNOSTIC_TYPES_HPP_
