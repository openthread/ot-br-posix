/*
 *    Copyright (c) 2024, The OpenThread Authors.
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

#include "ncp/posix/infra_if.hpp"
#include "ncp/posix/netif.hpp"

// Only Test on linux platform for now.
#ifdef __linux__

class InfraIfDependencyTest : public otbr::InfraIf::Dependencies
{
public:
    InfraIfDependencyTest(void)
        : mInfraIfIndex(0)
        , mIsRunning(false)
        , mSetInfraIfInvoked(false)
    {
    }

    otbrError SetInfraIf(unsigned int                         aInfraIfIndex,
                         bool                                 aIsRunning,
                         const std::vector<otbr::Ip6Address> &aIp6Addresses) override
    {
        mInfraIfIndex      = aInfraIfIndex;
        mIsRunning         = aIsRunning;
        mIp6Addresses      = aIp6Addresses;
        mSetInfraIfInvoked = true;

        return OTBR_ERROR_NONE;
    }

    unsigned int                  mInfraIfIndex;
    bool                          mIsRunning;
    std::vector<otbr::Ip6Address> mIp6Addresses;
    bool                          mSetInfraIfInvoked;
};

TEST(InfraIf, DepsSetInfraIfInvokedCorrectly_AfterSpecifyingInfraIf)
{
    const std::string fakeInfraIf = "wlx123";

    // Utilize the Netif module to create a network interface as the fake infrastructure interface.
    otbr::Netif::Dependencies defaultNetifDep;
    otbr::Netif               netif(defaultNetifDep);
    EXPECT_EQ(netif.Init(fakeInfraIf), OTBR_ERROR_NONE);

    const otIp6Address kTestAddr = {
        {0xfd, 0x35, 0x7a, 0x7d, 0x0f, 0x16, 0xe7, 0xe3, 0x73, 0xf3, 0x09, 0x00, 0x8e, 0xbe, 0x1b, 0x65}};
    std::vector<otbr::Ip6AddressInfo> addrs = {
        {kTestAddr, 64, 0, 1, 0},
    };
    netif.UpdateIp6UnicastAddresses(addrs);

    InfraIfDependencyTest testInfraIfDep;
    otbr::InfraIf         infraIf(testInfraIfDep);
    EXPECT_EQ(infraIf.SetInfraIf(fakeInfraIf.c_str()), OTBR_ERROR_NONE);

    EXPECT_NE(testInfraIfDep.mInfraIfIndex, 0);
    EXPECT_EQ(testInfraIfDep.mIsRunning, false);
    EXPECT_EQ(testInfraIfDep.mIp6Addresses.size(), 1);
    EXPECT_THAT(testInfraIfDep.mIp6Addresses, ::testing::Contains(otbr::Ip6Address(kTestAddr)));

    netif.Deinit();
}

TEST(InfraIf, DepsUpdateInfraIfStateInvokedCorrectly_AfterInfraIfStateChange)
{
    const std::string     fakeInfraIf = "wlx123";
    otbr::MainloopContext context;

    // Utilize the Netif module to create a network interface as the fake infrastructure interface.
    otbr::Netif::Dependencies defaultNetifDep;
    otbr::Netif               netif(defaultNetifDep);
    EXPECT_EQ(netif.Init(fakeInfraIf), OTBR_ERROR_NONE);

    const otIp6Address kTestAddr1 = {
        {0xfd, 0x35, 0x7a, 0x7d, 0x0f, 0x16, 0xe7, 0xe3, 0x73, 0xf3, 0x09, 0x00, 0x8e, 0xbe, 0x1b, 0x65}};
    const otIp6Address kTestAddr2 = {
        {0xfe, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa8, 0xa5, 0x42, 0xb7, 0x91, 0x80, 0xc3, 0xf8}};
    std::vector<otbr::Ip6AddressInfo> addrs = {
        {kTestAddr1, 64, 0, 1, 0},
        {kTestAddr2, 64, 0, 1, 0},
    };
    netif.UpdateIp6UnicastAddresses(addrs);

    InfraIfDependencyTest testInfraIfDep;
    otbr::InfraIf         infraIf(testInfraIfDep);
    infraIf.Init();
    EXPECT_EQ(infraIf.SetInfraIf(fakeInfraIf.c_str()), OTBR_ERROR_NONE);

    EXPECT_EQ(testInfraIfDep.mIsRunning, false);
    EXPECT_EQ(testInfraIfDep.mIp6Addresses.size(), 2);

    netif.SetNetifState(true);
    testInfraIfDep.mSetInfraIfInvoked = false;

    while (!testInfraIfDep.mSetInfraIfInvoked)
    {
        context.mMaxFd   = -1;
        context.mTimeout = {100, 0};
        FD_ZERO(&context.mReadFdSet);
        FD_ZERO(&context.mWriteFdSet);
        FD_ZERO(&context.mErrorFdSet);

        infraIf.UpdateFdSet(context);
        int rval = select(context.mMaxFd + 1, &context.mReadFdSet, &context.mWriteFdSet, &context.mErrorFdSet,
                          &context.mTimeout);
        if (rval < 0)
        {
            perror("select failed");
            exit(EXIT_FAILURE);
        }
        infraIf.Process(context);
    }
    EXPECT_EQ(testInfraIfDep.mIsRunning, true);

    addrs.clear();
    netif.UpdateIp6UnicastAddresses(addrs);
    testInfraIfDep.mSetInfraIfInvoked = false;
    while (!testInfraIfDep.mSetInfraIfInvoked)
    {
        context.mMaxFd   = -1;
        context.mTimeout = {100, 0};
        FD_ZERO(&context.mReadFdSet);
        FD_ZERO(&context.mWriteFdSet);
        FD_ZERO(&context.mErrorFdSet);

        infraIf.UpdateFdSet(context);
        int rval = select(context.mMaxFd + 1, &context.mReadFdSet, &context.mWriteFdSet, &context.mErrorFdSet,
                          &context.mTimeout);
        if (rval < 0)
        {
            perror("select failed");
            exit(EXIT_FAILURE);
        }
        infraIf.Process(context);
    }
    EXPECT_EQ(testInfraIfDep.mIp6Addresses.size(), 0);
    EXPECT_EQ(testInfraIfDep.mIsRunning, false);

    infraIf.Deinit();
    netif.Deinit();
}

#endif // __linux__
