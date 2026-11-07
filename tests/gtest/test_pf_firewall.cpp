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
        else if (aArgs == std::vector<std::string>{"-s", "info"})
        {
            aOutput = mInfoListing;
        }
        else if (aArgs.size() == 4 && aArgs[0] == "-a" && aArgs[2] == "-s")
        {
            // The anchor's own listings, as VerifyInstalled() asks for them.
            if (mFailListings)
            {
                aError = "pfctl: /dev/pf: Permission denied\n";
                error  = OTBR_ERROR_ERRNO;
            }
            else
            {
                aOutput = (aArgs[3] == "rules") ? mAnchorRules : (aArgs[3] == "Tables") ? mAnchorTables : mAnchorNat;
            }
        }
        else if (mFailTableReplaces > 0 && aArgs.size() >= 5 && aArgs[4] == "-T")
        {
            mFailTableReplaces--;
            aError = mTableReplaceError;
            error  = OTBR_ERROR_ERRNO;
        }
        else if (mCreateTables > 0 && aArgs.size() >= 6 && aArgs[4] == "-T" && aArgs[5] == "replace")
        {
            // A table the anchor lost: `-T replace` creates it and says so, exit status 0.
            mCreateTables--;
            aError = "1 table created.\n1/1 addresses added.\n";
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
    // What pfctl prints for the anchor after a load: pfctl rewrites the rules
    // in its own words, so these are listings, not the ruleset text.
    std::string mAnchorRules       = "pass out quick on utun5 inet6 from (utun5) to any flags S/SA keep state\n"
                                     "pass out quick on utun5 inet6 from (self) to any flags S/SA keep state\n"
                                     "block drop out quick on utun5 inet6 from <ingress_deny_src> to any\n"
                                     "pass out quick on utun5 inet6 from any to <ingress_allow_dst> flags S/SA keep state\n"
                                     "pass out quick on utun5 inet6 from any to ff00::/8 flags S/SA keep state\n"
                                     "block drop out quick on utun5 inet6 all\n";
    std::string mAnchorTables      = "ingress_allow_dst\ningress_deny_src\n";
    std::string mAnchorNat         = "nat pass on en0 inet tagged otbr_thread -> (en0) round-robin\n";
    std::string mInfoListing       = "Status: Enabled for 0 days 00:01:02           Debug: Urgent\n\nState Table\n";
    int         mFailTableReplaces = 0; ///< How many table replacements fail with mTableReplaceError.
    std::string mTableReplaceError = "pfctl: Table does not exist.\n";
    int         mCreateTables      = 0;     ///< How many `-T replace` runs report a table created.
    bool        mFailListings      = false; ///< Every anchor listing fails.
};

// The tag rule pfctl lists with the NAT44 masquerade on.
const char kAnchorTagRule[] = "pass in quick on utun5 inet all flags S/SA keep state tag otbr_thread\n";

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

TEST(PfFirewall, DisableNat44ReloadsTheRulesetWithoutTheNatRules)
{
    FakePfctl  pfctl;
    PfFirewall firewall(pfctl, "utun5");

    EXPECT_EQ(firewall.DisableNat44Masquerade(), OTBR_ERROR_INVALID_STATE);

    ASSERT_EQ(firewall.Init(), OTBR_ERROR_NONE);
    ASSERT_EQ(firewall.EnableIngressFilter(), OTBR_ERROR_NONE);
    ASSERT_EQ(firewall.EnableNat44Masquerade("en0"), OTBR_ERROR_NONE);

    ASSERT_EQ(firewall.DisableNat44Masquerade(), OTBR_ERROR_NONE);
    EXPECT_FALSE(firewall.IsNat44Enabled());
    EXPECT_EQ(pfctl.Last().Command(), "pfctl -a otbr -f -");
    EXPECT_EQ(pfctl.Last().mInput, kIngressRuleset);

    // Not enabled: nothing to reload.
    size_t calls = pfctl.mCalls.size();
    EXPECT_EQ(firewall.DisableNat44Masquerade(), OTBR_ERROR_NONE);
    EXPECT_EQ(pfctl.mCalls.size(), calls);
}

TEST(PfFirewall, FailedRulesetLoadKeepsNat44Enabled)
{
    FakePfctl  pfctl;
    PfFirewall firewall(pfctl, "utun5");

    ASSERT_EQ(firewall.Init(), OTBR_ERROR_NONE);
    ASSERT_EQ(firewall.EnableNat44Masquerade("en0"), OTBR_ERROR_NONE);

    pfctl.mFailRulesetLoad = true;
    EXPECT_NE(firewall.DisableNat44Masquerade(), OTBR_ERROR_NONE);
    EXPECT_TRUE(firewall.IsNat44Enabled());

    // The next load still carries the nat rule.
    pfctl.mFailRulesetLoad = false;
    ASSERT_EQ(firewall.EnableIngressFilter(), OTBR_ERROR_NONE);
    EXPECT_NE(pfctl.Last().mInput.find("nat pass on en0"), std::string::npos);
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

TEST(PfFirewall, VerifyInstalledDoesNothingBeforeAnythingIsEnabled)
{
    FakePfctl  pfctl;
    PfFirewall firewall(pfctl, "utun5");
    bool       reinstalled = true;

    ASSERT_EQ(firewall.Init(), OTBR_ERROR_NONE);
    size_t calls = pfctl.mCalls.size();

    EXPECT_EQ(firewall.VerifyInstalled(reinstalled), OTBR_ERROR_NONE);
    EXPECT_FALSE(reinstalled);
    EXPECT_EQ(pfctl.mCalls.size(), calls);
}

TEST(PfFirewall, VerifyInstalledIsQuietWhenTheAnchorHoldsTheRules)
{
    FakePfctl  pfctl;
    PfFirewall firewall(pfctl, "utun5");
    bool       reinstalled = true;

    ASSERT_EQ(firewall.Init(), OTBR_ERROR_NONE);
    ASSERT_EQ(firewall.EnableIngressFilter(), OTBR_ERROR_NONE);
    ASSERT_EQ(firewall.ReplaceIngressPrefixes({Ip6Prefix("fd00:1::", 64)}, {Ip6Prefix("fd00:1::", 64)}),
              OTBR_ERROR_NONE);
    size_t calls = pfctl.mCalls.size();

    EXPECT_EQ(firewall.VerifyInstalled(reinstalled), OTBR_ERROR_NONE);
    EXPECT_FALSE(reinstalled);
    // Four listings, nothing else: pf's status, the main ruleset's reference,
    // the anchor's rules and its tables.
    ASSERT_EQ(pfctl.mCalls.size(), calls + 4);
    EXPECT_EQ(pfctl.mCalls[calls].Command(), "pfctl -s info");
    EXPECT_EQ(pfctl.mCalls[calls + 1].Command(), "pfctl -s rules");
    EXPECT_EQ(pfctl.mCalls[calls + 2].Command(), "pfctl -a otbr -s rules");
    EXPECT_EQ(pfctl.mCalls[calls + 3].Command(), "pfctl -a otbr -s Tables");
}

TEST(PfFirewall, VerifyInstalledEnablesPfAgain)
{
    FakePfctl  pfctl;
    PfFirewall firewall(pfctl, "utun5");
    bool       reinstalled = true;

    ASSERT_EQ(firewall.Init(), OTBR_ERROR_NONE);
    ASSERT_EQ(firewall.EnableIngressFilter(), OTBR_ERROR_NONE);

    // `pfctl -d` behind the agent's back: the rules are all there, unapplied.
    pfctl.mInfoListing  = "Status: Disabled for 0 days 00:00:03           Debug: Urgent\n";
    pfctl.mEnableOutput = "pf enabled\nToken : 4343\n";
    size_t calls        = pfctl.mCalls.size();

    EXPECT_EQ(firewall.VerifyInstalled(reinstalled), OTBR_ERROR_NONE);
    EXPECT_FALSE(reinstalled);
    // The stale reference released, a new one taken, then the usual listings.
    ASSERT_GE(pfctl.mCalls.size(), calls + 3);
    EXPECT_EQ(pfctl.mCalls[calls].Command(), "pfctl -s info");
    EXPECT_EQ(pfctl.mCalls[calls + 1].Command(), "pfctl -X 4242");
    EXPECT_EQ(pfctl.mCalls[calls + 2].Command(), "pfctl -E");

    ASSERT_EQ(firewall.Deinit(), OTBR_ERROR_NONE);
    EXPECT_EQ(pfctl.Last().Command(), "pfctl -X 4343");
}

TEST(PfFirewall, VerifyInstalledFailsWhenTheMainRulesetDroppedTheAnchor)
{
    FakePfctl  pfctl;
    PfFirewall firewall(pfctl, "utun5");
    bool       reinstalled = true;

    ASSERT_EQ(firewall.Init(), OTBR_ERROR_NONE);
    ASSERT_EQ(firewall.EnableIngressFilter(), OTBR_ERROR_NONE);

    // `pfctl -F all` on the main ruleset: the anchor keeps its rules, unevaluated.
    pfctl.mRulesListing = "anchor \"com.apple/*\" all\n";
    size_t calls        = pfctl.mCalls.size();

    EXPECT_EQ(firewall.VerifyInstalled(reinstalled), OTBR_ERROR_NOT_FOUND);
    EXPECT_FALSE(reinstalled);
    // Nothing is reloaded: the anchor is not the problem.
    ASSERT_EQ(pfctl.mCalls.size(), calls + 2);
    EXPECT_EQ(pfctl.Last().Command(), "pfctl -s rules");
}

TEST(PfFirewall, VerifyInstalledNeedsTheFilterAnchorForNat44Alone)
{
    FakePfctl  pfctl;
    PfFirewall firewall(pfctl, "utun5");
    bool       reinstalled = true;

    ASSERT_EQ(firewall.Init(), OTBR_ERROR_NONE);
    ASSERT_EQ(firewall.EnableNat44Masquerade("en0"), OTBR_ERROR_NONE);

    // The rule that tags the packets the NAT rule keys on is a filter rule in
    // anchor "otbr", so the main ruleset must reference it with NAT44 alone too.
    pfctl.mRulesListing = "anchor \"com.apple/*\" all\n";
    size_t calls        = pfctl.mCalls.size();

    EXPECT_EQ(firewall.VerifyInstalled(reinstalled), OTBR_ERROR_NOT_FOUND);
    EXPECT_FALSE(reinstalled);
    ASSERT_EQ(pfctl.mCalls.size(), calls + 2);
    EXPECT_EQ(pfctl.Last().Command(), "pfctl -s rules");
}

TEST(PfFirewall, VerifyInstalledReinstallsAnEmptiedAnchor)
{
    FakePfctl  pfctl;
    PfFirewall firewall(pfctl, "utun5");
    bool       reinstalled = false;

    ASSERT_EQ(firewall.Init(), OTBR_ERROR_NONE);
    ASSERT_EQ(firewall.EnableIngressFilter(), OTBR_ERROR_NONE);
    ASSERT_EQ(firewall.ReplaceIngressPrefixes({Ip6Prefix("fd00:2::", 64), Ip6Prefix("fd00:1::", 64)},
                                              {Ip6Prefix("fd00:1::", 64)}),
              OTBR_ERROR_NONE);

    // `pfctl -a otbr -F all` behind the agent's back: no rules, no tables.
    pfctl.mAnchorRules.clear();
    pfctl.mAnchorTables.clear();
    size_t calls = pfctl.mCalls.size();

    EXPECT_EQ(firewall.VerifyInstalled(reinstalled), OTBR_ERROR_NONE);
    EXPECT_TRUE(reinstalled);
    // pf's status and the reference are fine; the anchor's rules come back
    // empty; then the ruleset is loaded again (no flush: the load replaces the
    // anchor's rulesets in one commit) and both tables get what they last held.
    ASSERT_EQ(pfctl.mCalls.size(), calls + 6);
    EXPECT_EQ(pfctl.mCalls[calls].Command(), "pfctl -s info");
    EXPECT_EQ(pfctl.mCalls[calls + 1].Command(), "pfctl -s rules");
    EXPECT_EQ(pfctl.mCalls[calls + 2].Command(), "pfctl -a otbr -s rules");
    EXPECT_EQ(pfctl.mCalls[calls + 3].Command(), "pfctl -a otbr -f -");
    EXPECT_EQ(pfctl.mCalls[calls + 3].mInput, kIngressRuleset);
    EXPECT_EQ(pfctl.mCalls[calls + 4].Command(),
              "pfctl -a otbr -t ingress_deny_src -T replace fd00:1::/64 fd00:2::/64");
    EXPECT_EQ(pfctl.mCalls[calls + 5].Command(), "pfctl -a otbr -t ingress_allow_dst -T replace fd00:1::/64");
}

TEST(PfFirewall, VerifyInstalledReinstallsWhenARuleIsGone)
{
    FakePfctl  pfctl;
    PfFirewall firewall(pfctl, "utun5");
    bool       reinstalled = false;

    ASSERT_EQ(firewall.Init(), OTBR_ERROR_NONE);
    ASSERT_EQ(firewall.EnableIngressFilter(), OTBR_ERROR_NONE);

    // Every rule but the terminal block is still there.
    pfctl.mAnchorRules = "pass out quick on utun5 inet6 from (utun5) to any flags S/SA keep state\n"
                         "block drop out quick on utun5 inet6 from <ingress_deny_src> to any\n"
                         "pass out quick on utun5 inet6 from any to <ingress_allow_dst> flags S/SA keep state\n";

    EXPECT_EQ(firewall.VerifyInstalled(reinstalled), OTBR_ERROR_NONE);
    EXPECT_TRUE(reinstalled);
    EXPECT_EQ(pfctl.mCalls[pfctl.mCalls.size() - 3].Command(), "pfctl -a otbr -f -");
}

TEST(PfFirewall, VerifyInstalledReinstallsWhenATableIsGone)
{
    FakePfctl  pfctl;
    PfFirewall firewall(pfctl, "utun5");
    bool       reinstalled = false;

    ASSERT_EQ(firewall.Init(), OTBR_ERROR_NONE);
    ASSERT_EQ(firewall.EnableIngressFilter(), OTBR_ERROR_NONE);

    // Whole names: a table whose name merely contains ours does not count.
    pfctl.mAnchorTables = "ingress_deny_src\nold_ingress_allow_dst\n";

    EXPECT_EQ(firewall.VerifyInstalled(reinstalled), OTBR_ERROR_NONE);
    EXPECT_TRUE(reinstalled);
    EXPECT_EQ(pfctl.Last().Command(), "pfctl -a otbr -t ingress_allow_dst -T flush");
}

TEST(PfFirewall, VerifyInstalledChecksTheNatRuleToo)
{
    FakePfctl  pfctl;
    PfFirewall firewall(pfctl, "utun5");
    bool       reinstalled = false;

    ASSERT_EQ(firewall.Init(), OTBR_ERROR_NONE);
    ASSERT_EQ(firewall.EnableIngressFilter(), OTBR_ERROR_NONE);
    ASSERT_EQ(firewall.EnableNat44Masquerade("en0"), OTBR_ERROR_NONE);
    pfctl.mAnchorRules = kAnchorTagRule + pfctl.mAnchorRules;

    size_t calls = pfctl.mCalls.size();
    EXPECT_EQ(firewall.VerifyInstalled(reinstalled), OTBR_ERROR_NONE);
    EXPECT_FALSE(reinstalled);
    // The nat-anchor reference and the anchor's NAT rules are listed as well.
    ASSERT_EQ(pfctl.mCalls.size(), calls + 6);
    EXPECT_EQ(pfctl.mCalls[calls + 2].Command(), "pfctl -s nat");
    EXPECT_EQ(pfctl.Last().Command(), "pfctl -a otbr -s nat");

    pfctl.mAnchorNat.clear();
    EXPECT_EQ(firewall.VerifyInstalled(reinstalled), OTBR_ERROR_NONE);
    EXPECT_TRUE(reinstalled);
    // The reloaded ruleset carries the NAT rules again.
    const PfctlCall &load = pfctl.mCalls[pfctl.mCalls.size() - 3];
    EXPECT_EQ(load.Command(), "pfctl -a otbr -f -");
    EXPECT_NE(load.mInput.find("nat pass on en0 inet tagged otbr_thread -> (en0)"), std::string::npos);

    // `pfctl -a otbr -F rules`: the nat rule stays, the rule that tags the
    // packets it keys on is gone.
    pfctl.mAnchorNat   = "nat pass on en0 inet tagged otbr_thread -> (en0) round-robin\n";
    pfctl.mAnchorRules = FakePfctl().mAnchorRules;
    EXPECT_EQ(firewall.VerifyInstalled(reinstalled), OTBR_ERROR_NONE);
    EXPECT_TRUE(reinstalled);
}

TEST(PfFirewall, VerifyInstalledIgnoresAListingThatFails)
{
    FakePfctl  pfctl;
    PfFirewall firewall(pfctl, "utun5");
    bool       reinstalled = true;

    ASSERT_EQ(firewall.Init(), OTBR_ERROR_NONE);
    ASSERT_EQ(firewall.EnableIngressFilter(), OTBR_ERROR_NONE);

    pfctl.mFailListings = true;
    size_t calls        = pfctl.mCalls.size();
    EXPECT_EQ(firewall.VerifyInstalled(reinstalled), OTBR_ERROR_NONE);
    EXPECT_FALSE(reinstalled);
    // pf's status and the reference, then the anchor listing that fails.
    EXPECT_EQ(pfctl.mCalls.size(), calls + 3);
}

TEST(PfFirewall, ReplaceIngressPrefixesReinstallsWhenTheTableIsGone)
{
    FakePfctl  pfctl;
    PfFirewall firewall(pfctl, "utun5");

    ASSERT_EQ(firewall.Init(), OTBR_ERROR_NONE);
    ASSERT_EQ(firewall.EnableIngressFilter(), OTBR_ERROR_NONE);

    // The first replacement, a flush, meets a table the anchor no longer has.
    pfctl.mFailTableReplaces = 1;
    size_t calls             = pfctl.mCalls.size();
    ASSERT_EQ(firewall.ReplaceIngressPrefixes({}, {Ip6Prefix("fd00:1::", 64)}), OTBR_ERROR_NONE);
    // The failed flush, then the ruleset and both tables with the new contents.
    ASSERT_EQ(pfctl.mCalls.size(), calls + 4);
    EXPECT_EQ(pfctl.mCalls[calls].Command(), "pfctl -a otbr -t ingress_deny_src -T flush");
    EXPECT_EQ(pfctl.mCalls[calls + 1].Command(), "pfctl -a otbr -f -");
    EXPECT_EQ(pfctl.mCalls[calls + 2].Command(), "pfctl -a otbr -t ingress_deny_src -T flush");
    EXPECT_EQ(pfctl.Last().Command(), "pfctl -a otbr -t ingress_allow_dst -T replace fd00:1::/64");
}

TEST(PfFirewall, ReplaceIngressPrefixesReinstallsWhenPfctlHadToCreateTheTable)
{
    FakePfctl  pfctl;
    PfFirewall firewall(pfctl, "utun5");

    ASSERT_EQ(firewall.Init(), OTBR_ERROR_NONE);
    ASSERT_EQ(firewall.EnableIngressFilter(), OTBR_ERROR_NONE);

    // `-T replace` with addresses creates a missing table and exits 0; only its
    // message tells that the anchor was emptied.
    pfctl.mCreateTables = 1;
    size_t calls        = pfctl.mCalls.size();
    ASSERT_EQ(firewall.ReplaceIngressPrefixes({Ip6Prefix("fd00:1::", 64)}, {Ip6Prefix("fd00:2::", 64)}),
              OTBR_ERROR_NONE);
    ASSERT_EQ(pfctl.mCalls.size(), calls + 4);
    EXPECT_EQ(pfctl.mCalls[calls].Command(), "pfctl -a otbr -t ingress_deny_src -T replace fd00:1::/64");
    EXPECT_EQ(pfctl.mCalls[calls + 1].Command(), "pfctl -a otbr -f -");
    EXPECT_EQ(pfctl.mCalls[calls + 2].Command(), "pfctl -a otbr -t ingress_deny_src -T replace fd00:1::/64");
    EXPECT_EQ(pfctl.Last().Command(), "pfctl -a otbr -t ingress_allow_dst -T replace fd00:2::/64");
}

TEST(PfFirewall, ReplaceIngressPrefixesRejectsAnInvalidPrefixAndKeepsThePrevious)
{
    FakePfctl  pfctl;
    PfFirewall firewall(pfctl, "utun5");
    Ip6Prefix  tooLong("fd00:1::", 64);

    tooLong.mLength = 129;

    ASSERT_EQ(firewall.Init(), OTBR_ERROR_NONE);
    ASSERT_EQ(firewall.EnableIngressFilter(), OTBR_ERROR_NONE);
    ASSERT_EQ(firewall.ReplaceIngressPrefixes({Ip6Prefix("fd00:1::", 64)}, {Ip6Prefix("fd00:1::", 64)}),
              OTBR_ERROR_NONE);

    size_t calls = pfctl.mCalls.size();
    EXPECT_EQ(firewall.ReplaceIngressPrefixes({tooLong}, {}), OTBR_ERROR_INVALID_ARGS);
    EXPECT_EQ(firewall.ReplaceIngressPrefixes({}, {tooLong}), OTBR_ERROR_INVALID_ARGS);
    // Nothing was sent, and a reinstall still puts the previous contents back.
    EXPECT_EQ(pfctl.mCalls.size(), calls);
    ASSERT_EQ(firewall.Reinstall(), OTBR_ERROR_NONE);
    EXPECT_EQ(pfctl.mCalls[pfctl.mCalls.size() - 2].Command(),
              "pfctl -a otbr -t ingress_deny_src -T replace fd00:1::/64");
    EXPECT_EQ(pfctl.Last().Command(), "pfctl -a otbr -t ingress_allow_dst -T replace fd00:1::/64");
}

TEST(PfFirewall, ReplaceIngressPrefixesReportsAnUnrelatedFailureAndKeepsThePrevious)
{
    FakePfctl  pfctl;
    PfFirewall firewall(pfctl, "utun5");

    ASSERT_EQ(firewall.Init(), OTBR_ERROR_NONE);
    ASSERT_EQ(firewall.EnableIngressFilter(), OTBR_ERROR_NONE);
    ASSERT_EQ(firewall.ReplaceIngressPrefixes({Ip6Prefix("fd00:1::", 64)}, {Ip6Prefix("fd00:1::", 64)}),
              OTBR_ERROR_NONE);

    // A failure that does not say the table is gone is not a reason to reinstall.
    pfctl.mFailTableReplaces = 1;
    pfctl.mTableReplaceError = "pfctl: /dev/pf: Permission denied\n";
    size_t calls             = pfctl.mCalls.size();
    EXPECT_EQ(firewall.ReplaceIngressPrefixes({Ip6Prefix("fd00:2::", 64)}, {Ip6Prefix("fd00:2::", 64)}),
              OTBR_ERROR_ERRNO);
    EXPECT_EQ(pfctl.mCalls.size(), calls + 1);
    ASSERT_EQ(firewall.Reinstall(), OTBR_ERROR_NONE);
    EXPECT_EQ(pfctl.Last().Command(), "pfctl -a otbr -t ingress_allow_dst -T replace fd00:1::/64");
}

TEST(PfFirewall, DeinitForgetsTheTableContents)
{
    FakePfctl  pfctl;
    PfFirewall firewall(pfctl, "utun5");

    ASSERT_EQ(firewall.Init(), OTBR_ERROR_NONE);
    ASSERT_EQ(firewall.EnableIngressFilter(), OTBR_ERROR_NONE);
    ASSERT_EQ(firewall.ReplaceIngressPrefixes({Ip6Prefix("fd00:1::", 64)}, {Ip6Prefix("fd00:1::", 64)}),
              OTBR_ERROR_NONE);
    ASSERT_EQ(firewall.Deinit(), OTBR_ERROR_NONE);

    // The same object, initialised again: a reinstall carries nothing from before.
    ASSERT_EQ(firewall.Init(), OTBR_ERROR_NONE);
    ASSERT_EQ(firewall.EnableIngressFilter(), OTBR_ERROR_NONE);
    ASSERT_EQ(firewall.Reinstall(), OTBR_ERROR_NONE);
    EXPECT_EQ(pfctl.mCalls[pfctl.mCalls.size() - 2].Command(), "pfctl -a otbr -t ingress_deny_src -T flush");
    EXPECT_EQ(pfctl.Last().Command(), "pfctl -a otbr -t ingress_allow_dst -T flush");
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
