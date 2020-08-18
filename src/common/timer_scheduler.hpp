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

#ifndef OTBR_COMMON_TIMER_SCHEDULER_HPP_
#define OTBR_COMMON_TIMER_SCHEDULER_HPP_

#include <list>

#include "common/time.hpp"
#include "common/types.hpp"

namespace otbr {

class Timer;

/**
 * This class implements a Timer Scheduler which accepts registration of
 * timer events and drives them.
 *
 */
class TimerScheduler
{
public:
    /**
     * This method returns the TimerScheduler singleton.
     *
     * @return A single, global Timer Scheduler.
     *
     */
    static TimerScheduler &Get();

    /**
     * This method process all timer events, cleanup dead timers.
     *
     * @param[in]  aNow  The current time point.
     *
     * @returns Delay of next earliest timer event in microseconds.
     *
     */
    MicroSeconds Process(TimePoint aNow);

    /**
     * This method adds a new timer into the scheduler.
     *
     * @param[in]  aTimer  The timer to be added.
     *
     */
    void Add(Timer *aTimer);

private:
    TimerScheduler() = default;

    // The timer list sorted by their Fire Time. Earlier timer comes first.
    std::list<Timer *> mSortedTimerList;
};

} // namespace otbr

#endif // OTBR_COMMON_TIMER_SCHEDULER_HPP_
