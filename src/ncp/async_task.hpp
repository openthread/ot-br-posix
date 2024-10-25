/*
 *  Copyright (c) 2024, The OpenThread Authors.
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
 *   This file includes definitions for chained async task.
 *   The utility class is used to support the usage of a `Then`-style chained async operations.
 */

#ifndef OTBR_AGENT_ASYNC_TASK_HPP_
#define OTBR_AGENT_ASYNC_TASK_HPP_

#include <functional>
#include <memory>

#include <openthread/error.h>

namespace otbr {
namespace Ncp {

class AsyncTask;
using AsyncTaskPtr = std::shared_ptr<AsyncTask>;

class AsyncTask
{
public:
    using ThenHandler   = std::function<void(AsyncTaskPtr)>;
    using ResultHandler = std::function<void(otError, const std::string &)>;

    /**
     * Constructor.
     *
     * @param[in]  The error handler called when the result is not OT_ERROR_NONE;
     */
    AsyncTask(const ResultHandler &aResultHandler);

    /**
     * Destructor.
     */
    ~AsyncTask(void);

    /**
     * Trigger the initial action of the chained async operations.
     *
     * This method should be called to trigger the chained async operations.
     */
    void Run(void);

    /**
     * Set the result of the previous async operation.
     *
     * This method should be called when the result of the previous async operation is ready.
     * This method will pass the result to next operation.
     *
     * @param[in] aError  The result for the previous async operation.
     */
    void SetResult(otError aError, const std::string &aErrorInfo);

    /**
     * Set the initial operation of the chained async operations.
     *
     * @param[in] aFirst  A reference to a function object for the initial action.
     *
     * @returns  A shared pointer to a AsyncTask object created in this method.
     */
    AsyncTaskPtr &First(const ThenHandler &aFirst);

    /**
     * Set the next operation of the chained async operations.
     *
     * @param[in] aThen  A reference to a function object for the next action.
     *
     * @returns A shared pointer to a AsyncTask object created in this method.
     */
    AsyncTaskPtr &Then(const ThenHandler &aThen);

private:
    union
    {
        ThenHandler   mThen;          // Only valid when `mNext` is not nullptr
        ResultHandler mResultHandler; // Only valid when `mNext` is nullptr
    };
    AsyncTaskPtr mNext;
};

} // namespace Ncp
} // namespace otbr

#endif // OTBR_AGENT_ASYNC_TASK_HPP_
