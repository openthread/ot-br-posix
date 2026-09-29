/*
 *    Copyright (c) 2025, The OpenThread Authors.
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

#include "common/code_utils.hpp"
#include "common/mainloop_manager.hpp"
#include "host/posix/dnssd.hpp"
#include "mdns/mdns.hpp"

#include "mock_mdns_publisher.hpp"

#if OTBR_ENABLE_DNSSD_PLAT

using ::testing::_;
using ::testing::Mock;
using ::testing::MockFunction;
using ::testing::SaveArg;
using ::testing::StrEq;

class DnssdTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        mPublisher     = std::make_unique<MockMdnsPublisher>();
        mDnssdPlatform = std::make_unique<otbr::DnssdPlatform>(*mPublisher);

        mStateSubject.AddObserver(*mDnssdPlatform);
        mStateSubject.UpdateState(otbr::Mdns::Publisher::State::kReady);
        mDnssdPlatform->Start();
    }

    otbr::Mdns::StateSubject             mStateSubject;
    std::unique_ptr<MockMdnsPublisher>   mPublisher;
    std::unique_ptr<otbr::DnssdPlatform> mDnssdPlatform;
};

void ProcessMainloop(void)
{
    otbr::MainloopContext context;

    context.mMaxFd   = -1;
    context.mTimeout = {0, 1};
    FD_ZERO(&context.mReadFdSet);
    FD_ZERO(&context.mWriteFdSet);
    FD_ZERO(&context.mErrorFdSet);

    otbr::MainloopManager::GetInstance().Update(context);
    int rval =
        select(context.mMaxFd + 1, &context.mReadFdSet, &context.mWriteFdSet, &context.mErrorFdSet, &context.mTimeout);
    if (rval < 0)
    {
        perror("select failed");
        exit(EXIT_FAILURE);
    }
    otbr::MainloopManager::GetInstance().Process(context);
}

TEST_F(DnssdTest, TestServiceBrowserCallbackIsCorrectlyInvoked)
{
    constexpr uint32_t kInfraIfIndex = 1;

    otbr::DnssdPlatform::Browser                                  browser;
    otbr::Mdns::Publisher::DiscoveredInstanceInfo                 discoveredInstanceInfo;
    const char                                                   *serviceType = "_plant._tcp";
    MockFunction<void(const otbr::DnssdPlatform::BrowseResult &)> mockCallback;

    browser.mServiceType  = serviceType;
    browser.mSubTypeLabel = nullptr;
    browser.mInfraIfIndex = kInfraIfIndex;
    browser.mCallback     = nullptr;

    // 1. A service is resovled and expect the callback is invoked.
    EXPECT_CALL(*mPublisher, SubscribeService(StrEq(serviceType), StrEq(""), kInfraIfIndex));

    mDnssdPlatform->StartServiceBrowser(
        browser, std::make_unique<otbr::DnssdPlatform::StdBrowseCallback>(mockCallback.AsStdFunction(), 1));

    EXPECT_CALL(mockCallback, Call(_)).WillOnce([&](const otbr::DnssdPlatform::BrowseResult &aResult) {
        EXPECT_EQ(aResult.mInfraIfIndex, kInfraIfIndex);
        EXPECT_EQ(aResult.mTtl, 10);
        EXPECT_EQ(aResult.mSubTypeLabel, nullptr);
        EXPECT_STREQ(aResult.mServiceType, serviceType);
        EXPECT_STREQ(aResult.mServiceInstance, "ZGMF-X42S #1");
    });
    ProcessMainloop();

    discoveredInstanceInfo.mRemoved    = false;
    discoveredInstanceInfo.mNetifIndex = kInfraIfIndex;
    discoveredInstanceInfo.mName       = "ZGMF-X42S #1";
    discoveredInstanceInfo.mHostName   = "ZGMF-X42S #1._plant._tcp.local.";
    discoveredInstanceInfo.mTtl        = 10;
    mPublisher->TestOnServiceResolved(serviceType, discoveredInstanceInfo);
    ProcessMainloop();

    // 2. Another service is resovled but the callback shouldn't be invoked again.
    EXPECT_CALL(*mPublisher, UnsubscribeService(StrEq(serviceType), StrEq(""), kInfraIfIndex));

    mDnssdPlatform->StopServiceBrowser(browser, otbr::DnssdPlatform::StdBrowseCallback(nullptr, 1));
    ProcessMainloop();

    discoveredInstanceInfo.mRemoved    = false;
    discoveredInstanceInfo.mNetifIndex = kInfraIfIndex;
    discoveredInstanceInfo.mName       = "ZGMF-X666S #1";
    discoveredInstanceInfo.mHostName   = "ZGMF-X666S #1._plant._tcp.local.";
    discoveredInstanceInfo.mTtl        = 10;
    mPublisher->TestOnServiceResolved(serviceType, discoveredInstanceInfo);
    ProcessMainloop();
}

TEST_F(DnssdTest, TestServiceResolverStoppedInCallbackOfStartWorksCorrectly)
{
    constexpr uint32_t kInfraIfIndex = 1;

    otbr::DnssdPlatform::SrvResolver              resolver1;
    otbr::DnssdPlatform::SrvResolver              resolver2;
    const char                                   *serviceType = "_plant._tcp";
    otbr::Mdns::Publisher::DiscoveredInstanceInfo discoveredInstanceInfo1;
    otbr::Mdns::Publisher::DiscoveredInstanceInfo discoveredInstanceInfo2;
    bool                                          invoked = false;
    uint64_t                                      id1     = 2;
    uint64_t                                      id2     = 3;

    resolver1.mServiceType     = serviceType;
    resolver1.mServiceInstance = "ZGMF-X10A #1";
    resolver1.mInfraIfIndex    = kInfraIfIndex;
    resolver1.mCallback        = nullptr;

    resolver2.mServiceType     = serviceType;
    resolver2.mServiceInstance = "ZGMF-X13A #1";
    resolver2.mInfraIfIndex    = kInfraIfIndex;
    resolver2.mCallback        = nullptr;

    // 1. Start 2 services resolver. Stop the resolvers in the callbacks.
    EXPECT_CALL(*mPublisher, SubscribeService(StrEq(serviceType), StrEq(resolver1.mServiceInstance), kInfraIfIndex))
        .Times(1);
    EXPECT_CALL(*mPublisher, UnsubscribeService(StrEq(serviceType), StrEq(resolver1.mServiceInstance), kInfraIfIndex))
        .Times(1);
    EXPECT_CALL(*mPublisher, SubscribeService(StrEq(serviceType), StrEq(resolver2.mServiceInstance), kInfraIfIndex))
        .Times(1);

    auto callbackPtr = std::make_unique<otbr::DnssdPlatform::StdSrvCallback>(
        [this, id1, &resolver1, &discoveredInstanceInfo1, &invoked](const otbr::DnssdPlatform::SrvResult &aResult) {
            mDnssdPlatform->StopServiceResolver(resolver1, otbr::DnssdPlatform::StdSrvCallback(nullptr, id1));

            EXPECT_EQ(aResult.mInfraIfIndex, resolver1.mInfraIfIndex);
            EXPECT_EQ(aResult.mTtl, discoveredInstanceInfo1.mTtl);
            EXPECT_EQ(aResult.mPort, discoveredInstanceInfo1.mPort);
            EXPECT_EQ(aResult.mPriority, discoveredInstanceInfo1.mPriority);
            EXPECT_EQ(aResult.mWeight, discoveredInstanceInfo1.mWeight);
            EXPECT_STREQ(aResult.mServiceInstance, resolver1.mServiceInstance);
            EXPECT_STREQ(aResult.mServiceType, resolver1.mServiceType);
            EXPECT_STREQ(aResult.mHostName, "Eternal");

            invoked = true;
        },
        id1);
    mDnssdPlatform->StartServiceResolver(resolver1, std::move(callbackPtr));
    callbackPtr = std::make_unique<otbr::DnssdPlatform::StdSrvCallback>(
        [&resolver2, &discoveredInstanceInfo2](const otbr::DnssdPlatform::SrvResult &aResult) {
            EXPECT_EQ(aResult.mInfraIfIndex, resolver2.mInfraIfIndex);
            EXPECT_EQ(aResult.mTtl, discoveredInstanceInfo2.mTtl);
            EXPECT_EQ(aResult.mPort, discoveredInstanceInfo2.mPort);
            EXPECT_EQ(aResult.mPriority, discoveredInstanceInfo2.mPriority);
            EXPECT_EQ(aResult.mWeight, discoveredInstanceInfo2.mWeight);
            EXPECT_STREQ(aResult.mServiceInstance, resolver2.mServiceInstance);
            EXPECT_STREQ(aResult.mServiceType, resolver2.mServiceType);
            EXPECT_STREQ(aResult.mHostName, "Genesis");
        },
        id2);
    mDnssdPlatform->StartServiceResolver(resolver2, std::move(callbackPtr));
    ProcessMainloop();

    // 2. Found an instance for Resolver1.
    discoveredInstanceInfo1.mRemoved    = false;
    discoveredInstanceInfo1.mNetifIndex = kInfraIfIndex;
    discoveredInstanceInfo1.mName       = "ZGMF-X10A #1";
    discoveredInstanceInfo1.mHostName   = "Eternal.";
    discoveredInstanceInfo1.mTtl        = 10;
    discoveredInstanceInfo1.mPort       = 11;
    discoveredInstanceInfo1.mPriority   = 12;
    discoveredInstanceInfo1.mWeight     = 13;

    mPublisher->TestOnServiceResolved(serviceType, discoveredInstanceInfo1);
    ProcessMainloop();

    // 3. Found an instance for Resolver2.
    discoveredInstanceInfo2.mRemoved    = false;
    discoveredInstanceInfo2.mNetifIndex = kInfraIfIndex;
    discoveredInstanceInfo2.mName       = "ZGMF-X13A #1";
    discoveredInstanceInfo2.mHostName   = "Genesis.";
    discoveredInstanceInfo2.mTtl        = 13;
    discoveredInstanceInfo2.mPort       = 14;
    discoveredInstanceInfo2.mPriority   = 15;
    discoveredInstanceInfo2.mWeight     = 16;

    mPublisher->TestOnServiceResolved(serviceType, discoveredInstanceInfo2);
    ProcessMainloop();

    // 4. Updated an instance for Resolver1. Callback shouldn't be invoked.
    invoked = false;

    discoveredInstanceInfo1.mHostName = "ArchAngel.";
    mPublisher->TestOnServiceResolved(serviceType, discoveredInstanceInfo1);
    ProcessMainloop();

    EXPECT_FALSE(invoked);
}

// The infrastructure interface indexes of the requests in the tests below.
constexpr uint32_t kInfraIfIndex1 = 11;
constexpr uint32_t kInfraIfIndex2 = 12;

TEST_F(DnssdTest, TestServiceBrowsersOfTwoInfraIfsHaveTheirOwnSubscriptionAndResults)
{
    otbr::DnssdPlatform::Browser                                  browser1;
    otbr::DnssdPlatform::Browser                                  browser2;
    otbr::Mdns::Publisher::DiscoveredInstanceInfo                 discoveredInstanceInfo;
    const char                                                   *serviceType = "_plant._tcp";
    MockFunction<void(const otbr::DnssdPlatform::BrowseResult &)> mockCallback1;
    MockFunction<void(const otbr::DnssdPlatform::BrowseResult &)> mockCallback2;
    uint64_t                                                      id1 = 1;
    uint64_t                                                      id2 = 2;

    browser1.mServiceType  = serviceType;
    browser1.mSubTypeLabel = nullptr;
    browser1.mInfraIfIndex = kInfraIfIndex1;
    browser1.mCallback     = nullptr;

    browser2               = browser1;
    browser2.mInfraIfIndex = kInfraIfIndex2;

    // 1. Two browsers for the same service type on two interfaces lead to one subscription per interface.
    EXPECT_CALL(*mPublisher, SubscribeService(StrEq(serviceType), StrEq(""), kInfraIfIndex1)).Times(1);
    EXPECT_CALL(*mPublisher, SubscribeService(StrEq(serviceType), StrEq(""), kInfraIfIndex2)).Times(1);

    mDnssdPlatform->StartServiceBrowser(
        browser1, std::make_unique<otbr::DnssdPlatform::StdBrowseCallback>(mockCallback1.AsStdFunction(), id1));
    mDnssdPlatform->StartServiceBrowser(
        browser2, std::make_unique<otbr::DnssdPlatform::StdBrowseCallback>(mockCallback2.AsStdFunction(), id2));
    ProcessMainloop();
    Mock::VerifyAndClearExpectations(mPublisher.get());

    // 2. Each browser gets the results reported on its own interface only.
    discoveredInstanceInfo.mRemoved  = false;
    discoveredInstanceInfo.mHostName = "Minerva.local.";
    discoveredInstanceInfo.mTtl      = 10;

    EXPECT_CALL(mockCallback1, Call(_)).WillOnce([&](const otbr::DnssdPlatform::BrowseResult &aResult) {
        EXPECT_EQ(aResult.mInfraIfIndex, kInfraIfIndex1);
        EXPECT_EQ(aResult.mTtl, 10);
        EXPECT_STREQ(aResult.mServiceInstance, "ZGMF-X56S #1");
    });
    EXPECT_CALL(mockCallback2, Call(_)).Times(0);

    discoveredInstanceInfo.mNetifIndex = kInfraIfIndex1;
    discoveredInstanceInfo.mName       = "ZGMF-X56S #1";
    mPublisher->TestOnServiceResolved(serviceType, discoveredInstanceInfo);
    ProcessMainloop();
    Mock::VerifyAndClearExpectations(&mockCallback1);
    Mock::VerifyAndClearExpectations(&mockCallback2);

    EXPECT_CALL(mockCallback1, Call(_)).Times(0);
    EXPECT_CALL(mockCallback2, Call(_)).WillOnce([&](const otbr::DnssdPlatform::BrowseResult &aResult) {
        EXPECT_EQ(aResult.mInfraIfIndex, kInfraIfIndex2);
        EXPECT_EQ(aResult.mTtl, 10);
        EXPECT_STREQ(aResult.mServiceInstance, "ZGMF-X56S #2");
    });

    discoveredInstanceInfo.mNetifIndex = kInfraIfIndex2;
    discoveredInstanceInfo.mName       = "ZGMF-X56S #2";
    mPublisher->TestOnServiceResolved(serviceType, discoveredInstanceInfo);
    ProcessMainloop();
    Mock::VerifyAndClearExpectations(&mockCallback1);
    Mock::VerifyAndClearExpectations(&mockCallback2);

    // 3. A removal reported on the first interface reaches the browser of that interface only.
    EXPECT_CALL(mockCallback1, Call(_)).WillOnce([&](const otbr::DnssdPlatform::BrowseResult &aResult) {
        EXPECT_EQ(aResult.mInfraIfIndex, kInfraIfIndex1);
        EXPECT_EQ(aResult.mTtl, 0);
        EXPECT_STREQ(aResult.mServiceInstance, "ZGMF-X56S #1");
    });
    EXPECT_CALL(mockCallback2, Call(_)).Times(0);

    discoveredInstanceInfo             = {};
    discoveredInstanceInfo.mRemoved    = true;
    discoveredInstanceInfo.mNetifIndex = kInfraIfIndex1;
    discoveredInstanceInfo.mName       = "ZGMF-X56S #1";
    mPublisher->TestOnServiceResolved(serviceType, discoveredInstanceInfo);
    ProcessMainloop();
    Mock::VerifyAndClearExpectations(&mockCallback1);
    Mock::VerifyAndClearExpectations(&mockCallback2);

    // 4. Stopping the browser of the first interface removes the subscription of that interface only.
    EXPECT_CALL(*mPublisher, UnsubscribeService(StrEq(serviceType), StrEq(""), kInfraIfIndex1)).Times(1);
    EXPECT_CALL(*mPublisher, UnsubscribeService(StrEq(serviceType), StrEq(""), kInfraIfIndex2)).Times(0);

    mDnssdPlatform->StopServiceBrowser(browser1, otbr::DnssdPlatform::StdBrowseCallback(nullptr, id1));
    ProcessMainloop();
    Mock::VerifyAndClearExpectations(mPublisher.get());

    // 5. The browser of the second interface still gets the results reported on its interface.
    EXPECT_CALL(mockCallback1, Call(_)).Times(0);
    EXPECT_CALL(mockCallback2, Call(_)).WillOnce([&](const otbr::DnssdPlatform::BrowseResult &aResult) {
        EXPECT_EQ(aResult.mInfraIfIndex, kInfraIfIndex2);
        EXPECT_STREQ(aResult.mServiceInstance, "ZGMF-X56S #3");
    });

    discoveredInstanceInfo.mRemoved    = false;
    discoveredInstanceInfo.mHostName   = "Minerva.local.";
    discoveredInstanceInfo.mTtl        = 10;
    discoveredInstanceInfo.mName       = "ZGMF-X56S #3";
    discoveredInstanceInfo.mNetifIndex = kInfraIfIndex1;
    mPublisher->TestOnServiceResolved(serviceType, discoveredInstanceInfo);
    discoveredInstanceInfo.mNetifIndex = kInfraIfIndex2;
    mPublisher->TestOnServiceResolved(serviceType, discoveredInstanceInfo);
    ProcessMainloop();
    Mock::VerifyAndClearExpectations(&mockCallback1);
    Mock::VerifyAndClearExpectations(&mockCallback2);

    EXPECT_CALL(*mPublisher, UnsubscribeService(StrEq(serviceType), StrEq(""), kInfraIfIndex2)).Times(1);

    mDnssdPlatform->StopServiceBrowser(browser2, otbr::DnssdPlatform::StdBrowseCallback(nullptr, id2));
    ProcessMainloop();
}

TEST_F(DnssdTest, TestServiceResolversOfTwoInfraIfsHaveTheirOwnSubscriptionAndResults)
{
    otbr::DnssdPlatform::SrvResolver                           resolver1;
    otbr::DnssdPlatform::SrvResolver                           resolver2;
    otbr::Mdns::Publisher::DiscoveredInstanceInfo              discoveredInstanceInfo;
    const char                                                *serviceType     = "_plant._tcp";
    const char                                                *serviceInstance = "ZGMF-X20A #1";
    MockFunction<void(const otbr::DnssdPlatform::SrvResult &)> mockCallback1;
    MockFunction<void(const otbr::DnssdPlatform::SrvResult &)> mockCallback2;
    uint64_t                                                   id1 = 1;
    uint64_t                                                   id2 = 2;

    resolver1.mServiceType     = serviceType;
    resolver1.mServiceInstance = serviceInstance;
    resolver1.mInfraIfIndex    = kInfraIfIndex1;
    resolver1.mCallback        = nullptr;

    resolver2               = resolver1;
    resolver2.mInfraIfIndex = kInfraIfIndex2;

    // 1. Two resolvers for the same service instance on two interfaces lead to one subscription per interface.
    EXPECT_CALL(*mPublisher, SubscribeService(StrEq(serviceType), StrEq(serviceInstance), kInfraIfIndex1)).Times(1);
    EXPECT_CALL(*mPublisher, SubscribeService(StrEq(serviceType), StrEq(serviceInstance), kInfraIfIndex2)).Times(1);

    mDnssdPlatform->StartServiceResolver(
        resolver1, std::make_unique<otbr::DnssdPlatform::StdSrvCallback>(mockCallback1.AsStdFunction(), id1));
    mDnssdPlatform->StartServiceResolver(
        resolver2, std::make_unique<otbr::DnssdPlatform::StdSrvCallback>(mockCallback2.AsStdFunction(), id2));
    ProcessMainloop();
    Mock::VerifyAndClearExpectations(mPublisher.get());

    // 2. Each resolver gets the results reported on its own interface only.
    discoveredInstanceInfo.mRemoved  = false;
    discoveredInstanceInfo.mName     = serviceInstance;
    discoveredInstanceInfo.mHostName = "Eternal.";
    discoveredInstanceInfo.mTtl      = 10;

    EXPECT_CALL(mockCallback1, Call(_)).WillOnce([&](const otbr::DnssdPlatform::SrvResult &aResult) {
        EXPECT_EQ(aResult.mInfraIfIndex, kInfraIfIndex1);
        EXPECT_EQ(aResult.mPort, 11);
    });
    EXPECT_CALL(mockCallback2, Call(_)).Times(0);

    discoveredInstanceInfo.mNetifIndex = kInfraIfIndex1;
    discoveredInstanceInfo.mPort       = 11;
    mPublisher->TestOnServiceResolved(serviceType, discoveredInstanceInfo);
    ProcessMainloop();
    Mock::VerifyAndClearExpectations(&mockCallback1);
    Mock::VerifyAndClearExpectations(&mockCallback2);

    EXPECT_CALL(mockCallback1, Call(_)).Times(0);
    EXPECT_CALL(mockCallback2, Call(_)).WillOnce([&](const otbr::DnssdPlatform::SrvResult &aResult) {
        EXPECT_EQ(aResult.mInfraIfIndex, kInfraIfIndex2);
        EXPECT_EQ(aResult.mPort, 12);
    });

    discoveredInstanceInfo.mNetifIndex = kInfraIfIndex2;
    discoveredInstanceInfo.mPort       = 12;
    mPublisher->TestOnServiceResolved(serviceType, discoveredInstanceInfo);
    ProcessMainloop();
    Mock::VerifyAndClearExpectations(&mockCallback1);
    Mock::VerifyAndClearExpectations(&mockCallback2);

    // 3. Stopping the resolver of the first interface removes the subscription of that interface only.
    EXPECT_CALL(*mPublisher, UnsubscribeService(StrEq(serviceType), StrEq(serviceInstance), kInfraIfIndex1)).Times(1);
    EXPECT_CALL(*mPublisher, UnsubscribeService(StrEq(serviceType), StrEq(serviceInstance), kInfraIfIndex2)).Times(0);

    mDnssdPlatform->StopServiceResolver(resolver1, otbr::DnssdPlatform::StdSrvCallback(nullptr, id1));
    ProcessMainloop();
    Mock::VerifyAndClearExpectations(mPublisher.get());

    // 4. The resolver of the second interface still gets the results reported on its interface.
    EXPECT_CALL(mockCallback1, Call(_)).Times(0);
    EXPECT_CALL(mockCallback2, Call(_)).WillOnce([&](const otbr::DnssdPlatform::SrvResult &aResult) {
        EXPECT_EQ(aResult.mInfraIfIndex, kInfraIfIndex2);
        EXPECT_EQ(aResult.mPort, 13);
    });

    discoveredInstanceInfo.mPort       = 13;
    discoveredInstanceInfo.mNetifIndex = kInfraIfIndex1;
    mPublisher->TestOnServiceResolved(serviceType, discoveredInstanceInfo);
    discoveredInstanceInfo.mNetifIndex = kInfraIfIndex2;
    mPublisher->TestOnServiceResolved(serviceType, discoveredInstanceInfo);
    ProcessMainloop();
    Mock::VerifyAndClearExpectations(&mockCallback1);
    Mock::VerifyAndClearExpectations(&mockCallback2);

    EXPECT_CALL(*mPublisher, UnsubscribeService(StrEq(serviceType), StrEq(serviceInstance), kInfraIfIndex2)).Times(1);

    mDnssdPlatform->StopServiceResolver(resolver2, otbr::DnssdPlatform::StdSrvCallback(nullptr, id2));
    ProcessMainloop();
}

TEST_F(DnssdTest, TestSrvAndTxtResolversOfTheSameInfraIfShareOneSubscription)
{
    otbr::DnssdPlatform::SrvResolver                           srvResolver;
    otbr::DnssdPlatform::TxtResolver                           txtResolver1;
    otbr::DnssdPlatform::TxtResolver                           txtResolver2;
    otbr::Mdns::Publisher::DiscoveredInstanceInfo              discoveredInstanceInfo;
    const char                                                *serviceType     = "_plant._tcp";
    const char                                                *serviceInstance = "ZGMF-X09A #1";
    MockFunction<void(const otbr::DnssdPlatform::SrvResult &)> mockSrvCallback;
    MockFunction<void(const otbr::DnssdPlatform::TxtResult &)> mockTxtCallback1;
    MockFunction<void(const otbr::DnssdPlatform::TxtResult &)> mockTxtCallback2;
    uint64_t                                                   srvId  = 1;
    uint64_t                                                   txtId1 = 2;
    uint64_t                                                   txtId2 = 3;

    srvResolver.mServiceType     = serviceType;
    srvResolver.mServiceInstance = serviceInstance;
    srvResolver.mInfraIfIndex    = kInfraIfIndex1;
    srvResolver.mCallback        = nullptr;

    txtResolver1.mServiceType     = serviceType;
    txtResolver1.mServiceInstance = serviceInstance;
    txtResolver1.mInfraIfIndex    = kInfraIfIndex1;
    txtResolver1.mCallback        = nullptr;

    txtResolver2               = txtResolver1;
    txtResolver2.mInfraIfIndex = kInfraIfIndex2;

    // 1. The SRV and the TXT resolver of the first interface share a subscription. The TXT resolver of the second
    //    interface has its own.
    EXPECT_CALL(*mPublisher, SubscribeService(StrEq(serviceType), StrEq(serviceInstance), kInfraIfIndex1)).Times(1);
    EXPECT_CALL(*mPublisher, SubscribeService(StrEq(serviceType), StrEq(serviceInstance), kInfraIfIndex2)).Times(1);

    mDnssdPlatform->StartServiceResolver(
        srvResolver, std::make_unique<otbr::DnssdPlatform::StdSrvCallback>(mockSrvCallback.AsStdFunction(), srvId));
    mDnssdPlatform->StartTxtResolver(
        txtResolver1, std::make_unique<otbr::DnssdPlatform::StdTxtCallback>(mockTxtCallback1.AsStdFunction(), txtId1));
    mDnssdPlatform->StartTxtResolver(
        txtResolver2, std::make_unique<otbr::DnssdPlatform::StdTxtCallback>(mockTxtCallback2.AsStdFunction(), txtId2));
    ProcessMainloop();
    Mock::VerifyAndClearExpectations(mPublisher.get());

    // 2. A result reported on the second interface reaches the TXT resolver of that interface only.
    discoveredInstanceInfo.mRemoved  = false;
    discoveredInstanceInfo.mName     = serviceInstance;
    discoveredInstanceInfo.mHostName = "Eternal.";
    discoveredInstanceInfo.mTtl      = 10;

    EXPECT_CALL(mockSrvCallback, Call(_)).Times(0);
    EXPECT_CALL(mockTxtCallback1, Call(_)).Times(0);
    EXPECT_CALL(mockTxtCallback2, Call(_)).WillOnce([&](const otbr::DnssdPlatform::TxtResult &aResult) {
        EXPECT_EQ(aResult.mInfraIfIndex, kInfraIfIndex2);
        EXPECT_EQ(aResult.mTxtDataLength, 2);
    });

    discoveredInstanceInfo.mNetifIndex = kInfraIfIndex2;
    discoveredInstanceInfo.mTxtData    = {1, 'b'};
    mPublisher->TestOnServiceResolved(serviceType, discoveredInstanceInfo);
    ProcessMainloop();
    Mock::VerifyAndClearExpectations(&mockSrvCallback);
    Mock::VerifyAndClearExpectations(&mockTxtCallback1);
    Mock::VerifyAndClearExpectations(&mockTxtCallback2);

    // 3. A result reported on the first interface reaches the SRV and the TXT resolver of that interface only.
    EXPECT_CALL(mockSrvCallback, Call(_)).WillOnce([&](const otbr::DnssdPlatform::SrvResult &aResult) {
        EXPECT_EQ(aResult.mInfraIfIndex, kInfraIfIndex1);
        EXPECT_EQ(aResult.mPort, 11);
    });
    EXPECT_CALL(mockTxtCallback1, Call(_)).WillOnce([&](const otbr::DnssdPlatform::TxtResult &aResult) {
        EXPECT_EQ(aResult.mInfraIfIndex, kInfraIfIndex1);
        EXPECT_EQ(aResult.mTxtDataLength, 4);
    });
    EXPECT_CALL(mockTxtCallback2, Call(_)).Times(0);

    discoveredInstanceInfo.mNetifIndex = kInfraIfIndex1;
    discoveredInstanceInfo.mPort       = 11;
    discoveredInstanceInfo.mTxtData    = {3, 'a', '=', '1'};
    mPublisher->TestOnServiceResolved(serviceType, discoveredInstanceInfo);
    ProcessMainloop();
    Mock::VerifyAndClearExpectations(&mockSrvCallback);
    Mock::VerifyAndClearExpectations(&mockTxtCallback1);
    Mock::VerifyAndClearExpectations(&mockTxtCallback2);

    // 4. The subscription of the first interface stays as long as one of its resolvers does.
    EXPECT_CALL(*mPublisher, UnsubscribeService(_, _, _)).Times(0);

    mDnssdPlatform->StopServiceResolver(srvResolver, otbr::DnssdPlatform::StdSrvCallback(nullptr, srvId));
    ProcessMainloop();
    Mock::VerifyAndClearExpectations(mPublisher.get());

    EXPECT_CALL(*mPublisher, UnsubscribeService(StrEq(serviceType), StrEq(serviceInstance), kInfraIfIndex1)).Times(1);
    EXPECT_CALL(*mPublisher, UnsubscribeService(StrEq(serviceType), StrEq(serviceInstance), kInfraIfIndex2)).Times(0);

    mDnssdPlatform->StopTxtResolver(txtResolver1, otbr::DnssdPlatform::StdTxtCallback(nullptr, txtId1));
    ProcessMainloop();
    Mock::VerifyAndClearExpectations(mPublisher.get());

    EXPECT_CALL(*mPublisher, UnsubscribeService(StrEq(serviceType), StrEq(serviceInstance), kInfraIfIndex2)).Times(1);

    mDnssdPlatform->StopTxtResolver(txtResolver2, otbr::DnssdPlatform::StdTxtCallback(nullptr, txtId2));
    ProcessMainloop();
}

TEST_F(DnssdTest, TestAddressResolversOfTwoInfraIfsHaveTheirOwnSubscriptionAndResults)
{
    otbr::DnssdPlatform::AddressResolver                           resolver1;
    otbr::DnssdPlatform::AddressResolver                           resolver2;
    otbr::Mdns::Publisher::DiscoveredHostInfo                      discoveredHostInfo;
    const char                                                    *hostName = "Eternal";
    MockFunction<void(const otbr::DnssdPlatform::AddressResult &)> mockCallback1;
    MockFunction<void(const otbr::DnssdPlatform::AddressResult &)> mockCallback2;
    uint64_t                                                       id1 = 1;
    uint64_t                                                       id2 = 2;

    resolver1.mHostName     = hostName;
    resolver1.mInfraIfIndex = kInfraIfIndex1;
    resolver1.mCallback     = nullptr;

    resolver2               = resolver1;
    resolver2.mInfraIfIndex = kInfraIfIndex2;

    // 1. Two resolvers for the same host on two interfaces lead to one subscription per interface.
    EXPECT_CALL(*mPublisher, SubscribeHost(StrEq(hostName), kInfraIfIndex1)).Times(1);
    EXPECT_CALL(*mPublisher, SubscribeHost(StrEq(hostName), kInfraIfIndex2)).Times(1);

    mDnssdPlatform->StartIp6AddressResolver(
        resolver1, std::make_unique<otbr::DnssdPlatform::StdAddressCallback>(mockCallback1.AsStdFunction(), id1));
    mDnssdPlatform->StartIp6AddressResolver(
        resolver2, std::make_unique<otbr::DnssdPlatform::StdAddressCallback>(mockCallback2.AsStdFunction(), id2));
    ProcessMainloop();
    Mock::VerifyAndClearExpectations(mPublisher.get());

    // 2. Each resolver gets the results reported on its own interface only.
    discoveredHostInfo.mHostName = "Eternal.local.";
    discoveredHostInfo.mTtl      = 10;

    EXPECT_CALL(mockCallback1, Call(_)).WillOnce([&](const otbr::DnssdPlatform::AddressResult &aResult) {
        EXPECT_EQ(aResult.mInfraIfIndex, kInfraIfIndex1);
        EXPECT_EQ(aResult.mAddressesLength, 1);
    });
    EXPECT_CALL(mockCallback2, Call(_)).Times(0);

    discoveredHostInfo.mNetifIndex = kInfraIfIndex1;
    discoveredHostInfo.mAddresses  = {otbr::Ip6Address("2002::1")};
    mPublisher->TestOnHostResolved(hostName, discoveredHostInfo);
    ProcessMainloop();
    Mock::VerifyAndClearExpectations(&mockCallback1);
    Mock::VerifyAndClearExpectations(&mockCallback2);

    EXPECT_CALL(mockCallback1, Call(_)).Times(0);
    EXPECT_CALL(mockCallback2, Call(_)).WillOnce([&](const otbr::DnssdPlatform::AddressResult &aResult) {
        EXPECT_EQ(aResult.mInfraIfIndex, kInfraIfIndex2);
        EXPECT_EQ(aResult.mAddressesLength, 2);
    });

    discoveredHostInfo.mNetifIndex = kInfraIfIndex2;
    discoveredHostInfo.mAddresses  = {otbr::Ip6Address("2002::1"), otbr::Ip6Address("2002::2")};
    mPublisher->TestOnHostResolved(hostName, discoveredHostInfo);
    ProcessMainloop();
    Mock::VerifyAndClearExpectations(&mockCallback1);
    Mock::VerifyAndClearExpectations(&mockCallback2);

    // 3. Stopping the resolver of the first interface removes the subscription of that interface only.
    EXPECT_CALL(*mPublisher, UnsubscribeHost(StrEq(hostName), kInfraIfIndex1)).Times(1);
    EXPECT_CALL(*mPublisher, UnsubscribeHost(StrEq(hostName), kInfraIfIndex2)).Times(0);

    mDnssdPlatform->StopIp6AddressResolver(resolver1, otbr::DnssdPlatform::StdAddressCallback(nullptr, id1));
    ProcessMainloop();
    Mock::VerifyAndClearExpectations(mPublisher.get());

    // 4. The resolver of the second interface still gets the results reported on its interface.
    EXPECT_CALL(mockCallback1, Call(_)).Times(0);
    EXPECT_CALL(mockCallback2, Call(_)).WillOnce([&](const otbr::DnssdPlatform::AddressResult &aResult) {
        EXPECT_EQ(aResult.mInfraIfIndex, kInfraIfIndex2);
        EXPECT_EQ(aResult.mAddressesLength, 3);
    });

    discoveredHostInfo.mAddresses  = {otbr::Ip6Address("2002::1"), otbr::Ip6Address("2002::2"),
                                      otbr::Ip6Address("2002::3")};
    discoveredHostInfo.mNetifIndex = kInfraIfIndex1;
    mPublisher->TestOnHostResolved(hostName, discoveredHostInfo);
    discoveredHostInfo.mNetifIndex = kInfraIfIndex2;
    mPublisher->TestOnHostResolved(hostName, discoveredHostInfo);
    ProcessMainloop();
    Mock::VerifyAndClearExpectations(&mockCallback1);
    Mock::VerifyAndClearExpectations(&mockCallback2);

    EXPECT_CALL(*mPublisher, UnsubscribeHost(StrEq(hostName), kInfraIfIndex2)).Times(1);

    mDnssdPlatform->StopIp6AddressResolver(resolver2, otbr::DnssdPlatform::StdAddressCallback(nullptr, id2));
    ProcessMainloop();
}

#endif // OTBR_ENABLE_DNSSD_PLAT
