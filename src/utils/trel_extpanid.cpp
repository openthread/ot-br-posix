/*
 *  Copyright (c) 2026, The OpenThread Authors.
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
 *   Utilities for TREL Extended PAN ID exclusion.
 */

#include "utils/trel_extpanid.hpp"

#include <cstring>

#include "common/code_utils.hpp"

namespace otbr {
namespace Utils {

namespace {

bool ParseHexNibble(char aChar, uint8_t &aNibble)
{
    bool valid = true;

    if ('0' <= aChar && aChar <= '9')
    {
        aNibble = static_cast<uint8_t>(aChar - '0');
    }
    else if ('a' <= aChar && aChar <= 'f')
    {
        aNibble = static_cast<uint8_t>(10 + (aChar - 'a'));
    }
    else if ('A' <= aChar && aChar <= 'F')
    {
        aNibble = static_cast<uint8_t>(10 + (aChar - 'A'));
    }
    else
    {
        valid = false;
    }

    return valid;
}

bool IsHexSeparator(char aChar)
{
    return aChar == ':' || aChar == '-';
}

} // namespace

bool ParseTrelExcludeExtPanId(const char *aStr, TrelExtPanId &aOut)
{
    bool    success     = false;
    size_t  index       = 0;
    uint8_t nibbleCount = 0;
    uint8_t byte        = 0;

    VerifyOrExit(aStr != nullptr);

    for (const char *cur = aStr; *cur != '\0'; ++cur)
    {
        if (IsHexSeparator(*cur))
        {
            continue;
        }

        {
            uint8_t nibble = 0;

            VerifyOrExit(ParseHexNibble(*cur, nibble));

            if (nibbleCount == 0)
            {
                byte        = static_cast<uint8_t>(nibble << 4);
                nibbleCount = 1;
            }
            else
            {
                byte |= nibble;
                VerifyOrExit(index < kTrelExtPanIdLength);
                aOut[index++] = byte;
                nibbleCount   = 0;
            }
        }
    }

    VerifyOrExit(nibbleCount == 0);
    VerifyOrExit(index == kTrelExtPanIdLength);

    success = true;

exit:
    return success;
}

bool IsTrelExcludedExtPanId(const TrelExtPanId &aPeer, const std::vector<TrelExtPanId> &aExcluded)
{
    for (const TrelExtPanId &excluded : aExcluded)
    {
        if (aPeer == excluded)
        {
            return true;
        }
    }

    return false;
}

bool ShouldExcludeTrelPeer(bool aHasExtPanId, const TrelExtPanId &aPeerExtPanId, const std::vector<TrelExtPanId> &aExcluded)
{
    return aHasExtPanId && !aExcluded.empty() && IsTrelExcludedExtPanId(aPeerExtPanId, aExcluded);
}

bool ReadTrelExtPanIdFromTxtData(const uint8_t *aTxtData, uint16_t aTxtLength, TrelExtPanId &aOut, bool &aHasExtPanId)
{
    bool   success = true;
    size_t offset  = 0;

    aHasExtPanId = false;

    VerifyOrExit(aTxtData != nullptr);

    while (offset < aTxtLength)
    {
        size_t entrySize = aTxtData[offset];
        size_t keyStart  = offset + 1;
        size_t entryEnd  = keyStart + entrySize;
        size_t keyEnd    = keyStart;

        VerifyOrExit(entryEnd <= aTxtLength, success = false);

        while (keyEnd < entryEnd && aTxtData[keyEnd] != '=')
        {
            keyEnd++;
        }

        if (keyEnd != entryEnd)
        {
            size_t valStart = keyEnd + 1;
            size_t valLen   = entryEnd - valStart;

            if (keyEnd - keyStart == 2 && (aTxtData[keyStart] == 'x' || aTxtData[keyStart] == 'X') &&
                (aTxtData[keyStart + 1] == 'p' || aTxtData[keyStart + 1] == 'P'))
            {
                VerifyOrExit(valLen == kTrelExtPanIdLength, success = false);
                memcpy(aOut.data(), &aTxtData[valStart], kTrelExtPanIdLength);
                aHasExtPanId = true;
                break;
            }
        }

        offset = entryEnd;
    }

exit:
    return success;
}

} // namespace Utils
} // namespace otbr
