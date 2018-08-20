/*
 *    Copyright (c) 2018, The OpenThread Authors.
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
 *   The file is the implementation of the address manipulation utilities for the commissioner test app.
 */

#include "addr.hpp"
#include <string.h>

#include <arpa/inet.h>

namespace ot {
namespace Utils {

static const uint8_t kLociidPrefix[] = {
    0x00, 0x00, 0x00, 0xff, 0xfe, 0x00,
};

enum
{
    kRlocRouterIDBitOffset = 10,
    kRlocAddrUint16Offset  = 7,
    kRlocAddrUint8Offset   = 14,
    kAlocRouterByte        = 0xfc,
    kIidAddrUint8Offset    = 8,
    kUlaPrefix             = 0xfd,
};

uint16_t ToRloc16(uint8_t aRouterId, uint16_t aChildId)
{
    uint16_t rloc16 = aRouterId;

    return (rloc16 << kRlocRouterIDBitOffset) | aChildId;
}

char *GetIPString(const struct sockaddr *aAddr, char *aOutBuf, size_t aLength)
{
    switch (aAddr->sa_family)
    {
    case AF_INET:
        inet_ntop(AF_INET, &((reinterpret_cast<const struct sockaddr_in *>(aAddr))->sin_addr), aOutBuf, aLength);
        break;

    case AF_INET6:
        inet_ntop(AF_INET6, &((reinterpret_cast<const struct sockaddr_in6 *>(aAddr))->sin6_addr), aOutBuf, aLength);
        break;

    default:
        strncpy(aOutBuf, "Unknown AF", aLength);
        return NULL;
    }

    return aOutBuf;
}

struct in6_addr ConcatRloc16Address(const in6_addr &aPrefix, uint16_t aRloc16)
{
    in6_addr addr = aPrefix;

    reinterpret_cast<uint16_t *>(&addr)[kRlocAddrUint16Offset] = htons(aRloc16);
    return addr;
}

struct in6_addr ConcatRloc16Address(const in6_addr &aPrefix, uint8_t aRouterId, uint16_t aChildId)
{
    return ConcatRloc16Address(aPrefix, ToRloc16(aRouterId, aChildId));
}

struct in6_addr FindRloc16Address(const std::vector<struct in6_addr> &aAddrs)
{
    struct in6_addr foundAddr;

    memset(&foundAddr, 0, sizeof(in6_addr));
    for (size_t i = 0; i < aAddrs.size(); i++)
    {
        const uint8_t *buf = reinterpret_cast<const uint8_t *>(&aAddrs[i]);
        if (!memcmp(&buf[kIidAddrUint8Offset], &kLociidPrefix[0], sizeof(kLociidPrefix)) &&
            buf[kRlocAddrUint8Offset] != kAlocRouterByte)
        {
            foundAddr = aAddrs[i];
        }
    }
    return foundAddr;
}

struct in6_addr FindMLEIDAddress(const std::vector<struct in6_addr> &aAddrs)
{
    struct in6_addr foundAddr;

    memset(&foundAddr, 0, sizeof(in6_addr));
    for (size_t i = 0; i < aAddrs.size(); i++)
    {
        const uint8_t *buf = reinterpret_cast<const uint8_t *>(&aAddrs[i]);
        if (buf[0] == kUlaPrefix && memcmp(&buf[kIidAddrUint8Offset], &kLociidPrefix[0], sizeof(kLociidPrefix)))
        {
            foundAddr = aAddrs[i];
        }
    }
    return foundAddr;
}

struct in6_addr GetRlocPrefix(const std::vector<struct in6_addr> &aAddrs)
{
    struct in6_addr rlocAddr = FindRloc16Address(aAddrs);

    return ToRlocPrefix(rlocAddr);
}

struct in6_addr ToRlocPrefix(const struct in6_addr &aRlocAddr)
{
    struct in6_addr prefix = aRlocAddr;

    reinterpret_cast<uint16_t *>(&prefix)[kRlocAddrUint16Offset] = 0;
    return prefix;
}

} // namespace Utils
} // namespace ot
