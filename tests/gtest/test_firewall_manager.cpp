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

using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::StrEq;

using otbr::Ip6Prefix;
using otbr::Firewall::FirewallManager;
using otbr::Firewall::Hook;
using otbr::Firewall::INftables;
using otbr::Firewall::Ip6Net;
using otbr::Firewall::PktType;
using otbr::Firewall::SetDirection;
using otbr::Firewall::Verdict;

namespace {

class MockNftables : public INftables
{
public:
    MOCK_METHOD(otbrError, Init, (), (override));
    MOCK_METHOD(otbrError, Deinit, (), (override));
    MOCK_METHOD(otbrError, BeginBatch, (), (override));
    MOCK_METHOD(otbrError, CommitBatch, (), (override));
    MOCK_METHOD(otbrError, DelTable, (const std::string &), (override));
    MOCK_METHOD(otbrError, AddTable, (const std::string &), (override));
    MOCK_METHOD(otbrError, AddChain, (const std::string &, const std::string &, Hook, int), (override));
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
    MOCK_METHOD(otbrError,
                AddRuleVerdict,
                (const std::string &, const std::string &, Verdict, uint64_t *),
                (override));
    MOCK_METHOD(otbrError,
                AddRuleNdNsRedirect,
                (const std::string &, const std::string &, const Ip6Net &, const std::string &, uint16_t, uint64_t *),
                (override));
    MOCK_METHOD(otbrError, DelRule, (const std::string &, const std::string &, uint64_t), (override));
};

/**
 * Default ON_CALL behaviors so a NiceMock's Init succeeds silently and rule
 * adds yield distinct synthetic handles. Tests that care about specific calls
 * still apply EXPECT_CALL to override.
 */
void SetSuccessfulDefaults(MockNftables &mock)
{
    using ::testing::DoAll;
    using ::testing::Return;

    static uint64_t sNextHandle = 100;

    auto returnNoneAndHandle = [](uint64_t *h) { *h = ++sNextHandle; return OTBR_ERROR_NONE; };
    (void)returnNoneAndHandle; // silence unused if compiler flags catch lambdas

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

    ON_CALL(mock, AddRuleOifnameNeqReturn)
        .WillByDefault(DoAll(SetArgPointee<3>(101), Return(OTBR_ERROR_NONE)));
    ON_CALL(mock, AddRuleIifPkttypeVerdict)
        .WillByDefault(DoAll(SetArgPointee<5>(102), Return(OTBR_ERROR_NONE)));
    ON_CALL(mock, AddRulePkttypeVerdict)
        .WillByDefault(DoAll(SetArgPointee<4>(103), Return(OTBR_ERROR_NONE)));
    ON_CALL(mock, AddRuleSetLookupVerdict)
        .WillByDefault(DoAll(SetArgPointee<5>(104), Return(OTBR_ERROR_NONE)));
    ON_CALL(mock, AddRuleVerdict).WillByDefault(DoAll(SetArgPointee<3>(105), Return(OTBR_ERROR_NONE)));
    ON_CALL(mock, AddRuleNdNsRedirect)
        .WillByDefault(DoAll(SetArgPointee<5>(200), Return(OTBR_ERROR_NONE)));
}

} // namespace

TEST(FirewallManagerTest, InitInstallsTableChainsSetsAndIngressRulesInOrder)
{
    NiceMock<MockNftables> mock;

    {
        InSequence seq;
        EXPECT_CALL(mock, DelTable(StrEq(FirewallManager::kTableName))).WillOnce(Return(OTBR_ERROR_NONE));
        EXPECT_CALL(mock, BeginBatch()).WillOnce(Return(OTBR_ERROR_NONE));
        EXPECT_CALL(mock, AddTable(StrEq(FirewallManager::kTableName))).WillOnce(Return(OTBR_ERROR_NONE));
        EXPECT_CALL(mock,
                    AddIp6PrefixSet(StrEq(FirewallManager::kTableName), StrEq(FirewallManager::kIngressDenySrcSet)))
            .WillOnce(Return(OTBR_ERROR_NONE));
        EXPECT_CALL(mock,
                    AddIp6PrefixSet(StrEq(FirewallManager::kTableName), StrEq(FirewallManager::kIngressAllowDstSet)))
            .WillOnce(Return(OTBR_ERROR_NONE));
        EXPECT_CALL(mock,
                    AddChain(StrEq(FirewallManager::kTableName),
                             StrEq(FirewallManager::kIngressChain),
                             Hook::kForward,
                             otbr::Firewall::kPriorityFilter))
            .WillOnce(Return(OTBR_ERROR_NONE));
        EXPECT_CALL(mock,
                    AddRuleOifnameNeqReturn(StrEq(FirewallManager::kTableName),
                                            StrEq(FirewallManager::kIngressChain),
                                            StrEq("wpan0"),
                                            _))
            .WillOnce(DoAll(SetArgPointee<3>(1), Return(OTBR_ERROR_NONE)));
        EXPECT_CALL(mock,
                    AddRuleIifPkttypeVerdict(StrEq(FirewallManager::kTableName),
                                             StrEq(FirewallManager::kIngressChain),
                                             StrEq("wpan0"),
                                             PktType::kUnicast,
                                             Verdict::kDrop,
                                             _))
            .WillOnce(DoAll(SetArgPointee<5>(2), Return(OTBR_ERROR_NONE)));
        EXPECT_CALL(mock,
                    AddRuleSetLookupVerdict(StrEq(FirewallManager::kTableName),
                                            StrEq(FirewallManager::kIngressChain),
                                            StrEq(FirewallManager::kIngressDenySrcSet),
                                            SetDirection::kSrc,
                                            Verdict::kDrop,
                                            _))
            .WillOnce(DoAll(SetArgPointee<5>(3), Return(OTBR_ERROR_NONE)));
        EXPECT_CALL(mock,
                    AddRuleSetLookupVerdict(StrEq(FirewallManager::kTableName),
                                            StrEq(FirewallManager::kIngressChain),
                                            StrEq(FirewallManager::kIngressAllowDstSet),
                                            SetDirection::kDst,
                                            Verdict::kAccept,
                                            _))
            .WillOnce(DoAll(SetArgPointee<5>(4), Return(OTBR_ERROR_NONE)));
        EXPECT_CALL(mock,
                    AddRulePkttypeVerdict(StrEq(FirewallManager::kTableName),
                                          StrEq(FirewallManager::kIngressChain),
                                          PktType::kUnicast,
                                          Verdict::kDrop,
                                          _))
            .WillOnce(DoAll(SetArgPointee<4>(5), Return(OTBR_ERROR_NONE)));
        EXPECT_CALL(mock,
                    AddRuleVerdict(StrEq(FirewallManager::kTableName),
                                   StrEq(FirewallManager::kIngressChain),
                                   Verdict::kAccept,
                                   _))
            .WillOnce(DoAll(SetArgPointee<3>(6), Return(OTBR_ERROR_NONE)));
        EXPECT_CALL(mock,
                    AddChain(StrEq(FirewallManager::kTableName),
                             StrEq(FirewallManager::kPreroutingChain),
                             Hook::kPrerouting,
                             otbr::Firewall::kPriorityRaw))
            .WillOnce(Return(OTBR_ERROR_NONE));
        EXPECT_CALL(mock, CommitBatch()).WillOnce(Return(OTBR_ERROR_NONE));
    }

    FirewallManager fw(mock, "wpan0");
    EXPECT_EQ(fw.Init(), OTBR_ERROR_NONE);
    EXPECT_TRUE(fw.IsInitialized());
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

    FirewallManager fw(mock, "wpan0");
    ASSERT_EQ(fw.Init(), OTBR_ERROR_NONE);
    EXPECT_EQ(fw.Deinit(), OTBR_ERROR_NONE);
    EXPECT_FALSE(fw.IsInitialized());
}

TEST(FirewallManagerTest, EnableNdProxyInstallsRule)
{
    NiceMock<MockNftables> mock;
    SetSuccessfulDefaults(mock);

    EXPECT_CALL(mock,
                AddRuleNdNsRedirect(StrEq(FirewallManager::kTableName),
                                    StrEq(FirewallManager::kPreroutingChain),
                                    _,
                                    StrEq("eth0"),
                                    88,
                                    _))
        .WillOnce(DoAll(SetArgPointee<5>(42), Return(OTBR_ERROR_NONE)));

    FirewallManager fw(mock, "wpan0");
    ASSERT_EQ(fw.Init(), OTBR_ERROR_NONE);

    Ip6Prefix domainPrefix("fd00::", 64);
    EXPECT_EQ(fw.EnableNdProxyRedirect(domainPrefix, "eth0", 88), OTBR_ERROR_NONE);
}

TEST(FirewallManagerTest, EnableNdProxyTwiceReplacesPreviousRule)
{
    NiceMock<MockNftables> mock;
    SetSuccessfulDefaults(mock);

    EXPECT_CALL(mock, AddRuleNdNsRedirect(_, _, _, _, _, _))
        .WillOnce(DoAll(SetArgPointee<5>(11), Return(OTBR_ERROR_NONE)))
        .WillOnce(DoAll(SetArgPointee<5>(12), Return(OTBR_ERROR_NONE)));
    // Second Enable must delete the previously installed rule (handle 11).
    EXPECT_CALL(mock, DelRule(StrEq(FirewallManager::kTableName), StrEq(FirewallManager::kPreroutingChain), 11))
        .Times(1);

    FirewallManager fw(mock, "wpan0");
    ASSERT_EQ(fw.Init(), OTBR_ERROR_NONE);

    Ip6Prefix domainPrefix("fd00::", 64);
    EXPECT_EQ(fw.EnableNdProxyRedirect(domainPrefix, "eth0", 88), OTBR_ERROR_NONE);
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

    EXPECT_CALL(mock, AddSetElement(StrEq(FirewallManager::kTableName),
                                    StrEq(FirewallManager::kIngressDenySrcSet),
                                    _))
        .Times(1);
    EXPECT_CALL(mock, AddSetElement(StrEq(FirewallManager::kTableName),
                                    StrEq(FirewallManager::kIngressAllowDstSet),
                                    _))
        .Times(1);
    EXPECT_CALL(mock, DelSetElement(StrEq(FirewallManager::kTableName),
                                    StrEq(FirewallManager::kIngressDenySrcSet),
                                    _))
        .Times(1);
    EXPECT_CALL(mock, FlushSet(StrEq(FirewallManager::kTableName),
                               StrEq(FirewallManager::kIngressAllowDstSet)))
        .Times(1);

    FirewallManager fw(mock, "wpan0");
    ASSERT_EQ(fw.Init(), OTBR_ERROR_NONE);

    Ip6Prefix prefix("2001:db8::", 64);
    EXPECT_EQ(fw.AddIngressSetElement(FirewallManager::IngressSet::kDenySrc, prefix), OTBR_ERROR_NONE);
    EXPECT_EQ(fw.AddIngressSetElement(FirewallManager::IngressSet::kAllowDst, prefix), OTBR_ERROR_NONE);
    EXPECT_EQ(fw.DelIngressSetElement(FirewallManager::IngressSet::kDenySrc, prefix), OTBR_ERROR_NONE);
    EXPECT_EQ(fw.FlushIngressSet(FirewallManager::IngressSet::kAllowDst), OTBR_ERROR_NONE);
}

TEST(FirewallManagerTest, AddIngressSetElementBeforeInitFails)
{
    NiceMock<MockNftables> mock;

    FirewallManager fw(mock, "wpan0");

    Ip6Prefix prefix("2001:db8::", 64);
    EXPECT_EQ(fw.AddIngressSetElement(FirewallManager::IngressSet::kDenySrc, prefix), OTBR_ERROR_INVALID_STATE);
}
