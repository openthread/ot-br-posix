/*
 *    Copyright (c) 2020, The OpenThread Authors.
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

#include <arpa/inet.h>
#include <assert.h>
#include <sstream>
#include <stdlib.h>
#include <sys/socket.h>

#include "common/byteswap.hpp"
#include "common/code_utils.hpp"
#include "common/logging.hpp"
#include "common/types.hpp"

namespace otbr {

std::string BytesToHexString(const uint8_t *aBytes, uint16_t aLength)
{
    std::string str;
    char        hexStr[3];

    str.reserve(aLength * 2);

    for (uint16_t i = 0; i < aLength; i++)
    {
        snprintf(hexStr, sizeof(hexStr), "%02x", aBytes[i]);
        str.append(hexStr);
    }

    return str;
}

Ip6Address::Ip6Address(const uint8_t (&aAddress)[16])
{
    memcpy(m8, aAddress, sizeof(m8));
}

std::string Ip6Address::ToString() const
{
    char strbuf[INET6_ADDRSTRLEN];

    VerifyOrDie(inet_ntop(AF_INET6, this->m8, strbuf, sizeof(strbuf)) != nullptr,
                "Failed to convert Ip6 address to string");

    return std::string(strbuf);
}

Ip6Address Ip6Address::ToSolicitedNodeMulticastAddress(void) const
{
    Ip6Address ma(Ip6Address::GetSolicitedMulticastAddressPrefix());

    ma.m8[13] = m8[13];
    ma.m8[14] = m8[14];
    ma.m8[15] = m8[15];

    return ma;
}

void Ip6Address::CopyTo(struct sockaddr_in6 &aSockAddr) const
{
    memset(&aSockAddr, 0, sizeof(aSockAddr));
    CopyTo(aSockAddr.sin6_addr);
    aSockAddr.sin6_family = AF_INET6;
}

void Ip6Address::CopyFrom(const struct sockaddr_in6 &aSockAddr)
{
    CopyFrom(aSockAddr.sin6_addr);
}

void Ip6Address::CopyTo(struct in6_addr &aIn6Addr) const
{
    static_assert(sizeof(m8) == sizeof(aIn6Addr.s6_addr), "invalid IPv6 address size");
    memcpy(aIn6Addr.s6_addr, m8, sizeof(aIn6Addr.s6_addr));
}

void Ip6Address::CopyFrom(const struct in6_addr &aIn6Addr)
{
    static_assert(sizeof(m8) == sizeof(aIn6Addr.s6_addr), "invalid IPv6 address size");
    memcpy(m8, aIn6Addr.s6_addr, sizeof(aIn6Addr.s6_addr));
}

otbrError Ip6Address::FromString(const char *aStr, Ip6Address &aAddr)
{
    int ret;

    ret = inet_pton(AF_INET6, aStr, &aAddr.m8);

    return ret == 1 ? OTBR_ERROR_NONE : OTBR_ERROR_INVALID_ARGS;
}

Ip6Address Ip6Address::FromString(const char *aStr)
{
    Ip6Address addr;

    SuccessOrDie(FromString(aStr, addr), "inet_pton failed");

    return addr;
}

void Ip6Prefix::Set(const otIp6Prefix &aPrefix)
{
    memcpy(reinterpret_cast<void *>(this), &aPrefix, sizeof(*this));
}

std::string Ip6Prefix::ToString() const
{
    std::string    strPrefix = mPrefix.ToString();
    std::stringbuf strBuilder;
    char           strbuf[5];

    strBuilder.sputn(strPrefix.c_str(), strPrefix.length());
    strBuilder.sputc('/');

    snprintf(strbuf, sizeof(strbuf), "%d", mLength);
    strBuilder.sputn(strbuf, strlen(strbuf));

    return strBuilder.str();
}

Ip6NetworkPrefix::Ip6NetworkPrefix(const uint8_t *aPrefix, uint8_t aLength)
{
    OTBR_UNUSED_VARIABLE(aLength);
    assert(aLength == sizeof(m8));
    memcpy(m8, aPrefix, sizeof(m8));
}

std::string Ip6NetworkPrefix::ToString() const
{
    char            strbuf[INET6_ADDRSTRLEN];
    struct in6_addr addr = {{{0}}};

    memcpy(addr.s6_addr, m8, sizeof(Ip6NetworkPrefix));
    inet_ntop(AF_INET6, &addr, strbuf, INET6_ADDRSTRLEN);

    return std::string(strbuf);
}
} // namespace otbr
