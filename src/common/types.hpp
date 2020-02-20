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

#ifndef TYPES_HPP_
#define TYPES_HPP_

#include <stdint.h>

#ifndef IN6ADDR_ANY
/**
 * Any IPv6 address literal.
 *
 */
#define IN6ADDR_ANY "::"
#endif

extern "C" {

/**
 * This enumeration represents error codes used throughout OpenThread Border Router.
 */
enum otbrErrorCode
{
    OTBR_ERROR_NONE       = 0,  ///< No error.
    OTBR_ERROR_ERRNO      = -1, ///< Error defined by errno.
    OTBR_ERROR_DTLS       = -2, ///< DTLS error.
    OTBR_ERROR_DBUS       = -3, ///< DBus error.
    OTBR_ERROR_MDNS       = -4, ///< MDNS error.
    OTBR_ERROR_OPENTHREAD = -5, ///< OpenThread error.
};

/**
 * This is the raw data for an otbrError, the top-level error code and the
 * third-party error code embedded
 */
struct otbrErrorVanilla
{
    otbrErrorCode mTopError;
    uint64_t      mEmbeddedError;
};
}

/**
 * This enumeration represents error codes used throughout OpenThread.
 *
 */
enum class otbrEmbedOtError
{
    /**
     * No error.
     */
    OT_ERROR_NONE = 0,

    /**
     * Operational failed.
     */
    OT_ERROR_FAILED = 1,

    /**
     * Message was dropped.
     */
    OT_ERROR_DROP = 2,

    /**
     * Insufficient buffers.
     */
    OT_ERROR_NO_BUFS = 3,

    /**
     * No route available.
     */
    OT_ERROR_NO_ROUTE = 4,

    /**
     * Service is busy and could not service the operation.
     */
    OT_ERROR_BUSY = 5,

    /**
     * Failed to parse message or arguments.
     */
    OT_ERROR_PARSE = 6,

    /**
     * Input arguments are invalid.
     */
    OT_ERROR_INVALID_ARGS = 7,

    /**
     * Security checks failed.
     */
    OT_ERROR_SECURITY = 8,

    /**
     * Address resolution requires an address query operation.
     */
    OT_ERROR_ADDRESS_QUERY = 9,

    /**
     * Address is not in the source match table.
     */
    OT_ERROR_NO_ADDRESS = 10,

    /**
     * Operation was aborted.
     */
    OT_ERROR_ABORT = 11,

    /**
     * Function or method is not implemented.
     */
    OT_ERROR_NOT_IMPLEMENTED = 12,

    /**
     * Cannot complete due to invalid state.
     */
    OT_ERROR_INVALID_STATE = 13,

    /**
     * No acknowledgment was received after macMaxFrameRetries (IEEE 802.15.4-2006).
     */
    OT_ERROR_NO_ACK = 14,

    /**
     * A transmission could not take place due to activity on the channel, i.e., the CSMA-CA mechanism has failed
     * (IEEE 802.15.4-2006).
     */
    OT_ERROR_CHANNEL_ACCESS_FAILURE = 15,

    /**
     * Not currently attached to a Thread Partition.
     */
    OT_ERROR_DETACHED = 16,

    /**
     * FCS check failure while receiving.
     */
    OT_ERROR_FCS = 17,

    /**
     * No frame received.
     */
    OT_ERROR_NO_FRAME_RECEIVED = 18,

    /**
     * Received a frame from an unknown neighbor.
     */
    OT_ERROR_UNKNOWN_NEIGHBOR = 19,

    /**
     * Received a frame from an invalid source address.
     */
    OT_ERROR_INVALID_SOURCE_ADDRESS = 20,

    /**
     * Received a frame filtered by the address filter (whitelisted or blacklisted).
     */
    OT_ERROR_ADDRESS_FILTERED = 21,

    /**
     * Received a frame filtered by the destination address check.
     */
    OT_ERROR_DESTINATION_ADDRESS_FILTERED = 22,

    /**
     * The requested item could not be found.
     */
    OT_ERROR_NOT_FOUND = 23,

    /**
     * The operation is already in progress.
     */
    OT_ERROR_ALREADY = 24,

    /**
     * The creation of IPv6 address failed.
     */
    OT_ERROR_IP6_ADDRESS_CREATION_FAILURE = 26,

    /**
     * Operation prevented by mode flags
     */
    OT_ERROR_NOT_CAPABLE = 27,

    /**
     * Coap response or acknowledgment or DNS, SNTP response not received.
     */
    OT_ERROR_RESPONSE_TIMEOUT = 28,

    /**
     * Received a duplicated frame.
     */
    OT_ERROR_DUPLICATED = 29,

    /**
     * Message is being dropped from reassembly list due to timeout.
     */
    OT_ERROR_REASSEMBLY_TIMEOUT = 30,

    /**
     * Message is not a TMF Message.
     */
    OT_ERROR_NOT_TMF = 31,

    /**
     * Received a non-lowpan data frame.
     */
    OT_ERROR_NOT_LOWPAN_DATA_FRAME = 32,

    /**
     * The link margin was too low.
     */
    OT_ERROR_LINK_MARGIN_LOW = 34,

    /**
     * The number of defined errors.
     */
    OT_NUM_ERRORS,

    /**
     * Generic error (should not use).
     */
    OT_ERROR_GENERIC = 255,
};

class otbrError
{
public:
    otbrError()
        : mError{OTBR_ERROR_NONE, 0}
    {
    }

    otbrError(otbrErrorCode aError)
        : mError{aError, 0}
    {
    }

    otbrError(otbrEmbedOtError aOtError)
        : mError{OTBR_ERROR_OPENTHREAD, static_cast<uint64_t>(aOtError)}
    {
    }

    operator otbrErrorCode() { return mError.mTopError; }
    operator otbrEmbedOtError() { return static_cast<otbrEmbedOtError>(mError.mEmbeddedError); }

private:
    otbrErrorVanilla mError;
};

namespace ot {

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

} // namespace ot

#endif // TYPES_HPP_
