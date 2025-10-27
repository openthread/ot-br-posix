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
#include "host/rcp_host.hpp"

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
    otError                                     error              = OT_ERROR_FAILED;
    bool                                        resultReceived     = false;
    otbr::Host::ThreadEnabledState              threadEnabledState = otbr::Host::ThreadEnabledState::kStateInvalid;
    otbr::MainloopContext                       mainloop;
    otbr::Host::ThreadHost::AsyncResultReceiver receiver = [&resultReceived, &error](otError            aError,
                                                                                     const std::string &aErrorMsg) {
        OT_UNUSED_VARIABLE(aErrorMsg);
        resultReceived = true;
        error          = aError;
    };
    otbr::Host::ThreadHost::ThreadEnabledStateCallback enabledStateCallback =
        [&threadEnabledState](otbr::Host::ThreadEnabledState aState) { threadEnabledState = aState; };
    otbr::Host::RcpHost host("wpan0", std::vector<const char *>(), /* aBackboneInterfaceName */ "", /* aDryRun */ false,
                             /* aEnableAutoAttach */ false);

    host.Init();
    host.AddThreadEnabledStateChangedCallback(enabledStateCallback);

    // 1. Active dataset hasn't been set, should succeed with device role still being disabled.
    host.SetThreadEnabled(true, receiver);
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 1, [&resultReceived]() { return resultReceived; });
    EXPECT_EQ(error, OT_ERROR_NONE);
    EXPECT_EQ(host.GetDeviceRole(), OT_DEVICE_ROLE_DISABLED);
    EXPECT_EQ(threadEnabledState, otbr::Host::ThreadEnabledState::kStateEnabled);

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
    EXPECT_EQ(threadEnabledState, otbr::Host::ThreadEnabledState::kStateEnabled);

    // 4. Disable it
    error          = OT_ERROR_FAILED;
    resultReceived = false;
    host.SetThreadEnabled(false, receiver);
    EXPECT_EQ(threadEnabledState, otbr::Host::ThreadEnabledState::kStateDisabling);
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 1, [&resultReceived]() { return resultReceived; });
    EXPECT_EQ(error, OT_ERROR_NONE);
    EXPECT_EQ(host.GetDeviceRole(), OT_DEVICE_ROLE_DISABLED);
    EXPECT_EQ(threadEnabledState, otbr::Host::ThreadEnabledState::kStateDisabled);

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
    EXPECT_EQ(threadEnabledState, otbr::Host::ThreadEnabledState::kStateDisabled);

    host.Deinit();
}

TEST(RcpHostApi, SetCountryCodeWorkCorrectly)
{
    otError                                     error          = OT_ERROR_FAILED;
    bool                                        resultReceived = false;
    otbr::MainloopContext                       mainloop;
    otbr::Host::ThreadHost::AsyncResultReceiver receiver = [&resultReceived, &error](otError            aError,
                                                                                     const std::string &aErrorMsg) {
        OT_UNUSED_VARIABLE(aErrorMsg);
        resultReceived = true;
        error          = aError;
    };
    otbr::Host::RcpHost host("wpan0", std::vector<const char *>(), /* aBackboneInterfaceName */ "", /* aDryRun */ false,
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

TEST(RcpHostApi, StateChangesCorrectlyAfterLeave)
{
    otError                                     error          = OT_ERROR_NONE;
    std::string                                 errorMsg       = "";
    bool                                        resultReceived = false;
    otbr::MainloopContext                       mainloop;
    otbr::Host::ThreadHost::AsyncResultReceiver receiver = [&resultReceived, &error,
                                                            &errorMsg](otError aError, const std::string &aErrorMsg) {
        resultReceived = true;
        error          = aError;
        errorMsg       = aErrorMsg;
    };

    otbr::Host::RcpHost host("wpan0", std::vector<const char *>(), /* aBackboneInterfaceName */ "", /* aDryRun */ false,
                             /* aEnableAutoAttach */ false);

    // 1. Call Leave when host hasn't been initialized.
    otbr::MainloopManager::GetInstance().RemoveMainloopProcessor(
        &host); // Temporarily remove RcpHost because it's not initialized yet.
    host.Leave(/* aEraseDataset */ true, receiver);
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 0, [&resultReceived]() { return resultReceived; });
    EXPECT_EQ(error, OT_ERROR_INVALID_STATE);
    EXPECT_STREQ(errorMsg.c_str(), "OT is not initialized");
    otbr::MainloopManager::GetInstance().AddMainloopProcessor(&host);

    host.Init();

    // 2. Call Leave when disabling Thread.
    error          = OT_ERROR_NONE;
    resultReceived = false;
    host.SetThreadEnabled(false, nullptr);
    host.Leave(/* aEraseDataset */ true, receiver);
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 0, [&resultReceived]() { return resultReceived; });
    EXPECT_EQ(error, OT_ERROR_BUSY);
    EXPECT_STREQ(errorMsg.c_str(), "Thread is disabling");

    // 3. Call Leave when Thread is disabled.
    error          = OT_ERROR_NONE;
    resultReceived = false;
    otOperationalDataset     dataset;
    otOperationalDatasetTlvs datasetTlvs;
    OT_UNUSED_VARIABLE(otDatasetCreateNewNetwork(ot::FakePlatform::CurrentInstance(), &dataset));
    otDatasetConvertToTlvs(&dataset, &datasetTlvs);
    OT_UNUSED_VARIABLE(otDatasetSetActiveTlvs(ot::FakePlatform::CurrentInstance(), &datasetTlvs));
    host.Leave(/* aEraseDataset */ true, receiver);
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 0, [&resultReceived]() { return resultReceived; });
    EXPECT_EQ(error, OT_ERROR_NONE);

    error = otDatasetGetActive(ot::FakePlatform::CurrentInstance(), &dataset);
    EXPECT_EQ(error, OT_ERROR_NOT_FOUND);

    // 4. Call Leave when Thread is enabled.
    error          = OT_ERROR_NONE;
    resultReceived = false;
    OT_UNUSED_VARIABLE(otDatasetSetActiveTlvs(ot::FakePlatform::CurrentInstance(), &datasetTlvs));
    host.SetThreadEnabled(true, nullptr);
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 1,
                         [&host]() { return host.GetDeviceRole() != OT_DEVICE_ROLE_DETACHED; });
    EXPECT_EQ(host.GetDeviceRole(), OT_DEVICE_ROLE_LEADER);
    host.Leave(/* aEraseDataset */ false, receiver);
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 0, [&resultReceived]() { return resultReceived; });
    EXPECT_EQ(error, OT_ERROR_NONE);

    error = otDatasetGetActive(ot::FakePlatform::CurrentInstance(), &dataset); // Dataset should still be there.
    EXPECT_EQ(error, OT_ERROR_NONE);

    host.Deinit();
}

TEST(RcpHostApi, StateChangesCorrectlyAfterScheduleMigration)
{
    otError                                     error          = OT_ERROR_NONE;
    std::string                                 errorMsg       = "";
    bool                                        resultReceived = false;
    otbr::MainloopContext                       mainloop;
    otbr::Host::ThreadHost::AsyncResultReceiver receiver = [&resultReceived, &error,
                                                            &errorMsg](otError aError, const std::string &aErrorMsg) {
        resultReceived = true;
        error          = aError;
        errorMsg       = aErrorMsg;
    };
    otbr::Host::RcpHost host("wpan0", std::vector<const char *>(), /* aBackboneInterfaceName */ "", /* aDryRun */ false,
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

TEST(RcpHostApi, StateChangesCorrectlyAfterJoin)
{
    otError                                     error           = OT_ERROR_NONE;
    otError                                     error_          = OT_ERROR_NONE;
    std::string                                 errorMsg        = "";
    std::string                                 errorMsg_       = "";
    bool                                        resultReceived  = false;
    bool                                        resultReceived_ = false;
    otbr::MainloopContext                       mainloop;
    otbr::Host::ThreadHost::AsyncResultReceiver receiver = [&resultReceived, &error,
                                                            &errorMsg](otError aError, const std::string &aErrorMsg) {
        resultReceived = true;
        error          = aError;
        errorMsg       = aErrorMsg;
    };
    otbr::Host::ThreadHost::AsyncResultReceiver receiver_ = [&resultReceived_, &error_,
                                                             &errorMsg_](otError aError, const std::string &aErrorMsg) {
        resultReceived_ = true;
        error_          = aError;
        errorMsg_       = aErrorMsg;
    };
    otbr::Host::RcpHost host("wpan0", std::vector<const char *>(), /* aBackboneInterfaceName */ "", /* aDryRun */ false,
                             /* aEnableAutoAttach */ false);

    otOperationalDataset dataset;
    (void)dataset;
    otOperationalDatasetTlvs datasetTlvs;

    // 1. Call Join when host hasn't been initialized.
    otbr::MainloopManager::GetInstance().RemoveMainloopProcessor(
        &host); // Temporarily remove RcpHost because it's not initialized yet.
    host.Join(datasetTlvs, receiver);
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 0, [&resultReceived]() { return resultReceived; });
    EXPECT_EQ(error, OT_ERROR_INVALID_STATE);
    EXPECT_STREQ(errorMsg.c_str(), "OT is not initialized");
    otbr::MainloopManager::GetInstance().AddMainloopProcessor(&host);

    host.Init();
    OT_UNUSED_VARIABLE(otDatasetCreateNewNetwork(ot::FakePlatform::CurrentInstance(), &dataset));
    otDatasetConvertToTlvs(&dataset, &datasetTlvs);

    // 2. Call Join when Thread is not enabled.
    error          = OT_ERROR_NONE;
    resultReceived = false;
    host.Join(datasetTlvs, receiver);
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 0, [&resultReceived]() { return resultReceived; });
    EXPECT_EQ(error, OT_ERROR_INVALID_STATE);
    EXPECT_STREQ(errorMsg.c_str(), "Thread is not enabled");

    // 3. Call two consecutive Join. The first one should be aborted. The second one should succeed.
    error          = OT_ERROR_NONE;
    resultReceived = false;
    host.SetThreadEnabled(true, receiver);
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 0, [&resultReceived]() { return resultReceived; });
    error          = OT_ERROR_NONE;
    resultReceived = false;
    host.Join(datasetTlvs, receiver_);
    host.Join(datasetTlvs, receiver);

    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 0,
                         [&resultReceived, &resultReceived_]() { return resultReceived && resultReceived_; });
    EXPECT_EQ(error_, OT_ERROR_ABORT);
    EXPECT_STREQ(errorMsg_.c_str(), "Aborted by leave/disable operation"); // The second Join will trigger Leave first.
    EXPECT_EQ(error, OT_ERROR_NONE);
    EXPECT_STREQ(errorMsg.c_str(), "Join succeeded");
    EXPECT_EQ(host.GetDeviceRole(), OT_DEVICE_ROLE_LEADER);

    // 4. Call Join with the same dataset.
    error          = OT_ERROR_NONE;
    resultReceived = false;
    host.Join(datasetTlvs, receiver);
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 0, [&resultReceived]() { return resultReceived; });
    EXPECT_EQ(error, OT_ERROR_NONE);
    EXPECT_STREQ(errorMsg.c_str(), "Already Joined the target network");

    // 5. Call Disable right after Join (Already Attached).
    error           = OT_ERROR_NONE;
    resultReceived  = false;
    error_          = OT_ERROR_NONE;
    resultReceived_ = false;

    OT_UNUSED_VARIABLE(otDatasetCreateNewNetwork(ot::FakePlatform::CurrentInstance(), &dataset));
    otDatasetConvertToTlvs(&dataset, &datasetTlvs); // Use a different dataset.

    host.Join(datasetTlvs, receiver_);
    host.SetThreadEnabled(false, receiver);

    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 0,
                         [&resultReceived, &resultReceived_]() { return resultReceived && resultReceived_; });
    EXPECT_EQ(error_, OT_ERROR_BUSY);
    EXPECT_STREQ(errorMsg_.c_str(), "Thread is disabling");
    EXPECT_EQ(error, OT_ERROR_NONE);
    EXPECT_EQ(host.GetDeviceRole(), OT_DEVICE_ROLE_DISABLED);

    // 6. Call Disable right after Join (not attached).
    resultReceived = false;
    host.Leave(true, receiver); // Leave the network first.
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 0, [&resultReceived]() { return resultReceived; });
    resultReceived = false; // Enale Thread.
    host.SetThreadEnabled(true, receiver);
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 0, [&resultReceived]() { return resultReceived; });

    error           = OT_ERROR_NONE;
    resultReceived  = false;
    error_          = OT_ERROR_NONE;
    resultReceived_ = false;
    host.Join(datasetTlvs, receiver_);
    host.SetThreadEnabled(false, receiver);

    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 0,
                         [&resultReceived, &resultReceived_]() { return resultReceived && resultReceived_; });
    EXPECT_EQ(error_, OT_ERROR_ABORT);
    EXPECT_STREQ(errorMsg_.c_str(), "Aborted by leave/disable operation");
    EXPECT_EQ(error, OT_ERROR_NONE);
    EXPECT_EQ(host.GetDeviceRole(), OT_DEVICE_ROLE_DISABLED);

    host.Deinit();
}

#if OTBR_ENABLE_BORDER_AGENT
TEST(RcpHostApi, BorderAgentCallbackEnablesOnAttach)
{
    otbr::MainloopContext mainloop;
    otbr::Host::RcpHost   host("wpan0", std::vector<const char *>(), /* aBackboneInterfaceName */ "",
                               /* aDryRun */ false,
                               /* aEnableAutoAttach */ false);

    host.Init();

    bool borderAgentEnabled = false;
    host.AddThreadRoleChangedCallback([&host, &borderAgentEnabled](otDeviceRole aRole) {
        OT_UNUSED_VARIABLE(aRole);
        if (host.IsAttached())
        {
            borderAgentEnabled = true;
        }
    });

    otInstance              *instance = ot::FakePlatform::CurrentInstance();
    otOperationalDataset     dataset;
    otOperationalDatasetTlvs datasetTlvs;

    OT_UNUSED_VARIABLE(otDatasetCreateNewNetwork(instance, &dataset));
    otDatasetConvertToTlvs(&dataset, &datasetTlvs);
    OT_UNUSED_VARIABLE(otDatasetSetActiveTlvs(instance, &datasetTlvs));

    ASSERT_EQ(otIp6SetEnabled(instance, true), OT_ERROR_NONE);
    ASSERT_EQ(otThreadSetEnabled(instance, true), OT_ERROR_NONE);

    EXPECT_FALSE(borderAgentEnabled);

    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 5, [&borderAgentEnabled]() { return borderAgentEnabled; });

    EXPECT_TRUE(host.IsAttached());
    EXPECT_TRUE(borderAgentEnabled);

    host.Deinit();
}
#endif // OTBR_ENABLE_BORDER_AGENT

TEST(RcpHostApi, ThreadRoleChangedCallbackInvoked)
{
    // Verify the Thread role change callback fires for each major transition: enabling Thread drives
    // Disabled -> Detached -> Leader, and disabling Thread brings the stack back to Disabled. The test
    // pumps the mainloop until each role is observed to confirm the callback sequencing.
    otbr::MainloopContext mainloop;
    otbr::Host::RcpHost   host("wpan0", std::vector<const char *>(), /* aBackboneInterfaceName */ "",
                               /* aDryRun */ false,
                               /* aEnableAutoAttach */ false);

    host.Init();

    std::vector<otDeviceRole> observedRoles;
    host.AddThreadRoleChangedCallback([&observedRoles](otDeviceRole aRole) { observedRoles.push_back(aRole); });

    otInstance              *instance = ot::FakePlatform::CurrentInstance();
    otOperationalDataset     dataset;
    otOperationalDatasetTlvs datasetTlvs;

    ASSERT_EQ(otDatasetCreateNewNetwork(instance, &dataset), OT_ERROR_NONE);
    otDatasetConvertToTlvs(&dataset, &datasetTlvs);
    ASSERT_EQ(otDatasetSetActiveTlvs(instance, &datasetTlvs), OT_ERROR_NONE);

    // Case 1. Check callback invocation when enabling Thread.
    ASSERT_EQ(otIp6SetEnabled(instance, true), OT_ERROR_NONE);
    ASSERT_EQ(otThreadSetEnabled(instance, true), OT_ERROR_NONE);

    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 1, [&observedRoles]() { return observedRoles.size() >= 1; });
    ASSERT_FALSE(observedRoles.empty());
    EXPECT_EQ(observedRoles.front(), OT_DEVICE_ROLE_DETACHED);

    // Wait until the device promotes to leader; the callback should fire again.
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 5,
                         [&]() { return host.GetDeviceRole() == OT_DEVICE_ROLE_LEADER && observedRoles.size() >= 2; });
    ASSERT_GE(observedRoles.size(), 2u);
    EXPECT_EQ(observedRoles.back(), OT_DEVICE_ROLE_LEADER);

    // Case 2. Check callback invocation when disabling Thread.
    ASSERT_EQ(otThreadSetEnabled(instance, false), OT_ERROR_NONE);

    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 1, [&]() {
        return host.GetDeviceRole() == OT_DEVICE_ROLE_DISABLED && observedRoles.size() >= 3;
    });
    EXPECT_GE(observedRoles.size(), 3u);
    EXPECT_EQ(observedRoles.back(), OT_DEVICE_ROLE_DISABLED);

    host.Deinit();
}
