/*
 *    Copyright (c) 2024, The OpenThread Authors.
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
 *   This file include definition of Icmp Ping Sender class to send ICMPv4
 *   echo request and handling ICMPv4 echo reply.
 */

#ifndef UTILS_ICMP_PING_HPP_
#define UTILS_ICMP_PING_HPP_

#include <netinet/in.h>

namespace otbr {
namespace Utils {

class IcmpPing
{
public:
    /**
     * @brief Constructor for IcmpPing class.
     * @param aTarget The target IP address or hostname to ping.
     */
    IcmpPing(const char *aTarget);

    /**
     * @brief Destructor for IcmpPing class.
     */
    ~IcmpPing();

    /**
     * @brief Sends an ICMP ping request to the target.
     * @return True if the ping request was sent successfully, false otherwise.
     */
    bool Send();

    /**
     * @brief Waits for a response to the ICMP ping request.
     * @param aTimeout The timeout value in milliseconds.
     * @return True if a response was received within the timeout, false otherwise.
     */
    bool WaitForResponse(int aTimeout);

    /**
     * @brief Receives the ICMP ping response.
     * @return True if the response was received successfully, false otherwise.
     */
    bool Receive();

private:
    int   mSockFd;
    pid_t mPid;

    // target IPv4 address
    struct in_addr mTargetAddr;

    // buffer to receive ICMP response
    char mRecvBuf[64];
};

} // namespace Utils
} // namespace otbr

#endif // UTILS_ICMP_PING_HPP_
