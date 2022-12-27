/*
 *  Copyright (c) 2017, The OpenThread Authors.
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
 * @file
 *   This file provides kinds of convertion functions.
 */

#ifndef OTBR_UTILS_HEX_HPP_
#define OTBR_UTILS_HEX_HPP_

#include "openthread-br/config.h"

#include <string>

#include <stddef.h>
#include <stdint.h>

namespace otbr {

namespace Utils {

/**
 * @brief Converts a hexadecimal string to a byte array.
 *
 * @param hexString A pointer to the hexadecimal string to be converted.
 * @param bytes A pointer to an array to store the resulting byte values.
 * @param maxBytesLength The maximum number of bytes that can be stored in the `bytes` array.
 *
 * @return The number of bytes stored in the `bytes` array, or -1 if an error occurred.
 */
int Hex2Bytes(const char *aHex, uint8_t *aBytes, uint16_t aBytesLength);

/**
 * @brief Converts a byte array to a hexadecimal string.
 *
 * @param[in]  aBytes A pointer to the byte array to be converted.
 * @param[in]  aBytesLength The length of the byte array.
 * @param[out] aHex A character array to store the resulting hexadecimal string.
 *                  Must be at least 2 * @param aBytesLength + 1 long.
 *
 * @return The length of the resulting hexadecimal string.
 */
size_t Bytes2Hex(const uint8_t *aBytes, const uint16_t aBytesLength, char *aHex);

/**
 * @brief Converts a byte array to a hexadecimal string.
 *
 * @param[in]  aBytes A pointer to the byte array to be converted.
 * @param[in]  aBytesLength The length of the byte array.
 *
 * @return The hexadecimal string.
 */
std::string Bytes2Hex(const uint8_t *aBytes, const uint16_t aBytesLength);

/**
 * @brief Converts a 64-bit integer to a hexadecimal string.
 *
 * @param[in]  aLong The 64-bit integer to be converted.
 * @param[out] aHex A character array to store the resulting hexadecimal string.
 *                  Must be at least 17 bytes long.
 *
 * @return The length of the resulting hexadecimal string.
 */
size_t Long2Hex(const uint64_t aLong, char *aHex);

} // namespace Utils

} // namespace otbr

#endif // OTBR_UTILS_HEX_
