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
 *
 */
#include "rest_server_common.hpp"

#ifdef __cplusplus
extern "C" {
#endif

#include <cJSON.h>
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

// count number of 1s in bitmask
int my_count_ones(uint32_t bitmask)
{
    int count = 0;
    while (bitmask)
    {
        count += bitmask & 1; // Increment count if the least significant bit is 1
        bitmask >>= 1;        // Shift bitmask to the right by 1
    }
    return count;
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

uint8_t joiner_verify_pskd(char *pskd)
{
    int len = strlen(pskd);
    if (OT_PSKD_LENGTH_MIN > len)
    {
        otLogWarnPlat("PSKd %s has incorrect length %d", pskd, len);
        return OT_JOINFAILED_LENGTH;
    }
    if (OT_PSKD_LENGTH_MAX < len)
    {
        otLogWarnPlat("PSKd %s has incorrect length %d", pskd, len);
        return OT_JOINFAILED_LENGTH;
    }
    for (int i = 0; i < len; i++)
    {
        if (!isalnum(pskd[i]))
        {
            otLogWarnPlat("PSKd %s has incorrect format and is not alphanumeric", pskd);
            return OT_JOINFAILED_PSKD_FORMAT;
        }
        if (islower(pskd[i]))
        {
            otLogWarnPlat("PSKd %s has incorrect format and is not all uppercase", pskd);
            return OT_JOINFAILED_PSKD_FORMAT;
        }
        if ('I' == pskd[i] || 'O' == pskd[i] || 'Q' == pskd[i] || 'Z' == pskd[i])
        {
            otLogWarnPlat("PSKd %s has incorrect format and contains illegal character %c", pskd, pskd[i]);
            return OT_JOINFAILED_PSKD_FORMAT;
        }
    }
    return WPANSTATUS_OK;
}

otError str_to_m8(uint8_t *m8, const char *str, uint8_t size)
{
    if (size * 2 > strlen(str))
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

bool is_hex_string(char *str)
{
    int offset = 0;
    if ('x' == str[1])
    {
        if ('0' != str[0])
        {
            return false;
        }
        offset = 2;
    }
    for (size_t i = offset; i < strlen(str); i++)
    {
        if (!isxdigit(str[i]))
        {
            return false;
        }
    }
    return true;
}

#ifdef __cplusplus
}
#endif
