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

#define OTBR_LOG_TAG "NcpSpinel"

#include "ncp_spinel.hpp"

#include <stdarg.h>

#include <algorithm>

#include <openthread/dataset.h>
#include <openthread/thread.h>

#include "common/code_utils.hpp"
#include "common/logging.hpp"
#include "lib/spinel/spinel.h"
#include "lib/spinel/spinel_decoder.hpp"
#include "lib/spinel/spinel_driver.hpp"
#include "lib/spinel/spinel_helper.hpp"

namespace otbr {
namespace Ncp {

static constexpr char kSpinelDataUnpackFormat[] = "CiiD";

NcpSpinel::NcpSpinel(void)
    : mSpinelDriver(nullptr)
    , mCmdTidsInUse(0)
    , mCmdNextTid(1)
    , mNcpBuffer(mTxBuffer, kTxBufferSize)
    , mEncoder(mNcpBuffer)
    , mIid(SPINEL_HEADER_INVALID_IID)
    , mPropsObserver(nullptr)
{
    std::fill_n(mWaitingKeyTable, SPINEL_PROP_LAST_STATUS, sizeof(mWaitingKeyTable));
    memset(mCmdTable, 0, sizeof(mCmdTable));
}

void NcpSpinel::Init(ot::Spinel::SpinelDriver &aSpinelDriver, PropsObserver &aObserver)
{
    mSpinelDriver  = &aSpinelDriver;
    mPropsObserver = &aObserver;
    mIid           = mSpinelDriver->GetIid();
    mSpinelDriver->SetFrameHandler(&HandleReceivedFrame, &HandleSavedFrame, this);
}

void NcpSpinel::Deinit(void)
{
    mSpinelDriver = nullptr;
}

otbrError NcpSpinel::SpinelDataUnpack(const uint8_t *aDataIn, spinel_size_t aDataLen, const char *aPackFormat, ...)
{
    otbrError      error = OTBR_ERROR_NONE;
    spinel_ssize_t unpacked;
    va_list        args;

    va_start(args, aPackFormat);
    unpacked = spinel_datatype_vunpack(aDataIn, aDataLen, aPackFormat, args);
    va_end(args);

    VerifyOrExit(unpacked > 0, error = OTBR_ERROR_PARSE);

exit:
    return error;
}

void NcpSpinel::DatasetSetActiveTlvs(const otOperationalDatasetTlvs &aActiveOpDatasetTlvs, AsyncTaskPtr aAsyncTask)
{
    otError      error        = OT_ERROR_NONE;
    EncodingFunc encodingFunc = [this, &aActiveOpDatasetTlvs] {
        return EncodeDatasetSetActiveTlvs(aActiveOpDatasetTlvs);
    };

    VerifyOrExit(mDatasetSetActiveTask == nullptr, error = OT_ERROR_BUSY);

    SuccessOrExit(error = SetProperty(SPINEL_PROP_THREAD_ACTIVE_DATASET, encodingFunc));
    mDatasetSetActiveTask = aAsyncTask;

exit:
    if (error != OT_ERROR_NONE)
    {
        mTaskRunner.Post([aAsyncTask, error](void) { aAsyncTask->SetResult(error, "Failed to set active dataset!"); });
    }
}

void NcpSpinel::Ip6SetEnabled(bool aEnable, AsyncTaskPtr aAsyncTask)
{
    otError      error        = OT_ERROR_NONE;
    EncodingFunc encodingFunc = [this, aEnable] { return mEncoder.WriteBool(aEnable); };

    VerifyOrExit(mIp6SetEnabledTask == nullptr, error = OT_ERROR_BUSY);

    SuccessOrExit(error = SetProperty(SPINEL_PROP_NET_IF_UP, encodingFunc));
    mIp6SetEnabledTask = aAsyncTask;

exit:
    if (error != OT_ERROR_NONE)
    {
        mTaskRunner.Post(
            [aAsyncTask, error](void) { aAsyncTask->SetResult(error, "Failed to enable the network interface!"); });
    }
    return;
}

void NcpSpinel::ThreadSetEnabled(bool aEnable, AsyncTaskPtr aAsyncTask)
{
    otError      error        = OT_ERROR_NONE;
    EncodingFunc encodingFunc = [this, aEnable] { return mEncoder.WriteBool(aEnable); };

    VerifyOrExit(mThreadSetEnabledTask == nullptr, error = OT_ERROR_BUSY);

    SuccessOrExit(error = SetProperty(SPINEL_PROP_NET_STACK_UP, encodingFunc));
    mThreadSetEnabledTask = aAsyncTask;

exit:
    if (error != OT_ERROR_NONE)
    {
        mTaskRunner.Post(
            [aAsyncTask, error](void) { aAsyncTask->SetResult(error, "Failed to enable the Thread network!"); });
    }
    return;
}

void NcpSpinel::ThreadDetachGracefully(AsyncTaskPtr aAsyncTask)
{
    otError      error        = OT_ERROR_NONE;
    EncodingFunc encodingFunc = [] { return OT_ERROR_NONE; };

    VerifyOrExit(mThreadDetachGracefullyTask == nullptr, error = OT_ERROR_BUSY);

    SuccessOrExit(error = SetProperty(SPINEL_PROP_NET_LEAVE_GRACEFULLY, encodingFunc));
    mThreadDetachGracefullyTask = aAsyncTask;

exit:
    if (error != OT_ERROR_NONE)
    {
        mTaskRunner.Post([aAsyncTask, error](void) { aAsyncTask->SetResult(error, "Failed to detach gracefully!"); });
    }
    return;
}

void NcpSpinel::ThreadErasePersistentInfo(AsyncTaskPtr aAsyncTask)
{
    otError      error = OT_ERROR_NONE;
    spinel_tid_t tid   = GetNextTid();
    va_list      args;

    VerifyOrExit(mThreadErasePersistentInfoTask == nullptr, error = OT_ERROR_BUSY);

    SuccessOrExit(error =
                      mSpinelDriver->SendCommand(SPINEL_CMD_NET_CLEAR, SPINEL_PROP_LAST_STATUS, tid, nullptr, args));

    mWaitingKeyTable[tid]          = SPINEL_PROP_LAST_STATUS;
    mCmdTable[tid]                 = SPINEL_CMD_NET_CLEAR;
    mThreadErasePersistentInfoTask = aAsyncTask;

exit:
    if (error != OT_ERROR_NONE)
    {
        FreeTid(tid);
        mTaskRunner.Post(
            [aAsyncTask, error](void) { aAsyncTask->SetResult(error, "Failed to erase persistent info!"); });
    }
}

void NcpSpinel::HandleReceivedFrame(const uint8_t *aFrame,
                                    uint16_t       aLength,
                                    uint8_t        aHeader,
                                    bool          &aSave,
                                    void          *aContext)
{
    static_cast<NcpSpinel *>(aContext)->HandleReceivedFrame(aFrame, aLength, aHeader, aSave);
}

void NcpSpinel::HandleReceivedFrame(const uint8_t *aFrame, uint16_t aLength, uint8_t aHeader, bool &aShouldSaveFrame)
{
    spinel_tid_t tid = SPINEL_HEADER_GET_TID(aHeader);

    if (tid == 0)
    {
        HandleNotification(aFrame, aLength);
    }
    else if (tid < kMaxTids)
    {
        HandleResponse(tid, aFrame, aLength);
    }
    else
    {
        otbrLogCrit("Received unexpected tid: %u", tid);
    }

    aShouldSaveFrame = false;
}

void NcpSpinel::HandleSavedFrame(const uint8_t *aFrame, uint16_t aLength, void *aContext)
{
    /* Intentionally Empty */
    OT_UNUSED_VARIABLE(aFrame);
    OT_UNUSED_VARIABLE(aLength);
    OT_UNUSED_VARIABLE(aContext);
}

void NcpSpinel::HandleNotification(const uint8_t *aFrame, uint16_t aLength)
{
    spinel_prop_key_t key;
    spinel_size_t     len  = 0;
    uint8_t          *data = nullptr;
    uint32_t          cmd;
    uint8_t           header;
    otbrError         error = OTBR_ERROR_NONE;

    SuccessOrExit(error = SpinelDataUnpack(aFrame, aLength, kSpinelDataUnpackFormat, &header, &cmd, &key, &data, &len));
    VerifyOrExit(SPINEL_HEADER_GET_TID(header) == 0, error = OTBR_ERROR_PARSE);
    VerifyOrExit(cmd == SPINEL_CMD_PROP_VALUE_IS);
    HandleValueIs(key, data, static_cast<uint16_t>(len));

exit:
    otbrLogResult(error, "HandleNotification: %s", __FUNCTION__);
}

void NcpSpinel::HandleResponse(spinel_tid_t aTid, const uint8_t *aFrame, uint16_t aLength)
{
    spinel_prop_key_t key;
    spinel_size_t     len  = 0;
    uint8_t          *data = nullptr;
    uint32_t          cmd;
    uint8_t           header;
    otbrError         error          = OTBR_ERROR_NONE;
    FailureHandler    failureHandler = nullptr;

    SuccessOrExit(error = SpinelDataUnpack(aFrame, aLength, kSpinelDataUnpackFormat, &header, &cmd, &key, &data, &len));

    VerifyOrExit(cmd == SPINEL_CMD_PROP_VALUE_IS && key == mWaitingKeyTable[aTid], error = OTBR_ERROR_INVALID_STATE);

    switch (key)
    {
    case SPINEL_PROP_LAST_STATUS:
    {
        spinel_status_t status = SPINEL_STATUS_OK;

        failureHandler = [this, aTid](otError aError) { HandleResponseForCommand(aTid, aError); };
        SuccessOrExit(error = SpinelDataUnpack(data, len, SPINEL_DATATYPE_UINT_PACKED_S, &status));

        HandleResponseForCommand(aTid, ot::Spinel::SpinelStatusToOtError(status));
        break;
    }
    case SPINEL_PROP_THREAD_ACTIVE_DATASET:
    {
        CallAndClear(mDatasetSetActiveTask, OT_ERROR_NONE);
        break;
    }
    case SPINEL_PROP_NET_IF_UP:
    {
        CallAndClear(mIp6SetEnabledTask, OT_ERROR_NONE);
        break;
    }
    case SPINEL_PROP_NET_STACK_UP:
    {
        CallAndClear(mThreadSetEnabledTask, OT_ERROR_NONE);
        break;
    }
    default:
        break;
    }

exit:
    FreeTid(aTid);

    if (error == OTBR_ERROR_INVALID_STATE)
    {
        otbrLogCrit("Received unexpected response with cmd:%u, key:%u, waiting key:%u for tid:%u", cmd, key,
                    mWaitingKeyTable[aTid], aTid);
    }
    else if (error == OTBR_ERROR_PARSE)
    {
        otbrLogCrit("Error parsing response with tid:%u", aTid);
        if (failureHandler)
        {
            failureHandler(OtbrErrorToOtError(error));
        }
    }
}

void NcpSpinel::HandleValueIs(spinel_prop_key_t aKey, const uint8_t *aBuffer, uint16_t aLength)
{
    otbrError error = OTBR_ERROR_NONE;

    switch (aKey)
    {
    case SPINEL_PROP_LAST_STATUS:
    {
        spinel_status_t status = SPINEL_STATUS_OK;

        SuccessOrExit(error = SpinelDataUnpack(aBuffer, aLength, SPINEL_DATATYPE_UINT_PACKED_S, &status));

        otbrLogInfo("NCP last status: %s", spinel_status_to_cstr(status));
        break;
    }

    case SPINEL_PROP_NET_ROLE:
    {
        spinel_net_role_t role;
        otDeviceRole      deviceRole;

        SuccessOrExit(error = SpinelDataUnpack(aBuffer, aLength, SPINEL_DATATYPE_UINT8_S, &role));

        deviceRole = SpinelRoleToDeviceRole(role);
        mPropsObserver->SetDeviceRole(deviceRole);

        otbrLogInfo("Device role changed to %s", otThreadDeviceRoleToString(deviceRole));
        break;
    }

    case SPINEL_PROP_NET_LEAVE_GRACEFULLY:
    {
        CallAndClear(mThreadDetachGracefullyTask, OT_ERROR_NONE);
        break;
    }

    default:
        otbrLogWarning("Received uncognized key: %u", aKey);
        break;
    }

exit:
    otbrLogResult(error, "NcpSpinel: %s", __FUNCTION__);
    return;
}

void NcpSpinel::HandleResponseForCommand(spinel_tid_t aTid, otError aError)
{
    switch (mCmdTable[aTid])
    {
    case SPINEL_CMD_NET_CLEAR:
    {
        CallAndClear(mThreadErasePersistentInfoTask, aError);
        break;
    }
    default:
        break;
    }

    mCmdTable[aTid] = SPINEL_CMD_NOOP;
}

spinel_tid_t NcpSpinel::GetNextTid(void)
{
    spinel_tid_t tid = mCmdNextTid;

    while (((1 << tid) & mCmdTidsInUse) != 0)
    {
        tid = SPINEL_GET_NEXT_TID(tid);

        if (tid == mCmdNextTid)
        {
            // We looped back to `mCmdNextTid` indicating that all
            // TIDs are in-use.

            ExitNow(tid = 0);
        }
    }

    mCmdTidsInUse |= (1 << tid);
    mCmdNextTid = SPINEL_GET_NEXT_TID(tid);

exit:
    return tid;
}

otError NcpSpinel::SetProperty(spinel_prop_key_t aKey, const EncodingFunc &aEncodingFunc)
{
    otError      error  = OT_ERROR_NONE;
    spinel_tid_t tid    = GetNextTid();
    uint8_t      header = SPINEL_HEADER_FLAG | SPINEL_HEADER_IID(mIid) | tid;

    VerifyOrExit(tid != 0, error = OT_ERROR_BUSY);
    SuccessOrExit(error = mEncoder.BeginFrame(header, SPINEL_CMD_PROP_VALUE_SET, aKey));
    SuccessOrExit(error = aEncodingFunc());
    SuccessOrExit(error = mEncoder.EndFrame());
    SuccessOrExit(error = SendEncodedFrame());

    mWaitingKeyTable[tid] = aKey;
exit:
    if (error != OT_ERROR_NONE)
    {
        FreeTid(tid);
    }
    return error;
}

otError NcpSpinel::SendEncodedFrame(void)
{
    otError  error = OT_ERROR_NONE;
    uint8_t  frame[kTxBufferSize];
    uint16_t frameLength;

    SuccessOrExit(error = mNcpBuffer.OutFrameBegin());
    frameLength = mNcpBuffer.OutFrameGetLength();
    VerifyOrExit(mNcpBuffer.OutFrameRead(frameLength, frame) == frameLength, error = OT_ERROR_FAILED);
    SuccessOrExit(error = mSpinelDriver->GetSpinelInterface()->SendFrame(frame, frameLength));

exit:
    error = mNcpBuffer.OutFrameRemove();
    return error;
}

void NcpSpinel::GetFlagsFromSecurityPolicy(const otSecurityPolicy *aSecurityPolicy,
                                           uint8_t                *aFlags,
                                           uint8_t                 aFlagsLength)
{
    static constexpr uint8_t kObtainNetworkKeyMask        = 1 << 7; // Byte 0
    static constexpr uint8_t kNativeCommissioningMask     = 1 << 6; // Byte 0
    static constexpr uint8_t kRoutersMask                 = 1 << 5; // Byte 0
    static constexpr uint8_t kExternalCommissioningMask   = 1 << 4; // Byte 0
    static constexpr uint8_t kCommercialCommissioningMask = 1 << 2; // Byte 0
    static constexpr uint8_t kAutonomousEnrollmentMask    = 1 << 1; // Byte 0
    static constexpr uint8_t kNetworkKeyProvisioningMask  = 1 << 0; // Byte 0

    static constexpr uint8_t kTobleLinkMask     = 1 << 7; // Byte 1
    static constexpr uint8_t kNonCcmRoutersMask = 1 << 6; // Byte 1
    static constexpr uint8_t kReservedMask      = 0x38;   // Byte 1

    VerifyOrExit(aFlagsLength > 1);

    memset(aFlags, 0, aFlagsLength);

    if (aSecurityPolicy->mObtainNetworkKeyEnabled)
    {
        aFlags[0] |= kObtainNetworkKeyMask;
    }

    if (aSecurityPolicy->mNativeCommissioningEnabled)
    {
        aFlags[0] |= kNativeCommissioningMask;
    }

    if (aSecurityPolicy->mRoutersEnabled)
    {
        aFlags[0] |= kRoutersMask;
    }

    if (aSecurityPolicy->mExternalCommissioningEnabled)
    {
        aFlags[0] |= kExternalCommissioningMask;
    }

    if (!aSecurityPolicy->mCommercialCommissioningEnabled)
    {
        aFlags[0] |= kCommercialCommissioningMask;
    }

    if (!aSecurityPolicy->mAutonomousEnrollmentEnabled)
    {
        aFlags[0] |= kAutonomousEnrollmentMask;
    }

    if (!aSecurityPolicy->mNetworkKeyProvisioningEnabled)
    {
        aFlags[0] |= kNetworkKeyProvisioningMask;
    }

    VerifyOrExit(aFlagsLength > sizeof(aFlags[0]));

    if (aSecurityPolicy->mTobleLinkEnabled)
    {
        aFlags[1] |= kTobleLinkMask;
    }

    if (!aSecurityPolicy->mNonCcmRoutersEnabled)
    {
        aFlags[1] |= kNonCcmRoutersMask;
    }

    aFlags[1] |= kReservedMask;
    aFlags[1] |= aSecurityPolicy->mVersionThresholdForRouting;

exit:
    return;
}

otError NcpSpinel::EncodeDatasetSetActiveTlvs(const otOperationalDatasetTlvs &aActiveOpDatasetTlvs)
{
    otError              error = OT_ERROR_NONE;
    otOperationalDataset dataset;
    otIp6Address         addrMlPrefix;
    uint8_t              flagsSecurityPolicy[2];

    SuccessOrExit(error = otDatasetParseTlvs(&aActiveOpDatasetTlvs, &dataset));
    GetFlagsFromSecurityPolicy(&dataset.mSecurityPolicy, flagsSecurityPolicy, sizeof(flagsSecurityPolicy));
    memcpy(addrMlPrefix.mFields.m8, dataset.mMeshLocalPrefix.m8, sizeof(otMeshLocalPrefix));
    memset(addrMlPrefix.mFields.m8 + 8, 0, sizeof(otMeshLocalPrefix));

    SuccessOrExit(error = mEncoder.OpenStruct());
    SuccessOrExit(error = mEncoder.WriteUintPacked(SPINEL_PROP_DATASET_ACTIVE_TIMESTAMP));
    SuccessOrExit(error = mEncoder.WriteUint64(dataset.mActiveTimestamp.mSeconds));
    SuccessOrExit(error = mEncoder.CloseStruct());

    SuccessOrExit(error = mEncoder.OpenStruct());
    SuccessOrExit(error = mEncoder.WriteUintPacked(SPINEL_PROP_NET_NETWORK_KEY));
    SuccessOrExit(error = mEncoder.WriteData(dataset.mNetworkKey.m8, sizeof(dataset.mNetworkKey.m8)));
    SuccessOrExit(error = mEncoder.CloseStruct());

    SuccessOrExit(error = mEncoder.OpenStruct());
    SuccessOrExit(error = mEncoder.WriteUintPacked(SPINEL_PROP_NET_NETWORK_NAME));
    SuccessOrExit(error = mEncoder.WriteUtf8(dataset.mNetworkName.m8));
    SuccessOrExit(error = mEncoder.CloseStruct());

    SuccessOrExit(error = mEncoder.OpenStruct());
    SuccessOrExit(error = mEncoder.WriteUintPacked(SPINEL_PROP_NET_XPANID));
    SuccessOrExit(error = mEncoder.WriteData(dataset.mExtendedPanId.m8, sizeof(dataset.mExtendedPanId.m8)));
    SuccessOrExit(error = mEncoder.CloseStruct());

    SuccessOrExit(error = mEncoder.OpenStruct());
    SuccessOrExit(error = mEncoder.WriteUintPacked(SPINEL_PROP_IPV6_ML_PREFIX));
    SuccessOrExit(error = mEncoder.WriteIp6Address(addrMlPrefix));
    SuccessOrExit(error = mEncoder.WriteUint8(OT_IP6_PREFIX_BITSIZE));
    SuccessOrExit(error = mEncoder.CloseStruct());

    SuccessOrExit(error = mEncoder.OpenStruct());
    SuccessOrExit(error = mEncoder.WriteUintPacked(SPINEL_PROP_MAC_15_4_PANID));
    SuccessOrExit(error = mEncoder.WriteUint16(dataset.mPanId));
    SuccessOrExit(error = mEncoder.CloseStruct());

    SuccessOrExit(error = mEncoder.OpenStruct());
    SuccessOrExit(error = mEncoder.WriteUintPacked(SPINEL_PROP_PHY_CHAN));
    SuccessOrExit(error = mEncoder.WriteUint16(dataset.mChannel));
    SuccessOrExit(error = mEncoder.CloseStruct());

    SuccessOrExit(error = mEncoder.OpenStruct());
    SuccessOrExit(error = mEncoder.WriteUintPacked(SPINEL_PROP_NET_PSKC));
    SuccessOrExit(error = mEncoder.WriteData(dataset.mPskc.m8, sizeof(dataset.mPskc.m8)));
    SuccessOrExit(error = mEncoder.CloseStruct());

    SuccessOrExit(error = mEncoder.OpenStruct());
    SuccessOrExit(error = mEncoder.WriteUintPacked(SPINEL_PROP_DATASET_SECURITY_POLICY));
    SuccessOrExit(error = mEncoder.WriteUint16(dataset.mSecurityPolicy.mRotationTime));
    SuccessOrExit(error = mEncoder.WriteUint8(flagsSecurityPolicy[0]));
    SuccessOrExit(error = mEncoder.WriteUint8(flagsSecurityPolicy[1]));
    SuccessOrExit(error = mEncoder.CloseStruct());

exit:
    return error;
}

otDeviceRole NcpSpinel::SpinelRoleToDeviceRole(spinel_net_role_t aRole)
{
    otDeviceRole role = OT_DEVICE_ROLE_DISABLED;

    switch (aRole)
    {
    case SPINEL_NET_ROLE_DISABLED:
        role = OT_DEVICE_ROLE_DISABLED;
        break;
    case SPINEL_NET_ROLE_DETACHED:
        role = OT_DEVICE_ROLE_DETACHED;
        break;
    case SPINEL_NET_ROLE_CHILD:
        role = OT_DEVICE_ROLE_CHILD;
        break;
    case SPINEL_NET_ROLE_ROUTER:
        role = OT_DEVICE_ROLE_ROUTER;
        break;
    case SPINEL_NET_ROLE_LEADER:
        role = OT_DEVICE_ROLE_LEADER;
        break;
    default:
        otbrLogWarning("Unsupported spinel net role: %u", aRole);
        break;
    }

    return role;
}

} // namespace Ncp
} // namespace otbr
