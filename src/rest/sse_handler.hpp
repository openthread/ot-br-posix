/*
 *  Copyright (c) 2026, The OpenThread Authors.
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

/**
 * @file
 *   Generic Server-Sent Events broadcaster.
 */

#ifndef OTBR_REST_SSE_HANDLER_HPP_
#define OTBR_REST_SSE_HANDLER_HPP_

#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>

namespace otbr {
namespace rest {

class SseBroadcaster
{
public:
    static constexpr uint8_t kMaxClients         = 4;
    static constexpr int     kHeartbeatIntervalS = 30;
    static constexpr int     kInvalidSlot        = -1;

    SseBroadcaster(void);

    // ── Producer API (mainloop thread) ───────────────────────────────

    /**
     * Broadcast an opaque payload to all connected SSE clients.
     *
     * The payload type is owned by the domain (e.g. a vector of
     * otNetworkDiagTlv); the engine only forwards the pointer and
     * signals that a new event is available.
     *
     * @param[in] aPayload  Domain payload, shared with all consumers.
     */
    void Broadcast(std::shared_ptr<const void> aPayload);

    // ── Consumer API (httplib worker threads) ────────────────────────

    /**
     * Block until a new payload is available, the stream is closed, or
     * the heartbeat timeout expires.
     *
     * @param[in,out] aClientEventId  Caller's last-seen event ID (stack var). Updated.
     * @param[out]    aPayload        Domain payload to format (nullptr on heartbeat).
     *
     * @returns  true  → aPayload valid (or heartbeat — aPayload nullptr).
     *           false → stream closed, consumer should exit.
     */
    bool WaitEvent(int &aClientEventId, std::shared_ptr<const void> &aPayload);

    /**
     * Register a new SSE client.
     *
     * @param[out] aSlot    Assigned slot index (0..kMaxClients-1).
     *
     * @returns true if accepted, false if all slots full.
     */
    bool AddClient(int &aSlot);

    /**
     * Unregister an SSE client by slot.
     *
     * @param[in] aSlot  Slot returned by AddClient().
     */
    void RemoveClient(int aSlot);

    uint8_t GetClientCount(void) const;

    /**
     * Close all streams. Wakes all WaitEvent() callers.
     */
    void Close(void);

    /**
     * Reset for reuse (new streaming session).
     * Client slots are NOT cleared — existing connections persist.
     */
    void Reset(void);

private:
    struct ClientSlot
    {
        bool mActive;
    };

    mutable std::mutex      mMutex;
    std::condition_variable mCv;

    // Current broadcast payload (opaque; shared by all consumers)
    std::shared_ptr<const void> mCurrentPayload;
    int                         mEventId;     ///< Bumped on each Broadcast()
    int                         mLastEventId; ///< ID of current payload

    ClientSlot mSlots[kMaxClients];
    uint8_t    mClientCount;
    bool       mClosed;
};

} // namespace rest
} // namespace otbr

#endif // OTBR_REST_SSE_HANDLER_HPP_
