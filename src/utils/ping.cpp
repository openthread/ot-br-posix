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
 *   Util to send ICMP(v4) Echo Request and receive Echo Reply to/from infra interface
 */

#include <arpa/inet.h> // For inet_pton, inet_ntoa
#include <iostream>
#include <netinet/ip_icmp.h> // For ICMP header
#include <unistd.h>

#include "ping.hpp"
#include "common/code_utils.hpp"
#include "utils/internet_checksum.hpp"

namespace otbr {
namespace Utils {

IcmpPing::IcmpPing(const char *aTarget)
{
    // Create a Ipv4 RAW socket for ICMP protocol communication.
    mSockFd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    VerifyOrDie(mSockFd >= 0, "IcmpPing socket");

    inet_pton(AF_INET, aTarget, &mTargetAddr);
    VerifyOrDie(mTargetAddr.s_addr != INADDR_NONE, "IcmpPing inet_pton");

    mPid = getpid();
    VerifyOrDie(mPid > 0, "IcmpPing getpid");
}

IcmpPing::~IcmpPing()
{
    close(mSockFd);
}

bool IcmpPing::Send()
{
    bool ret = true;

    // Construct the ICMP Echo Request packet.
    struct icmphdr icmp;
    icmp.type             = ICMP_ECHO;
    icmp.code             = 0;
    icmp.checksum         = 0; // fill later
    icmp.un.echo.id       = mPid;
    icmp.un.echo.sequence = 0;

    // Calculate the checksum of the ICMP packet.
    icmp.checksum = CalculateInternetChecksum((uint16_t *)&icmp, sizeof(icmp));

    // Set target address
    struct sockaddr_in dst;
    dst.sin_family = AF_INET;
    dst.sin_port   = 0; // port is not used for ICMP
    dst.sin_addr   = mTargetAddr;

    // Send the ICMP Echo Request.
    ssize_t sent = sendto(mSockFd,                 // socket file descriptor
                          &icmp,                   // buffer containing the ICMP packet
                          sizeof(icmp),            // size of the ICMP packet
                          0,                       // flags
                          (struct sockaddr *)&dst, // destination address
                          sizeof(dst)              // size of the destination address
    );

    VerifyOrExit(sent > 0, ret = false);

exit:
    return ret;
}

bool IcmpPing::WaitForResponse(int aTimeoutSec)
{
    bool ret = true;
    int  val;

    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(mSockFd, &readfds);

    struct timeval timeout = {aTimeoutSec, 0};
    val                    = select(mSockFd + 1, &readfds, NULL, NULL, &timeout);

    VerifyOrExit(val > 0 && FD_ISSET(mSockFd, &readfds), ret = false);

exit:
    return ret;
}

bool IcmpPing::Receive()
{
    bool               ret = true;
    struct iphdr      *ip;
    struct icmphdr    *icmp;
    struct sockaddr_in src;
    socklen_t          addr_len = sizeof(src);

    memset(mRecvBuf, 0, sizeof(mRecvBuf));

    ssize_t len = recvfrom(mSockFd,                 // socket file descriptor
                           mRecvBuf,                // buffer to store the received packet
                           sizeof(mRecvBuf),        // size of the buffer
                           0,                       // flags
                           (struct sockaddr *)&src, // source address
                           &addr_len                // size of the source address
    );

    VerifyOrExit(len > 0, ret = false);

    ip   = (struct iphdr *)mRecvBuf;
    icmp = (struct icmphdr *)(mRecvBuf + (ip->ihl << 2));

    // print mRecvBuf
    for (int i = 0; i < len; i++)
    {
        std::cout << std::hex << (int)mRecvBuf[i] << " ";
    }

    std::cout << "icmp->type=" << std::dec << (int)icmp->type << std::endl;

    VerifyOrExit(icmp->type == ICMP_ECHOREPLY, ret = false);

    std::cout << "Received ICMP Echo Reply from " << inet_ntoa(src.sin_addr) << ", seq=" << (int)icmp->un.echo.sequence
              << ", ttl=" << std::dec << (int)ip->ttl << std::endl;

exit:
    return ret;
}

} // namespace Utils
} // namespace otbr
