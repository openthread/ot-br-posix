/*
 *    Copyright (c) 2024, The OpenThread Authors.
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
 *   This file include definitions for the partition wide single uploader.
 */

#ifndef SINGLE_UPLOADER_HPP
#define SINGLE_UPLOADER_HPP

#include <iostream>
#include <sys/timerfd.h>

#include "common/mainloop.hpp"
#include "common/code_utils.hpp"

namespace otbr {

class Uploader : public MainloopProcessor, private NonCopyable
{
public:
    enum class State
    {
        kNotPublished,
        kWaitForPublish,
        kPublished,
        kWaitForUnpublish,
    };

    Uploader();
    ~Uploader();

    void Update(MainloopContext &aMainloop) override;
    void Process(const MainloopContext &aMainloop) override;

private:
    enum class NetDataService
    {
        kNone,               // No uploader service published in network data
        kOne,                // only one uploader service published in network data
        kMultiWithLowerPri,  // more than one uploader services are published in network data,
                             // and the current node published a lower priority service
        kMultiWithHighestPri // more than one uploader services are published in network data,
                             // and the current node published a highest priority service
    };

    const int kLoopCheckIntervalSec = 1; // TODO: 600

    NetDataService checkUploaderServiceInNetworkData();

    // TODO: add implementation
    bool anyUploaderServiceInNetworkData() { return true; }

    // set timer by timerfd_settime()
    void SetTimer(int64_t aTimeoutSec) {
        struct itimerspec its;
        its.it_value.tv_sec = aTimeoutSec;
        its.it_value.tv_nsec = 0;
        its.it_interval.tv_sec = 0;
        its.it_interval.tv_nsec = 0;

        if (timerfd_settime(mTimerFd, 0, &its, NULL) < 0) {
            std::cerr << "timerfd_settime() failed" << std::endl;
        }
    }

    // TODO: add implementation
    void RamdomBackoffDelay() {
        std::cout << "RamdomBackoffDelay()" << std::endl;
        SetTimer(1);
    }

    // TODO: add implementation
    bool publishUploaderService() {
        std::cout << "publishUploaderService()" << std::endl;
        return true;
    }

    // TODO: add implementation
    bool unpublishUploaderService() {
        std::cout << "unpublishUploaderService()" << std::endl;
        return true;
    }

    int mTimerFd;
    State mState;

    // TODO: add task to check Internet access
    bool mHasInternetAccess;
}; // class Uploader

} // namespace otbr

#endif // SINGLE_UPLOADER_HPP