/*
 *    Copyright (c) 2026, The OpenThread Authors.
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
#include <openthread/openthread-system.h>
#include <openthread/platform/misc.h>

#include "common/mainloop.hpp"
#include "common/mainloop_manager.hpp"
#include "host/ncp_host.hpp"
#include "host/thread_helper.hpp"
#include "lib/spinel/coprocessor_type.h"
#include "lib/spinel/spinel.h"
#include "lib/spinel/spinel_driver.hpp"
#include "lib/spinel/spinel_interface.hpp"

extern CoprocessorType sCoprocessorType;

void otPlatReset(otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);
}

namespace {

static void MainloopProcessUntil(otbr::MainloopContext    &aMainloop,
                                 uint32_t                  aTimeoutSec,
                                 std::function<bool(void)> aCondition)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(aTimeoutSec);

    while (!aCondition())
    {
        otbr::MainloopManager::GetInstance().Update(aMainloop);
        FD_ZERO(&aMainloop.mErrorFdSet);
        otbr::MainloopManager::GetInstance().Process(aMainloop);

        if (std::chrono::steady_clock::now() > deadline)
        {
            break;
        }
    }
}

static otOperationalDataset CreateTestActiveDataset(void)
{
    otOperationalDataset dataset{};

    dataset.mActiveTimestamp.mSeconds             = 1;
    dataset.mComponents.mIsActiveTimestampPresent = true;
    dataset.mChannel                              = 15;
    dataset.mComponents.mIsChannelPresent         = true;
    dataset.mChannelMask                          = 0x07fff800;
    dataset.mComponents.mIsChannelMaskPresent     = true;
    dataset.mExtendedPanId.m8[0]                  = 0x11;
    dataset.mComponents.mIsExtendedPanIdPresent   = true;
    dataset.mMeshLocalPrefix.m8[0]                = 0xfd;
    dataset.mComponents.mIsMeshLocalPrefixPresent = true;
    dataset.mNetworkKey.m8[0]                     = 0xaa;
    dataset.mComponents.mIsNetworkKeyPresent      = true;
    OT_UNUSED_VARIABLE(otNetworkNameFromString(&dataset.mNetworkName, "OpenThread-Test"));
    dataset.mComponents.mIsNetworkNamePresent    = true;
    dataset.mPanId                               = 0x1234;
    dataset.mComponents.mIsPanIdPresent          = true;
    dataset.mPskc.m8[0]                          = 0xbb;
    dataset.mComponents.mIsPskcPresent           = true;
    dataset.mSecurityPolicy.mRotationTime        = 672;
    dataset.mComponents.mIsSecurityPolicyPresent = true;

    return dataset;
}

class FakeNcpSpinelInterface : public ot::Spinel::SpinelInterface
{
public:
    otError Init(ReceiveFrameCallback aCallback, void *aCallbackContext, RxFrameBuffer &aFrameBuffer) override
    {
        mCallback        = aCallback;
        mCallbackContext = aCallbackContext;
        mFrameBuffer     = &aFrameBuffer;
        return OT_ERROR_NONE;
    }

    void Deinit(void) override {}

    otError SendFrame(const uint8_t *aFrame, uint16_t aLength) override
    {
        uint8_t           header = 0;
        uint32_t          cmd    = 0;
        spinel_prop_key_t key    = SPINEL_PROP_LAST_STATUS;
        uint8_t          *data   = nullptr;
        spinel_size_t     len    = 0;

        if (spinel_datatype_unpack(aFrame, aLength, "CiiD", &header, &cmd, &key, &data, &len) > 0)
        {
            mLastHeader = header;
            mLastCmd    = cmd;
            mLastKey    = key;

            if (cmd == SPINEL_CMD_PROP_VALUE_SET && key == SPINEL_PROP_THREAD_MGMT_SET_PENDING_DATASET_TLVS &&
                len <= sizeof(mLastSentDatasetTlvs.mTlvs))
            {
                memcpy(mLastSentDatasetTlvs.mTlvs, data, len);
                mLastSentDatasetTlvs.mLength = static_cast<uint8_t>(len);
                mPendingMgmtSetHeader        = header;
                mHasPendingMgmtSetResponse   = true;
            }
        }

        return OT_ERROR_NONE;
    }

    otError WaitForFrame(uint64_t) override
    {
        uint8_t        frame[256];
        spinel_ssize_t packed = -1;

        if (!mSentResetStatus)
        {
            mSentResetStatus = true;
            packed = spinel_datatype_pack(frame, sizeof(frame), "Ciii", SPINEL_HEADER_FLAG, SPINEL_CMD_PROP_VALUE_IS,
                                          SPINEL_PROP_LAST_STATUS, SPINEL_STATUS_RESET_POWER_ON);
            PushRxFrame(frame, static_cast<uint16_t>(packed));
        }

        if (mLastKey == SPINEL_PROP_PROTOCOL_VERSION)
        {
            packed = spinel_datatype_pack(frame, sizeof(frame), "Ciiii", mLastHeader, SPINEL_CMD_PROP_VALUE_IS,
                                          SPINEL_PROP_PROTOCOL_VERSION, SPINEL_PROTOCOL_VERSION_THREAD_MAJOR,
                                          SPINEL_PROTOCOL_VERSION_THREAD_MINOR);
        }
        else if (mLastKey == SPINEL_PROP_NCP_VERSION)
        {
            packed = spinel_datatype_pack(frame, sizeof(frame), "CiiU", mLastHeader, SPINEL_CMD_PROP_VALUE_IS,
                                          SPINEL_PROP_NCP_VERSION, "OPENTHREAD/test");
        }
        else if (mLastKey == SPINEL_PROP_CAPS)
        {
            packed = spinel_datatype_pack(frame, sizeof(frame), "Ciii", mLastHeader, SPINEL_CMD_PROP_VALUE_IS,
                                          SPINEL_PROP_CAPS, SPINEL_CAP_CONFIG_FTD);
        }

        if (packed > 0)
        {
            PushRxFrame(frame, static_cast<uint16_t>(packed));
        }

        return OT_ERROR_NONE;
    }

    void PushRxFrame(const uint8_t *aFrame, uint16_t aLength)
    {
        for (uint16_t i = 0; i < aLength; ++i)
        {
            EXPECT_EQ(mFrameBuffer->WriteByte(aFrame[i]), OT_ERROR_NONE);
        }
        mCallback(mCallbackContext);
    }

    void UpdateFdSet(void *) override {}

    void Process(const void *) override
    {
        if (mHasPendingMgmtSetResponse)
        {
            uint8_t        respFrame[64];
            spinel_ssize_t packed;

            mHasPendingMgmtSetResponse = false;

            packed = spinel_datatype_pack(respFrame, sizeof(respFrame), "Cii", mPendingMgmtSetHeader,
                                          SPINEL_CMD_PROP_VALUE_IS, SPINEL_PROP_THREAD_MGMT_SET_PENDING_DATASET_TLVS);
            if (packed > 0)
            {
                PushRxFrame(respFrame, static_cast<uint16_t>(packed));
            }

            packed =
                spinel_datatype_pack(respFrame, sizeof(respFrame), "Ciii", SPINEL_HEADER_FLAG, SPINEL_CMD_PROP_VALUE_IS,
                                     SPINEL_PROP_THREAD_MGMT_SET_PENDING_DATASET_TLVS, SPINEL_STATUS_OK);
            if (packed > 0)
            {
                PushRxFrame(respFrame, static_cast<uint16_t>(packed));
            }
        }
    }

    uint32_t                     GetBusSpeed(void) const override { return 1000000; }
    otError                      HardwareReset(void) override { return OT_ERROR_NOT_IMPLEMENTED; }
    const otRcpInterfaceMetrics *GetRcpInterfaceMetrics(void) const override { return nullptr; }

    ReceiveFrameCallback     mCallback                  = nullptr;
    void                    *mCallbackContext           = nullptr;
    RxFrameBuffer           *mFrameBuffer               = nullptr;
    bool                     mSentResetStatus           = false;
    uint8_t                  mLastHeader                = 0;
    uint32_t                 mLastCmd                   = 0;
    spinel_prop_key_t        mLastKey                   = SPINEL_PROP_LAST_STATUS;
    uint8_t                  mPendingMgmtSetHeader      = 0;
    bool                     mHasPendingMgmtSetResponse = false;
    otOperationalDatasetTlvs mLastSentDatasetTlvs{};
};

class NcpHostApi : public ::testing::Test
{
protected:
    NcpHostApi(void)
        : mSpinelDriver(static_cast<ot::Spinel::SpinelDriver *>(otSysGetSpinelDriver()))
        , mNcpHost("wpan0", /* aBackboneInterfaceName */ "", /* aDryRun */ true, /* aDaemonMode */ 0)
    {
    }

    void SetUp(void) override
    {
        spinel_iid_t iidList[] = {0};

        sCoprocessorType = OT_COPROCESSOR_NCP;

        ASSERT_EQ(mSpinelDriver->Init(mFakeInterface, /* aSoftwareReset */ false, iidList, 1), OT_COPROCESSOR_NCP);
        mSpinelDriverInitialized = true;
        mNcpHost.Init();
        mNcpHostInitialized = true;
    }

    void TearDown(void) override
    {
        if (mNcpHostInitialized)
        {
            mNcpHost.Deinit();
        }
        if (mSpinelDriverInitialized)
        {
            mSpinelDriver->Deinit();
        }
        sCoprocessorType = OT_COPROCESSOR_UNKNOWN;
    }

    FakeNcpSpinelInterface    mFakeInterface;
    ot::Spinel::SpinelDriver *mSpinelDriver            = nullptr;
    bool                      mSpinelDriverInitialized = false;
    otbr::Host::NcpHost       mNcpHost;
    bool                      mNcpHostInitialized = false;
};

} // namespace

TEST_F(NcpHostApi, ScheduleMigration)
{
    otbr::Host::PropsObserver                  &propsObserver  = mNcpHost;
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

    otOperationalDataset     dataset = CreateTestActiveDataset();
    otOperationalDatasetTlvs activeDatasetTlvs{};
    otOperationalDatasetTlvs pendingDatasetTlvs{};

    otDatasetConvertToTlvs(&dataset, &activeDatasetTlvs);
    ASSERT_TRUE(otDatasetIsValid(&activeDatasetTlvs, /* aActive */ true));

    // 1. Reject ScheduleMigration when device is detached/disabled.
    mNcpHost.ScheduleMigration(activeDatasetTlvs, receiver);
    MainloopProcessUntil(mainloop, /* aTimeoutSec */ 0, [&resultReceived]() { return resultReceived; });
    EXPECT_TRUE(resultReceived);
    EXPECT_EQ(error, OT_ERROR_INVALID_STATE);

    propsObserver.SetDeviceRole(OT_DEVICE_ROLE_LEADER);

    // 2. When cached Active Dataset is empty/incomplete, an Active Dataset defaults to kDefaultMigrationDelayMs (300s).
    {
        otOperationalDataset     newActive = dataset;
        otOperationalDatasetTlvs newActiveTlvs{};
        newActive.mActiveTimestamp.mSeconds++;
        otDatasetConvertToTlvs(&newActive, &newActiveTlvs);

        error          = OT_ERROR_FAILED;
        resultReceived = false;
        mNcpHost.ScheduleMigration(newActiveTlvs, receiver);
        mSpinelDriver->Process(nullptr);
        MainloopProcessUntil(mainloop, /* aTimeoutSec */ 1, [&resultReceived]() { return resultReceived; });
        EXPECT_TRUE(resultReceived);
        EXPECT_EQ(error, OT_ERROR_NONE);

        otOperationalDataset parsedSent{};
        ASSERT_EQ(otDatasetParseTlvs(&mFakeInterface.mLastSentDatasetTlvs, &parsedSent), OT_ERROR_NONE);
        EXPECT_TRUE(parsedSent.mComponents.mIsDelayPresent);
        EXPECT_EQ(parsedSent.mDelay, otbr::Host::kDefaultMigrationDelayMs);
    }

    propsObserver.SetDatasetActiveTlvs(activeDatasetTlvs);

    // 3. Reject invalid Operational Dataset TLVs.
    {
        otOperationalDatasetTlvs invalidTlvs{};
        invalidTlvs.mLength = 3;

        error          = OT_ERROR_NONE;
        resultReceived = false;
        mNcpHost.ScheduleMigration(invalidTlvs, receiver);
        MainloopProcessUntil(mainloop, /* aTimeoutSec */ 0, [&resultReceived]() { return resultReceived; });
        EXPECT_TRUE(resultReceived);
        EXPECT_EQ(error, OT_ERROR_INVALID_ARGS);
    }

    // 4. Accept a valid Pending Dataset and forward its TLVs directly.
    {
        otOperationalDataset pendingInput = dataset;
        pendingInput.mActiveTimestamp.mSeconds++;
        pendingInput.mPendingTimestamp.mSeconds             = 10;
        pendingInput.mComponents.mIsPendingTimestampPresent = true;
        pendingInput.mDelay                                 = 45000;
        pendingInput.mComponents.mIsDelayPresent            = true;
        otDatasetConvertToTlvs(&pendingInput, &pendingDatasetTlvs);

        error          = OT_ERROR_FAILED;
        resultReceived = false;
        mNcpHost.ScheduleMigration(pendingDatasetTlvs, receiver);
        mSpinelDriver->Process(nullptr);
        MainloopProcessUntil(mainloop, /* aTimeoutSec */ 1, [&resultReceived]() { return resultReceived; });
        EXPECT_TRUE(resultReceived);
        EXPECT_EQ(error, OT_ERROR_NONE);

        otOperationalDataset parsedSent{};
        ASSERT_EQ(otDatasetParseTlvs(&mFakeInterface.mLastSentDatasetTlvs, &parsedSent), OT_ERROR_NONE);
        EXPECT_EQ(parsedSent.mPendingTimestamp.mSeconds, 10ULL);
        EXPECT_EQ(parsedSent.mDelay, 45000U);
    }

    // 5. Active Dataset that does NOT affect connectivity uses kMinMigrationDelayMs (30s)
    // and advances PendingTimestamp past the cached mDatasetPendingTlvs.
    {
        otOperationalDataset     cachedPending = dataset;
        otOperationalDatasetTlvs cachedPendingTlvs{};
        const uint64_t           futureSeconds = 20000000000ULL;

        cachedPending.mPendingTimestamp.mSeconds             = futureSeconds;
        cachedPending.mPendingTimestamp.mTicks               = 100;
        cachedPending.mComponents.mIsPendingTimestampPresent = true;
        cachedPending.mDelay                                 = 30000;
        cachedPending.mComponents.mIsDelayPresent            = true;
        otDatasetConvertToTlvs(&cachedPending, &cachedPendingTlvs);
        propsObserver.SetDatasetPendingTlvs(cachedPendingTlvs);

        otOperationalDataset     nonDisruptiveActive = dataset;
        otOperationalDatasetTlvs nonDisruptiveTlvs{};
        nonDisruptiveActive.mActiveTimestamp.mSeconds += 2;
        OT_UNUSED_VARIABLE(otNetworkNameFromString(&nonDisruptiveActive.mNetworkName, "NcpNonDisrupt"));
        otDatasetConvertToTlvs(&nonDisruptiveActive, &nonDisruptiveTlvs);

        error          = OT_ERROR_FAILED;
        resultReceived = false;
        mNcpHost.ScheduleMigration(nonDisruptiveTlvs, receiver);
        mSpinelDriver->Process(nullptr);
        MainloopProcessUntil(mainloop, /* aTimeoutSec */ 1, [&resultReceived]() { return resultReceived; });
        EXPECT_TRUE(resultReceived);
        EXPECT_EQ(error, OT_ERROR_NONE);

        otOperationalDataset parsedSent{};
        ASSERT_EQ(otDatasetParseTlvs(&mFakeInterface.mLastSentDatasetTlvs, &parsedSent), OT_ERROR_NONE);
        EXPECT_STREQ(parsedSent.mNetworkName.m8, "NcpNonDisrupt");
        EXPECT_TRUE(parsedSent.mComponents.mIsPendingTimestampPresent);
        EXPECT_EQ(parsedSent.mPendingTimestamp.mSeconds, futureSeconds);
        EXPECT_EQ(parsedSent.mPendingTimestamp.mTicks, 101U);
        EXPECT_TRUE(parsedSent.mComponents.mIsDelayPresent);
        EXPECT_EQ(parsedSent.mDelay, otbr::Host::kMinMigrationDelayMs);
    }

    // 6. Active Dataset that DOES affect connectivity (Channel, PAN ID, Mesh-Local Prefix, or Network Key)
    // uses kDefaultMigrationDelayMs (300s).
    {
        auto verifyDisruptiveDelay = [&](const otOperationalDataset &aModifiedDataset) {
            otOperationalDatasetTlvs disruptiveTlvs{};
            otDatasetConvertToTlvs(&aModifiedDataset, &disruptiveTlvs);

            error          = OT_ERROR_FAILED;
            resultReceived = false;
            mNcpHost.ScheduleMigration(disruptiveTlvs, receiver);
            mSpinelDriver->Process(nullptr);
            MainloopProcessUntil(mainloop, /* aTimeoutSec */ 1, [&resultReceived]() { return resultReceived; });
            EXPECT_TRUE(resultReceived);
            EXPECT_EQ(error, OT_ERROR_NONE);

            otOperationalDataset parsedSent{};
            ASSERT_EQ(otDatasetParseTlvs(&mFakeInterface.mLastSentDatasetTlvs, &parsedSent), OT_ERROR_NONE);
            EXPECT_TRUE(parsedSent.mComponents.mIsDelayPresent);
            EXPECT_EQ(parsedSent.mDelay, otbr::Host::kDefaultMigrationDelayMs);
        };

        // 6a. Channel change.
        otOperationalDataset channelChange = dataset;
        channelChange.mActiveTimestamp.mSeconds += 3;
        channelChange.mChannel = (dataset.mChannel == 11) ? 12 : 11;
        verifyDisruptiveDelay(channelChange);

        // 6b. PAN ID change.
        otOperationalDataset panIdChange = dataset;
        panIdChange.mActiveTimestamp.mSeconds += 4;
        panIdChange.mPanId = dataset.mPanId ^ 0x0001;
        verifyDisruptiveDelay(panIdChange);

        // 6c. Mesh-Local Prefix change.
        otOperationalDataset mlpChange = dataset;
        mlpChange.mActiveTimestamp.mSeconds += 5;
        mlpChange.mMeshLocalPrefix.m8[7] ^= 0x01;
        verifyDisruptiveDelay(mlpChange);

        // 6d. Network Key change.
        otOperationalDataset netKeyChange = dataset;
        netKeyChange.mActiveTimestamp.mSeconds += 6;
        netKeyChange.mNetworkKey.m8[0] ^= 0x01;
        verifyDisruptiveDelay(netKeyChange);
    }
}
