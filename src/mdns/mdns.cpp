/*
 *    Copyright (c) 2021, The OpenThread Authors.
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

/**
 * @file
 *   This file includes implementation of mDNS publisher.
 */

#include "mdns/mdns.hpp"

#include "common/code_utils.hpp"

namespace otbr {

namespace Mdns {

bool Publisher::IsServiceTypeEqual(const char *aFirstType, const char *aSecondType)
{
    size_t firstLength  = strlen(aFirstType);
    size_t secondLength = strlen(aSecondType);

    if (firstLength > 0 && aFirstType[firstLength - 1] == '.')
    {
        --firstLength;
    }
    if (secondLength > 0 && aSecondType[secondLength - 1] == '.')
    {
        --secondLength;
    }

    return firstLength == secondLength && memcmp(aFirstType, aSecondType, firstLength) == 0;
}

otbrError Publisher::EncodeTxtData(const TxtList &aTxtList, uint8_t *aTxtData, uint16_t &aTxtLength)
{
    otbrError error = OTBR_ERROR_NONE;
    uint8_t * cur   = aTxtData;

    for (const auto &txtEntry : aTxtList)
    {
        const char *   name        = txtEntry.mName.c_str();
        const size_t   nameLength  = txtEntry.mName.length();
        const uint8_t *value       = txtEntry.mValue.data();
        const size_t   valueLength = txtEntry.mValue.size();
        const size_t   entryLength = nameLength + 1 + valueLength;

        VerifyOrExit(nameLength > 0 && nameLength <= kMaxTextEntrySize, error = OTBR_ERROR_INVALID_ARGS);
        VerifyOrExit(cur + entryLength + 1 <= aTxtData + aTxtLength, error = OTBR_ERROR_INVALID_ARGS);

        cur[0] = static_cast<uint8_t>(entryLength);
        ++cur;

        memcpy(cur, name, nameLength);
        cur += nameLength;

        cur[0] = '=';
        ++cur;

        memcpy(cur, value, valueLength);
        cur += valueLength;
    }

    aTxtLength = cur - aTxtData;

exit:
    return error;
}

} // namespace Mdns

} // namespace otbr
