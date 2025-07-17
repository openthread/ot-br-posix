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

#include <common/code_utils.hpp>

#include "action.hpp"

#include <array>
#include <cJSON.h>
#include "rest/json.hpp"

namespace otbr {
namespace rest {
namespace actions {

constexpr Seconds BasicActions::kDefaultTimeout;

// BasicAction class implementation item
BasicActions::BasicActions(const cJSON &aJson, Services &aServices)
    : BasicCollectionItem()
    , mCreatedSteady{Clock::now()}
    , mTimeout{ReadTimeoutOrDefault(aJson, kDefaultTimeout)}
    , mServices{aServices}
    , mJson{cJSON_Duplicate(&aJson, true)}
    , mStatus{kActionStatusPending}
{
    mRelationships = nullptr;
}

BasicActions::BasicActions(const cJSON &aJson, Seconds aTimeout, Services &aServices)
    : BasicCollectionItem()
    , mCreatedSteady{Clock::now()}
    , mTimeout{aTimeout}
    , mServices{aServices}
    , mJson{cJSON_Duplicate(&aJson, true)}
    , mStatus{kActionStatusPending}
{
    mRelationships = nullptr;
}

BasicActions::~BasicActions()
{
    if (mJson != nullptr)
    {
        cJSON_Delete(mJson);
        mJson = nullptr;
    }
}

std::string BasicActions::ToJsonString(std::set<std::string> aKeys)
{
    std::string ret;
    cJSON      *json = Jsonify(aKeys);

    VerifyOrExit(json != nullptr);

    ret = Json::Json2String(json);
    cJSON_Delete(json);

exit:
    return ret;
}

std::string BasicActions::ToJsonApiItem(std::set<std::string> aKeys)
{
    return Json::jsonStr2JsonApiItem(mUuid.ToString(), GetTypeName(), ToJsonStringTs(aKeys), mRelationships);
}

otError BasicActions::ReadTimeout(const cJSON &aJson, Seconds &aTimeout)
{
    otError error = OT_ERROR_NOT_FOUND;
    cJSON  *timeout;

    VerifyOrExit(cJSON_IsObject(&aJson));

    timeout = cJSON_GetObjectItemCaseSensitive(&aJson, KEY_TIMEOUT);

    VerifyOrExit(timeout != nullptr);
    VerifyOrExit(cJSON_IsNumber(timeout), error = OT_ERROR_PARSE);

    aTimeout = Seconds(timeout->valueint);
    error    = OT_ERROR_NONE;

exit:
    return error;
}

Seconds BasicActions::ReadTimeoutOrDefault(const cJSON &aJson, Seconds aDefault)
{
    IgnoreError(ReadTimeout(aJson, aDefault));
    return aDefault;
}

const char *BasicActions::ReadDestination(const cJSON &aJson, AddressType &aType)
{
    const char *str = nullptr;
    size_t      addressLength;
    cJSON      *address;
    cJSON      *type;

    VerifyOrExit(cJSON_IsObject(&aJson));

    address = cJSON_GetObjectItemCaseSensitive(&aJson, "destination");
    type    = cJSON_GetObjectItemCaseSensitive(&aJson, "destinationType");
    VerifyOrExit(address != nullptr);
    VerifyOrExit(cJSON_IsString(address));
    addressLength = strlen(address->valuestring);

    if (type != nullptr)
    {
        const char *typestr;
        VerifyOrExit(cJSON_IsString(type));
        typestr = type->valuestring;

        if (strcmp(typestr, "extended") == 0)
        {
            VerifyOrExit(addressLength == 16);
            aType = kAddressTypeExt;
        }
        else if (strcmp(typestr, "mleid") == 0)
        {
            VerifyOrExit(addressLength == 16);
            aType = kAddressTypeMleid;
        }
        else if (strcmp(typestr, "rloc") == 0)
        {
            VerifyOrExit(addressLength == 6);
            aType = kAddressTypeRloc;
        }
        else
        {
            ExitNow();
        }
    }
    else
    {
        if (addressLength == 16)
        {
            aType = kAddressTypeExt;
        }
        else if (addressLength == 6)
        {
            aType = kAddressTypeRloc;
        }
        else
        {
            ExitNow();
        }
    }

    str = address->valuestring;

exit:
    return str;
}

void BasicActions::JsonifyTimeout(cJSON &aAttributes) const
{
    Timepoint now       = Clock::now();
    Timepoint timeout   = mCreatedSteady + mTimeout;
    uint32_t  remaining = 0;

    if (now < timeout)
    {
        remaining = std::chrono::duration_cast<Seconds>(timeout - now).count();
    }

    if (!cJSON_ReplaceItemInObjectCaseSensitive(&aAttributes, KEY_TIMEOUT, cJSON_CreateNumber(remaining)))
    {
        cJSON_AddItemToObject(&aAttributes, KEY_TIMEOUT, cJSON_CreateNumber(remaining));
    }
}

void BasicActions::SetResult(const std::string &aType, const std::string &aUuid)
{
    /* Example:
    "relationships": {
        "result": {
          "data": {
            "type": "diagnostics",
            "id": "0a97ef16-1997-43dd-91c4-7fbcb1ec6713"
          }
        }
    */
    cJSON *result = cJSON_CreateObject();
    cJSON *data   = cJSON_CreateObject();

    cJSON_AddStringToObject(data, "type", aType.c_str());
    cJSON_AddStringToObject(data, "id", aUuid.c_str());

    cJSON_AddItemToObject(result, "data", data);

    if (mRelationships != nullptr)
    {
        cJSON_Delete(mRelationships);
    }
    mRelationships = cJSON_CreateObject();
    cJSON_AddItemToObject(mRelationships, "result", result);
}

const char *BasicActions::StatusToString(ActionStatus aStatus)
{
    constexpr std::array<const char *, 5> kStatusStrings = {
        "pending",   // (0) kActionStatusPending
        "active",    // (1) kActionStatusActive
        "completed", // (2) kActionStatusCompleted
        "stopped",   // (3) kActionStatusStopped
        "failed",    // (4) kActionStatusFailed
    };

    static_assert(kActionStatusPending == 0, "kActionStatusPending value is incorrect");
    static_assert(kActionStatusActive == 1, "kActionStatusActive value is incorrect");
    static_assert(kActionStatusCompleted == 2, "kActionStatusCompleted value is incorrect");
    static_assert(kActionStatusStopped == 3, "kActionStatusStopped value is incorrect");
    static_assert(kActionStatusFailed == 4, "kActionStatusFailed value is incorrect");

    return kStatusStrings[aStatus];
}

} // namespace actions
} // namespace rest
} // namespace otbr
