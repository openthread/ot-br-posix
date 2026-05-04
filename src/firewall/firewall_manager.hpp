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

/**
 * @file
 *   FirewallManager owns the OTBR-specific firewall policy and translates it
 *   into nftables primitives via INftables. It replaces the iptables/ipset
 *   shell scripts and the ip6tables call in nd_proxy.cpp.
 */

#ifndef OTBR_FIREWALL_FIREWALL_MANAGER_HPP_
#define OTBR_FIREWALL_FIREWALL_MANAGER_HPP_

#include "openthread-br/config.h"

#include <stdint.h>
#include <string>

#include "common/code_utils.hpp"
#include "common/types.hpp"
#include "firewall/nftables.hpp"

namespace otbr {
namespace Firewall {

class FirewallManager : private NonCopyable
{
public:
    /**
     * @param[in] aNftables  Backend implementation. Must outlive this object.
     *                       Caller is responsible for Init()/Deinit() of the
     *                       backend; FirewallManager only uses it.
     * @param[in] aThreadInterfaceName  Interface name (e.g. "wpan0") used to
     *                                  scope ingress filtering rules.
     */
    FirewallManager(INftables &aNftables, std::string aThreadInterfaceName);

    /**
     * Create the OTBR table. Idempotent: tears down any pre-existing OTBR
     * table first. The ingress filter chain (forward_ingress) and the
     * prerouting chain (dua_prerouting) are NOT created here — they are
     * created on demand by EnableIngressFilter() and EnableNdProxyRedirect()
     * respectively, so a phase that doesn't need them doesn't pay for them.
     */
    otbrError Init(void);

    /**
     * Tear down the OTBR table (cascades chains/sets/rules).
     */
    otbrError Deinit(void);

    /**
     * Install the static ingress filter chain (forward_ingress) and the
     * named sets (ingress_deny_src, ingress_allow_dst). After this returns,
     * AddIngressSetElement / DelIngressSetElement / FlushIngressSet are
     * available. No-op if already enabled.
     */
    otbrError EnableIngressFilter(void);

    /**
     * Install the ND-proxy NFQUEUE redirect rule for the given Domain prefix
     * on the given backbone interface. Lazily creates the dua_prerouting
     * chain on first call. Subsequent calls without an intervening Disable
     * replace the previous rule.
     */
    otbrError EnableNdProxyRedirect(const Ip6Prefix   &aDomainPrefix,
                                    const std::string &aBackboneInterfaceName,
                                    uint16_t           aQueueNum);

    /**
     * Remove the ND-proxy NFQUEUE redirect rule. No-op if not currently enabled.
     */
    otbrError DisableNdProxyRedirect(void);

    enum class IngressSet
    {
        kDenySrc,  ///< Source addresses denied on Thread-bound traffic.
        kAllowDst, ///< Destination addresses allowed on Thread-bound traffic.
    };

    otbrError AddIngressSetElement(IngressSet aSet, const Ip6Prefix &aPrefix);
    otbrError DelIngressSetElement(IngressSet aSet, const Ip6Prefix &aPrefix);
    otbrError FlushIngressSet(IngressSet aSet);

    bool IsInitialized(void) const { return mInitialized; }
    bool IsIngressFilterEnabled(void) const { return mIngressFilterEnabled; }

    static constexpr const char *kTableName            = "otbr";
    static constexpr const char *kIngressChain         = "forward_ingress";
    static constexpr const char *kPreroutingChain      = "dua_prerouting";
    static constexpr const char *kIngressDenySrcSet    = "ingress_deny_src";
    static constexpr const char *kIngressAllowDstSet   = "ingress_allow_dst";

private:
    static const char *SetName(IngressSet aSet);
    static Ip6Net      ToIp6Net(const Ip6Prefix &aPrefix);

    INftables  &mNftables;
    std::string mThreadIfName;
    bool        mInitialized;
    bool        mIngressFilterEnabled;
    bool        mDuaChainCreated; ///< True once the dua_prerouting chain has been created.
    uint64_t    mNdRuleHandle;    ///< Kernel handle of the active ND-proxy rule, 0 if none.
};

} // namespace Firewall
} // namespace otbr

#endif // OTBR_FIREWALL_FIREWALL_MANAGER_HPP_
