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

    // Idempotent reset: drop any leftover OTBR table from a prior run.
    SuccessOrExit(error = mNftables.DelTable(kTableName));

    SuccessOrExit(error = mNftables.BeginBatch());
    SuccessOrExit(error = mNftables.AddTable(kTableName));
    SuccessOrExit(error = mNftables.CommitBatch());

    mInitialized = true;

exit:
    otbrLogResult(error, "FirewallManager: %s", __FUNCTION__);
    return error;
}

otbrError FirewallManager::Deinit(void)
{
    otbrError error = OTBR_ERROR_NONE;

    VerifyOrExit(mInitialized);

    error                 = mNftables.DelTable(kTableName);
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
    otbrError error  = OTBR_ERROR_NONE;
    uint64_t  handle = 0;

    VerifyOrExit(mInitialized, error = OTBR_ERROR_INVALID_STATE);
    VerifyOrExit(!mIngressFilterEnabled);

    SuccessOrExit(error = mNftables.BeginBatch());

    SuccessOrExit(error = mNftables.AddIp6PrefixSet(kTableName, kIngressDenySrcSet));
    SuccessOrExit(error = mNftables.AddIp6PrefixSet(kTableName, kIngressAllowDstSet));

    SuccessOrExit(error = mNftables.AddChain(kTableName, kIngressChain, Hook::kForward, kPriorityFilter));

    SuccessOrExit(error = mNftables.AddRuleOifnameNeqReturn(kTableName, kIngressChain, mThreadIfName, &handle));
    SuccessOrExit(error = mNftables.AddRuleIifPkttypeVerdict(kTableName,
                                                             kIngressChain,
                                                             mThreadIfName,
                                                             PktType::kUnicast,
                                                             Verdict::kDrop,
                                                             &handle));
    SuccessOrExit(error = mNftables.AddRuleSetLookupVerdict(kTableName,
                                                             kIngressChain,
                                                             kIngressDenySrcSet,
                                                             SetDirection::kSrc,
                                                             Verdict::kDrop,
                                                             &handle));
    SuccessOrExit(error = mNftables.AddRuleSetLookupVerdict(kTableName,
                                                             kIngressChain,
                                                             kIngressAllowDstSet,
                                                             SetDirection::kDst,
                                                             Verdict::kAccept,
                                                             &handle));
    SuccessOrExit(error = mNftables.AddRulePkttypeVerdict(kTableName,
                                                           kIngressChain,
                                                           PktType::kUnicast,
                                                           Verdict::kDrop,
                                                           &handle));
    SuccessOrExit(error = mNftables.AddRuleVerdict(kTableName, kIngressChain, Verdict::kAccept, &handle));

    SuccessOrExit(error = mNftables.CommitBatch());

    mIngressFilterEnabled = true;

exit:
    otbrLogResult(error, "FirewallManager: %s", __FUNCTION__);
    return error;
}

otbrError FirewallManager::EnableNat44Masquerade(const std::string &aUpstreamInterfaceName)
{
    otbrError error  = OTBR_ERROR_NONE;
    uint64_t  handle = 0;

    VerifyOrExit(mInitialized, error = OTBR_ERROR_INVALID_STATE);
    VerifyOrExit(!mNat44Enabled);

    SuccessOrExit(error = mNftables.BeginBatch());

    // Mangle-prerouting: tag packets coming from the Thread interface so the
    // postrouting chain can MASQUERADE them. Type filter, mangle priority.
    SuccessOrExit(error = mNftables.AddChain(kTableName,
                                             kNatPreroutingChain,
                                             Hook::kPrerouting,
                                             kPriorityMangle,
                                             ChainType::kFilter));
    SuccessOrExit(error = mNftables.AddRuleIifMark(kTableName,
                                                   kNatPreroutingChain,
                                                   mThreadIfName,
                                                   kNat44Mark,
                                                   &handle));

    // Postrouting: source-NAT marked traffic. Type nat, srcnat priority.
    SuccessOrExit(error = mNftables.AddChain(kTableName,
                                             kNatPostroutingChain,
                                             Hook::kPostrouting,
                                             kPrioritySrcNat,
                                             ChainType::kNat));
    SuccessOrExit(error = mNftables.AddRuleMarkMasquerade(kTableName, kNatPostroutingChain, kNat44Mark, &handle));

    // Forward: accept traffic in either direction on the upstream interface.
    // Hooked at FORWARD/filter alongside forward_ingress; the iifname/oifname
    // matches scope each rule cleanly.
    SuccessOrExit(error = mNftables.AddChain(kTableName,
                                             kNatForwardChain,
                                             Hook::kForward,
                                             kPriorityFilter,
                                             ChainType::kFilter));
    SuccessOrExit(error = mNftables.AddRuleOifnameVerdict(kTableName,
                                                          kNatForwardChain,
                                                          aUpstreamInterfaceName,
                                                          Verdict::kAccept,
                                                          &handle));
    SuccessOrExit(error = mNftables.AddRuleIifnameVerdict(kTableName,
                                                          kNatForwardChain,
                                                          aUpstreamInterfaceName,
                                                          Verdict::kAccept,
                                                          &handle));

    SuccessOrExit(error = mNftables.CommitBatch());

    mNat44Enabled = true;

exit:
    otbrLogResult(error, "FirewallManager: %s", __FUNCTION__);
    return error;
}

otbrError FirewallManager::EnableNdProxyRedirect(const Ip6Prefix   &aDomainPrefix,
                                                 const std::string &aBackboneInterfaceName,
                                                 uint16_t           aQueueNum)
{
    otbrError error     = OTBR_ERROR_NONE;
    uint64_t  newHandle = 0;
    Ip6Net    daddr     = ToIp6Net(aDomainPrefix);

    VerifyOrExit(mInitialized, error = OTBR_ERROR_INVALID_STATE);
    VerifyOrExit(aDomainPrefix.IsValid(), error = OTBR_ERROR_INVALID_ARGS);

    SuccessOrExit(error = mNftables.BeginBatch());

    if (!mDuaChainCreated)
    {
        SuccessOrExit(error = mNftables.AddChain(kTableName, kPreroutingChain, Hook::kPrerouting, kPriorityRaw));
    }

    if (mNdRuleHandle != 0)
    {
        SuccessOrExit(error = mNftables.DelRule(kTableName, kPreroutingChain, mNdRuleHandle));
        mNdRuleHandle = 0;
    }

    SuccessOrExit(error = mNftables.AddRuleNdNsRedirect(kTableName,
                                                        kPreroutingChain,
                                                        daddr,
                                                        aBackboneInterfaceName,
                                                        aQueueNum,
                                                        &newHandle));

    SuccessOrExit(error = mNftables.CommitBatch());

    mDuaChainCreated = true;
    mNdRuleHandle    = newHandle;

exit:
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
    otbrLogResult(error, "FirewallManager: %s", __FUNCTION__);
    return error;
}

otbrError FirewallManager::AddIngressSetElement(IngressSet aSet, const Ip6Prefix &aPrefix)
{
    otbrError error = OTBR_ERROR_NONE;
    Ip6Net    net   = ToIp6Net(aPrefix);

    VerifyOrExit(mIngressFilterEnabled, error = OTBR_ERROR_INVALID_STATE);
    VerifyOrExit(aPrefix.IsValid(), error = OTBR_ERROR_INVALID_ARGS);

    error = mNftables.AddSetElement(kTableName, SetName(aSet), net);

exit:
    return error;
}

otbrError FirewallManager::DelIngressSetElement(IngressSet aSet, const Ip6Prefix &aPrefix)
{
    otbrError error = OTBR_ERROR_NONE;
    Ip6Net    net   = ToIp6Net(aPrefix);

    VerifyOrExit(mIngressFilterEnabled, error = OTBR_ERROR_INVALID_STATE);
    VerifyOrExit(aPrefix.IsValid(), error = OTBR_ERROR_INVALID_ARGS);

    error = mNftables.DelSetElement(kTableName, SetName(aSet), net);

exit:
    return error;
}

otbrError FirewallManager::FlushIngressSet(IngressSet aSet)
{
    otbrError error = OTBR_ERROR_NONE;

    VerifyOrExit(mIngressFilterEnabled, error = OTBR_ERROR_INVALID_STATE);

    error = mNftables.FlushSet(kTableName, SetName(aSet));

exit:
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

Ip6Net FirewallManager::ToIp6Net(const Ip6Prefix &aPrefix)
{
    Ip6Net net;
    memcpy(net.mAddr, aPrefix.mPrefix.m8, sizeof(net.mAddr));
    net.mPrefixLen = aPrefix.mLength;
    return net;
}

} // namespace Firewall
} // namespace otbr
