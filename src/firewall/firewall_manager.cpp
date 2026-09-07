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
#include <utility>

#include "common/logging.hpp"

namespace otbr {
namespace Firewall {

const char *const  FirewallManager::kTableName           = "otbr";
const char *const  FirewallManager::kIngressChain        = "forward_ingress";
const char *const  FirewallManager::kPreroutingChain     = "dua_prerouting";
const char *const  FirewallManager::kNatPreroutingChain  = "nat_prerouting";
const char *const  FirewallManager::kNatPostroutingChain = "nat_postrouting";
const char *const  FirewallManager::kNatForwardChain     = "nat_forward";
const char *const  FirewallManager::kIngressDenySrcSet   = "ingress_deny_src";
const char *const  FirewallManager::kIngressAllowDstSet  = "ingress_allow_dst";
constexpr uint32_t FirewallManager::kNat44Mark;

FirewallManager::FirewallManager(INftables &aNftables, std::string aThreadInterfaceName)
    : mNftables(aNftables)
    , mThreadIfName(std::move(aThreadInterfaceName))
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

    // Idempotent reset: drop any leftover OTBR table from a prior run and
    // recreate it empty, in one transaction so no partial state is visible.
    SuccessOrExit(error = mNftables.BeginBatch());
    SuccessOrExit(error = mNftables.DelTable(kTableName));
    SuccessOrExit(error = mNftables.AddTable(kTableName));
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
        if ((error = mNftables.DelTable(kTableName)) == OTBR_ERROR_NONE)
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

    SuccessOrExit(error = mNftables.AddIp6PrefixSet(kTableName, kIngressDenySrcSet));
    SuccessOrExit(error = mNftables.AddIp6PrefixSet(kTableName, kIngressAllowDstSet));

    SuccessOrExit(error = mNftables.AddChain(kTableName, kIngressChain, Hook::kForward, ChainPriority::kFilter));

    SuccessOrExit(error = mNftables.AddRuleOifnameNeqReturn(kTableName, kIngressChain, mThreadIfName, nullptr));
    SuccessOrExit(error = mNftables.AddRuleIifPkttypeVerdict(kTableName, kIngressChain, mThreadIfName,
                                                             PktType::kUnicast, Verdict::kDrop, nullptr));
    SuccessOrExit(error = mNftables.AddRuleSetLookupVerdict(kTableName, kIngressChain, kIngressDenySrcSet,
                                                            SetDirection::kSrc, Verdict::kDrop, nullptr));
    SuccessOrExit(error = mNftables.AddRuleSetLookupVerdict(kTableName, kIngressChain, kIngressAllowDstSet,
                                                            SetDirection::kDst, Verdict::kAccept, nullptr));
    SuccessOrExit(
        error = mNftables.AddRulePkttypeVerdict(kTableName, kIngressChain, PktType::kUnicast, Verdict::kDrop, nullptr));
    SuccessOrExit(error = mNftables.AddRuleVerdict(kTableName, kIngressChain, Verdict::kAccept, nullptr));

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
    SuccessOrExit(error = mNftables.AddChain(kTableName, kNatPreroutingChain, Hook::kPrerouting, ChainPriority::kMangle,
                                             ChainType::kFilter));
    SuccessOrExit(error =
                      mNftables.AddRuleIifMark(kTableName, kNatPreroutingChain, mThreadIfName, kNat44Mark, nullptr));

    // Postrouting: source-NAT marked traffic. Type nat, srcnat priority.
    SuccessOrExit(error = mNftables.AddChain(kTableName, kNatPostroutingChain, Hook::kPostrouting,
                                             ChainPriority::kSrcNat, ChainType::kNat));
    SuccessOrExit(error = mNftables.AddRuleMarkMasquerade(kTableName, kNatPostroutingChain, kNat44Mark, nullptr));

    // Forward: accept traffic in either direction on the upstream interface.
    // Hooked at FORWARD/filter alongside forward_ingress; the iifname/oifname
    // matches scope each rule cleanly.
    SuccessOrExit(error = mNftables.AddChain(kTableName, kNatForwardChain, Hook::kForward, ChainPriority::kFilter,
                                             ChainType::kFilter));
    SuccessOrExit(error = mNftables.AddRuleOifnameVerdict(kTableName, kNatForwardChain, aUpstreamInterfaceName,
                                                          Verdict::kAccept, nullptr));
    SuccessOrExit(error = mNftables.AddRuleIifnameVerdict(kTableName, kNatForwardChain, aUpstreamInterfaceName,
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
        SuccessOrExit(error = mNftables.AddChain(kTableName, kPreroutingChain, Hook::kPrerouting, ChainPriority::kRaw));
    }

    if (mNdRuleHandle != 0)
    {
        SuccessOrExit(error = mNftables.DelRule(kTableName, kPreroutingChain, mNdRuleHandle));
    }

    SuccessOrExit(error = mNftables.AddRuleNdNsRedirect(kTableName, kPreroutingChain, aDomainPrefix,
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
    SuccessOrExit(error = mNftables.DelRule(kTableName, kPreroutingChain, mNdRuleHandle));
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
    SuccessOrExit(error = mNftables.AddSetElement(kTableName, SetName(aSet), aPrefix));
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
    SuccessOrExit(error = mNftables.DelSetElement(kTableName, SetName(aSet), aPrefix));
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
    SuccessOrExit(error = mNftables.FlushSet(kTableName, SetName(aSet)));
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

    SuccessOrExit(error = mNftables.BeginBatch());
    SuccessOrExit(error = mNftables.FlushSet(kTableName, kIngressDenySrcSet));
    SuccessOrExit(error = mNftables.FlushSet(kTableName, kIngressAllowDstSet));
    for (const Ip6Prefix &prefix : aDenySrc)
    {
        VerifyOrExit(prefix.IsValid(), error = OTBR_ERROR_INVALID_ARGS);
        SuccessOrExit(error = mNftables.AddSetElement(kTableName, kIngressDenySrcSet, prefix));
    }
    for (const Ip6Prefix &prefix : aAllowDst)
    {
        VerifyOrExit(prefix.IsValid(), error = OTBR_ERROR_INVALID_ARGS);
        SuccessOrExit(error = mNftables.AddSetElement(kTableName, kIngressAllowDstSet, prefix));
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
