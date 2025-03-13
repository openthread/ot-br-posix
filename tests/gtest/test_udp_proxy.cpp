/*
 *    Copyright (c) 2025, The OpenThread Authors.
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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include "common/mainloop_manager.hpp"
#include "common/types.hpp"
#include "host/posix/udp_proxy.hpp"

static constexpr size_t   kMaxUdpSize       = 1280;
static constexpr uint16_t kTestThreadBaPort = 49191;
const std::string         kHello            = "Hello UdpProxy!";

class UdpProxyTest : public otbr::UdpProxy::Dependencies
{
public:
    UdpProxyTest(void)
        : mForwarded(false)
    {
    }

    otbrError UdpForward(const uint8_t        *aUdpPayload,
                         uint16_t              aLength,
                         const otIp6Address   &aRemoteAddr,
                         uint16_t              aRemotePort,
                         const otbr::UdpProxy &aUdpProxy) override
    {
        mForwarded = true;
        assert(aLength < kMaxUdpSize);

        memcpy(mPayload, aUdpPayload, aLength);
        mLength = aLength;
        memcpy(mRemoteAddress.mFields.m8, aRemoteAddr.mFields.m8, sizeof(mRemoteAddress));
        mRemotePort = aRemotePort;
        mLocalPort  = aUdpProxy.GetThreadPort();

        return OTBR_ERROR_NONE;
    }

    bool         mForwarded;
    uint8_t      mPayload[kMaxUdpSize];
    uint16_t     mLength;
    otIp6Address mRemoteAddress;
    uint16_t     mRemotePort;
    uint16_t     mLocalPort;
};

TEST(UdpProxy, UdpProxyForwardCorrectlyWhenActive)
{
    UdpProxyTest   tester;
    otbr::UdpProxy udpProxy(tester);

    udpProxy.Start(kTestThreadBaPort);
    EXPECT_NE(udpProxy.GetHostPort(), 0);

    // Send a UDP packet destined to loopback address.
    {
        int                sockFd;
        struct sockaddr_in destAddr;

        if ((sockFd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        {
            perror("socket creation failed");
            exit(EXIT_FAILURE);
        }

        memset(&destAddr, 0, sizeof(destAddr));
        destAddr.sin_family      = AF_INET;
        destAddr.sin_port        = htons(udpProxy.GetHostPort());
        destAddr.sin_addr.s_addr = inet_addr("127.0.0.1"); // Loopback address

        if (sendto(sockFd, kHello.c_str(), kHello.size(), 0, (const struct sockaddr *)&destAddr, sizeof(destAddr)) < 0)
        {
            perror("Failed to send UDP packet through loopback interface");
            exit(EXIT_FAILURE);
        }
        close(sockFd);
    }

    otbr::MainloopContext context;
    while (!tester.mForwarded)
    {
        context.mMaxFd   = -1;
        context.mTimeout = {100, 0};
        FD_ZERO(&context.mReadFdSet);
        FD_ZERO(&context.mWriteFdSet);
        FD_ZERO(&context.mErrorFdSet);

        otbr::MainloopManager::GetInstance().Update(context);
        int rval = select(context.mMaxFd + 1, &context.mReadFdSet, &context.mWriteFdSet, &context.mErrorFdSet,
                          &context.mTimeout);
        if (rval < 0)
        {
            perror("select failed");
            exit(EXIT_FAILURE);
        }
        otbr::MainloopManager::GetInstance().Process(context);
    }

    std::string udpPayload(reinterpret_cast<const char *>(tester.mPayload), tester.mLength);
    EXPECT_EQ(udpPayload, kHello);
    EXPECT_EQ(tester.mLength, kHello.size());
    EXPECT_EQ(tester.mLocalPort, kTestThreadBaPort);

    udpProxy.Stop();
}

TEST(UdpProxy, UdpProxySendToPeerCorrectlyWhenActive)
{
    UdpProxyTest   tester;
    otbr::UdpProxy udpProxy(tester);

    udpProxy.Start(kTestThreadBaPort);

    // Receive UDP packets on loopback address with specified port
    int                sockFd;
    const uint16_t     port = 12345;
    struct sockaddr_in listenAddr;
    uint8_t            recvBuf[kMaxUdpSize];

    if ((sockFd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&listenAddr, 0, sizeof(listenAddr));
    listenAddr.sin_family      = AF_INET;
    listenAddr.sin_port        = htons(port);
    listenAddr.sin_addr.s_addr = inet_addr("127.0.0.1"); // Loopback address

    if (bind(sockFd, (const struct sockaddr *)&listenAddr, sizeof(listenAddr)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Send a UDP packet through UDP Proxy
    otIp6Address peerAddress = {
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0x7f, 0x00, 0x00, 0x01}};
    udpProxy.SendToPeer(reinterpret_cast<const uint8_t *>(kHello.c_str()), kHello.size(), peerAddress, port);

    // Receive the UDP packet
    socklen_t   len = sizeof(listenAddr);
    int         n   = recvfrom(sockFd, (char *)recvBuf, kMaxUdpSize, MSG_WAITALL, (struct sockaddr *)&listenAddr, &len);
    std::string udpPayload(reinterpret_cast<const char *>(recvBuf), n);
    EXPECT_EQ(udpPayload, kHello);

    close(sockFd);

    udpProxy.Stop();
}
