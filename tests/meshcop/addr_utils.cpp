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

#include "addr_utils.hpp"
#include <string.h>

namespace ot {
namespace BorderRouter {

static const uint8_t kLociid[] = {
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

uint16_t ToRloc16(uint8_t routerID, uint16_t childID)
{
    uint16_t rloc16 = routerID;

    return (rloc16 << kRlocRouterIDBitOffset) | childID;
}

#define IPSTR_BUFSIZE (((INET6_ADDRSTRLEN > INET_ADDRSTRLEN) ? INET6_ADDRSTRLEN : INET_ADDRSTRLEN) + 1)
char *GetIPString(const struct sockaddr *sa, char *s, size_t maxlen)
{
    switch (sa->sa_family)
    {
    case AF_INET:
        inet_ntop(AF_INET, &(((struct sockaddr_in *)sa)->sin_addr), s, maxlen);
        break;

    case AF_INET6:
        inet_ntop(AF_INET6, &(((struct sockaddr_in6 *)sa)->sin6_addr), s, maxlen);
        break;

    default:
        strncpy(s, "Unknown AF", maxlen);
        return NULL;
    }

    return s;
}

struct in6_addr ConcatRloc16Address(const in6_addr &aPrefix, uint16_t aRloc16)
{
    in6_addr addr = aPrefix;

    reinterpret_cast<uint16_t *>(&addr)[kRlocAddrUint16Offset] = htons(aRloc16);
    return addr;
}

struct in6_addr ConcatRloc16Address(const in6_addr &aPrefix, uint8_t aRouterID, uint16_t aChildID)
{
    return ConcatRloc16Address(aPrefix, ToRloc16(aRouterID, aChildID));
}

struct in6_addr FindRloc16Address(const std::vector<struct in6_addr> &aAddrs)
{
    struct in6_addr foundAddr;

    memset(&foundAddr, 0, sizeof(in6_addr));
    for (size_t i = 0; i < aAddrs.size(); i++)
    {
        const uint8_t *buf = reinterpret_cast<const uint8_t *>(&aAddrs[i]);
        if (!memcmp(&buf[kIidAddrUint8Offset], &kLociid[0], sizeof(kLociid)) &&
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
        if (buf[0] == kUlaPrefix && memcmp(&buf[kIidAddrUint8Offset], &kLociid[0], sizeof(kLociid)))
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

} // namespace BorderRouter
} // namespace ot
