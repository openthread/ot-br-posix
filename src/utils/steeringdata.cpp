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

/* length of an EUI64 in bytes */
#define LEN_BIN_EUI64 (64 / 8)
/* length of EUI64 in ascii form */

#include "steeringdata.hpp"
#include "crc16.hpp"
#include "hex.hpp"
#include <stdio.h>
#include <string.h>

namespace ot {

bool SteeringData::IsCleared(void)
{
    bool rval = true;

    for (uint8_t i = 0; i < GetLength(); i++)
    {
        if (mSteeringData[i] != 0)
        {
            rval = false;
            break;
        }
    }

    return rval;
}

void SteeringData::ComputeBloomFilter(const uint8_t *aExtAddress)
{
    Crc16 ccitt(Crc16::kCcitt);
    Crc16 ansi(Crc16::kAnsi);

    for (size_t j = 0; j < (64 / 8); j++)
    {
        uint8_t byte = aExtAddress[j];
        ccitt.Update(byte);
        ansi.Update(byte);
    }

    SetBit(ccitt.Get() % GetNumBits());
    SetBit(ansi.Get() % GetNumBits());
}

bool SteeringData::ComputeBloomFilterAscii(const char *ascii_eui64)
{
    int     r;
    uint8_t bin_eui[LEN_BIN_EUI64];

    /* Test 1: simple check for length */
    if ((LEN_BIN_EUI64 * 2) != strlen(ascii_eui64))
    {
        return false;
    }

    /* convert string as hex bytes */
    r = ot::Utils::Hex2Bytes(ascii_eui64, bin_eui, sizeof(bin_eui));

    /* how many did we get? */
    if (r != LEN_BIN_EUI64)
    {
        return false;
    }

    ComputeBloomFilter(bin_eui);
    return true;
}

} // namespace ot
