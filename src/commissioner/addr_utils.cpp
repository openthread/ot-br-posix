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
 *   The file is the implementation of the address manipulation utilities for the commissioner test app.
 */

#include "addr_utils.hpp"

#include <arpa/inet.h>

#include "utils/strcpy_utils.hpp"

namespace ot {
namespace BorderRouter {

char *GetIPString(const struct sockaddr *aAddr, char *aOutBuf, size_t aLength)
{
    switch (aAddr->sa_family)
    {
    case AF_INET:
        inet_ntop(AF_INET, &((reinterpret_cast<const struct sockaddr_in *>(aAddr))->sin_addr), aOutBuf,
                  static_cast<socklen_t>(aLength));
        break;

    case AF_INET6:
        inet_ntop(AF_INET6, &((reinterpret_cast<const struct sockaddr_in6 *>(aAddr))->sin6_addr), aOutBuf,
                  static_cast<socklen_t>(aLength));
        break;

    default:
        strcpy_safe(aOutBuf, aLength, "Unknown AF");
        return NULL;
    }

    return aOutBuf;
}

} // namespace BorderRouter
} // namespace ot
