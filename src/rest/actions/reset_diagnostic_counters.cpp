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

#include "reset_diagnostic_counters.hpp"

#include "common/code_utils.hpp"
#include "rest/json.hpp"
#include "rest/services.hpp"

#include <openthread/netdiag.h>
#include <openthread/thread.h>

#include <set>

namespace otbr {
namespace rest {
namespace actions {

constexpr const char *ResetDiagnosticCounters::kJsonType;

ResetDiagnosticCounters::ResetDiagnosticCounters(const cJSON &aJson, Services &aServices)
    : BasicActions(aJson, aServices)
    , mDestination{nullptr}
{
    cJSON            *item;
    uint8_t           id;
    std::set<uint8_t> tlvs;

    cJSON *destination = cJSON_GetObjectItemCaseSensitive(&aJson, KEY_DESTINATION);
    cJSON *types       = cJSON_GetObjectItemCaseSensitive(mJson, KEY_TYPES);

    // Were guaranteed that Validate has run already
    if (destination != nullptr)
    {
        mDestination = ReadDestination(*mJson, mDestinationType);
    }

    cJSON_ArrayForEach(item, types)
    {
        IgnoreError(DiagnosticTypes::FindId(item->valuestring, id));
        tlvs.insert(id);
    }

    assert(tlvs.size() <= DiagnosticTypes::kMaxResettableCount);
    uint8_t i = 0;
    for (auto it : tlvs)
    {
        mTypeList[i++] = it;
    }
    mTypeCount = i;
}

std::string ResetDiagnosticCounters::GetTypeName() const
{
    return std::string(RESET_DIAG_COUNTERS_ACTION_TYPE_NAME);
}

void ResetDiagnosticCounters::Update(void)
{
    if (IsPendingOrActive())
    {
        otInstance  *instance = mServices.GetInstance();
        otIp6Address destination;

        if (mDestination != nullptr)
        {
            SuccessOrExit(mServices.LookupAddress(mDestination, mDestinationType, destination));
        }
        else
        {
            destination = *otThreadGetRealmLocalAllThreadNodesMulticastAddress(instance);
        }

        if (otThreadSendDiagnosticReset(instance, &destination, mTypeList, mTypeCount) == OT_ERROR_NONE)
        {
            mStatus = kActionStatusCompleted;
        }
        else
        {
            mStatus = kActionStatusFailed;
        }
    }

exit:
    return;
}

void ResetDiagnosticCounters::Stop(void)
{
    if (IsPendingOrActive())
    {
        mStatus = kActionStatusStopped;
    }
}

cJSON *ResetDiagnosticCounters::Jsonify(std::set<std::string> aFieldset)
{
    cJSON *attributes = cJSON_CreateObject();
    cJSON *types      = cJSON_CreateArray();

    if (mDestination != nullptr)
    {
        if (hasKey(aFieldset, KEY_DESTINATION))
        {
            cJSON_AddItemToObject(attributes, KEY_DESTINATION, cJSON_CreateString(mDestination));
        }
        if (hasKey(aFieldset, KEY_DESTINATION_TYPE))
        {
            cJSON_AddItemToObject(attributes, KEY_DESTINATION_TYPE,
                                  cJSON_CreateString(AddressTypeToString(mDestinationType)));
        }
    }

    if (hasKey(aFieldset, KEY_TYPES))
    {
        for (uint32_t i = 0; i < mTypeCount; i++)
        {
            cJSON_AddItemToArray(types, cJSON_CreateString(DiagnosticTypes::GetJsonKey(mTypeList[i])));
        }
        cJSON_AddItemToObject(attributes, KEY_TYPES, types);
    }

    if (IsPendingOrActive())
    {
        if (hasKey(aFieldset, KEY_TIMEOUT))
        {
            JsonifyTimeout(*attributes);
        }
    }

    if (hasKey(aFieldset, KEY_STATUS))
    {
        cJSON_AddItemToObject(attributes, KEY_STATUS, cJSON_CreateString(GetStatusString()));
    }

    return attributes;
}

bool ResetDiagnosticCounters::Validate(const cJSON &aJson)
{
    bool        ok = false;
    AddressType type;
    Seconds     timeout;
    cJSON      *item;
    uint8_t     id;

    cJSON *destination = cJSON_GetObjectItemCaseSensitive(&aJson, KEY_DESTINATION);
    cJSON *types       = cJSON_GetObjectItemCaseSensitive(&aJson, KEY_TYPES);

    SuccessOrExit(ReadTimeout(aJson, timeout));

    if (destination != nullptr)
    {
        VerifyOrExit(ReadDestination(aJson, type) != nullptr);
    }

    VerifyOrExit(types != nullptr);
    VerifyOrExit(cJSON_IsArray(types));
    cJSON_ArrayForEach(item, types)
    {
        VerifyOrExit(cJSON_IsString(item));
        SuccessOrExit(DiagnosticTypes::FindId(item->valuestring, id));
        VerifyOrExit(DiagnosticTypes::CanReset(id));
    }

    ok = true;

exit:
    return ok;
}

} // namespace actions
} // namespace rest
} // namespace otbr
