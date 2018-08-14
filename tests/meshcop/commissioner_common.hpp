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
 *   The file is the header for the command line params for the commissioner test app.
 */

#ifndef OTBR_COMMISSION_COMMON_H_
#define OTBR_COMMISSION_COMMON_H_

namespace ot {
namespace BorderRouter {

/**
 * Constants
 */
enum
{
    /* max size of a network packet */
    kSizeMaxPacket = 1500,

    /* delay between failed attempts to petition */
    kPetitionAttemptDelay = 5,

    /* max retry for petition */
    kPetitionMaxRetry = 2,

    /* Default size of steering data */
    kSteeringDefaultLength = 15,

    /* how long is an EUI64 in bytes */
    kEui64Len = (64 / 8),

    /* how long is a PSKd in bytes */
    kPSKdLength = 32,

    /* What port does our internal server use? */
    kPortJoinerSession = 49192,

    /* 64bit xpanid length in bytes */
    kXpanidLength = (64 / 8), /* 64bits */

    /* specification: 8.10.4 */
    kNetworkNameLenMax = 16,

    /* Spec is not specific about this items max length, so we choose 64 */
    kBorderRouterPassPhraseLen = 64,

    /* String buffer size for ip address */
    kIPAddrNameBufSize = 100,

    /* String buffer size for port */
    kPortNameBufSize = 6,

    /* mbed debug print threshold */
    kMBedDebugDefaultThreshold = 4,

    /* dtls handshake min timeout */
    kMbedDtlsHandshakeMinTimeout = 8000,

    /* dtls handshake min timeout */
    kMbedDtlsHandshakeMaxTimeout = 60000,

    /* key encrypted key(KEK) size */
    kKEKSize = 32,
};

} // namespace BorderRouter
} // namespace ot

#endif // OTBR_COMMISSION_COMMON_H_
