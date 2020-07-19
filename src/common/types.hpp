/*
 *    Copyright (c) 2017, The OpenThread Authors.
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
 *   This file includes definition for data types used by Thread border agent.
 */

#ifndef OTBR_COMMON_TYPES_HPP_
#define OTBR_COMMON_TYPES_HPP_

#include "openthread-br/config.h"

#include <stdint.h>
#include <string>
#include <vector>

#ifndef IN6ADDR_ANY
/**
 * Any IPv6 address literal.
 *
 */
#define IN6ADDR_ANY "::"
#endif

#define OTBR_IP6_ADDRESS_SIZE 16
#define OTBR_IP6_PREFIX_SIZE 8
#define OTBR_MASTER_KEY_SIZE 16
#define OTBR_PSKC_SIZE 16

/**
 * This enumeration represents error codes used throughout OpenThread Border Router.
 */
enum otbrError
{
    OTBR_ERROR_NONE = 0, ///< No error.

    OTBR_ERROR_ERRNO      = -1, ///< Error defined by errno.
    OTBR_ERROR_DBUS       = -2, ///< DBus error.
    OTBR_ERROR_MDNS       = -3, ///< MDNS error.
    OTBR_ERROR_OPENTHREAD = -4, ///< OpenThread error.
    OTBR_ERROR_REST       = -5  ///< Rest Server error.
};

namespace otbr {

enum
{
    kSizePSKc        = 16, ///< Size of PSKc.
    kSizeNetworkName = 16, ///< Max size of Network Name.
    kSizeExtPanId    = 8,  ///< Size of Extended PAN ID.
    kSizeEui64       = 8,  ///< Size of Eui64.
};

/**
 * This class implements the Ipv6 address functionality.
 *
 */
class Ip6Address
{
public:
    /**
     * Default constructor.
     *
     */
    Ip6Address(void)
    {
        m64[0] = 0;
        m64[1] = 0;
    }

    /**
     * Constructor with an 16-bit Thread locator.
     *
     * @param[in]   aLocator    16-bit Thread locator, RLOC or ALOC.
     *
     */
    Ip6Address(uint16_t aLocator)
    {
        m64[0] = 0;
        m32[2] = 0;
        m16[6] = 0;
        m8[14] = aLocator >> 8;
        m8[15] = aLocator & 0xff;
    }

    /**
     * Retrieve the 16-bit Thread locator.
     *
     * @returns RLOC16 or ALOC16.
     *
     */
    uint16_t ToLocator(void) const { return static_cast<uint16_t>(m8[14] << 8 | m8[15]); }

    union
    {
        uint8_t  m8[16];
        uint16_t m16[8];
        uint32_t m32[4];
        uint64_t m64[2];
    };
};

} // namespace otbr

#endif // OTBR_COMMON_TYPES_HPP_
