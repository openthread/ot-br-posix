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
 *   This file declares an abstract interface to the nftables subsystem.
 *
 *   The interface intentionally exposes a small set of OTBR-specific rule
 *   shapes rather than a generic "build any rule" API. This keeps the policy
 *   layer (FirewallManager) free of libnftnl details and keeps the surface
 *   area mockable for unit tests.
 */

#ifndef OTBR_FIREWALL_NFTABLES_HPP_
#define OTBR_FIREWALL_NFTABLES_HPP_

#include "openthread-br/config.h"

#include <stdint.h>
#include <string>

#include "common/types.hpp"

namespace otbr {
namespace Firewall {

enum class Hook
{
    kForward,
    kPrerouting,
    kPostrouting,
};

enum class ChainType
{
    kFilter, ///< Default filter chain.
    kNat,    ///< NAT chain — required for masquerade / dnat / snat expressions.
};

enum class Verdict
{
    kAccept,
    kDrop,
    kReturn,
};

enum class SetDirection
{
    kSrc,
    kDst,
};

enum class PktType
{
    kUnicast,
};

/**
 * Standard nftables chain priorities (see linux/netfilter_ipv4.h /
 * libnftnl docs). Listed here so callers don't have to pull kernel
 * headers.
 */
enum ChainPriority
{
    kPriorityRaw     = -300,
    kPriorityMangle  = -150,
    kPriorityFilter  = 0,
    kPrioritySrcNat  = 100,
};

/**
 * An IPv6 prefix in wire format. Used for set elements and for the
 * domain-prefix match in the ND-proxy redirect rule.
 */
struct Ip6Net
{
    uint8_t mAddr[16];
    uint8_t mPrefixLen;
};

/**
 * Abstract interface to the nftables subsystem.
 *
 * The concrete implementation (Nftables in nftables_impl.hpp) talks to the
 * kernel via libnftnl/libmnl. Tests substitute a gmock.
 *
 * Rule-creation methods return a kernel-assigned handle through @p aHandleOut
 * so the rule can be deleted later with DelRule(). Set elements are addressed
 * by value, not handle.
 *
 * Mutating operations must be wrapped in BeginBatch()/CommitBatch() so the
 * kernel applies them atomically. DelTable() is the only operation safe to
 * invoke outside a batch and is idempotent (treats ENOENT as success).
 */
class INftables
{
public:
    virtual ~INftables(void) = default;

    /**
     * Open the underlying mnl socket. Must be called before any other method.
     */
    virtual otbrError Init(void) = 0;

    /**
     * Close the underlying mnl socket. Does not delete any kernel state;
     * pair with DelTable() if you want a clean slate.
     */
    virtual otbrError Deinit(void) = 0;

    virtual otbrError BeginBatch(void)  = 0;
    virtual otbrError CommitBatch(void) = 0;

    /**
     * Idempotent: returns OTBR_ERROR_NONE if the table did not exist.
     * Cascades: deleting a table also deletes all its chains, sets, and rules.
     */
    virtual otbrError DelTable(const std::string &aTable) = 0;

    virtual otbrError AddTable(const std::string &aTable) = 0;

    /**
     * Create a base chain with the given netfilter hook, priority, and type.
     * NAT operations (masquerade/snat/dnat) require @p aType == kNat.
     */
    virtual otbrError AddChain(const std::string &aTable,
                               const std::string &aChain,
                               Hook               aHook,
                               int                aPriority,
                               ChainType          aType = ChainType::kFilter) = 0;

    /**
     * Create a named set of `ipv6_addr` with the `interval` flag,
     * giving prefix-match semantics equivalent to `ipset hash:net family inet6`.
     */
    virtual otbrError AddIp6PrefixSet(const std::string &aTable, const std::string &aSet) = 0;

    virtual otbrError AddSetElement(const std::string &aTable, const std::string &aSet, const Ip6Net &aPrefix) = 0;
    virtual otbrError DelSetElement(const std::string &aTable, const std::string &aSet, const Ip6Net &aPrefix) = 0;
    virtual otbrError FlushSet(const std::string &aTable, const std::string &aSet)                            = 0;

    /**
     * `meta oifname != <ifname> return` — used as the first rule of an
     * inet/forward base chain to bail out on traffic not destined for the
     * Thread interface.
     */
    virtual otbrError AddRuleOifnameNeqReturn(const std::string &aTable,
                                              const std::string &aChain,
                                              const std::string &aOifname,
                                              uint64_t          *aHandleOut) = 0;

    /**
     * `iifname <ifname> meta pkttype <pkttype> <verdict>`
     */
    virtual otbrError AddRuleIifPkttypeVerdict(const std::string &aTable,
                                               const std::string &aChain,
                                               const std::string &aIifname,
                                               PktType            aPktType,
                                               Verdict            aVerdict,
                                               uint64_t          *aHandleOut) = 0;

    /**
     * `meta pkttype <pkttype> <verdict>`
     */
    virtual otbrError AddRulePkttypeVerdict(const std::string &aTable,
                                            const std::string &aChain,
                                            PktType            aPktType,
                                            Verdict            aVerdict,
                                            uint64_t          *aHandleOut) = 0;

    /**
     * `ip6 [s|d]addr @<set> <verdict>`
     */
    virtual otbrError AddRuleSetLookupVerdict(const std::string &aTable,
                                              const std::string &aChain,
                                              const std::string &aSet,
                                              SetDirection       aDir,
                                              Verdict            aVerdict,
                                              uint64_t          *aHandleOut) = 0;

    /**
     * `<verdict>` — unconditional verdict, typically used as the chain tail.
     */
    virtual otbrError AddRuleVerdict(const std::string &aTable,
                                     const std::string &aChain,
                                     Verdict            aVerdict,
                                     uint64_t          *aHandleOut) = 0;

    /**
     * `ip6 daddr <prefix> icmpv6 type nd-neighbor-solicit iifname <ifname> queue num <q> bypass`
     *
     * Installed in a prerouting chain (raw priority) to redirect unicast
     * neighbor solicitations destined for the Thread Domain prefix into an
     * NFQUEUE handled by the ND-proxy.
     */
    virtual otbrError AddRuleNdNsRedirect(const std::string &aTable,
                                          const std::string &aChain,
                                          const Ip6Net      &aDaddrPrefix,
                                          const std::string &aIifname,
                                          uint16_t           aQueueNum,
                                          uint64_t          *aHandleOut) = 0;

    /**
     * `iifname <ifname> meta mark set <mark>` — used in mangle-prerouting to
     * tag packets coming from the Thread interface for downstream NAT.
     */
    virtual otbrError AddRuleIifMark(const std::string &aTable,
                                     const std::string &aChain,
                                     const std::string &aIifname,
                                     uint32_t           aMark,
                                     uint64_t          *aHandleOut) = 0;

    /**
     * `meta mark <mark> masquerade` — used in srcnat-postrouting to source-NAT
     * marked packets out the upstream interface.
     */
    virtual otbrError AddRuleMarkMasquerade(const std::string &aTable,
                                            const std::string &aChain,
                                            uint32_t           aMark,
                                            uint64_t          *aHandleOut) = 0;

    /**
     * `oifname <ifname> <verdict>` — used in NAT forward to allow traffic out
     * the upstream interface.
     */
    virtual otbrError AddRuleOifnameVerdict(const std::string &aTable,
                                            const std::string &aChain,
                                            const std::string &aOifname,
                                            Verdict            aVerdict,
                                            uint64_t          *aHandleOut) = 0;

    /**
     * `iifname <ifname> <verdict>` — used in NAT forward to allow return
     * traffic from the upstream interface.
     */
    virtual otbrError AddRuleIifnameVerdict(const std::string &aTable,
                                            const std::string &aChain,
                                            const std::string &aIifname,
                                            Verdict            aVerdict,
                                            uint64_t          *aHandleOut) = 0;

    virtual otbrError DelRule(const std::string &aTable, const std::string &aChain, uint64_t aHandle) = 0;
};

} // namespace Firewall
} // namespace otbr

#endif // OTBR_FIREWALL_NFTABLES_HPP_
