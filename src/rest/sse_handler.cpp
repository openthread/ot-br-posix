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

#define OTBR_LOG_TAG "REST"

#include "sse_handler.hpp"

#include "common/logging.hpp"

namespace otbr {
namespace rest {

// ─────────────────────────────────────────────────────────────────────
//  SseBroadcaster static constexpr definitions
// ─────────────────────────────────────────────────────────────────────
constexpr uint8_t SseBroadcaster::kMaxClients;
constexpr int     SseBroadcaster::kHeartbeatIntervalS;
constexpr int     SseBroadcaster::kInvalidSlot;

// ─────────────────────────────────────────────────────────────────────
//  SseBroadcaster implementation
// ─────────────────────────────────────────────────────────────────────

SseBroadcaster::SseBroadcaster(void)
    : mEventId(0)
    , mLastEventId(-1)
    , mClientCount(0)
    , mClosed(false)
{
    for (uint8_t i = 0; i < kMaxClients; i++)
    {
        mSlots[i].mActive = false;
    }
}

void SseBroadcaster::Broadcast(std::shared_ptr<const void> aPayload)
{
    std::lock_guard<std::mutex> lock(mMutex);
    mLastEventId    = mEventId++;
    mCurrentPayload = std::move(aPayload);
    otbrLogDebug("SSE: broadcast eventId=%d clients=%u", mLastEventId, mClientCount);
    mCv.notify_all();
}

bool SseBroadcaster::WaitEvent(int &aClientEventId, std::shared_ptr<const void> &aPayload)
{
    std::unique_lock<std::mutex> lock(mMutex);

    bool got = mCv.wait_for(lock, std::chrono::seconds(kHeartbeatIntervalS),
                            [this, aClientEventId] { return mLastEventId != aClientEventId || mClosed; });

    if (mClosed)
    {
        otbrLogDebug("SSE: WaitEvent exiting because broadcaster is closed");
        return false;
    }

    if (!got)
    {
        // Timeout — caller sends heartbeat (null payload = heartbeat signal)
        aPayload.reset();
        otbrLogDebug("SSE: heartbeat timeout fired for client eventId=%d", aClientEventId);
        return true;
    }

    aClientEventId = mLastEventId;
    aPayload       = mCurrentPayload;
    otbrLogDebug("SSE: client woke on eventId=%d", aClientEventId);
    return true;
}

bool SseBroadcaster::AddClient(int &aSlot)
{
    std::lock_guard<std::mutex> lock(mMutex);

    if (mClientCount >= kMaxClients)
    {
        aSlot = kInvalidSlot;
        return false;
    }

    for (uint8_t i = 0; i < kMaxClients; i++)
    {
        if (!mSlots[i].mActive)
        {
            mSlots[i].mActive = true;
            mClientCount++;
            aSlot = static_cast<int>(i);
            otbrLogInfo("SSE: client added slot=%d total_clients=%u", aSlot, mClientCount);
            return true;
        }
    }

    // Should not happen if mClientCount is accurate
    aSlot = kInvalidSlot;
    return false;
}

void SseBroadcaster::RemoveClient(int aSlot)
{
    std::lock_guard<std::mutex> lock(mMutex);

    if (aSlot >= 0 && aSlot < kMaxClients && mSlots[aSlot].mActive)
    {
        mSlots[aSlot].mActive = false;
        if (mClientCount > 0)
        {
            mClientCount--;
        }
        otbrLogInfo("SSE: client removed slot=%d total_clients=%u", aSlot, mClientCount);
    }
}

uint8_t SseBroadcaster::GetClientCount(void) const
{
    std::lock_guard<std::mutex> lock(mMutex);
    return mClientCount;
}

void SseBroadcaster::Close(void)
{
    std::lock_guard<std::mutex> lock(mMutex);
    mClosed = true;
    otbrLogInfo("SSE: broadcaster closed");
    mCv.notify_all();
}

void SseBroadcaster::Reset(void)
{
    std::lock_guard<std::mutex> lock(mMutex);
    mClosed = false;
    mCurrentPayload.reset();
    mEventId     = 0;
    mLastEventId = -1;
    otbrLogDebug("SSE: broadcaster reset (clients preserved=%u)", mClientCount);
    // Client slots are NOT cleared — existing connections persist.
}

} // namespace rest
} // namespace otbr
