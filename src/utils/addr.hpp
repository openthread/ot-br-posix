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
 *   The file is the header for the address manipulation utilities for the commissioner test app.
 */

#ifndef OTBR_ADDR_UTILS_HPP_
#define OTBR_ADDR_UTILS_HPP_

#include <sys/socket.h>

namespace ot {
namespace Utils {

/**
 * This function computes rloc16 given router id and child id
 *
 * @param[in]    aRouterId     router id of thread node
 * @param[in]    aChildId      child id of thread node, for router itself use zero
 *
 * @returns rloc16 of node
 *
 */
uint16_t ToRloc16(uint8_t aRouterId, uint16_t aChildId);

/**
 * This function prints sockaddr struct to a string buffer
 *
 * @param[in]    aAddr         address to print
 * @param[out]   aOutBuf       output buffer
 * @param[in]    aLength       length of output buffer
 *
 * @returns same pointer as aOutBuf with string serialized in it
 *
 */
char *GetIPString(const struct sockaddr *aAddr, char *aOutBuf, size_t aLength);

/**
 * This function concats rloc16 and mle prefix to an ipv6 address
 *
 * @param[in]    aPrefix       thread network mle prefix
 * @param[out]   aRloc16       rloc16 of thread node
 *
 * @returns the ipv6 routing locator address of node
 *
 */
struct in6_addr ConcatRloc16Address(const in6_addr &aPrefix, uint16_t aRloc16);

/**
 * This function concats router id, child id and mle prefix to an ipv6 address
 *
 * @param[in]    aPrefix       thread network mle prefix
 * @param[in]    routerID      router id of thread node
 * @param[in]    childID       child id of thread node, for router itself use zero
 *
 * @returns the ipv6 routing locator address of node
 *
 */
struct in6_addr ConcatRloc16Address(const in6_addr &aPrefix, uint8_t aRouterId, uint16_t aChildId);

/**
 * This function finds RLOC ipv6 address from address list of a thread node
 *
 * @param[in]    aAddrs        address list of a thread node
 *
 * @returns the ipv6 routing locator address of node
 *
 */
struct in6_addr FindRloc16Address(const std::vector<struct in6_addr> &aAddrs);

/**
 * This function finds ML-EID ipv6 address from address list of a thread node
 *
 * @param[in]    aAddrs        address list of a thread node
 *
 * @returns the ML-EID ipv6 address of node
 *
 */
struct in6_addr FindMLEIDAddress(const std::vector<struct in6_addr> &aAddrs);

/**
 * This function finds mesh prefix for rloc16 from address list of a thread node
 *
 * @param[in]    aAddrs        address list of a thread node
 *
 * @returns mesh prefix for rloc16
 *
 */
struct in6_addr GetRlocPrefix(const std::vector<struct in6_addr> &aAddrs);

/**
 * This function strips mesh prefix for rloc16 from routing locator address
 *
 * @param[in]    aRlocAddr      routing locator address
 *
 * @returns mesh prefix for rloc16
 *
 */
struct in6_addr ToRlocPrefix(const struct in6_addr &aRlocAddr);

} // namespace Utils
} // namespace ot

#endif // OTBR_ADDR_UTILS_HPP_
