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
#include "lib/spinel/spinel_encoder.hpp"
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
    mSpinelDriver              = nullptr;
    mIp6AddressTableCallback   = nullptr;
    mNetifStateChangedCallback = nullptr;
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
    EncodingFunc encodingFunc = [&aActiveOpDatasetTlvs](ot::Spinel::Encoder &aEncoder) {
        return aEncoder.WriteData(aActiveOpDatasetTlvs.mTlvs, aActiveOpDatasetTlvs.mLength);
    };

    VerifyOrExit(mDatasetSetActiveTask == nullptr, error = OT_ERROR_BUSY);

    SuccessOrExit(error = SetProperty(SPINEL_PROP_THREAD_ACTIVE_DATASET_TLVS, encodingFunc));
    mDatasetSetActiveTask = aAsyncTask;

exit:
    if (error != OT_ERROR_NONE)
    {
        mTaskRunner.Post([aAsyncTask, error](void) { aAsyncTask->SetResult(error, "Failed to set active dataset!"); });
    }
}

void NcpSpinel::DatasetMgmtSetPending(std::shared_ptr<otOperationalDatasetTlvs> aPendingOpDatasetTlvsPtr,
                                      AsyncTaskPtr                              aAsyncTask)
{
    otError      error        = OT_ERROR_NONE;
    EncodingFunc encodingFunc = [aPendingOpDatasetTlvsPtr](ot::Spinel::Encoder &aEncoder) {
        return aEncoder.WriteData(aPendingOpDatasetTlvsPtr->mTlvs, aPendingOpDatasetTlvsPtr->mLength);
    };

    VerifyOrExit(mDatasetMgmtSetPendingTask == nullptr, error = OT_ERROR_BUSY);

    SuccessOrExit(error = SetProperty(SPINEL_PROP_THREAD_MGMT_SET_PENDING_DATASET_TLVS, encodingFunc));
    mDatasetMgmtSetPendingTask = aAsyncTask;

exit:
    if (error != OT_ERROR_NONE)
    {
        mTaskRunner.Post([aAsyncTask, error] { aAsyncTask->SetResult(error, "Failed to set pending dataset!"); });
    }
}

void NcpSpinel::Ip6SetEnabled(bool aEnable, AsyncTaskPtr aAsyncTask)
{
    otError      error        = OT_ERROR_NONE;
    EncodingFunc encodingFunc = [aEnable](ot::Spinel::Encoder &aEncoder) { return aEncoder.WriteBool(aEnable); };

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

otbrError NcpSpinel::Ip6Send(const uint8_t *aData, uint16_t aLength)
{
    otbrError    error        = OTBR_ERROR_NONE;
    EncodingFunc encodingFunc = [aData, aLength](ot::Spinel::Encoder &aEncoder) {
        return aEncoder.WriteDataWithLen(aData, aLength);
    };

    SuccessOrExit(SetProperty(SPINEL_PROP_STREAM_NET, encodingFunc), error = OTBR_ERROR_OPENTHREAD);

exit:
    return error;
}

void NcpSpinel::ThreadSetEnabled(bool aEnable, AsyncTaskPtr aAsyncTask)
{
    otError      error        = OT_ERROR_NONE;
    EncodingFunc encodingFunc = [aEnable](ot::Spinel::Encoder &aEncoder) { return aEncoder.WriteBool(aEnable); };

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
    EncodingFunc encodingFunc = [](ot::Spinel::Encoder &) { return OT_ERROR_NONE; };

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

    VerifyOrExit(mThreadErasePersistentInfoTask == nullptr, error = OT_ERROR_BUSY);

    SuccessOrExit(error = mSpinelDriver->SendCommand(SPINEL_CMD_NET_CLEAR, SPINEL_PROP_LAST_STATUS, tid));

    mWaitingKeyTable[tid]          = SPINEL_PROP_LAST_STATUS;
    mCmdTable[tid]                 = SPINEL_CMD_NET_CLEAR;
    mThreadErasePersistentInfoTask = aAsyncTask;

exit:
    if (error != OT_ERROR_NONE)
    {
        FreeTidTableItem(tid);
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
    otbrLogResult(error, "%s", __FUNCTION__);
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

    switch (mCmdTable[aTid])
    {
    case SPINEL_CMD_PROP_VALUE_SET:
    {
        error = HandleResponseForPropSet(aTid, key, data, len);
        break;
    }
    case SPINEL_CMD_PROP_VALUE_INSERT:
    {
        error = HandleResponseForPropInsert(aTid, cmd, key, data, len);
        break;
    }
    case SPINEL_CMD_PROP_VALUE_REMOVE:
    {
        error = HandleResponseForPropRemove(aTid, cmd, key, data, len);
        break;
    }
    case SPINEL_CMD_NET_CLEAR:
    {
        spinel_status_t status = SPINEL_STATUS_OK;

        SuccessOrExit(error = SpinelDataUnpack(data, len, SPINEL_DATATYPE_UINT_PACKED_S, &status));
        CallAndClear(mThreadErasePersistentInfoTask, ot::Spinel::SpinelStatusToOtError(status));
        break;
    }
    default:
        break;
    }

exit:
    if (error == OTBR_ERROR_INVALID_STATE)
    {
        otbrLogCrit("Received unexpected response with (cmd:%u, key:%u), waiting (cmd:%u, key:%u) for tid:%u", cmd, key,
                    mCmdTable[aTid], mWaitingKeyTable[aTid], aTid);
    }
    else if (error == OTBR_ERROR_PARSE)
    {
        otbrLogCrit("Error parsing response with tid:%u", aTid);
    }
    FreeTidTableItem(aTid);
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
        spinel_net_role_t role = SPINEL_NET_ROLE_DISABLED;
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

    case SPINEL_PROP_THREAD_MGMT_SET_PENDING_DATASET_TLVS:
    {
        spinel_status_t status = SPINEL_STATUS_OK;

        SuccessOrExit(error = SpinelDataUnpack(aBuffer, aLength, SPINEL_DATATYPE_UINT_PACKED_S, &status));
        CallAndClear(mDatasetMgmtSetPendingTask, ot::Spinel::SpinelStatusToOtError(status));
        break;
    }

    case SPINEL_PROP_IPV6_ADDRESS_TABLE:
    {
        std::vector<Ip6AddressInfo> addressInfoTable;

        VerifyOrExit(ParseIp6AddressTable(aBuffer, aLength, addressInfoTable) == OT_ERROR_NONE,
                     error = OTBR_ERROR_PARSE);
        SafeInvoke(mIp6AddressTableCallback, addressInfoTable);
        break;
    }

    case SPINEL_PROP_IPV6_MULTICAST_ADDRESS_TABLE:
    {
        std::vector<Ip6Address> addressTable;

        VerifyOrExit(ParseIp6MulticastAddresses(aBuffer, aLength, addressTable) == OT_ERROR_NONE,
                     error = OTBR_ERROR_PARSE);
        SafeInvoke(mIp6MulticastAddressTableCallback, addressTable);
        break;
    }

    case SPINEL_PROP_NET_IF_UP:
    {
        bool isUp;
        SuccessOrExit(error = SpinelDataUnpack(aBuffer, aLength, SPINEL_DATATYPE_BOOL_S, &isUp));
        SafeInvoke(mNetifStateChangedCallback, isUp);
        break;
    }

    case SPINEL_PROP_STREAM_NET:
    {
        const uint8_t *data;
        uint16_t       dataLen;

        SuccessOrExit(ParseIp6StreamNet(aBuffer, aLength, data, dataLen), error = OTBR_ERROR_PARSE);
        SafeInvoke(mIp6ReceiveCallback, data, dataLen);
        break;
    }

    case SPINEL_PROP_INFRA_IF_SEND_ICMP6:
    {
        uint32_t            infraIfIndex;
        const otIp6Address *destAddress;
        const uint8_t      *data;
        uint16_t            dataLen;

        SuccessOrExit(ParseInfraIfIcmp6Nd(aBuffer, aLength, infraIfIndex, destAddress, data, dataLen),
                      error = OTBR_ERROR_PARSE);
        SafeInvoke(mInfraIfIcmp6NdCallback, infraIfIndex, *destAddress, data, dataLen);
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

otbrError NcpSpinel::HandleResponseForPropSet(spinel_tid_t      aTid,
                                              spinel_prop_key_t aKey,
                                              const uint8_t    *aData,
                                              uint16_t          aLength)
{
    OTBR_UNUSED_VARIABLE(aData);
    OTBR_UNUSED_VARIABLE(aLength);

    otbrError       error  = OTBR_ERROR_NONE;
    spinel_status_t status = SPINEL_STATUS_OK;

    switch (mWaitingKeyTable[aTid])
    {
    case SPINEL_PROP_THREAD_ACTIVE_DATASET_TLVS:
        VerifyOrExit(aKey == SPINEL_PROP_THREAD_ACTIVE_DATASET_TLVS, error = OTBR_ERROR_INVALID_STATE);
        CallAndClear(mDatasetSetActiveTask, OT_ERROR_NONE);
        {
            otOperationalDatasetTlvs datasetTlvs;
            VerifyOrExit(ParseOperationalDatasetTlvs(aData, aLength, datasetTlvs) == OT_ERROR_NONE,
                         error = OTBR_ERROR_PARSE);
            mPropsObserver->SetDatasetActiveTlvs(datasetTlvs);
        }
        break;

    case SPINEL_PROP_NET_IF_UP:
        VerifyOrExit(aKey == SPINEL_PROP_NET_IF_UP, error = OTBR_ERROR_INVALID_STATE);
        CallAndClear(mIp6SetEnabledTask, OT_ERROR_NONE);
        {
            bool isUp;
            SuccessOrExit(error = SpinelDataUnpack(aData, aLength, SPINEL_DATATYPE_BOOL_S, &isUp));
            SafeInvoke(mNetifStateChangedCallback, isUp);
        }
        break;

    case SPINEL_PROP_NET_STACK_UP:
        VerifyOrExit(aKey == SPINEL_PROP_NET_STACK_UP, error = OTBR_ERROR_INVALID_STATE);
        CallAndClear(mThreadSetEnabledTask, OT_ERROR_NONE);
        break;

    case SPINEL_PROP_THREAD_MGMT_SET_PENDING_DATASET_TLVS:
        if (aKey == SPINEL_PROP_LAST_STATUS)
        { // Failed case
            SuccessOrExit(error = SpinelDataUnpack(aData, aLength, SPINEL_DATATYPE_UINT_PACKED_S, &status));
            CallAndClear(mDatasetMgmtSetPendingTask, ot::Spinel::SpinelStatusToOtError(status));
        }
        else if (aKey != SPINEL_PROP_THREAD_MGMT_SET_PENDING_DATASET_TLVS)
        {
            ExitNow(error = OTBR_ERROR_INVALID_STATE);
        }
        break;

    case SPINEL_PROP_STREAM_NET:
        break;

    case SPINEL_PROP_INFRA_IF_STATE:
        VerifyOrExit(aKey == SPINEL_PROP_LAST_STATUS, error = OTBR_ERROR_INVALID_STATE);
        SuccessOrExit(error = SpinelDataUnpack(aData, aLength, SPINEL_DATATYPE_UINT_PACKED_S, &status));
        otbrLogInfo("Infra If state update result: %s", spinel_status_to_cstr(status));
        break;

    case SPINEL_PROP_INFRA_IF_RECV_ICMP6:
        VerifyOrExit(aKey == SPINEL_PROP_LAST_STATUS, error = OTBR_ERROR_INVALID_STATE);
        SuccessOrExit(error = SpinelDataUnpack(aData, aLength, SPINEL_DATATYPE_UINT_PACKED_S, &status));
        otbrLogInfo("Infra If handle ICMP6 ND result: %s", spinel_status_to_cstr(status));
        break;

    default:
        VerifyOrExit(aKey == mWaitingKeyTable[aTid], error = OTBR_ERROR_INVALID_STATE);
        break;
    }

exit:
    return error;
}

otbrError NcpSpinel::HandleResponseForPropInsert(spinel_tid_t      aTid,
                                                 spinel_command_t  aCmd,
                                                 spinel_prop_key_t aKey,
                                                 const uint8_t    *aData,
                                                 uint16_t          aLength)
{
    otbrError error = OTBR_ERROR_NONE;

    switch (mWaitingKeyTable[aTid])
    {
    case SPINEL_PROP_IPV6_MULTICAST_ADDRESS_TABLE:
        if (aCmd == SPINEL_CMD_PROP_VALUE_IS)
        {
            spinel_status_t status = SPINEL_STATUS_OK;

            VerifyOrExit(aKey == SPINEL_PROP_LAST_STATUS, error = OTBR_ERROR_INVALID_STATE);
            SuccessOrExit(error = SpinelDataUnpack(aData, aLength, SPINEL_DATATYPE_UINT_PACKED_S, &status));
            otbrLogInfo("Failed to subscribe to multicast address on NCP, error:%s", spinel_status_to_cstr(status));
        }
        else
        {
            error = aCmd == SPINEL_CMD_PROP_VALUE_INSERTED ? OTBR_ERROR_NONE : OTBR_ERROR_INVALID_STATE;
        }
        break;
    default:
        break;
    }

exit:
    otbrLogResult(error, "HandleResponseForPropInsert, key:%u", mWaitingKeyTable[aTid]);
    return error;
}

otbrError NcpSpinel::HandleResponseForPropRemove(spinel_tid_t      aTid,
                                                 spinel_command_t  aCmd,
                                                 spinel_prop_key_t aKey,
                                                 const uint8_t    *aData,
                                                 uint16_t          aLength)
{
    otbrError error = OTBR_ERROR_NONE;

    switch (mWaitingKeyTable[aTid])
    {
    case SPINEL_PROP_IPV6_MULTICAST_ADDRESS_TABLE:
        if (aCmd == SPINEL_CMD_PROP_VALUE_IS)
        {
            spinel_status_t status = SPINEL_STATUS_OK;

            VerifyOrExit(aKey == SPINEL_PROP_LAST_STATUS, error = OTBR_ERROR_INVALID_STATE);
            SuccessOrExit(error = SpinelDataUnpack(aData, aLength, SPINEL_DATATYPE_UINT_PACKED_S, &status));
            otbrLogInfo("Failed to unsubscribe to multicast address on NCP, error:%s", spinel_status_to_cstr(status));
        }
        else
        {
            error = aCmd == SPINEL_CMD_PROP_VALUE_REMOVED ? OTBR_ERROR_NONE : OTBR_ERROR_INVALID_STATE;
        }
        break;
    default:
        break;
    }

exit:
    otbrLogResult(error, "HandleResponseForPropRemove, key:%u", mWaitingKeyTable[aTid]);
    return error;
}

otbrError NcpSpinel::Ip6MulAddrUpdateSubscription(const otIp6Address &aAddress, bool aIsAdded)
{
    otbrError    error        = OTBR_ERROR_NONE;
    EncodingFunc encodingFunc = [&aAddress](ot::Spinel::Encoder &aEncoder) {
        return aEncoder.WriteIp6Address(aAddress);
    };

    if (aIsAdded)
    {
        SuccessOrExit(InsertProperty(SPINEL_PROP_IPV6_MULTICAST_ADDRESS_TABLE, encodingFunc),
                      error = OTBR_ERROR_OPENTHREAD);
    }
    else
    {
        SuccessOrExit(RemoveProperty(SPINEL_PROP_IPV6_MULTICAST_ADDRESS_TABLE, encodingFunc),
                      error = OTBR_ERROR_OPENTHREAD);
    }

exit:
    return error;
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

void NcpSpinel::FreeTidTableItem(spinel_tid_t aTid)
{
    mCmdTidsInUse &= ~(1 << aTid);

    mCmdTable[aTid]        = SPINEL_CMD_NOOP;
    mWaitingKeyTable[aTid] = SPINEL_PROP_LAST_STATUS;
}

otError NcpSpinel::SendCommand(spinel_command_t aCmd, spinel_prop_key_t aKey, const EncodingFunc &aEncodingFunc)
{
    otError      error  = OT_ERROR_NONE;
    spinel_tid_t tid    = GetNextTid();
    uint8_t      header = SPINEL_HEADER_FLAG | SPINEL_HEADER_IID(mIid) | tid;

    VerifyOrExit(tid != 0, error = OT_ERROR_BUSY);
    SuccessOrExit(error = mEncoder.BeginFrame(header, aCmd, aKey));
    SuccessOrExit(error = aEncodingFunc(mEncoder));
    SuccessOrExit(error = mEncoder.EndFrame());
    SuccessOrExit(error = SendEncodedFrame());

    mCmdTable[tid]        = aCmd;
    mWaitingKeyTable[tid] = aKey;
exit:
    if (error != OT_ERROR_NONE)
    {
        FreeTidTableItem(tid);
    }
    return error;
}

otError NcpSpinel::SetProperty(spinel_prop_key_t aKey, const EncodingFunc &aEncodingFunc)
{
    return SendCommand(SPINEL_CMD_PROP_VALUE_SET, aKey, aEncodingFunc);
}

otError NcpSpinel::InsertProperty(spinel_prop_key_t aKey, const EncodingFunc &aEncodingFunc)
{
    return SendCommand(SPINEL_CMD_PROP_VALUE_INSERT, aKey, aEncodingFunc);
}

otError NcpSpinel::RemoveProperty(spinel_prop_key_t aKey, const EncodingFunc &aEncodingFunc)
{
    return SendCommand(SPINEL_CMD_PROP_VALUE_REMOVE, aKey, aEncodingFunc);
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

otError NcpSpinel::ParseIp6AddressTable(const uint8_t               *aBuf,
                                        uint16_t                     aLength,
                                        std::vector<Ip6AddressInfo> &aAddressTable)
{
    otError             error = OT_ERROR_NONE;
    ot::Spinel::Decoder decoder;

    VerifyOrExit(aBuf != nullptr, error = OT_ERROR_INVALID_ARGS);
    decoder.Init(aBuf, aLength);

    while (!decoder.IsAllReadInStruct())
    {
        Ip6AddressInfo      cur;
        const otIp6Address *addr;
        uint8_t             prefixLength;
        uint32_t            preferredLifetime;
        uint32_t            validLifetime;

        SuccessOrExit(error = decoder.OpenStruct());
        SuccessOrExit(error = decoder.ReadIp6Address(addr));
        memcpy(&cur.mAddress, addr, sizeof(otIp6Address));
        SuccessOrExit(error = decoder.ReadUint8(prefixLength));
        cur.mPrefixLength = prefixLength;
        SuccessOrExit(error = decoder.ReadUint32(preferredLifetime));
        cur.mPreferred = preferredLifetime ? true : false;
        SuccessOrExit(error = decoder.ReadUint32(validLifetime));
        OTBR_UNUSED_VARIABLE(validLifetime);
        SuccessOrExit((error = decoder.CloseStruct()));

        aAddressTable.push_back(cur);
    }

exit:
    return error;
}

otError NcpSpinel::ParseIp6MulticastAddresses(const uint8_t *aBuf, uint16_t aLen, std::vector<Ip6Address> &aAddressList)
{
    otError             error = OT_ERROR_NONE;
    ot::Spinel::Decoder decoder;

    VerifyOrExit(aBuf != nullptr, error = OT_ERROR_INVALID_ARGS);

    decoder.Init(aBuf, aLen);

    while (!decoder.IsAllReadInStruct())
    {
        const otIp6Address *addr;

        SuccessOrExit(error = decoder.OpenStruct());
        SuccessOrExit(error = decoder.ReadIp6Address(addr));
        aAddressList.emplace_back(Ip6Address(*addr));
        SuccessOrExit((error = decoder.CloseStruct()));
    }

exit:
    return error;
}

otError NcpSpinel::ParseIp6StreamNet(const uint8_t *aBuf, uint16_t aLen, const uint8_t *&aData, uint16_t &aDataLen)
{
    otError             error = OT_ERROR_NONE;
    ot::Spinel::Decoder decoder;

    VerifyOrExit(aBuf != nullptr, error = OT_ERROR_INVALID_ARGS);

    decoder.Init(aBuf, aLen);
    error = decoder.ReadDataWithLen(aData, aDataLen);

exit:
    return error;
}

otError NcpSpinel::ParseOperationalDatasetTlvs(const uint8_t            *aBuf,
                                               uint16_t                  aLen,
                                               otOperationalDatasetTlvs &aDatasetTlvs)
{
    otError             error = OT_ERROR_NONE;
    ot::Spinel::Decoder decoder;
    const uint8_t      *datasetTlvsData;
    uint16_t            datasetTlvsLen;

    decoder.Init(aBuf, aLen);
    SuccessOrExit(error = decoder.ReadData(datasetTlvsData, datasetTlvsLen));
    VerifyOrExit(datasetTlvsLen <= sizeof(aDatasetTlvs.mTlvs), error = OT_ERROR_PARSE);

    memcpy(aDatasetTlvs.mTlvs, datasetTlvsData, datasetTlvsLen);
    aDatasetTlvs.mLength = datasetTlvsLen;

exit:
    return error;
}

otError NcpSpinel::ParseInfraIfIcmp6Nd(const uint8_t       *aBuf,
                                       uint8_t              aLen,
                                       uint32_t            &aInfraIfIndex,
                                       const otIp6Address *&aAddr,
                                       const uint8_t      *&aData,
                                       uint16_t            &aDataLen)
{
    otError             error = OT_ERROR_NONE;
    ot::Spinel::Decoder decoder;

    VerifyOrExit(aBuf != nullptr, error = OT_ERROR_INVALID_ARGS);

    decoder.Init(aBuf, aLen);
    SuccessOrExit(error = decoder.ReadUint32(aInfraIfIndex));
    SuccessOrExit(error = decoder.ReadIp6Address(aAddr));
    SuccessOrExit(error = decoder.ReadDataWithLen(aData, aDataLen));

exit:
    return error;
}

otbrError NcpSpinel::SetInfraIf(uint32_t aInfraIfIndex, bool aIsRunning, const std::vector<Ip6Address> &aIp6Addresses)
{
    otbrError    error        = OTBR_ERROR_NONE;
    EncodingFunc encodingFunc = [aInfraIfIndex, aIsRunning, &aIp6Addresses](ot::Spinel::Encoder &aEncoder) {
        otError error = OT_ERROR_NONE;
        SuccessOrExit(error = aEncoder.WriteUint32(aInfraIfIndex));
        SuccessOrExit(error = aEncoder.WriteBool(aIsRunning));
        for (const Ip6Address &addr : aIp6Addresses)
        {
            SuccessOrExit(error = aEncoder.WriteIp6Address(reinterpret_cast<const otIp6Address &>(addr)));
        }

    exit:
        return error;
    };

    SuccessOrExit(SetProperty(SPINEL_PROP_INFRA_IF_STATE, encodingFunc), error = OTBR_ERROR_OPENTHREAD);

exit:
    return error;
}

otbrError NcpSpinel::HandleIcmp6Nd(uint32_t          aInfraIfIndex,
                                   const Ip6Address &aIp6Address,
                                   const uint8_t    *aData,
                                   uint16_t          aDataLen)
{
    otbrError    error        = OTBR_ERROR_NONE;
    EncodingFunc encodingFunc = [aInfraIfIndex, &aIp6Address, aData, aDataLen](ot::Spinel::Encoder &aEncoder) {
        otError error = OT_ERROR_NONE;
        SuccessOrExit(error = aEncoder.WriteUint32(aInfraIfIndex));
        SuccessOrExit(error = aEncoder.WriteIp6Address(reinterpret_cast<const otIp6Address &>(aIp6Address)));
        SuccessOrExit(error = aEncoder.WriteData(aData, aDataLen));
    exit:
        return error;
    };

    SuccessOrExit(SetProperty(SPINEL_PROP_INFRA_IF_RECV_ICMP6, encodingFunc), error = OTBR_ERROR_OPENTHREAD);

exit:
    if (error != OTBR_ERROR_NONE)
    {
        otbrLogWarning("Failed to passthrough ICMP6 ND to NCP, %s", otbrErrorString(error));
    }
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
