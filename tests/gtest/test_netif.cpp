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
#include <netinet/ip6.h>
#include <netinet/udp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <vector>

#ifdef __linux__
#include <linux/if_link.h>
#endif

#include <openthread/ip6.h>

#include "common/code_utils.hpp"
#include "common/mainloop.hpp"
#include "common/types.hpp"
#include "ncp/posix/netif.hpp"
#include "utils/socket_utils.hpp"

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

static int ParseHex(char *aStr, unsigned char *aAddr)
{
    int len = 0;

    while (*aStr)
    {
        int tmp;
        if (aStr[1] == 0)
        {
            return -1;
        }
        if (sscanf(aStr, "%02x", &tmp) != 1)
        {
            return -1;
        }
        aAddr[len] = tmp;
        len++;
        aStr += 2;
    }

    return len;
}

std::vector<std::string> GetAllIp6MulAddrs(const char *aInterfaceName)
{
    const char              *kPathIgmp6 = "/proc/net/igmp6";
    std::string              line;
    std::vector<std::string> ip6MulAddrs;

    std::ifstream file(kPathIgmp6);
    if (!file.is_open())
    {
        perror("Cannot open IGMP6 file");
        exit(EXIT_FAILURE);
    }

    while (std::getline(file, line))
    {
        char          interfaceName[256] = {0};
        char          hexa[256]          = {0};
        int           index;
        int           users;
        unsigned char addr[16];

        sscanf(line.c_str(), "%d%s%s%d", &index, interfaceName, hexa, &users);
        if (strcmp(interfaceName, aInterfaceName) == 0)
        {
            char addrStr[INET6_ADDRSTRLEN];
            ParseHex(hexa, addr);
            if (inet_ntop(AF_INET6, addr, addrStr, sizeof(addrStr)) == NULL)
            {
                perror("inet_ntop");
                exit(EXIT_FAILURE);
            }
            ip6MulAddrs.emplace_back(addrStr);
        }
    }

    file.close();

    return ip6MulAddrs;
}

static otbr::Netif::Dependencies sDefaultNetifDependencies;

TEST(Netif, WpanInitWithFullInterfaceName)
{
    const char  *wpan = "wpan0";
    int          sockfd;
    struct ifreq ifr;

    otbr::Netif netif(sDefaultNetifDependencies);
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

    otbr::Netif netif(sDefaultNetifDependencies);
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

    otbr::Netif netif(sDefaultNetifDependencies);
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

    otbr::Netif netif(sDefaultNetifDependencies);
    EXPECT_EQ(netif.Init(invalid_netif_name), OTBR_ERROR_INVALID_ARGS);
}

TEST(Netif, WpanMtuSize)
{
    const char  *wpan = "wpan0";
    int          sockfd;
    struct ifreq ifr;

    otbr::Netif netif(sDefaultNetifDependencies);
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

    otbr::Netif netif(sDefaultNetifDependencies);
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
    otbr::Netif netif(sDefaultNetifDependencies);
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

    otbr::Netif netif(sDefaultNetifDependencies);
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

TEST(Netif, WpanIfHasCorrectMulticastAddresses_AfterUpdatingMulticastAddresses)
{
    const char *wpan = "wpan0";
    otbr::Netif netif(sDefaultNetifDependencies);
    EXPECT_EQ(netif.Init(wpan), OT_ERROR_NONE);

    otbr::Ip6Address kDefaultMulAddr1 = {
        {0xff, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01}};
    const char *kDefaultMulAddr1Str = "ff01::1";
    const char *kDefaultMulAddr2Str = "ff02::1";
    const char *kDefaultMulAddr3Str = "ff02::2";
    const char *kDefaultMulAddr4Str = "ff02::16";

    otbr::Ip6Address kMulAddr1 = {
        {0xff, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc}};
    otbr::Ip6Address kMulAddr2 = {
        {0xff, 0x32, 0x00, 0x40, 0xfd, 0x0d, 0x07, 0xfc, 0xa1, 0xb9, 0xf0, 0x50, 0x00, 0x00, 0x00, 0x01}};
    const char *kMulAddr1Str = "ff03::fc";
    const char *kMulAddr2Str = "ff32:40:fd0d:7fc:a1b9:f050:0:1";

    otbr::Ip6Address testArray1[] = {
        kMulAddr1,
    };
    std::vector<otbr::Ip6Address> testVec1(testArray1, testArray1 + sizeof(testArray1) / sizeof(otbr::Ip6Address));
    netif.UpdateIp6MulticastAddresses(testVec1);
    std::vector<std::string> wpanMulAddrs = GetAllIp6MulAddrs(wpan);
    EXPECT_EQ(wpanMulAddrs.size(), 5);
    EXPECT_THAT(wpanMulAddrs, ::testing::Contains(kMulAddr1Str));
    EXPECT_THAT(wpanMulAddrs, ::testing::Contains(kDefaultMulAddr1Str));
    EXPECT_THAT(wpanMulAddrs, ::testing::Contains(kDefaultMulAddr2Str));
    EXPECT_THAT(wpanMulAddrs, ::testing::Contains(kDefaultMulAddr3Str));
    EXPECT_THAT(wpanMulAddrs, ::testing::Contains(kDefaultMulAddr4Str));

    otbr::Ip6Address              testArray2[] = {kMulAddr1, kMulAddr2};
    std::vector<otbr::Ip6Address> testVec2(testArray2, testArray2 + sizeof(testArray2) / sizeof(otbr::Ip6Address));
    netif.UpdateIp6MulticastAddresses(testVec2);
    wpanMulAddrs = GetAllIp6MulAddrs(wpan);
    EXPECT_EQ(wpanMulAddrs.size(), 6);
    EXPECT_THAT(wpanMulAddrs, ::testing::Contains(kMulAddr1Str));
    EXPECT_THAT(wpanMulAddrs, ::testing::Contains(kMulAddr2Str));
    EXPECT_THAT(wpanMulAddrs, ::testing::Contains(kDefaultMulAddr1Str));
    EXPECT_THAT(wpanMulAddrs, ::testing::Contains(kDefaultMulAddr2Str));
    EXPECT_THAT(wpanMulAddrs, ::testing::Contains(kDefaultMulAddr3Str));
    EXPECT_THAT(wpanMulAddrs, ::testing::Contains(kDefaultMulAddr4Str));

    otbr::Ip6Address              testArray3[] = {kDefaultMulAddr1};
    std::vector<otbr::Ip6Address> testVec3(testArray3, testArray3 + sizeof(testArray3) / sizeof(otbr::Ip6Address));
    netif.UpdateIp6MulticastAddresses(testVec3);
    wpanMulAddrs = GetAllIp6MulAddrs(wpan);
    EXPECT_EQ(wpanMulAddrs.size(), 4);
    EXPECT_THAT(wpanMulAddrs, ::testing::Contains(kDefaultMulAddr1Str));
    EXPECT_THAT(wpanMulAddrs, ::testing::Contains(kDefaultMulAddr2Str));
    EXPECT_THAT(wpanMulAddrs, ::testing::Contains(kDefaultMulAddr3Str));
    EXPECT_THAT(wpanMulAddrs, ::testing::Contains(kDefaultMulAddr4Str));

    std::vector<otbr::Ip6Address> empty;
    netif.UpdateIp6MulticastAddresses(empty);
    wpanMulAddrs = GetAllIp6MulAddrs(wpan);
    EXPECT_EQ(wpanMulAddrs.size(), 4);
    EXPECT_THAT(wpanMulAddrs, ::testing::Contains(kDefaultMulAddr1Str));
    EXPECT_THAT(wpanMulAddrs, ::testing::Contains(kDefaultMulAddr2Str));
    EXPECT_THAT(wpanMulAddrs, ::testing::Contains(kDefaultMulAddr3Str));
    EXPECT_THAT(wpanMulAddrs, ::testing::Contains(kDefaultMulAddr4Str));

    netif.Deinit();
}

TEST(Netif, WpanIfStateChangesCorrectly_AfterSettingNetifState)
{
    otbr::Netif netif(sDefaultNetifDependencies);
    const char *wpan = "wpan0";
    EXPECT_EQ(netif.Init(wpan), OTBR_ERROR_NONE);

    int fd = SocketWithCloseExec(AF_INET6, SOCK_DGRAM, IPPROTO_IP, kSocketNonBlock);
    if (fd < 0)
    {
        perror("Failed to create test socket");
        exit(EXIT_FAILURE);
    }

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, wpan, IFNAMSIZ - 1);

    netif.SetNetifState(true);
    ioctl(fd, SIOCGIFFLAGS, &ifr);
    EXPECT_EQ(ifr.ifr_flags & IFF_UP, IFF_UP);

    netif.SetNetifState(false);
    ioctl(fd, SIOCGIFFLAGS, &ifr);
    EXPECT_EQ(ifr.ifr_flags & IFF_UP, 0);

    netif.Deinit();
}

TEST(Netif, WpanIfRecvIp6PacketCorrectly_AfterReceivingFromNetif)
{
    otbr::Netif netif(sDefaultNetifDependencies);
    EXPECT_EQ(netif.Init("wpan0"), OTBR_ERROR_NONE);

    const otIp6Address kOmr = {
        {0xfd, 0x2a, 0xc3, 0x0c, 0x87, 0xd3, 0x00, 0x01, 0xed, 0x1c, 0x0c, 0x91, 0xcc, 0xb6, 0x57, 0x8b}};
    std::vector<otbr::Ip6AddressInfo> addrs = {
        {kOmr, 64, 0, 1, 0},
    };
    netif.UpdateIp6UnicastAddresses(addrs);
    netif.SetNetifState(true);

    // Receive UDP packets on wpan address with specified port.
    int                 sockFd;
    const uint16_t      port = 12345;
    struct sockaddr_in6 listenAddr;
    const char         *listenIp = "fd2a:c30c:87d3:1:ed1c:c91:ccb6:578b";
    uint8_t             recvBuf[kMaxIp6Size];

    if ((sockFd = socket(AF_INET6, SOCK_DGRAM, 0)) < 0)
    {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&listenAddr, 0, sizeof(listenAddr));
    listenAddr.sin6_family = AF_INET6;
    listenAddr.sin6_port   = htons(port);
    inet_pton(AF_INET6, listenIp, &(listenAddr.sin6_addr));

    if (bind(sockFd, (const struct sockaddr *)&listenAddr, sizeof(listenAddr)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Udp Packet
    // Ip6 source: fd2a:c30c:87d3:1:ed1c:c91:ccb6:578a
    // Ip6 destination: fd2a:c30c:87d3:1:ed1c:c91:ccb6:578b
    // Udp destination port: 12345
    // Udp payload: "Hello Otbr Netif!"
    const uint8_t udpPacket[] = {0x60, 0x0e, 0xea, 0x69, 0x00, 0x19, 0x11, 0x40, 0xfd, 0x2a, 0xc3, 0x0c, 0x87,
                                 0xd3, 0x00, 0x01, 0xed, 0x1c, 0x0c, 0x91, 0xcc, 0xb6, 0x57, 0x8a, 0xfd, 0x2a,
                                 0xc3, 0x0c, 0x87, 0xd3, 0x00, 0x01, 0xed, 0x1c, 0x0c, 0x91, 0xcc, 0xb6, 0x57,
                                 0x8b, 0xe7, 0x08, 0x30, 0x39, 0x00, 0x19, 0x36, 0x81, 0x48, 0x65, 0x6c, 0x6c,
                                 0x6f, 0x20, 0x4f, 0x74, 0x62, 0x72, 0x20, 0x4e, 0x65, 0x74, 0x69, 0x66, 0x21};
    netif.Ip6Receive(udpPacket, sizeof(udpPacket));

    socklen_t   len = sizeof(listenAddr);
    int         n   = recvfrom(sockFd, (char *)recvBuf, kMaxIp6Size, MSG_WAITALL, (struct sockaddr *)&listenAddr, &len);
    std::string udpPayload(reinterpret_cast<const char *>(recvBuf), n);
    EXPECT_EQ(udpPayload, "Hello Otbr Netif!");

    close(sockFd);
    netif.Deinit();
}

class NetifDependencyTestIp6Send : public otbr::Netif::Dependencies
{
public:
    NetifDependencyTestIp6Send(bool &aReceived, std::string &aReceivedPayload)
        : mReceived(aReceived)
        , mReceivedPayload(aReceivedPayload)
    {
    }

    otbrError Ip6Send(const uint8_t *aData, uint16_t aLength) override
    {
        const ip6_hdr *ipv6_header = reinterpret_cast<const ip6_hdr *>(aData);
        if (ipv6_header->ip6_nxt == IPPROTO_UDP)
        {
            const uint8_t *udpPayload    = aData + aLength - ntohs(ipv6_header->ip6_plen) + sizeof(udphdr);
            uint16_t       udpPayloadLen = ntohs(ipv6_header->ip6_plen) - sizeof(udphdr);
            mReceivedPayload             = std::string(reinterpret_cast<const char *>(udpPayload), udpPayloadLen);

            mReceived = true;
        }

        return OTBR_ERROR_NONE;
    }

    bool        &mReceived;
    std::string &mReceivedPayload;
};

TEST(Netif, WpanIfSendIp6PacketCorrectly_AfterReceivingOnIf)
{
    bool                       received = false;
    std::string                receivedPayload;
    NetifDependencyTestIp6Send netifDependency(received, receivedPayload);
    const char                *hello = "Hello Otbr Netif!";

    otbr::Netif netif(netifDependency);
    EXPECT_EQ(netif.Init("wpan0"), OT_ERROR_NONE);

    // OMR Prefix: fd76:a5d1:fcb0:1707::/64
    const otIp6Address kOmr = {
        {0xfd, 0x76, 0xa5, 0xd1, 0xfc, 0xb0, 0x17, 0x07, 0xf3, 0xc7, 0xd8, 0x8c, 0xef, 0xd1, 0x24, 0xa9}};
    std::vector<otbr::Ip6AddressInfo> addrs = {
        {kOmr, 64, 0, 1, 0},
    };
    netif.UpdateIp6UnicastAddresses(addrs);
    netif.SetNetifState(true);

    // Send a UDP packet destined to an address with OMR prefix.
    {
        int                 sockFd;
        const uint16_t      destPort = 12345;
        struct sockaddr_in6 destAddr;
        const char         *destIp = "fd76:a5d1:fcb0:1707:3f1:47ce:85d3:77f";

        if ((sockFd = socket(AF_INET6, SOCK_DGRAM, 0)) < 0)
        {
            perror("socket creation failed");
            exit(EXIT_FAILURE);
        }

        memset(&destAddr, 0, sizeof(destAddr));
        destAddr.sin6_family = AF_INET6;
        destAddr.sin6_port   = htons(destPort);
        inet_pton(AF_INET6, destIp, &(destAddr.sin6_addr));

        if (sendto(sockFd, hello, strlen(hello), MSG_CONFIRM, (const struct sockaddr *)&destAddr, sizeof(destAddr)) < 0)
        {
            FAIL() << "Failed to send UDP packet through WPAN interface";
        }
        close(sockFd);
    }

    otbr::MainloopContext context;
    while (!received)
    {
        context.mMaxFd   = -1;
        context.mTimeout = {100, 0};
        FD_ZERO(&context.mReadFdSet);
        FD_ZERO(&context.mWriteFdSet);
        FD_ZERO(&context.mErrorFdSet);

        netif.UpdateFdSet(&context);
        int rval = select(context.mMaxFd + 1, &context.mReadFdSet, &context.mWriteFdSet, &context.mErrorFdSet,
                          &context.mTimeout);
        if (rval < 0)
        {
            perror("select failed");
            exit(EXIT_FAILURE);
        }
        netif.Process(&context);
    }

    EXPECT_STREQ(receivedPayload.c_str(), hello);

    netif.Deinit();
}

class NetifDependencyTestMulSub : public otbr::Netif::Dependencies
{
public:
    NetifDependencyTestMulSub(bool &aReceived, otIp6Address &aMulAddr, bool &aIsAdded)
        : mReceived(aReceived)
        , mMulAddr(aMulAddr)
        , mIsAdded(aIsAdded)
    {
    }

    otbrError Ip6MulAddrUpdateSubscription(const otIp6Address &aAddress, bool aIsAdded) override
    {
        mMulAddr  = aAddress;
        mIsAdded  = aIsAdded;
        mReceived = true;
        return OTBR_ERROR_NONE;
    }

    bool         &mReceived;
    otIp6Address &mMulAddr;
    bool         &mIsAdded;
};

TEST(Netif, WpanIfUpdateMulAddrSubscription_AfterAppJoiningMulGrp)
{
    bool                      received = false;
    otIp6Address              subscribedMulAddr;
    bool                      isAdded = false;
    NetifDependencyTestMulSub dependency(received, subscribedMulAddr, isAdded);
    const char               *multicastGroup = "ff99::1";
    const char               *wpan           = "wpan0";
    int                       sockFd;
    otbr::Netif               netif(dependency);
    const otIp6Address        expectedMulAddr = {0xff, 0x99, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};

    EXPECT_EQ(netif.Init("wpan0"), OT_ERROR_NONE);

    const otIp6Address kLl = {
        {0xfe, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x14, 0x03, 0x32, 0x4c, 0xc2, 0xf8, 0xd0}};
    std::vector<otbr::Ip6AddressInfo> addrs = {
        {kLl, 64, 0, 1, 0},
    };
    netif.UpdateIp6UnicastAddresses(addrs);
    netif.SetNetifState(true);

    {
        struct ipv6_mreq    mreq;
        struct sockaddr_in6 addr;

        if ((sockFd = socket(AF_INET6, SOCK_DGRAM, 0)) < 0)
        {
            perror("socket creation failed");
            exit(EXIT_FAILURE);
        }

        memset(&addr, 0, sizeof(addr));
        addr.sin6_family = AF_INET6;
        addr.sin6_addr   = in6addr_any;
        addr.sin6_port   = htons(9999);

        if (bind(sockFd, (const struct sockaddr *)&addr, sizeof(addr)) < 0)
        {
            perror("bind failed");
            exit(EXIT_FAILURE);
        }

        inet_pton(AF_INET6, multicastGroup, &(mreq.ipv6mr_multiaddr));
        mreq.ipv6mr_interface = if_nametoindex(wpan);

        if (setsockopt(sockFd, IPPROTO_IPV6, IPV6_JOIN_GROUP, &mreq, sizeof(mreq)) < 0)
        {
            perror("Error joining multicast group");
            exit(EXIT_FAILURE);
        }
    }

    otbr::MainloopContext context;
    while (!received)
    {
        context.mMaxFd   = -1;
        context.mTimeout = {100, 0};
        FD_ZERO(&context.mReadFdSet);
        FD_ZERO(&context.mWriteFdSet);
        FD_ZERO(&context.mErrorFdSet);

        netif.UpdateFdSet(&context);
        int rval = select(context.mMaxFd + 1, &context.mReadFdSet, &context.mWriteFdSet, &context.mErrorFdSet,
                          &context.mTimeout);
        if (rval < 0)
        {
            perror("select failed");
            exit(EXIT_FAILURE);
        }
        netif.Process(&context);
    }

    EXPECT_EQ(otbr::Ip6Address(subscribedMulAddr), otbr::Ip6Address(expectedMulAddr));
    EXPECT_EQ(isAdded, true);
    close(sockFd);
    netif.Deinit();
}

#endif // __linux__
