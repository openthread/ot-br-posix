/*
 *  Copyright (c) 2020, The OpenThread Authors.
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

#define OTBR_LOG_TAG "REST"

#include "rest/rest_web_server.hpp"

#include <arpa/inet.h>
#include <cerrno>

#include <fcntl.h>

#include "utils/socket_utils.hpp"

using std::chrono::duration_cast;
using std::chrono::microseconds;
using std::chrono::steady_clock;

namespace otbr {
namespace rest {

RestWebServer::RestWebServer(RcpHost &aHost, const std::string &aRestListenAddress, int aRestListenPort)
    : mResource(mServer, &aHost)
{
    mServer.listen(aRestListenAddress, aRestListenPort);
}

RestWebServer::~RestWebServer(void)
{
}

void RestWebServer::Init(void)
{
    mResource.Init();
}

bool RestWebServer::ParseListenAddress(const std::string listenAddress, struct in6_addr *sin6_addr)
{
    const std::string ipv4_prefix       = "::FFFF:";
    const std::string ipv4ListenAddress = ipv4_prefix + listenAddress;

    if (inet_pton(AF_INET6, listenAddress.c_str(), sin6_addr) == 1)
    {
        return true;
    }

    if (inet_pton(AF_INET6, ipv4ListenAddress.c_str(), sin6_addr) == 1)
    {
        return true;
    }

    return false;
}

} // namespace rest
} // namespace otbr
