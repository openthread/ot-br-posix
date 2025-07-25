/*
 *  Copyright (c) 2025, The OpenThread Authors.
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
    , mPskd{nullptr}
    , mStateString{nullptr}
{
    // Guaranteed that Validate has run already
    cJSON *value;

    mJoiner.mType = OT_JOINER_INFO_TYPE_ANY;
    memset(&mJoiner.mSharedId.mEui64, 0, sizeof(mJoiner.mSharedId.mEui64));
    memset(&mJoiner.mPskd.m8, 0, sizeof(mJoiner.mPskd.m8));

    value = cJSON_GetObjectItemCaseSensitive(mJson, KEY_PSKD);
    strncpy(mJoiner.mPskd.m8, value->valuestring, OT_JOINER_MAX_PSKD_LENGTH);
    mPskd = value->valuestring;

    value = cJSON_GetObjectItemCaseSensitive(mJson, KEY_JOINERID);
    if (cJSON_IsString(value))
    {
        if (strncmp(value->valuestring, "*", 1) != 0)
        {
            otbrError err = Json::StringDiscerner2Discerner(value->valuestring, mJoiner.mSharedId.mDiscerner);
            if (err == OTBR_ERROR_NOT_FOUND)
            {
                VerifyOrExit(Json::Hex2BytesJsonString(std::string(value->valuestring), mJoiner.mSharedId.mEui64.m8,
                                                       OT_EXT_ADDRESS_SIZE) == OT_EXT_ADDRESS_SIZE);
                mJoiner.mType = OT_JOINER_INFO_TYPE_EUI64;
            }
            else
            {
                VerifyOrExit(err == OTBR_ERROR_NONE);
                mJoiner.mType = OT_JOINER_INFO_TYPE_DISCERNER;
            }
        }
    }

    value = cJSON_GetObjectItemCaseSensitive(mJson, KEY_DISCERNER);
    if (cJSON_IsString(value))
    {
        if (strncmp(value->valuestring, "*", 1) != 0)
        {
            VerifyOrExit(Json::StringDiscerner2Discerner(value->valuestring, mJoiner.mSharedId.mDiscerner) ==
                         OTBR_ERROR_NONE);
            mJoiner.mType = OT_JOINER_INFO_TYPE_DISCERNER;
        }
    }

    value = cJSON_GetObjectItemCaseSensitive(mJson, KEY_EUI);
    if (cJSON_IsString(value))
    {
        if (strncmp(value->valuestring, "*", 1) != 0)
        {
            VerifyOrExit(Json::Hex2BytesJsonString(std::string(value->valuestring), mJoiner.mSharedId.mEui64.m8,
                                                   OT_EXT_ADDRESS_SIZE) == OT_EXT_ADDRESS_SIZE);
            mJoiner.mType = OT_JOINER_INFO_TYPE_EUI64;
        }
    }

    // timeout is from the parent class

exit:
    mStatus = kActionStatusPending;
    Update();
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
    otError                                 error;

    switch (mStatus)
    {
    case kActionStatusPending:
        error = mServices.GetCommissionerManager().AddJoiner(mJoiner, static_cast<uint32_t>(mTimeout.count()));
        if (error != OT_ERROR_NONE)
        {
            break;
        }
        mStatus = kActionStatusActive;
        OT_FALL_THROUGH;

    case kActionStatusActive:
        joiner = mServices.GetCommissionerManager().FindJoiner(mJoiner);
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
            mServices.GetCommissionerManager().RemoveJoiner(mJoiner);
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
        mServices.GetCommissionerManager().RemoveJoiner(mJoiner);
        mStatus = kActionStatusStopped;
        break;

    default:
        break;
    }
}

cJSON *AddThreadDevice::Jsonify(std::set<std::string> aFieldset)
{
    cJSON      *attributes = cJSON_CreateObject();
    const char *status     = GetStatusString();

    if (mJoiner.mType == OT_JOINER_INFO_TYPE_DISCERNER)
    {
        if (hasKey(aFieldset, KEY_DISCERNER))
        {
            char hexValue[((OT_JOINER_MAX_DISCERNER_LENGTH / 8) * 2) + 1]                              = {0};
            char string[sizeof("0x") + ((OT_JOINER_MAX_DISCERNER_LENGTH / 8) * 2) + sizeof("/xx") + 1] = {0};

            otbr::Utils::Long2Hex(mJoiner.mSharedId.mDiscerner.mValue, hexValue);
            snprintf(string, sizeof(string), "0x%s/%d", hexValue, mJoiner.mSharedId.mDiscerner.mLength);
            cJSON_AddItemToObject(attributes, KEY_DISCERNER, cJSON_CreateString(string));
        }
    }
    else if (mJoiner.mType == OT_JOINER_INFO_TYPE_EUI64)
    {
        if (hasKey(aFieldset, KEY_EUI))
        {
            std::string eui = Json::Bytes2HexJsonString(mJoiner.mSharedId.mEui64.m8, OT_EXT_ADDRESS_SIZE);
            cJSON_AddItemToObject(attributes, KEY_EUI, cJSON_Parse(eui.c_str()));
        }
    }
    else
    {
        if (hasKey(aFieldset, KEY_JOINERID))
        {
            cJSON_AddItemToObject(attributes, KEY_JOINERID, cJSON_CreateString("*"));
        }
    }

    if (hasKey(aFieldset, KEY_PSKD))
    {
        cJSON_AddItemToObject(attributes, KEY_PSKD, cJSON_CreateString(mPskd));
    }

    if (IsPendingOrActive())
    {
        if (hasKey(aFieldset, KEY_TIMEOUT))
        {
            JsonifyTimeout(*attributes);
        }
    }

    if (mStatus == kActionStatusActive)
    {
        status = mStateString;
    }
    if (hasKey(aFieldset, KEY_STATUS))
    {
        cJSON_AddItemToObject(attributes, KEY_STATUS, cJSON_CreateString(status));
    }

    return attributes;
}

bool AddThreadDevice::Validate(const cJSON &aJson)
{
    bool         ok       = false;
    std::string  errormsg = "ok";
    otExtAddress eui64;
    const char  *pskdStr;
    uint32_t     pskdLen;

    cJSON  *eui       = cJSON_GetObjectItemCaseSensitive(&aJson, KEY_EUI);
    cJSON  *discerner = cJSON_GetObjectItemCaseSensitive(&aJson, KEY_DISCERNER);
    cJSON  *joinerId  = cJSON_GetObjectItemCaseSensitive(&aJson, KEY_JOINERID);
    cJSON  *pskd      = cJSON_GetObjectItemCaseSensitive(&aJson, KEY_PSKD);
    Seconds timeout   = kDefaultTimeout;

    VerifyOrExit(ReadTimeout(aJson, timeout) != OT_ERROR_PARSE, errormsg = KEY_TIMEOUT);

    VerifyOrExit(((eui != nullptr) || (discerner != nullptr) || (joinerId != nullptr)),
                 errormsg = "No exclusive eui/discerner/joinerId");
    if (eui != nullptr)
    {
        VerifyOrExit(discerner == nullptr, errormsg = "eui and discerner are exclusive");
        VerifyOrExit(joinerId == nullptr, errormsg = "eui and joinerId are exclusive");
        VerifyOrExit(cJSON_IsString(eui), errormsg = "eui not a string");
        VerifyOrExit(strlen(eui->valuestring) == 16, errormsg = "eui length invalid");
        SuccessOrExit(str_to_m8(eui64.m8, eui->valuestring, OT_EXT_ADDRESS_SIZE), errormsg = "eui invalid");
    }
    else if (discerner != nullptr)
    {
        VerifyOrExit(joinerId == nullptr, errormsg = "discerner and joinerId are exclusive");
        VerifyOrExit(cJSON_IsString(discerner), errormsg = "discerner not a string");
    }
    else if (joinerId != nullptr)
    {
        VerifyOrExit(discerner == nullptr, errormsg = "joinerId and discerner are exclusive");
        VerifyOrExit(cJSON_IsString(joinerId), errormsg = "joinerId not a string");
    }

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
