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

#include "network_diagnostic.hpp"

#include <set>
#include <unordered_map>

#include "common/code_utils.hpp"

#include "rest/json.hpp"
#include "rest/network_diag_handler.hpp"
#include "rest/rest_diagnostics_coll.hpp"
#include "rest/rest_server_common.hpp"
#include "rest/services.hpp"

#include <cJSON.h>

namespace otbr {
namespace rest {
namespace actions {

NetworkDiagnostic::NetworkDiagnostic(const cJSON &aJson, Services &aServices)
    : BasicActions(aJson, aServices)
{
    cJSON            *item;
    uint8_t           id;
    std::set<uint8_t> tlvs;

    cJSON *types = cJSON_GetObjectItemCaseSensitive(mJson, KEY_TYPES);

    // Were guaranteed that Validate has run already
    mDestination = ReadDestination(*mJson, mDestinationType);

    cJSON_ArrayForEach(item, types)
    {
        IgnoreError(DiagnosticTypes::FindId(item->valuestring, id));
        tlvs.insert(id);
    }

    assert(tlvs.size() <= DiagnosticTypes::kMaxTotalCount);
    uint8_t i = 0;
    for (auto it : tlvs)
    {
        mTypeList[i++] = it;
    }
    mTypeCount = i;
}

NetworkDiagnostic::~NetworkDiagnostic(void)
{
    // Make sure we always deregister properly
    Stop();
}

std::string NetworkDiagnostic::GetTypeName() const
{
    return std::string(NETWORK_DIAG_ACTION_TYPE_NAME);
}

void NetworkDiagnostic::Update(void)
{
    std::string results(UUID_STR_LEN, '\0'); // An empty Uuid string for obtaining the created actionId

    if (mStatus == kActionStatusPending)
    {
        otIp6Address address;
        SuccessOrExit(mServices.LookupAddress(mDestination, mDestinationType, address));

        if (mServices.GetNetworkDiagHandler().StartDiagnosticsRequest(address, mTypeList, mTypeCount, mTimeout) ==
            OT_ERROR_NONE)
        {
            mStatus = kActionStatusActive;
        }
    }

    if (mStatus == kActionStatusActive)
    {
        switch (mServices.GetNetworkDiagHandler().GetDiagnosticsStatus(
            mDestination, mDestinationType,
            results)) // TODO: add parameter for diagnostic types (mTypeList, mTypeCount)
        {
        case OT_ERROR_NONE:
            SetResult(mServices.GetDiagnosticsCollection().GetCollectionName(), results);
            mStatus = kActionStatusCompleted;
            break;

        case OT_ERROR_PENDING:
            break;

        default:
            mStatus = kActionStatusFailed;
            break;
        }

        if (mStatus != kActionStatusActive)
        {
            mServices.GetNetworkDiagHandler().StopDiagnosticsRequest();
        }
    }

exit:
    return;
}

void NetworkDiagnostic::Stop(void)
{
    switch (mStatus)
    {
    case kActionStatusPending:
        mStatus = kActionStatusStopped;
        break;

    case kActionStatusActive:
        mServices.GetNetworkDiagHandler().StopDiagnosticsRequest();
        mStatus = kActionStatusStopped;
        break;

    default:
        break;
    }
}

cJSON *NetworkDiagnostic::Jsonify(std::set<std::string> aFieldset)
{
    cJSON *attributes = cJSON_CreateObject();
    cJSON *types      = cJSON_CreateArray();

    if (hasKey(aFieldset, KEY_DESTINATION))
    {
        cJSON_AddItemToObject(attributes, KEY_DESTINATION, cJSON_CreateString(mDestination));
    }
    if (hasKey(aFieldset, KEY_DESTINATION_TYPE))
    {
        cJSON_AddItemToObject(attributes, KEY_DESTINATION_TYPE,
                              cJSON_CreateString(AddressTypeToString(mDestinationType)));
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

bool NetworkDiagnostic::Validate(const cJSON &aJson)
{
    bool        ok       = false;
    std::string errormsg = "ok";
    AddressType type;
    Seconds     timeout;
    cJSON      *item;
    uint8_t     id;

    cJSON *types = cJSON_GetObjectItemCaseSensitive(&aJson, KEY_TYPES);

    errormsg = KEY_TIMEOUT;
    SuccessOrExit(ReadTimeout(aJson, timeout));

    errormsg = KEY_DESTINATION;
    VerifyOrExit(ReadDestination(aJson, type) != nullptr);

    errormsg = KEY_TYPES;
    VerifyOrExit(types != nullptr);
    VerifyOrExit(cJSON_IsArray(types));
    cJSON_ArrayForEach(item, types)
    {
        VerifyOrExit(cJSON_IsString(item));
        SuccessOrExit(DiagnosticTypes::FindId(item->valuestring, id));
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
