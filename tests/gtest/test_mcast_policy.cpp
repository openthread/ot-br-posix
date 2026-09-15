/*
 *    Copyright (c) 2026, The OpenThread Authors.
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

#include <gtest/gtest.h>

#include <string.h>
#include <vector>

#include "host/posix/mcast_policy.hpp"

namespace {

using otbr::Ip6Address;
using otbr::McastDedupCache;
using otbr::McastForwardPolicy;
using otbr::McastRateLimiter;
using otbr::Milliseconds;
using otbr::Seconds;
using otbr::Timepoint;
using otbr::TokenBucket;

// A minimal IPv6/UDP packet: src -> dst, hop limit, payload.
std::vector<uint8_t> MakePacket(const char *aSource, const char *aDestination, uint8_t aHopLimit, const char *aPayload)
{
    std::vector<uint8_t> packet(40, 0);
    size_t               payloadLength = 8 + strlen(aPayload);

    packet[0] = 0x60;
    packet[4] = static_cast<uint8_t>(payloadLength >> 8);
    packet[5] = static_cast<uint8_t>(payloadLength & 0xff);
    packet[6] = 17;
    packet[7] = aHopLimit;
    memcpy(&packet[8], Ip6Address(aSource).m8, 16);
    memcpy(&packet[24], Ip6Address(aDestination).m8, 16);

    std::vector<uint8_t> udp = {
        0x10, 0xe1, 0x04, 0xd2, static_cast<uint8_t>(payloadLength >> 8), static_cast<uint8_t>(payloadLength & 0xff),
        0,    0};

    packet.insert(packet.end(), udp.begin(), udp.end());
    packet.insert(packet.end(), aPayload, aPayload + strlen(aPayload));

    return packet;
}

Timepoint At(uint32_t aMs)
{
    return Timepoint() + Milliseconds(aMs);
}

TEST(McastForwardPolicy, ForwardsSiteAndGlobalScopeFromRoutableSources)
{
    std::vector<uint8_t> packet = MakePacket("fd12::1", "ff05::abcd", 64, "hi");

    EXPECT_EQ(McastForwardPolicy::Check(packet.data(), packet.size()), McastForwardPolicy::kForward);

    packet = MakePacket("2001:db8::1", "ff0e::1", 2, "hi");
    EXPECT_EQ(McastForwardPolicy::Check(packet.data(), packet.size()), McastForwardPolicy::kForward);

    packet = MakePacket("fd12::1", "ff04::1", 64, "hi");
    EXPECT_EQ(McastForwardPolicy::Check(packet.data(), packet.size()), McastForwardPolicy::kForward);
}

TEST(McastForwardPolicy, RejectsWhatABackboneRouterMustNotForward)
{
    std::vector<uint8_t> packet;

    packet = MakePacket("fd12::1", "ff02::1", 64, "hi");
    EXPECT_EQ(McastForwardPolicy::Check(packet.data(), packet.size()), McastForwardPolicy::kScopeTooSmall);

    packet = MakePacket("fd12::1", "ff03::fc", 64, "hi");
    EXPECT_EQ(McastForwardPolicy::Check(packet.data(), packet.size()), McastForwardPolicy::kScopeTooSmall);

    packet = MakePacket("fd12::1", "fd12::2", 64, "hi");
    EXPECT_EQ(McastForwardPolicy::Check(packet.data(), packet.size()), McastForwardPolicy::kNotMulticast);

    packet = MakePacket("fe80::1", "ff05::abcd", 64, "hi");
    EXPECT_EQ(McastForwardPolicy::Check(packet.data(), packet.size()), McastForwardPolicy::kBadSource);

    packet = MakePacket("::", "ff05::abcd", 64, "hi");
    EXPECT_EQ(McastForwardPolicy::Check(packet.data(), packet.size()), McastForwardPolicy::kBadSource);

    packet = MakePacket("fd12::1", "ff05::abcd", 1, "hi");
    EXPECT_EQ(McastForwardPolicy::Check(packet.data(), packet.size()), McastForwardPolicy::kHopLimitExceeded);

    packet = MakePacket("fd12::1", "ff05::abcd", 64, "hi");
    EXPECT_EQ(McastForwardPolicy::Check(packet.data(), 39), McastForwardPolicy::kNotIp6);
    packet[0] = 0x45;
    EXPECT_EQ(McastForwardPolicy::Check(packet.data(), packet.size()), McastForwardPolicy::kNotIp6);

    // Payload length claims more than the packet carries
    packet    = MakePacket("fd12::1", "ff05::abcd", 64, "hi");
    packet[5] = 0xff;
    EXPECT_EQ(McastForwardPolicy::Check(packet.data(), packet.size()), McastForwardPolicy::kNotIp6);
}

TEST(McastForwardPolicy, BuildsTheEthernetFrame)
{
    std::vector<uint8_t> packet = MakePacket("fd12::1", "ff05::1234:abcd", 64, "hi");
    const uint8_t        mac[6] = {0x02, 0x11, 0x22, 0x33, 0x44, 0x55};
    uint8_t              frame[1514];
    size_t               length;

    McastForwardPolicy::DecrementHopLimit(packet.data());
    EXPECT_EQ(packet[7], 63);

    length = McastForwardPolicy::BuildEthernetFrame(packet.data(), packet.size(), mac, frame, sizeof(frame));
    ASSERT_EQ(length, 14 + packet.size());
    const uint8_t expectedDst[6] = {0x33, 0x33, 0x12, 0x34, 0xab, 0xcd};
    EXPECT_EQ(memcmp(frame, expectedDst, 6), 0);
    EXPECT_EQ(memcmp(frame + 6, mac, 6), 0);
    EXPECT_EQ(frame[12], 0x86);
    EXPECT_EQ(frame[13], 0xdd);
    EXPECT_EQ(memcmp(frame + 14, packet.data(), packet.size()), 0);

    EXPECT_EQ(McastForwardPolicy::BuildEthernetFrame(packet.data(), packet.size(), mac, frame, 20), 0u);
}

TEST(McastDedupCache, SuppressesCopiesWithinTheWindow)
{
    McastDedupCache      cache(1000, 16);
    std::vector<uint8_t> packet = MakePacket("fd12::1", "ff05::abcd", 64, "hello");
    std::vector<uint8_t> other  = MakePacket("fd12::1", "ff05::abcd", 64, "hellp");
    uint64_t             fp     = McastDedupCache::Fingerprint(packet.data(), packet.size());

    EXPECT_FALSE(cache.Check(fp, At(0)));
    EXPECT_TRUE(cache.Check(fp, At(500)));
    EXPECT_NE(fp, McastDedupCache::Fingerprint(other.data(), other.size()));
    EXPECT_FALSE(cache.Check(McastDedupCache::Fingerprint(other.data(), other.size()), At(500)));

    // The same packet after the window is new again.
    EXPECT_FALSE(cache.Check(fp, At(1600)));
}

TEST(McastDedupCache, IgnoresTheHopLimit)
{
    std::vector<uint8_t> a = MakePacket("fd12::1", "ff05::abcd", 64, "hello");
    std::vector<uint8_t> b = MakePacket("fd12::1", "ff05::abcd", 63, "hello");

    EXPECT_EQ(McastDedupCache::Fingerprint(a.data(), a.size()), McastDedupCache::Fingerprint(b.data(), b.size()));
}

TEST(McastDedupCache, EvictsTheOldestWhenFull)
{
    McastDedupCache cache(10000, 4);

    for (uint64_t fp = 1; fp <= 4; fp++)
    {
        EXPECT_FALSE(cache.Check(fp, At(static_cast<uint32_t>(fp))));
    }
    EXPECT_EQ(cache.GetSize(), 4u);
    EXPECT_FALSE(cache.Check(5, At(10)));
    EXPECT_EQ(cache.GetSize(), 4u);
    EXPECT_FALSE(cache.Check(1, At(11))); // the oldest was evicted, so it is new again
    EXPECT_TRUE(cache.Check(5, At(12)));
}

TEST(TokenBucket, BurstsThenRefills)
{
    TokenBucket bucket(10, 3);

    EXPECT_TRUE(bucket.Take(At(0)));
    EXPECT_TRUE(bucket.Take(At(0)));
    EXPECT_TRUE(bucket.Take(At(0)));
    EXPECT_FALSE(bucket.Take(At(0)));
    EXPECT_FALSE(bucket.Take(At(50))); // half a token
    EXPECT_TRUE(bucket.Take(At(100)));
    EXPECT_FALSE(bucket.Take(At(100)));
    // Long idle: refills to the burst, not beyond.
    EXPECT_TRUE(bucket.Take(At(5000)));
    EXPECT_TRUE(bucket.Take(At(5000)));
    EXPECT_TRUE(bucket.Take(At(5000)));
    EXPECT_FALSE(bucket.Take(At(5000)));
}

TEST(McastRateLimiter, LimitsPerGroupAndOverall)
{
    McastRateLimiter::Config config;

    config.mPerGroupRatePerSecond = 10;
    config.mPerGroupBurst         = 2;
    config.mTotalRatePerSecond    = 100;
    config.mTotalBurst            = 3;
    config.mMaxGroups             = 2;

    McastRateLimiter limiter(config);
    Ip6Address       a("ff05::a");
    Ip6Address       b("ff05::b");
    Ip6Address       c("ff05::c");

    EXPECT_TRUE(limiter.Allow(a, At(0)));
    EXPECT_TRUE(limiter.Allow(a, At(0)));
    EXPECT_FALSE(limiter.Allow(a, At(0))); // group a exhausted; the total still has one left
    EXPECT_TRUE(limiter.Allow(b, At(0)));
    EXPECT_FALSE(limiter.Allow(b, At(0))); // total exhausted
    EXPECT_TRUE(limiter.Allow(b, At(100)));
    // A third group evicts the idlest tracked one and starts with a full bucket.
    EXPECT_TRUE(limiter.Allow(c, At(200)));
}

} // namespace
