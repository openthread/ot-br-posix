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

#include <CppUTest/TestHarness.h>

#include "common/types.hpp"

//-------------------------------------------------------------
// Test for Ip6Address
// TODO: Add Ip6Address tests
TEST_GROUP(Ip6Address){};

TEST(Ip6Address, NULL)
{
    TEST_EXIT;
}

//-------------------------------------------------------------
// Test for Ip6Prefix
TEST_GROUP(Ip6Prefix){};

TEST(Ip6Prefix, ConstructorWithAddressAndLength)
{
    using otbr::Ip6Prefix;

    Ip6Prefix prefix1("::", 0);
    STRCMP_EQUAL("::/0", prefix1.ToString().c_str());
    CHECK_EQUAL(0, prefix1.mLength);

    Ip6Prefix prefix2("fc00::", 7);
    STRCMP_EQUAL("fc00::/7", prefix2.ToString().c_str());
    CHECK_EQUAL(7, prefix2.mLength);

    Ip6Prefix prefix3("2001:db8::", 64);
    STRCMP_EQUAL("2001:db8::/64", prefix3.ToString().c_str());
    CHECK_EQUAL(64, prefix3.mLength);

    Ip6Prefix prefix4("2001:db8::1", 128);
    STRCMP_EQUAL("2001:db8::1/128", prefix4.ToString().c_str());
    CHECK_EQUAL(128, prefix4.mLength);
}

TEST(Ip6Prefix, EqualityOperator)
{
    using otbr::Ip6Prefix;

    // same prefix and length
    CHECK(Ip6Prefix("::", 0) == Ip6Prefix("::", 0));
    CHECK(Ip6Prefix("fc00::", 0) == Ip6Prefix("fc00::", 0));
    CHECK(Ip6Prefix("2001:db8::", 64) == Ip6Prefix("2001:db8::", 64));

    // same prefix, different length
    CHECK_FALSE(Ip6Prefix("::", 0) == Ip6Prefix("::", 7));
    CHECK_FALSE(Ip6Prefix("fc00::", 0) == Ip6Prefix("fc00::", 7));
    CHECK_FALSE(Ip6Prefix("fc00::", 7) == Ip6Prefix("fc00::", 8));
    CHECK_FALSE(Ip6Prefix("2001:db8::", 64) == Ip6Prefix("2001:db8::", 32));

    // different prefix object, same length
    CHECK(Ip6Prefix("::", 0) == Ip6Prefix("::1", 0));
    CHECK(Ip6Prefix("::", 0) == Ip6Prefix("2001::", 0));
    CHECK(Ip6Prefix("::", 0) == Ip6Prefix("2001:db8::1", 0));
    CHECK(Ip6Prefix("fc00::", 7) == Ip6Prefix("fd00::", 7));
    CHECK(Ip6Prefix("fc00::", 8) == Ip6Prefix("fc00:1234::", 8));
    CHECK(Ip6Prefix("2001:db8::", 32) == Ip6Prefix("2001:db8:abcd::", 32));
    CHECK(Ip6Prefix("2001:db8:0:1::", 63) == Ip6Prefix("2001:db8::", 63));
    CHECK(Ip6Prefix("2001:db8::", 64) == Ip6Prefix("2001:db8::1", 64));
    CHECK(Ip6Prefix("2001:db8::3", 127) == Ip6Prefix("2001:db8::2", 127));

    CHECK_FALSE(Ip6Prefix("fc00::", 7) == Ip6Prefix("fe00::", 7));
    CHECK_FALSE(Ip6Prefix("fc00::", 16) == Ip6Prefix("fc01::", 16));
    CHECK_FALSE(Ip6Prefix("fc00::", 32) == Ip6Prefix("fc00:1::", 32));
    CHECK_FALSE(Ip6Prefix("2001:db8:0:1::", 64) == Ip6Prefix("2001:db8::", 64));
    CHECK_FALSE(Ip6Prefix("2001:db8::1", 128) == Ip6Prefix("2001:db8::", 128));

    // different prefix object, different length
    CHECK_FALSE(Ip6Prefix("::", 0) == Ip6Prefix("2001::", 7));
    CHECK_FALSE(Ip6Prefix("fc00::", 7) == Ip6Prefix("fd00::", 8));
    CHECK_FALSE(Ip6Prefix("2001:db8:0:1::", 63) == Ip6Prefix("2001:db8::", 64));
}

// TODO: add more test cases for otbr::Ip6Prefix

//-------------------------------------------------------------
// Test for MacAddress
// TODO: Add MacAddress tests
TEST_GROUP(MacAddress){};

TEST(MacAddress, NULL)
{
    TEST_EXIT;
}
