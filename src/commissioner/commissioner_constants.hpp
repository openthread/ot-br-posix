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

#include "netinet/in.h"

namespace ot {
namespace BorderRouter {

/**
 * Constants
 */
enum
{
    kSizeMaxPacket = 1500, ///< max size of a network packet

    kPetitionAttemptDelay = 5, ///< delay between failed attempts to petition

    kPetitionMaxRetry = 2, ///< max retry for petition

    kSteeringDefaultLength = 15, ///< Default size of steering data

    kEui64Len = (64 / 8), ///< how long is an EUI64 in bytes

    kPSKcLength = 16, ///< how long is a PSKc in bytes
    kPSKdLength = 32, ///< how long is a PSKd in bytes

    kPortJoinerSession = 49192, ///< What port does our internal server use?

    kXPanIdLength = (64 / 8), ///< 64bit xpanid length in bytes

    kNetworkNameLenMax = 16, ///< specification: 8.10.4

    kBorderRouterPassPhraseLen = 64, ///< Spec is not specific about this items max length, so we choose 64

    kIPAddrNameBufSize = INET6_ADDRSTRLEN, ///< String buffer size for ip address

    kPortNameBufSize = 6, ///< String buffer size for port

    kMBedDebugDefaultThreshold = 4, ///< mbed debug print threshold

    kMbedDtlsHandshakeMinTimeout = 8000, ///< dtls handshake min timeout

    kMbedDtlsHandshakeMaxTimeout = 60000, ///< dtls handshake min timeout

    kKEKSize = 32, ///< key encrypted key(KEK) size
};

} // namespace BorderRouter
} // namespace ot

#endif // OTBR_COMMISSION_COMMON_H_
