/*
 *    Copyright (c) 2017, The OpenThread Authors.
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

#ifndef OTBR_COMMON_TIME_HPP_
#define OTBR_COMMON_TIME_HPP_

#include "openthread-br/config.h"

#include <chrono>

#include <stdint.h>

#include <sys/time.h>

namespace otbr {

using Clock        = std::chrono::steady_clock;
using Duration     = std::chrono::steady_clock::duration;
using TimePoint    = std::chrono::time_point<Clock>;
using MicroSeconds = std::chrono::microseconds;
using Seconds      = std::chrono::seconds;

/**
 * This method returns the timestamp in milliseconds of @aTime.
 *
 * @param[in]   aTime   The time to convert to timestamp.
 *
 * @returns timestamp in milliseconds.
 *
 */
inline unsigned long GetTimestamp(const timeval &aTime)
{
    return static_cast<unsigned long>(aTime.tv_sec * 1000 + aTime.tv_usec / 1000);
}

/**
 * This function returns the timeval in microseconds.
 *
 * @param  aTimeval  The timeval to convert to microseconds.
 *
 * @returns timeval in microseconds.
 *
 */
inline MicroSeconds GetMicroSeconds(const timeval &aTimeval)
{
    return MicroSeconds{aTimeval.tv_sec * 1000000 + aTimeval.tv_usec};
}

/**
 * This function returns microseconds in timeval.
 *
 * @param  aMicroSeconds  the microseconds to convert to timeval.
 *
 * @returns microseconds in timeval.
 *
 */
inline struct timeval GetTimeval(MicroSeconds aMicroSeconds)
{
    struct timeval ret;

    ret.tv_sec  = aMicroSeconds.count() / 1000000;
    ret.tv_usec = aMicroSeconds.count() % 1000000;

    return ret;
}

/**
 * This method returns the current timestamp in milliseconds.
 *
 * @returns Current timestamp in milliseconds.
 *
 */
inline unsigned long GetNow(void)
{
    timeval now;

    gettimeofday(&now, nullptr);
    return static_cast<unsigned long>(now.tv_sec * 1000 + now.tv_usec / 1000);
}

} // namespace otbr

#endif // OTBR_COMMON_TIME_HPP_
