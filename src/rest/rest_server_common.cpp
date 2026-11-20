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

/**
 * @brief   Implements APIs used for conversions of data from one form to another
 */
#include "rest_server_common.hpp"

#include <arpa/inet.h>

#include "common/code_utils.hpp"
#include "common/types.hpp"

namespace otbr {
namespace rest {

#include <ctype.h>
#include <openthread/logging.h>

// Function to combine Mesh Local Prefix and IID to form an IPv6 address
void combineMeshLocalPrefixAndIID(const otMeshLocalPrefix        *meshLocalPrefix,
                                  const otIp6InterfaceIdentifier *iid,
                                  otIp6Address                   *ip6Address)
{
    // Copy the Mesh Local Prefix to the first 8 bytes of the IPv6 address
    for (size_t i = 0; i < 8; ++i)
    {
        ip6Address->mFields.m8[i] = meshLocalPrefix->m8[i];
    }

    // Copy the IID to the last 8 bytes of the IPv6 address
    for (size_t i = 0; i < 8; ++i)
    {
        ip6Address->mFields.m8[i + 8] = iid->mFields.m8[i];
    }
}

static int hex_char_to_int(char c)
{
    if (('A' <= c) && (c <= 'F'))
    {
        return (uint8_t)c - (uint8_t)'A' + 10;
    }
    if (('a' <= c) && (c <= 'f'))
    {
        return (uint8_t)c - (uint8_t)'a' + 10;
    }
    if (('0' <= c) && (c <= '9'))
    {
        return (uint8_t)c - (uint8_t)'0';
    }
    return -1;
}

otError str_to_m8(uint8_t *m8, const char *str, uint8_t size)
{
    if (m8 == nullptr || str == nullptr || strlen(str) != size * 2)
    {
        return OT_ERROR_FAILED;
    }

    for (int i = 0; i < size; i++)
    {
        int hex_int_1 = hex_char_to_int(str[i * 2]);
        int hex_int_2 = hex_char_to_int(str[i * 2 + 1]);
        if (-1 == hex_int_1 || -1 == hex_int_2)
        {
            return OT_ERROR_FAILED;
        }
        m8[i] = (uint8_t)(hex_int_1 * 16 + hex_int_2);
    }

    return OT_ERROR_NONE;
}

otError str_to_rloc16(uint16_t &aRloc16, const char *aStr)
{
    uint8_t bytes[2];

    if (aStr == nullptr || strlen(aStr) != 6)
    {
        return OT_ERROR_FAILED;
    }

    if (aStr[0] != '0' || (aStr[1] != 'x' && aStr[1] != 'X' && aStr[1] != '0'))
    {
        return OT_ERROR_FAILED;
    }

    if (str_to_m8(bytes, aStr + 2, sizeof(bytes)) != OT_ERROR_NONE)
    {
        return OT_ERROR_FAILED;
    }

    aRloc16 = static_cast<uint16_t>((static_cast<uint16_t>(bytes[0]) << 8) | bytes[1]);
    return OT_ERROR_NONE;
}

static bool IsLocatorIid(const otIp6Address &aAddress)
{
    return (ntohs(aAddress.mFields.m16[4]) == 0x0000) && (ntohs(aAddress.mFields.m16[5]) == 0x00ff) &&
           (ntohs(aAddress.mFields.m16[6]) == 0xfe00);
}

otError ParseUnicastIp6Address(const char *aStr, otIp6Address &aAddress)
{
    otError    error    = OT_ERROR_PARSE;
    size_t     groupLen = 0;
    Ip6Address ip6;

    VerifyOrExit(aStr != nullptr);

    // Enforce RFC 4291 character set and 4-hex-digit group limit portably:
    // macOS/BSD `inet_pton(AF_INET6)` accepts `%<zone>` suffixes and
    // leading-zero groups longer than 4 hex digits (e.g., `fe80::00001`).
    for (const char *p = aStr; *p != '\0'; ++p)
    {
        if (*p == ':' || *p == '.')
        {
            groupLen = 0;
        }
        else if (hex_char_to_int(*p) >= 0)
        {
            groupLen++;
            VerifyOrExit(groupLen <= 4);
        }
        else
        {
            ExitNow();
        }
    }

    VerifyOrExit(Ip6Address::FromString(aStr, ip6) == OTBR_ERROR_NONE);

    // Reject multicast (ff00::/8), unspecified (::), loopback (::1), IPv4-mapped (::ffff:0:0/96),
    // and Subnet-Router Anycast (all-zero IID, RFC 4291 Section 2.6.1).
    VerifyOrExit(!ip6.IsMulticast());
    VerifyOrExit(!ip6.IsUnspecified());
    VerifyOrExit(!ip6.IsLoopback());
    VerifyOrExit(!ip6.IsIp4Mapped());
    VerifyOrExit(ip6.m64[1] != 0);

    ip6.CopyTo(aAddress);
    error = OT_ERROR_NONE;

exit:
    return error;
}

bool IsLinkLocalUnicast(const otIp6Address &aAddress)
{
    return (ntohs(aAddress.mFields.m16[0]) == 0xfe80) && (aAddress.mFields.m16[1] == 0) &&
           (aAddress.mFields.m16[2] == 0) && (aAddress.mFields.m16[3] == 0);
}

bool IsAnycastLocatorIid(const otIp6Address &aAddress)
{
    constexpr uint8_t kAloc16Mask = 0xfc;

    return IsLocatorIid(aAddress) && (aAddress.mFields.m8[14] == kAloc16Mask);
}

bool IsMeshLocalRloc(const otIp6Address &aAddress, const otMeshLocalPrefix &aMeshLocalPrefix, uint16_t &aRloc16)
{
    constexpr uint8_t kAloc16Mask            = 0xfc;
    constexpr uint8_t kRloc16ReservedBitMask = 0x02;

    bool isRloc = false;

    VerifyOrExit(
        memcmp(aAddress.mFields.mComponents.mNetworkPrefix.m8, aMeshLocalPrefix.m8, sizeof(otMeshLocalPrefix)) == 0);
    VerifyOrExit(IsLocatorIid(aAddress));
    VerifyOrExit((aAddress.mFields.m8[14] < kAloc16Mask) && ((aAddress.mFields.m8[14] & kRloc16ReservedBitMask) == 0));

    aRloc16 = ntohs(aAddress.mFields.m16[7]);
    isRloc  = true;

exit:
    return isRloc;
}

} // namespace rest
} // namespace otbr
