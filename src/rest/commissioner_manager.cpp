/*
 *  Copyright (c) 2025, The OpenThread Authors.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

#include "rest/commissioner_manager.hpp"

#include <string.h>

#include "common/code_utils.hpp"

namespace otbr {
namespace rest {

CommissionerManager::~CommissionerManager()
{
    if (mState != OT_COMMISSIONER_STATE_DISABLED)
    {
        IgnoreError(otCommissionerStop(mInstance));
    }
}

otError CommissionerManager::AddJoiner(const otJoinerInfo &aJoiner, uint32_t aTimeout)
{
    otError error = OT_ERROR_NONE;
    bool    is_conflicting;

#ifndef OPENTHREAD_COMMISSIONER_ALLOW_ANY_JOINER
    VerifyOrExit(aJoiner.mType != OT_JOINER_INFO_TYPE_ANY, error = OT_ERROR_INVALID_ARGS);
#endif

    for (const JoinerEntry &joiner : mJoiners)
    {
        is_conflicting = false;

        if (aJoiner.mType == joiner.mJoiner.mType)
        {
            switch (aJoiner.mType)
            {
            case OT_JOINER_INFO_TYPE_EUI64:
                is_conflicting = joiner.MatchesEui64(aJoiner.mSharedId.mEui64);
                break;
            case OT_JOINER_INFO_TYPE_DISCERNER:
                is_conflicting = joiner.MatchesDiscerner(aJoiner.mSharedId.mDiscerner);
                break;
            case OT_JOINER_INFO_TYPE_ANY:
                // There can only be one joiner entry with type ANY in the joiner table
                is_conflicting = true;
                break;
            default:
                break;
            }
        }

        // We may instead ignore existing joiners and overwrite the timeout
        VerifyOrExit(!is_conflicting, error = OT_ERROR_ALREADY);
    }

    mJoiners.emplace_back(JoinerEntry(aJoiner, aTimeout));

    if (mState == OT_COMMISSIONER_STATE_ACTIVE)
    {
        IgnoreError(mJoiners.back().Register(mInstance));
    }
    else
    {
        TryActivate();
    }

exit:
    return error;
}

void CommissionerManager::RemoveJoiner(const otJoinerInfo &aJoiner)
{
    for (auto it = mJoiners.begin(); it < mJoiners.end(); ++it)
    {
        if ((aJoiner.mType == OT_JOINER_INFO_TYPE_EUI64) && (it->MatchesEui64(aJoiner.mSharedId.mEui64)))
        {
            if (mState == OT_COMMISSIONER_STATE_ACTIVE)
            {
                const otExtAddress *addrPtr = &aJoiner.mSharedId.mEui64;
                if (IsEui64Null(aJoiner.mSharedId.mEui64))
                {
                    addrPtr = nullptr;
                }

                IgnoreError(otCommissionerRemoveJoiner(mInstance, addrPtr));
            }

            mJoiners.erase(it);

            break;
        }

        if (aJoiner.mType == OT_JOINER_INFO_TYPE_DISCERNER)
        {
            if (it->mJoiner.mSharedId.mDiscerner.mValue == aJoiner.mSharedId.mDiscerner.mValue &&
                it->mJoiner.mSharedId.mDiscerner.mLength == aJoiner.mSharedId.mDiscerner.mLength)
            {
                if (mState == OT_COMMISSIONER_STATE_ACTIVE)
                {
                    IgnoreError(otCommissionerRemoveJoinerWithDiscerner(mInstance, &aJoiner.mSharedId.mDiscerner));
                }

                mJoiners.erase(it);

                break;
            }
        }
    }
}

void CommissionerManager::RemoveAllJoiners(void)
{
    if (mState == OT_COMMISSIONER_STATE_ACTIVE)
    {
        for (const JoinerEntry &joiner : mJoiners)
        {
            if (joiner.mJoiner.mType == OT_JOINER_INFO_TYPE_EUI64)
            {
                IgnoreError(otCommissionerRemoveJoiner(mInstance, &joiner.mJoiner.mSharedId.mEui64));
            }
            else if (joiner.mJoiner.mType == OT_JOINER_INFO_TYPE_DISCERNER)
            {
                IgnoreError(otCommissionerRemoveJoinerWithDiscerner(mInstance, &joiner.mJoiner.mSharedId.mDiscerner));
            }
            else
            {
                // Joiner type is ANY
                IgnoreError(otCommissionerRemoveJoiner(mInstance, nullptr));
            }
        }
    }

    mJoiners.clear();
}

const CommissionerManager::JoinerEntry *CommissionerManager::FindJoiner(const otJoinerInfo &aJoiner)
{
    const JoinerEntry *entry = nullptr;

    for (JoinerEntry &joiner : mJoiners)
    {
        if ((aJoiner.mType == OT_JOINER_INFO_TYPE_EUI64) && (joiner.MatchesEui64(aJoiner.mSharedId.mEui64)))
        {
            entry = &joiner;
            break;
        }
        else if ((aJoiner.mType == OT_JOINER_INFO_TYPE_DISCERNER) &&
                 (joiner.mJoiner.mSharedId.mDiscerner.mValue == aJoiner.mSharedId.mDiscerner.mValue) &&
                 (joiner.mJoiner.mSharedId.mDiscerner.mLength == aJoiner.mSharedId.mDiscerner.mLength))
        {
            entry = &joiner;
            break;
        }
        else if ((aJoiner.mType == OT_JOINER_INFO_TYPE_ANY) && (joiner.mJoiner.mType == OT_JOINER_INFO_TYPE_ANY))
        {
            // there can only be one joiner entry with type ANY in the joiner table
            entry = &joiner;
            break;
        }
    }

    return entry;
}

otError CommissionerManager::StartEnergyScan(uint32_t            aChannelMask,
                                             uint8_t             aCount,
                                             uint16_t            aPeriod,
                                             uint16_t            aScanDuration,
                                             const otIp6Address *aAddress)
{
    otError         error = OT_ERROR_NONE;
    std::bitset<32> mask(aChannelMask);

    VerifyOrExit(mEnergyScanTimeout <= Clock::now(), error = OT_ERROR_ALREADY);
    VerifyOrExit(mEnergyScanState == kEnergyScanStateFree, error = OT_ERROR_ALREADY);
    VerifyOrExit(aChannelMask != 0, error = OT_ERROR_INVALID_ARGS);
    VerifyOrExit(aAddress != nullptr, error = OT_ERROR_INVALID_ARGS);

    mEnergyScanState       = kEnergyScanStateWaiting;
    mEnergyScanChannelMask = aChannelMask;
    mEnergyScanCount       = aCount;
    mEnergyScanPeriod      = aPeriod;
    mEnergyScanDuration    = aScanDuration;
    memcpy(&mEnergyScanAddress.mFields.m8[0], &aAddress->mFields.m8[0], OT_IP6_ADDRESS_SIZE);

    mEnergyScanReport.mReports.clear();
    for (size_t i = 0; i < mask.size(); i++)
    {
        if (!mask[i])
        {
            continue;
        }

        mEnergyScanReport.mReports.emplace_back(
            EnergyReport{.mChannel = static_cast<uint8_t>(i), .mMaxRssi = std::vector<int8_t>()});
    }
    memcpy(&mEnergyScanReport.mOrigin.mFields.m8[0], &aAddress->mFields.mComponents.mIid.mFields.m8[0],
           OT_IP6_IID_SIZE);

    if (mState == OT_COMMISSIONER_STATE_ACTIVE)
    {
        SendEnergyScan();
    }
    else
    {
        TryActivate();
    }

exit:
    return error;
}

otError CommissionerManager::GetEnergyScanStatus(void)
{
    otError error = OT_ERROR_NONE;

    switch (mEnergyScanState)
    {
    case kEnergyScanStateFree:
        error = OT_ERROR_INVALID_STATE;
        break;

    case kEnergyScanStateWaiting:
        error = OT_ERROR_PENDING;
        break;

    case kEnergyScanStateSent:
        if (Clock::now() < mEnergyScanTimeout)
        {
            error = OT_ERROR_PENDING;
        }
        else
        {
            mEnergyScanState = kEnergyScanStateReady;
            error            = OT_ERROR_NONE;
        }
        break;

    case kEnergyScanStateReady:
        error = OT_ERROR_NONE;
        break;

    case kEnergyScanStateFailed:
        error = OT_ERROR_FAILED;
        break;
    }

    return error;
}

const EnergyScanReport &CommissionerManager::GetEnergyScanResult(void) const
{
    return mEnergyScanReport;
}

void CommissionerManager::StopEnergyScan()
{
    mEnergyScanState = kEnergyScanStateFree;
}

void CommissionerManager::Process(void)
{
    if (ShouldActivate())
    {
        TryActivate();
    }
    else if (mState != OT_COMMISSIONER_STATE_DISABLED)
    {
        // Delay stopping to here in order to avoid potential recursion by
        // stopping in response to a event
        IgnoreError(otCommissionerStop(mInstance));
    }
}

CommissionerManager::JoinerEntry::JoinerEntry(const otJoinerInfo &aJoiner, uint32_t aTimeout)
    : mJoiner{}
    , mState{kJoinerStatePending}
    , mTimeout{Clock::now() + Seconds(aTimeout)}
{
    mJoiner.mType = aJoiner.mType;
    memcpy(&mJoiner.mSharedId, &aJoiner.mSharedId, sizeof(mJoiner.mSharedId));
    memcpy(mJoiner.mPskd.m8, aJoiner.mPskd.m8, sizeof(mJoiner.mPskd.m8));
}

bool CommissionerManager::JoinerEntry::MatchesEui64(const otExtAddress &aEui64) const
{
    bool matches = true;

    for (uint32_t i = 0; i < OT_EXT_ADDRESS_SIZE; i++)
    {
        if (mJoiner.mSharedId.mEui64.m8[i] != aEui64.m8[i])
        {
            matches = false;
            break;
        }
    }

    return matches;
}

bool CommissionerManager::JoinerEntry::MatchesDiscerner(const otJoinerDiscerner &aDiscerner) const
{
    bool matches = true;

    if (mJoiner.mSharedId.mDiscerner.mLength != aDiscerner.mLength)
    {
        matches = false;
    }
    if (mJoiner.mSharedId.mDiscerner.mValue != aDiscerner.mValue)
    {
        matches = false;
    }

    return matches;
}

otError CommissionerManager::JoinerEntry::Register(otInstance *aInstance)
{
    otError             error = OT_ERROR_NONE;
    Timepoint           now   = Clock::now();
    uint32_t            timeout;
    const otExtAddress *addrPtr = &mJoiner.mSharedId.mEui64;

    VerifyOrExit(IsPending(), error = OT_ERROR_INVALID_STATE);

    if (now >= mTimeout)
    {
        mState = kJoinerStateFailed;
        ExitNow(error = OT_ERROR_INVALID_STATE);
    }

    if (CommissionerManager::IsEui64Null(mJoiner.mSharedId.mEui64))
    {
        addrPtr = nullptr;
    }

    timeout = std::chrono::duration_cast<Seconds>(mTimeout - now).count();
    if (mJoiner.mType == OT_JOINER_INFO_TYPE_DISCERNER)
    {
        error =
            otCommissionerAddJoinerWithDiscerner(aInstance, &mJoiner.mSharedId.mDiscerner, mJoiner.mPskd.m8, timeout);
    }
    else
    {
        error = otCommissionerAddJoiner(aInstance, addrPtr, mJoiner.mPskd.m8, timeout);
    }

    if (error == OT_ERROR_NONE && mState == kJoinerStateWaiting)
    {
        mState = kJoinerStatePending;
    }

exit:
    return error;
}

bool CommissionerManager::ShouldActivate(void) const
{
    bool shouldActivate = false;

    for (const JoinerEntry &joiner : mJoiners)
    {
        if (joiner.IsPending())
        {
            ExitNow(shouldActivate = true);
        }
    }

    if (mEnergyScanState == kEnergyScanStateWaiting || mEnergyScanState == kEnergyScanStateSent)
    {
        ExitNow(shouldActivate = true);
    }

exit:
    return shouldActivate;
}

void CommissionerManager::TryActivate(void)
{
    if (mState == OT_COMMISSIONER_STATE_DISABLED)
    {
        IgnoreError(otCommissionerStart(mInstance, StateCallback, JoinerCallback, this));
    }
}

void CommissionerManager::SendEnergyScan(void)
{
    otError      error   = OT_ERROR_NONE;
    Milliseconds timeout = kEnergyScanNetDelay;

    VerifyOrExit(mEnergyScanState == kEnergyScanStateWaiting);

    SuccessOrExit(error = otCommissionerEnergyScan(mInstance, mEnergyScanChannelMask, mEnergyScanCount,
                                                   mEnergyScanPeriod, mEnergyScanDuration, &mEnergyScanAddress,
                                                   &CommissionerManager::EnergyScanReportCallback, this));

    mEnergyScanState = kEnergyScanStateSent;

    timeout += GetEnergyScanMinDelay(mEnergyScanChannelMask, mEnergyScanCount, mEnergyScanPeriod, mEnergyScanDuration);
    mEnergyScanTimeout = Clock::now() + timeout;

exit:
    if (error != OT_ERROR_NONE)
    {
        otbrLogWarning("Failed to start energy scan: %s", otThreadErrorToString(error));
    }

    return;
}

void CommissionerManager::HandleCommissionerStateCallback(otCommissionerState aState)
{
    if (mState != OT_COMMISSIONER_STATE_ACTIVE && aState == OT_COMMISSIONER_STATE_ACTIVE)
    {
        for (JoinerEntry &joiner : mJoiners)
        {
            IgnoreError(joiner.Register(mInstance));
        }

        SendEnergyScan();
    }

    if (aState == OT_COMMISSIONER_STATE_DISABLED)
    {
        if (mEnergyScanState == kEnergyScanStateSent)
        {
            mEnergyScanState = kEnergyScanStateFailed;
        }
    }

    mState = aState;
}

void CommissionerManager::HandleCommissionerJoinerCallback(otCommissionerJoinerEvent aEvent,
                                                           const otJoinerInfo       *aJoinerInfo,
                                                           const otExtAddress       *aJoinerId)
{
    OT_UNUSED_VARIABLE(aJoinerId);
    JoinerEntry *entry = nullptr;

    VerifyOrExit(aJoinerInfo != nullptr);

    if (aJoinerInfo->mType == OT_JOINER_INFO_TYPE_EUI64)
    {
        for (JoinerEntry &listEntry : mJoiners)
        {
            if (listEntry.MatchesEui64(aJoinerInfo->mSharedId.mEui64))
            {
                entry = &listEntry;
                break;
            }
        }
    }
    if (aJoinerInfo->mType == OT_JOINER_INFO_TYPE_DISCERNER)
    {
        for (JoinerEntry &listEntry : mJoiners)
        {
            if (listEntry.MatchesDiscerner(aJoinerInfo->mSharedId.mDiscerner))
            {
                entry = &listEntry;
                break;
            }
        }
    }
#ifdef OPENTHREAD_COMMISSIONER_ALLOW_ANY_JOINER
    else if (aJoinerInfo->mType == OT_JOINER_INFO_TYPE_ANY)
    {
        for (JoinerEntry &listEntry : mJoiners)
        {
            if (IsEui64Null(listEntry.mEui64))
            {
                entry = &listEntry;
                break;
            }
        }
    }
#endif

    VerifyOrExit(entry != nullptr);
    // TODO: for discerners or wildcards, we may count the number of events
    switch (aEvent)
    {
    case OT_COMMISSIONER_JOINER_START:
        entry->mState = kJoinerStateAttempted;
        break;

    case OT_COMMISSIONER_JOINER_CONNECTED:
        break;

    case OT_COMMISSIONER_JOINER_FINALIZE:
        break;

    case OT_COMMISSIONER_JOINER_END:
        entry->mState = kJoinerStateJoined;
        break;

    case OT_COMMISSIONER_JOINER_REMOVED:
        if (entry->mState == kJoinerStatePending)
        {
            entry->mState = kJoinerStateExpired;
        }
        else if (entry->mState == kJoinerStateAttempted)
        {
            entry->mState = kJoinerStateFailed;
        }
        break;
    }

exit:
    return;
}

void CommissionerManager::HandleEnergyScanReportCallback(uint32_t       aChannelMask,
                                                         const uint8_t *aEnergyList,
                                                         uint8_t        aEnergyListLength)
{
    std::bitset<32> mask(aChannelMask);
    size_t          reportCount = aEnergyListLength / mask.count();

    VerifyOrExit(mEnergyScanState == kEnergyScanStateSent);
    VerifyOrExit(mEnergyScanChannelMask == aChannelMask);
    VerifyOrExit(aEnergyListLength == reportCount * mask.count());

    for (size_t i = 0; i < reportCount; i++)
    {
        for (size_t channel = 0; channel < mask.count(); channel++)
        {
            int8_t rssi = aEnergyList[(i * mask.count()) + channel];
            mEnergyScanReport.mReports[channel].mMaxRssi.push_back(rssi);
        }
    }

exit:
    return;
}

bool CommissionerManager::IsEui64Null(const otExtAddress &aEui64)
{
    bool matches = true;

    for (uint32_t i = 0; i < OT_EXT_ADDRESS_SIZE; i++)
    {
        if (aEui64.m8[i] != 0)
        {
            matches = false;
            break;
        }
    }

    return matches;
}

void CommissionerManager::StateCallback(otCommissionerState aState, void *aContext)
{
    CommissionerManager *manager = reinterpret_cast<CommissionerManager *>(aContext);
    manager->HandleCommissionerStateCallback(aState);
}

void CommissionerManager::JoinerCallback(otCommissionerJoinerEvent aEvent,
                                         const otJoinerInfo       *aJoinerInfo,
                                         const otExtAddress       *aJoinerId,
                                         void                     *aContext)
{
    CommissionerManager *manager = reinterpret_cast<CommissionerManager *>(aContext);
    manager->HandleCommissionerJoinerCallback(aEvent, aJoinerInfo, aJoinerId);
}

void CommissionerManager::EnergyScanReportCallback(uint32_t       aChannelMask,
                                                   const uint8_t *aEnergyList,
                                                   uint8_t        aEnergyListLength,
                                                   void          *aContext)
{
    CommissionerManager *manager = reinterpret_cast<CommissionerManager *>(aContext);
    manager->HandleEnergyScanReportCallback(aChannelMask, aEnergyList, aEnergyListLength);
}

const char *CommissionerManager::JoinerStateToString(JoinerState aState)
{
    static const char *const kJoinerStateStrings[] = {
        "waiting",      // (0) kJoinerStateWaiting
        "undiscovered", // (1) kJoinerStatePending
        "completed",    // (2) kJoinerStateJoined
        "attempted",    // (3) kJoinerStateAttempted
        "failed",       // (4) kJoinerStateFailed
        "stopped",      // (5) kJoinerStateExpired
    };

    static_assert(kJoinerStateWaiting == 0, "kJoinerStateWaiting value is incorrect");
    static_assert(kJoinerStatePending == 1, "kJoinerStatePending value is incorrect");
    static_assert(kJoinerStateJoined == 2, "kJoinerStateJoined value is incorrect");
    static_assert(kJoinerStateAttempted == 3, "kJoinerStateAttempted value is incorrect");
    static_assert(kJoinerStateFailed == 4, "kJoinerStateFailed value is incorrect");
    static_assert(kJoinerStateExpired == 5, "kJoinerStateStopped value is incorrect");

    return kJoinerStateStrings[aState];
}

} // namespace rest
} // namespace otbr
