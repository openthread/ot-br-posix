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
#include <string>
#include <vector>

#include "host/posix/mcast_forwarder.hpp"

namespace {

using otbr::Ip6Address;
using otbr::McastForwarder;

// Builds an IPv6 packet from fe80::1 to aDestination carrying aPayload behind a
// Hop-by-Hop header with the Router Alert option, the way MLD messages travel.
std::vector<uint8_t> MakeMldPacket(const char                 *aDestination,
                                   const std::vector<uint8_t> &aPayload,
                                   bool                        aRouterAlert = true)
{
    std::vector<uint8_t> packet(40, 0);
    uint16_t             payloadLength = static_cast<uint16_t>(aPayload.size() + (aRouterAlert ? 8 : 0));

    packet[0]  = 0x60;
    packet[4]  = static_cast<uint8_t>(payloadLength >> 8);
    packet[5]  = static_cast<uint8_t>(payloadLength & 0xff);
    packet[6]  = aRouterAlert ? 0 : 58; // Hop-by-Hop or ICMPv6
    packet[7]  = 1;
    packet[8]  = 0xfe;
    packet[9]  = 0x80;
    packet[23] = 1;
    memcpy(&packet[24], Ip6Address(aDestination).m8, 16);

    if (aRouterAlert)
    {
        // next header ICMPv6, length 0 (8 bytes), Router Alert (5,2,0) + PadN(1,0)
        const uint8_t hopByHop[8] = {58, 0, 5, 2, 0, 0, 1, 0};

        packet.insert(packet.end(), hopByHop, hopByHop + 8);
    }
    packet.insert(packet.end(), aPayload.begin(), aPayload.end());

    return packet;
}

std::vector<uint8_t> Address(const char *aString)
{
    Ip6Address address(aString);

    return std::vector<uint8_t>(address.m8, address.m8 + 16);
}

void Append(std::vector<uint8_t> &aTo, const std::vector<uint8_t> &aBytes)
{
    aTo.insert(aTo.end(), aBytes.begin(), aBytes.end());
}

// One MLDv2 multicast address record.
std::vector<uint8_t> Record(uint8_t aType, const char *aGroup, const std::vector<const char *> &aSources = {})
{
    std::vector<uint8_t> record = {aType, 0, 0, static_cast<uint8_t>(aSources.size())};

    Append(record, Address(aGroup));
    for (const char *source : aSources)
    {
        Append(record, Address(source));
    }

    return record;
}

std::vector<uint8_t> Mldv2Report(const std::vector<std::vector<uint8_t>> &aRecords)
{
    std::vector<uint8_t> report = {143, 0, 0, 0, 0, 0, 0, static_cast<uint8_t>(aRecords.size())};

    for (const auto &record : aRecords)
    {
        Append(report, record);
    }

    return report;
}

TEST(McastForwarder, ParsesMldv1ReportAndDone)
{
    std::vector<McastForwarder::MldRecord> records;
    std::vector<uint8_t>                   report = {131, 0, 0, 0, 0, 0, 0, 0};
    std::vector<uint8_t>                   done   = {132, 0, 0, 0, 0, 0, 0, 0};

    Append(report, Address("ff05::abcd"));
    Append(done, Address("ff05::abcd"));

    EXPECT_TRUE(McastForwarder::ParseMld(MakeMldPacket("ff05::abcd", report).data(),
                                         MakeMldPacket("ff05::abcd", report).size(), records));
    ASSERT_EQ(records.size(), 1u);
    EXPECT_EQ(records[0].mGroup, Ip6Address("ff05::abcd"));
    EXPECT_TRUE(records[0].mIsJoin);

    std::vector<uint8_t> packet = MakeMldPacket("ff02::2", done);

    EXPECT_TRUE(McastForwarder::ParseMld(packet.data(), packet.size(), records));
    ASSERT_EQ(records.size(), 1u);
    EXPECT_EQ(records[0].mGroup, Ip6Address("ff05::abcd"));
    EXPECT_FALSE(records[0].mIsJoin);
}

TEST(McastForwarder, ParsesMldv2ReportRecordTypes)
{
    std::vector<McastForwarder::MldRecord> records;
    std::vector<uint8_t>                   packet =
        MakeMldPacket("ff02::16", Mldv2Report({
                                      Record(4, "ff05::1"),                  // CHANGE_TO_EXCLUDE: join
                                      Record(2, "ff05::2"),                  // MODE_IS_EXCLUDE: join
                                      Record(3, "ff05::3"),                  // CHANGE_TO_INCLUDE, no sources: leave
                                      Record(1, "ff05::4", {"2001:db8::1"}), // MODE_IS_INCLUDE with a source: join
                                      Record(1, "ff05::5"),                  // MODE_IS_INCLUDE, no sources: nothing
                                      Record(5, "ff05::6", {"2001:db8::2"}), // ALLOW_NEW_SOURCES: join
                                      Record(6, "ff05::7", {"2001:db8::3"}), // BLOCK_OLD_SOURCES: nothing
                                      Record(6, "ff05::a"),                  // empty BLOCK: leave (macOS)
                                      Record(4, "ff0e::8"),                  // global scope join
                                  }));

    EXPECT_TRUE(McastForwarder::ParseMld(packet.data(), packet.size(), records));
    ASSERT_EQ(records.size(), 7u);
    EXPECT_EQ(records[0].mGroup, Ip6Address("ff05::1"));
    EXPECT_TRUE(records[0].mIsJoin);
    EXPECT_EQ(records[1].mGroup, Ip6Address("ff05::2"));
    EXPECT_TRUE(records[1].mIsJoin);
    EXPECT_EQ(records[2].mGroup, Ip6Address("ff05::3"));
    EXPECT_FALSE(records[2].mIsJoin);
    EXPECT_EQ(records[3].mGroup, Ip6Address("ff05::4"));
    EXPECT_TRUE(records[3].mIsJoin);
    EXPECT_EQ(records[4].mGroup, Ip6Address("ff05::6"));
    EXPECT_TRUE(records[4].mIsJoin);
    EXPECT_EQ(records[5].mGroup, Ip6Address("ff05::a"));
    EXPECT_FALSE(records[5].mIsJoin);
    EXPECT_EQ(records[6].mGroup, Ip6Address("ff0e::8"));
    EXPECT_TRUE(records[6].mIsJoin);
}

TEST(McastForwarder, ParsesMldWithoutRouterAlert)
{
    std::vector<McastForwarder::MldRecord> records;
    std::vector<uint8_t>                   packet =
        MakeMldPacket("ff02::16", Mldv2Report({Record(4, "ff05::9")}), /* aRouterAlert */ false);

    EXPECT_TRUE(McastForwarder::ParseMld(packet.data(), packet.size(), records));
    ASSERT_EQ(records.size(), 1u);
    EXPECT_EQ(records[0].mGroup, Ip6Address("ff05::9"));
}

TEST(McastForwarder, RejectsNonMldAndTruncatedPackets)
{
    std::vector<McastForwarder::MldRecord> records;
    std::vector<uint8_t>                   echo = {128, 0, 0, 0, 0, 0, 0, 0};
    std::vector<uint8_t>                   packet;

    packet = MakeMldPacket("ff02::1", echo);
    EXPECT_FALSE(McastForwarder::ParseMld(packet.data(), packet.size(), records));
    EXPECT_TRUE(records.empty());

    // A UDP packet
    packet    = MakeMldPacket("ff05::abcd", {0, 0, 0, 0, 0, 8, 0, 0}, false);
    packet[6] = 17;
    EXPECT_FALSE(McastForwarder::ParseMld(packet.data(), packet.size(), records));

    // An MLDv2 report announcing two records but carrying one
    packet             = MakeMldPacket("ff02::16", Mldv2Report({Record(4, "ff05::1")}));
    packet[40 + 8 + 7] = 2;
    EXPECT_FALSE(McastForwarder::ParseMld(packet.data(), packet.size(), records));
    EXPECT_TRUE(records.empty());

    // Truncated MLDv1 report
    packet = MakeMldPacket("ff05::abcd", {131, 0, 0, 0, 0, 0, 0, 0, 0xff, 0x05});
    EXPECT_FALSE(McastForwarder::ParseMld(packet.data(), packet.size(), records));

    // Too short for an IPv6 header
    EXPECT_FALSE(McastForwarder::ParseMld(packet.data(), 20, records));
}

TEST(McastForwarder, AppliesEveryTrackedRecordOfAReport)
{
    McastForwarder forwarder("utun0", "");

    // A state-change report batches link-local groups with the interesting ones.
    forwarder.HandleMldRecords({{Ip6Address("ff02::1:ff00:1"), true},
                                {Ip6Address("ff05::beef"), true},
                                {Ip6Address("ff02::fb"), true},
                                {Ip6Address("ff0e::1"), true}});
    EXPECT_TRUE(forwarder.HasBackboneListener(Ip6Address("ff05::beef")));
    EXPECT_TRUE(forwarder.HasBackboneListener(Ip6Address("ff0e::1")));
    EXPECT_FALSE(forwarder.HasBackboneListener(Ip6Address("ff02::fb")));

    forwarder.HandleMldRecords({{Ip6Address("ff02::1:ff00:1"), false}, {Ip6Address("ff05::beef"), false}});
    EXPECT_FALSE(forwarder.HasBackboneListener(Ip6Address("ff05::beef")));
    EXPECT_TRUE(forwarder.HasBackboneListener(Ip6Address("ff0e::1")));
}

TEST(McastForwarder, TracksOnlyMulticastBeyondLinkLocal)
{
    EXPECT_TRUE(McastForwarder::IsTrackedGroup(Ip6Address("ff05::abcd")));
    EXPECT_TRUE(McastForwarder::IsTrackedGroup(Ip6Address("ff03::fc")));
    EXPECT_TRUE(McastForwarder::IsTrackedGroup(Ip6Address("ff0e::1")));
    EXPECT_FALSE(McastForwarder::IsTrackedGroup(Ip6Address("ff02::1")));
    EXPECT_FALSE(McastForwarder::IsTrackedGroup(Ip6Address("ff01::1")));
    EXPECT_FALSE(McastForwarder::IsTrackedGroup(Ip6Address("fd00::1")));
}

// A minimal IPv6/UDP packet from the mesh.
std::vector<uint8_t> MeshPacket(const char *aSource, const char *aGroup, uint8_t aHopLimit, const char *aPayload)
{
    std::vector<uint8_t> packet(40, 0);
    size_t               payloadLength = 8 + strlen(aPayload);

    packet[0] = 0x60;
    packet[4] = static_cast<uint8_t>(payloadLength >> 8);
    packet[5] = static_cast<uint8_t>(payloadLength & 0xff);
    packet[6] = 17;
    packet[7] = aHopLimit;
    memcpy(&packet[8], Ip6Address(aSource).m8, 16);
    memcpy(&packet[24], Ip6Address(aGroup).m8, 16);
    packet.resize(40 + payloadLength, 0);
    memcpy(&packet[48], aPayload, strlen(aPayload));

    return packet;
}

TEST(McastForwarder, ThreadPacketPipelineCountsEveryOutcome)
{
    McastForwarder                  forwarder("utun0", "");
    otbr::Timepoint                 now = otbr::Clock::now();
    std::vector<uint8_t>            packet;
    const McastForwarder::Counters &c = forwarder.GetThreadToBackboneCounters();

    // Not forwardable: link-local destination, link-local source, exhausted hop limit.
    packet = MeshPacket("fd12::1", "ff02::1", 64, "a");
    forwarder.HandleThreadPacket(packet.data(), packet.size(), now);
    packet = MeshPacket("fe80::1", "ff05::1", 64, "a");
    forwarder.HandleThreadPacket(packet.data(), packet.size(), now);
    packet = MeshPacket("fd12::1", "ff05::1", 1, "a");
    forwarder.HandleThreadPacket(packet.data(), packet.size(), now);
    EXPECT_EQ(c.mReceived, 3u);
    EXPECT_EQ(c.mRejected, 3u);

    // Forwardable, but no backbone tap is attached: reaches the write and fails there.
    packet = MeshPacket("fd12::1", "ff05::abcd", 64, "hello");
    forwarder.HandleThreadPacket(packet.data(), packet.size(), now);
    EXPECT_EQ(c.mErrors, 1u);
    EXPECT_EQ(c.mForwarded, 0u);

    // The same packet again (even with another hop limit) is a duplicate, not another write.
    packet[7] = 60;
    forwarder.HandleThreadPacket(packet.data(), packet.size(), now + otbr::Milliseconds(100));
    EXPECT_EQ(c.mDuplicates, 1u);
    EXPECT_EQ(c.mErrors, 1u);

    // A flood of distinct packets to one group hits the per-group limit (40 burst).
    for (int i = 0; i < 100; i++)
    {
        std::string payload = "flood-" + std::to_string(i);

        packet = MeshPacket("fd12::1", "ff05::abcd", 64, payload.c_str());
        forwarder.HandleThreadPacket(packet.data(), packet.size(), now + otbr::Milliseconds(200));
    }
    EXPECT_EQ(c.mRateLimited, 100u - 40u); // burst 40: the 200 ms since the first packet refilled the token it spent
    EXPECT_EQ(c.mErrors, 1u + 40u);
    EXPECT_EQ(c.mReceived, 105u);
}

TEST(McastForwarder, BackbonePacketPipelineInjectsOnlyForThreadListeners)
{
    McastForwarder                  forwarder("utun0", "");
    otbr::Timepoint                 now = otbr::Clock::now();
    std::vector<uint8_t>            packet;
    const McastForwarder::Counters &c = forwarder.GetBackboneToThreadCounters();

    // Policy first: a link-local group never enters the mesh.
    packet = MeshPacket("fd00:1::1", "ff02::1", 64, "a");
    forwarder.HandleBackbonePacket(packet.data(), packet.size(), now);
    EXPECT_EQ(c.mRejected, 1u);

    // No Thread device registered for the group: nothing is injected.
    packet = MeshPacket("fd00:1::1", "ff05::abcd", 64, "hello");
    forwarder.HandleBackbonePacket(packet.data(), packet.size(), now);
    EXPECT_EQ(c.mNoListener, 1u);
    EXPECT_EQ(c.mErrors, 0u);

    // With a listener the packet reaches the write (and fails there: no tap).
    forwarder.HandleBackboneMulticastListenerEvent(OT_BACKBONE_ROUTER_MULTICAST_LISTENER_ADDED,
                                                   Ip6Address("ff05::abcd"));
    forwarder.HandleBackbonePacket(packet.data(), packet.size(), now);
    EXPECT_EQ(c.mErrors, 1u);
    EXPECT_EQ(c.mForwarded, 0u);

    // The same packet again is a duplicate.
    forwarder.HandleBackbonePacket(packet.data(), packet.size(), now + otbr::Milliseconds(100));
    EXPECT_EQ(c.mDuplicates, 1u);

    // A packet the forwarder itself just emitted towards the backbone comes
    // back through the backbone tap (see-sent): the duplicate cache catches
    // it, so the mesh never sees its own traffic again.
    packet = MeshPacket("fd12::1", "ff05::abcd", 64, "from-the-mesh");
    forwarder.HandleThreadPacket(packet.data(), packet.size(), now + otbr::Milliseconds(200));
    packet[7] = 63;
    forwarder.HandleBackbonePacket(packet.data(), packet.size(), now + otbr::Milliseconds(201));
    EXPECT_EQ(c.mDuplicates, 2u);
    EXPECT_EQ(c.mErrors, 1u);

    // A flood of distinct packets is held to the tighter mesh-bound limit (20 burst).
    for (int i = 0; i < 100; i++)
    {
        std::string payload = "flood-" + std::to_string(i);

        packet = MeshPacket("fd00:1::1", "ff05::abcd", 64, payload.c_str());
        forwarder.HandleBackbonePacket(packet.data(), packet.size(), now + otbr::Milliseconds(300));
    }
    EXPECT_EQ(c.mRateLimited, 100u - 20u);
    EXPECT_EQ(c.mErrors, 1u + 20u);

    // Once the listener is gone, the group is closed again.
    forwarder.HandleBackboneMulticastListenerEvent(OT_BACKBONE_ROUTER_MULTICAST_LISTENER_REMOVED,
                                                   Ip6Address("ff05::abcd"));
    packet = MeshPacket("fd00:1::1", "ff05::abcd", 64, "late");
    forwarder.HandleBackbonePacket(packet.data(), packet.size(), now + otbr::Milliseconds(400));
    EXPECT_EQ(c.mNoListener, 2u);
}

} // namespace
