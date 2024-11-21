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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <sys/time.h>

#include <openthread/dataset.h>
#include <openthread/dataset_ftd.h>

#include "common/mainloop.hpp"
#include "common/mainloop_manager.hpp"
#include "ncp/rcp_host.hpp"

#include "fake_platform.hpp"

static void MainloopProcessUntil(otbr::MainloopContext    &aMainloop,
                                 uint32_t                  aTimeoutSec,
                                 std::function<bool(void)> aCondition)
{
    timeval startTime;
    timeval now;
    gettimeofday(&startTime, nullptr);

    while (!aCondition())
    {
        gettimeofday(&now, nullptr);
        // Simply compare the second. We don't need high precision here.
        if (now.tv_sec - startTime.tv_sec > aTimeoutSec)
        {
            break;
        }

        otbr::MainloopManager::GetInstance().Update(aMainloop);
        otbr::MainloopManager::GetInstance().Process(aMainloop);
    }
}

TEST(RcpHostApi, DeviceRoleChangesCorrectlyAfterSetThreadEnabled)
{
    otError                                    error              = OT_ERROR_FAILED;
    bool                                       resultReceived     = false;
    otbr::Ncp::ThreadEnabledState              threadEnabledState = otbr::Ncp::ThreadEnabledState::kStateInvalid;
    otbr::MainloopContext                      mainloop;
    otbr::Ncp::ThreadHost::AsyncResultReceiver receiver = [&resultReceived, &error](otError            aError,
                                                                                    const std::string &aErrorMsg) {
        OT_UNUSED_VARIABLE(aErrorMsg);
        resultReceived = true;
        error          = aError;
    };
    otbr::Ncp::ThreadHost::ThreadEnabledStateCallback enabledStateCallback =
        [&threadEnabledState](otbr::Ncp::ThreadEnabledState aState) { threadEnabledState = aState; };
    otbr::Ncp::RcpHost host("wpan0", std::vector<const char *>(), /* aBackboneInterfaceName */ "", /* aDryRun */ false,
                            /* aEnableAutoAttach */ false);

    host.Init();
    host.AddThreadEnabledStateChangedCallback(enabledStateCallback);

    // 1. Active dataset hasn't been set, should succeed with device role still being disabled.
    host.SetThreadEnabled(true, receiver);
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 1, [&resultReceived]() { return resultReceived; });
    EXPECT_EQ(error, OT_ERROR_NONE);
    EXPECT_EQ(host.GetDeviceRole(), OT_DEVICE_ROLE_DISABLED);
    EXPECT_EQ(threadEnabledState, otbr::Ncp::ThreadEnabledState::kStateEnabled);

    // 2. Set active dataset and start it
    {
        otOperationalDataset     dataset;
        otOperationalDatasetTlvs datasetTlvs;
        OT_UNUSED_VARIABLE(otDatasetCreateNewNetwork(ot::FakePlatform::CurrentInstance(), &dataset));
        otDatasetConvertToTlvs(&dataset, &datasetTlvs);
        OT_UNUSED_VARIABLE(otDatasetSetActiveTlvs(ot::FakePlatform::CurrentInstance(), &datasetTlvs));
    }
    OT_UNUSED_VARIABLE(otIp6SetEnabled(ot::FakePlatform::CurrentInstance(), true));
    OT_UNUSED_VARIABLE(otThreadSetEnabled(ot::FakePlatform::CurrentInstance(), true));

    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 1,
                         [&host]() { return host.GetDeviceRole() != OT_DEVICE_ROLE_DETACHED; });
    EXPECT_EQ(host.GetDeviceRole(), OT_DEVICE_ROLE_LEADER);

    // 3. Enable again, the enabled state should not change.
    error          = OT_ERROR_FAILED;
    resultReceived = false;
    host.SetThreadEnabled(true, receiver);
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 1, [&resultReceived]() { return resultReceived; });
    EXPECT_EQ(error, OT_ERROR_NONE);
    EXPECT_EQ(threadEnabledState, otbr::Ncp::ThreadEnabledState::kStateEnabled);

    // 4. Disable it
    error          = OT_ERROR_FAILED;
    resultReceived = false;
    host.SetThreadEnabled(false, receiver);
    EXPECT_EQ(threadEnabledState, otbr::Ncp::ThreadEnabledState::kStateDisabling);
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 1, [&resultReceived]() { return resultReceived; });
    EXPECT_EQ(error, OT_ERROR_NONE);
    EXPECT_EQ(host.GetDeviceRole(), OT_DEVICE_ROLE_DISABLED);
    EXPECT_EQ(threadEnabledState, otbr::Ncp::ThreadEnabledState::kStateDisabled);

    // 5. Duplicate call, should get OT_ERROR_BUSY
    error                   = OT_ERROR_FAILED;
    resultReceived          = false;
    otError error2          = OT_ERROR_FAILED;
    bool    resultReceived2 = false;
    host.SetThreadEnabled(false, receiver);
    host.SetThreadEnabled(false, [&resultReceived2, &error2](otError aError, const std::string &aErrorMsg) {
        OT_UNUSED_VARIABLE(aErrorMsg);
        error2          = aError;
        resultReceived2 = true;
    });
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 1,
                         [&resultReceived, &resultReceived2]() { return resultReceived && resultReceived2; });
    EXPECT_EQ(error, OT_ERROR_NONE);
    EXPECT_EQ(error2, OT_ERROR_BUSY);
    EXPECT_EQ(threadEnabledState, otbr::Ncp::ThreadEnabledState::kStateDisabled);

    host.Deinit();
}

TEST(RcpHostApi, SetCountryCodeWorkCorrectly)
{
    otError                                    error          = OT_ERROR_FAILED;
    bool                                       resultReceived = false;
    otbr::MainloopContext                      mainloop;
    otbr::Ncp::ThreadHost::AsyncResultReceiver receiver = [&resultReceived, &error](otError            aError,
                                                                                    const std::string &aErrorMsg) {
        OT_UNUSED_VARIABLE(aErrorMsg);
        resultReceived = true;
        error          = aError;
    };
    otbr::Ncp::RcpHost host("wpan0", std::vector<const char *>(), /* aBackboneInterfaceName */ "", /* aDryRun */ false,
                            /* aEnableAutoAttach */ false);

    // 1. Call SetCountryCode when host hasn't been initialized.
    otbr::MainloopManager::GetInstance().RemoveMainloopProcessor(
        &host); // Temporarily remove RcpHost because it's not initialized yet.
    host.SetCountryCode("AF", receiver);
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 0, [&resultReceived]() { return resultReceived; });
    EXPECT_EQ(error, OT_ERROR_INVALID_STATE);
    otbr::MainloopManager::GetInstance().AddMainloopProcessor(&host);

    host.Init();
    // 2. Call SetCountryCode with invalid arguments
    resultReceived = false;
    error          = OT_ERROR_NONE;
    host.SetCountryCode("AFA", receiver);
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 0, [&resultReceived]() { return resultReceived; });
    EXPECT_EQ(error, OT_ERROR_INVALID_ARGS);

    resultReceived = false;
    error          = OT_ERROR_NONE;
    host.SetCountryCode("A", receiver);
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 0, [&resultReceived]() { return resultReceived; });
    EXPECT_EQ(error, OT_ERROR_INVALID_ARGS);

    resultReceived = false;
    error          = OT_ERROR_NONE;
    host.SetCountryCode("12", receiver);
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 0, [&resultReceived]() { return resultReceived; });
    EXPECT_EQ(error, OT_ERROR_INVALID_ARGS);

    // 3. Call SetCountryCode with valid argument
    resultReceived = false;
    error          = OT_ERROR_NONE;
    host.SetCountryCode("AF", receiver);
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 0, [&resultReceived]() { return resultReceived; });
    EXPECT_EQ(error, OT_ERROR_NOT_IMPLEMENTED); // The current platform weak implmentation returns 'NOT_IMPLEMENTED'.

    host.Deinit();
}

TEST(RcpHostApi, StateChangesCorrectlyAfterScheduleMigration)
{
    otError                                    error          = OT_ERROR_NONE;
    std::string                                errorMsg       = "";
    bool                                       resultReceived = false;
    otbr::MainloopContext                      mainloop;
    otbr::Ncp::ThreadHost::AsyncResultReceiver receiver = [&resultReceived, &error,
                                                           &errorMsg](otError aError, const std::string &aErrorMsg) {
        resultReceived = true;
        error          = aError;
        errorMsg       = aErrorMsg;
    };
    otbr::Ncp::RcpHost host("wpan0", std::vector<const char *>(), /* aBackboneInterfaceName */ "", /* aDryRun */ false,
                            /* aEnableAutoAttach */ false);

    otOperationalDataset     dataset;
    otOperationalDatasetTlvs datasetTlvs;

    // 1. Call ScheduleMigration when host hasn't been initialized.
    otbr::MainloopManager::GetInstance().RemoveMainloopProcessor(
        &host); // Temporarily remove RcpHost because it's not initialized yet.
    host.ScheduleMigration(datasetTlvs, receiver);
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 0, [&resultReceived]() { return resultReceived; });
    EXPECT_EQ(error, OT_ERROR_INVALID_STATE);
    EXPECT_STREQ(errorMsg.c_str(), "OT is not initialized");
    otbr::MainloopManager::GetInstance().AddMainloopProcessor(&host);

    host.Init();

    // 2. Call ScheduleMigration when the Thread is not enabled.
    error          = OT_ERROR_NONE;
    resultReceived = false;
    host.ScheduleMigration(datasetTlvs, receiver);
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 0, [&resultReceived]() { return resultReceived; });
    EXPECT_EQ(error, OT_ERROR_INVALID_STATE);
    EXPECT_STREQ(errorMsg.c_str(), "Thread is disabled");

    // 3. Schedule migration to another network.
    OT_UNUSED_VARIABLE(otDatasetCreateNewNetwork(ot::FakePlatform::CurrentInstance(), &dataset));
    otDatasetConvertToTlvs(&dataset, &datasetTlvs);
    OT_UNUSED_VARIABLE(otDatasetSetActiveTlvs(ot::FakePlatform::CurrentInstance(), &datasetTlvs));
    error          = OT_ERROR_NONE;
    resultReceived = false;
    host.SetThreadEnabled(true, receiver);
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 1,
                         [&host]() { return host.GetDeviceRole() != OT_DEVICE_ROLE_DETACHED; });
    EXPECT_EQ(host.GetDeviceRole(), OT_DEVICE_ROLE_LEADER);

    host.ScheduleMigration(datasetTlvs, receiver);
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 0, [&resultReceived]() { return resultReceived; });
    EXPECT_EQ(error, OT_ERROR_NONE);

    host.Deinit();
}
