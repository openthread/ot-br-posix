/*
 *    Copyright (c) 2021, The OpenThread Authors.
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

#include "hidl/1.0/hidl_agent.hpp"

#include <hidl/HidlTransportSupport.h>

#include "common/code_utils.hpp"
#include "common/logging.hpp"
#include "common/types.hpp"

namespace otbr {
namespace Hidl {
using android::hardware::handleTransportPoll;
using android::hardware::setupTransportPolling;

HidlAgent::HidlAgent(otbr::Ncp::ControllerOpenThread *aNcp)
    : mThread(aNcp)
    , mSettings()
{
    VerifyOrDie((mHidlFd = setupTransportPolling()) >= 0, "Setup HIDL transport for use with (e)poll failed");
    mSettings.Init();
}

void HidlAgent::Init(void)
{
    mThread.Init();
}

void HidlAgent::UpdateFdSet(otSysMainloopContext &aMainloop) const
{
    FD_SET(mHidlFd, &aMainloop.mReadFdSet);
    FD_SET(mHidlFd, &aMainloop.mWriteFdSet);

    if (mHidlFd > aMainloop.mMaxFd)
    {
        aMainloop.mMaxFd = mHidlFd;
    }
}

void HidlAgent::Process(const otSysMainloopContext &aMainloop)
{
    if (FD_ISSET(mHidlFd, &aMainloop.mReadFdSet) || FD_ISSET(mHidlFd, &aMainloop.mWriteFdSet))
    {
        handleTransportPoll(mHidlFd);
    }
}
} // namespace Hidl
} // namespace otbr
