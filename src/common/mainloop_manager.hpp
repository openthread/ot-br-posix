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
 *   This file includes definitions for mainloop manager.
 */

#ifndef OTBR_COMMON_MAINLOOP_MANAGER_HPP_
#define OTBR_COMMON_MAINLOOP_MANAGER_HPP_

#include <openthread-br/config.h>

#include <openthread/openthread-system.h>

#include <atomic>
#include <list>
#include <memory>
#include <mutex>

#include "common/mainloop.hpp"
#include "common/task_runner.hpp"
#include "common/time.hpp"
#include "ncp/ncp_openthread.hpp"

namespace otbr {

/**
 * This class implements the mainloop manager.
 *
 */
class MainloopManager
{
public:
    MainloopManager()  = default;
    ~MainloopManager() = default;

    /**
     * This method returns the singleton instance of the mainloop manager.
     *
     */
    static MainloopManager &GetInstance(void);

    /**
     * This method adds a mainloop processors to the mainloop managger.
     *
     * @param[in] aMainloopProcessor  A pointer to the mainloop processor.
     *
     */
    void AddMainloopProcessor(MainloopProcessor *aMainloopProcessor);

    /**
     * This method removes a mainloop processors from the mainloop managger.
     *
     * @param[in] aMainloopProcessor  A pointer to the mainloop processor.
     *
     */
    void RemoveMainloopProcessor(MainloopProcessor *aMainloopProcessor);

    /**
     * Runs the mainloop and blocks current thread until unrecoverable errors
     * are encountered or `BreakMainloop()` is invoked.
     *
     * @param[in] aMaxPollTimeout  The maximum polling timeout value for one iteration.
     *
     * @returns 0 if this method is stopped by `BreakMainloop()`, -1 if the the system
     *          function `select()` fails and `errno` is set to indicate the error. Note
     *          that EINTR is captured/tolerated by this method.
     *
     */
    int RunMainloop(Seconds aMaxPollTimeout = Seconds(10));

    /**
     * Force breaks function `RunMainloop()`. It's safe to call this method from multiple
     * thread simultaneously.
     *
     * Typical usages are breaking the mainloop from signal handlers or terminating the
     * mainloop after a given delay in unit tests. For example, run the mainloop for 3
     * seconds:
     * @code {.cpp}
     * TaskRunner taskRunner;
     *
     * taskRunner.Post(Seconds(3), []() { MainloopManager::GetInstance().BreakMainloop(); });
     * MainloopManager::GetInstance().RunMainloop();
     * @endcode
     *
     */
    void BreakMainloop();

private:
    std::list<MainloopProcessor *> mMainloopProcessorList;
    std::atomic<bool>              mShouldBreak{false};
    std::mutex                     mBreakMainloopTaskMutex;
    std::unique_ptr<TaskRunner>    mBreakMainloopTask;
};
} // namespace otbr
#endif // OTBR_COMMON_MAINLOOP_MANAGER_HPP_
