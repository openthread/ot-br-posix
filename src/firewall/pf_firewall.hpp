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
 *   PfFirewall owns the OTBR firewall policy on macOS and installs it into a
 *   pf anchor through pfctl(8). It is the pf counterpart of FirewallManager
 *   (the nftables backend) and exposes the same surface to the agent.
 */

#ifndef OTBR_FIREWALL_PF_FIREWALL_HPP_
#define OTBR_FIREWALL_PF_FIREWALL_HPP_

#include "openthread-br/config.h"

#include <string>
#include <vector>

#include "common/code_utils.hpp"
#include "common/types.hpp"
#include "firewall/pfctl.hpp"

namespace otbr {
namespace Firewall {

/**
 * The Thread ingress filter (and NAT44 masquerade) as a pf anchor.
 *
 * Everything lives in the anchor named `kAnchorName`. pf only evaluates an
 * anchor the main ruleset references, so /etc/pf.conf must carry
 * `anchor "otbr"` (and `nat-anchor "otbr"` for NAT44); script/_firewall adds
 * them. Init() refuses to proceed without the reference, so a build that
 * includes the firewall never runs unfiltered.
 *
 * The ruleset is regenerated and reloaded atomically whenever a feature is
 * enabled. The two ingress prefix tables are declared `persist`, so a reload
 * keeps their contents, and each is replaced atomically (`-T replace`) as
 * Thread network data changes.
 *
 * pf has no event for a change made behind the agent's back, so the
 * application asks VerifyInstalled() periodically: it enables pf again after a
 * `pfctl -d`, reports a main ruleset that dropped the anchor reference (a
 * `pfctl -F all`, a reload of /etc/pf.conf without the anchor lines), and puts
 * the anchor's contents back after a `pfctl -a otbr -F all` or a reload by a
 * tool that flushes the anchors.
 */
class PfFirewall : private NonCopyable
{
public:
    /**
     * @param[in] aPfctl                Runs pfctl. Must outlive this object.
     * @param[in] aThreadInterfaceName  The Thread interface (e.g. "utun5") the
     *                                  rules are scoped to. On macOS the kernel
     *                                  assigns it, so pass the name the platform
     *                                  reports, not the one that was requested.
     */
    PfFirewall(IPfctl &aPfctl, std::string aThreadInterfaceName);

    /**
     * Takes a pf enable reference (`pfctl -E`), verifies the main ruleset
     * references the anchor, and flushes any stale anchor contents.
     */
    otbrError Init(void);

    /**
     * Flushes the anchor and releases the pf enable reference.
     */
    otbrError Deinit(void);

    /**
     * Installs the Thread ingress filter rules. No-op if already enabled.
     */
    otbrError EnableIngressFilter(void);

    /**
     * Masquerades IPv4 traffic that arrived on the Thread interface out the
     * upstream interface (the NAT64 egress path). Requires `nat-anchor "otbr"`
     * in the main ruleset. No-op if already enabled.
     *
     * @param[in] aUpstreamInterfaceName  Upstream interface (e.g. "en0").
     */
    otbrError EnableNat44Masquerade(const std::string &aUpstreamInterfaceName);

    /**
     * Removes the masquerade again. No-op if not enabled.
     */
    otbrError DisableNat44Masquerade(void);

    /**
     * Replaces the contents of both ingress tables. Each table is replaced
     * atomically; pfctl cannot batch the two, so the window between them is
     * one process run.
     *
     * @param[in] aDenySrc   Prefixes for the deny-source table (on-mesh + mesh-local).
     * @param[in] aAllowDst  Prefixes for the allow-destination table (on-mesh).
     */
    otbrError ReplaceIngressPrefixes(const std::vector<Ip6Prefix> &aDenySrc, const std::vector<Ip6Prefix> &aAllowDst);

    /**
     * Checks that the firewall is still in place and repairs what it can. pf
     * disabled behind the agent's back (`pfctl -d`) is enabled again. The anchor
     * emptied behind its back (`pfctl -a otbr -F all`, a reload of /etc/pf.conf
     * by a tool that flushes the anchors) is reinstalled: the ruleset for the
     * enabled features and the last contents of the two ingress tables. The
     * nftables backend learns of its deleted table from the kernel; this is the
     * polling equivalent. A listing that fails is logged and ignored.
     *
     * A main ruleset that no longer references the anchor (`pfctl -F all`
     * flushes the main ruleset's rules; the anchor's own stay, unevaluated) is
     * an error, as at Init(): the agent cannot put the reference back and must
     * not run with a filter that is not applied. A failed reinstall or a
     * failed `pfctl -E` is an error too.
     *
     * @param[out] aReinstalled  True when the anchor had to be reinstalled.
     */
    otbrError VerifyInstalled(bool &aReinstalled);

    /**
     * Reinstalls everything that was asked for: the ruleset for the enabled
     * features and the last contents of the ingress tables.
     */
    otbrError Reinstall(void);

    bool IsInitialized(void) const { return mInitialized; }
    bool IsIngressFilterEnabled(void) const { return mIngressFilterEnabled; }
    bool IsNat44Enabled(void) const { return mNat44Enabled; }

    /**
     * Renders the anchor ruleset for the currently enabled features, in the
     * order pf requires (tables, translation, filtering).
     */
    std::string GenerateRuleset(void) const;

    static const char *const kAnchorName;
    static const char *const kIngressDenySrcTable;
    static const char *const kIngressAllowDstTable;
    static const char *const kThreadTag;

private:
    otbrError RunPfctl(const std::vector<std::string> &aArgs,
                       const std::string              &aInput,
                       std::string                    &aOutput,
                       std::string                    *aStderr = nullptr);
    otbrError EnablePf(void);
    void      DisablePf(void);
    otbrError VerifyPfEnabled(void);
    otbrError VerifyAnchorReferenced(const char *aListFlag, const char *aDirective);
    otbrError FlushAnchor(void);
    otbrError LoadRuleset(void);
    otbrError ReplaceTable(const char *aTable, const std::vector<Ip6Prefix> &aPrefixes, bool &aTableWasMissing);
    otbrError ListAnchor(const char *aListFlag, std::string &aOutput);

    static bool ListingHasLine(const std::string &aListing, const std::string &aLine);

    IPfctl     &mPfctl;
    std::string mThreadIfName;
    std::string mUpstreamIfName;
    std::string mEnableToken; ///< Reference token from `pfctl -E`, released by Deinit().
    bool        mInitialized;
    bool        mIngressFilterEnabled;
    bool        mNat44Enabled;
    // What the tables were last asked to hold, for Reinstall().
    std::vector<Ip6Prefix> mDenySrc;
    std::vector<Ip6Prefix> mAllowDst;
};

} // namespace Firewall
} // namespace otbr

#endif // OTBR_FIREWALL_PF_FIREWALL_HPP_
