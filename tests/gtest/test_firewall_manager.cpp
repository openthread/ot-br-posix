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

#include "common/types.hpp"
#include "firewall/firewall_manager.hpp"
#include "firewall/nftables.hpp"

#if OTBR_ENABLE_NFTABLES
#include "fake_netlink_socket.hpp"
#include "firewall/netlink_socket.hpp"
#include "firewall/nftables_impl.hpp"
#endif

using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::StrEq;

using otbr::Ip6Prefix;
using otbr::Firewall::ChainType;
using otbr::Firewall::FirewallManager;
using otbr::Firewall::Hook;
using otbr::Firewall::INftables;
using otbr::Firewall::Ip6Net;
using otbr::Firewall::PktType;
using otbr::Firewall::SetDirection;

#if OTBR_ENABLE_NFTABLES
using otbr::Firewall::FakeNetlinkSocket;
using otbr::Firewall::MnlNetlinkSocket;
using otbr::Firewall::Nftables;
#endif
using otbr::Firewall::Verdict;

namespace {

class MockNftables : public INftables
{
public:
    MOCK_METHOD(otbrError, Init, (), (override));
    MOCK_METHOD(otbrError, Deinit, (), (override));
    MOCK_METHOD(otbrError, BeginBatch, (), (override));
    MOCK_METHOD(otbrError, CommitBatch, (), (override));
    MOCK_METHOD(void, AbortBatch, (), (override));
    MOCK_METHOD(otbrError, DelTable, (const std::string &), (override));
    MOCK_METHOD(otbrError, AddTable, (const std::string &), (override));
    MOCK_METHOD(otbrError, AddChain, (const std::string &, const std::string &, Hook, int, ChainType), (override));
    MOCK_METHOD(otbrError, AddIp6PrefixSet, (const std::string &, const std::string &), (override));
    MOCK_METHOD(otbrError, AddSetElement, (const std::string &, const std::string &, const Ip6Net &), (override));
    MOCK_METHOD(otbrError, DelSetElement, (const std::string &, const std::string &, const Ip6Net &), (override));
    MOCK_METHOD(otbrError, FlushSet, (const std::string &, const std::string &), (override));
    MOCK_METHOD(otbrError,
                AddRuleOifnameNeqReturn,
                (const std::string &, const std::string &, const std::string &, uint64_t *),
                (override));
    MOCK_METHOD(otbrError,
                AddRuleIifPkttypeVerdict,
                (const std::string &, const std::string &, const std::string &, PktType, Verdict, uint64_t *),
                (override));
    MOCK_METHOD(otbrError,
                AddRulePkttypeVerdict,
                (const std::string &, const std::string &, PktType, Verdict, uint64_t *),
                (override));
    MOCK_METHOD(otbrError,
                AddRuleSetLookupVerdict,
                (const std::string &, const std::string &, const std::string &, SetDirection, Verdict, uint64_t *),
                (override));
    MOCK_METHOD(otbrError, AddRuleVerdict, (const std::string &, const std::string &, Verdict, uint64_t *), (override));
    MOCK_METHOD(otbrError,
                AddRuleNdNsRedirect,
                (const std::string &, const std::string &, const Ip6Net &, const std::string &, uint16_t, uint64_t *),
                (override));
    MOCK_METHOD(otbrError,
                AddRuleIifMark,
                (const std::string &, const std::string &, const std::string &, uint32_t, uint64_t *),
                (override));
    MOCK_METHOD(otbrError,
                AddRuleMarkMasquerade,
                (const std::string &, const std::string &, uint32_t, uint64_t *),
                (override));
    MOCK_METHOD(otbrError,
                AddRuleOifnameVerdict,
                (const std::string &, const std::string &, const std::string &, Verdict, uint64_t *),
                (override));
    MOCK_METHOD(otbrError,
                AddRuleIifnameVerdict,
                (const std::string &, const std::string &, const std::string &, Verdict, uint64_t *),
                (override));
    MOCK_METHOD(otbrError, DelRule, (const std::string &, const std::string &, uint64_t), (override));
};

void SetSuccessfulDefaults(MockNftables &mock)
{
    using ::testing::DoAll;
    using ::testing::Return;

    ON_CALL(mock, DelTable).WillByDefault(Return(OTBR_ERROR_NONE));
    ON_CALL(mock, AddTable).WillByDefault(Return(OTBR_ERROR_NONE));
    ON_CALL(mock, AddChain).WillByDefault(Return(OTBR_ERROR_NONE));
    ON_CALL(mock, AddIp6PrefixSet).WillByDefault(Return(OTBR_ERROR_NONE));
    ON_CALL(mock, AddSetElement).WillByDefault(Return(OTBR_ERROR_NONE));
    ON_CALL(mock, DelSetElement).WillByDefault(Return(OTBR_ERROR_NONE));
    ON_CALL(mock, FlushSet).WillByDefault(Return(OTBR_ERROR_NONE));
    ON_CALL(mock, BeginBatch).WillByDefault(Return(OTBR_ERROR_NONE));
    ON_CALL(mock, CommitBatch).WillByDefault(Return(OTBR_ERROR_NONE));
    ON_CALL(mock, DelRule).WillByDefault(Return(OTBR_ERROR_NONE));

    ON_CALL(mock, AddRuleOifnameNeqReturn).WillByDefault(Return(OTBR_ERROR_NONE));
    ON_CALL(mock, AddRuleIifPkttypeVerdict).WillByDefault(Return(OTBR_ERROR_NONE));
    ON_CALL(mock, AddRulePkttypeVerdict).WillByDefault(Return(OTBR_ERROR_NONE));
    ON_CALL(mock, AddRuleSetLookupVerdict).WillByDefault(Return(OTBR_ERROR_NONE));
    ON_CALL(mock, AddRuleVerdict).WillByDefault(Return(OTBR_ERROR_NONE));
    ON_CALL(mock, AddRuleNdNsRedirect).WillByDefault(DoAll(SetArgPointee<5>(200), Return(OTBR_ERROR_NONE)));
    ON_CALL(mock, AddRuleIifMark).WillByDefault(Return(OTBR_ERROR_NONE));
    ON_CALL(mock, AddRuleMarkMasquerade).WillByDefault(Return(OTBR_ERROR_NONE));
    ON_CALL(mock, AddRuleOifnameVerdict).WillByDefault(Return(OTBR_ERROR_NONE));
    ON_CALL(mock, AddRuleIifnameVerdict).WillByDefault(Return(OTBR_ERROR_NONE));
}

} // namespace

TEST(FirewallManagerTest, InitOnlyCreatesTable)
{
    NiceMock<MockNftables> mock;

    {
        // The delete must be inside the batch: nf_tables rejects a table
        // change sent on its own with EINVAL.
        InSequence seq;
        EXPECT_CALL(mock, BeginBatch()).WillOnce(Return(OTBR_ERROR_NONE));
        EXPECT_CALL(mock, DelTable(StrEq(FirewallManager::kTableName))).WillOnce(Return(OTBR_ERROR_NONE));
        EXPECT_CALL(mock, AddTable(StrEq(FirewallManager::kTableName))).WillOnce(Return(OTBR_ERROR_NONE));
        EXPECT_CALL(mock, CommitBatch()).WillOnce(Return(OTBR_ERROR_NONE));
    }
    // Init must NOT touch any chains, sets, or rules — those are lazy.
    EXPECT_CALL(mock, AddChain(_, _, _, _, _)).Times(0);
    EXPECT_CALL(mock, AddIp6PrefixSet(_, _)).Times(0);
    EXPECT_CALL(mock, AddRuleOifnameNeqReturn(_, _, _, _)).Times(0);

    FirewallManager fw(mock, "wpan0");
    EXPECT_EQ(fw.Init(), OTBR_ERROR_NONE);
    EXPECT_TRUE(fw.IsInitialized());
    EXPECT_FALSE(fw.IsIngressFilterEnabled());
}

TEST(FirewallManagerTest, InitTwiceFails)
{
    NiceMock<MockNftables> mock;
    SetSuccessfulDefaults(mock);

    FirewallManager fw(mock, "wpan0");
    ASSERT_EQ(fw.Init(), OTBR_ERROR_NONE);
    EXPECT_EQ(fw.Init(), OTBR_ERROR_INVALID_STATE);
}

TEST(FirewallManagerTest, InitPropagatesBackendError)
{
    NiceMock<MockNftables> mock;
    SetSuccessfulDefaults(mock);
    EXPECT_CALL(mock, BeginBatch()).WillOnce(Return(OTBR_ERROR_ERRNO));

    FirewallManager fw(mock, "wpan0");
    EXPECT_EQ(fw.Init(), OTBR_ERROR_ERRNO);
    EXPECT_FALSE(fw.IsInitialized());
}

TEST(FirewallManagerTest, DeinitDeletesTable)
{
    NiceMock<MockNftables> mock;
    SetSuccessfulDefaults(mock);
    EXPECT_CALL(mock, DelTable(StrEq(FirewallManager::kTableName))).Times(2); // once in Init, once in Deinit

    {
        // Deinit deletes the table in a batch of its own.
        InSequence seq;
        EXPECT_CALL(mock, BeginBatch()).WillOnce(Return(OTBR_ERROR_NONE)); // Init
        EXPECT_CALL(mock, CommitBatch()).WillOnce(Return(OTBR_ERROR_NONE));
        EXPECT_CALL(mock, BeginBatch()).WillOnce(Return(OTBR_ERROR_NONE)); // Deinit
        EXPECT_CALL(mock, CommitBatch()).WillOnce(Return(OTBR_ERROR_NONE));
    }

    FirewallManager fw(mock, "wpan0");
    ASSERT_EQ(fw.Init(), OTBR_ERROR_NONE);
    EXPECT_EQ(fw.Deinit(), OTBR_ERROR_NONE);
    EXPECT_FALSE(fw.IsInitialized());
}

TEST(FirewallManagerTest, EnableIngressFilterInstallsChainAndRulesInOrder)
{
    NiceMock<MockNftables> mock;
    SetSuccessfulDefaults(mock);

    // BeginBatch/CommitBatch deliberately omitted from this InSequence — they
    // are also called by Init(), and including them in the sequence would
    // cross-couple unrelated expectations.
    {
        InSequence seq;
        EXPECT_CALL(mock,
                    AddIp6PrefixSet(StrEq(FirewallManager::kTableName), StrEq(FirewallManager::kIngressDenySrcSet)))
            .Times(1);
        EXPECT_CALL(mock,
                    AddIp6PrefixSet(StrEq(FirewallManager::kTableName), StrEq(FirewallManager::kIngressAllowDstSet)))
            .Times(1);
        EXPECT_CALL(mock, AddChain(StrEq(FirewallManager::kTableName), StrEq(FirewallManager::kIngressChain),
                                   Hook::kForward, otbr::Firewall::kPriorityFilter, ChainType::kFilter))
            .Times(1);
        EXPECT_CALL(mock, AddRuleOifnameNeqReturn(StrEq(FirewallManager::kTableName),
                                                  StrEq(FirewallManager::kIngressChain), StrEq("wpan0"), _))
            .Times(1);
        EXPECT_CALL(mock, AddRuleIifPkttypeVerdict(_, StrEq(FirewallManager::kIngressChain), StrEq("wpan0"),
                                                   PktType::kUnicast, Verdict::kDrop, _))
            .Times(1);
        EXPECT_CALL(mock, AddRuleSetLookupVerdict(_, StrEq(FirewallManager::kIngressChain),
                                                  StrEq(FirewallManager::kIngressDenySrcSet), SetDirection::kSrc,
                                                  Verdict::kDrop, _))
            .Times(1);
        EXPECT_CALL(mock, AddRuleSetLookupVerdict(_, StrEq(FirewallManager::kIngressChain),
                                                  StrEq(FirewallManager::kIngressAllowDstSet), SetDirection::kDst,
                                                  Verdict::kAccept, _))
            .Times(1);
        EXPECT_CALL(
            mock, AddRulePkttypeVerdict(_, StrEq(FirewallManager::kIngressChain), PktType::kUnicast, Verdict::kDrop, _))
            .Times(1);
        EXPECT_CALL(mock, AddRuleVerdict(_, StrEq(FirewallManager::kIngressChain), Verdict::kAccept, _)).Times(1);
    }

    FirewallManager fw(mock, "wpan0");
    ASSERT_EQ(fw.Init(), OTBR_ERROR_NONE);
    EXPECT_EQ(fw.EnableIngressFilter(), OTBR_ERROR_NONE);
    EXPECT_TRUE(fw.IsIngressFilterEnabled());
}

TEST(FirewallManagerTest, EnableIngressFilterTwiceIsIdempotent)
{
    NiceMock<MockNftables> mock;
    SetSuccessfulDefaults(mock);
    EXPECT_CALL(mock, AddChain(_, StrEq(FirewallManager::kIngressChain), _, _, _)).Times(1);

    FirewallManager fw(mock, "wpan0");
    ASSERT_EQ(fw.Init(), OTBR_ERROR_NONE);
    ASSERT_EQ(fw.EnableIngressFilter(), OTBR_ERROR_NONE);
    EXPECT_EQ(fw.EnableIngressFilter(), OTBR_ERROR_NONE); // second call: no-op.
}

TEST(FirewallManagerTest, EnableNdProxyLazilyCreatesPreroutingChainOnFirstCall)
{
    NiceMock<MockNftables> mock;
    SetSuccessfulDefaults(mock);

    EXPECT_CALL(mock, AddChain(StrEq(FirewallManager::kTableName), StrEq(FirewallManager::kPreroutingChain),
                               Hook::kPrerouting, otbr::Firewall::kPriorityRaw, ChainType::kFilter))
        .Times(1);
    EXPECT_CALL(mock, AddRuleNdNsRedirect(StrEq(FirewallManager::kTableName), StrEq(FirewallManager::kPreroutingChain),
                                          _, StrEq("eth0"), 88, _))
        .Times(2)
        .WillOnce(DoAll(SetArgPointee<5>(42), Return(OTBR_ERROR_NONE)))
        .WillOnce(DoAll(SetArgPointee<5>(43), Return(OTBR_ERROR_NONE)));
    EXPECT_CALL(mock, DelRule(_, StrEq(FirewallManager::kPreroutingChain), 42)).Times(1);

    FirewallManager fw(mock, "wpan0");
    ASSERT_EQ(fw.Init(), OTBR_ERROR_NONE);

    Ip6Prefix domainPrefix("fd00::", 64);
    EXPECT_EQ(fw.EnableNdProxyRedirect(domainPrefix, "eth0", 88), OTBR_ERROR_NONE);
    // Second call must NOT recreate the chain, but must replace the previous rule (handle 42).
    EXPECT_EQ(fw.EnableNdProxyRedirect(domainPrefix, "eth0", 88), OTBR_ERROR_NONE);
}

TEST(FirewallManagerTest, DisableNdProxyDeletesRule)
{
    NiceMock<MockNftables> mock;
    SetSuccessfulDefaults(mock);

    EXPECT_CALL(mock, AddRuleNdNsRedirect(_, _, _, _, _, _))
        .WillOnce(DoAll(SetArgPointee<5>(99), Return(OTBR_ERROR_NONE)));
    EXPECT_CALL(mock, DelRule(StrEq(FirewallManager::kTableName), StrEq(FirewallManager::kPreroutingChain), 99))
        .Times(1);

    FirewallManager fw(mock, "wpan0");
    ASSERT_EQ(fw.Init(), OTBR_ERROR_NONE);

    Ip6Prefix domainPrefix("fd00::", 64);
    ASSERT_EQ(fw.EnableNdProxyRedirect(domainPrefix, "eth0", 88), OTBR_ERROR_NONE);
    EXPECT_EQ(fw.DisableNdProxyRedirect(), OTBR_ERROR_NONE);
}

TEST(FirewallManagerTest, DisableNdProxyWhenNotEnabledIsNoOp)
{
    NiceMock<MockNftables> mock;
    SetSuccessfulDefaults(mock);
    EXPECT_CALL(mock, DelRule(_, _, _)).Times(0);

    FirewallManager fw(mock, "wpan0");
    ASSERT_EQ(fw.Init(), OTBR_ERROR_NONE);

    EXPECT_EQ(fw.DisableNdProxyRedirect(), OTBR_ERROR_NONE);
}

TEST(FirewallManagerTest, EnableNdProxyRejectsInvalidPrefix)
{
    NiceMock<MockNftables> mock;
    SetSuccessfulDefaults(mock);

    FirewallManager fw(mock, "wpan0");
    ASSERT_EQ(fw.Init(), OTBR_ERROR_NONE);

    Ip6Prefix invalid; // length 0
    EXPECT_EQ(fw.EnableNdProxyRedirect(invalid, "eth0", 88), OTBR_ERROR_INVALID_ARGS);
}

TEST(FirewallManagerTest, InitRejectsEmptyThreadInterfaceName)
{
    NiceMock<MockNftables> mock;
    SetSuccessfulDefaults(mock);

    EXPECT_CALL(mock, BeginBatch()).Times(0);

    FirewallManager fw(mock, "");
    EXPECT_EQ(fw.Init(), OTBR_ERROR_INVALID_ARGS);
}

TEST(FirewallManagerTest, EnableNdProxyRejectsEmptyBackboneInterface)
{
    NiceMock<MockNftables> mock;
    SetSuccessfulDefaults(mock);

    FirewallManager fw(mock, "wpan0");
    ASSERT_EQ(fw.Init(), OTBR_ERROR_NONE);

    // Rejected before any batch is opened (Init() opens one of its own).
    EXPECT_CALL(mock, BeginBatch()).Times(0);

    Ip6Prefix domain("2001:db8::", 64);
    EXPECT_EQ(fw.EnableNdProxyRedirect(domain, "", 88), OTBR_ERROR_INVALID_ARGS);
}

TEST(FirewallManagerTest, EnableNdProxyBeforeInitFails)
{
    NiceMock<MockNftables> mock;

    FirewallManager fw(mock, "wpan0");

    Ip6Prefix domainPrefix("fd00::", 64);
    EXPECT_EQ(fw.EnableNdProxyRedirect(domainPrefix, "eth0", 88), OTBR_ERROR_INVALID_STATE);
}

TEST(FirewallManagerTest, IngressSetOpsRouteToCorrectSets)
{
    NiceMock<MockNftables> mock;
    SetSuccessfulDefaults(mock);

    EXPECT_CALL(mock, AddSetElement(_, StrEq(FirewallManager::kIngressDenySrcSet), _)).Times(1);
    EXPECT_CALL(mock, AddSetElement(_, StrEq(FirewallManager::kIngressAllowDstSet), _)).Times(1);
    EXPECT_CALL(mock, DelSetElement(_, StrEq(FirewallManager::kIngressDenySrcSet), _)).Times(1);
    EXPECT_CALL(mock, FlushSet(_, StrEq(FirewallManager::kIngressAllowDstSet))).Times(1);

    FirewallManager fw(mock, "wpan0");
    ASSERT_EQ(fw.Init(), OTBR_ERROR_NONE);
    ASSERT_EQ(fw.EnableIngressFilter(), OTBR_ERROR_NONE);

    Ip6Prefix prefix("2001:db8::", 64);
    EXPECT_EQ(fw.AddIngressSetElement(FirewallManager::IngressSet::kDenySrc, prefix), OTBR_ERROR_NONE);
    EXPECT_EQ(fw.AddIngressSetElement(FirewallManager::IngressSet::kAllowDst, prefix), OTBR_ERROR_NONE);
    EXPECT_EQ(fw.DelIngressSetElement(FirewallManager::IngressSet::kDenySrc, prefix), OTBR_ERROR_NONE);
    EXPECT_EQ(fw.FlushIngressSet(FirewallManager::IngressSet::kAllowDst), OTBR_ERROR_NONE);
}

TEST(FirewallManagerTest, FailedOperationAbortsTheBatch)
{
    // A failure between BeginBatch() and CommitBatch() must discard the batch,
    // otherwise the backend stays in a batch and every later one is rejected.
    NiceMock<MockNftables> mock;
    SetSuccessfulDefaults(mock);

    FirewallManager fw(mock, "wpan0");
    ASSERT_EQ(fw.Init(), OTBR_ERROR_NONE);
    ASSERT_EQ(fw.EnableIngressFilter(), OTBR_ERROR_NONE);

    EXPECT_CALL(mock, AddSetElement(_, _, _)).WillOnce(Return(OTBR_ERROR_ERRNO));
    EXPECT_CALL(mock, CommitBatch()).Times(0);
    EXPECT_CALL(mock, AbortBatch()).Times(1);

    Ip6Prefix prefix("2001:db8::", 64);
    EXPECT_NE(fw.AddIngressSetElement(FirewallManager::IngressSet::kDenySrc, prefix), OTBR_ERROR_NONE);
}

TEST(FirewallManagerTest, EnableNdProxyKeepsRuleHandleWhenCommitFails)
{
    // The kernel rolls a failed batch back, so the rule installed earlier still
    // exists. The handle must be kept, or that rule can never be deleted.
    NiceMock<MockNftables> mock;
    SetSuccessfulDefaults(mock);
    ON_CALL(mock, AddRuleNdNsRedirect(_, _, _, _, _, _))
        .WillByDefault(DoAll(SetArgPointee<5>(4242), Return(OTBR_ERROR_NONE)));

    FirewallManager fw(mock, "wpan0");
    ASSERT_EQ(fw.Init(), OTBR_ERROR_NONE);

    Ip6Prefix domain("2001:db8::", 64);
    ASSERT_EQ(fw.EnableNdProxyRedirect(domain, "eth0", 88), OTBR_ERROR_NONE);

    // The next call fails at commit; the one after it must still delete the rule
    // installed above, i.e. DelRule() is expected for both.
    EXPECT_CALL(mock, CommitBatch()).WillOnce(Return(OTBR_ERROR_ERRNO)).WillRepeatedly(Return(OTBR_ERROR_NONE));
    EXPECT_CALL(mock, AbortBatch()).Times(1);
    EXPECT_CALL(mock, DelRule(_, _, 4242u)).Times(2);

    EXPECT_NE(fw.EnableNdProxyRedirect(domain, "eth0", 88), OTBR_ERROR_NONE);
    EXPECT_EQ(fw.EnableNdProxyRedirect(domain, "eth0", 88), OTBR_ERROR_NONE);
}

TEST(FirewallManagerTest, IngressSetOpsRequireEnableIngressFilter)
{
    NiceMock<MockNftables> mock;
    SetSuccessfulDefaults(mock);

    FirewallManager fw(mock, "wpan0");
    ASSERT_EQ(fw.Init(), OTBR_ERROR_NONE);

    // EnableIngressFilter NOT called — set ops must reject.
    Ip6Prefix prefix("2001:db8::", 64);
    EXPECT_EQ(fw.AddIngressSetElement(FirewallManager::IngressSet::kDenySrc, prefix), OTBR_ERROR_INVALID_STATE);
}

TEST(FirewallManagerTest, AddIngressSetElementBeforeInitFails)
{
    NiceMock<MockNftables> mock;

    FirewallManager fw(mock, "wpan0");

    Ip6Prefix prefix("2001:db8::", 64);
    EXPECT_EQ(fw.AddIngressSetElement(FirewallManager::IngressSet::kDenySrc, prefix), OTBR_ERROR_INVALID_STATE);
}

TEST(FirewallManagerTest, EnableNat44InstallsThreeChainsAndFiveRules)
{
    NiceMock<MockNftables> mock;
    SetSuccessfulDefaults(mock);

    EXPECT_CALL(mock, AddChain(StrEq(FirewallManager::kTableName), StrEq(FirewallManager::kNatPreroutingChain),
                               Hook::kPrerouting, otbr::Firewall::kPriorityMangle, ChainType::kFilter))
        .Times(1);
    EXPECT_CALL(mock, AddChain(StrEq(FirewallManager::kTableName), StrEq(FirewallManager::kNatPostroutingChain),
                               Hook::kPostrouting, otbr::Firewall::kPrioritySrcNat, ChainType::kNat))
        .Times(1);
    EXPECT_CALL(mock, AddChain(StrEq(FirewallManager::kTableName), StrEq(FirewallManager::kNatForwardChain),
                               Hook::kForward, otbr::Firewall::kPriorityFilter, ChainType::kFilter))
        .Times(1);

    EXPECT_CALL(mock, AddRuleIifMark(StrEq(FirewallManager::kTableName), StrEq(FirewallManager::kNatPreroutingChain),
                                     StrEq("wpan0"), FirewallManager::kNat44Mark, _))
        .Times(1);
    EXPECT_CALL(mock,
                AddRuleMarkMasquerade(StrEq(FirewallManager::kTableName), StrEq(FirewallManager::kNatPostroutingChain),
                                      FirewallManager::kNat44Mark, _))
        .Times(1);
    EXPECT_CALL(mock,
                AddRuleOifnameVerdict(StrEq(FirewallManager::kTableName), StrEq(FirewallManager::kNatForwardChain),
                                      StrEq("eth0"), Verdict::kAccept, _))
        .Times(1);
    EXPECT_CALL(mock,
                AddRuleIifnameVerdict(StrEq(FirewallManager::kTableName), StrEq(FirewallManager::kNatForwardChain),
                                      StrEq("eth0"), Verdict::kAccept, _))
        .Times(1);

    FirewallManager fw(mock, "wpan0");
    ASSERT_EQ(fw.Init(), OTBR_ERROR_NONE);
    EXPECT_EQ(fw.EnableNat44Masquerade("eth0"), OTBR_ERROR_NONE);
    EXPECT_TRUE(fw.IsNat44Enabled());
}

TEST(FirewallManagerTest, EnableNat44TwiceIsNoOp)
{
    NiceMock<MockNftables> mock;
    SetSuccessfulDefaults(mock);
    // Three chains created on the first call (mangle-prerouting, nat-
    // postrouting, nat-forward); the second call must make zero additional
    // AddChain calls.
    EXPECT_CALL(mock, AddChain(_, _, _, _, _)).Times(3);

    FirewallManager fw(mock, "wpan0");
    ASSERT_EQ(fw.Init(), OTBR_ERROR_NONE);
    ASSERT_EQ(fw.EnableNat44Masquerade("eth0"), OTBR_ERROR_NONE);
    EXPECT_EQ(fw.EnableNat44Masquerade("eth0"), OTBR_ERROR_NONE);
}

TEST(FirewallManagerTest, EnableNat44RejectsEmptyUpstreamInterface)
{
    NiceMock<MockNftables> mock;
    SetSuccessfulDefaults(mock);

    FirewallManager fw(mock, "wpan0");
    ASSERT_EQ(fw.Init(), OTBR_ERROR_NONE);

    // Must be rejected before any batch is opened (Init() opens one of its own,
    // so this expectation is set only after Init has run).
    EXPECT_CALL(mock, BeginBatch()).Times(0);

    EXPECT_EQ(fw.EnableNat44Masquerade(""), OTBR_ERROR_INVALID_ARGS);
}

TEST(FirewallManagerTest, EnableNat44BeforeInitFails)
{
    NiceMock<MockNftables> mock;

    FirewallManager fw(mock, "wpan0");
    EXPECT_EQ(fw.EnableNat44Masquerade("eth0"), OTBR_ERROR_INVALID_STATE);
}

#if OTBR_ENABLE_NFTABLES

// --------------------------------------------------------------------------
// Nftables backend tests.
//
// These drive the real Nftables against a scripted socket, which is what makes
// the reply handling reachable at all: FirewallManager's tests mock INftables
// and so never enter this code. Each case below corresponds to a defect that
// review found and that no test could previously have caught.
// --------------------------------------------------------------------------

class NftablesBackendTest : public ::testing::Test
{
protected:
    void SetUp(void) override { ASSERT_EQ(mNftables.Init(), OTBR_ERROR_NONE); }

    // Runs one trivial batch, letting the caller script what the kernel says.
    otbrError RunBatch(void)
    {
        otbrError error = mNftables.BeginBatch();

        if (error == OTBR_ERROR_NONE)
        {
            error = mNftables.AddTable("otbr-test");
        }
        if (error == OTBR_ERROR_NONE)
        {
            error = mNftables.CommitBatch();
        }

        return error;
    }

    FakeNetlinkSocket mSocket;
    Nftables          mNftables{mSocket};
};

TEST_F(NftablesBackendTest, CommitRetriesWhenTheReceiveIsInterrupted)
{
    // A signal must not turn into a failed commit: otbr-agent treats firewall
    // failures as fatal, so this would take the daemon down.
    mSocket.QueueFailure(EINTR);
    mSocket.QueueAck(0);

    EXPECT_EQ(RunBatch(), OTBR_ERROR_NONE);
    EXPECT_EQ(mSocket.PendingReplies(), 0u);
}

TEST_F(NftablesBackendTest, CommitFailsWhenTheReceiveTimesOut)
{
    // The receive timeout exists so a lost ack cannot hang the single-threaded
    // daemon; it must surface as an error rather than being retried forever.
    mSocket.QueueFailure(EAGAIN);

    EXPECT_NE(RunBatch(), OTBR_ERROR_NONE);
}

TEST_F(NftablesBackendTest, CommitFailsOnAZeroLengthReply)
{
    // Leaving the receive loop without an ack means the batch was never
    // acknowledged, so it must not be reported as committed.
    mSocket.QueueZeroLengthReply();

    // A valid ack behind the zero-length reply: the commit must stop at the
    // short read rather than read past it. Without that check the loop treats
    // the empty reply as "nothing to parse", goes round again, and reports this
    // ack as if it had answered the batch.
    mSocket.QueueAck(0);

    EXPECT_NE(RunBatch(), OTBR_ERROR_NONE);
    EXPECT_EQ(mSocket.PendingReplies(), 1u);
}

TEST_F(NftablesBackendTest, CommitDrainsStaleRepliesBeforeSending)
{
    // Replies left behind by an earlier timed-out transaction would otherwise be
    // consumed as if they answered this batch.
    mSocket.QueueAck(0);

    EXPECT_EQ(RunBatch(), OTBR_ERROR_NONE);
    ASSERT_EQ(mSocket.SendCount(), 1u);
    EXPECT_GE(mSocket.DrainsBeforeSend(0), 1u);
}

TEST_F(NftablesBackendTest, AFailedOperationDoesNotWedgeTheBackend)
{
    // A failure between BeginBatch() and CommitBatch() used to leave mInBatch
    // set, after which every later BeginBatch() was rejected for the life of the
    // process.
    mSocket.FailNextSend(EPERM);
    EXPECT_NE(RunBatch(), OTBR_ERROR_NONE);

    EXPECT_EQ(mNftables.BeginBatch(), OTBR_ERROR_NONE);
    EXPECT_EQ(mNftables.CommitBatch(), OTBR_ERROR_ERRNO);
}

TEST_F(NftablesBackendTest, BeginBatchTwiceLeavesTheOpenBatchAlone)
{
    // The guard against nesting must reject the second call without tearing down
    // the batch the first call is still building.
    ASSERT_EQ(mNftables.BeginBatch(), OTBR_ERROR_NONE);
    EXPECT_EQ(mNftables.BeginBatch(), OTBR_ERROR_INVALID_STATE);

    // The first batch is still usable.
    mSocket.QueueAck(0);
    EXPECT_EQ(mNftables.AddTable("otbr-test"), OTBR_ERROR_NONE);
    EXPECT_EQ(mNftables.CommitBatch(), OTBR_ERROR_NONE);
}

TEST(MnlNetlinkSocketTest, RejectsIoWhileClosedRatherThanDereferencingNull)
{
    MnlNetlinkSocket socket;
    uint8_t          buffer[8] = {0};

    // Never opened, so the underlying mnl_socket is null. libmnl dereferences
    // the handle it is given, so these must be rejected here rather than passed
    // down.
    ASSERT_FALSE(socket.IsOpen());

    errno = 0;
    EXPECT_EQ(socket.Send(buffer, sizeof(buffer)), -1);
    EXPECT_EQ(errno, EBADF);

    errno = 0;
    EXPECT_EQ(socket.Recv(buffer, sizeof(buffer)), -1);
    EXPECT_EQ(errno, EBADF);

    // Must simply return; there is no fd to drain.
    socket.Drain();
}

#endif // OTBR_ENABLE_NFTABLES
