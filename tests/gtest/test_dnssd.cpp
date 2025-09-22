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

#if OTBR_ENABLE_DNSSD_PLAT

using ::testing::_;
using ::testing::MockFunction;
using ::testing::SaveArg;
using ::testing::StrEq;

class MockMdnsPublisher : public otbr::Mdns::Publisher
{
public:
    MockMdnsPublisher(void)           = default;
    ~MockMdnsPublisher(void) override = default;

    MOCK_METHOD(otbrError,
                PublishServiceImpl,
                (const std::string &aHostName,
                 const std::string &aName,
                 const std::string &aType,
                 const SubTypeList &aSubTypeList,
                 uint16_t           aPort,
                 const TxtData     &aTxtData,
                 ResultCallback   &&aCallback),
                (override));
    MOCK_METHOD(void,
                UnpublishService,
                (const std::string &aName, const std::string &aType, ResultCallback &&aCallback),
                (override));
    MOCK_METHOD(otbrError,
                PublishHostImpl,
                (const std::string &aName, const AddressList &aAddresses, ResultCallback &&aCallback),
                (override));
    MOCK_METHOD(void, UnpublishHost, (const std::string &aName, ResultCallback &&aCallback), (override));
    MOCK_METHOD(otbrError,
                PublishKeyImpl,
                (const std::string &aName, const KeyData &aKey, ResultCallback &&aCallback),
                (override));
    MOCK_METHOD(void, UnpublishKey, (const std::string &aName, ResultCallback &&aCallback), (override));
    MOCK_METHOD(void, SubscribeService, (const std::string &aType, const std::string &aInstanceName), (override));
    MOCK_METHOD(void, UnsubscribeService, (const std::string &aType, const std::string &aInstanceName), (override));
    MOCK_METHOD(void, SubscribeHost, (const std::string &aHostName), (override));
    MOCK_METHOD(void, UnsubscribeHost, (const std::string &aHostName), (override));

    otbrError Start(void) override { return OTBR_ERROR_NONE; }
    void      Stop(void) override {}
    bool      IsStarted(void) const override { return true; }

    void OnServiceResolveFailedImpl(const std::string &aType,
                                    const std::string &aInstanceName,
                                    int32_t            aErrorCode) override
    {
        OTBR_UNUSED_VARIABLE(aType);
        OTBR_UNUSED_VARIABLE(aInstanceName);
        OTBR_UNUSED_VARIABLE(aErrorCode);
    }

    void OnHostResolveFailedImpl(const std::string &aHostName, int32_t aErrorCode) override
    {
        OTBR_UNUSED_VARIABLE(aHostName);
        OTBR_UNUSED_VARIABLE(aErrorCode);
    }

    otbrError DnsErrorToOtbrError(int32_t aError) override
    {
        OTBR_UNUSED_VARIABLE(aError);
        return OTBR_ERROR_NONE;
    }

    void TestOnServiceResolved(std::string aType, otbr::Mdns::Publisher::DiscoveredInstanceInfo aInstanceInfo)
    {
        OnServiceResolved(std::move(aType), std::move(aInstanceInfo));
    }
};

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
    constexpr uint8_t kInfraIfIndex = 1;

    otbr::DnssdPlatform::Browser                                  browser;
    otbr::Mdns::Publisher::DiscoveredInstanceInfo                 discoveredInstanceInfo;
    const char                                                   *serviceType = "_plant._tcp";
    MockFunction<void(const otbr::DnssdPlatform::BrowseResult &)> mockCallback;

    browser.mServiceType  = serviceType;
    browser.mSubTypeLabel = nullptr;
    browser.mInfraIfIndex = kInfraIfIndex;
    browser.mCallback     = nullptr;

    // 1. A service is resovled and expect the callback is invoked.
    EXPECT_CALL(*mPublisher, SubscribeService(StrEq(serviceType), StrEq("")));

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
    EXPECT_CALL(*mPublisher, UnsubscribeService(StrEq(serviceType), StrEq("")));

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
    constexpr uint8_t kInfraIfIndex = 1;

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
    EXPECT_CALL(*mPublisher, SubscribeService(StrEq(serviceType), StrEq(resolver1.mServiceInstance))).Times(1);
    EXPECT_CALL(*mPublisher, UnsubscribeService(StrEq(serviceType), StrEq(resolver1.mServiceInstance))).Times(1);
    EXPECT_CALL(*mPublisher, SubscribeService(StrEq(serviceType), StrEq(resolver2.mServiceInstance))).Times(1);

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

#endif // OTBR_ENABLE_DNSSD_PLAT
