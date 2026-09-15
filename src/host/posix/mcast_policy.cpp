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

#include "host/posix/mcast_policy.hpp"

#include <string.h>

#include "common/code_utils.hpp"

namespace otbr {

namespace {

constexpr size_t  kHopLimitOffset    = 7;
constexpr size_t  kSourceOffset      = 8;
constexpr size_t  kDestinationOffset = 24;
constexpr uint8_t kMinForwardedScope = Ip6Address::kAdminLocalScope;
constexpr uint8_t kMinHopLimit       = 2;

} // namespace

// ---------------------------------------------------------------------------
// McastForwardPolicy

McastForwardPolicy::Verdict McastForwardPolicy::Check(const uint8_t *aPacket, size_t aLength)
{
    Verdict    verdict = kForward;
    Ip6Address destination;
    Ip6Address source;
    uint16_t   payloadLength;

    VerifyOrExit(aLength >= kIp6HeaderSize && (aPacket[0] >> 4) == 6, verdict = kNotIp6);
    payloadLength = static_cast<uint16_t>((aPacket[4] << 8) | aPacket[5]);
    VerifyOrExit(kIp6HeaderSize + payloadLength <= aLength, verdict = kNotIp6);

    destination = GetDestination(aPacket);
    VerifyOrExit(destination.IsMulticast(), verdict = kNotMulticast);
    VerifyOrExit(destination.GetScope() >= kMinForwardedScope, verdict = kScopeTooSmall);

    source = GetSource(aPacket);
    VerifyOrExit(!source.IsMulticast() && !source.IsUnspecified() && !source.IsLinkLocal(), verdict = kBadSource);

    VerifyOrExit(aPacket[kHopLimitOffset] >= kMinHopLimit, verdict = kHopLimitExceeded);

exit:
    return verdict;
}

void McastForwardPolicy::DecrementHopLimit(uint8_t *aPacket)
{
    if (aPacket[kHopLimitOffset] > 0)
    {
        aPacket[kHopLimitOffset]--;
    }
}

size_t McastForwardPolicy::BuildEthernetFrame(const uint8_t *aPacket,
                                              size_t         aLength,
                                              const uint8_t *aSourceMac,
                                              uint8_t       *aFrame,
                                              size_t         aFrameCapacity)
{
    size_t length = 0;

    VerifyOrExit(aLength >= kIp6HeaderSize && kEthernetHeaderSize + aLength <= aFrameCapacity);

    // RFC 2464 §7: the multicast MAC is 33:33 followed by the group's low 32 bits.
    aFrame[0] = 0x33;
    aFrame[1] = 0x33;
    memcpy(aFrame + 2, aPacket + kDestinationOffset + 12, 4);
    memcpy(aFrame + kMacSize, aSourceMac, kMacSize);
    aFrame[12] = 0x86;
    aFrame[13] = 0xdd;
    memcpy(aFrame + kEthernetHeaderSize, aPacket, aLength);
    length = kEthernetHeaderSize + aLength;

exit:
    return length;
}

Ip6Address McastForwardPolicy::GetDestination(const uint8_t *aPacket)
{
    Ip6Address address;

    memcpy(address.m8, aPacket + kDestinationOffset, sizeof(address.m8));
    return address;
}

Ip6Address McastForwardPolicy::GetSource(const uint8_t *aPacket)
{
    Ip6Address address;

    memcpy(address.m8, aPacket + kSourceOffset, sizeof(address.m8));
    return address;
}

const char *McastForwardPolicy::VerdictToString(Verdict aVerdict)
{
    const char *str = "unknown";

    switch (aVerdict)
    {
    case kForward:
        str = "forward";
        break;
    case kNotIp6:
        str = "not IPv6";
        break;
    case kNotMulticast:
        str = "not multicast";
        break;
    case kScopeTooSmall:
        str = "scope too small";
        break;
    case kBadSource:
        str = "bad source";
        break;
    case kHopLimitExceeded:
        str = "hop limit exceeded";
        break;
    }

    return str;
}

// ---------------------------------------------------------------------------
// McastDedupCache

McastDedupCache::McastDedupCache(uint32_t aWindowMs, size_t aMaxEntries)
    : mWindowMs(aWindowMs)
    , mMaxEntries(aMaxEntries)
{
}

uint64_t McastDedupCache::Fingerprint(const uint8_t *aPacket, size_t aLength)
{
    // FNV-1a, 64-bit.
    uint64_t hash = 0xcbf29ce484222325ULL;

    for (size_t i = 0; i < aLength; i++)
    {
        uint8_t byte = (i == kHopLimitOffset) ? 0 : aPacket[i];

        hash ^= byte;
        hash *= 0x100000001b3ULL;
    }

    return hash;
}

bool McastDedupCache::Check(uint64_t aFingerprint, Timepoint aNow)
{
    bool isDuplicate = false;
    auto it          = mSeen.find(aFingerprint);

    if (it != mSeen.end())
    {
        isDuplicate = aNow - it->second->second <= Milliseconds(mWindowMs);
        // Seen again: it becomes the most recent entry.
        it->second->second = aNow;
        mOrder.splice(mOrder.end(), mOrder, it->second);
        ExitNow();
    }

    // The list is ordered by last sighting, so expired entries are at the
    // front; drop them, and if the cache is still full, the least recent one.
    while (!mOrder.empty() && aNow - mOrder.front().second > Milliseconds(mWindowMs))
    {
        mSeen.erase(mOrder.front().first);
        mOrder.pop_front();
    }
    if (mOrder.size() >= mMaxEntries)
    {
        mSeen.erase(mOrder.front().first);
        mOrder.pop_front();
    }
    mOrder.emplace_back(aFingerprint, aNow);
    mSeen.emplace(aFingerprint, std::prev(mOrder.end()));

exit:
    return isDuplicate;
}

// ---------------------------------------------------------------------------
// TokenBucket

TokenBucket::TokenBucket(uint32_t aRatePerSecond, uint32_t aBurst)
    : mRatePerSecond(aRatePerSecond)
    , mBurst(aBurst)
    , mTokens(aBurst)
    , mLastRefill(Timepoint::min())
{
}

bool TokenBucket::Take(Timepoint aNow)
{
    bool taken = false;

    if (mLastRefill != Timepoint::min() && aNow > mLastRefill)
    {
        double elapsedSec = std::chrono::duration_cast<std::chrono::duration<double>>(aNow - mLastRefill).count();

        mTokens += elapsedSec * mRatePerSecond;
        if (mTokens > mBurst)
        {
            mTokens = mBurst;
        }
    }
    mLastRefill = aNow;

    if (mTokens >= 1.0)
    {
        mTokens -= 1.0;
        taken = true;
    }

    return taken;
}

// ---------------------------------------------------------------------------
// McastRateLimiter

McastRateLimiter::McastRateLimiter(const Config &aConfig)
    : mConfig(aConfig)
    , mTotal(aConfig.mTotalRatePerSecond, aConfig.mTotalBurst)
{
}

bool McastRateLimiter::Allow(const Ip6Address &aGroup, Timepoint aNow)
{
    bool allowed = false;
    auto it      = mGroups.find(aGroup);

    if (it == mGroups.end())
    {
        if (mGroups.size() >= mConfig.mMaxGroups)
        {
            auto idlest = mGroups.begin();

            for (auto candidate = mGroups.begin(); candidate != mGroups.end(); ++candidate)
            {
                if (candidate->second.mLastUse < idlest->second.mLastUse)
                {
                    idlest = candidate;
                }
            }
            mGroups.erase(idlest);
        }
        it = mGroups.emplace(aGroup, GroupState(mConfig, aNow)).first;
    }

    it->second.mLastUse = aNow;

    // Take from the group first: a group over its own limit must not drain
    // the shared budget for the others.
    VerifyOrExit(it->second.mBucket.Take(aNow));
    VerifyOrExit(mTotal.Take(aNow));
    allowed = true;

exit:
    return allowed;
}

} // namespace otbr
