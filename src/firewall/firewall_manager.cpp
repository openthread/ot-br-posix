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

#define OTBR_LOG_TAG "FIREWALL"

#include "firewall/firewall_manager.hpp"

#include <string.h>
#include <vector>

#include "common/logging.hpp"

namespace otbr {
namespace Firewall {

namespace {

// Whether every address of aInner is an address of aOuter as well.
bool Covers(const Ip6Prefix &aOuter, const Ip6Prefix &aInner)
{
    bool      covers    = false;
    Ip6Prefix truncated = aInner;

    VerifyOrExit(aOuter.mLength <= aInner.mLength);

    // Ip6Prefix compares the leading mLength bits and nothing after them.
    truncated.mLength = aOuter.mLength;
    covers            = (truncated == aOuter);

exit:
    return covers;
}

// The prefixes of aPrefixes that no other one covers, in their order.
//
// The sets are interval sets, and the kernel refuses an interval that
// overlaps one already in the set. Two prefixes overlap exactly when one
// covers the other, and the covered one adds no address to the set.
std::vector<Ip6Prefix> WithoutCoveredPrefixes(const std::vector<Ip6Prefix> &aPrefixes)
{
    std::vector<Ip6Prefix> uncovered;

    for (size_t i = 0; i < aPrefixes.size(); i++)
    {
        bool covered = false;

        for (size_t j = 0; j < aPrefixes.size() && !covered; j++)
        {
            // Equal prefixes cover each other; the first of them stays. The
            // cheap test first: it also rules out j == i, and most pairs.
            covered = (aPrefixes[j].mLength < aPrefixes[i].mLength || j < i) && Covers(aPrefixes[j], aPrefixes[i]);
        }

        if (!covered)
        {
            uncovered.push_back(aPrefixes[i]);
        }
    }

    return uncovered;
}

} // namespace

const char *const  FirewallManager::kIngressChain        = "forward_ingress";
const char *const  FirewallManager::kPreroutingChain     = "dua_prerouting";
const char *const  FirewallManager::kNatPreroutingChain  = "nat_prerouting";
const char *const  FirewallManager::kNatPostroutingChain = "nat_postrouting";
const char *const  FirewallManager::kNatForwardChain     = "nat_forward";
const char *const  FirewallManager::kIngressDenySrcSet   = "ingress_deny_src";
const char *const  FirewallManager::kIngressAllowDstSet  = "ingress_allow_dst";
constexpr uint32_t FirewallManager::kNat44Mark;

FirewallManager::FirewallManager(INftables &aNftables, const std::string &aThreadInterfaceName)
    : mNftables(aNftables)
    , mThreadIfName(aThreadInterfaceName)
    , mTableName("otbr_" + aThreadInterfaceName)
    , mInitialized(false)
    , mIngressFilterEnabled(false)
    , mNat44Enabled(false)
    , mDuaChainCreated(false)
    , mNdRuleHandle(0)
{
}

otbrError FirewallManager::Init(void)
{
    otbrError error = OTBR_ERROR_NONE;

    VerifyOrExit(!mInitialized, error = OTBR_ERROR_INVALID_STATE);
    VerifyOrExit(!mThreadIfName.empty(), error = OTBR_ERROR_INVALID_ARGS);

    // Idempotent reset: drop a leftover table of this name from a prior run
    // and recreate it empty, in one transaction so no partial state is visible.
    SuccessOrExit(error = mNftables.BeginBatch());
    SuccessOrExit(error = mNftables.DelTable(mTableName));
    SuccessOrExit(error = mNftables.AddTable(mTableName));
    SuccessOrExit(error = mNftables.CommitBatch());

    mInitialized = true;

exit:
    if (error != OTBR_ERROR_NONE)
    {
        mNftables.AbortBatch();
    }
    otbrLogResult(error, "FirewallManager: %s", __FUNCTION__);
    return error;
}

otbrError FirewallManager::Deinit(void)
{
    otbrError error = OTBR_ERROR_NONE;

    VerifyOrExit(mInitialized);

    if ((error = mNftables.BeginBatch()) == OTBR_ERROR_NONE)
    {
        if ((error = mNftables.DelTable(mTableName)) == OTBR_ERROR_NONE)
        {
            error = mNftables.CommitBatch();
        }

        if (error != OTBR_ERROR_NONE)
        {
            mNftables.AbortBatch();
        }
    }

    mNdRuleHandle         = 0;
    mDuaChainCreated      = false;
    mIngressFilterEnabled = false;
    mNat44Enabled         = false;
    mInitialized          = false;

exit:
    otbrLogResult(error, "FirewallManager: %s", __FUNCTION__);
    return error;
}

otbrError FirewallManager::EnableIngressFilter(void)
{
    otbrError error = OTBR_ERROR_NONE;

    VerifyOrExit(mInitialized, error = OTBR_ERROR_INVALID_STATE);
    VerifyOrExit(!mIngressFilterEnabled);

    SuccessOrExit(error = mNftables.BeginBatch());

    SuccessOrExit(error = mNftables.AddIp6PrefixSet(mTableName, kIngressDenySrcSet));
    SuccessOrExit(error = mNftables.AddIp6PrefixSet(mTableName, kIngressAllowDstSet));

    SuccessOrExit(error = mNftables.AddChain(mTableName, kIngressChain, Hook::kForward, ChainPriority::kFilter));

    // This chain filters IPv6 and nothing else, as the ip6tables rules it
    // replaces did. The table is inet, though, so without this the unicast
    // drop below also catches IPv4 forwarded to the Thread interface -- the
    // replies NAT64 depends on. nat_forward accepting them does not help:
    // an accept ends that base chain only, and this one still runs.
    SuccessOrExit(error = mNftables.AddRuleNfprotoNeqIp6Return(mTableName, kIngressChain, nullptr));
    SuccessOrExit(error = mNftables.AddRuleOifnameNeqReturn(mTableName, kIngressChain, mThreadIfName, nullptr));
    SuccessOrExit(error = mNftables.AddRuleIifPkttypeVerdict(mTableName, kIngressChain, mThreadIfName,
                                                             PktType::kUnicast, Verdict::kDrop, nullptr));
    SuccessOrExit(error = mNftables.AddRuleSetLookupVerdict(mTableName, kIngressChain, kIngressDenySrcSet,
                                                            SetDirection::kSrc, Verdict::kDrop, nullptr));
    SuccessOrExit(error = mNftables.AddRuleSetLookupVerdict(mTableName, kIngressChain, kIngressAllowDstSet,
                                                            SetDirection::kDst, Verdict::kAccept, nullptr));
    SuccessOrExit(
        error = mNftables.AddRulePkttypeVerdict(mTableName, kIngressChain, PktType::kUnicast, Verdict::kDrop, nullptr));
    SuccessOrExit(error = mNftables.AddRuleVerdict(mTableName, kIngressChain, Verdict::kAccept, nullptr));

    SuccessOrExit(error = mNftables.CommitBatch());

    mIngressFilterEnabled = true;

exit:
    if (error != OTBR_ERROR_NONE)
    {
        mNftables.AbortBatch();
    }
    otbrLogResult(error, "FirewallManager: %s", __FUNCTION__);
    return error;
}

otbrError FirewallManager::EnableNat44Masquerade(const std::string &aUpstreamInterfaceName)
{
    otbrError error = OTBR_ERROR_NONE;

    VerifyOrExit(mInitialized, error = OTBR_ERROR_INVALID_STATE);
    VerifyOrExit(!aUpstreamInterfaceName.empty(), error = OTBR_ERROR_INVALID_ARGS);
    VerifyOrExit(!mNat44Enabled);

    SuccessOrExit(error = mNftables.BeginBatch());

    // Mangle-prerouting: tag packets coming from the Thread interface so the
    // postrouting chain can MASQUERADE them. Type filter, mangle priority.
    SuccessOrExit(error = mNftables.AddChain(mTableName, kNatPreroutingChain, Hook::kPrerouting, ChainPriority::kMangle,
                                             ChainType::kFilter));
    SuccessOrExit(error =
                      mNftables.AddRuleIifMark(mTableName, kNatPreroutingChain, mThreadIfName, kNat44Mark, nullptr));

    // Postrouting: source-NAT marked traffic. Type nat, srcnat priority.
    SuccessOrExit(error = mNftables.AddChain(mTableName, kNatPostroutingChain, Hook::kPostrouting,
                                             ChainPriority::kSrcNat, ChainType::kNat));
    SuccessOrExit(error = mNftables.AddRuleMarkMasquerade(mTableName, kNatPostroutingChain, kNat44Mark, nullptr));

    // Forward: accept traffic in either direction on the upstream interface.
    // Hooked at FORWARD/filter alongside forward_ingress. Accepting here does
    // not exempt a packet from that chain -- every base chain on a hook runs
    // -- so forward_ingress leaves IPv4 alone by itself.
    SuccessOrExit(error = mNftables.AddChain(mTableName, kNatForwardChain, Hook::kForward, ChainPriority::kFilter,
                                             ChainType::kFilter));
    SuccessOrExit(error = mNftables.AddRuleOifnameVerdict(mTableName, kNatForwardChain, aUpstreamInterfaceName,
                                                          Verdict::kAccept, nullptr));
    SuccessOrExit(error = mNftables.AddRuleIifnameVerdict(mTableName, kNatForwardChain, aUpstreamInterfaceName,
                                                          Verdict::kAccept, nullptr));

    SuccessOrExit(error = mNftables.CommitBatch());

    mNat44Enabled = true;

exit:
    if (error != OTBR_ERROR_NONE)
    {
        mNftables.AbortBatch();
    }
    otbrLogResult(error, "FirewallManager: %s", __FUNCTION__);
    return error;
}

otbrError FirewallManager::EnableNdProxyRedirect(const Ip6Prefix   &aDomainPrefix,
                                                 const std::string &aBackboneInterfaceName,
                                                 uint16_t           aQueueNum)
{
    otbrError error     = OTBR_ERROR_NONE;
    uint64_t  newHandle = 0;

    VerifyOrExit(mInitialized, error = OTBR_ERROR_INVALID_STATE);
    VerifyOrExit(aDomainPrefix.IsValid(), error = OTBR_ERROR_INVALID_ARGS);
    VerifyOrExit(!aBackboneInterfaceName.empty(), error = OTBR_ERROR_INVALID_ARGS);

    SuccessOrExit(error = mNftables.BeginBatch());

    if (!mDuaChainCreated)
    {
        SuccessOrExit(error = mNftables.AddChain(mTableName, kPreroutingChain, Hook::kPrerouting, ChainPriority::kRaw));
    }

    if (mNdRuleHandle != 0)
    {
        SuccessOrExit(error = mNftables.DelRule(mTableName, kPreroutingChain, mNdRuleHandle));
    }

    SuccessOrExit(error = mNftables.AddRuleNdNsRedirect(mTableName, kPreroutingChain, aDomainPrefix,
                                                        aBackboneInterfaceName, aQueueNum, &newHandle));

    SuccessOrExit(error = mNftables.CommitBatch());

    mDuaChainCreated = true;
    mNdRuleHandle    = newHandle;

exit:
    if (error != OTBR_ERROR_NONE)
    {
        mNftables.AbortBatch();
    }
    otbrLogResult(error, "FirewallManager: %s", __FUNCTION__);
    return error;
}

otbrError FirewallManager::DisableNdProxyRedirect(void)
{
    otbrError error = OTBR_ERROR_NONE;

    VerifyOrExit(mInitialized, error = OTBR_ERROR_INVALID_STATE);
    VerifyOrExit(mNdRuleHandle != 0);

    SuccessOrExit(error = mNftables.BeginBatch());
    SuccessOrExit(error = mNftables.DelRule(mTableName, kPreroutingChain, mNdRuleHandle));
    SuccessOrExit(error = mNftables.CommitBatch());

    mNdRuleHandle = 0;

exit:
    if (error != OTBR_ERROR_NONE)
    {
        mNftables.AbortBatch();
    }
    otbrLogResult(error, "FirewallManager: %s", __FUNCTION__);
    return error;
}

otbrError FirewallManager::AddIngressSetElement(IngressSet aSet, const Ip6Prefix &aPrefix)
{
    otbrError error = OTBR_ERROR_NONE;

    VerifyOrExit(mIngressFilterEnabled, error = OTBR_ERROR_INVALID_STATE);
    VerifyOrExit(aPrefix.IsValid(), error = OTBR_ERROR_INVALID_ARGS);

    SuccessOrExit(error = mNftables.BeginBatch());
    SuccessOrExit(error = mNftables.AddSetElement(mTableName, SetName(aSet), aPrefix));
    SuccessOrExit(error = mNftables.CommitBatch());

exit:
    if (error != OTBR_ERROR_NONE)
    {
        mNftables.AbortBatch();
    }
    return error;
}

otbrError FirewallManager::DelIngressSetElement(IngressSet aSet, const Ip6Prefix &aPrefix)
{
    otbrError error = OTBR_ERROR_NONE;

    VerifyOrExit(mIngressFilterEnabled, error = OTBR_ERROR_INVALID_STATE);
    VerifyOrExit(aPrefix.IsValid(), error = OTBR_ERROR_INVALID_ARGS);

    SuccessOrExit(error = mNftables.BeginBatch());
    SuccessOrExit(error = mNftables.DelSetElement(mTableName, SetName(aSet), aPrefix));
    SuccessOrExit(error = mNftables.CommitBatch());

exit:
    if (error != OTBR_ERROR_NONE)
    {
        mNftables.AbortBatch();
    }
    return error;
}

otbrError FirewallManager::FlushIngressSet(IngressSet aSet)
{
    otbrError error = OTBR_ERROR_NONE;

    VerifyOrExit(mIngressFilterEnabled, error = OTBR_ERROR_INVALID_STATE);

    SuccessOrExit(error = mNftables.BeginBatch());
    SuccessOrExit(error = mNftables.FlushSet(mTableName, SetName(aSet)));
    SuccessOrExit(error = mNftables.CommitBatch());

exit:
    if (error != OTBR_ERROR_NONE)
    {
        mNftables.AbortBatch();
    }
    return error;
}

otbrError FirewallManager::ReplaceIngressPrefixes(const std::vector<Ip6Prefix> &aDenySrc,
                                                  const std::vector<Ip6Prefix> &aAllowDst)
{
    otbrError error = OTBR_ERROR_NONE;

    VerifyOrExit(mIngressFilterEnabled, error = OTBR_ERROR_INVALID_STATE);

    // Validated before a batch is opened, and before the covered prefixes are
    // dropped: an invalid prefix longer than 128 bits that a shorter, valid
    // one covers would otherwise be dropped unseen.
    for (const Ip6Prefix &prefix : aDenySrc)
    {
        VerifyOrExit(prefix.IsValid(), error = OTBR_ERROR_INVALID_ARGS);
    }
    for (const Ip6Prefix &prefix : aAllowDst)
    {
        VerifyOrExit(prefix.IsValid(), error = OTBR_ERROR_INVALID_ARGS);
    }

    SuccessOrExit(error = mNftables.BeginBatch());
    SuccessOrExit(error = mNftables.FlushSet(mTableName, kIngressDenySrcSet));
    SuccessOrExit(error = mNftables.FlushSet(mTableName, kIngressAllowDstSet));
    // Each set on its own: what one set covers says nothing about the other.
    for (const Ip6Prefix &prefix : WithoutCoveredPrefixes(aDenySrc))
    {
        SuccessOrExit(error = mNftables.AddSetElement(mTableName, kIngressDenySrcSet, prefix));
    }
    for (const Ip6Prefix &prefix : WithoutCoveredPrefixes(aAllowDst))
    {
        SuccessOrExit(error = mNftables.AddSetElement(mTableName, kIngressAllowDstSet, prefix));
    }
    SuccessOrExit(error = mNftables.CommitBatch());

exit:
    if (error != OTBR_ERROR_NONE)
    {
        mNftables.AbortBatch();
    }
    otbrLogResult(error, "FirewallManager: %s", __FUNCTION__);
    return error;
}

const char *FirewallManager::SetName(IngressSet aSet)
{
    switch (aSet)
    {
    case IngressSet::kDenySrc:
        return kIngressDenySrcSet;
    case IngressSet::kAllowDst:
        return kIngressAllowDstSet;
    }
    return "";
}

} // namespace Firewall
} // namespace otbr
