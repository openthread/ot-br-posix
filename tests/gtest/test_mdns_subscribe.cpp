/*
 *    Copyright (c) 2023, The OpenThread Authors.
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
#include <limits.h>
#include <netinet/in.h>
#include <signal.h>

#include <set>
#include <vector>

#include "common/mainloop.hpp"
#include "common/mainloop_manager.hpp"
#include "mdns/mdns.hpp"

using namespace otbr;
using namespace otbr::Mdns;

static constexpr int kTimeoutSeconds = 3;

int RunMainloopUntilTimeout(int aSeconds)
{
    using namespace otbr;

    int  rval      = 0;
    auto beginTime = Clock::now();

    while (true)
    {
        MainloopContext mainloop;

        mainloop.mMaxFd   = -1;
        mainloop.mTimeout = {1, 0};
        FD_ZERO(&mainloop.mReadFdSet);
        FD_ZERO(&mainloop.mWriteFdSet);
        FD_ZERO(&mainloop.mErrorFdSet);

        MainloopManager::GetInstance().Update(mainloop);
        rval = select(mainloop.mMaxFd + 1, &mainloop.mReadFdSet, &mainloop.mWriteFdSet, &mainloop.mErrorFdSet,
                      (mainloop.mTimeout.tv_sec == INT_MAX ? nullptr : &mainloop.mTimeout));

        if (rval < 0)
        {
            perror("select");
            break;
        }

        MainloopManager::GetInstance().Process(mainloop);

        if (Clock::now() - beginTime >= std::chrono::seconds(aSeconds))
        {
            break;
        }
    }

    return rval;
}

template <typename Container> std::set<typename Container::value_type> AsSet(const Container &aContainer)
{
    return std::set<typename Container::value_type>(aContainer.begin(), aContainer.end());
}

Publisher::ResultCallback NoOpCallback(void)
{
    return [](otbrError aError) { OTBR_UNUSED_VARIABLE(aError); };
}

std::map<std::string, std::vector<uint8_t>> AsTxtMap(const Publisher::TxtData &aTxtData)
{
    Publisher::TxtList                          txtList;
    std::map<std::string, std::vector<uint8_t>> map;

    Publisher::DecodeTxtData(txtList, aTxtData.data(), aTxtData.size());
    for (const auto &entry : txtList)
    {
        map[entry.mKey] = entry.mValue;
    }

    return map;
}

Publisher::TxtList sTxtList1{{"a", "1"}, {"b", "2"}};
Publisher::TxtData sTxtData1;
Ip6Address         sAddr1;
Ip6Address         sAddr2;
Ip6Address         sAddr3;
Ip6Address         sAddr4;

class MdnsTest : public ::testing::Test
{
protected:
    MdnsTest()
    {
        SuccessOrDie(Ip6Address::FromString("2002::1", sAddr1), "");
        SuccessOrDie(Ip6Address::FromString("2002::2", sAddr2), "");
        SuccessOrDie(Ip6Address::FromString("2002::3", sAddr3), "");
        SuccessOrDie(Ip6Address::FromString("2002::4", sAddr4), "");
        SuccessOrDie(Publisher::EncodeTxtData(sTxtList1, sTxtData1), "");
    }
};

std::unique_ptr<Publisher> CreatePublisher(void)
{
    bool                       ready = false;
    std::unique_ptr<Publisher> publisher{Publisher::Create([&ready](Mdns::Publisher::State aState) {
        if (aState == Publisher::State::kReady)
        {
            ready = true;
        }
    })};

    publisher->Start();
    RunMainloopUntilTimeout(kTimeoutSeconds);
    EXPECT_TRUE(ready);

    return publisher;
}

void CheckServiceInstance(const Publisher::DiscoveredInstanceInfo aInstanceInfo,
                          bool                                    aRemoved,
                          const std::string                      &aHostName,
                          const std::vector<Ip6Address>          &aAddresses,
                          const std::string                      &aServiceName,
                          uint16_t                                aPort,
                          const Publisher::TxtData                aTxtData)
{
    EXPECT_EQ(aRemoved, aInstanceInfo.mRemoved);
    EXPECT_EQ(aServiceName, aInstanceInfo.mName);
    if (!aRemoved)
    {
        EXPECT_EQ(aHostName, aInstanceInfo.mHostName);
        EXPECT_EQ(AsSet(aAddresses), AsSet(aInstanceInfo.mAddresses));
        EXPECT_EQ(aPort, aInstanceInfo.mPort);
        EXPECT_TRUE(AsTxtMap(aTxtData) == AsTxtMap(aInstanceInfo.mTxtData));
    }
}

void CheckServiceInstanceAdded(const Publisher::DiscoveredInstanceInfo aInstanceInfo,
                               const std::string                      &aHostName,
                               const std::vector<Ip6Address>          &aAddresses,
                               const std::string                      &aServiceName,
                               uint16_t                                aPort,
                               const Publisher::TxtData                aTxtData)
{
    CheckServiceInstance(aInstanceInfo, false, aHostName, aAddresses, aServiceName, aPort, aTxtData);
}

void CheckServiceInstanceRemoved(const Publisher::DiscoveredInstanceInfo aInstanceInfo, const std::string &aServiceName)
{
    CheckServiceInstance(aInstanceInfo, true, "", {}, aServiceName, 0, {});
}

void CheckHostAdded(const Publisher::DiscoveredHostInfo &aHostInfo,
                    const std::string                   &aHostName,
                    const std::vector<Ip6Address>       &aAddresses)
{
    EXPECT_EQ(aHostName, aHostInfo.mHostName);
    EXPECT_EQ(AsSet(aAddresses), AsSet(aHostInfo.mAddresses));
}

TEST_F(MdnsTest, SubscribeHost)
{
    std::unique_ptr<Publisher>    pub = CreatePublisher();
    std::string                   lastHostName;
    Publisher::DiscoveredHostInfo lastHostInfo{};

    auto clearLastHost = [&lastHostName, &lastHostInfo] {
        lastHostName = "";
        lastHostInfo = {};
    };

    pub->AddSubscriptionCallbacks(
        nullptr,
        [&lastHostName, &lastHostInfo](const std::string &aHostName, const Publisher::DiscoveredHostInfo &aHostInfo) {
            lastHostName = aHostName;
            lastHostInfo = aHostInfo;
        });
    pub->SubscribeHost("host1");

    pub->PublishHost("host1", Publisher::AddressList{sAddr1, sAddr2}, NoOpCallback());
    pub->PublishService("host1", "service1", "_test._tcp", Publisher::SubTypeList{"_sub1", "_sub2"}, 11111, sTxtData1,
                        NoOpCallback());
    RunMainloopUntilTimeout(kTimeoutSeconds);
    EXPECT_EQ("host1", lastHostName);
    CheckHostAdded(lastHostInfo, "host1.local.", {sAddr1, sAddr2});
    clearLastHost();

    pub->PublishService("host1", "service2", "_test._tcp", {}, 22222, {}, NoOpCallback());
    RunMainloopUntilTimeout(kTimeoutSeconds);
    EXPECT_EQ("", lastHostName);
    clearLastHost();

    pub->PublishHost("host2", Publisher::AddressList{sAddr3}, NoOpCallback());
    pub->PublishService("host2", "service3", "_test._tcp", {}, 33333, {}, NoOpCallback());
    RunMainloopUntilTimeout(kTimeoutSeconds);
    EXPECT_EQ("", lastHostName);
    clearLastHost();
}

TEST_F(MdnsTest, SubscribeServiceInstance)
{
    std::unique_ptr<Publisher>        pub = CreatePublisher();
    std::string                       lastServiceType;
    Publisher::DiscoveredInstanceInfo lastInstanceInfo{};

    auto clearLastInstance = [&lastServiceType, &lastInstanceInfo] {
        lastServiceType  = "";
        lastInstanceInfo = {};
    };

    pub->AddSubscriptionCallbacks(
        [&lastServiceType, &lastInstanceInfo](const std::string                &aType,
                                              Publisher::DiscoveredInstanceInfo aInstanceInfo) {
            lastServiceType  = aType;
            lastInstanceInfo = aInstanceInfo;
        },
        nullptr);
    pub->SubscribeService("_test._tcp", "service1");

    pub->PublishHost("host1", Publisher::AddressList{sAddr1, sAddr2}, NoOpCallback());
    pub->PublishService("host1", "service1", "_test._tcp", Publisher::SubTypeList{"_sub1", "_sub2"}, 11111, sTxtData1,
                        NoOpCallback());
    RunMainloopUntilTimeout(kTimeoutSeconds);
    EXPECT_EQ("_test._tcp", lastServiceType);
    CheckServiceInstanceAdded(lastInstanceInfo, "host1.local.", {sAddr1, sAddr2}, "service1", 11111, sTxtData1);
    clearLastInstance();

    pub->PublishService("host1", "service2", "_test._tcp", {}, 22222, {}, NoOpCallback());
    RunMainloopUntilTimeout(kTimeoutSeconds);
    EXPECT_EQ("", lastServiceType);
    clearLastInstance();

    pub->PublishHost("host2", Publisher::AddressList{sAddr3}, NoOpCallback());
    pub->PublishService("host2", "service3", "_test._tcp", {}, 33333, {}, NoOpCallback());
    RunMainloopUntilTimeout(kTimeoutSeconds);
    EXPECT_EQ("", lastServiceType);
    clearLastInstance();
}

TEST_F(MdnsTest, SubscribeServiceType)
{
    std::unique_ptr<Publisher>        pub = CreatePublisher();
    std::string                       lastServiceType;
    Publisher::DiscoveredInstanceInfo lastInstanceInfo{};

    auto clearLastInstance = [&lastServiceType, &lastInstanceInfo] {
        lastServiceType  = "";
        lastInstanceInfo = {};
    };

    pub->AddSubscriptionCallbacks(
        [&lastServiceType, &lastInstanceInfo](const std::string                &aType,
                                              Publisher::DiscoveredInstanceInfo aInstanceInfo) {
            lastServiceType  = aType;
            lastInstanceInfo = aInstanceInfo;
        },
        nullptr);
    pub->SubscribeService("_test._tcp", "");

    pub->PublishHost("host1", Publisher::AddressList{sAddr1, sAddr2}, NoOpCallback());
    pub->PublishService("host1", "service1", "_test._tcp", Publisher::SubTypeList{"_sub1", "_sub2"}, 11111, sTxtData1,
                        NoOpCallback());
    RunMainloopUntilTimeout(kTimeoutSeconds);
    EXPECT_EQ("_test._tcp", lastServiceType);
    CheckServiceInstanceAdded(lastInstanceInfo, "host1.local.", {sAddr1, sAddr2}, "service1", 11111, sTxtData1);
    clearLastInstance();

    pub->PublishService("host1", "service2", "_test._tcp", {}, 22222, {}, NoOpCallback());
    RunMainloopUntilTimeout(kTimeoutSeconds);
    EXPECT_EQ("_test._tcp", lastServiceType);
    CheckServiceInstanceAdded(lastInstanceInfo, "host1.local.", {sAddr1, sAddr2}, "service2", 22222, {});
    clearLastInstance();

    pub->PublishHost("host2", Publisher::AddressList{sAddr3}, NoOpCallback());
    pub->PublishService("host2", "service3", "_test._tcp", {}, 33333, {}, NoOpCallback());
    RunMainloopUntilTimeout(kTimeoutSeconds);
    EXPECT_EQ("_test._tcp", lastServiceType);
    CheckServiceInstanceAdded(lastInstanceInfo, "host2.local.", {sAddr3}, "service3", 33333, {});
    clearLastInstance();

    pub->UnpublishHost("host2", NoOpCallback());
    pub->UnpublishService("service3", "_test._tcp", NoOpCallback());
    RunMainloopUntilTimeout(kTimeoutSeconds);
    EXPECT_EQ("_test._tcp", lastServiceType);
    CheckServiceInstanceRemoved(lastInstanceInfo, "service3");
    clearLastInstance();

    pub->PublishHost("host2", {sAddr3}, NoOpCallback());
    pub->PublishService("host2", "service3", "_test._tcp", {}, 44444, {}, NoOpCallback());
    pub->PublishHost("host2", {sAddr3, sAddr4}, NoOpCallback());
    RunMainloopUntilTimeout(kTimeoutSeconds);
    EXPECT_EQ("_test._tcp", lastServiceType);
    CheckServiceInstanceAdded(lastInstanceInfo, "host2.local.", {sAddr3, sAddr4}, "service3", 44444, {});
    clearLastInstance();

    pub->PublishHost("host2", {sAddr4}, NoOpCallback());
    RunMainloopUntilTimeout(kTimeoutSeconds);
    EXPECT_EQ("_test._tcp", lastServiceType);
    CheckServiceInstanceAdded(lastInstanceInfo, "host2.local.", {sAddr4}, "service3", 44444, {});
    clearLastInstance();
}
