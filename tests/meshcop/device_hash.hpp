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
 *   The file is the header for the hash functions for joiner and commissioner device
 */

#ifndef OTBR_DEVICE_HASH_HPP_
#define OTBR_DEVICE_HASH_HPP_

#include <stdint.h>

#include "utils/steering_data.hpp"

namespace ot {
namespace BorderRouter {

/**
 * This method computes pskc from network parameter
 *
 * @param[in]    aExtPanIdBin      extended pan id in binary form
 * @param[in]    aNetworkName      network name in string form
 * @param[in]    aPassphrase       commissioner passphrase in string form
 * @param[out]   aPskcOutBuf       output buffer for pskc
 *
 */
void ComputePskc(const uint8_t *aExtPanIdBin, const char *aNetworkName, const char *aPassphrase, uint8_t *aPskcOutBuf);

/**
 * This method computes joiner hash mac from its eui64
 *
 * @param[in]    aEui64Bin          eui64 of joiner in binary form
 * @param[out]   aHashMacOutBuf     output buffer for hash mac
 *
 */
void ComputeHashMac(uint8_t *aEui64Bin, uint8_t *aHashMacOutBuf);

/**
 * This method computes steering data to filter joiner
 *
 * @param[in]    aLength        steering data length
 * @param[in]    aAllowAny      whether to allow any joiner to join, if set to true, aEui64Bin will bo of no use
 * @param[in]    aEui64Bin      eui64 of the joiner we want to commission
 *
 * @returns the steering data
 *
 */
SteeringData ComputeSteeringData(uint8_t aLength, bool aAllowAny, uint8_t *aEui64Bin);

} // namespace BorderRouter
} // namespace ot

#endif // OTBR_DEVICE_HASH_HPP_
