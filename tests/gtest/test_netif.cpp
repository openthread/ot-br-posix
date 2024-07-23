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
#include <ifaddrs.h>
#include <iostream>
#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <vector>

#ifdef __linux__
#include <linux/if_addr.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#endif

#include <openthread/ip6.h>

#include "ncp/posix/netif.hpp"

// Only Test on linux platform for now.
#ifdef __linux__
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

TEST(Netif, UpdateUnicastAddresses)
{
    const char *wpan = "wpan0";

    const otIp6Address kUaLl = {
        {0xfe, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x14, 0x03, 0x32, 0x4c, 0xc2, 0xf8, 0xd0}};
    const otIp6Address kUaMlEid = {
        {0xfd, 0x0d, 0x07, 0xfc, 0xa1, 0xb9, 0xf0, 0x50, 0x03, 0xf1, 0x47, 0xce, 0x85, 0xd3, 0x07, 0x7f}};
    const otIp6Address kUaMlRloc = {
        {0xfd, 0x0d, 0x07, 0xfc, 0xa1, 0xb9, 0xf0, 0x50, 0x00, 0x00, 0x00, 0xff, 0xfe, 0x00, 0xb8, 0x00}};
    const otIp6Address kUaMlAloc = {
        {0xfd, 0x0d, 0x07, 0xfc, 0xa1, 0xb9, 0xf0, 0x50, 0x00, 0x00, 0x00, 0xff, 0xfe, 0x00, 0xfc, 0x00}};

    const char *kUaLlStr     = "fe80::8014:332:4cc2:f8d0";
    const char *kUaMlEidStr  = "fd0d:7fc:a1b9:f050:3f1:47ce:85d3:77f";
    const char *kUaMlRlocStr = "fd0d:7fc:a1b9:f050:0:ff:fe00:b800";
    const char *kUaMlAlocStr = "fd0d:7fc:a1b9:f050:0:ff:fe00:fc00";

    otbr::Posix::Netif netif(wpan);
    netif.Init();

    otIp6AddressInfo testUaArray1[] = {
        {&kUaLl, 64, 0, 1, 0},
        {&kUaMlEid, 64, 0, 1, 1},
        {&kUaMlRloc, 64, 0, 1, 1},
    };
    std::vector<otIp6AddressInfo> testUaVec1(testUaArray1,
                                             testUaArray1 + sizeof(testUaArray1) / sizeof(otIp6AddressInfo));
    netif.UpdateIp6Addresses(testUaVec1);
    std::vector<std::string> wpan_addrs = GetAllIp6Addrs(wpan);
    EXPECT_EQ(wpan_addrs.size(), 3);
    EXPECT_THAT(wpan_addrs, ::testing::Contains(kUaLlStr));
    EXPECT_THAT(wpan_addrs, ::testing::Contains(kUaMlEidStr));
    EXPECT_THAT(wpan_addrs, ::testing::Contains(kUaMlRlocStr));

    otIp6AddressInfo testUaArray2[] = {
        {&kUaLl, 64, 0, 1, 0},
        {&kUaMlEid, 64, 0, 1, 1},
        {&kUaMlRloc, 64, 0, 1, 1},
        {&kUaMlAloc, 64, 0, 1, 1},
    };
    std::vector<otIp6AddressInfo> testUaVec2(testUaArray2,
                                             testUaArray2 + sizeof(testUaArray2) / sizeof(otIp6AddressInfo));
    netif.UpdateIp6Addresses(testUaVec2);
    wpan_addrs = GetAllIp6Addrs(wpan);
    EXPECT_EQ(wpan_addrs.size(), 4);
    EXPECT_THAT(wpan_addrs, ::testing::Contains(kUaLlStr));
    EXPECT_THAT(wpan_addrs, ::testing::Contains(kUaMlEidStr));
    EXPECT_THAT(wpan_addrs, ::testing::Contains(kUaMlRlocStr));
    EXPECT_THAT(wpan_addrs, ::testing::Contains(kUaMlAlocStr));

    netif.Deinit();
}

#endif // __linux__
