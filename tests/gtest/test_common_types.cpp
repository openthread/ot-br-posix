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

#include <arpa/inet.h>

#include <gtest/gtest.h>

#include "common/types.hpp"

//-------------------------------------------------------------
// Test for Ip6Address

TEST(Ip6Address, Ipv4MappedAddress)
{
    using otbr::Ip6Address;

    struct sockaddr_in sockAddr;
    memset(&sockAddr, 0, sizeof(sockAddr));
    sockAddr.sin_family = AF_INET;
    ASSERT_EQ(inet_pton(AF_INET, "192.168.1.100", &sockAddr.sin_addr), 1);

    Ip6Address address;
    EXPECT_EQ(address.CopyFrom(reinterpret_cast<const struct sockaddr &>(sockAddr)), OTBR_ERROR_NONE);
    EXPECT_TRUE(address.IsIp4Mapped());
    EXPECT_STREQ(address.ToString().c_str(), "::ffff:192.168.1.100");

    Ip6Address addressFromSockAddrIn;
    addressFromSockAddrIn.CopyFrom(sockAddr);
    EXPECT_TRUE(addressFromSockAddrIn.IsIp4Mapped());
    EXPECT_STREQ(addressFromSockAddrIn.ToString().c_str(), "::ffff:192.168.1.100");

    struct in_addr inAddr;
    ASSERT_EQ(inet_pton(AF_INET, "10.0.0.1", &inAddr), 1);

    Ip6Address addressFromInAddr;
    addressFromInAddr.CopyFrom(inAddr);
    EXPECT_TRUE(addressFromInAddr.IsIp4Mapped());
    EXPECT_STREQ(addressFromInAddr.ToString().c_str(), "::ffff:10.0.0.1");

    struct sockaddr_in6 sockAddr6;
    memset(&sockAddr6, 0, sizeof(sockAddr6));
    sockAddr6.sin6_family = AF_INET6;
    ASSERT_EQ(inet_pton(AF_INET6, "fd00::1", &sockAddr6.sin6_addr), 1);

    Ip6Address address6;
    EXPECT_EQ(address6.CopyFrom(reinterpret_cast<const struct sockaddr &>(sockAddr6)), OTBR_ERROR_NONE);
    EXPECT_FALSE(address6.IsIp4Mapped());
    EXPECT_STREQ(address6.ToString().c_str(), "fd00::1");

    struct sockaddr invalidSockAddr;
    memset(&invalidSockAddr, 0, sizeof(invalidSockAddr));
    invalidSockAddr.sa_family = AF_UNSPEC;
    EXPECT_EQ(address.CopyFrom(invalidSockAddr), OTBR_ERROR_INVALID_ARGS);

    // Negative matches
    EXPECT_FALSE(Ip6Address("::").IsIp4Mapped());
    EXPECT_FALSE(Ip6Address("::1").IsIp4Mapped());
    EXPECT_FALSE(Ip6Address("::ffff:0:0:0").IsIp4Mapped());
    EXPECT_FALSE(Ip6Address("::1.2.3.4").IsIp4Mapped());
    EXPECT_FALSE(Ip6Address("ffff::").IsIp4Mapped());
    EXPECT_FALSE(Ip6Address("::fffe:0:0").IsIp4Mapped());

    // Boundary positive matches
    EXPECT_TRUE(Ip6Address("::ffff:0:0").IsIp4Mapped());
    EXPECT_TRUE(Ip6Address("::ffff:0.0.0.0").IsIp4Mapped());
    EXPECT_STREQ(Ip6Address("::ffff:0:0").ToString().c_str(), "::ffff:0.0.0.0");
    EXPECT_TRUE(Ip6Address("::ffff:255.255.255.255").IsIp4Mapped());
    EXPECT_TRUE(Ip6Address("::ffff:ffff:ffff").IsIp4Mapped());
    EXPECT_STREQ(Ip6Address("::ffff:ffff:ffff").ToString().c_str(), "::ffff:255.255.255.255");

    inAddr.s_addr = INADDR_ANY;
    addressFromInAddr.CopyFrom(inAddr);
    EXPECT_TRUE(addressFromInAddr.IsIp4Mapped());
    EXPECT_STREQ(addressFromInAddr.ToString().c_str(), "::ffff:0.0.0.0");

    inAddr.s_addr = INADDR_NONE;
    addressFromInAddr.CopyFrom(inAddr);
    EXPECT_TRUE(addressFromInAddr.IsIp4Mapped());
    EXPECT_STREQ(addressFromInAddr.ToString().c_str(), "::ffff:255.255.255.255");
}

//-------------------------------------------------------------
// Test for Ip6Prefix

TEST(Ip6Prefix, ConstructorWithAddressAndLength)
{
    using otbr::Ip6Prefix;

    Ip6Prefix prefix1("::", 0);
    EXPECT_STREQ(prefix1.ToString().c_str(), "::/0");
    EXPECT_EQ(prefix1.mLength, 0);

    Ip6Prefix prefix2("fc00::", 7);
    EXPECT_STREQ(prefix2.ToString().c_str(), "fc00::/7");
    EXPECT_EQ(prefix2.mLength, 7);

    Ip6Prefix prefix3("2001:db8::", 64);
    EXPECT_STREQ(prefix3.ToString().c_str(), "2001:db8::/64");
    EXPECT_EQ(prefix3.mLength, 64);

    Ip6Prefix prefix4("2001:db8::1", 128);
    EXPECT_STREQ(prefix4.ToString().c_str(), "2001:db8::1/128");
    EXPECT_EQ(prefix4.mLength, 128);
}

TEST(Ip6Prefix, EqualityOperator)
{
    using otbr::Ip6Prefix;

    // same prefix and length
    EXPECT_EQ(Ip6Prefix("::", 0), Ip6Prefix("::", 0));
    EXPECT_EQ(Ip6Prefix("fc00::", 0), Ip6Prefix("fc00::", 0));
    EXPECT_EQ(Ip6Prefix("2001:db8::", 64), Ip6Prefix("2001:db8::", 64));

    // same prefix, different length
    EXPECT_NE(Ip6Prefix("::", 0), Ip6Prefix("::", 7));
    EXPECT_NE(Ip6Prefix("fc00::", 0), Ip6Prefix("fc00::", 7));
    EXPECT_NE(Ip6Prefix("fc00::", 7), Ip6Prefix("fc00::", 8));
    EXPECT_NE(Ip6Prefix("2001:db8::", 64), Ip6Prefix("2001:db8::", 32));

    // different prefix object, same length
    EXPECT_EQ(Ip6Prefix("::", 0), Ip6Prefix("::1", 0));
    EXPECT_EQ(Ip6Prefix("::", 0), Ip6Prefix("2001::", 0));
    EXPECT_EQ(Ip6Prefix("::", 0), Ip6Prefix("2001:db8::1", 0));
    EXPECT_EQ(Ip6Prefix("fc00::", 7), Ip6Prefix("fd00::", 7));
    EXPECT_EQ(Ip6Prefix("fc00::", 8), Ip6Prefix("fc00:1234::", 8));
    EXPECT_EQ(Ip6Prefix("2001:db8::", 32), Ip6Prefix("2001:db8:abcd::", 32));
    EXPECT_EQ(Ip6Prefix("2001:db8:0:1::", 63), Ip6Prefix("2001:db8::", 63));
    EXPECT_EQ(Ip6Prefix("2001:db8::", 64), Ip6Prefix("2001:db8::1", 64));
    EXPECT_EQ(Ip6Prefix("2001:db8::3", 127), Ip6Prefix("2001:db8::2", 127));

    EXPECT_NE(Ip6Prefix("fc00::", 7), Ip6Prefix("fe00::", 7));
    EXPECT_NE(Ip6Prefix("fc00::", 16), Ip6Prefix("fc01::", 16));
    EXPECT_NE(Ip6Prefix("fc00::", 32), Ip6Prefix("fc00:1::", 32));
    EXPECT_NE(Ip6Prefix("2001:db8:0:1::", 64), Ip6Prefix("2001:db8::", 64));
    EXPECT_NE(Ip6Prefix("2001:db8::1", 128), Ip6Prefix("2001:db8::", 128));

    // different prefix object, different length
    EXPECT_NE(Ip6Prefix("::", 0), Ip6Prefix("2001::", 7));
    EXPECT_NE(Ip6Prefix("fc00::", 7), Ip6Prefix("fd00::", 8));
    EXPECT_NE(Ip6Prefix("2001:db8:0:1::", 63), Ip6Prefix("2001:db8::", 64));
}

// TODO: add more test cases for otbr::Ip6Prefix

//-------------------------------------------------------------
// Test for MacAddress
// TODO: Add MacAddress tests
