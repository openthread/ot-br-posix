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
#include <assert.h>

#include <memory>
#include <mutex>

#include "common/mainloop_manager.hpp"
#include "common/task_runner.hpp"

namespace otbr {

MainloopManager &MainloopManager::GetInstance(void)
{
    static MainloopManager sMainloopManager;

    return sMainloopManager;
}

void MainloopManager::AddMainloopProcessor(MainloopProcessor *aMainloopProcessor)
{
    assert(aMainloopProcessor != nullptr);
    mMainloopProcessorList.emplace_back(aMainloopProcessor);
}

void MainloopManager::RemoveMainloopProcessor(MainloopProcessor *aMainloopProcessor)
{
    mMainloopProcessorList.remove(aMainloopProcessor);
}

int MainloopManager::RunMainloop(Seconds aMaxPollTimeout)
{
    int rval = 0;

    mShouldBreak.store(false);

    {
        std::lock_guard<std::mutex> _(mBreakMainloopTaskMutex);

        mBreakMainloopTask = std::unique_ptr<TaskRunner>(new TaskRunner());
    }

    while (!mShouldBreak.load())
    {
        otbr::MainloopContext mainloop;

        mainloop.mMaxFd   = -1;
        mainloop.mTimeout = ToTimeval(aMaxPollTimeout);
        FD_ZERO(&mainloop.mReadFdSet);
        FD_ZERO(&mainloop.mWriteFdSet);
        FD_ZERO(&mainloop.mErrorFdSet);

        for (auto &mainloopProcessor : mMainloopProcessorList)
        {
            mainloopProcessor->Update(mainloop);
        }

        rval = select(mainloop.mMaxFd + 1, &mainloop.mReadFdSet, &mainloop.mWriteFdSet, &mainloop.mErrorFdSet,
                      &mainloop.mTimeout);
        if (rval == -1 && errno != EINTR)
        {
            break;
        }

        if (mShouldBreak.load())
        {
            rval = 0;
            break;
        }

        for (auto &mainloopProcessor : mMainloopProcessorList)
        {
            mainloopProcessor->Process(mainloop);
        }
    }

    // TODO(wgtdkp): We don't really need to free `mBreakMainloopTask` here
    // since it will be auto freed by the destructor of `MainloopManager`.
    // But somehow the memory leakage detector of the unit test framework
    // is complaining about unfreed `mBreakMainloopTask`. See example failure
    // report: https://github.com/openthread/ot-br-posix/runs/4345737895.
    {
        std::lock_guard<std::mutex> _(mBreakMainloopTaskMutex);

        mBreakMainloopTask = nullptr;
    }

    return rval;
}

void MainloopManager::BreakMainloop()
{
    mShouldBreak.store(true);

    {
        std::lock_guard<std::mutex> _(mBreakMainloopTaskMutex);

        if (mBreakMainloopTask != nullptr)
        {
            // Post a noop task to wake up the select() system call,
            // so that we can always break the mainloop.
            mBreakMainloopTask->Post([]() {});
        }
    }
}

} // namespace otbr
