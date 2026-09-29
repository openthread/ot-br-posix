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
#include <ifaddrs.h>
#include <limits.h>
#include <net/if.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>

#include <set>
#include <vector>

#include "common/mainloop.hpp"
#include "common/mainloop_manager.hpp"
#include "host/posix/dnssd.hpp"
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

static const char kNoTestNetifMessage[] =
    "found no network interface that is up and running, supports multicast, is neither a loopback nor a "
    "point-to-point interface, and has an IPv4 address";

// The names that one test publishes. No other test publishes them, and no other run of the tests does, so that a
// test never gets what another one published.
struct TestNames
{
    TestNames(void)
    {
        static int  sCount = 0;
        std::string label  = std::to_string(getpid()) + "-" + std::to_string(++sCount);

        mHost     = "host-" + label;
        mFullHost = mHost + ".local.";
        mInstance = "service-" + label;
        mType     = "_t" + label + "._tcp";
    }

    std::string mHost;     // The host name, without domain.
    std::string mFullHost; // The full host name.
    std::string mInstance; // The service instance name.
    std::string mType;     // The service type.
};

// Returns the index of the network interface the tests on one network interface use: the first one that is up and
// running, supports multicast, is neither a loopback nor a point-to-point interface, and has an IPv4 address.
// Returns 0 if there is none.
uint32_t GetTestNetifIndex(void)
{
    const unsigned int kRequiredFlags = IFF_UP | IFF_RUNNING | IFF_MULTICAST;
    const unsigned int kExcludedFlags = IFF_LOOPBACK | IFF_POINTOPOINT;

    uint32_t        netifIndex = 0;
    struct ifaddrs *ifAddrs    = nullptr;

    if (getifaddrs(&ifAddrs) != 0)
    {
        ifAddrs = nullptr;
    }

    for (const struct ifaddrs *ifAddr = ifAddrs; ifAddr != nullptr && netifIndex == 0; ifAddr = ifAddr->ifa_next)
    {
        if (ifAddr->ifa_addr == nullptr || ifAddr->ifa_addr->sa_family != AF_INET)
        {
            continue;
        }

        if ((ifAddr->ifa_flags & kRequiredFlags) != kRequiredFlags || (ifAddr->ifa_flags & kExcludedFlags) != 0)
        {
            continue;
        }

        netifIndex = if_nametoindex(ifAddr->ifa_name);
    }

    if (ifAddrs != nullptr)
    {
        freeifaddrs(ifAddrs);
    }

    return netifIndex;
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
    pub->SubscribeHost("host1", Publisher::kNetifIndexAny);

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
    pub->SubscribeService("_test._tcp", "service1", Publisher::kNetifIndexAny);

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
    pub->SubscribeService("_test._tcp", "", Publisher::kNetifIndexAny);

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

// Subscribes to a host on one network interface. Every notification must carry that network interface index.
TEST_F(MdnsTest, SubscribeHostOnOneNetif)
{
    uint32_t                      netifIndex = GetTestNetifIndex();
    int                           notified   = 0;
    TestNames                     names;
    std::string                   lastHostName;
    Publisher::DiscoveredHostInfo lastHostInfo{};

    if (netifIndex == 0)
    {
        GTEST_SKIP() << kNoTestNetifMessage;
    }

    std::unique_ptr<Publisher> pub = CreatePublisher();

    pub->AddSubscriptionCallbacks(nullptr,
                                  [&](const std::string &aHostName, const Publisher::DiscoveredHostInfo &aHostInfo) {
                                      EXPECT_EQ(netifIndex, aHostInfo.mNetifIndex);
                                      notified++;
                                      lastHostName = aHostName;
                                      lastHostInfo = aHostInfo;
                                  });
    pub->SubscribeHost(names.mHost, netifIndex);

    pub->PublishHost(names.mHost, Publisher::AddressList{sAddr1, sAddr2}, NoOpCallback());
    RunMainloopUntilTimeout(kTimeoutSeconds);
    // One notification per reply that adds an address on the network interface, at most. A reply ignored for
    // its network interface does not report the unchanged addresses again.
    EXPECT_GE(notified, 1);
    EXPECT_LE(notified, 2);
    EXPECT_EQ(names.mHost, lastHostName);
    CheckHostAdded(lastHostInfo, names.mFullHost, {sAddr1, sAddr2});
    notified = 0;

    // A change of the addresses notifies a subscription that is alive.
    pub->PublishHost(names.mHost, Publisher::AddressList{sAddr1, sAddr2, sAddr3}, NoOpCallback());
    RunMainloopUntilTimeout(kTimeoutSeconds);
    EXPECT_GE(notified, 1);
    CheckHostAdded(lastHostInfo, names.mFullHost, {sAddr1, sAddr2, sAddr3});

    // It does not notify a subscription that is removed.
    pub->UnsubscribeHost(names.mHost, netifIndex);
    notified = 0;

    pub->PublishHost(names.mHost, Publisher::AddressList{sAddr1, sAddr2, sAddr3, sAddr4}, NoOpCallback());
    RunMainloopUntilTimeout(kTimeoutSeconds);
    EXPECT_EQ(0, notified);
}

// Subscribes to a service instance on one network interface. Every notification must carry that network interface
// index.
TEST_F(MdnsTest, SubscribeServiceInstanceOnOneNetif)
{
    uint32_t                          netifIndex = GetTestNetifIndex();
    int                               notified   = 0;
    TestNames                         names;
    std::string                       lastServiceType;
    Publisher::DiscoveredInstanceInfo lastInstanceInfo{};

    if (netifIndex == 0)
    {
        GTEST_SKIP() << kNoTestNetifMessage;
    }

    std::unique_ptr<Publisher> pub = CreatePublisher();

    pub->AddSubscriptionCallbacks(
        [&](const std::string &aType, Publisher::DiscoveredInstanceInfo aInstanceInfo) {
            EXPECT_EQ(netifIndex, aInstanceInfo.mNetifIndex);
            notified++;
            lastServiceType  = aType;
            lastInstanceInfo = aInstanceInfo;
        },
        nullptr);
    pub->SubscribeService(names.mType, names.mInstance, netifIndex);

    pub->PublishHost(names.mHost, Publisher::AddressList{sAddr1, sAddr2}, NoOpCallback());
    pub->PublishService(names.mHost, names.mInstance, names.mType, {}, 11111, sTxtData1, NoOpCallback());
    RunMainloopUntilTimeout(kTimeoutSeconds);
    EXPECT_GE(notified, 1);
    EXPECT_EQ(names.mType, lastServiceType);
    CheckServiceInstanceAdded(lastInstanceInfo, names.mFullHost, {sAddr1, sAddr2}, names.mInstance, 11111, sTxtData1);
    notified = 0;

    // The resolution stops at the first resolve reply, but the address lookup of the resolved instance goes on. So
    // a change of the addresses of the host notifies a subscription that is alive.
    pub->PublishHost(names.mHost, Publisher::AddressList{sAddr1, sAddr2, sAddr3}, NoOpCallback());
    RunMainloopUntilTimeout(kTimeoutSeconds);
    EXPECT_GE(notified, 1);
    CheckServiceInstanceAdded(lastInstanceInfo, names.mFullHost, {sAddr1, sAddr2, sAddr3}, names.mInstance, 11111,
                              sTxtData1);

    // It does not notify a subscription that is removed.
    pub->UnsubscribeService(names.mType, names.mInstance, netifIndex);
    notified = 0;

    pub->PublishHost(names.mHost, Publisher::AddressList{sAddr1, sAddr2, sAddr3, sAddr4}, NoOpCallback());
    RunMainloopUntilTimeout(kTimeoutSeconds);
    EXPECT_EQ(0, notified);
}

// Subscribes to a service type on one network interface. Every notification must carry that network interface
// index.
TEST_F(MdnsTest, SubscribeServiceTypeOnOneNetif)
{
    uint32_t                          netifIndex = GetTestNetifIndex();
    int                               notified   = 0;
    TestNames                         names;
    std::string                       lastServiceType;
    Publisher::DiscoveredInstanceInfo lastInstanceInfo{};

    if (netifIndex == 0)
    {
        GTEST_SKIP() << kNoTestNetifMessage;
    }

    std::unique_ptr<Publisher> pub = CreatePublisher();

    pub->AddSubscriptionCallbacks(
        [&](const std::string &aType, Publisher::DiscoveredInstanceInfo aInstanceInfo) {
            EXPECT_EQ(netifIndex, aInstanceInfo.mNetifIndex);
            notified++;
            lastServiceType  = aType;
            lastInstanceInfo = aInstanceInfo;
        },
        nullptr);
    pub->SubscribeService(names.mType, "", netifIndex);

    // A new instance of the service type notifies a subscription that is alive.
    pub->PublishHost(names.mHost, Publisher::AddressList{sAddr1, sAddr2}, NoOpCallback());
    pub->PublishService(names.mHost, names.mInstance, names.mType, {}, 11111, sTxtData1, NoOpCallback());
    RunMainloopUntilTimeout(kTimeoutSeconds);
    EXPECT_GE(notified, 1);
    EXPECT_EQ(names.mType, lastServiceType);
    CheckServiceInstanceAdded(lastInstanceInfo, names.mFullHost, {sAddr1, sAddr2}, names.mInstance, 11111, sTxtData1);
    notified = 0;

    pub->UnpublishService(names.mInstance, names.mType, NoOpCallback());
    RunMainloopUntilTimeout(kTimeoutSeconds);
    EXPECT_GE(notified, 1);
    EXPECT_EQ(names.mType, lastServiceType);
    CheckServiceInstanceRemoved(lastInstanceInfo, names.mInstance);

    // It does not notify a subscription that is removed.
    pub->UnsubscribeService(names.mType, "", netifIndex);
    notified = 0;

    pub->PublishService(names.mHost, names.mInstance + "-2", names.mType, {}, 22222, {}, NoOpCallback());
    RunMainloopUntilTimeout(kTimeoutSeconds);
    EXPECT_EQ(0, notified);
}

// Subscribes to the same service instance on one network interface and on every network interface. Removing one of
// the two subscriptions must leave the other one alive.
TEST_F(MdnsTest, SubscribeServiceInstanceOnOneNetifAndOnEveryNetif)
{
    uint32_t netifIndex = GetTestNetifIndex();

    if (netifIndex == 0)
    {
        GTEST_SKIP() << kNoTestNetifMessage;
    }

    for (uint32_t keptNetifIndex : {netifIndex, static_cast<uint32_t>(Publisher::kNetifIndexAny)})
    {
        uint32_t  removedNetifIndex  = (keptNetifIndex == netifIndex) ? Publisher::kNetifIndexAny : netifIndex;
        bool      isOnlyOneNetifLeft = false;
        int       notified           = 0;
        TestNames names;
        Publisher::DiscoveredInstanceInfo lastInstanceInfo{};
        std::unique_ptr<Publisher>        pub = CreatePublisher();

        pub->AddSubscriptionCallbacks(
            [&](const std::string &aType, Publisher::DiscoveredInstanceInfo aInstanceInfo) {
                EXPECT_EQ(names.mType, aType);
                if (isOnlyOneNetifLeft)
                {
                    EXPECT_EQ(netifIndex, aInstanceInfo.mNetifIndex);
                }
                notified++;
                lastInstanceInfo = aInstanceInfo;
            },
            nullptr);
        pub->SubscribeService(names.mType, names.mInstance, netifIndex);
        pub->SubscribeService(names.mType, names.mInstance, Publisher::kNetifIndexAny);

        pub->PublishHost(names.mHost, Publisher::AddressList{sAddr1, sAddr2}, NoOpCallback());
        pub->PublishService(names.mHost, names.mInstance, names.mType, {}, 11111, sTxtData1, NoOpCallback());
        RunMainloopUntilTimeout(kTimeoutSeconds);
        EXPECT_GE(notified, 1);

        // A change of the addresses of the host notifies the subscription that is left.
        pub->UnsubscribeService(names.mType, names.mInstance, removedNetifIndex);
        isOnlyOneNetifLeft = (keptNetifIndex == netifIndex);
        notified           = 0;

        pub->PublishHost(names.mHost, Publisher::AddressList{sAddr1, sAddr2, sAddr3}, NoOpCallback());
        RunMainloopUntilTimeout(kTimeoutSeconds);
        EXPECT_GE(notified, 1) << "no notification for the subscription on network interface " << keptNetifIndex;
        CheckServiceInstanceAdded(lastInstanceInfo, names.mFullHost, {sAddr1, sAddr2, sAddr3}, names.mInstance, 11111,
                                  sTxtData1);

        // It notifies nobody when both subscriptions are removed.
        pub->UnsubscribeService(names.mType, names.mInstance, keptNetifIndex);
        isOnlyOneNetifLeft = false;
        notified           = 0;

        pub->PublishHost(names.mHost, Publisher::AddressList{sAddr1, sAddr2, sAddr3, sAddr4}, NoOpCallback());
        RunMainloopUntilTimeout(kTimeoutSeconds);
        EXPECT_EQ(0, notified);
    }
}

#if OTBR_ENABLE_DNSSD_PLAT
// Starts a SRV resolver on one network interface, as OpenThread does on the infrastructure interface, for a service
// published on this host, and expects the resolver to get the service.
TEST_F(MdnsTest, DnssdPlatformResolvesServiceOfThisHostOnOneNetif)
{
    constexpr uint64_t kCallbackId = 1;

    uint32_t                   netifIndex = GetTestNetifIndex();
    int                        invoked    = 0;
    TestNames                  names;
    DnssdPlatform::SrvResolver resolver;
    Mdns::StateSubject         stateSubject;

    if (netifIndex == 0)
    {
        GTEST_SKIP() << kNoTestNetifMessage;
    }

    std::unique_ptr<Publisher> pub = CreatePublisher();
    DnssdPlatform              platform(*pub);

    stateSubject.AddObserver(platform);
    stateSubject.UpdateState(Publisher::State::kReady);
    platform.Start();

    resolver.mServiceInstance = names.mInstance.c_str();
    resolver.mServiceType     = names.mType.c_str();
    resolver.mInfraIfIndex    = netifIndex;
    resolver.mCallback        = nullptr;

    platform.StartServiceResolver(resolver, std::make_shared<DnssdPlatform::StdSrvCallback>(
                                                [&](const DnssdPlatform::SrvResult &aResult) {
                                                    EXPECT_EQ(netifIndex, aResult.mInfraIfIndex);
                                                    EXPECT_EQ(names.mInstance, aResult.mServiceInstance);
                                                    EXPECT_EQ(names.mType, aResult.mServiceType);
                                                    EXPECT_EQ(names.mHost, aResult.mHostName);
                                                    EXPECT_EQ(11111, aResult.mPort);
                                                    invoked++;
                                                },
                                                kCallbackId));

    pub->PublishHost(names.mHost, Publisher::AddressList{sAddr1, sAddr2}, NoOpCallback());
    pub->PublishService(names.mHost, names.mInstance, names.mType, {}, 11111, sTxtData1, NoOpCallback());
    RunMainloopUntilTimeout(kTimeoutSeconds);
    EXPECT_GE(invoked, 1) << "the SRV resolver on network interface " << netifIndex << " got no result";

    platform.StopServiceResolver(resolver, DnssdPlatform::StdSrvCallback(nullptr, kCallbackId));
    RunMainloopUntilTimeout(1);
    platform.Stop();
}
#endif // OTBR_ENABLE_DNSSD_PLAT
