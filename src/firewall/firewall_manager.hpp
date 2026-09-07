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
#include <vector>

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
     * Install IPv4 masquerade NAT44 for traffic from the Thread interface
     * out the upstream interface, plus FORWARD ACCEPTs in both directions.
     * Replaces the legacy iptables-based NAT64 setup that used to live in
     * the Docker entrypoint. No-op if already enabled.
     *
     * @param[in] aUpstreamInterfaceName  Upstream interface (e.g. "eth0").
     */
    otbrError EnableNat44Masquerade(const std::string &aUpstreamInterfaceName);

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

    /**
     * Atomically replace the full contents of both ingress sets in a single
     * nftables transaction: flush ingress_deny_src and ingress_allow_dst, then
     * add the given prefixes. This is the ingress-prefix producer path, driven
     * by otbr-agent on Thread network-data changes; doing it in one batch means
     * there is never a window where the sets are half-populated.
     *
     * @param[in] aDenySrc   Prefixes for the deny-source set (on-mesh + mesh-local).
     * @param[in] aAllowDst  Prefixes for the allow-destination set (on-mesh).
     */
    otbrError ReplaceIngressPrefixes(const std::vector<Ip6Prefix> &aDenySrc, const std::vector<Ip6Prefix> &aAllowDst);

    bool IsInitialized(void) const { return mInitialized; }
    bool IsIngressFilterEnabled(void) const { return mIngressFilterEnabled; }
    bool IsNat44Enabled(void) const { return mNat44Enabled; }

    // Plain const rather than constexpr: under C++17 a static constexpr
    // member is implicitly inline, and its weak per-TU definitions collide
    // with the C++11 out-of-line ones when parts of a build (a C++17 gtest,
    // say) compile this header under a newer standard. A single definition
    // in the .cpp means the same thing in every standard.
    static const char *const kTableName;
    static const char *const kIngressChain;
    static const char *const kPreroutingChain;
    static const char *const kNatPreroutingChain;
    static const char *const kNatPostroutingChain;
    static const char *const kNatForwardChain;
    static const char *const kIngressDenySrcSet;
    static const char *const kIngressAllowDstSet;

    /// Mark used to tag NAT44'd traffic from the Thread interface.
    static constexpr uint32_t kNat44Mark = 0x1001;

private:
    static const char *SetName(IngressSet aSet);

    INftables  &mNftables;
    std::string mThreadIfName;
    bool        mInitialized;
    bool        mIngressFilterEnabled;
    bool        mNat44Enabled;
    bool        mDuaChainCreated; ///< True once the dua_prerouting chain has been created.
    uint64_t    mNdRuleHandle;    ///< Kernel handle of the active ND-proxy rule, 0 if none.
};

} // namespace Firewall
} // namespace otbr

#endif // OTBR_FIREWALL_FIREWALL_MANAGER_HPP_
