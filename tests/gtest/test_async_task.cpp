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

#include <gtest/gtest.h>

#include <memory>
#include <string>

#include <openthread/error.h>

#include "common/code_utils.hpp"
#include "ncp/async_task.hpp"

using otbr::Ncp::AsyncTask;
using otbr::Ncp::AsyncTaskPtr;

TEST(AsyncTask, TestOneStep)
{
    AsyncTaskPtr task;
    AsyncTaskPtr step1;
    int          resultHandlerCalledTimes = 0;
    int          stepCount                = 0;

    auto errorHandler = [&resultHandlerCalledTimes](otError aError, const std::string &aErrorInfo) {
        OTBR_UNUSED_VARIABLE(aError);
        OTBR_UNUSED_VARIABLE(aErrorInfo);

        resultHandlerCalledTimes++;
    };

    task = std::make_shared<AsyncTask>(errorHandler);
    task->First([&stepCount, &step1](AsyncTaskPtr aNext) {
        step1 = std::move(aNext);
        stepCount++;
    });
    task->Run();

    step1->SetResult(OT_ERROR_NONE, "Success");

    EXPECT_EQ(resultHandlerCalledTimes, 1);
    EXPECT_EQ(stepCount, 1);
}

TEST(AsyncTask, TestNoResultReturned)
{
    AsyncTaskPtr task;
    AsyncTaskPtr step1;
    AsyncTaskPtr step2;
    AsyncTaskPtr step3;

    int     resultHandlerCalledTimes = 0;
    int     stepCount                = 0;
    otError error                    = OT_ERROR_NONE;

    auto errorHandler = [&resultHandlerCalledTimes, &error](otError aError, const std::string &aErrorInfo) {
        OTBR_UNUSED_VARIABLE(aErrorInfo);

        resultHandlerCalledTimes++;
        error = aError;
    };

    task = std::make_shared<AsyncTask>(errorHandler);
    task->First([&stepCount, &step1](AsyncTaskPtr aNext) {
            step1 = std::move(aNext);
            stepCount++;
        })
        ->Then([&stepCount, &step2](AsyncTaskPtr aNext) {
            step2 = std::move(aNext);
            stepCount++;
        })
        ->Then([&stepCount, &step3](AsyncTaskPtr aNext) {
            step3 = std::move(aNext);
            stepCount++;
        });
    task->Run();

    // Asyn task ends without calling 'SetResult'.
    step1 = nullptr;
    task  = nullptr;

    EXPECT_EQ(resultHandlerCalledTimes, 1);
    EXPECT_EQ(stepCount, 1);
    EXPECT_EQ(error, OT_ERROR_FAILED);
}

TEST(AsyncTask, TestMultipleStepsSuccess)
{
    AsyncTaskPtr task;
    AsyncTaskPtr step1;
    AsyncTaskPtr step2;
    AsyncTaskPtr step3;

    int     resultHandlerCalledTimes = 0;
    int     stepCount                = 0;
    otError error                    = OT_ERROR_NONE;

    auto errorHandler = [&resultHandlerCalledTimes, &error](otError aError, const std::string &aErrorInfo) {
        OTBR_UNUSED_VARIABLE(aErrorInfo);

        resultHandlerCalledTimes++;
        error = aError;
    };

    task = std::make_shared<AsyncTask>(errorHandler);
    task->First([&stepCount, &step1](AsyncTaskPtr aNext) {
            step1 = std::move(aNext);
            stepCount++;
        })
        ->Then([&stepCount, &step2](AsyncTaskPtr aNext) {
            step2 = std::move(aNext);
            stepCount++;
        })
        ->Then([&stepCount, &step3](AsyncTaskPtr aNext) {
            step3 = std::move(aNext);
            stepCount++;
        });
    task->Run();

    EXPECT_EQ(stepCount, 1);
    step1->SetResult(OT_ERROR_NONE, "");
    EXPECT_EQ(resultHandlerCalledTimes, 0);

    EXPECT_EQ(stepCount, 2);
    step2->SetResult(OT_ERROR_NONE, "");
    EXPECT_EQ(resultHandlerCalledTimes, 0);

    EXPECT_EQ(stepCount, 3);
    error = OT_ERROR_GENERIC;
    step3->SetResult(OT_ERROR_NONE, "");
    EXPECT_EQ(resultHandlerCalledTimes, 1);
    EXPECT_EQ(error, OT_ERROR_NONE);
}

TEST(AsyncTask, TestMultipleStepsFailedHalfWay)
{
    AsyncTaskPtr task;
    AsyncTaskPtr step1;
    AsyncTaskPtr step2;
    AsyncTaskPtr step3;

    int     resultHandlerCalledTimes = 0;
    int     stepCount                = 0;
    otError error                    = OT_ERROR_NONE;

    auto errorHandler = [&resultHandlerCalledTimes, &error](otError aError, const std::string &aErrorInfo) {
        OTBR_UNUSED_VARIABLE(aErrorInfo);

        resultHandlerCalledTimes++;
        error = aError;
    };

    task = std::make_shared<AsyncTask>(errorHandler);
    task->First([&stepCount, &step1](AsyncTaskPtr aNext) {
            step1 = std::move(aNext);
            stepCount++;
        })
        ->Then([&stepCount, &step2](AsyncTaskPtr aNext) {
            step2 = std::move(aNext);
            stepCount++;
        })
        ->Then([&stepCount, &step3](AsyncTaskPtr aNext) {
            step3 = std::move(aNext);
            stepCount++;
        });
    task->Run();

    EXPECT_EQ(stepCount, 1);
    step1->SetResult(OT_ERROR_NONE, "");
    EXPECT_EQ(resultHandlerCalledTimes, 0);

    EXPECT_EQ(stepCount, 2);
    step2->SetResult(OT_ERROR_BUSY, "");
    EXPECT_EQ(resultHandlerCalledTimes, 1);
    EXPECT_EQ(error, OT_ERROR_BUSY);
}
