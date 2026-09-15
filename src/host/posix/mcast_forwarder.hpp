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
 *   This file includes definitions for the userspace Backbone Router
 *   multicast forwarder.
 */

#ifndef OTBR_HOST_POSIX_MCAST_FORWARDER_HPP_
#define OTBR_HOST_POSIX_MCAST_FORWARDER_HPP_

#include "openthread-br/config.h"

#include <map>
#include <set>
#include <string>
#include <vector>

#include <openthread/backbone_router_ftd.h>

#include "common/code_utils.hpp"
#include "common/mainloop.hpp"
#include "common/time.hpp"
#include "common/types.hpp"
#include "host/posix/bpf_tap.hpp"
#include "host/posix/mcast_policy.hpp"

namespace otbr {

/**
 * Userspace multicast forwarder for a Backbone Router.
 *
 * The OpenThread posix platform forwards multicast between the Thread
 * network and the backbone through the kernel's multicast routing (MRT6),
 * which only Linux offers. This class takes that role on platforms without
 * it. It tracks the groups that have listeners on each side — the Thread
 * side from the core's Multicast Listener Registration table, the backbone
 * side by snooping MLD reports on the backbone interface — and moves
 * packets between the Thread interface and the backbone through BPF taps,
 * applying the same rules as the Linux path:
 *
 * - Thread -> backbone: every multicast packet the Thread stack hands to
 *   the host with a scope beyond realm-local is forwarded, whether or not
 *   a backbone listener is known.
 * - Backbone -> Thread: only packets to groups with a registered Thread
 *   listener are injected into the mesh, where they are flooded (MPL).
 *
 * Both directions forward each packet once (duplicate suppression), within
 * per-group and overall rate limits, with the hop limit decremented.
 */
class McastForwarder : public MainloopProcessor, private NonCopyable
{
public:
    /**
     * A group membership change carried by an MLD message.
     */
    struct MldRecord
    {
        Ip6Address mGroup;
        bool       mIsJoin;
    };

    /**
     * Forwarding statistics for one direction.
     */
    struct Counters
    {
        uint64_t mReceived    = 0; ///< Packets taken from the tap.
        uint64_t mForwarded   = 0; ///< Packets written to the other side.
        uint64_t mRejected    = 0; ///< Not forwardable (policy).
        uint64_t mNoListener  = 0; ///< No listener for the group on the other side.
        uint64_t mDuplicates  = 0; ///< Copies of a packet already forwarded.
        uint64_t mRateLimited = 0; ///< Dropped by the rate limiter.
        uint64_t mErrors      = 0; ///< Write failures.
    };

    /**
     * Constructor.
     *
     * @param[in] aThreadIfName    The Thread network interface name.
     * @param[in] aBackboneIfName  The backbone interface name; may be empty.
     */
    McastForwarder(const std::string &aThreadIfName, const std::string &aBackboneIfName);

    ~McastForwarder(void) override;

    /**
     * Handles a Backbone Router state change. Forwarding runs only while this
     * device is the Primary Backbone Router.
     */
    void HandleBackboneRouterStateChange(otBackboneRouterState aState);

    /**
     * Handles a change of the core's Multicast Listener Registration table.
     */
    void HandleBackboneMulticastListenerEvent(otBackboneRouterMulticastListenerEvent aEvent,
                                              const Ip6Address                      &aAddress);

    bool IsEnabled(void) const { return mEnabled; }
    bool HasThreadListener(const Ip6Address &aGroup) const { return mThreadListeners.count(aGroup) != 0; }
    bool HasBackboneListener(const Ip6Address &aGroup) const { return mBackboneListeners.count(aGroup) != 0; }

    const Counters &GetThreadToBackboneCounters(void) const { return mThreadToBackbone; }
    const Counters &GetBackboneToThreadCounters(void) const { return mBackboneToThread; }

    const std::string                     &GetThreadIfName(void) const { return mThreadIfName; }
    const std::string                     &GetBackboneIfName(void) const { return mBackboneIfName; }
    const std::set<Ip6Address>            &GetThreadListeners(void) const { return mThreadListeners; }
    const std::map<Ip6Address, Timepoint> &GetBackboneListeners(void) const { return mBackboneListeners; }

    /**
     * Extracts the group membership changes an MLD message carries.
     *
     * @param[in]  aPacket   An IPv6 packet, starting at the IPv6 header.
     * @param[in]  aLength   The packet length.
     * @param[out] aRecords  The membership changes, in message order.
     *
     * @returns true if the packet is a well-formed MLD report or done message, false otherwise.
     */
    static bool ParseMld(const uint8_t *aPacket, size_t aLength, std::vector<MldRecord> &aRecords);

    /**
     * Applies the membership changes of one MLD message to the backbone
     * listener table. Groups the forwarder does not track are ignored.
     */
    void HandleMldRecords(const std::vector<MldRecord> &aRecords);

    /**
     * Runs one IPv6 packet from the Thread side through the forwarding
     * pipeline (policy, duplicate suppression, rate limit) and, if it
     * passes, writes it to the backbone. Public so the pipeline can be
     * exercised without a tap; the counters record the outcome.
     *
     * @param[in] aPacket  The IPv6 packet, starting at the IPv6 header.
     * @param[in] aLength  The packet length.
     * @param[in] aNow     The current time.
     */
    void HandleThreadPacket(const uint8_t *aPacket, size_t aLength, Timepoint aNow);

    /**
     * Runs one IPv6 packet from the backbone through the forwarding pipeline
     * (policy, Thread listener check, duplicate suppression, rate limit)
     * and, if it passes, injects it into the Thread network. Public for the
     * same reason as HandleThreadPacket().
     */
    void HandleBackbonePacket(const uint8_t *aPacket, size_t aLength, Timepoint aNow);

    /**
     * Indicates whether a group is one the forwarder tracks: multicast with a
     * scope beyond link-local, the only scopes a Backbone Router carries.
     */
    static bool IsTrackedGroup(const Ip6Address &aGroup);

    void Update(MainloopContext &aMainloop) override;
    void Process(const MainloopContext &aMainloop) override;

private:
    // RFC 3810 §9.4: Multicast Listener Interval = Robustness Variable (2) *
    // Query Interval (125 s) + Query Response Interval (10 s).
    static constexpr uint32_t kBackboneListenerTimeoutSec = 260;
    static constexpr uint32_t kExpireIntervalSec          = 30;
    static constexpr uint32_t kReportIntervalSec          = 60;
    static constexpr uint32_t kDedupWindowMs              = 5000;
    static constexpr size_t   kDedupEntries               = 512;
    static constexpr size_t   kMaxFrameSize               = 1514;

    void Enable(void);
    void Disable(void);
    void OpenBackboneMldTap(void);
    void OpenBackboneDataTap(void);
    void OpenThreadTap(void);
    void HandleMldFrame(const uint8_t *aFrame, size_t aLength);
    void HandleBackboneDataFrame(const uint8_t *aFrame, size_t aLength);
    void HandleThreadFrame(const uint8_t *aFrame, size_t aLength);
    void ExpireBackboneListeners(void);
    void ReportCounters(bool aForce);
    void ReportCounters(const char *aDirection, const Counters &aCounters, Counters &aReported, bool aForce);
    bool ReadBackboneMac(void);

    std::string                     mThreadIfName;
    std::string                     mBackboneIfName;
    bool                            mEnabled;
    BpfTap                          mBackboneMldTap;
    BpfTap                          mBackboneDataTap;
    BpfTap                          mThreadTap;
    uint8_t                         mBackboneMac[McastForwardPolicy::kMacSize];
    bool                            mHasBackboneMac;
    std::set<Ip6Address>            mThreadListeners;
    std::map<Ip6Address, Timepoint> mBackboneListeners;
    McastDedupCache                 mDedup;
    McastRateLimiter                mThreadToBackboneLimiter;
    McastRateLimiter                mBackboneToThreadLimiter;
    Counters                        mThreadToBackbone;
    Counters                        mBackboneToThread;
    Counters                        mReportedThreadToBackbone;
    Counters                        mReportedBackboneToThread;
    Timepoint                       mNextExpire;
    Timepoint                       mNextReport;
};

} // namespace otbr

#endif // OTBR_HOST_POSIX_MCAST_FORWARDER_HPP_
