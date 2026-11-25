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

#include <stdint.h>
#include <string>
#include <vector>

#include "border_agent/border_agent.hpp"
#include "mdns/mdns.hpp"

#include "mock_mdns_publisher.hpp"

#if OTBR_ENABLE_BORDER_AGENT && OTBR_ENABLE_BORDER_AGENT_MESHCOP_SERVICE

using ::testing::_;
using ::testing::Mock;
using ::testing::StartsWith;
using ::testing::StrEq;

static const char kMeshCoPServiceType[] = "_meshcop._udp";
static const char kEpskcServiceType[]   = "_meshcop-e._udp";
static const char kInstanceBaseName[]   = "Vendor Product";
static const char kExpectedInstance[]   = "Vendor Product #7AC3";

class TestMdnsPublisher : public MockMdnsPublisher
{
public:
    bool IsStarted(void) const override { return mIsStarted; }

    bool mIsStarted = true;
};

class BorderAgentTest : public ::testing::Test
{
protected:
    BorderAgentTest(void)
        : mBorderAgent(mPublisher)
    {
    }

    void SetUp(void) override
    {
        ASSERT_EQ(mBorderAgent.SetMeshCoPServiceValues(kInstanceBaseName, "Product", "Vendor"), OTBR_ERROR_NONE);
    }

    static otbr::BorderAgent::TxtData MakeOtTxtData(void)
    {
        const uint8_t                  kExtAddr[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x7a, 0xc3};
        otbr::Mdns::Publisher::TxtList txtList;
        otbr::BorderAgent::TxtData     txtData;

        txtList.emplace_back("xa", kExtAddr, sizeof(kExtAddr));
        EXPECT_EQ(otbr::Mdns::Publisher::EncodeTxtData(txtList, txtData), OTBR_ERROR_NONE);
        return txtData;
    }

    void InitializeBorderAgent(void)
    {
        mBorderAgent.HandleBorderAgentMeshCoPServiceChanged(/* aIsActive */ true, /* aPort */ 49152, MakeOtTxtData());
    }

    void ExpectNoServiceCalls(void)
    {
        EXPECT_CALL(mPublisher, PublishServiceImpl(_, _, _, _, _, _, _)).Times(0);
        EXPECT_CALL(mPublisher, UnpublishService(_, _, _)).Times(0);
    }

    void VerifyExpectations(void) { Mock::VerifyAndClearExpectations(&mPublisher); }

    ::testing::StrictMock<TestMdnsPublisher> mPublisher;
    otbr::BorderAgent                        mBorderAgent;
};

TEST_F(BorderAgentTest, EpskcDoesNotPublishWhenBorderAgentDisabled)
{
    ExpectNoServiceCalls();

    mBorderAgent.HandleEpskcStateChanged(OT_BORDER_AGENT_STATE_STARTED, 49154);
    mBorderAgent.HandleEpskcStateChanged(OT_BORDER_AGENT_STATE_CONNECTED, 49154);
    mBorderAgent.HandleEpskcStateChanged(OT_BORDER_AGENT_STATE_ACCEPTED, 49154);
    mBorderAgent.HandleEpskcStateChanged(OT_BORDER_AGENT_STATE_STOPPED, 0);
    mBorderAgent.HandleEpskcStateChanged(OT_BORDER_AGENT_STATE_DISABLED, 0);
    VerifyExpectations();

    ExpectNoServiceCalls();
    InitializeBorderAgent();
    mBorderAgent.HandleEpskcStateChanged(OT_BORDER_AGENT_STATE_STARTED, 49154);
    mBorderAgent.HandleEpskcStateChanged(OT_BORDER_AGENT_STATE_CONNECTED, 49154);
    mBorderAgent.HandleEpskcStateChanged(OT_BORDER_AGENT_STATE_ACCEPTED, 49154);
    mBorderAgent.HandleEpskcStateChanged(OT_BORDER_AGENT_STATE_STOPPED, 0);
    mBorderAgent.HandleEpskcStateChanged(OT_BORDER_AGENT_STATE_DISABLED, 0);
    VerifyExpectations();
}

TEST_F(BorderAgentTest, EpskcPublishesAndUnpublishesWhenBorderAgentEnabled)
{
    EXPECT_CALL(mPublisher,
                PublishServiceImpl(StrEq(""), StrEq(kExpectedInstance), StrEq(kMeshCoPServiceType), _, 49152, _, _))
        .Times(1);
    mBorderAgent.SetEnabled(true);
    InitializeBorderAgent();
    VerifyExpectations();

    EXPECT_CALL(mPublisher,
                PublishServiceImpl(StrEq(""), StrEq(kExpectedInstance), StrEq(kEpskcServiceType), _, 49154, _, _))
        .Times(3);
    mBorderAgent.HandleEpskcStateChanged(OT_BORDER_AGENT_STATE_STARTED, 49154);
    mBorderAgent.HandleEpskcStateChanged(OT_BORDER_AGENT_STATE_CONNECTED, 49154);
    mBorderAgent.HandleEpskcStateChanged(OT_BORDER_AGENT_STATE_ACCEPTED, 49154);
    VerifyExpectations();

    EXPECT_CALL(mPublisher, UnpublishService(StrEq(kExpectedInstance), StrEq(kEpskcServiceType), _)).Times(1);
    mBorderAgent.HandleEpskcStateChanged(OT_BORDER_AGENT_STATE_STOPPED, 0);
    mBorderAgent.HandleEpskcStateChanged(OT_BORDER_AGENT_STATE_DISABLED, 0);
    VerifyExpectations();
}

TEST_F(BorderAgentTest, StopUnpublishesActiveEpskcServiceOnlyOnce)
{
    EXPECT_CALL(mPublisher,
                PublishServiceImpl(StrEq(""), StrEq(kExpectedInstance), StrEq(kMeshCoPServiceType), _, 49152, _, _))
        .Times(1);
    EXPECT_CALL(mPublisher,
                PublishServiceImpl(StrEq(""), StrEq(kExpectedInstance), StrEq(kEpskcServiceType), _, 49154, _, _))
        .Times(1);
    mBorderAgent.SetEnabled(true);
    InitializeBorderAgent();
    mBorderAgent.HandleEpskcStateChanged(OT_BORDER_AGENT_STATE_STARTED, 49154);
    VerifyExpectations();

    EXPECT_CALL(mPublisher, UnpublishService(StrEq(kExpectedInstance), StrEq(kMeshCoPServiceType), _)).Times(1);
    EXPECT_CALL(mPublisher, UnpublishService(StrEq(kExpectedInstance), StrEq(kEpskcServiceType), _)).Times(1);
    mBorderAgent.SetEnabled(false);
    VerifyExpectations();

    ExpectNoServiceCalls();
    mBorderAgent.HandleEpskcStateChanged(OT_BORDER_AGENT_STATE_STOPPED, 0);
    VerifyExpectations();
}

TEST_F(BorderAgentTest, StopDoesNotUnpublishInactiveEpskcService)
{
    EXPECT_CALL(mPublisher,
                PublishServiceImpl(StrEq(""), StrEq(kExpectedInstance), StrEq(kMeshCoPServiceType), _, 49152, _, _))
        .Times(1);
    mBorderAgent.SetEnabled(true);
    InitializeBorderAgent();
    VerifyExpectations();

    EXPECT_CALL(mPublisher, UnpublishService(StrEq(kExpectedInstance), StrEq(kMeshCoPServiceType), _)).Times(1);
    EXPECT_CALL(mPublisher, UnpublishService(_, StrEq(kEpskcServiceType), _)).Times(0);
    mBorderAgent.SetEnabled(false);
    VerifyExpectations();
}

TEST_F(BorderAgentTest, EnableBorderAgentPublishesAlreadyActiveEpskcService)
{
    ExpectNoServiceCalls();
    InitializeBorderAgent();
    mBorderAgent.HandleEpskcStateChanged(OT_BORDER_AGENT_STATE_STARTED, 49155);
    VerifyExpectations();

    EXPECT_CALL(mPublisher,
                PublishServiceImpl(StrEq(""), StrEq(kExpectedInstance), StrEq(kMeshCoPServiceType), _, 49152, _, _))
        .Times(1);
    EXPECT_CALL(mPublisher,
                PublishServiceImpl(StrEq(""), StrEq(kExpectedInstance), StrEq(kEpskcServiceType), _, 49155, _, _))
        .Times(1);
    mBorderAgent.SetEnabled(true);
    VerifyExpectations();

    EXPECT_CALL(mPublisher, UnpublishService(StrEq(kExpectedInstance), StrEq(kMeshCoPServiceType), _)).Times(1);
    EXPECT_CALL(mPublisher, UnpublishService(StrEq(kExpectedInstance), StrEq(kEpskcServiceType), _)).Times(1);
    mBorderAgent.SetEnabled(false);
    VerifyExpectations();
}

TEST_F(BorderAgentTest, EpskcStartedBeforeInitializedPublishesOnceInitialized)
{
    ExpectNoServiceCalls();
    mBorderAgent.SetEnabled(true);
    mBorderAgent.HandleEpskcStateChanged(OT_BORDER_AGENT_STATE_STARTED, 49156);
    VerifyExpectations();

    EXPECT_CALL(mPublisher,
                PublishServiceImpl(StrEq(""), StrEq(kExpectedInstance), StrEq(kMeshCoPServiceType), _, 49152, _, _))
        .Times(1);
    EXPECT_CALL(mPublisher,
                PublishServiceImpl(StrEq(""), StrEq(kExpectedInstance), StrEq(kEpskcServiceType), _, 49156, _, _))
        .Times(1);
    InitializeBorderAgent();
    VerifyExpectations();

    EXPECT_CALL(mPublisher,
                PublishServiceImpl(StrEq(""), StrEq(kExpectedInstance), StrEq(kMeshCoPServiceType), _, 49152, _, _))
        .Times(1);
    EXPECT_CALL(mPublisher, PublishServiceImpl(_, _, StrEq(kEpskcServiceType), _, _, _, _)).Times(0);
    InitializeBorderAgent();
    VerifyExpectations();
}

TEST_F(BorderAgentTest, HandleMdnsStateReadyRepublishesActiveEpskcService)
{
    mPublisher.mIsStarted = false;
    ExpectNoServiceCalls();
    mBorderAgent.SetEnabled(true);
    InitializeBorderAgent();
    mBorderAgent.HandleEpskcStateChanged(OT_BORDER_AGENT_STATE_STARTED, 49157);
    VerifyExpectations();

    mPublisher.mIsStarted = true;
    EXPECT_CALL(mPublisher,
                PublishServiceImpl(StrEq(""), StrEq(kExpectedInstance), StrEq(kMeshCoPServiceType), _, 49152, _, _))
        .Times(1);
    EXPECT_CALL(mPublisher,
                PublishServiceImpl(StrEq(""), StrEq(kExpectedInstance), StrEq(kEpskcServiceType), _, 49157, _, _))
        .Times(1);
    mBorderAgent.HandleMdnsState(otbr::Mdns::Publisher::State::kReady);
    VerifyExpectations();

    ExpectNoServiceCalls();
    mBorderAgent.HandleMdnsState(otbr::Mdns::Publisher::State::kIdle);
    VerifyExpectations();

    EXPECT_CALL(mPublisher,
                PublishServiceImpl(StrEq(""), StrEq(kExpectedInstance), StrEq(kMeshCoPServiceType), _, 49152, _, _))
        .Times(1);
    EXPECT_CALL(mPublisher,
                PublishServiceImpl(StrEq(""), StrEq(kExpectedInstance), StrEq(kEpskcServiceType), _, 49157, _, _))
        .Times(1);
    mBorderAgent.HandleMdnsState(otbr::Mdns::Publisher::State::kReady);
    VerifyExpectations();

    EXPECT_CALL(mPublisher, UnpublishService(StrEq(kExpectedInstance), StrEq(kMeshCoPServiceType), _)).Times(1);
    EXPECT_CALL(mPublisher, UnpublishService(StrEq(kExpectedInstance), StrEq(kEpskcServiceType), _)).Times(1);
    mBorderAgent.SetEnabled(false);
    VerifyExpectations();

    ExpectNoServiceCalls();
    mBorderAgent.HandleMdnsState(otbr::Mdns::Publisher::State::kReady);
    VerifyExpectations();
}

TEST_F(BorderAgentTest, EpskcNameConflictRetriesWithAlternativeNameOnlyWhenEnabledAndActive)
{
    otbr::Mdns::Publisher::ResultCallback epskcCallback(nullptr);
    otbr::Mdns::Publisher::ResultCallback chainedCallback(nullptr);
    std::string                           altInstanceName;

    EXPECT_CALL(mPublisher,
                PublishServiceImpl(StrEq(""), StrEq(kExpectedInstance), StrEq(kMeshCoPServiceType), _, 49152, _, _))
        .Times(1);
    EXPECT_CALL(mPublisher,
                PublishServiceImpl(StrEq(""), StrEq(kExpectedInstance), StrEq(kEpskcServiceType), _, 49158, _, _))
        .WillOnce([&epskcCallback](const std::string &, const std::string &, const std::string &,
                                   const otbr::Mdns::Publisher::SubTypeList &, uint16_t,
                                   const otbr::Mdns::Publisher::TxtData &,
                                   otbr::Mdns::Publisher::ResultCallback &&aCallback) {
            epskcCallback = std::move(aCallback);
            return OTBR_ERROR_NONE;
        })
        .WillOnce([&chainedCallback](const std::string &, const std::string &, const std::string &,
                                     const otbr::Mdns::Publisher::SubTypeList &, uint16_t,
                                     const otbr::Mdns::Publisher::TxtData &,
                                     otbr::Mdns::Publisher::ResultCallback &&aCallback) {
            chainedCallback = std::move(aCallback);
            return OTBR_ERROR_NONE;
        });

    mBorderAgent.SetEnabled(true);
    InitializeBorderAgent();
    mBorderAgent.HandleEpskcStateChanged(OT_BORDER_AGENT_STATE_STARTED, 49158);
    mBorderAgent.HandleEpskcStateChanged(OT_BORDER_AGENT_STATE_CONNECTED, 49158);
    VerifyExpectations();
    ASSERT_FALSE(epskcCallback.IsNull());
    ASSERT_FALSE(chainedCallback.IsNull());

    EXPECT_CALL(mPublisher, UnpublishService(StrEq(kExpectedInstance), StrEq(kEpskcServiceType), _)).Times(1);
    EXPECT_CALL(mPublisher, PublishServiceImpl(StrEq(""), StartsWith("Vendor Product #7AC3 ("),
                                               StrEq(kEpskcServiceType), _, 49158, _, _))
        .WillOnce([&altInstanceName, &epskcCallback](const std::string &, const std::string &aName, const std::string &,
                                                     const otbr::Mdns::Publisher::SubTypeList &, uint16_t,
                                                     const otbr::Mdns::Publisher::TxtData &,
                                                     otbr::Mdns::Publisher::ResultCallback &&aCallback) {
            altInstanceName = aName;
            epskcCallback   = std::move(aCallback);
            return OTBR_ERROR_NONE;
        });

    std::move(epskcCallback)(OTBR_ERROR_DUPLICATED);
    VerifyExpectations();
    ASSERT_FALSE(epskcCallback.IsNull());

    // A stale/chained callback for the previously conflicted name must not trigger another rename.
    ExpectNoServiceCalls();
    std::move(chainedCallback)(OTBR_ERROR_DUPLICATED);
    VerifyExpectations();

    EXPECT_CALL(mPublisher, UnpublishService(StrEq(kExpectedInstance), StrEq(kMeshCoPServiceType), _)).Times(1);
    EXPECT_CALL(mPublisher, UnpublishService(StrEq(altInstanceName), StrEq(kEpskcServiceType), _)).Times(1);
    mBorderAgent.SetEnabled(false);
    VerifyExpectations();

    ExpectNoServiceCalls();
    std::move(epskcCallback)(OTBR_ERROR_DUPLICATED);
    VerifyExpectations();

    EXPECT_CALL(mPublisher,
                PublishServiceImpl(StrEq(""), StrEq(kExpectedInstance), StrEq(kMeshCoPServiceType), _, 49152, _, _))
        .Times(1);
    EXPECT_CALL(mPublisher,
                PublishServiceImpl(StrEq(""), StrEq(kExpectedInstance), StrEq(kEpskcServiceType), _, 49158, _, _))
        .WillOnce([&epskcCallback](const std::string &, const std::string &, const std::string &,
                                   const otbr::Mdns::Publisher::SubTypeList &, uint16_t,
                                   const otbr::Mdns::Publisher::TxtData &,
                                   otbr::Mdns::Publisher::ResultCallback &&aCallback) {
            epskcCallback = std::move(aCallback);
            return OTBR_ERROR_NONE;
        });
    mBorderAgent.SetEnabled(true);
    VerifyExpectations();
    ASSERT_FALSE(epskcCallback.IsNull());

    EXPECT_CALL(mPublisher, UnpublishService(StrEq(kExpectedInstance), StrEq(kEpskcServiceType), _)).Times(1);
    mBorderAgent.HandleEpskcStateChanged(OT_BORDER_AGENT_STATE_STOPPED, 0);
    VerifyExpectations();

    // A conflict callback after ePSKc stops while BorderAgent remains enabled must be ignored.
    ExpectNoServiceCalls();
    std::move(epskcCallback)(OTBR_ERROR_DUPLICATED);
    VerifyExpectations();
}

#endif // OTBR_ENABLE_BORDER_AGENT && OTBR_ENABLE_BORDER_AGENT_MESHCOP_SERVICE
