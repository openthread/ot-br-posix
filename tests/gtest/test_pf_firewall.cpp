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

#include <string>
#include <vector>

#include "common/types.hpp"
#include "firewall/pf_firewall.hpp"
#include "firewall/pfctl.hpp"

namespace {

using otbr::Ip6Prefix;
using otbr::Firewall::IPfctl;
using otbr::Firewall::PfFirewall;

struct PfctlCall
{
    std::vector<std::string> mArgs;
    std::string              mInput;

    std::string Command(void) const
    {
        std::string command = "pfctl";

        for (const std::string &arg : mArgs)
        {
            command += " " + arg;
        }

        return command;
    }
};

// Records every pfctl invocation and answers the listing/enable queries the
// backend makes, so the tests can assert on exactly what would be run.
class FakePfctl : public IPfctl
{
public:
    otbrError Run(const std::vector<std::string> &aArgs,
                  const std::string              &aInput,
                  std::string                    &aOutput,
                  std::string                    &aError) override
    {
        otbrError error = OTBR_ERROR_NONE;

        mCalls.push_back({aArgs, aInput});
        aOutput.clear();
        aError.clear();

        if (aArgs == std::vector<std::string>{"-E"})
        {
            aError = mEnableOutput;
        }
        else if (aArgs == std::vector<std::string>{"-s", "rules"})
        {
            aOutput = mRulesListing;
        }
        else if (aArgs == std::vector<std::string>{"-s", "nat"})
        {
            aOutput = mNatListing;
        }
        else if (mFailRulesetLoad && aArgs.size() >= 4 && aArgs[2] == "-f")
        {
            aError = "pfctl: Syntax error in config file: pf rules not loaded\n";
            error  = OTBR_ERROR_ERRNO;
        }

        return error;
    }

    const PfctlCall &Last(void) const { return mCalls.back(); }

    std::vector<PfctlCall> mCalls;
    std::string            mEnableOutput    = "pf enabled\nToken : 4242\n";
    std::string            mRulesListing    = "anchor \"com.apple/*\" all\nanchor \"otbr\" all\n";
    std::string            mNatListing      = "nat-anchor \"com.apple/*\" all\nnat-anchor \"otbr\" all\n";
    bool                   mFailRulesetLoad = false;
};

const char kIngressRuleset[] = "# Installed by otbr-agent; do not edit.\n"
                               "table <ingress_deny_src> persist\n"
                               "table <ingress_allow_dst> persist\n"
                               "pass out quick on utun5 inet6 from (utun5) to any\n"
                               "pass out quick on utun5 inet6 from (self) to any\n"
                               "block out quick on utun5 inet6 from <ingress_deny_src> to any\n"
                               "pass out quick on utun5 inet6 from any to <ingress_allow_dst>\n"
                               "pass out quick on utun5 inet6 from any to ff00::/8\n"
                               "block out quick on utun5 inet6 all\n";

TEST(PfFirewall, InitEnablesPfVerifiesAnchorAndFlushes)
{
    FakePfctl  pfctl;
    PfFirewall firewall(pfctl, "utun5");

    ASSERT_EQ(firewall.Init(), OTBR_ERROR_NONE);
    EXPECT_TRUE(firewall.IsInitialized());

    ASSERT_EQ(pfctl.mCalls.size(), 3u);
    EXPECT_EQ(pfctl.mCalls[0].Command(), "pfctl -E");
    EXPECT_EQ(pfctl.mCalls[1].Command(), "pfctl -s rules");
    EXPECT_EQ(pfctl.mCalls[2].Command(), "pfctl -a otbr -F all");
}

TEST(PfFirewall, InitFailsWhenMainRulesetDoesNotReferenceAnchor)
{
    FakePfctl  pfctl;
    PfFirewall firewall(pfctl, "utun5");

    pfctl.mRulesListing = "anchor \"com.apple/*\" all\n";

    EXPECT_EQ(firewall.Init(), OTBR_ERROR_NOT_FOUND);
    EXPECT_FALSE(firewall.IsInitialized());
    // The enable reference taken before the check is given back.
    EXPECT_EQ(pfctl.Last().Command(), "pfctl -X 4242");
}

TEST(PfFirewall, InitSucceedsWithoutReferenceTokenAndDeinitSkipsRelease)
{
    FakePfctl  pfctl;
    PfFirewall firewall(pfctl, "utun5");

    // pf is enabled regardless of whether the token can be read back.
    pfctl.mEnableOutput = "pf enabled\n";

    ASSERT_EQ(firewall.Init(), OTBR_ERROR_NONE);
    EXPECT_TRUE(firewall.IsInitialized());

    ASSERT_EQ(firewall.Deinit(), OTBR_ERROR_NONE);
    // Nothing to release: the last call is the anchor flush, not `pfctl -X`.
    EXPECT_EQ(pfctl.Last().Command(), "pfctl -a otbr -F all");
}

TEST(PfFirewall, InitRejectsEmptyInterfaceName)
{
    FakePfctl  pfctl;
    PfFirewall firewall(pfctl, "");

    EXPECT_EQ(firewall.Init(), OTBR_ERROR_INVALID_ARGS);
    EXPECT_TRUE(pfctl.mCalls.empty());
}

TEST(PfFirewall, EnableIngressFilterLoadsRulesetIntoAnchor)
{
    FakePfctl  pfctl;
    PfFirewall firewall(pfctl, "utun5");

    ASSERT_EQ(firewall.Init(), OTBR_ERROR_NONE);
    ASSERT_EQ(firewall.EnableIngressFilter(), OTBR_ERROR_NONE);
    EXPECT_TRUE(firewall.IsIngressFilterEnabled());

    EXPECT_EQ(pfctl.Last().Command(), "pfctl -a otbr -f -");
    EXPECT_EQ(pfctl.Last().mInput, kIngressRuleset);

    // Enabling again is a no-op.
    size_t calls = pfctl.mCalls.size();
    EXPECT_EQ(firewall.EnableIngressFilter(), OTBR_ERROR_NONE);
    EXPECT_EQ(pfctl.mCalls.size(), calls);
}

TEST(PfFirewall, EnableIngressFilterRequiresInit)
{
    FakePfctl  pfctl;
    PfFirewall firewall(pfctl, "utun5");

    EXPECT_EQ(firewall.EnableIngressFilter(), OTBR_ERROR_INVALID_STATE);
    EXPECT_FALSE(firewall.IsIngressFilterEnabled());
}

TEST(PfFirewall, FailedRulesetLoadLeavesFilterDisabled)
{
    FakePfctl  pfctl;
    PfFirewall firewall(pfctl, "utun5");

    ASSERT_EQ(firewall.Init(), OTBR_ERROR_NONE);
    pfctl.mFailRulesetLoad = true;

    EXPECT_EQ(firewall.EnableIngressFilter(), OTBR_ERROR_ERRNO);
    EXPECT_FALSE(firewall.IsIngressFilterEnabled());
}

TEST(PfFirewall, Nat44RequiresNatAnchorAndPrecedesFilterRules)
{
    FakePfctl  pfctl;
    PfFirewall firewall(pfctl, "utun5");

    ASSERT_EQ(firewall.Init(), OTBR_ERROR_NONE);
    ASSERT_EQ(firewall.EnableIngressFilter(), OTBR_ERROR_NONE);

    pfctl.mNatListing = "nat-anchor \"com.apple/*\" all\n";
    EXPECT_EQ(firewall.EnableNat44Masquerade("en0"), OTBR_ERROR_NOT_FOUND);
    EXPECT_FALSE(firewall.IsNat44Enabled());

    pfctl.mNatListing = "nat-anchor \"otbr\" all\n";
    ASSERT_EQ(firewall.EnableNat44Masquerade("en0"), OTBR_ERROR_NONE);
    EXPECT_TRUE(firewall.IsNat44Enabled());

    {
        const std::string &ruleset = pfctl.Last().mInput;
        size_t             nat     = ruleset.find("nat pass on en0 inet tagged otbr_thread -> (en0)\n");
        size_t             tag     = ruleset.find("pass in quick on utun5 inet tag otbr_thread\n");
        size_t             filter  = ruleset.find("pass out quick on utun5 inet6 from (utun5) to any\n");

        EXPECT_EQ(pfctl.Last().Command(), "pfctl -a otbr -f -");
        ASSERT_NE(nat, std::string::npos);
        ASSERT_NE(tag, std::string::npos);
        ASSERT_NE(filter, std::string::npos);
        // pf requires translation rules before filter rules.
        EXPECT_LT(nat, tag);
        EXPECT_LT(tag, filter);
    }
}

TEST(PfFirewall, ReplaceIngressPrefixesReplacesTablesSortedAndDeduplicated)
{
    FakePfctl  pfctl;
    PfFirewall firewall(pfctl, "utun5");

    ASSERT_EQ(firewall.Init(), OTBR_ERROR_NONE);
    ASSERT_EQ(firewall.EnableIngressFilter(), OTBR_ERROR_NONE);

    std::vector<Ip6Prefix> denySrc  = {Ip6Prefix("fd00:2::", 64), Ip6Prefix("fd00:1::", 64), Ip6Prefix("fd00:2::", 64)};
    std::vector<Ip6Prefix> allowDst = {Ip6Prefix("fd00:1::", 64)};

    ASSERT_EQ(firewall.ReplaceIngressPrefixes(denySrc, allowDst), OTBR_ERROR_NONE);

    ASSERT_GE(pfctl.mCalls.size(), 2u);
    EXPECT_EQ(pfctl.mCalls[pfctl.mCalls.size() - 2].Command(),
              "pfctl -a otbr -t ingress_deny_src -T replace fd00:1::/64 fd00:2::/64");
    EXPECT_EQ(pfctl.Last().Command(), "pfctl -a otbr -t ingress_allow_dst -T replace fd00:1::/64");
}

TEST(PfFirewall, ReplaceIngressPrefixesFlushesEmptyTable)
{
    FakePfctl  pfctl;
    PfFirewall firewall(pfctl, "utun5");

    ASSERT_EQ(firewall.Init(), OTBR_ERROR_NONE);
    ASSERT_EQ(firewall.EnableIngressFilter(), OTBR_ERROR_NONE);

    ASSERT_EQ(firewall.ReplaceIngressPrefixes({Ip6Prefix("fd00:1::", 64)}, {}), OTBR_ERROR_NONE);
    EXPECT_EQ(pfctl.Last().Command(), "pfctl -a otbr -t ingress_allow_dst -T flush");
}

TEST(PfFirewall, ReplaceIngressPrefixesRequiresIngressFilter)
{
    FakePfctl  pfctl;
    PfFirewall firewall(pfctl, "utun5");

    ASSERT_EQ(firewall.Init(), OTBR_ERROR_NONE);
    EXPECT_EQ(firewall.ReplaceIngressPrefixes({}, {}), OTBR_ERROR_INVALID_STATE);
}

TEST(PfFirewall, DeinitFlushesAnchorAndReleasesReference)
{
    FakePfctl  pfctl;
    PfFirewall firewall(pfctl, "utun5");

    ASSERT_EQ(firewall.Init(), OTBR_ERROR_NONE);
    ASSERT_EQ(firewall.EnableIngressFilter(), OTBR_ERROR_NONE);

    ASSERT_EQ(firewall.Deinit(), OTBR_ERROR_NONE);
    EXPECT_FALSE(firewall.IsInitialized());
    EXPECT_FALSE(firewall.IsIngressFilterEnabled());

    ASSERT_GE(pfctl.mCalls.size(), 2u);
    EXPECT_EQ(pfctl.mCalls[pfctl.mCalls.size() - 2].Command(), "pfctl -a otbr -F all");
    EXPECT_EQ(pfctl.Last().Command(), "pfctl -X 4242");

    // Deinit without Init is a no-op.
    size_t calls = pfctl.mCalls.size();
    EXPECT_EQ(firewall.Deinit(), OTBR_ERROR_NONE);
    EXPECT_EQ(pfctl.mCalls.size(), calls);
}

} // namespace
