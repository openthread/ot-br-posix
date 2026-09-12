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
 *   This file includes definitions for the forwarding policy pieces of the
 *   userspace multicast forwarder: what may be forwarded, duplicate
 *   suppression and rate limiting.
 */

#ifndef OTBR_HOST_POSIX_MCAST_POLICY_HPP_
#define OTBR_HOST_POSIX_MCAST_POLICY_HPP_

#include "openthread-br/config.h"

#include <list>
#include <map>
#include <unordered_map>
#include <utility>

#include <stddef.h>
#include <stdint.h>

#include "common/time.hpp"
#include "common/types.hpp"

namespace otbr {

/**
 * Decides whether an IPv6 packet is one a Backbone Router forwards between
 * the Thread network and the backbone, and prepares it for the other side.
 */
class McastForwardPolicy
{
public:
    static constexpr size_t kIp6HeaderSize      = 40;
    static constexpr size_t kEthernetHeaderSize = 14;
    static constexpr size_t kMacSize            = 6;

    enum Verdict : uint8_t
    {
        kForward,          ///< The packet may be forwarded.
        kNotIp6,           ///< Not a well-formed IPv6 packet.
        kNotMulticast,     ///< Unicast destination.
        kScopeTooSmall,    ///< Multicast scope realm-local or smaller; never crosses a Backbone Router.
        kBadSource,        ///< Multicast, unspecified or link-local source.
        kHopLimitExceeded, ///< Hop limit 1 or less: forwarding would expire it.
    };

    /**
     * Checks whether an IPv6 packet may be forwarded to the other side.
     *
     * @param[in] aPacket  The IPv6 packet, starting at the IPv6 header.
     * @param[in] aLength  Its length.
     */
    static Verdict Check(const uint8_t *aPacket, size_t aLength);

    /**
     * Decrements the hop limit, as a router forwarding the packet would.
     */
    static void DecrementHopLimit(uint8_t *aPacket);

    /**
     * Wraps an IPv6 multicast packet in an Ethernet frame for the backbone:
     * the group's multicast MAC as destination, @p aSourceMac as source.
     *
     * @returns The frame length, or 0 if @p aFrame is too small.
     */
    static size_t BuildEthernetFrame(const uint8_t *aPacket,
                                     size_t         aLength,
                                     const uint8_t *aSourceMac,
                                     uint8_t       *aFrame,
                                     size_t         aFrameCapacity);

    static Ip6Address GetDestination(const uint8_t *aPacket);
    static Ip6Address GetSource(const uint8_t *aPacket);

    static const char *VerdictToString(Verdict aVerdict);
};

/**
 * Remembers recently forwarded packets so a copy that comes back — through
 * another Backbone Router, a looped switch port, or the mesh flooding it a
 * second time — is not forwarded again.
 */
class McastDedupCache
{
public:
    /**
     * @param[in] aWindowMs    How long a packet is remembered.
     * @param[in] aMaxEntries  Upper bound on remembered packets; the oldest go first.
     */
    McastDedupCache(uint32_t aWindowMs, size_t aMaxEntries);

    /**
     * Fingerprints an IPv6 packet. The hop limit is left out so a copy that
     * travelled a different path still matches.
     */
    static uint64_t Fingerprint(const uint8_t *aPacket, size_t aLength);

    /**
     * Records a packet and reports whether it was already known.
     *
     * @returns true if the packet was seen within the window, false if it is new (and is now recorded).
     */
    bool Check(uint64_t aFingerprint, Timepoint aNow);

    size_t GetSize(void) const { return mSeen.size(); }

private:
    using Entry = std::pair<uint64_t, Timepoint>;
    using Order = std::list<Entry>; // least recently seen first

    uint32_t                                      mWindowMs;
    size_t                                        mMaxEntries;
    Order                                         mOrder;
    std::unordered_map<uint64_t, Order::iterator> mSeen;
};

/**
 * A token bucket: @p aRatePerSecond tokens arrive per second, at most
 * @p aBurst accumulate.
 */
class TokenBucket
{
public:
    TokenBucket(uint32_t aRatePerSecond, uint32_t aBurst);

    /**
     * Takes one token if available.
     *
     * @returns true if a token was taken, false if the bucket is empty.
     */
    bool Take(Timepoint aNow);

private:
    uint32_t  mRatePerSecond;
    uint32_t  mBurst;
    double    mTokens;
    Timepoint mLastRefill;
};

/**
 * Rate limits forwarded multicast per group and overall, so one chatty
 * group or a storm cannot saturate either side.
 */
class McastRateLimiter
{
public:
    struct Config
    {
        uint32_t mPerGroupRatePerSecond;
        uint32_t mPerGroupBurst;
        uint32_t mTotalRatePerSecond;
        uint32_t mTotalBurst;
        size_t   mMaxGroups; ///< Groups tracked at once; an idle group's bucket is dropped for a new one.
    };

    explicit McastRateLimiter(const Config &aConfig);

    /**
     * Accounts one packet for @p aGroup.
     *
     * @returns true if the packet is within both limits, false if it must be dropped.
     */
    bool Allow(const Ip6Address &aGroup, Timepoint aNow);

private:
    struct GroupState
    {
        GroupState(const Config &aConfig, Timepoint aNow)
            : mBucket(aConfig.mPerGroupRatePerSecond, aConfig.mPerGroupBurst)
            , mLastUse(aNow)
        {
        }

        TokenBucket mBucket;
        Timepoint   mLastUse;
    };

    Config                           mConfig;
    TokenBucket                      mTotal;
    std::map<Ip6Address, GroupState> mGroups;
};

} // namespace otbr

#endif // OTBR_HOST_POSIX_MCAST_POLICY_HPP_
