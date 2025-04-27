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

#include <arpa/inet.h>
#include <array>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <memory>
#include <string>
#include <sys/time.h>
#include <vector>

#include "common/mainloop.hpp"
#include "common/mainloop_manager.hpp"
#include "common/types.hpp"
#include "host/posix/infra_if.hpp"
#include "host/posix/multicast_routing_manager.hpp"
#include "host/posix/netif.hpp"
#include "host/thread_host.hpp"
#include "utils/socket_utils.hpp"

// Only Test on linux platform for now.
#ifdef __linux__
#if OTBR_ENABLE_BACKBONE_ROUTER

std::string Exec(const char *aCmd)
{
    std::array<char, 128>                  buffer;
    std::string                            result;
    std::unique_ptr<FILE, int (*)(FILE *)> pipe(popen(aCmd, "r"), pclose);
    if (!pipe)
    {
        perror("Failed to open pipe!");
        exit(EXIT_FAILURE);
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
    {
        result += buffer.data();
    }
    return result;
}

std::vector<std::string> GetMulticastRoutingTable(void)
{
    std::vector<std::string> lines;
    std::stringstream        ss(Exec("ip -6 mroute"));
    std::string              line;

    while (std::getline(ss, line))
    {
        lines.push_back(line);
    }

    return lines;
}

static void MainloopProcess(uint32_t aTimeoutMs)
{
    otbr::MainloopContext mainloop;

    auto start = std::chrono::high_resolution_clock::now();

    while (true)
    {
        FD_ZERO(&mainloop.mReadFdSet);
        FD_ZERO(&mainloop.mWriteFdSet);
        FD_ZERO(&mainloop.mErrorFdSet);
        otbr::MainloopManager::GetInstance().Update(mainloop);

        int rval =
            select(mainloop.mMaxFd + 1, &mainloop.mReadFdSet, &mainloop.mWriteFdSet, &mainloop.mErrorFdSet, nullptr);

        if (rval >= 0)
        {
            otbr::MainloopManager::GetInstance().Process(mainloop);
        }
        else
        {
            perror("select()");
            exit(EXIT_FAILURE);
        }

        auto elapsedTime = std::chrono::high_resolution_clock::now() - start;
        if (elapsedTime > std::chrono::milliseconds(aTimeoutMs))
        {
            break;
        }
    }
}

class DummyNetworkProperties : public otbr::Host::NetworkProperties
{
public:
    otDeviceRole GetDeviceRole(void) const override { return OT_DEVICE_ROLE_DISABLED; }
    bool         Ip6IsEnabled(void) const override { return false; }
    uint32_t     GetPartitionId(void) const override { return 0; }
    void         GetDatasetActiveTlvs(otOperationalDatasetTlvs &aDatasetTlvs) const override
    {
        OTBR_UNUSED_VARIABLE(aDatasetTlvs);
    }
    void GetDatasetPendingTlvs(otOperationalDatasetTlvs &aDatasetTlvs) const override
    {
        OTBR_UNUSED_VARIABLE(aDatasetTlvs);
    }
    const otMeshLocalPrefix *GetMeshLocalPrefix(void) const override { return &mMeshLocalPrefix; }

    otMeshLocalPrefix mMeshLocalPrefix = {0};
};

TEST(BbrMcastRouting, MulticastRoutingTableSetCorrectlyAfterHandlingMlrEvents)
{
    otbr::Netif::Dependencies defaultNetifDep;
    otbr::Netif               netif("wpan0", defaultNetifDep);
    otbr::Netif               fakeInfraIf("wlx123", defaultNetifDep);
    EXPECT_EQ(netif.Init(), OTBR_ERROR_NONE);
    EXPECT_EQ(fakeInfraIf.Init(), OTBR_ERROR_NONE);

    const otIp6Address kInfraIfAddr = {
        {0x91, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02}};
    std::vector<otbr::Ip6AddressInfo> addrs = {
        {kInfraIfAddr, 64, 0, 1, 0},
    };
    fakeInfraIf.UpdateIp6UnicastAddresses(addrs);
    fakeInfraIf.SetNetifState(true);

    otbr::InfraIf::Dependencies defaultInfraIfDep;
    otbr::InfraIf               infraIf(defaultInfraIfDep);
    EXPECT_EQ(infraIf.SetInfraIf("wlx123"), OTBR_ERROR_NONE);

    DummyNetworkProperties        dummyNetworkProperties;
    otbr::MulticastRoutingManager mcastRtMgr(netif, infraIf, dummyNetworkProperties);
    mcastRtMgr.HandleStateChange(OT_BACKBONE_ROUTER_STATE_PRIMARY);

    /*
     * IP6 9101::1 > ff05::abcd: ICMP6, echo request, id 8, seq 1, length 64
     *   0x0000:  6003 742b 0040 3a05 9101 0000 0000 0000
     *   0x0010:  0000 0000 0000 0001 ff05 0000 0000 0000
     *   0x0020:  0000 0000 0000 abcd 8000 f9ae 0008 0001
     *   0x0030:  49b3 f867 0000 0000 4809 0100 0000 0000
     *   0x0040:  1011 1213 1415 1617 1819 1a1b 1c1d 1e1f
     *   0x0050:  2021 2223 2425 2627 2829 2a2b 2c2d 2e2f
     *   0x0060:  3031 3233 3435 3637
     */
    const uint8_t icmp6Packet[] = {
        0x60, 0x03, 0x74, 0x2b, 0x00, 0x40, 0x3a, 0x05, 0x91, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xff, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0xab, 0xcd, 0x80, 0x00, 0xf9, 0xae, 0x00, 0x08, 0x00, 0x01, 0x49, 0xb3, 0xf8, 0x67, 0x00, 0x00,
        0x00, 0x00, 0x48, 0x09, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
        0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29,
        0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
    };
    fakeInfraIf.Ip6Receive(icmp6Packet, sizeof(icmp6Packet));

    MainloopProcess(10);

    const std::string kAddressPair   = "(9101::1,ff05::abcd)";
    const std::string kIif           = "Iif: wlx123";
    const std::string kOifs          = "Oifs: wpan0";
    const std::string kStateResolved = "State: resolved";

    auto lines = GetMulticastRoutingTable();
    EXPECT_EQ(lines.size(), 1);
    EXPECT_THAT(lines.front(), ::testing::HasSubstr(kAddressPair));
    EXPECT_THAT(lines.front(), ::testing::HasSubstr(kIif));
    EXPECT_THAT(lines.front(), ::testing::Not(::testing::HasSubstr(kOifs)));
    EXPECT_THAT(lines.front(), ::testing::HasSubstr(kStateResolved));

    otbr::Ip6Address kMulAddr1 = {
        {0xff, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xab, 0xcd}};
    mcastRtMgr.HandleBackboneMulticastListenerEvent(OT_BACKBONE_ROUTER_MULTICAST_LISTENER_ADDED, kMulAddr1);

    MainloopProcess(10);
    lines = GetMulticastRoutingTable();
    EXPECT_THAT(lines.front(), ::testing::HasSubstr(kAddressPair));
    EXPECT_THAT(lines.front(), ::testing::HasSubstr(kIif));
    EXPECT_THAT(lines.front(), ::testing::HasSubstr(kOifs));
    EXPECT_THAT(lines.front(), ::testing::HasSubstr(kStateResolved));
}

#endif // OTBR_ENABLE_BACKBONE_ROUTER
#endif // __linux__
