/*
 *  Copyright (c) 2024, The OpenThread Authors.
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

#ifndef UUID_HPP
#define UUID_HPP

#include <cstdint>
#include <cstring>
#include <string>

namespace otbr {
namespace rest {

const int UUID_LEN     = 16;
const int UUID_STR_LEN = 37;

class UUID
{
public:
    UUID();
    void        GenerateRandom();
    std::string ToString() const;
    bool        Parse(const std::string &str);
    bool        Equals(const UUID &other) const;

    // copy constructor
    UUID(const UUID &other) { std::memcpy(&uuid, &other.uuid, sizeof(UUIDData)); }

    bool operator<(const UUID &other) const { return std::memcmp(uuid.buf, other.uuid.buf, UUID_LEN) < 0; }

    bool operator==(const UUID &other) const { return Equals(other); }

    // assigment operator
    UUID &operator=(const UUID &other)
    {
        if (this != &other)
        {
            std::memcpy(&uuid, &other.uuid, sizeof(UUIDData));
        }
        return *this;
    }

private:
    union UUIDData
    {
        struct
        {
            uint32_t time_low;
            uint16_t time_mid;
            uint16_t time_hi_and_version;
            uint8_t  clock_seq_hi_and_reserved;
            uint8_t  clock_seq_low;
            uint8_t  node[6];
        };
        uint8_t buf[UUID_LEN];
    } uuid;
};

} // namespace rest
} // namespace otbr

#endif // UUID_HPP
