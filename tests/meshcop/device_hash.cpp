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
 *   The file implements the hash functions for joiner and commissioner device
 */

#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif
#include <mbedtls/certs.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/debug.h>
#include <mbedtls/ecjpake.h>
#include <mbedtls/entropy.h>
#include <mbedtls/error.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/ssl.h>
#include <mbedtls/timing.h>

#include "commissioner_constants.hpp"
#include "device_hash.hpp"
#include "web/pskc-generator/pskc.hpp"

namespace ot {
namespace BorderRouter {

void ComputePskc(const uint8_t *aExtPanIdBin, const char *aNetworkName, const char *aPassphrase, uint8_t *aPskcOutBuf)
{
    ot::Psk::Pskc  pskc;
    const uint8_t *pskcBin = pskc.ComputePskc(aExtPanIdBin, aNetworkName, aPassphrase);

    memcpy(aPskcOutBuf, pskcBin, OT_PSKC_LENGTH);
}

void ComputeHashMac(uint8_t *aEui64Bin, uint8_t *aHashMacOutBuf)
{
    mbedtls_sha256_context sha256;
    uint8_t                hash_result[32];

    mbedtls_sha256_init(&sha256);
    mbedtls_sha256_starts(&sha256, 0);
    mbedtls_sha256_update(&sha256, aEui64Bin, kEui64Len);
    mbedtls_sha256_finish(&sha256, hash_result);
    /* Bytes 0..7, is the new data */
    memcpy(aHashMacOutBuf, hash_result, 8);
    /* Set the locally admin bit, byte 0, bit 1 */
    aHashMacOutBuf[0] |= 2;
}

SteeringData ComputeSteeringData(uint8_t aLength, bool aAllowAny, uint8_t *aEui64Bin)
{
    SteeringData data;

    data.Init(aLength);
    if (aAllowAny)
    {
        data.Set();
    }
    else
    {
        uint8_t hashMacBin[kEui64Len];

        ComputeHashMac(aEui64Bin, hashMacBin);
        data.ComputeBloomFilter(hashMacBin);
    }
    return data;
}

} // namespace BorderRouter
} // namespace ot
