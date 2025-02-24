/*
 *  Copyright (c) 2017, The OpenThread Authors.
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
 *   This file provides kinds of convertion functions.
 */

#include "utils/hex.hpp"

#include <string>

#include <stdio.h>
#include <string.h>

namespace otbr {

namespace Utils {

int Hex2Bytes(const char *aHex, uint8_t *aBytes, uint16_t aBytesLength)
{
    size_t      hexLength = strlen(aHex);
    const char *hexEnd    = aHex + hexLength;
    uint8_t    *cur       = aBytes;
    uint8_t     numChars  = hexLength & 1;
    uint8_t     byte      = 0;

    if ((hexLength + 1) / 2 > aBytesLength)
    {
        return -1;
    }

    while (aHex < hexEnd)
    {
        if ('A' <= *aHex && *aHex <= 'F')
        {
            byte |= 10 + (*aHex - 'A');
        }
        else if ('a' <= *aHex && *aHex <= 'f')
        {
            byte |= 10 + (*aHex - 'a');
        }
        else if ('0' <= *aHex && *aHex <= '9')
        {
            byte |= *aHex - '0';
        }
        else
        {
            return -1;
        }

        aHex++;
        numChars++;

        if (numChars >= 2)
        {
            numChars = 0;
            *cur++   = byte;
            byte     = 0;
        }
        else
        {
            byte <<= 4;
        }
    }

    return static_cast<int>(cur - aBytes);
}

size_t Bytes2Hex(const uint8_t *aBytes, const uint16_t aBytesLength, char *aHex)
{
    char byteHex[3];

    // Make sure strcat appends at the beginning of the output buffer even
    // if uninitialized.
    aHex[0] = '\0';

    for (int i = 0; i < aBytesLength; i++)
    {
        snprintf(byteHex, sizeof(byteHex), "%02X", aBytes[i]);
        strcat(aHex, byteHex);
    }

    return strlen(aHex);
}

std::string Bytes2Hex(const uint8_t *aBytes, const uint16_t aBytesLength)
{
    char       *hex = new char[2 * aBytesLength + 1];
    std::string s;
    size_t      len;

    len = Bytes2Hex(aBytes, aBytesLength, hex);
    s   = std::string(hex, len);
    delete[] hex;

    return s;
}

size_t Long2Hex(const uint64_t aLong, char *aHex)
{
    char byteHex[3];

    // Make sure strcat appends at the beginning of the output buffer even
    // if uninitialized.
    aHex[0] = '\0';

    for (uint8_t i = 0; i < sizeof(aLong); i++)
    {
        uint8_t byte = (aLong >> (8 * (sizeof(aLong) - i - 1))) & 0xff;
        snprintf(byteHex, sizeof(byteHex), "%02X", byte);
        strcat(aHex, byteHex);
    }

    return strlen(aHex);
}

} // namespace Utils

} // namespace otbr
