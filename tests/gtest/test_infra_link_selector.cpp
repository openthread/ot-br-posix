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

#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <string>
#include <sys/select.h>
#include <unistd.h>

#include "common/code_utils.hpp"
#include "common/mainloop.hpp"
#include "common/mainloop_manager.hpp"
#include "utils/infra_link_selector.hpp"

// The selector watches two interfaces created for the test: dummy interfaces on Linux, fake Ethernet (feth) on
// macOS, each peered with a third and fourth one so that it has a carrier. Both need root, so the test is skipped
// otherwise.
namespace {

constexpr std::chrono::milliseconds kSelectionDelay(1000);

// Runs a command whose failure does not matter (cleanup); system() is declared warn_unused_result.
void RunIgnoringResult(const std::string &aCommand)
{
    int status = system(aCommand.c_str());

    OTBR_UNUSED_VARIABLE(status);
}

#if __linux__
const char kLinkA[] = "ilsa0";
const char kLinkB[] = "ilsb0";

void CreateLinks(void)
{
    for (const char *name : {kLinkA, kLinkB})
    {
        std::string cmd = std::string("ip link add ") + name + " type dummy && ip link set " + name + " up";
        ASSERT_EQ(system(cmd.c_str()), 0);
    }
}

void DestroyLinks(void)
{
    for (const char *name : {kLinkA, kLinkB})
    {
        RunIgnoringResult(std::string("ip link del ") + name + " 2>/dev/null");
    }
}

void SetLink(const char *aName, const char *aState)
{
    ASSERT_EQ(system((std::string("ip link set ") + aName + " " + aState).c_str()), 0);
}
#elif defined(__APPLE__)
const char kLinkA[]     = "feth200";
const char kLinkB[]     = "feth201";
const char kLinkAPeer[] = "feth202";
const char kLinkBPeer[] = "feth203";

void CreateLinks(void)
{
    for (const char *name : {kLinkA, kLinkB, kLinkAPeer, kLinkBPeer})
    {
        ASSERT_EQ(system((std::string("ifconfig ") + name + " create").c_str()), 0);
    }
    // A fake Ethernet reports an active media (a carrier) once it has a peer.
    ASSERT_EQ(system((std::string("ifconfig ") + kLinkA + " peer " + kLinkAPeer).c_str()), 0);
    ASSERT_EQ(system((std::string("ifconfig ") + kLinkB + " peer " + kLinkBPeer).c_str()), 0);
    for (const char *name : {kLinkA, kLinkB, kLinkAPeer, kLinkBPeer})
    {
        ASSERT_EQ(system((std::string("ifconfig ") + name + " up").c_str()), 0);
    }
}

void DestroyLinks(void)
{
    for (const char *name : {kLinkA, kLinkB, kLinkAPeer, kLinkBPeer})
    {
        RunIgnoringResult(std::string("ifconfig ") + name + " destroy 2>/dev/null");
    }
}

void SetLink(const char *aName, const char *aState)
{
    ASSERT_EQ(system((std::string("ifconfig ") + aName + " " + aState).c_str()), 0);
}
#else
const char kLinkA[] = "";
const char kLinkB[] = "";

void CreateLinks(void)
{
    GTEST_SKIP() << "no interface pair known for this system";
}
void DestroyLinks(void)
{
}
void SetLink(const char *, const char *)
{
}
#endif

class InfraLinkSelectorTest : public ::testing::Test
{
protected:
    void SetUp(void) override
    {
        if (getuid() != 0)
        {
            GTEST_SKIP() << "creating interfaces needs root";
        }

        DestroyLinks();
        CreateLinks();
    }

    void TearDown(void) override
    {
        if (getuid() == 0)
        {
            DestroyLinks();
        }
    }

    // Runs the mainloop until the selector names `aExpected` or `aTimeout` passes.
    static bool RunUntilSelected(otbr::Utils::InfraLinkSelector &aSelector,
                                 const char                     *aExpected,
                                 std::chrono::milliseconds       aTimeout)
    {
        auto deadline = std::chrono::steady_clock::now() + aTimeout;
        bool selected = false;

        // Processes the mainloop first and asks the selector after, as Application::Run() does.
        do
        {
            otbr::MainloopContext context;

            context.mMaxFd           = -1;
            context.mTimeout.tv_sec  = 0;
            context.mTimeout.tv_usec = 100000;
            FD_ZERO(&context.mReadFdSet);
            FD_ZERO(&context.mWriteFdSet);
            FD_ZERO(&context.mErrorFdSet);
            otbr::MainloopManager::GetInstance().Update(context);

            if (select(context.mMaxFd + 1, &context.mReadFdSet, &context.mWriteFdSet, &context.mErrorFdSet,
                       &context.mTimeout) >= 0)
            {
                otbr::MainloopManager::GetInstance().Process(context);
            }

            selected = std::string(aSelector.Select()) == aExpected;
        } while (!selected && std::chrono::steady_clock::now() < deadline);

        return selected;
    }

    // A switch happens about one selection delay after the event; a switch that must not happen would within two.
    static std::chrono::milliseconds SwitchTimeout(void) { return kSelectionDelay * 5; }
    static std::chrono::milliseconds NoSwitchTimeout(void) { return kSelectionDelay * 2; }
};

TEST_F(InfraLinkSelectorTest, FollowsTheLinkThatIsUpAndRunning)
{
    otbr::Utils::InfraLinkSelector selector({kLinkA, kLinkB}, kSelectionDelay);

    // Both links run: the first one is taken and kept.
    EXPECT_STREQ(selector.Select(), kLinkA);

    // The first link is set down: after the selection delay the second one is chosen.
    SetLink(kLinkA, "down");
    EXPECT_TRUE(RunUntilSelected(selector, kLinkB, SwitchTimeout()));

    // It comes back: a running link is kept, no switch back.
    SetLink(kLinkA, "up");
    EXPECT_FALSE(RunUntilSelected(selector, kLinkA, NoSwitchTimeout()));

    // The second link is set down: the first one, running again, is chosen.
    SetLink(kLinkB, "down");
    EXPECT_TRUE(RunUntilSelected(selector, kLinkA, SwitchTimeout()));
    SetLink(kLinkB, "up");
}

#ifdef __APPLE__
TEST_F(InfraLinkSelectorTest, FollowsTheCarrier)
{
    otbr::Utils::InfraLinkSelector selector({kLinkA, kLinkB}, kSelectionDelay);

    EXPECT_STREQ(selector.Select(), kLinkA);

    // The first link loses its carrier while staying up: the second one is chosen.
    ASSERT_EQ(system((std::string("ifconfig ") + kLinkA + " -peer").c_str()), 0);
    EXPECT_TRUE(RunUntilSelected(selector, kLinkB, SwitchTimeout()));

    // The carrier returns: no switch back.
    ASSERT_EQ(system((std::string("ifconfig ") + kLinkA + " peer " + kLinkAPeer).c_str()), 0);
    EXPECT_FALSE(RunUntilSelected(selector, kLinkA, NoSwitchTimeout()));
}
#endif

} // namespace
