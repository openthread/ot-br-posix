/*
 *    Copyright (c) 2020, The OpenThread Authors.
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

#ifndef OTBR_COMMON_TIMER_HPP
#define OTBR_COMMON_TIMER_HPP

#include <functional>
#include <list>

#include "common/time.hpp"
#include "common/timer_scheduler.hpp"
#include "common/types.hpp"

namespace otbr {

/**
 * This class implements delayed function call.
 *
 */
class Timer {
public:
    friend class TimerScheduler;

    /**
     * This type represents the function bound to a Timer object.
     *
     */
    using Callback = std::function<void(Timer &aTimer)>;

    /**
     * This constructor binds the callback.
     *
     * @param  aCallback  A function to be called when the timer fires.
     *
     */
    explicit Timer(Callback aCallback)
        : mCallback(aCallback)
        , mFireTime(TimePoint::max())
        , mIsRunning(false) {}

    /**
     * This method starts the timer with given delay.
     *
     * @param  aDelay  The delay which the timer will fire after.
     *
     */
    void Start(Duration aDelay)
    {
        Start(Clock::now() + aDelay);
    }

    /**
     * This method starts the timer with given fire time.
     *
     * @param  aFireTime  The time point the timer will fire at.
     *
     */
    void Start(TimePoint aFireTime)
    {
        Stop();
        mFireTime  = aFireTime;
        mIsRunning = true;
        TimerScheduler::Get().Add(this);
    }

    /**
     * This method stops a timer.
     *
     */
    void Stop()
    {
        // We don't need to manually remove timer from the TimerScheduler.
        // Dead timers will be automatically cleaned up.
        mIsRunning = false;
    }

    /**
     * This method devices if the timer is running.
     *
     * @returns A boolean indicates if the timer is running.
     *
     */
    bool IsRunning() const { return mIsRunning; }

    /**
     * this method returns the time point the timer will fire at.
     *
     * @returns The fire time point.
     *
     */
    TimePoint GetFireTime() const { return mFireTime; }

private:
    void Fire() {
        if (mIsRunning && mCallback != nullptr)
        {
            mCallback(*this);
        }
        Stop();
    }

    Callback mCallback;
    TimePoint mFireTime;
    bool mIsRunning;
};

} // namespace otbr

#endif // OTBR_COMMON_TIMER_HPP
