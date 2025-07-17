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

#include "diagnostic_types.hpp"

#include "common/code_utils.hpp"

namespace otbr {
namespace rest {

constexpr uint32_t DiagnosticTypes::kMaxTotalCount;
constexpr uint32_t DiagnosticTypes::kMaxQueryCount;
constexpr uint32_t DiagnosticTypes::kMaxResettableCount;

const char *DiagnosticTypes::GetJsonKey(uint8_t aTypeId)
{
    const char *key = nullptr;

    VerifyOrExit(aTypeId < kTypeListSize);
    key = kTypeInfos[aTypeId].mJsonKey;

exit:
    return key;
}

bool DiagnosticTypes::RequiresQuery(uint8_t aTypeId)
{
    bool retval = false;

    VerifyOrExit(aTypeId < kTypeListSize);
    retval = kTypeInfos[aTypeId].mProperties & kPropertyQuery;

exit:
    return retval;
}

bool DiagnosticTypes::CanReset(uint8_t aTypeId)
{
    bool retval = false;

    VerifyOrExit(aTypeId < kTypeListSize);
    retval = kTypeInfos[aTypeId].mProperties & kPropertyCanReset;

exit:
    return retval;
}

bool DiagnosticTypes::Omittable(uint8_t aTypeId)
{
    bool retval = false;

    VerifyOrExit(aTypeId < kTypeListSize);
    retval = kTypeInfos[aTypeId].mProperties & kPropertyOmittable;

exit:
    return retval;
}

otError DiagnosticTypes::FindId(const char *aJsonKey, uint8_t &aTypeId)
{
    otError error = OT_ERROR_NONE;
    auto    it    = kKeyMap.find(aJsonKey);

    VerifyOrExit(it != kKeyMap.end(), error = OT_ERROR_NOT_FOUND);
    aTypeId = it->second;

exit:
    return error;
}

constexpr uint32_t DiagnosticTypes::kTypeListSize;

constexpr DiagnosticTypes::TypeInfo DiagnosticTypes::kTypeInfos[kTypeListSize];

#define MAP_ENTRY(i) {kTypeInfos[i].mJsonKey, i}
const std::unordered_map<std::string, uint8_t> DiagnosticTypes::kKeyMap = {
    MAP_ENTRY(0),  //
    MAP_ENTRY(1),  //
    MAP_ENTRY(2),  //
    MAP_ENTRY(3),  //
    MAP_ENTRY(4),  //
    MAP_ENTRY(5),  //
    MAP_ENTRY(6),  //
    MAP_ENTRY(7),  //
    MAP_ENTRY(8),  //
    MAP_ENTRY(9),  //
    MAP_ENTRY(14), //
    MAP_ENTRY(15), //
    MAP_ENTRY(16), //
    MAP_ENTRY(17), //
    MAP_ENTRY(19), //
    MAP_ENTRY(20), //
    MAP_ENTRY(21), //
    MAP_ENTRY(23), //
    MAP_ENTRY(24), //
    MAP_ENTRY(25), //
    MAP_ENTRY(26), //
    MAP_ENTRY(27), //
    MAP_ENTRY(28), //
    MAP_ENTRY(29), //
    MAP_ENTRY(30), //
    MAP_ENTRY(31), //
    MAP_ENTRY(34), //
};

} // namespace rest
} // namespace otbr
