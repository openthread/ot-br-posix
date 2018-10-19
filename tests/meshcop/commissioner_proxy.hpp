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
 *   The file is the header of commissioner proxy class
 */

#ifndef OTBR_COMMISSIONER_PROXY_HPP_
#define OTBR_COMMISSIONER_PROXY_HPP_

#include <arpa/inet.h>
#include <set>
#include <sys/socket.h>
#include <sys/types.h>
#include <vector>
#include "agent/coap.hpp"

namespace ot {
namespace BorderRouter {

class CommissionerProxy
{
public:
    /**
     * The constructor to initialize Commissioner proxy
     *
     * @param[in]  aProxyPort       Proxy port
     *
     */
    CommissionerProxy(int aProxyPort);

    /**
     * This method send data through commission proxy
     *
     * @param[in]   aDestAddr       Destination address to send data to
     * @param[in]   aBuf            Buffer of data
     * @param[in]   aLength         length to send
     *
     * @returns upon success returns number of bytes sent, upon failure a negative value
     */
    int SendTo(const struct sockaddr_in6 &aDestAddr, const void *aBuf, size_t aLength);

    /**
     * This method receives data from commission proxy
     *
     * @param[out]  aBuf            Buffer of data
     * @param[in]   aLength         Length of buffer
     * @param[out]  aSrcAddr        source address of packet
     *
     * @returns upon success returns number of bytes read, upon failure a negative value
     */
    int RecvFrom(void *aBuf, size_t aLength, struct sockaddr_in6 &aSrcAddr);

    ~CommissionerProxy();

private:
    CommissionerProxy(const CommissionerProxy &);
    CommissionerProxy &operator=(const CommissionerProxy &);

    int      mClientFd;
    uint16_t mProxyPort;
};

} // namespace BorderRouter
} // namespace ot

#endif // OTBR_COMMISSIONER_PROXY_HPP_
