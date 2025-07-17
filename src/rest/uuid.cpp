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

#include "uuid.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>

namespace otbr {
namespace rest {

Uuid::Uuid()
{
    std::memset(&uuid, 0, sizeof(uuid));
}

void Uuid::GenerateRandom()
{
    std::random_device              rd;           // Seed for random number engine
    std::mt19937                    gen(rd());    // Mersenne Twister RNG
    std::uniform_int_distribution<> dist(0, 255); // Byte range: 0 to 255

    for (size_t i = 0; i < UUID_LEN; ++i)
    {
        uuid.buf[i] = static_cast<unsigned char>(dist(gen));
    }

    // Mark off appropriate bits as per RFC4122 sction 4.4
    uuid.clock_seq_hi_and_reserved = (uuid.clock_seq_hi_and_reserved & 0x3F) | 0x80;
    uuid.time_hi_and_version       = (uuid.time_hi_and_version & 0x0FFF) | 0x4000;
}

std::string Uuid::ToString() const
{
    char out[UUID_STR_LEN];
    std::snprintf(out, UUID_STR_LEN, "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x", uuid.time_low, uuid.time_mid,
                  uuid.time_hi_and_version, uuid.clock_seq_hi_and_reserved, uuid.clock_seq_low, uuid.node[0],
                  uuid.node[1], uuid.node[2], uuid.node[3], uuid.node[4], uuid.node[5]);
    return std::string(out);
}

bool Uuid::Parse(const std::string &str)
{
    if (str.length() != UUID_STR_LEN - 1)
    {
        return false;
    }

    int temp[11] = {0};
    int r = std::sscanf(str.c_str(), "%8x-%4x-%4x-%2x%2x-%2x%2x%2x%2x%2x%2x", &temp[0], &temp[1], &temp[2], &temp[3],
                        &temp[4], &temp[5], &temp[6], &temp[7], &temp[8], &temp[9], &temp[10]);

    if (r != 11)
    {
        return false;
    }

    uuid.time_low                  = temp[0];
    uuid.time_mid                  = temp[1];
    uuid.time_hi_and_version       = temp[2];
    uuid.clock_seq_hi_and_reserved = temp[3];
    uuid.clock_seq_low             = temp[4];
    for (int i = 0; i < 6; i++)
    {
        uuid.node[i] = temp[5 + i];
    }
    return true;
}

bool Uuid::Equals(const Uuid &other) const
{
    return uuid.time_low == other.uuid.time_low && uuid.time_mid == other.uuid.time_mid &&
           uuid.time_hi_and_version == other.uuid.time_hi_and_version &&
           uuid.clock_seq_hi_and_reserved == other.uuid.clock_seq_hi_and_reserved &&
           uuid.clock_seq_low == other.uuid.clock_seq_low &&
           std::memcmp(uuid.node, other.uuid.node, sizeof(uuid.node)) == 0;
}

} // namespace rest
} // namespace otbr
