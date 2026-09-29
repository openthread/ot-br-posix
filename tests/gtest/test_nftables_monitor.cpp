/*
 *  Copyright (c) 2026, The OpenThread Authors.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

#include <errno.h>
#include <netinet/in.h>

#include <vector>

#include <gtest/gtest.h>

#include <linux/netfilter.h>
#include <linux/netfilter/nf_tables.h>
#include <linux/netfilter/nfnetlink.h>
#include <linux/netlink.h>

#include <libnftnl/table.h>

#include "fake_netlink_socket.hpp"
#include "firewall/nftables_monitor.hpp"

using otbr::Firewall::FakeNetlinkSocket;
using otbr::Firewall::NftablesMonitor;

namespace {

constexpr uint32_t kOwnPortId   = 77;
constexpr uint32_t kOtherPortId = 99;

// A table event as the kernel announces it, from the socket with @p aPortId.
std::vector<uint8_t> TableEvent(uint16_t aMsgType, const char *aName, uint8_t aFamily, uint32_t aPortId)
{
    std::vector<uint8_t> buf(512, 0);
    struct nlmsghdr *nlh = nftnl_table_nlmsg_build_hdr(reinterpret_cast<char *>(buf.data()), aMsgType, aFamily, 0, 1);
    struct nftnl_table *table = nftnl_table_alloc();

    nftnl_table_set_str(table, NFTNL_TABLE_NAME, aName);
    nftnl_table_nlmsg_build_payload(nlh, table);
    nftnl_table_free(table);
    nlh->nlmsg_pid = aPortId;
    buf.resize(nlh->nlmsg_len);

    return buf;
}

class NftablesMonitorTest : public ::testing::Test
{
protected:
    void SetUp(void) override { ASSERT_EQ(mMonitor.Init(kOwnPortId), OTBR_ERROR_NONE); }

    int               mDeleted = 0;
    FakeNetlinkSocket mSocket;
    NftablesMonitor   mMonitor{mSocket, "otbr_wpan0", [this](void) { mDeleted++; }};
};

} // namespace

TEST_F(NftablesMonitorTest, SubscribesToTheNftablesEvents)
{
    EXPECT_TRUE(mSocket.IsOpen());
    EXPECT_EQ(mSocket.SubscribedGroup(), static_cast<uint32_t>(NFNLGRP_NFTABLES));
}

TEST_F(NftablesMonitorTest, ReportsTheDeletionOfItsTable)
{
    mSocket.QueueDatagram(TableEvent(NFT_MSG_DELTABLE, "otbr_wpan0", NFPROTO_INET, kOtherPortId));

    mMonitor.ProcessEvents();

    EXPECT_EQ(mDeleted, 1);
    EXPECT_EQ(mSocket.PendingReplies(), 0u);
}

TEST_F(NftablesMonitorTest, ReportsABurstOnce)
{
    // Two deletions before the loop got to read: one reinstall covers both.
    mSocket.QueueDatagram(TableEvent(NFT_MSG_DELTABLE, "otbr_wpan0", NFPROTO_INET, kOtherPortId));
    mSocket.QueueDatagram(TableEvent(NFT_MSG_DELTABLE, "otbr_wpan0", NFPROTO_INET, kOtherPortId));

    mMonitor.ProcessEvents();

    EXPECT_EQ(mDeleted, 1);
}

TEST_F(NftablesMonitorTest, IgnoresItsOwnDeletions)
{
    // What a reinstall, Init() or Deinit() announces.
    mSocket.QueueDatagram(TableEvent(NFT_MSG_DELTABLE, "otbr_wpan0", NFPROTO_INET, kOwnPortId));

    mMonitor.ProcessEvents();

    EXPECT_EQ(mDeleted, 0);
}

TEST_F(NftablesMonitorTest, IgnoresOtherTables)
{
    mSocket.QueueDatagram(TableEvent(NFT_MSG_DELTABLE, "fw4", NFPROTO_INET, kOtherPortId));
    mSocket.QueueDatagram(TableEvent(NFT_MSG_DELTABLE, "otbr_wpan1", NFPROTO_INET, kOtherPortId));
    mSocket.QueueDatagram(TableEvent(NFT_MSG_DELTABLE, "otbr_wpan0", NFPROTO_IPV6, kOtherPortId));

    mMonitor.ProcessEvents();

    EXPECT_EQ(mDeleted, 0);
}

TEST_F(NftablesMonitorTest, IgnoresOtherEvents)
{
    mSocket.QueueDatagram(TableEvent(NFT_MSG_NEWTABLE, "otbr_wpan0", NFPROTO_INET, kOtherPortId));

    mMonitor.ProcessEvents();

    EXPECT_EQ(mDeleted, 0);
}

TEST_F(NftablesMonitorTest, LostEventsCountAsADeletion)
{
    // The kernel dropped events; the deletion may have been among them.
    mSocket.QueueFailure(ENOBUFS);

    mMonitor.ProcessEvents();

    EXPECT_EQ(mDeleted, 1);
}

TEST_F(NftablesMonitorTest, AnOlderRecreationOfItsOwnDoesNotCancelLostEvents)
{
    // The kernel reports the overrun ahead of the events still queued, which
    // are older than the ones it dropped: a reinstall among them says nothing
    // about a deletion that came later and was lost.
    mSocket.QueueFailure(ENOBUFS);
    mSocket.QueueDatagram(TableEvent(NFT_MSG_NEWTABLE, "otbr_wpan0", NFPROTO_INET, kOwnPortId));

    mMonitor.ProcessEvents();

    EXPECT_EQ(mDeleted, 1);
}

TEST_F(NftablesMonitorTest, NothingQueuedReportsNothing)
{
    mMonitor.ProcessEvents();

    EXPECT_EQ(mDeleted, 0);
}

TEST_F(NftablesMonitorTest, InitTwiceFails)
{
    EXPECT_EQ(mMonitor.Init(kOwnPortId), OTBR_ERROR_INVALID_STATE);
}

TEST_F(NftablesMonitorTest, ItsOwnRecreationCancelsAnEarlierDeletion)
{
    // An update ran into ENOENT and reinstalled before the events were read;
    // the table is back, so there is nothing to report.
    mSocket.QueueDatagram(TableEvent(NFT_MSG_DELTABLE, "otbr_wpan0", NFPROTO_INET, kOtherPortId));
    mSocket.QueueDatagram(TableEvent(NFT_MSG_DELTABLE, "otbr_wpan0", NFPROTO_INET, kOwnPortId));
    mSocket.QueueDatagram(TableEvent(NFT_MSG_NEWTABLE, "otbr_wpan0", NFPROTO_INET, kOwnPortId));

    mMonitor.ProcessEvents();

    EXPECT_EQ(mDeleted, 0);
}

TEST_F(NftablesMonitorTest, ADeletionAfterItsOwnRecreationStillCounts)
{
    mSocket.QueueDatagram(TableEvent(NFT_MSG_NEWTABLE, "otbr_wpan0", NFPROTO_INET, kOwnPortId));
    mSocket.QueueDatagram(TableEvent(NFT_MSG_DELTABLE, "otbr_wpan0", NFPROTO_INET, kOtherPortId));

    mMonitor.ProcessEvents();

    EXPECT_EQ(mDeleted, 1);
}

TEST_F(NftablesMonitorTest, SomeoneElsesRecreationCancelsNothing)
{
    // Only the agent's own new table means the agent reinstalled.
    mSocket.QueueDatagram(TableEvent(NFT_MSG_DELTABLE, "otbr_wpan0", NFPROTO_INET, kOtherPortId));
    mSocket.QueueDatagram(TableEvent(NFT_MSG_NEWTABLE, "otbr_wpan0", NFPROTO_INET, kOtherPortId));

    mMonitor.ProcessEvents();

    EXPECT_EQ(mDeleted, 1);
}
