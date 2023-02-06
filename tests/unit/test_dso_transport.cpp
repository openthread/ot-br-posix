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

#if OTBR_ENABLE_DNS_DSO

#include <chrono>
#include <fcntl.h>
#include <functional>
#include <iostream>
#include <thread>
#include <unistd.h>
#include "common/mainloop_manager.hpp"
#include "dso/dso_transport.hpp"
#include "openthread/platform/dso_transport.h"

#include <CppUTest/TestHarness.h>

void otPlatReset(otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);
}

class DsoTestFixture
{
public:
    enum EventType
    {
        kConnected,
        kDisconnected,
        kReceive,
    };

    struct Event
    {
        otPlatDsoConnection *mPlatDsoConnection;
        EventType            mType;
        std::vector<uint8_t> mData;

        void AssertConnected(otPlatDsoConnection *aConnection) const
        {
            CHECK(mPlatDsoConnection == aConnection);
            CHECK(mType == EventType::kConnected);
        }

        void AssertDisconnected(otPlatDsoConnection *aConnection, otPlatDsoDisconnectMode aMode) const
        {
            CHECK(mPlatDsoConnection == aConnection);
            CHECK(mType == EventType::kDisconnected);
            CHECK(mData.size() == 1);
            CHECK(mData[0] == static_cast<uint8_t>(aMode));
        }

        void AssertReceive(otPlatDsoConnection *aConnection, const std::vector<uint8_t> &aData) const
        {
            CHECK(mPlatDsoConnection == aConnection);
            CHECK(mType == EventType::kReceive);
            CHECK(mData == aData);
        }
    };

    class AutoCleanUp
    {
    public:
        AutoCleanUp(std::function<void(void)> aCleanUp)
            : mCleanUp(std::move(aCleanUp))
        {
        }

        AutoCleanUp(const AutoCleanUp &aOther)            = delete;
        AutoCleanUp &operator=(const AutoCleanUp &aOther) = delete;
        AutoCleanUp(AutoCleanUp &&aOther)                 = default;

        ~AutoCleanUp(void) { mCleanUp(); }

    private:
        std::function<void(void)> mCleanUp;
    };

    DsoTestFixture() { SetUp(); }

    otPlatDsoConnection *AcceptConnection(otInstance *aInstance, otSockAddr *aSockAddr)
    {
        OT_UNUSED_VARIABLE(aInstance);
        OT_UNUSED_VARIABLE(aSockAddr);

        otPlatDsoConnection *newConnection = reinterpret_cast<otPlatDsoConnection *>(++mCurrentPlatDsoConnectionId);

        mPlatDsoConnections.push_back(newConnection);

        return newConnection;
    }

    void HandleConnected(otbr::Dso::DsoAgent::DsoConnection *aConnection)
    {
        mEvents.push_back(Event{aConnection->mConnection, EventType::kConnected, {}});
    }

    void HandleDisconnected(otbr::Dso::DsoAgent::DsoConnection *aConnection, otPlatDsoDisconnectMode aMode)
    {
        mEvents.push_back({Event{aConnection->mConnection, EventType::kDisconnected, {static_cast<uint8_t>(aMode)}}});
    }

    void HandleReceive(otbr::Dso::DsoAgent::DsoConnection *aConnection, const std::vector<uint8_t> &aData)
    {
        mEvents.push_back({Event{aConnection->mConnection, EventType::kReceive, std::move(aData)}});
    }

    static std::vector<uint8_t> MakeDataForSending(std::vector<uint8_t> aData)
    {
        int length = aData.size();

        aData.insert(aData.begin(), length >> 8);
        aData.insert(aData.begin() + 1, length & ((1 << 8) - 1));

        return aData;
    }

    static void Send(mbedtls_net_context &aCtx, const std::vector<uint8_t> &aData)
    {
        auto data = MakeDataForSending(aData);

        mbedtls_net_send(&aCtx, data.data(), data.size());
    }

    static std::vector<uint8_t> Receive(mbedtls_net_context &aCtx)
    {
        uint16_t             length;
        std::vector<uint8_t> data;

        CHECK(sizeof(length) == mbedtls_net_recv(&aCtx, reinterpret_cast<uint8_t *>(&length), sizeof(length)));
        length = ntohs(length);
        data.resize(length);
        CHECK(length == mbedtls_net_recv(&aCtx, data.data(), length));

        return data;
    }

    static void Reset(mbedtls_net_context &aCtx)
    {
        struct linger l;

        l.l_onoff  = 1;
        l.l_linger = 0;
        setsockopt(aCtx.fd, SOL_SOCKET, SO_LINGER, &l, sizeof(l));
        mbedtls_net_close(&aCtx);
    }

    void SetUp()
    {
        mAgent.Init(mInstance, "lo");
        mAgent.SetHandlers(
            [this](otInstance *aInstance, otSockAddr *aSockAddr) { return AcceptConnection(aInstance, aSockAddr); },
            [this](otbr::Dso::DsoAgent::DsoConnection *aConnection) { HandleConnected(aConnection); },
            [this](otbr::Dso::DsoAgent::DsoConnection *aConnection, otPlatDsoDisconnectMode aMode) {
                HandleDisconnected(aConnection, aMode);
            },
            [this](otbr::Dso::DsoAgent::DsoConnection *aConnection, const std::vector<uint8_t> &aData) {
                HandleReceive(aConnection, aData);
            });
    }

    AutoCleanUp RunMainLoop()
    {
        mShouldExit                 = false;
        std::thread *mainLoopThread = new std::thread([this] {
            while (!mShouldExit)
            {
                int                   rval;
                otbr::MainloopContext mainloop{};

                mainloop.mMaxFd           = -1;
                mainloop.mTimeout.tv_sec  = 1; // 1 second
                mainloop.mTimeout.tv_usec = 0;

                FD_ZERO(&mainloop.mReadFdSet);
                FD_ZERO(&mainloop.mWriteFdSet);
                FD_ZERO(&mainloop.mErrorFdSet);

                otbr::MainloopManager::GetInstance().Update(mainloop);

                rval = select(mainloop.mMaxFd + 1, &mainloop.mReadFdSet, &mainloop.mWriteFdSet, &mainloop.mErrorFdSet,
                              &mainloop.mTimeout);

                if (rval >= 0)
                {
                    otbr::MainloopManager::GetInstance().Process(mainloop);
                }
            }
        });

        return AutoCleanUp([this, mainLoopThread]() {
            mShouldExit = true;
            mainLoopThread->join();
            delete mainLoopThread;
        });
    }

    void RunMainLoopFor(const std::chrono::milliseconds &aMilliseconds)
    {
        auto quitMainLoop = RunMainLoop();

        std::this_thread::sleep_for(aMilliseconds);
    }

    void TestServer(void)
    {
        mAgent.SetEnabled(mInstance, true);

        mbedtls_net_context ctx;

        {
            auto _ = RunMainLoop();

            CHECK(!mbedtls_net_connect(&ctx, "localhost", "853", MBEDTLS_NET_PROTO_TCP));
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        auto conn = mAgent.FindOrCreateConnection(mPlatDsoConnections[0]);

        std::vector<uint8_t> message1 = {0x61, 0x62, 0x63, 0x64, 0x31, 0x32, 0x33, 0x34}; // "abcd1234"
        std::vector<uint8_t> message2 = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
        std::vector<uint8_t> message3 = {0x31, 0x32, 0x33, 0x34, 0x61, 0x62, 0x63, 0x64}; // "1234abcd"
        std::vector<uint8_t> message4 = {0x41, 0x42, 0x43, 0x44, 0x45};                   // "ABCDE"

        Send(ctx, message1);
        Send(ctx, message2);

        conn->Send(message3);
        conn->Send(message4);

        {
            auto _ = RunMainLoop();

            std::this_thread::sleep_for(std::chrono::milliseconds(500));

            CHECK(Receive(ctx) == message3);
            CHECK(Receive(ctx) == message4);

            mbedtls_net_free(&ctx);

            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        CHECK(mEvents.size() == 4);
        CHECK(mAgent.mMap.empty());

        int index = 0;
        mEvents[index++].AssertConnected(mPlatDsoConnections[0]);
        mEvents[index++].AssertReceive(mPlatDsoConnections[0], message1);
        mEvents[index++].AssertReceive(mPlatDsoConnections[0], message2);
        mEvents[index++].AssertDisconnected(mPlatDsoConnections[0], OT_PLAT_DSO_DISCONNECT_MODE_GRACEFULLY_CLOSE);
    }

    void TestServerOnClientError(void)
    {
        mAgent.SetEnabled(mInstance, true);

        mbedtls_net_context ctx;

        {
            auto _ = RunMainLoop();

            CHECK(!mbedtls_net_connect(&ctx, "localhost", "853", MBEDTLS_NET_PROTO_TCP));
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        Reset(ctx);

        RunMainLoopFor(std::chrono::milliseconds(500));

        printf("%zu", mEvents.size());
        CHECK(mEvents.size() == 2);
        CHECK(mAgent.mMap.empty());

        int index = 0;
        mEvents[index++].AssertConnected(mPlatDsoConnections[0]);
        mEvents[index++].AssertDisconnected(mPlatDsoConnections[0], OT_PLAT_DSO_DISCONNECT_MODE_FORCIBLY_ABORT);
    }

    void TestClient(void)
    {
        const char         *kAddress = "::1";
        constexpr int       kPort    = 54321;
        otSockAddr          sockAddr;
        mbedtls_net_context listeningCtx;
        mbedtls_net_context clientCtx;

        mAgent.SetEnabled(mInstance, true);

        CHECK(!mbedtls_net_bind(&listeningCtx, kAddress, std::to_string(kPort).c_str(), MBEDTLS_NET_PROTO_TCP));

        std::vector<uint8_t> message1 = {0x61, 0x62, 0x63, 0x64, 0x31, 0x32, 0x33, 0x34}; // "abcd1234"
        std::vector<uint8_t> message2 = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
        std::vector<uint8_t> message3 = {0x31, 0x32, 0x33, 0x34, 0x61, 0x62, 0x63, 0x64}; // "1234abcd"
        std::vector<uint8_t> message4 = {0x41, 0x42, 0x43, 0x44, 0x45};                   // "ABCDE"

        auto conn = mAgent.FindOrCreateConnection(reinterpret_cast<otPlatDsoConnection *>(1));

        SuccessOrDie(otIp6AddressFromString(kAddress, &sockAddr.mAddress), "Failed to parse IPv6 address");
        sockAddr.mPort = kPort;

        conn->Connect(&sockAddr);

        CHECK(!mbedtls_net_accept(&listeningCtx, &clientCtx, nullptr, 0, nullptr));

        RunMainLoopFor(std::chrono::milliseconds(500));

        CHECK(conn->GetState() == otbr::Dso::DsoAgent::DsoConnection::State::kConnected);

        conn->Send(message3);
        conn->Send(message4);

        Send(clientCtx, message1);
        Send(clientCtx, message2);

        RunMainLoopFor(std::chrono::milliseconds(500));

        CHECK(Receive(clientCtx) == message3);
        CHECK(Receive(clientCtx) == message4);

        mbedtls_net_free(&clientCtx);

        RunMainLoopFor(std::chrono::milliseconds(500));

        mbedtls_net_free(&listeningCtx);

        CHECK(mEvents.size() == 4);

        int index = 0;
        mEvents[index++].AssertConnected(reinterpret_cast<otPlatDsoConnection *>(1));
        mEvents[index++].AssertReceive(reinterpret_cast<otPlatDsoConnection *>(1), message1);
        mEvents[index++].AssertReceive(reinterpret_cast<otPlatDsoConnection *>(1), message2);
        mEvents[index++].AssertDisconnected(reinterpret_cast<otPlatDsoConnection *>(1),
                                            OT_PLAT_DSO_DISCONNECT_MODE_GRACEFULLY_CLOSE);

        CHECK(mAgent.mMap.empty());
    }

    void TestClientOnServerError(void)
    {
        const char         *kAddress = "::1";
        constexpr int       kPort    = 54321;
        otSockAddr          sockAddr;
        mbedtls_net_context listeningCtx;
        mbedtls_net_context clientCtx;

        mAgent.SetEnabled(mInstance, true);

        CHECK(!mbedtls_net_bind(&listeningCtx, kAddress, std::to_string(kPort).c_str(), MBEDTLS_NET_PROTO_TCP));

        auto conn = mAgent.FindOrCreateConnection(reinterpret_cast<otPlatDsoConnection *>(1));

        SuccessOrDie(otIp6AddressFromString(kAddress, &sockAddr.mAddress), "Failed to parse IPv6 address");
        sockAddr.mPort = kPort;

        conn->Connect(&sockAddr);

        CHECK(!mbedtls_net_accept(&listeningCtx, &clientCtx, nullptr, 0, nullptr));

        RunMainLoopFor(std::chrono::milliseconds(500));

        CHECK(conn->GetState() == otbr::Dso::DsoAgent::DsoConnection::State::kConnected);

        RunMainLoopFor(std::chrono::milliseconds(500));

        Reset(clientCtx);

        RunMainLoopFor(std::chrono::milliseconds(500));

        mbedtls_net_free(&listeningCtx);

        CHECK(mEvents.size() == 2);

        int index = 0;
        mEvents[index++].AssertConnected(reinterpret_cast<otPlatDsoConnection *>(1));
        mEvents[index++].AssertDisconnected(reinterpret_cast<otPlatDsoConnection *>(1),
                                            OT_PLAT_DSO_DISCONNECT_MODE_FORCIBLY_ABORT);

        CHECK(mAgent.mMap.empty());
    }

    void TestServerWithMultipleConnections(void)
    {
        constexpr int        kClients = 5;
        std::vector<uint8_t> message1 = {0x61, 0x62, 0x63, 0x64, 0x31, 0x32, 0x33, 0x34}; // "abcd1234"
        std::vector<uint8_t> message2 = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
        std::vector<uint8_t> message3 = {0x31, 0x32, 0x33, 0x34, 0x61, 0x62, 0x63, 0x64}; // "1234abcd"
        std::vector<uint8_t> message4 = {0x41, 0x42, 0x43, 0x44, 0x45};                   // "ABCDE"

        std::vector<mbedtls_net_context> contexts(kClients);

        mAgent.SetEnabled(mInstance, true);

        for (auto &ctx : contexts)
        {
            CHECK(!mbedtls_net_connect(&ctx, "localhost", "853", MBEDTLS_NET_PROTO_TCP));
        }

        RunMainLoopFor(std::chrono::milliseconds(500));

        for (int i = 0; i < kClients; ++i)
        {
            auto conn = mAgent.FindOrCreateConnection(mPlatDsoConnections[i]);

            Send(contexts[i], message1);
            conn->Send(message2);
            Send(contexts[i], message3);
            conn->Send(message4);
        }

        RunMainLoopFor(std::chrono::milliseconds(500));

        for (auto &ctx : contexts)
        {
            CHECK(Receive(ctx) == message2);
            CHECK(Receive(ctx) == message4);

            mbedtls_net_close(&ctx);
        }

        RunMainLoopFor(std::chrono::milliseconds(500));

        CHECK(mEvents.size() == 4 * kClients);

        std::map<otPlatDsoConnection *, std::vector<Event>> eventsPerClient;

        for (const auto &event : mEvents)
        {
            eventsPerClient[event.mPlatDsoConnection].push_back(event);
        }

        CHECK(eventsPerClient.size() == kClients);

        for (const auto &pair : eventsPerClient)
        {
            const auto &events = pair.second;
            int         index  = 0;

            events[index++].AssertConnected(pair.first);
            events[index++].AssertReceive(pair.first, message1);
            events[index++].AssertReceive(pair.first, message3);
            events[index++].AssertDisconnected(pair.first, OT_PLAT_DSO_DISCONNECT_MODE_GRACEFULLY_CLOSE);
        }

        CHECK(mAgent.mMap.empty());
    }

    void TestDoubleConnect(void)
    {
        const char         *kAddress = "::1";
        constexpr int       kPort    = 54321;
        otSockAddr          sockAddr;
        mbedtls_net_context listeningCtx;
        mbedtls_net_context clientCtx;

        mAgent.SetEnabled(mInstance, true);

        CHECK(!mbedtls_net_bind(&listeningCtx, kAddress, std::to_string(kPort).c_str(), MBEDTLS_NET_PROTO_TCP));

        auto conn = mAgent.FindOrCreateConnection(reinterpret_cast<otPlatDsoConnection *>(1));

        SuccessOrDie(otIp6AddressFromString(kAddress, &sockAddr.mAddress), "Failed to parse IPv6 address");
        sockAddr.mPort = kPort;

        conn->Connect(&sockAddr);
        conn->Connect(&sockAddr);

        CHECK(!mbedtls_net_accept(&listeningCtx, &clientCtx, nullptr, 0, nullptr));

        RunMainLoopFor(std::chrono::milliseconds(500));

        CHECK(conn->GetState() == otbr::Dso::DsoAgent::DsoConnection::State::kConnected);
        CHECK(mAgent.mMap.size() == 1);
    }

    std::atomic_bool                   mShouldExit;
    otInstance                        *mInstance = reinterpret_cast<otInstance *>(1);
    otbr::Dso::DsoAgent                mAgent;
    std::vector<otPlatDsoConnection *> mPlatDsoConnections;
    std::vector<Event>                 mEvents;
    int                                mCurrentPlatDsoConnectionId = 10000;
};

TEST_GROUP(DsoTransport){};

TEST(DsoTransport, TestAll)
{
    DsoTestFixture().TestServer();
    DsoTestFixture().TestServerOnClientError();
    DsoTestFixture().TestServerWithMultipleConnections();
    DsoTestFixture().TestClient();
    DsoTestFixture().TestClientOnServerError();
    DsoTestFixture().TestDoubleConnect();
}

#endif // OTBR_ENABLE_DNS_DSO
