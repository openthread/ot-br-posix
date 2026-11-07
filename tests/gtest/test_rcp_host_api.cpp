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

#include <chrono>

#include <openthread/dataset.h>
#include <openthread/dataset_ftd.h>

#include "common/mainloop.hpp"
#include "common/mainloop_manager.hpp"
#include "host/rcp_host.hpp"
#include "host/thread_helper.hpp"

#include "fake_platform.hpp"

static void MainloopProcessUntil(otbr::MainloopContext    &aMainloop,
                                 uint32_t                  aTimeoutSec,
                                 std::function<bool(void)> aCondition)
{
    // Use a monotonic clock with sub-second precision.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(aTimeoutSec);

    while (!aCondition())
    {
        // Process pending tasks before checking the deadline, so that an
        // `aTimeoutSec` of 0 still gets exactly one Update()/Process() pass
        // instead of exiting the loop without processing anything.
        otbr::MainloopManager::GetInstance().Update(aMainloop);
        otbr::MainloopManager::GetInstance().Process(aMainloop);

        if (std::chrono::steady_clock::now() > deadline)
        {
            break;
        }
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
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 1, [&resultReceived]() { return resultReceived; });
    EXPECT_TRUE(resultReceived); // A 0-second wait here passes vacuously: the detach needs mainloop time.
    EXPECT_EQ(error, OT_ERROR_NONE);
    EXPECT_EQ(host.GetDeviceRole(), OT_DEVICE_ROLE_DISABLED); // Leave disables the stack, not just detaches.

    error = otDatasetGetActive(ot::FakePlatform::CurrentInstance(), &dataset); // Dataset should still be there.
    EXPECT_EQ(error, OT_ERROR_NONE);

    // 5. Call Leave with erase while attached: the stack must end up disabled, the dataset
    // gone, and subscribers must hear about the erase as an empty dataset.
    error                                          = OT_ERROR_NONE;
    resultReceived                                 = false;
    bool                     datasetChangeReceived = false;
    otOperationalDatasetTlvs lastDatasetTlvs;
    lastDatasetTlvs.mLength = 0xff;
    host.GetThreadHelper()->AddActiveDatasetChangeHandler(
        [&datasetChangeReceived, &lastDatasetTlvs](const otOperationalDatasetTlvs &aDatasetTlvs) {
            datasetChangeReceived = true;
            lastDatasetTlvs       = aDatasetTlvs;
        });
    host.SetThreadEnabled(true, nullptr);
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 1,
                         [&host]() { return host.GetDeviceRole() != OT_DEVICE_ROLE_DETACHED; });
    EXPECT_EQ(host.GetDeviceRole(), OT_DEVICE_ROLE_LEADER);
    host.Leave(/* aEraseDataset */ true, receiver);
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 1, [&resultReceived]() { return resultReceived; });
    EXPECT_TRUE(resultReceived);
    EXPECT_EQ(error, OT_ERROR_NONE);
    EXPECT_EQ(host.GetDeviceRole(), OT_DEVICE_ROLE_DISABLED);
    error = otDatasetGetActive(ot::FakePlatform::CurrentInstance(), &dataset);
    EXPECT_EQ(error, OT_ERROR_NOT_FOUND);
    EXPECT_TRUE(datasetChangeReceived);
    EXPECT_EQ(lastDatasetTlvs.mLength, 0);

    // 6. Call Leave with erase when the stack was started out-of-band (ot-ctl, ubus): the
    // state machine never saw SetThreadEnabled(), so only the device role knows Thread is
    // running. Leave must still detach, disable and erase rather than taking the
    // disabled-role shortcut and failing the erase.
    error          = OT_ERROR_NONE;
    resultReceived = false;
    OT_UNUSED_VARIABLE(otDatasetSetActiveTlvs(ot::FakePlatform::CurrentInstance(), &datasetTlvs));
    OT_UNUSED_VARIABLE(otIp6SetEnabled(ot::FakePlatform::CurrentInstance(), true));
    OT_UNUSED_VARIABLE(otThreadSetEnabled(ot::FakePlatform::CurrentInstance(), true));
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 1,
                         [&host]() { return host.GetDeviceRole() == OT_DEVICE_ROLE_LEADER; });
    EXPECT_EQ(host.GetDeviceRole(), OT_DEVICE_ROLE_LEADER);
    host.Leave(/* aEraseDataset */ true, receiver);
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 1, [&resultReceived]() { return resultReceived; });
    EXPECT_TRUE(resultReceived);
    EXPECT_EQ(error, OT_ERROR_NONE);
    EXPECT_EQ(host.GetDeviceRole(), OT_DEVICE_ROLE_DISABLED);
    error = otDatasetGetActive(ot::FakePlatform::CurrentInstance(), &dataset);
    EXPECT_EQ(error, OT_ERROR_NOT_FOUND);

    // 7. Enable Thread without an active dataset: the role stays disabled while the state
    // machine says enabled. Leave through the disabled-role shortcut must bring the state
    // machine back to disabled rather than preserving the stale enabled state -- and a
    // null receiver must be tolerated, not crash the shortcut's result post.
    otbr::Host::ThreadEnabledState enabledState = otbr::Host::ThreadEnabledState::kStateDisabled;
    host.AddThreadEnabledStateChangedCallback(
        [&enabledState](otbr::Host::ThreadEnabledState aState) { enabledState = aState; });
    host.SetThreadEnabled(true, nullptr);
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 0,
                         [&enabledState]() { return enabledState == otbr::Host::ThreadEnabledState::kStateEnabled; });
    EXPECT_EQ(enabledState, otbr::Host::ThreadEnabledState::kStateEnabled);
    EXPECT_EQ(host.GetDeviceRole(), OT_DEVICE_ROLE_DISABLED);
    host.Leave(/* aEraseDataset */ true, /* aReceiver */ nullptr);
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 0,
                         [&enabledState]() { return enabledState == otbr::Host::ThreadEnabledState::kStateDisabled; });
    EXPECT_EQ(enabledState, otbr::Host::ThreadEnabledState::kStateDisabled);

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

    otOperationalDataset     dataset{};
    otOperationalDatasetTlvs datasetTlvs{};
    otOperationalDatasetTlvs activeDatasetTlvs{};

    // 1. Call ScheduleMigration when host hasn't been initialized.
    otbr::MainloopManager::GetInstance().RemoveMainloopProcessor(
        &host); // Temporarily remove RcpHost because it's not initialized yet.
    host.ScheduleMigration(datasetTlvs, receiver);
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 0, [&resultReceived]() { return resultReceived; });
    EXPECT_EQ(error, OT_ERROR_INVALID_STATE);
    EXPECT_STREQ(errorMsg.c_str(), "OT is not initialized");
    otbr::MainloopManager::GetInstance().AddMainloopProcessor(&host);

    host.Init();

    OT_UNUSED_VARIABLE(otDatasetCreateNewNetwork(ot::FakePlatform::CurrentInstance(), &dataset));
    otDatasetConvertToTlvs(&dataset, &activeDatasetTlvs);
    dataset.mPendingTimestamp.mSeconds             = 1;
    dataset.mComponents.mIsPendingTimestampPresent = true;
    dataset.mDelay                                 = 30000;
    dataset.mComponents.mIsDelayPresent            = true;
    otDatasetConvertToTlvs(&dataset, &datasetTlvs);

    // 2. Call ScheduleMigration when the Thread is not enabled.
    error          = OT_ERROR_NONE;
    resultReceived = false;
    host.ScheduleMigration(datasetTlvs, receiver);
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 0, [&resultReceived]() { return resultReceived; });
    EXPECT_EQ(error, OT_ERROR_INVALID_STATE);
    EXPECT_STREQ(errorMsg.c_str(), "Thread is disabled");

    // 3. Call ScheduleMigration when Thread is enabled without an active dataset (device is not attached).
    error          = OT_ERROR_FAILED;
    resultReceived = false;
    host.SetThreadEnabled(true, receiver);
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 1, [&resultReceived]() { return resultReceived; });
    EXPECT_EQ(error, OT_ERROR_NONE);
    EXPECT_EQ(host.GetDeviceRole(), OT_DEVICE_ROLE_DISABLED);

    error          = OT_ERROR_NONE;
    resultReceived = false;
    host.ScheduleMigration(datasetTlvs, receiver);
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 0, [&resultReceived]() { return resultReceived; });
    EXPECT_TRUE(resultReceived);
    EXPECT_EQ(error, OT_ERROR_INVALID_STATE);
    EXPECT_STREQ(errorMsg.c_str(), "Device is detached");

    OT_UNUSED_VARIABLE(otDatasetSetActiveTlvs(ot::FakePlatform::CurrentInstance(), &activeDatasetTlvs));
    OT_UNUSED_VARIABLE(otIp6SetEnabled(ot::FakePlatform::CurrentInstance(), true));
    OT_UNUSED_VARIABLE(otThreadSetEnabled(ot::FakePlatform::CurrentInstance(), true));
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 1,
                         [&host]() { return host.GetDeviceRole() == OT_DEVICE_ROLE_LEADER; });
    EXPECT_EQ(host.GetDeviceRole(), OT_DEVICE_ROLE_LEADER);

    // 4. Reject malformed or incomplete Operational Dataset TLVs.
    // 4a. Half-pending dataset: PendingTimestamp present without DelayTimer.
    {
        otOperationalDataset     halfPendingDataset = dataset;
        otOperationalDatasetTlvs halfPendingTlvs{};

        halfPendingDataset.mPendingTimestamp.mSeconds             = 1;
        halfPendingDataset.mComponents.mIsPendingTimestampPresent = true;
        halfPendingDataset.mComponents.mIsDelayPresent            = false;
        otDatasetConvertToTlvs(&halfPendingDataset, &halfPendingTlvs);

        error          = OT_ERROR_NONE;
        resultReceived = false;
        host.ScheduleMigration(halfPendingTlvs, receiver);
        MainloopProcessUntil(mainloop, /* aTimeoutSec */ 0, [&resultReceived]() { return resultReceived; });
        EXPECT_TRUE(resultReceived);
        EXPECT_EQ(error, OT_ERROR_INVALID_ARGS);
        EXPECT_STREQ(errorMsg.c_str(), "Operational dataset is not valid");
    }

    // 4b. Half-pending dataset: DelayTimer present without PendingTimestamp.
    {
        otOperationalDataset     halfPendingDataset = dataset;
        otOperationalDatasetTlvs halfPendingTlvs{};

        halfPendingDataset.mComponents.mIsPendingTimestampPresent = false;
        halfPendingDataset.mDelay                                 = 30000;
        halfPendingDataset.mComponents.mIsDelayPresent            = true;
        otDatasetConvertToTlvs(&halfPendingDataset, &halfPendingTlvs);

        error          = OT_ERROR_NONE;
        resultReceived = false;
        host.ScheduleMigration(halfPendingTlvs, receiver);
        MainloopProcessUntil(mainloop, /* aTimeoutSec */ 0, [&resultReceived]() { return resultReceived; });
        EXPECT_TRUE(resultReceived);
        EXPECT_EQ(error, OT_ERROR_INVALID_ARGS);
        EXPECT_STREQ(errorMsg.c_str(), "Operational dataset is not valid");
    }

    // 4c. Missing ActiveTimestamp TLV.
    {
        otOperationalDataset     noActiveTsDataset = dataset;
        otOperationalDatasetTlvs noActiveTsTlvs{};

        noActiveTsDataset.mComponents.mIsActiveTimestampPresent  = false;
        noActiveTsDataset.mPendingTimestamp.mSeconds             = 1;
        noActiveTsDataset.mComponents.mIsPendingTimestampPresent = true;
        noActiveTsDataset.mDelay                                 = 30000;
        noActiveTsDataset.mComponents.mIsDelayPresent            = true;
        otDatasetConvertToTlvs(&noActiveTsDataset, &noActiveTsTlvs);

        error          = OT_ERROR_NONE;
        resultReceived = false;
        host.ScheduleMigration(noActiveTsTlvs, receiver);
        MainloopProcessUntil(mainloop, /* aTimeoutSec */ 0, [&resultReceived]() { return resultReceived; });
        EXPECT_TRUE(resultReceived);
        EXPECT_EQ(error, OT_ERROR_INVALID_ARGS);
        EXPECT_STREQ(errorMsg.c_str(), "Operational dataset is not valid");
    }

    // 4d. Truncated TLV buffer, oversized mLength (> sizeof(mTlvs)), and extended-length escape (0xff).
    {
        otOperationalDatasetTlvs malformedTlvs{};

        malformedTlvs.mTlvs[0] = OT_MESHCOP_TLV_CHANNEL;
        malformedTlvs.mTlvs[1] = 10;
        malformedTlvs.mTlvs[2] = 0x00;
        malformedTlvs.mLength  = 3;

        error          = OT_ERROR_NONE;
        resultReceived = false;
        host.ScheduleMigration(malformedTlvs, receiver);
        MainloopProcessUntil(mainloop, /* aTimeoutSec */ 0, [&resultReceived]() { return resultReceived; });
        EXPECT_TRUE(resultReceived);
        EXPECT_EQ(error, OT_ERROR_INVALID_ARGS);
        EXPECT_STREQ(errorMsg.c_str(), "Operational dataset is not valid");

        otOperationalDatasetTlvs maxLenTlvs{};
        memset(maxLenTlvs.mTlvs, 0xaa, sizeof(maxLenTlvs.mTlvs));
        maxLenTlvs.mTlvs[0] = OT_MESHCOP_TLV_VENDOR_DATA_TLV;
        maxLenTlvs.mTlvs[1] = 252;
        maxLenTlvs.mLength  = 255;

        error          = OT_ERROR_NONE;
        resultReceived = false;
        host.ScheduleMigration(maxLenTlvs, receiver);
        MainloopProcessUntil(mainloop, /* aTimeoutSec */ 0, [&resultReceived]() { return resultReceived; });
        EXPECT_TRUE(resultReceived);
        EXPECT_EQ(error, OT_ERROR_INVALID_ARGS);
        EXPECT_STREQ(errorMsg.c_str(), "Operational dataset is not valid");

        otOperationalDatasetTlvs extLenTlvs{};
        memset(extLenTlvs.mTlvs, 0, sizeof(extLenTlvs.mTlvs));
        extLenTlvs.mTlvs[sizeof(extLenTlvs.mTlvs) - 2] = OT_MESHCOP_TLV_VENDOR_DATA_TLV;
        extLenTlvs.mTlvs[sizeof(extLenTlvs.mTlvs) - 1] = 0xff;
        extLenTlvs.mLength                             = sizeof(extLenTlvs.mTlvs);

        error          = OT_ERROR_NONE;
        resultReceived = false;
        host.ScheduleMigration(extLenTlvs, receiver);
        MainloopProcessUntil(mainloop, /* aTimeoutSec */ 0, [&resultReceived]() { return resultReceived; });
        EXPECT_TRUE(resultReceived);
        EXPECT_EQ(error, OT_ERROR_INVALID_ARGS);
        EXPECT_STREQ(errorMsg.c_str(), "Operational dataset is not valid");

        // Directly exercise StripTlvs() and ProcessDatasetForMigration() bounds checks on mLength = 255 and 0xff.
        otOperationalDatasetTlvs strippedMax =
            otbr::Host::RcpHost::StripTlvs(maxLenTlvs, {OT_MESHCOP_TLV_PENDINGTIMESTAMP, OT_MESHCOP_TLV_DELAYTIMER});
        EXPECT_EQ(strippedMax.mLength, 0U);
        EXPECT_EQ(otbr::Host::ThreadHelper::ProcessDatasetForMigration(maxLenTlvs, 30000), OT_ERROR_INVALID_ARGS);

        otOperationalDatasetTlvs strippedExt =
            otbr::Host::RcpHost::StripTlvs(extLenTlvs, {OT_MESHCOP_TLV_PENDINGTIMESTAMP, OT_MESHCOP_TLV_DELAYTIMER});
        EXPECT_EQ(strippedExt.mLength, sizeof(extLenTlvs.mTlvs) - 2);
        EXPECT_EQ(otbr::Host::ThreadHelper::ProcessDatasetForMigration(extLenTlvs, 30000), OT_ERROR_INVALID_ARGS);
    }

    // 4e. Oversized Active Dataset (no room for PendingTimestamp + DelayTimer when MGMT_PENDING_SET.req is needed).
    {
        otOperationalDataset     oversizedDataset = dataset;
        otOperationalDatasetTlvs oversizedTlvs{};

        oversizedDataset.mActiveTimestamp.mSeconds++;
        oversizedDataset.mChannel                               = (dataset.mChannel == 11) ? 12 : 11;
        oversizedDataset.mComponents.mIsPendingTimestampPresent = false;
        oversizedDataset.mComponents.mIsDelayPresent            = false;
        otDatasetConvertToTlvs(&oversizedDataset, &oversizedTlvs);

        const size_t paddingValueLen                 = sizeof(oversizedTlvs.mTlvs) - oversizedTlvs.mLength - 2;
        oversizedTlvs.mTlvs[oversizedTlvs.mLength++] = OT_MESHCOP_TLV_VENDOR_DATA_TLV;
        oversizedTlvs.mTlvs[oversizedTlvs.mLength++] = static_cast<uint8_t>(paddingValueLen);
        memset(oversizedTlvs.mTlvs + oversizedTlvs.mLength, 0xaa, paddingValueLen);
        oversizedTlvs.mLength += static_cast<uint8_t>(paddingValueLen);

        error          = OT_ERROR_NONE;
        resultReceived = false;
        host.ScheduleMigration(oversizedTlvs, receiver);
        MainloopProcessUntil(mainloop, /* aTimeoutSec */ 0, [&resultReceived]() { return resultReceived; });
        EXPECT_TRUE(resultReceived);
        EXPECT_EQ(error, OT_ERROR_INVALID_ARGS);
        EXPECT_STREQ(errorMsg.c_str(), "Operational dataset is not valid");
    }

    // 5a. Schedule migration with a Pending Dataset that does not affect connectivity (uses MGMT_ACTIVE_SET.req
    // and preserves unknown/vendor TLVs while stripping PendingTimestamp and DelayTimer).
    const uint16_t initialChannel = dataset.mChannel;

    dataset.mActiveTimestamp.mSeconds++;
    dataset.mPendingTimestamp.mSeconds             = 1;
    dataset.mComponents.mIsPendingTimestampPresent = true;
    dataset.mDelay                                 = 30000;
    dataset.mComponents.mIsDelayPresent            = true;
    OT_UNUSED_VARIABLE(otNetworkNameFromString(&dataset.mNetworkName, "NewNetName"));
    otDatasetConvertToTlvs(&dataset, &datasetTlvs);

    const uint8_t vendorTlv[] = {OT_MESHCOP_TLV_VENDOR_DATA_TLV, 2, 0xab, 0xcd};
    ASSERT_LE(datasetTlvs.mLength + sizeof(vendorTlv), sizeof(datasetTlvs.mTlvs));
    memcpy(datasetTlvs.mTlvs + datasetTlvs.mLength, vendorTlv, sizeof(vendorTlv));
    datasetTlvs.mLength += sizeof(vendorTlv);

    error          = OT_ERROR_FAILED;
    resultReceived = false;
    host.ScheduleMigration(datasetTlvs, receiver);
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 1, [&resultReceived]() { return resultReceived; });
    EXPECT_TRUE(resultReceived);
    EXPECT_EQ(error, OT_ERROR_NONE);
    {
        otOperationalDataset     activeDataset{};
        otOperationalDatasetTlvs activeDatasetTlvs{};
        otOperationalDataset     pendingDataset{};

        EXPECT_EQ(otDatasetGetActive(ot::FakePlatform::CurrentInstance(), &activeDataset), OT_ERROR_NONE);
        EXPECT_STREQ(activeDataset.mNetworkName.m8, "NewNetName");
        EXPECT_FALSE(activeDataset.mComponents.mIsPendingTimestampPresent);
        EXPECT_FALSE(activeDataset.mComponents.mIsDelayPresent);
        EXPECT_EQ(otDatasetGetPending(ot::FakePlatform::CurrentInstance(), &pendingDataset), OT_ERROR_NOT_FOUND);

        EXPECT_EQ(otDatasetGetActiveTlvs(ot::FakePlatform::CurrentInstance(), &activeDatasetTlvs), OT_ERROR_NONE);
        bool foundVendorTlv = false;
        for (size_t i = 0; i + sizeof(vendorTlv) <= activeDatasetTlvs.mLength; ++i)
        {
            if (memcmp(activeDatasetTlvs.mTlvs + i, vendorTlv, sizeof(vendorTlv)) == 0)
            {
                foundVendorTlv = true;
                break;
            }
        }
        EXPECT_TRUE(foundVendorTlv);
    }

    // 5b. Schedule migration with an Active Dataset that does not affect connectivity
    // (sends MGMT_ACTIVE_SET.req directly and preserves vendor TLVs).
    {
        otOperationalDataset     directActiveDataset = dataset;
        otOperationalDatasetTlvs directActiveTlvs{};
        const uint8_t            vendorTlv2[] = {OT_MESHCOP_TLV_VENDOR_DATA_TLV, 2, 0x12, 0x34};

        dataset.mActiveTimestamp.mSeconds++;
        directActiveDataset.mActiveTimestamp                       = dataset.mActiveTimestamp;
        directActiveDataset.mComponents.mIsPendingTimestampPresent = false;
        directActiveDataset.mComponents.mIsDelayPresent            = false;
        OT_UNUSED_VARIABLE(otNetworkNameFromString(&directActiveDataset.mNetworkName, "ActiveNetName"));
        dataset.mNetworkName = directActiveDataset.mNetworkName;
        otDatasetConvertToTlvs(&directActiveDataset, &directActiveTlvs);

        ASSERT_LE(directActiveTlvs.mLength + sizeof(vendorTlv2), sizeof(directActiveTlvs.mTlvs));
        memcpy(directActiveTlvs.mTlvs + directActiveTlvs.mLength, vendorTlv2, sizeof(vendorTlv2));
        directActiveTlvs.mLength += sizeof(vendorTlv2);

        error          = OT_ERROR_FAILED;
        resultReceived = false;
        host.ScheduleMigration(directActiveTlvs, receiver);
        MainloopProcessUntil(mainloop, /* aTimeoutSec */ 1, [&resultReceived]() { return resultReceived; });
        EXPECT_TRUE(resultReceived);
        EXPECT_EQ(error, OT_ERROR_NONE);

        otOperationalDataset     curActiveDataset{};
        otOperationalDatasetTlvs curActiveTlvs{};
        otOperationalDataset     curPendingDataset{};

        EXPECT_EQ(otDatasetGetActive(ot::FakePlatform::CurrentInstance(), &curActiveDataset), OT_ERROR_NONE);
        EXPECT_STREQ(curActiveDataset.mNetworkName.m8, "ActiveNetName");
        EXPECT_EQ(otDatasetGetPending(ot::FakePlatform::CurrentInstance(), &curPendingDataset), OT_ERROR_NOT_FOUND);

        EXPECT_EQ(otDatasetGetActiveTlvs(ot::FakePlatform::CurrentInstance(), &curActiveTlvs), OT_ERROR_NONE);
        bool foundVendorTlv = false;
        for (size_t i = 0; i + sizeof(vendorTlv2) <= curActiveTlvs.mLength; ++i)
        {
            if (memcmp(curActiveTlvs.mTlvs + i, vendorTlv2, sizeof(vendorTlv2)) == 0)
            {
                foundVendorTlv = true;
                break;
            }
        }
        EXPECT_TRUE(foundVendorTlv);
    }

    // 6. Non-incremented ActiveTimestamp on MGMT_ACTIVE_SET.req is rejected by the Leader.
    dataset.mPendingTimestamp.mSeconds             = 2;
    dataset.mComponents.mIsPendingTimestampPresent = true;
    dataset.mDelay                                 = 30000;
    dataset.mComponents.mIsDelayPresent            = true;
    OT_UNUSED_VARIABLE(otNetworkNameFromString(&dataset.mNetworkName, "RejectedName"));
    otDatasetConvertToTlvs(&dataset, &datasetTlvs);

    error          = OT_ERROR_NONE;
    resultReceived = false;
    host.ScheduleMigration(datasetTlvs, receiver);
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 1, [&resultReceived]() { return resultReceived; });
    EXPECT_TRUE(resultReceived);
    EXPECT_EQ(error, OT_ERROR_REJECTED);
    {
        otOperationalDataset activeDataset{};

        EXPECT_EQ(otDatasetGetActive(ot::FakePlatform::CurrentInstance(), &activeDataset), OT_ERROR_NONE);
        EXPECT_STREQ(activeDataset.mNetworkName.m8, "ActiveNetName");
    }

    // 7. Schedule migration when dataset affects connectivity (uses MGMT_PENDING_SET.req),
    // and reject an overlapping ScheduleMigration() across the MGMT_ACTIVE_SET.req path with OT_ERROR_BUSY.
    dataset.mActiveTimestamp.mSeconds++;
    dataset.mPendingTimestamp.mSeconds             = 3;
    dataset.mComponents.mIsPendingTimestampPresent = true;
    dataset.mDelay                                 = 30000;
    dataset.mComponents.mIsDelayPresent            = true;
    OT_UNUSED_VARIABLE(otNetworkNameFromString(&dataset.mNetworkName, "PendingNet"));
    dataset.mChannel = (initialChannel == 11) ? 12 : 11;
    otDatasetConvertToTlvs(&dataset, &datasetTlvs);

    otOperationalDataset     overlappingDataset = dataset;
    otOperationalDatasetTlvs overlappingTlvs{};
    overlappingDataset.mChannel = initialChannel;
    overlappingDataset.mActiveTimestamp.mSeconds++;
    overlappingDataset.mPendingTimestamp.mSeconds = 4;
    otDatasetConvertToTlvs(&overlappingDataset, &overlappingTlvs);

    otError     secondError          = OT_ERROR_NONE;
    std::string secondErrorMsg       = "";
    bool        secondResultReceived = false;
    auto        secondReceiver       = [&secondResultReceived, &secondError, &secondErrorMsg](otError            aError,
                                                                                 const std::string &aErrorMsg) {
        secondResultReceived = true;
        secondError          = aError;
        secondErrorMsg       = aErrorMsg;
    };

    error          = OT_ERROR_FAILED;
    resultReceived = false;
    host.ScheduleMigration(datasetTlvs, receiver);
    host.ScheduleMigration(overlappingTlvs, secondReceiver);
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 1,
                         [&resultReceived, &secondResultReceived]() { return resultReceived && secondResultReceived; });
    EXPECT_TRUE(secondResultReceived);
    EXPECT_EQ(secondError, OT_ERROR_BUSY);
    EXPECT_STREQ(secondErrorMsg.c_str(), "Another migration is in progress");
    EXPECT_TRUE(resultReceived);
    EXPECT_EQ(error, OT_ERROR_NONE);
    {
        otOperationalDataset pendingDataset{};

        EXPECT_EQ(otDatasetGetPending(ot::FakePlatform::CurrentInstance(), &pendingDataset), OT_ERROR_NONE);
        EXPECT_EQ(pendingDataset.mChannel, dataset.mChannel);
    }

    // 8. While a Pending Dataset is already active, calling ScheduleMigration() with connectivity
    // matching the current Active Dataset uses MGMT_PENDING_SET.req to supersede the Pending Dataset.
    dataset.mChannel = initialChannel;
    dataset.mActiveTimestamp.mSeconds++;
    dataset.mPendingTimestamp.mSeconds             = 5;
    dataset.mComponents.mIsPendingTimestampPresent = true;
    dataset.mDelay                                 = 30000;
    dataset.mComponents.mIsDelayPresent            = true;
    OT_UNUSED_VARIABLE(otNetworkNameFromString(&dataset.mNetworkName, "SupersededNet"));
    otDatasetConvertToTlvs(&dataset, &datasetTlvs);

    error          = OT_ERROR_FAILED;
    resultReceived = false;
    host.ScheduleMigration(datasetTlvs, receiver);
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 1, [&resultReceived]() { return resultReceived; });
    EXPECT_TRUE(resultReceived);
    EXPECT_EQ(error, OT_ERROR_NONE);
    {
        otOperationalDataset pendingDataset{};

        EXPECT_EQ(otDatasetGetPending(ot::FakePlatform::CurrentInstance(), &pendingDataset), OT_ERROR_NONE);
        EXPECT_EQ(pendingDataset.mChannel, initialChannel);
        EXPECT_STREQ(pendingDataset.mNetworkName.m8, "SupersededNet");
        EXPECT_EQ(pendingDataset.mPendingTimestamp.mSeconds, uint64_t{5});
    }

    // 9. Active Dataset that affects connectivity synthesizes PendingTimestamp + default 300s DelayTimer
    // and preserves vendor TLVs via MGMT_PENDING_SET.req.
    otTimestamp prevPendingTimestamp{};
    {
        otOperationalDataset     activeMigDataset = dataset;
        otOperationalDatasetTlvs activeMigTlvs{};

        dataset.mActiveTimestamp.mSeconds++;
        activeMigDataset.mActiveTimestamp                       = dataset.mActiveTimestamp;
        activeMigDataset.mChannel                               = (initialChannel == 11) ? 12 : 11;
        activeMigDataset.mComponents.mIsPendingTimestampPresent = false;
        activeMigDataset.mComponents.mIsDelayPresent            = false;
        OT_UNUSED_VARIABLE(otNetworkNameFromString(&activeMigDataset.mNetworkName, "ActPendingNet"));
        otDatasetConvertToTlvs(&activeMigDataset, &activeMigTlvs);

        ASSERT_LE(activeMigTlvs.mLength + sizeof(vendorTlv), sizeof(activeMigTlvs.mTlvs));
        memcpy(activeMigTlvs.mTlvs + activeMigTlvs.mLength, vendorTlv, sizeof(vendorTlv));
        activeMigTlvs.mLength += sizeof(vendorTlv);

        error          = OT_ERROR_FAILED;
        resultReceived = false;
        host.ScheduleMigration(activeMigTlvs, receiver);
        MainloopProcessUntil(mainloop, /* aTimeoutSec */ 1, [&resultReceived]() { return resultReceived; });
        EXPECT_TRUE(resultReceived);
        EXPECT_EQ(error, OT_ERROR_NONE);

        otOperationalDataset     pendingDataset{};
        otOperationalDatasetTlvs pendingTlvs{};

        EXPECT_EQ(otDatasetGetPending(ot::FakePlatform::CurrentInstance(), &pendingDataset), OT_ERROR_NONE);
        EXPECT_EQ(pendingDataset.mChannel, activeMigDataset.mChannel);
        EXPECT_STREQ(pendingDataset.mNetworkName.m8, "ActPendingNet");
        EXPECT_TRUE(pendingDataset.mComponents.mIsPendingTimestampPresent);
        EXPECT_GT(pendingDataset.mPendingTimestamp.mSeconds, uint64_t{5});
        prevPendingTimestamp = pendingDataset.mPendingTimestamp;
        EXPECT_TRUE(pendingDataset.mComponents.mIsDelayPresent);
        EXPECT_GT(pendingDataset.mDelay, 250000U);
        EXPECT_LE(pendingDataset.mDelay, 300000U);

        EXPECT_EQ(otDatasetGetPendingTlvs(ot::FakePlatform::CurrentInstance(), &pendingTlvs), OT_ERROR_NONE);
        bool foundVendorTlv = false;
        for (size_t i = 0; i + sizeof(vendorTlv) <= pendingTlvs.mLength; ++i)
        {
            if (memcmp(pendingTlvs.mTlvs + i, vendorTlv, sizeof(vendorTlv)) == 0)
            {
                foundVendorTlv = true;
                break;
            }
        }
        EXPECT_TRUE(foundVendorTlv);
    }

    // 10. Active Dataset that does not affect connectivity while a Pending Dataset is active
    // synthesizes PendingTimestamp (monotonically advancing past the active Pending Dataset even within
    // the same tick) + minimum 30s DelayTimer to supersede the Pending Dataset.
    {
        otOperationalDataset     activeSupersedeDataset = dataset;
        otOperationalDatasetTlvs activeSupersedeTlvs{};

        dataset.mActiveTimestamp.mSeconds++;
        activeSupersedeDataset.mActiveTimestamp                       = dataset.mActiveTimestamp;
        activeSupersedeDataset.mChannel                               = initialChannel;
        activeSupersedeDataset.mComponents.mIsPendingTimestampPresent = false;
        activeSupersedeDataset.mComponents.mIsDelayPresent            = false;
        OT_UNUSED_VARIABLE(otNetworkNameFromString(&activeSupersedeDataset.mNetworkName, "ActSuperseded"));
        otDatasetConvertToTlvs(&activeSupersedeDataset, &activeSupersedeTlvs);

        error          = OT_ERROR_FAILED;
        resultReceived = false;
        host.ScheduleMigration(activeSupersedeTlvs, receiver);
        MainloopProcessUntil(mainloop, /* aTimeoutSec */ 1, [&resultReceived]() { return resultReceived; });
        EXPECT_TRUE(resultReceived);
        EXPECT_EQ(error, OT_ERROR_NONE);

        otOperationalDataset pendingDataset{};

        EXPECT_EQ(otDatasetGetPending(ot::FakePlatform::CurrentInstance(), &pendingDataset), OT_ERROR_NONE);
        EXPECT_EQ(pendingDataset.mChannel, initialChannel);
        EXPECT_STREQ(pendingDataset.mNetworkName.m8, "ActSuperseded");
        EXPECT_TRUE(pendingDataset.mComponents.mIsPendingTimestampPresent);
        EXPECT_TRUE((pendingDataset.mPendingTimestamp.mSeconds > prevPendingTimestamp.mSeconds) ||
                    (pendingDataset.mPendingTimestamp.mSeconds == prevPendingTimestamp.mSeconds &&
                     pendingDataset.mPendingTimestamp.mTicks > prevPendingTimestamp.mTicks));
        EXPECT_TRUE(pendingDataset.mComponents.mIsDelayPresent);
        EXPECT_GT(pendingDataset.mDelay, 0U);
        EXPECT_LE(pendingDataset.mDelay, 30000U);

        // Also verify superseding when the active Pending Dataset has a PendingTimestamp ahead of CLOCK_REALTIME.
        otOperationalDataset     futurePendingDataset = dataset;
        otOperationalDatasetTlvs futurePendingTlvs{};
        const uint64_t           futureSeconds = pendingDataset.mPendingTimestamp.mSeconds + 100000ULL;

        dataset.mActiveTimestamp.mSeconds++;
        futurePendingDataset.mActiveTimestamp                       = dataset.mActiveTimestamp;
        futurePendingDataset.mChannel                               = (initialChannel == 11) ? 12 : 11;
        futurePendingDataset.mPendingTimestamp.mSeconds             = futureSeconds;
        futurePendingDataset.mPendingTimestamp.mTicks               = 32767;
        futurePendingDataset.mPendingTimestamp.mAuthoritative       = true;
        futurePendingDataset.mComponents.mIsPendingTimestampPresent = true;
        futurePendingDataset.mDelay                                 = 30000;
        futurePendingDataset.mComponents.mIsDelayPresent            = true;
        otDatasetConvertToTlvs(&futurePendingDataset, &futurePendingTlvs);

        error          = OT_ERROR_FAILED;
        resultReceived = false;
        host.ScheduleMigration(futurePendingTlvs, receiver);
        MainloopProcessUntil(mainloop, /* aTimeoutSec */ 1, [&resultReceived]() { return resultReceived; });
        EXPECT_TRUE(resultReceived);
        EXPECT_EQ(error, OT_ERROR_NONE);

        dataset.mActiveTimestamp.mSeconds++;
        activeSupersedeDataset.mActiveTimestamp = dataset.mActiveTimestamp;
        OT_UNUSED_VARIABLE(otNetworkNameFromString(&activeSupersedeDataset.mNetworkName, "FutureSuperseded"));
        otDatasetConvertToTlvs(&activeSupersedeDataset, &activeSupersedeTlvs);

        error          = OT_ERROR_FAILED;
        resultReceived = false;
        host.ScheduleMigration(activeSupersedeTlvs, receiver);
        MainloopProcessUntil(mainloop, /* aTimeoutSec */ 1, [&resultReceived]() { return resultReceived; });
        EXPECT_TRUE(resultReceived);
        EXPECT_EQ(error, OT_ERROR_NONE);

        EXPECT_EQ(otDatasetGetPending(ot::FakePlatform::CurrentInstance(), &pendingDataset), OT_ERROR_NONE);
        EXPECT_STREQ(pendingDataset.mNetworkName.m8, "FutureSuperseded");
        EXPECT_EQ(pendingDataset.mPendingTimestamp.mSeconds, futureSeconds + 1ULL);
        EXPECT_EQ(pendingDataset.mPendingTimestamp.mTicks, 0U);
        EXPECT_FALSE(pendingDataset.mPendingTimestamp.mAuthoritative);
    }

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

    // Unlike the single-pass 0s waits above, this step undergoes a full
    // Detach -> re-Join -> Leader-formation sequence which requires multiple
    // mainloop iterations to complete.
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 1,
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

    // Like the step-3 wait above, this Join races a Disable that triggers
    // its own Detach sequence, so it needs multiple mainloop iterations.
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 1,
                         [&resultReceived, &resultReceived_]() { return resultReceived && resultReceived_; });
    EXPECT_EQ(error_, OT_ERROR_ABORT);
    EXPECT_STREQ(errorMsg_.c_str(), "Aborted by leave/disable operation");
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

    // Same Detach -> re-Join -> Leader-formation sequence as step 3, so it
    // needs the same 1s budget rather than a single mainloop pass.
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 1,
                         [&resultReceived, &resultReceived_]() { return resultReceived && resultReceived_; });
    EXPECT_EQ(error_, OT_ERROR_ABORT);
    EXPECT_STREQ(errorMsg_.c_str(), "Aborted by leave/disable operation");
    EXPECT_EQ(error, OT_ERROR_NONE);
    EXPECT_EQ(host.GetDeviceRole(), OT_DEVICE_ROLE_DISABLED);

    // 7. Call Join with a different dataset while attached as leader. The device must detach,
    // take the new dataset and attach to the new network, not stay on the old one or loop
    // in the detach branch.
    resultReceived = false;
    host.SetThreadEnabled(true, receiver);
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 0, [&resultReceived]() { return resultReceived; });
    error          = OT_ERROR_NONE;
    resultReceived = false;
    host.Join(datasetTlvs, receiver);
    // Wait for the join's own result, not just the role: the result is posted
    // to the task runner and would otherwise land in the next step's wait.
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 1, [&resultReceived]() { return resultReceived; });
    EXPECT_EQ(error, OT_ERROR_NONE);
    EXPECT_STREQ(errorMsg.c_str(), "Join succeeded");
    EXPECT_EQ(host.GetDeviceRole(), OT_DEVICE_ROLE_LEADER);

    otOperationalDataset     newDataset;
    otOperationalDatasetTlvs newDatasetTlvs;
    OT_UNUSED_VARIABLE(otDatasetCreateNewNetwork(ot::FakePlatform::CurrentInstance(), &newDataset));
    otDatasetConvertToTlvs(&newDataset, &newDatasetTlvs);
    ASSERT_NE(memcmp(newDataset.mExtendedPanId.m8, dataset.mExtendedPanId.m8, sizeof(dataset.mExtendedPanId)), 0);

    error_          = OT_ERROR_NONE;
    resultReceived_ = false;
    host.Join(newDatasetTlvs, receiver_);
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 1, [&resultReceived_]() { return resultReceived_; });
    EXPECT_TRUE(resultReceived_);
    EXPECT_EQ(error_, OT_ERROR_NONE);
    EXPECT_STREQ(errorMsg_.c_str(), "Join succeeded");
    EXPECT_EQ(host.GetDeviceRole(), OT_DEVICE_ROLE_LEADER);
    otOperationalDataset activeDataset;
    ASSERT_EQ(otDatasetGetActive(ot::FakePlatform::CurrentInstance(), &activeDataset), OT_ERROR_NONE);
    EXPECT_EQ(memcmp(activeDataset.mExtendedPanId.m8, newDataset.mExtendedPanId.m8, sizeof(newDataset.mExtendedPanId)),
              0);

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

    ASSERT_EQ(otDatasetCreateNewNetwork(instance, &dataset), OT_ERROR_NONE);
    otDatasetConvertToTlvs(&dataset, &datasetTlvs);
    ASSERT_EQ(otDatasetSetActiveTlvs(instance, &datasetTlvs), OT_ERROR_NONE);

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

TEST(RcpHostApi, ThreadDetachGracefullyCallbackReentrancy)
{
    otbr::MainloopContext mainloop;
    otbr::Host::RcpHost   host("wpan0", std::vector<const char *>(), /* aBackboneInterfaceName */ "",
                               /* aDryRun */ false,
                               /* aEnableAutoAttach */ false);

    host.Init();

    otInstance              *instance = ot::FakePlatform::CurrentInstance();
    otOperationalDataset     dataset;
    otOperationalDatasetTlvs datasetTlvs;

    ASSERT_EQ(otDatasetCreateNewNetwork(instance, &dataset), OT_ERROR_NONE);
    otDatasetConvertToTlvs(&dataset, &datasetTlvs);
    ASSERT_EQ(otDatasetSetActiveTlvs(instance, &datasetTlvs), OT_ERROR_NONE);

    // Make sure the device is in Leader role first.
    ASSERT_EQ(otIp6SetEnabled(instance, true), OT_ERROR_NONE);
    ASSERT_EQ(otThreadSetEnabled(instance, true), OT_ERROR_NONE);

    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 5,
                         [&]() { return host.GetDeviceRole() == OT_DEVICE_ROLE_LEADER; });
    ASSERT_EQ(host.GetDeviceRole(), OT_DEVICE_ROLE_LEADER);

    // Queue multiple Join calls with a different dataset to trigger the ThreadDetachGracefully recursive scenario
    otOperationalDataset     joinDataset;
    otOperationalDatasetTlvs joinDatasetTlvs;
    ASSERT_EQ(otDatasetCreateNewNetwork(instance, &joinDataset), OT_ERROR_NONE);
    otDatasetConvertToTlvs(&joinDataset, &joinDatasetTlvs);

    bool done1 = false, done2 = false, done3 = false, done4 = false;
    host.Join(joinDatasetTlvs, [&](otError aError, const std::string &aErrorMsg) {
        OT_UNUSED_VARIABLE(aError);
        OT_UNUSED_VARIABLE(aErrorMsg);
        done1 = true;
    });
    host.Join(joinDatasetTlvs, [&](otError aError, const std::string &aErrorMsg) {
        OT_UNUSED_VARIABLE(aError);
        OT_UNUSED_VARIABLE(aErrorMsg);
        done2 = true;
    });
    host.Join(joinDatasetTlvs, [&](otError aError, const std::string &aErrorMsg) {
        OT_UNUSED_VARIABLE(aError);
        OT_UNUSED_VARIABLE(aErrorMsg);
        done3 = true;
    });
    host.Join(joinDatasetTlvs, [&](otError aError, const std::string &aErrorMsg) {
        OT_UNUSED_VARIABLE(aError);
        OT_UNUSED_VARIABLE(aErrorMsg);
        done4 = true;
    });

    // Force the detach callbacks to run.
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 5, [&]() { return done1 && done2 && done3 && done4; });

    EXPECT_TRUE(done1);
    EXPECT_TRUE(done2);
    EXPECT_TRUE(done3);
    EXPECT_TRUE(done4);

    host.Deinit();
}

static bool ExtendedPanIdsMatch(const otOperationalDataset &aLhs, const otOperationalDataset &aRhs)
{
    return memcmp(aLhs.mExtendedPanId.m8, aRhs.mExtendedPanId.m8, sizeof(aLhs.mExtendedPanId)) == 0;
}

TEST(RcpHostApi, JoinDuringPendingDetachSupersedesEarlierJoins)
{
    otbr::MainloopContext    mainloop;
    otbr::Host::RcpHost      host("wpan0", std::vector<const char *>(), /* aBackboneInterfaceName */ "",
                                  /* aDryRun */ false,
                                  /* aEnableAutoAttach */ false);
    otInstance              *instance;
    bool                     resultReceived = false;
    otOperationalDataset     datasets[3];
    otOperationalDatasetTlvs datasetTlvs[3];
    otOperationalDataset     activeDataset;
    otError                  errors[3]         = {OT_ERROR_FAILED, OT_ERROR_FAILED, OT_ERROR_FAILED};
    bool                     received[3]       = {false, false, false};
    otError                  migrationError    = OT_ERROR_FAILED;
    bool                     migrationReceived = false;

    host.Init();
    instance = ot::FakePlatform::CurrentInstance();

    for (size_t i = 0; i < 3; i++)
    {
        ASSERT_EQ(otDatasetCreateNewNetwork(instance, &datasets[i]), OT_ERROR_NONE);
        otDatasetConvertToTlvs(&datasets[i], &datasetTlvs[i]);
    }

    // Attach as leader on the first network.
    ASSERT_EQ(otDatasetSetActiveTlvs(instance, &datasetTlvs[0]), OT_ERROR_NONE);
    host.SetThreadEnabled(true, [&resultReceived](otError, const std::string &) { resultReceived = true; });
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 5,
                         [&]() { return resultReceived && host.GetDeviceRole() == OT_DEVICE_ROLE_LEADER; });
    ASSERT_EQ(host.GetDeviceRole(), OT_DEVICE_ROLE_LEADER);

    // Join the second, the third and then the current (first) network. Each join supersedes the
    // previous one right away, and the last one must not take the already-joined shortcut while a
    // graceful detach for an earlier join is pending.
    for (size_t i : {1, 2, 0})
    {
        host.Join(datasetTlvs[i], [&errors, &received, i](otError aError, const std::string &) {
            errors[i]   = aError;
            received[i] = true;
        });
    }

    EXPECT_TRUE(received[1]);
    EXPECT_TRUE(received[2]);
    EXPECT_FALSE(received[0]);
    EXPECT_EQ(errors[1], OT_ERROR_ABORT);
    EXPECT_EQ(errors[2], OT_ERROR_ABORT);

    // The device is still attached to the first network, so a migration must not be sent there.
    host.ScheduleMigration(datasetTlvs[1], [&migrationError, &migrationReceived](otError aError, const std::string &) {
        migrationError    = aError;
        migrationReceived = true;
    });

    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 10, [&]() {
        return migrationReceived && received[0] && host.GetDeviceRole() == OT_DEVICE_ROLE_LEADER;
    });

    EXPECT_TRUE(migrationReceived);
    EXPECT_EQ(migrationError, OT_ERROR_BUSY);

    EXPECT_TRUE(received[0]);
    EXPECT_EQ(errors[0], OT_ERROR_NONE);
    EXPECT_EQ(host.GetDeviceRole(), OT_DEVICE_ROLE_LEADER);
    ASSERT_EQ(otDatasetGetActive(instance, &activeDataset), OT_ERROR_NONE);
    EXPECT_TRUE(ExtendedPanIdsMatch(activeDataset, datasets[0]));

    host.Deinit();
}

TEST(RcpHostApi, JoinFromAbortedJoinReceiverIsAppliedAfterDetach)
{
    otbr::MainloopContext    mainloop;
    otbr::Host::RcpHost      host("wpan0", std::vector<const char *>(), /* aBackboneInterfaceName */ "",
                                  /* aDryRun */ false,
                                  /* aEnableAutoAttach */ false);
    otInstance              *instance;
    bool                     resultReceived = false;
    otOperationalDataset     datasets[3];
    otOperationalDatasetTlvs datasetTlvs[3];
    otOperationalDataset     activeDataset;
    otError                  errors[3]   = {OT_ERROR_FAILED, OT_ERROR_FAILED, OT_ERROR_FAILED};
    bool                     received[3] = {false, false, false};

    host.Init();
    instance = ot::FakePlatform::CurrentInstance();

    for (size_t i = 0; i < 3; i++)
    {
        ASSERT_EQ(otDatasetCreateNewNetwork(instance, &datasets[i]), OT_ERROR_NONE);
        otDatasetConvertToTlvs(&datasets[i], &datasetTlvs[i]);
    }

    host.SetThreadEnabled(true, [&resultReceived](otError, const std::string &) { resultReceived = true; });
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 1, [&resultReceived]() { return resultReceived; });
    ASSERT_TRUE(resultReceived);

    // The first join starts attaching right away. When the graceful detach requested by the
    // second join completes, the first join is aborted and its receiver joins the third network,
    // which must supersede the second join.
    host.Join(datasetTlvs[0], [&](otError aError, const std::string &) {
        errors[0]   = aError;
        received[0] = true;

        if (aError == OT_ERROR_ABORT)
        {
            host.Join(datasetTlvs[2], [&errors, &received](otError aError, const std::string &) {
                errors[2]   = aError;
                received[2] = true;
            });
        }
    });
    ASSERT_NE(host.GetDeviceRole(), OT_DEVICE_ROLE_DISABLED);

    host.Join(datasetTlvs[1], [&errors, &received](otError aError, const std::string &) {
        errors[1]   = aError;
        received[1] = true;
    });

    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 10,
                         [&]() { return received[2] && host.GetDeviceRole() == OT_DEVICE_ROLE_LEADER; });

    EXPECT_TRUE(received[0] && received[1] && received[2]);
    EXPECT_EQ(errors[0], OT_ERROR_ABORT);
    EXPECT_EQ(errors[1], OT_ERROR_ABORT);
    EXPECT_EQ(errors[2], OT_ERROR_NONE);
    EXPECT_EQ(host.GetDeviceRole(), OT_DEVICE_ROLE_LEADER);
    ASSERT_EQ(otDatasetGetActive(instance, &activeDataset), OT_ERROR_NONE);
    EXPECT_TRUE(ExtendedPanIdsMatch(activeDataset, datasets[2]));

    host.Deinit();
}

TEST(RcpHostApi, JoinSucceedsAfterResetDuringPendingDetach)
{
    otbr::MainloopContext    mainloop;
    otbr::Host::RcpHost      host("wpan0", std::vector<const char *>(), /* aBackboneInterfaceName */ "",
                                  /* aDryRun */ false,
                                  /* aEnableAutoAttach */ false);
    otInstance              *instance;
    bool                     resultReceived = false;
    otOperationalDataset     datasets[3];
    otOperationalDatasetTlvs datasetTlvs[3];
    otOperationalDataset     activeDataset;
    otError                  errors[3]   = {OT_ERROR_FAILED, OT_ERROR_FAILED, OT_ERROR_FAILED};
    bool                     received[3] = {false, false, false};

    host.Init();
    instance = ot::FakePlatform::CurrentInstance();

    for (size_t i = 0; i < 3; i++)
    {
        ASSERT_EQ(otDatasetCreateNewNetwork(instance, &datasets[i]), OT_ERROR_NONE);
        otDatasetConvertToTlvs(&datasets[i], &datasetTlvs[i]);
    }

    // Attach as leader on the first network.
    ASSERT_EQ(otDatasetSetActiveTlvs(instance, &datasetTlvs[0]), OT_ERROR_NONE);
    host.SetThreadEnabled(true, [&resultReceived](otError, const std::string &) { resultReceived = true; });
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 5,
                         [&]() { return resultReceived && host.GetDeviceRole() == OT_DEVICE_ROLE_LEADER; });
    ASSERT_EQ(host.GetDeviceRole(), OT_DEVICE_ROLE_LEADER);

    // Reset while the graceful detach for this join is pending.
    host.Join(datasetTlvs[1], [&errors, &received](otError aError, const std::string &) {
        errors[1]   = aError;
        received[1] = true;
    });
    host.Reset();

    EXPECT_TRUE(received[1]);
    EXPECT_EQ(errors[1], OT_ERROR_ABORT);

    instance = ot::FakePlatform::CurrentInstance();
    host.Join(datasetTlvs[2], [&errors, &received](otError aError, const std::string &) {
        errors[2]   = aError;
        received[2] = true;
    });

    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 10,
                         [&]() { return received[2] && host.GetDeviceRole() == OT_DEVICE_ROLE_LEADER; });

    EXPECT_TRUE(received[2]);
    EXPECT_EQ(errors[2], OT_ERROR_NONE);
    EXPECT_EQ(host.GetDeviceRole(), OT_DEVICE_ROLE_LEADER);
    ASSERT_EQ(otDatasetGetActive(instance, &activeDataset), OT_ERROR_NONE);
    EXPECT_TRUE(ExtendedPanIdsMatch(activeDataset, datasets[2]));

    host.Deinit();
}

TEST(RcpHostApi, ResetFromAbortedJoinReceiverAbortsLeave)
{
    otbr::MainloopContext    mainloop;
    otbr::Host::RcpHost      host("wpan0", std::vector<const char *>(), /* aBackboneInterfaceName */ "",
                                  /* aDryRun */ false,
                                  /* aEnableAutoAttach */ false);
    otInstance              *instance;
    bool                     resultReceived = false;
    otOperationalDataset     datasets[2];
    otOperationalDatasetTlvs datasetTlvs[2];
    otError                  joinError    = OT_ERROR_FAILED;
    bool                     joinReceived = false;
    otError                  leaveError   = OT_ERROR_FAILED;
    std::string              leaveErrorMsg;
    bool                     leaveReceived = false;

    host.Init();
    instance = ot::FakePlatform::CurrentInstance();

    for (size_t i = 0; i < 2; i++)
    {
        ASSERT_EQ(otDatasetCreateNewNetwork(instance, &datasets[i]), OT_ERROR_NONE);
        otDatasetConvertToTlvs(&datasets[i], &datasetTlvs[i]);
    }

    // Attach as leader on the first network.
    ASSERT_EQ(otDatasetSetActiveTlvs(instance, &datasetTlvs[0]), OT_ERROR_NONE);
    host.SetThreadEnabled(true, [&resultReceived](otError, const std::string &) { resultReceived = true; });
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 5,
                         [&]() { return resultReceived && host.GetDeviceRole() == OT_DEVICE_ROLE_LEADER; });
    ASSERT_EQ(host.GetDeviceRole(), OT_DEVICE_ROLE_LEADER);

    // The leave aborts the pending join, whose receiver resets the host. The leave must not
    // resume on the recreated instance.
    host.Join(datasetTlvs[1], [&](otError aError, const std::string &) {
        joinError    = aError;
        joinReceived = true;
        host.Reset();
    });
    host.Leave(/* aEraseDataset */ true, [&](otError aError, const std::string &aErrorMsg) {
        leaveError    = aError;
        leaveErrorMsg = aErrorMsg;
        leaveReceived = true;
    });

    EXPECT_TRUE(joinReceived);
    EXPECT_EQ(joinError, OT_ERROR_ABORT);
    EXPECT_TRUE(leaveReceived);
    EXPECT_EQ(leaveError, OT_ERROR_ABORT);
    EXPECT_STREQ(leaveErrorMsg.c_str(), "Aborted by reset");

    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 1, [] { return false; });
    EXPECT_EQ(host.GetDeviceRole(), OT_DEVICE_ROLE_DISABLED);

    host.Deinit();
}
