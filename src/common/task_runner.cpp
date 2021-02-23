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

/**
 * @file
 * This file implements the Task Runner that executes tasks on the mainloop.
 */

#include "common/task_runner.hpp"

#include <algorithm>

#include <fcntl.h>
#include <unistd.h>

#include "common/code_utils.hpp"

namespace otbr {

TaskRunner::TaskRunner(void)
{
    int flags;

    // We do not handle failures when creating a pipe, simply die.
    VerifyOrDie(pipe(mEventFd) != -1, strerror(errno));

    flags = fcntl(mEventFd[kRead], F_GETFL, 0);
    VerifyOrDie(fcntl(mEventFd[kRead], F_SETFL, flags | O_NONBLOCK) != -1, strerror(errno));
    flags = fcntl(mEventFd[kWrite], F_GETFL, 0);
    VerifyOrDie(fcntl(mEventFd[kWrite], F_SETFL, flags | O_NONBLOCK) != -1, strerror(errno));
}

TaskRunner::~TaskRunner(void)
{
    if (mEventFd[kRead] != -1)
    {
        close(mEventFd[kRead]);
        mEventFd[kRead] = -1;
    }
    if (mEventFd[kWrite] != -1)
    {
        close(mEventFd[kWrite]);
        mEventFd[kWrite] = -1;
    }
}

void TaskRunner::Post(const Task<void> &aTask)
{
    PushTask(aTask);
}

void TaskRunner::UpdateFdSet(otSysMainloopContext &aMainloop)
{
    FD_SET(mEventFd[kRead], &aMainloop.mReadFdSet);
    aMainloop.mMaxFd = std::max(mEventFd[kRead], aMainloop.mMaxFd);
}

void TaskRunner::Process(const otSysMainloopContext &aMainloop)
{
    if (FD_ISSET(mEventFd[kRead], &aMainloop.mReadFdSet))
    {
        uint8_t n;

        // Read any data in the pipe.
        while (read(mEventFd[kRead], &n, sizeof(n)) == sizeof(n))
        {
        }

        PopTasks();
    }
}

void TaskRunner::PushTask(const Task<void> &aTask)
{
    ssize_t                     rval;
    const uint8_t               kOne = 1;
    std::lock_guard<std::mutex> _(mTaskQueueMutex);

    mTaskQueue.emplace(aTask);
    do
    {
        rval = write(mEventFd[kWrite], &kOne, sizeof(kOne));
    } while (rval == -1 && errno == EINTR);

    VerifyOrExit(rval == -1);

    // Critical error happens, simply die.
    VerifyOrDie(errno == EAGAIN || errno == EWOULDBLOCK, strerror(errno));

    // We are blocked because there are already data (written by other concurrent callers in
    // different threads) in the pipe, and the mEventFd[kRead] should be readable now.
    otbrLog(OTBR_LOG_WARNING, "failed to write fd %d: %s", mEventFd[kWrite], strerror(errno));

exit:
    return;
}

void TaskRunner::PopTasks(void)
{
    while (true)
    {
        Task<void> task;

        // The braces here are necessary for auto-releasing of the mutex.
        {
            std::lock_guard<std::mutex> _(mTaskQueueMutex);

            if (!mTaskQueue.empty())
            {
                task = std::move(mTaskQueue.front());
                mTaskQueue.pop();
            }
            else
            {
                break;
            }
        }

        task();
    }
}

} // namespace otbr
