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

#include <arpa/inet.h>
#include <cstring>
#include <fstream>
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

#ifdef __linux__
#include <linux/if_link.h>
#endif

#include <openthread/ip6.h>

#include "common/types.hpp"
#include "ncp/posix/netif.hpp"

// Only Test on linux platform for now.
#ifdef __linux__

static constexpr size_t kMaxIp6Size = 1280;

std::vector<std::string> GetAllIp6Addrs(const char *aInterfaceName)
{
    struct ifaddrs          *ifaddr, *ifa;
    int                      family;
    std::vector<std::string> ip6Addrs;

    if (getifaddrs(&ifaddr) == -1)
    {
        perror("getifaddrs");
        exit(EXIT_FAILURE);
    }

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
    {
        if (ifa->ifa_addr == NULL)
        {
            continue;
        }

        family = ifa->ifa_addr->sa_family;
        if (family == AF_INET6 && strcmp(ifa->ifa_name, aInterfaceName) == 0)
        {
            struct sockaddr_in6 *in6 = (struct sockaddr_in6 *)ifa->ifa_addr;
            char                 addrstr[INET6_ADDRSTRLEN];
            if (inet_ntop(AF_INET6, &(in6->sin6_addr), addrstr, sizeof(addrstr)) == NULL)
            {
                perror("inet_ntop");
                exit(EXIT_FAILURE);
            }

            ip6Addrs.emplace_back(addrstr);
        }
    }

    freeifaddrs(ifaddr);

    return ip6Addrs;
}

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

TEST(Netif, WpanAddrGenMode)
{
    otbr::Netif netif;
    EXPECT_EQ(netif.Init("wpan0"), OT_ERROR_NONE);

    std::fstream file("/proc/sys/net/ipv6/conf/wpan0/addr_gen_mode", std::ios::in);
    if (!file.is_open())
    {
        FAIL() << "wpan0 interface doesn't exist!";
    }
    std::string fileContents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    EXPECT_EQ(std::stoi(fileContents), IN6_ADDR_GEN_MODE_NONE);

    netif.Deinit();
}

TEST(Netif, WpanIfHasCorrectUnicastAddresses_AfterUpdatingUnicastAddresses)
{
    const char *wpan = "wpan0";

    const otIp6Address kLl = {
        {0xfe, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x14, 0x03, 0x32, 0x4c, 0xc2, 0xf8, 0xd0}};
    const otIp6Address kMlEid = {
        {0xfd, 0x0d, 0x07, 0xfc, 0xa1, 0xb9, 0xf0, 0x50, 0x03, 0xf1, 0x47, 0xce, 0x85, 0xd3, 0x07, 0x7f}};
    const otIp6Address kMlRloc = {
        {0xfd, 0x0d, 0x07, 0xfc, 0xa1, 0xb9, 0xf0, 0x50, 0x00, 0x00, 0x00, 0xff, 0xfe, 0x00, 0xb8, 0x00}};
    const otIp6Address kMlAloc = {
        {0xfd, 0x0d, 0x07, 0xfc, 0xa1, 0xb9, 0xf0, 0x50, 0x00, 0x00, 0x00, 0xff, 0xfe, 0x00, 0xfc, 0x00}};

    const char *kLlStr     = "fe80::8014:332:4cc2:f8d0";
    const char *kMlEidStr  = "fd0d:7fc:a1b9:f050:3f1:47ce:85d3:77f";
    const char *kMlRlocStr = "fd0d:7fc:a1b9:f050:0:ff:fe00:b800";
    const char *kMlAlocStr = "fd0d:7fc:a1b9:f050:0:ff:fe00:fc00";

    otbr::Netif netif;
    EXPECT_EQ(netif.Init(wpan), OT_ERROR_NONE);

    otbr::Ip6AddressInfo testArray1[] = {
        {kLl, 64, 0, 1, 0},
        {kMlEid, 64, 0, 1, 1},
        {kMlRloc, 64, 0, 1, 1},
    };
    std::vector<otbr::Ip6AddressInfo> testVec1(testArray1,
                                               testArray1 + sizeof(testArray1) / sizeof(otbr::Ip6AddressInfo));
    netif.UpdateIp6UnicastAddresses(testVec1);
    std::vector<std::string> wpan_addrs = GetAllIp6Addrs(wpan);
    EXPECT_EQ(wpan_addrs.size(), 3);
    EXPECT_THAT(wpan_addrs, ::testing::Contains(kLlStr));
    EXPECT_THAT(wpan_addrs, ::testing::Contains(kMlEidStr));
    EXPECT_THAT(wpan_addrs, ::testing::Contains(kMlRlocStr));

    otbr::Ip6AddressInfo testArray2[] = {
        {kLl, 64, 0, 1, 0},
        {kMlEid, 64, 0, 1, 1},
        {kMlRloc, 64, 0, 1, 1},
        {kMlAloc, 64, 0, 1, 1},
    };
    std::vector<otbr::Ip6AddressInfo> testVec2(testArray2,
                                               testArray2 + sizeof(testArray2) / sizeof(otbr::Ip6AddressInfo));
    netif.UpdateIp6UnicastAddresses(testVec2);
    wpan_addrs = GetAllIp6Addrs(wpan);
    EXPECT_EQ(wpan_addrs.size(), 4);
    EXPECT_THAT(wpan_addrs, ::testing::Contains(kLlStr));
    EXPECT_THAT(wpan_addrs, ::testing::Contains(kMlEidStr));
    EXPECT_THAT(wpan_addrs, ::testing::Contains(kMlRlocStr));
    EXPECT_THAT(wpan_addrs, ::testing::Contains(kMlAlocStr));

    std::vector<otbr::Ip6AddressInfo> testVec3;
    netif.UpdateIp6UnicastAddresses(testVec3);
    wpan_addrs = GetAllIp6Addrs(wpan);
    EXPECT_EQ(wpan_addrs.size(), 0);

    netif.Deinit();
}
#endif // __linux__
