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
    using namespace otbr;
    using namespace otbr::Ncp;

    Error                           error              = kErrorFailed;
    bool                            resultReceived     = false;
    ThreadEnabledState              threadEnabledState = ThreadEnabledState::kStateInvalid;
    MainloopContext                 mainloop;
    ThreadHost::AsyncResultReceiver receiver = [&resultReceived, &error](Error aError, const std::string &aErrorMsg) {
        OT_UNUSED_VARIABLE(aErrorMsg);
        resultReceived = true;
        error          = aError;
    };
    ThreadHost::ThreadEnabledStateCallback enabledStateCallback = [&threadEnabledState](ThreadEnabledState aState) {
        threadEnabledState = aState;
    };
    RcpHost host("wpan0", std::vector<const char *>(), /* aBackboneInterfaceName */ "", /* aDryRun */ false,
                 /* aEnableAutoAttach */ false);

    host.Init();
    host.AddThreadEnabledStateChangedCallback(enabledStateCallback);

    // 1. Active dataset hasn't been set, should succeed with device role still being disabled.
    host.SetThreadEnabled(true, receiver);
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 1, [&resultReceived]() { return resultReceived; });
    EXPECT_EQ(error, kErrorNone);
    EXPECT_EQ(host.GetDeviceRole(), OT_DEVICE_ROLE_DISABLED);
    EXPECT_EQ(threadEnabledState, ThreadEnabledState::kStateEnabled);

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
    error          = kErrorFailed;
    resultReceived = false;
    host.SetThreadEnabled(true, receiver);
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 1, [&resultReceived]() { return resultReceived; });
    EXPECT_EQ(error, kErrorNone);
    EXPECT_EQ(threadEnabledState, ThreadEnabledState::kStateEnabled);

    // 4. Disable it
    error          = kErrorFailed;
    resultReceived = false;
    host.SetThreadEnabled(false, receiver);
    EXPECT_EQ(threadEnabledState, ThreadEnabledState::kStateDisabling);
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 1, [&resultReceived]() { return resultReceived; });
    EXPECT_EQ(error, kErrorNone);
    EXPECT_EQ(host.GetDeviceRole(), OT_DEVICE_ROLE_DISABLED);
    EXPECT_EQ(threadEnabledState, ThreadEnabledState::kStateDisabled);

    // 5. Duplicate call, should get kErrorBusy
    error                 = kErrorFailed;
    resultReceived        = false;
    Error error2          = kErrorFailed;
    bool  resultReceived2 = false;
    host.SetThreadEnabled(false, receiver);
    host.SetThreadEnabled(false, [&resultReceived2, &error2](Error aError, const std::string &aErrorMsg) {
        OT_UNUSED_VARIABLE(aErrorMsg);
        error2          = aError;
        resultReceived2 = true;
    });
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 1,
                         [&resultReceived, &resultReceived2]() { return resultReceived && resultReceived2; });
    EXPECT_EQ(error, kErrorNone);
    EXPECT_EQ(error2, kErrorBusy);
    EXPECT_EQ(threadEnabledState, ThreadEnabledState::kStateDisabled);

    host.Deinit();
}

TEST(RcpHostApi, SetCountryCodeWorkCorrectly)
{
    using namespace otbr;
    using namespace otbr::Ncp;

    Error                           error          = kErrorFailed;
    bool                            resultReceived = false;
    MainloopContext                 mainloop;
    ThreadHost::AsyncResultReceiver receiver = [&resultReceived, &error](Error aError, const std::string &aErrorMsg) {
        OT_UNUSED_VARIABLE(aErrorMsg);
        resultReceived = true;
        error          = aError;
    };
    RcpHost host("wpan0", std::vector<const char *>(), /* aBackboneInterfaceName */ "", /* aDryRun */ false,
                 /* aEnableAutoAttach */ false);

    // 1. Call SetCountryCode when host hasn't been initialized.
    MainloopManager::GetInstance().RemoveMainloopProcessor(
        &host); // Temporarily remove RcpHost because it's not initialized yet.
    host.SetCountryCode("AF", receiver);
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 0, [&resultReceived]() { return resultReceived; });
    EXPECT_EQ(error, kErrorInvalidState);
    MainloopManager::GetInstance().AddMainloopProcessor(&host);

    host.Init();
    // 2. Call SetCountryCode with invalid arguments
    resultReceived = false;
    error          = kErrorNone;
    host.SetCountryCode("AFA", receiver);
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 0, [&resultReceived]() { return resultReceived; });
    EXPECT_EQ(error, kErrorInvalidArgs);

    resultReceived = false;
    error          = kErrorNone;
    host.SetCountryCode("A", receiver);
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 0, [&resultReceived]() { return resultReceived; });
    EXPECT_EQ(error, kErrorInvalidArgs);

    resultReceived = false;
    error          = kErrorNone;
    host.SetCountryCode("12", receiver);
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 0, [&resultReceived]() { return resultReceived; });
    EXPECT_EQ(error, kErrorInvalidArgs);

    // 3. Call SetCountryCode with valid argument
    resultReceived = false;
    error          = kErrorNone;
    host.SetCountryCode("AF", receiver);
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 0, [&resultReceived]() { return resultReceived; });
    EXPECT_EQ(error, OT_ERROR_NOT_IMPLEMENTED); // The current platform weak implmentation returns 'NOT_IMPLEMENTED'.

    host.Deinit();
}

TEST(RcpHostApi, StateChangesCorrectlyAfterLeave)
{
    using namespace otbr;
    using namespace otbr::Ncp;

    Error                           error          = kErrorNone;
    std::string                     errorMsg       = "";
    bool                            resultReceived = false;
    MainloopContext                 mainloop;
    ThreadHost::AsyncResultReceiver receiver = [&resultReceived, &error, &errorMsg](Error              aError,
                                                                                    const std::string &aErrorMsg) {
        resultReceived = true;
        error          = aError;
        errorMsg       = aErrorMsg;
    };

    RcpHost host("wpan0", std::vector<const char *>(), /* aBackboneInterfaceName */ "", /* aDryRun */ false,
                 /* aEnableAutoAttach */ false);

    // 1. Call Leave when host hasn't been initialized.
    MainloopManager::GetInstance().RemoveMainloopProcessor(
        &host); // Temporarily remove RcpHost because it's not initialized yet.
    host.Leave(/* aEraseDataset */ true, receiver);
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 0, [&resultReceived]() { return resultReceived; });
    EXPECT_EQ(error, kErrorInvalidState);
    EXPECT_STREQ(errorMsg.c_str(), "OT is not initialized");
    MainloopManager::GetInstance().AddMainloopProcessor(&host);

    host.Init();

    // 2. Call Leave when disabling Thread.
    error          = kErrorNone;
    resultReceived = false;
    host.SetThreadEnabled(false, nullptr);
    host.Leave(/* aEraseDataset */ true, receiver);
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 0, [&resultReceived]() { return resultReceived; });
    EXPECT_EQ(error, kErrorBusy);
    EXPECT_STREQ(errorMsg.c_str(), "Thread is disabling");

    // 3. Call Leave when Thread is disabled.
    error          = kErrorNone;
    resultReceived = false;
    otOperationalDataset     dataset;
    otOperationalDatasetTlvs datasetTlvs;
    OT_UNUSED_VARIABLE(otDatasetCreateNewNetwork(ot::FakePlatform::CurrentInstance(), &dataset));
    otDatasetConvertToTlvs(&dataset, &datasetTlvs);
    OT_UNUSED_VARIABLE(otDatasetSetActiveTlvs(ot::FakePlatform::CurrentInstance(), &datasetTlvs));
    host.Leave(/* aEraseDataset */ true, receiver);
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 0, [&resultReceived]() { return resultReceived; });
    EXPECT_EQ(error, kErrorNone);

    error = otDatasetGetActive(ot::FakePlatform::CurrentInstance(), &dataset);
    EXPECT_EQ(error, OT_ERROR_NOT_FOUND);

    // 4. Call Leave when Thread is enabled.
    error          = kErrorNone;
    resultReceived = false;
    OT_UNUSED_VARIABLE(otDatasetSetActiveTlvs(ot::FakePlatform::CurrentInstance(), &datasetTlvs));
    host.SetThreadEnabled(true, nullptr);
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 1,
                         [&host]() { return host.GetDeviceRole() != OT_DEVICE_ROLE_DETACHED; });
    EXPECT_EQ(host.GetDeviceRole(), OT_DEVICE_ROLE_LEADER);
    host.Leave(/* aEraseDataset */ false, receiver);
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 0, [&resultReceived]() { return resultReceived; });
    EXPECT_EQ(error, kErrorNone);

    error = otDatasetGetActive(ot::FakePlatform::CurrentInstance(), &dataset); // Dataset should still be there.
    EXPECT_EQ(error, kErrorNone);

    host.Deinit();
}

TEST(RcpHostApi, StateChangesCorrectlyAfterScheduleMigration)
{
    using namespace otbr;
    using namespace otbr::Ncp;

    Error                           error          = kErrorNone;
    std::string                     errorMsg       = "";
    bool                            resultReceived = false;
    MainloopContext                 mainloop;
    ThreadHost::AsyncResultReceiver receiver = [&resultReceived, &error, &errorMsg](Error              aError,
                                                                                    const std::string &aErrorMsg) {
        resultReceived = true;
        error          = aError;
        errorMsg       = aErrorMsg;
    };
    RcpHost host("wpan0", std::vector<const char *>(), /* aBackboneInterfaceName */ "", /* aDryRun */ false,
                 /* aEnableAutoAttach */ false);

    otOperationalDataset     dataset;
    otOperationalDatasetTlvs datasetTlvs;

    // 1. Call ScheduleMigration when host hasn't been initialized.
    MainloopManager::GetInstance().RemoveMainloopProcessor(
        &host); // Temporarily remove RcpHost because it's not initialized yet.
    host.ScheduleMigration(datasetTlvs, receiver);
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 0, [&resultReceived]() { return resultReceived; });
    EXPECT_EQ(error, kErrorInvalidState);
    EXPECT_STREQ(errorMsg.c_str(), "OT is not initialized");
    MainloopManager::GetInstance().AddMainloopProcessor(&host);

    host.Init();

    // 2. Call ScheduleMigration when the Thread is not enabled.
    error          = kErrorNone;
    resultReceived = false;
    host.ScheduleMigration(datasetTlvs, receiver);
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 0, [&resultReceived]() { return resultReceived; });
    EXPECT_EQ(error, kErrorInvalidState);
    EXPECT_STREQ(errorMsg.c_str(), "Thread is disabled");

    // 3. Schedule migration to another network.
    OT_UNUSED_VARIABLE(otDatasetCreateNewNetwork(ot::FakePlatform::CurrentInstance(), &dataset));
    otDatasetConvertToTlvs(&dataset, &datasetTlvs);
    OT_UNUSED_VARIABLE(otDatasetSetActiveTlvs(ot::FakePlatform::CurrentInstance(), &datasetTlvs));
    error          = kErrorNone;
    resultReceived = false;
    host.SetThreadEnabled(true, receiver);
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 1,
                         [&host]() { return host.GetDeviceRole() != OT_DEVICE_ROLE_DETACHED; });
    EXPECT_EQ(host.GetDeviceRole(), OT_DEVICE_ROLE_LEADER);

    host.ScheduleMigration(datasetTlvs, receiver);
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 0, [&resultReceived]() { return resultReceived; });
    EXPECT_EQ(error, kErrorNone);

    host.Deinit();
}
