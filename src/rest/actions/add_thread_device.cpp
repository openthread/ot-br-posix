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

#include "add_thread_device.hpp"

#include <cctype>

#include "common/code_utils.hpp"

#include "rest/actions/handler.hpp"
#include "rest/commissioner_manager.hpp"
#include "rest/json.hpp"
#include "rest/rest_server_common.hpp"
#include "rest/services.hpp"

#include <cJSON.h>

namespace otbr {
namespace rest {
namespace actions {

// AddDeviceAction class implementation
AddThreadDevice::AddThreadDevice(const cJSON &aJson, Services &aServices)
    : BasicActions(aJson, aServices)
    , mEui64{0}
    , mPskd{nullptr}
    , mStateString{nullptr}
{
    cJSON *eui  = cJSON_GetObjectItemCaseSensitive(mJson, "eui");
    cJSON *pskd = cJSON_GetObjectItemCaseSensitive(mJson, "pskd");

    // Were guaranteed that Validate has run already
    IgnoreError(str_to_m8(mEui64.m8, eui->valuestring, OT_EXT_ADDRESS_SIZE));
    mPskd = pskd->valuestring;

    if (mServices.GetCommissionerManager().AddJoiner(mEui64, static_cast<uint32_t>(mTimeout.count()), mPskd) ==
        OT_ERROR_NONE)
    {
        mStateString = aServices.GetCommissionerManager().FindJoiner(mEui64)->GetStateString();
        mStatus      = kActionStatusActive;
    }
}

AddThreadDevice::~AddThreadDevice()
{
    // Make sure we always deregister properly
    Stop();
}

std::string AddThreadDevice::GetTypeName() const
{
    return std::string(ADD_DEVICE_ACTION_TYPE_NAME);
}

void AddThreadDevice::Update(void)
{
    const CommissionerManager::JoinerEntry *joiner;

    switch (mStatus)
    {
    case kActionStatusPending:
        if (mServices.GetCommissionerManager().AddJoiner(mEui64, static_cast<uint32_t>(mTimeout.count()), mPskd) !=
            OT_ERROR_NONE)
        {
            break;
        }
        mStatus = kActionStatusActive;
        OT_FALL_THROUGH;

    case kActionStatusActive:
        joiner = mServices.GetCommissionerManager().FindJoiner(mEui64);
        if (joiner == nullptr)
        {
            mStatus = kActionStatusFailed;
            break;
        }

        mStateString = joiner->GetStateString();
        switch (joiner->GetState())
        {
        case CommissionerManager::kJoinerStateJoined:
            mStatus = kActionStatusCompleted;
            break;

        case CommissionerManager::kJoinerStateFailed:
            mStatus = kActionStatusFailed;
            break;

        case CommissionerManager::kJoinerStateExpired:
            mStatus = kActionStatusStopped;
            break;

        default:
            break;
        }

        if (!IsPendingOrActive())
        {
            mServices.GetCommissionerManager().RemoveJoiner(mEui64);
        }
        break;

    default:
        break;
    }
}

void AddThreadDevice::Stop(void)
{
    switch (mStatus)
    {
    case kActionStatusPending:
        mStatus = kActionStatusStopped;
        break;

    case kActionStatusActive:
        mServices.GetCommissionerManager().RemoveJoiner(mEui64);
        mStatus = kActionStatusStopped;
        break;

    default:
        break;
    }
}

cJSON *AddThreadDevice::Jsonify(void)
{
    cJSON      *attributes = cJSON_CreateObject();
    std::string eui        = Json::Bytes2HexJsonString(mEui64.m8, OT_EXT_ADDRESS_SIZE);
    const char *status     = GetStatusString();

    cJSON_AddItemToObject(attributes, "eui", cJSON_Parse(eui.c_str()));
    cJSON_AddItemToObject(attributes, "pskd", cJSON_CreateString(mPskd));

    if (IsPendingOrActive())
    {
        JsonifyTimeout(attributes);
    }

    if (mStatus == kActionStatusActive)
    {
        status = mStateString;
    }
    cJSON_AddItemToObject(attributes, "status", cJSON_CreateString(status));

    return attributes;
}

bool AddThreadDevice::Validate(const cJSON &aJson)
{
    bool         ok       = false;
    std::string  errormsg = "ok";
    otExtAddress eui64;
    const char  *pskdStr;
    uint32_t     pskdLen;

    cJSON  *eui  = cJSON_GetObjectItemCaseSensitive(&aJson, "eui");
    cJSON  *pskd = cJSON_GetObjectItemCaseSensitive(&aJson, "pskd");
    Seconds timeout;

    SuccessOrExit(ReadTimeout(&aJson, timeout));

    VerifyOrExit(eui != nullptr, errormsg = "eui missing");
    VerifyOrExit(cJSON_IsString(eui), errormsg = "eui not a string");
    VerifyOrExit(strlen(eui->valuestring) == 16, errormsg = "eui length invalid");
    SuccessOrExit(str_to_m8(eui64.m8, eui->valuestring, OT_EXT_ADDRESS_SIZE), errormsg = "eui invalid");

    VerifyOrExit(pskd != nullptr, errormsg = "pskd missing");
    VerifyOrExit(cJSON_IsString(pskd), errormsg = "pskd not a string");

    pskdStr = pskd->valuestring;
    pskdLen = strlen(pskd->valuestring);
    VerifyOrExit(pskdLen >= 6 && pskdLen <= 32, errormsg = "pskd length invalid");
    errormsg = "pskd invalid";
    for (uint32_t i = 0; i < pskdLen; i++)
    {
        // VerifyOrExit(isalnum(pskdStr[i]));
        // VerifyOrExit(islower(pskdStr[i]));
        VerifyOrExit(pskdStr[i] != 'I');
        VerifyOrExit(pskdStr[i] != 'O');
        VerifyOrExit(pskdStr[i] != 'Q');
        VerifyOrExit(pskdStr[i] != 'Z');
    }

    ok = true;

exit:
    if (!ok)
    {
        otbrLogWarning("%s:%d Error (%s)", __FILE__, __LINE__, errormsg.c_str());
    }
    return ok;
}

} // namespace actions
} // namespace rest
} // namespace otbr
