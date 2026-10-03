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

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <net/if.h>

#include <string>
#include <vector>

#include "host/rcp_host.hpp"
#include "mdns/mdns.hpp"
#include "trel_dnssd/trel_dnssd.hpp"

#include "mock_mdns_publisher.hpp"

#if OTBR_ENABLE_TREL_DNSSD

using ::testing::_;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::StrEq;

static const char kTrelServiceType[] = "_trel._udp";
static const char kNoNetifMessage[]  = "found no network interface";
static const char kOneNetifMessage[] = "found fewer than two network interfaces";

class TrelDnssdTest : public ::testing::Test
{
protected:
    TrelDnssdTest(void)
        : mHost("wpan0",
                std::vector<const char *>(),
                /* aBackboneInterfaceName */ "",
                /* aDryRun */ false,
                /* aEnableAutoAttach */ false)
        , mTrelDnssd(mHost, mPublisher)
    {
    }

    void SetUp(void) override
    {
        struct if_nameindex *netifs = if_nameindex();

        for (struct if_nameindex *netif = netifs; netif != nullptr && netif->if_index != 0; netif++)
        {
            mNetifNames.push_back(netif->if_name);
            mNetifIndexes.push_back(netif->if_index);
        }

        if (netifs != nullptr)
        {
            if_freenameindex(netifs);
        }
    }

    // Tells whether this host has at least `aCount` network interfaces.
    bool HasNetifs(size_t aCount) const { return mNetifNames.size() >= aCount; }

    void SetPublisherReady(void) { mTrelDnssd.HandleMdnsState(otbr::Mdns::Publisher::State::kReady); }
    void SetPublisherIdle(void) { mTrelDnssd.HandleMdnsState(otbr::Mdns::Publisher::State::kIdle); }

    void ExpectSubscribe(uint32_t aNetifIndex)
    {
        EXPECT_CALL(mPublisher, SubscribeService(StrEq(kTrelServiceType), StrEq(""), aNetifIndex)).Times(1);
    }

    void ExpectUnsubscribe(uint32_t aNetifIndex)
    {
        EXPECT_CALL(mPublisher, UnsubscribeService(StrEq(kTrelServiceType), StrEq(""), aNetifIndex)).Times(1);
    }

    void ExpectNoSubscribeOrUnsubscribe(void)
    {
        EXPECT_CALL(mPublisher, SubscribeService(_, _, _)).Times(0);
        EXPECT_CALL(mPublisher, UnsubscribeService(_, _, _)).Times(0);
    }

    void VerifyExpectations(void) { Mock::VerifyAndClearExpectations(&mPublisher); }

    // The names and indexes of the network interfaces of this host, in any state.
    std::vector<std::string> mNetifNames;
    std::vector<uint32_t>    mNetifIndexes;

    // The host is not initialized: browsing uses the publisher only.
    otbr::Host::RcpHost                      mHost;
    ::testing::StrictMock<MockMdnsPublisher> mPublisher;
    otbr::TrelDnssd::TrelDnssd               mTrelDnssd;
};

TEST_F(TrelDnssdTest, BrowseStartedWhenReadySubscribesOnTheTrelNetif)
{
    if (!HasNetifs(1))
    {
        GTEST_SKIP() << kNoNetifMessage;
    }

    // 1. Nothing is subscribed before the browse starts.
    ExpectNoSubscribeOrUnsubscribe();
    SetPublisherReady();
    mTrelDnssd.Initialize(mNetifNames[0]);
    VerifyExpectations();

    // 2. The browse subscribes on the TREL interface.
    ExpectSubscribe(mNetifIndexes[0]);
    mTrelDnssd.StartBrowse();
    VerifyExpectations();

    // 3. Stopping the browse unsubscribes with the same interface index.
    ExpectUnsubscribe(mNetifIndexes[0]);
    mTrelDnssd.StopBrowse();
    VerifyExpectations();
}

TEST_F(TrelDnssdTest, BrowseStartedBeforeThePublisherIsReadySubscribesWhenItIs)
{
    if (!HasNetifs(1))
    {
        GTEST_SKIP() << kNoNetifMessage;
    }

    // 1. The publisher is not ready: nothing is subscribed.
    ExpectNoSubscribeOrUnsubscribe();
    mTrelDnssd.Initialize(mNetifNames[0]);
    mTrelDnssd.StartBrowse();
    VerifyExpectations();

    // 2. The publisher becomes ready.
    ExpectSubscribe(mNetifIndexes[0]);
    SetPublisherReady();
    VerifyExpectations();

    ExpectUnsubscribe(mNetifIndexes[0]);
    mTrelDnssd.StopBrowse();
    VerifyExpectations();
}

TEST_F(TrelDnssdTest, BrowseStoppedBeforeTheTrelNetifIsReadyDoesNotUnsubscribe)
{
    // The interface does not exist: nothing is subscribed, so there is nothing to unsubscribe.
    ExpectNoSubscribeOrUnsubscribe();
    SetPublisherReady();
    mTrelDnssd.Initialize("otbr-no-netif");
    mTrelDnssd.StartBrowse();
    mTrelDnssd.StopBrowse();
    VerifyExpectations();
}

TEST_F(TrelDnssdTest, PublisherRestartSubscribesAgainWithoutUnsubscribing)
{
    if (!HasNetifs(1))
    {
        GTEST_SKIP() << kNoNetifMessage;
    }

    ExpectSubscribe(mNetifIndexes[0]);
    SetPublisherReady();
    mTrelDnssd.Initialize(mNetifNames[0]);
    mTrelDnssd.StartBrowse();
    VerifyExpectations();

    // 1. The publisher stops: nothing is subscribed or unsubscribed.
    ExpectNoSubscribeOrUnsubscribe();
    SetPublisherIdle();
    VerifyExpectations();

    // 2. A publisher that becomes ready again has lost its subscriptions: the browse subscribes again.
    ExpectSubscribe(mNetifIndexes[0]);
    SetPublisherReady();
    VerifyExpectations();

    ExpectUnsubscribe(mNetifIndexes[0]);
    mTrelDnssd.StopBrowse();
    VerifyExpectations();
}

TEST_F(TrelDnssdTest, InitializeOnAnotherNetifMovesTheSubscription)
{
    if (!HasNetifs(2))
    {
        GTEST_SKIP() << kOneNetifMessage;
    }

    ExpectSubscribe(mNetifIndexes[0]);
    SetPublisherReady();
    mTrelDnssd.Initialize(mNetifNames[0]);
    mTrelDnssd.StartBrowse();
    VerifyExpectations();

    // 1. The subscription of the old interface is removed, then the new interface is subscribed.
    {
        InSequence sequence;

        ExpectUnsubscribe(mNetifIndexes[0]);
        ExpectSubscribe(mNetifIndexes[1]);
        mTrelDnssd.Initialize(mNetifNames[1]);
    }
    VerifyExpectations();

    // 2. Stopping the browse unsubscribes with the index of the new interface.
    ExpectUnsubscribe(mNetifIndexes[1]);
    mTrelDnssd.StopBrowse();
    VerifyExpectations();
}

TEST_F(TrelDnssdTest, InitializeOnTheSameNetifSubscribesAgainOnce)
{
    if (!HasNetifs(1))
    {
        GTEST_SKIP() << kNoNetifMessage;
    }

    ExpectSubscribe(mNetifIndexes[0]);
    SetPublisherReady();
    mTrelDnssd.Initialize(mNetifNames[0]);
    mTrelDnssd.StartBrowse();
    VerifyExpectations();

    {
        InSequence sequence;

        ExpectUnsubscribe(mNetifIndexes[0]);
        ExpectSubscribe(mNetifIndexes[0]);
        mTrelDnssd.Initialize(mNetifNames[0]);
    }
    VerifyExpectations();

    ExpectUnsubscribe(mNetifIndexes[0]);
    mTrelDnssd.StopBrowse();
    VerifyExpectations();
}

TEST_F(TrelDnssdTest, InitializeOnAMissingNetifRemovesTheSubscription)
{
    if (!HasNetifs(1))
    {
        GTEST_SKIP() << kNoNetifMessage;
    }

    ExpectSubscribe(mNetifIndexes[0]);
    SetPublisherReady();
    mTrelDnssd.Initialize(mNetifNames[0]);
    mTrelDnssd.StartBrowse();
    VerifyExpectations();

    // 1. The new interface does not exist: the old subscription is removed and none is made.
    ExpectUnsubscribe(mNetifIndexes[0]);
    mTrelDnssd.Initialize("otbr-no-netif");
    VerifyExpectations();

    // 2. Nothing is subscribed, so stopping the browse unsubscribes nothing.
    ExpectNoSubscribeOrUnsubscribe();
    mTrelDnssd.StopBrowse();
    VerifyExpectations();
}

#endif // OTBR_ENABLE_TREL_DNSSD
