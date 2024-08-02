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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstring>
#include <ifaddrs.h>
#include <iostream>
#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <vector>

#include <openthread/ip6.h>

#include "common/types.hpp"
#include "ncp/posix/netif.hpp"

// Only Test on linux platform for now.
#ifdef __linux__

static constexpr size_t kMaxIp6Size = 1280;

TEST(Netif, WpanInitWithFullInterfaceName)
{
    const char  *wpan = "wpan0";
    int          sockfd;
    struct ifreq ifr;

    otbr::Netif netif;
    EXPECT_EQ(netif.Init(wpan), OT_ERROR_NONE);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        FAIL() << "Error creating socket: " << std::strerror(errno);
    }

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, wpan, IFNAMSIZ - 1);

    EXPECT_GE(ioctl(sockfd, SIOCGIFFLAGS, &ifr), 0) << "'" << wpan << "' not found";

    netif.Deinit();
}

TEST(Netif, WpanInitWithFormatInterfaceName)
{
    const char  *wpan    = "tun%d";
    const char  *if_name = "tun0";
    int          sockfd;
    struct ifreq ifr;

    otbr::Netif netif;
    EXPECT_EQ(netif.Init(wpan), OT_ERROR_NONE);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        FAIL() << "Error creating socket: " << std::strerror(errno);
    }

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, if_name, IFNAMSIZ - 1);

    EXPECT_GE(ioctl(sockfd, SIOCGIFFLAGS, &ifr), 0) << "'" << if_name << "' not found";

    netif.Deinit();
}

TEST(Netif, WpanInitWithEmptyInterfaceName)
{
    const char  *if_name = "wpan0";
    int          sockfd;
    struct ifreq ifr;

    otbr::Netif netif;
    EXPECT_EQ(netif.Init(""), OT_ERROR_NONE);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        FAIL() << "Error creating socket: " << std::strerror(errno);
    }

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, if_name, IFNAMSIZ - 1);

    EXPECT_GE(ioctl(sockfd, SIOCGIFFLAGS, &ifr), 0) << "'" << if_name << "' not found";

    netif.Deinit();
}

TEST(Netif, WpanInitWithInvalidInterfaceName)
{
    const char *invalid_netif_name = "invalid_netif_name";

    otbr::Netif netif;
    EXPECT_EQ(netif.Init(invalid_netif_name), OTBR_ERROR_INVALID_ARGS);
}

TEST(Netif, WpanMtuSize)
{
    const char  *wpan = "wpan0";
    int          sockfd;
    struct ifreq ifr;

    otbr::Netif netif;
    EXPECT_EQ(netif.Init(wpan), OT_ERROR_NONE);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        FAIL() << "Error creating socket: " << std::strerror(errno);
    }

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, wpan, IFNAMSIZ - 1);
    EXPECT_GE(ioctl(sockfd, SIOCGIFMTU, &ifr), 0) << "Error getting MTU for '" << wpan << "': " << std::strerror(errno);
    EXPECT_EQ(ifr.ifr_mtu, kMaxIp6Size) << "MTU isn't set correctly";

    netif.Deinit();
}

TEST(Netif, WpanDeinit)
{
    const char  *wpan = "wpan0";
    int          sockfd;
    struct ifreq ifr;

    otbr::Netif netif;
    EXPECT_EQ(netif.Init(wpan), OT_ERROR_NONE);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        FAIL() << "Error creating socket: " << std::strerror(errno);
    }

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, wpan, IFNAMSIZ - 1);
    EXPECT_GE(ioctl(sockfd, SIOCGIFFLAGS, &ifr), 0) << "'" << wpan << "' not found";

    netif.Deinit();
    EXPECT_LT(ioctl(sockfd, SIOCGIFFLAGS, &ifr), 0) << "'" << wpan << "' isn't shutdown";
}

#endif // __linux__
