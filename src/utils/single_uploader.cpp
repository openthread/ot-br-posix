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
 *   The file implements partition-wide single uploader.
 */

#define OTBR_LOG_TAG "UPLDR"

#include <unistd.h>
#include "common/logging.hpp"
#include "single_uploader.hpp"

namespace otbr {

Uploader::Uploader()
    : mState(State::kNotPublished), mHasInternetAccess(false)
{
    mTimerFd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    VerifyOrDie(mTimerFd >= 0, "timerfd_create failed");
    otbrLogInfo("Uploader started");
    SetTimer(5);
}

Uploader::~Uploader()
{
    close(mTimerFd);
}

void Uploader::Update(MainloopContext &aMainloop)
{
    FD_SET(mTimerFd, &aMainloop.mReadFdSet);
    aMainloop.mMaxFd = std::max(aMainloop.mMaxFd, mTimerFd);
}

void Uploader::Process(const MainloopContext &aMainloop)
{
    if (!FD_ISSET(mTimerFd, &aMainloop.mReadFdSet)) {
        return;
    }

    uint64_t exp;
    std::cout << "calling read()" << std::endl;
    read(mTimerFd, &exp, sizeof(exp)); // read from timerfd to consume the data

    switch(mState)
    {
        case State::kNotPublished:
        {
            if (!mHasInternetAccess) {
                break;
            }

            switch(checkUploaderServiceInNetworkData())
            {
                case NetDataService::kNone:
                    RamdomBackoffDelay();
                    mState = State::kWaitForPublish;
                    break;
                case NetDataService::kMultiWithLowerPri:
                    unpublishUploaderService();
                    mState = State::kNotPublished;
                    break;
                case NetDataService::kOne:
                case NetDataService::kMultiWithHighestPri:
                    // already published by a node, or current node has published a service
                    // with highest priority, do nothing
                    break;
            }
            break;
        }
        case State::kWaitForPublish:
        {
            if (!mHasInternetAccess) {
                mState = State::kNotPublished;
                break;
            }

            if (checkUploaderServiceInNetworkData() == NetDataService::kNone) {
                publishUploaderService();
                mState = State::kPublished;
            } else {
                mState = State::kNotPublished;
            }
            break;
        }
        case State::kPublished:
        {
            if (!mHasInternetAccess) {
                // no internet access currently, wait for unpublish
                mState = State::kWaitForUnpublish;
                break;
            }

            if (checkUploaderServiceInNetworkData() == NetDataService::kMultiWithLowerPri) {
                // should be rare case that there are multiple nodes in Thread network
                // published the uploader services
                unpublishUploaderService();
                mState = State::kNotPublished;
            }
            break;
        }
        case State::kWaitForUnpublish:
        {
            if (mHasInternetAccess) {
                // Internet access assumed
                mState = State::kPublished;
            } else {
                unpublishUploaderService();
                mState = State::kNotPublished;
            }
            break;
        }
    }

    SetTimer(kLoopCheckIntervalSec);
}

Uploader::NetDataService Uploader::checkUploaderServiceInNetworkData() {
    NetDataService ret = NetDataService::kNone;
    std::cout << "checkUploaderServiceInNetworkData()" << std::endl;

    // TODO: iteration network data

    return ret;
}


} // namespace otbr