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
 * @brief   Implements APIs used for conversions of data from one form to another.
 *
 */
#ifndef REST_SERVER_COMMON_HPP_
#define REST_SERVER_COMMON_HPP_

#include "utils/thread_helper.hpp"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <openthread/error.h>

typedef enum
{
    LOCK_TYPE_BLOCKING,
    LOCK_TYPE_NONBLOCKING,
    LOCK_TYPE_TIMED
} LockType;

#define WPANSTATUS_OK 0
#define OT_NETWORKKEY_LENGTH 32
#define OT_PSKD_LENGTH_MIN 6
#define OT_PSKD_LENGTH_MAX 32
#define OT_JOINFAILED_LENGTH 16
#define OT_JOINFAILED_PSKD_FORMAT 17

// Function to combine Mesh Local Prefix and IID to form an IPv6 address
void combineMeshLocalPrefixAndIID(const otMeshLocalPrefix        *meshLocalPrefix,
                                  const otIp6InterfaceIdentifier *iid,
                                  otIp6Address                   *ip6Address);

// count number of 1s in bitmask
int my_count_ones(uint32_t bitmask);

uint8_t joiner_verify_pskd(char *pskd);

/**
 * @brief   str_to_m8, is designed to convert a string of hexadecimal characters
 *          into an array of bytes (uint8_t). It performs this conversion by processing
 *          each pair of hexadecimal characters in the input string, converting them
 *          into their corresponding byte value, and storing the result in the provided array.
 * @param   uint8_t *m8: A pointer to the array where the converted bytes will be stored.
 * @param   const char *str: A pointer to the input string containing hexadecimal characters.
 * @param   uint8_t size: The number of bytes that the m8 array can hold, which dictates how many characters from str
 * should be processed.
 * @return    The function returns an otError code, indicating the success or failure of the conversion process.
 */
otError str_to_m8(uint8_t *m8, const char *str, uint8_t size);

/**
 * @brief [DEPRECATED] please use isValidPerRegex() for new work.
 * This function checks if the input string is hex or not
 * @param str Hex string to be checked
 * @return true if the string is HEX
 * @return false if the string is not HEX
 */
bool is_hex_string(char *str);

#ifdef __cplusplus
} // end of extern "C"
#endif

#endif
