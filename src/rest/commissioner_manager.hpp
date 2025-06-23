/*
 *  Copyright (c) 2024, The OpenThread Authors.
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
#ifndef EXTENSIONS_COMMISSIONER_MANAGER_HPP_
#define EXTENSIONS_COMMISSIONER_MANAGER_HPP_

#include <bitset>
#include <vector>

#include "common/time.hpp"
#include "rest/types.hpp"

#include "openthread/commissioner.h"

namespace otbr {
namespace rest {

/**
 * This class implements a commissioner manager.
 */
class CommissionerManager
{
public:
    enum JoinerState : uint8_t
    {
        kJoinerStateWaiting,   // Not added to commissioner yet
        kJoinerStatePending,   // Not attempted
        kJoinerStateJoined,    // Success
        kJoinerStateAttempted, // Connected at least once
        kJoinerStateFailed,    // Failed and expired
        kJoinerStateExpired,   // Expired without any attempt
    };

    /**
     * This class implements a joiner entry.
     */
    class JoinerEntry
    {
        friend class CommissionerManager;

    public:
        /**
         * Constructor for a JoinerEntry.
         *
         * @param[in] aJoiner  The joinerInfo of the joiner.
         * @param[in] aTimeout The timeout of the joiner.
         */
        JoinerEntry(const otJoinerInfo &aJoiner, uint32_t aTimeout);

        /**
         * Checks if the joiner matches the given EUI64.
         */
        bool MatchesEui64(const otExtAddress &aEui64) const;

        /**
         * Checks if the joiner matches the given Discerner.
         */
        bool MatchesDiscerner(const otJoinerDiscerner &aDiscerner) const;

        /**
         * Checks if the joiner is joined.
         */
        bool IsJoined(void) const { return mState == kJoinerStateJoined; }

        /**
         * Checks if the joiner is pending.
         */
        bool IsPending(void) const
        {
            return mState == kJoinerStateWaiting || mState == kJoinerStatePending || mState == kJoinerStateAttempted;
        }

        /**
         * Gets the state of the joiner.
         */
        JoinerState GetState(void) const { return mState; }

        /**
         * Gets the state of the joiner as a string.
         */
        const char *GetStateString(void) const { return CommissionerManager::JoinerStateToString(mState); }

        /**
         * Gets the timeout of the joiner.
         */
        Timepoint GetTimeout(void) const { return mTimeout; }

    private:
        /**
         * Registers the joiner.
         *
         * @param[in] aInstance The OpenThread instance.
         *
         * @retval OT_ERROR_NONE          Successfully registered.
         * @retval OT_ERROR_INVALID_STATE The joiner is not in a valid state.
         */
        otError Register(otInstance *aInstance);

        otJoinerInfo mJoiner; // Duplicated here to enable reading the joiner after loosing the commissioner
        JoinerState  mState;
        Timepoint    mTimeout; // Duplicated here to enable readding the joiner after lossing the commissioner
    };

    /**
     * Constructor for a CommissionerManager.
     *
     * @param[in] aInstance The OpenThread instance.
     */
    CommissionerManager(otInstance *aInstance)
        : mInstance{aInstance}
        , mJoiners{}
        , mState{OT_COMMISSIONER_STATE_DISABLED}
    {
    }

    /**
     * Destructor for a CommissionerManager.
     */
    ~CommissionerManager();

    /**
     * Adds a joiner.
     *
     * @param[in] aJoiner  The joinerInfo of the joiner.
     * @param[in] aTimeout The timeout of the joiner.
     *
     * @retval OT_ERROR_NONE          Successfully added.
     * @retval OT_ERROR_ALREADY       The joiner is already added.
     * @retval OT_ERROR_INVALID_ARGS  The joiner is invalid.
     */
    otError AddJoiner(const otJoinerInfo &aJoiner, uint32_t aTimeout);

    /**
     * Removes a joiner.
     *
     * @param[in] aEui64 The EUI64 of the joiner.
     */
    void RemoveJoiner(const otJoinerInfo &aJoiner);

    /**
     * Removes all joiners.
     */
    void RemoveAllJoiners(void);

    /**
     * Finds a joiner.
     *
     * @param[in] aEui64 The EUI64 of the joiner.
     *
     * @returns The joiner entry if found, nullptr otherwise.
     */
    const JoinerEntry *FindJoiner(const otExtAddress &aEui64);
    const JoinerEntry *FindJoiner(const otJoinerInfo &aJoiner);

    /**
     * Processes the commissioner manager.
     */
    void Process(void);

    /**
     * Gets the state of the joiner.
     */
    static const char *JoinerStateToString(JoinerState aState);

private:
    bool ShouldActivate(void) const;
    void TryActivate(void);

    void HandleCommissionerStateCallback(otCommissionerState aState);
    void HandleCommissionerJoinerCallback(otCommissionerJoinerEvent aEvent,
                                          const otJoinerInfo       *aJoinerInfo,
                                          const otExtAddress       *aJoinerId);

    static bool IsEui64Null(const otExtAddress &aEui64);

    static void StateCallback(otCommissionerState aState, void *aContext);
    static void JoinerCallback(otCommissionerJoinerEvent aEvent,
                               const otJoinerInfo       *aJoinerInfo,
                               const otExtAddress       *aJoinerId,
                               void                     *aContext);

    otInstance *mInstance;

    std::vector<JoinerEntry> mJoiners;

    otCommissionerState mState; // The current commissioner state from our perspective.
                                // If we do not own the commissioner this is OT_COMMISSIONER_STATE_DISABLE
};

} // namespace rest
} // namespace otbr

#endif // EXTENSIONS_COMMISSIONER_MANAGER_HPP_
