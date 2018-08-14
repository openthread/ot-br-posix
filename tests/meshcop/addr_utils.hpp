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

#ifndef OTBR_ADDR_UTILS_H_
#define OTBR_ADDR_UTILS_H_

#include <arpa/inet.h>
#include <errno.h>
#include <sys/socket.h>
#include <vector>

namespace ot {
namespace BorderRouter {

uint16_t toRloc16(uint8_t routerID, uint16_t childID);

char *get_ip_str(const struct sockaddr *sa, char *s, size_t maxlen);

struct in6_addr ConcatRloc16Address(const in6_addr &aPrefix, uint16_t aRloc16);

struct in6_addr ConcatRloc16Address(const in6_addr &aPrefix, uint8_t aRouterID, uint16_t aChildID);

struct in6_addr FindRloc16Address(const std::vector<struct in6_addr> &aAddrs);

struct in6_addr FindMLEIDAddress(const std::vector<struct in6_addr> &aAddrs);

struct in6_addr GetRlocPrefix(const std::vector<struct in6_addr> &aAddrs);

struct in6_addr ToRlocPrefix(const struct in6_addr &aRlocAddr);

} // namespace BorderRouter
} // namespace ot

#endif
