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

#include <openthread/thread.h>

#include "common/code_utils.hpp"
#include "common/logging.hpp"
#include "lib/spinel/spinel.h"
#include "lib/spinel/spinel_driver.hpp"

namespace otbr {
namespace Ncp {

NcpSpinel::NcpSpinel(void)
    : mSpinelDriver(nullptr)
    , mCmdTidsInUse(0)
    , mCmdNextTid(1)
    , mDeviceRole(OT_DEVICE_ROLE_DISABLED)
    , mGetDeviceRoleHandler(nullptr)
{
    memset(mWaitingKeyTable, 0xff, sizeof(mWaitingKeyTable));
}

NcpSpinel::~NcpSpinel(void) = default;

void NcpSpinel::Init(ot::Spinel::SpinelDriver *aSpinelDriver)
{
    mSpinelDriver = aSpinelDriver;
    mSpinelDriver->SetFrameHandler(&HandleReceivedFrame, &HandleSavedFrame, this);
}

void NcpSpinel::Deinit(void)
{
    mSpinelDriver = nullptr;
}

void NcpSpinel::GetDeviceRole(GetDeviceRoleHandler aHandler)
{
    otError      error = OT_ERROR_NONE;
    spinel_tid_t tid   = GetNextTid();
    va_list      args;

    error = mSpinelDriver->SendCommand(SPINEL_CMD_PROP_VALUE_GET, SPINEL_PROP_NET_ROLE, tid, nullptr, args);
    if (error != OT_ERROR_NONE)
    {
        FreeTid(tid);
        aHandler(error, OT_DEVICE_ROLE_DISABLED);
    }
    mWaitingKeyTable[tid] = SPINEL_PROP_NET_ROLE;

    mGetDeviceRoleHandler = aHandler;

    return;
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
    OT_UNUSED_VARIABLE(aFrame);
    OT_UNUSED_VARIABLE(aLength);
    OT_UNUSED_VARIABLE(aContext);
}

void NcpSpinel::HandleNotification(const uint8_t *aFrame, uint16_t aLength)
{
    spinel_prop_key_t key;
    spinel_size_t     len = 0;
    spinel_ssize_t    unpacked;
    uint8_t          *data = nullptr;
    uint32_t          cmd;
    uint8_t           header;
    otbrError         error = OTBR_ERROR_NONE;

    unpacked = spinel_datatype_unpack(aFrame, aLength, "CiiD", &header, &cmd, &key, &data, &len);
    VerifyOrExit(unpacked > 0, error = OTBR_ERROR_PARSE);
    VerifyOrExit(SPINEL_HEADER_GET_TID(header) == 0, error = OTBR_ERROR_PARSE);
    VerifyOrExit(cmd == SPINEL_CMD_PROP_VALUE_IS);
    HandleValueIs(key, data, static_cast<uint16_t>(len));

exit:
    if (error != OTBR_ERROR_NONE)
    {
        otbrLogResult(error, "HandleNotification: %s", __FUNCTION__);
    }
}

void NcpSpinel::HandleResponse(spinel_tid_t aTid, const uint8_t *aFrame, uint16_t aLength)
{
    spinel_prop_key_t key;
    spinel_size_t     len = 0;
    spinel_ssize_t    unpacked;
    uint8_t          *data = nullptr;
    uint32_t          cmd;
    uint8_t           header;
    otError           error = OT_ERROR_NONE;

    unpacked = spinel_datatype_unpack(aFrame, aLength, "CiiD", &header, &cmd, &key, &data, &len);
    VerifyOrExit(unpacked > 0, error = OT_ERROR_PARSE);

    VerifyOrExit(cmd == SPINEL_CMD_PROP_VALUE_IS && key == mWaitingKeyTable[aTid], error = OT_ERROR_INVALID_STATE);

    if (key == SPINEL_PROP_NET_ROLE)
    {
        spinel_net_role_t spinelRole;
        otDeviceRole      role;
        unpacked = spinel_datatype_unpack(data, len, "i", &spinelRole);

        switch (spinelRole)
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
        }

        if (mGetDeviceRoleHandler)
        {
            mGetDeviceRoleHandler(error, role);
        }
    }

exit:
    if (error == OT_ERROR_INVALID_STATE)
    {
        otbrLogCrit("Received unexpected response with cmd:%u, key:%u, waiting key:%u for tid:%u", cmd, key,
                    mWaitingKeyTable[aTid], aTid);
    }
}

void NcpSpinel::HandleValueIs(spinel_prop_key_t aKey, const uint8_t *aBuffer, uint16_t aLength)
{
    otbrError      error = OTBR_ERROR_NONE;
    spinel_ssize_t unpacked;

    if (aKey == SPINEL_PROP_LAST_STATUS)
    {
        spinel_status_t status = SPINEL_STATUS_OK;

        unpacked = spinel_datatype_unpack(aBuffer, aLength, "i", &status);
        VerifyOrExit(unpacked > 0, error = OTBR_ERROR_PARSE);

        otbrLogInfo("NCP last status: %s", spinel_status_to_cstr(status));
    }
    else if (aKey == SPINEL_PROP_NET_ROLE)
    {
        spinel_net_role_t role;

        unpacked = spinel_datatype_unpack(aBuffer, aLength, "C", &role);
        VerifyOrExit(unpacked > 0, error = OTBR_ERROR_PARSE);

        switch (role)
        {
        case SPINEL_NET_ROLE_DETACHED:
            mDeviceRole = OT_DEVICE_ROLE_DETACHED;
            break;
        case SPINEL_NET_ROLE_CHILD:
            mDeviceRole = OT_DEVICE_ROLE_CHILD;
            break;
        case SPINEL_NET_ROLE_ROUTER:
            mDeviceRole = OT_DEVICE_ROLE_ROUTER;
            break;
        case SPINEL_NET_ROLE_LEADER:
            mDeviceRole = OT_DEVICE_ROLE_LEADER;
            break;
        case SPINEL_NET_ROLE_DISABLED:
            mDeviceRole = OT_DEVICE_ROLE_DISABLED;
            break;
        }

        otbrLogInfo("Device role changed to %s", otThreadDeviceRoleToString(mDeviceRole));
    }

exit:
    otbrLogResult(error, "NcpSpinel: %s", __FUNCTION__);
    return;
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

} // namespace Ncp
} // namespace otbr
