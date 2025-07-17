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

#include "discover_network.hpp"

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

DiscoverNetwork::DiscoverNetwork(const cJSON &aJson, Services &aServices)
    : BasicActions(aJson, aServices)
{
    cJSON *maxAge      = cJSON_GetObjectItemCaseSensitive(&aJson, KEY_MAX_AGE);
    cJSON *maxRetries  = cJSON_GetObjectItemCaseSensitive(&aJson, KEY_MAX_RETRIES);
    cJSON *deviceCount = cJSON_GetObjectItemCaseSensitive(&aJson, KEY_DEVICE_COUNT);

    // We guaranteed that Validate has run already
    mMaxAge      = static_cast<uint32_t>(maxAge->valuedouble * 1000); // Convert seconds to milliseconds
    mMaxRetries  = static_cast<uint8_t>(maxRetries->valueint);
    mDeviceCount = static_cast<uint32_t>(deviceCount->valueint);
}

DiscoverNetwork::~DiscoverNetwork(void)
{
    // Make sure we always deregister properly
    Stop();
}

std::string DiscoverNetwork::GetTypeName() const
{
    return std::string(DISCOVER_NETWORK_ACTION_TYPE_NAME);
}

void DiscoverNetwork::Update(void)
{
    if (mStatus == kActionStatusPending)
    {
        if (mServices.GetNetworkDiagHandler().HandleNetworkDiscoveryRequest(
                static_cast<uint32_t>(GetTimeout().count() * 1000), mMaxAge, mMaxRetries) == OT_ERROR_NONE)
        {
            mStatus = kActionStatusActive;
        }
    }
    else if (mStatus == kActionStatusActive) // continue on next call
    {
        switch (mServices.GetNetworkDiagHandler().GetDiscoveryStatus(mActualDeviceCount))
        {
        case OT_ERROR_NONE:
            // results are already set in the NetworkDiagHandler
            if ((mActualDeviceCount < mDeviceCount) && (mRetries++ <= mMaxRetries))
            {
                // If we have not discovered enough devices in device collection, retry
                mStatus = kActionStatusPending;
            }
            else
            {
                // We have discovered enough devices or reached the maximum number of retries
                mStatus = kActionStatusCompleted;
            }
            break;

        case OT_ERROR_PENDING:
            break;

        default:
            otbrLogWarning("%s:%d - %s - Error while processing discovery request:", __FILE__, __LINE__, __func__);
            mStatus = kActionStatusFailed;
            break;
        }

        if (mStatus != kActionStatusActive)
        {
            mServices.GetNetworkDiagHandler().StopDiagnosticsRequest();
        }
    }
}

void DiscoverNetwork::Stop(void)
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

cJSON *DiscoverNetwork::Jsonify(std::set<std::string> aFieldset)
{
    cJSON *attributes = cJSON_CreateObject();

    if (hasKey(aFieldset, KEY_MAX_AGE))
    {
        cJSON_AddItemToObject(attributes, KEY_MAX_AGE, cJSON_CreateNumber(static_cast<double>(mMaxAge) / 1000.0));
    }
    if (hasKey(aFieldset, KEY_MAX_RETRIES))
    {
        cJSON_AddItemToObject(attributes, KEY_MAX_RETRIES, cJSON_CreateNumber(mMaxRetries));
    }
    if (hasKey(aFieldset, KEY_DEVICE_COUNT))
    {
        cJSON_AddItemToObject(attributes, KEY_DEVICE_COUNT, cJSON_CreateNumber(mDeviceCount));
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

bool DiscoverNetwork::Validate(const cJSON &aJson)
{
    bool        ok       = false;
    std::string errormsg = "ok";
    Seconds     timeout;

    // example JSON:API document
    // {
    //    "maxAge": 1.500,
    //    "maxRetries": 3,
    //    "deviceCount": 5,
    //    "timeout": 5
    // }

    cJSON *maxAge      = cJSON_GetObjectItemCaseSensitive(&aJson, KEY_MAX_AGE);
    cJSON *maxRetries  = cJSON_GetObjectItemCaseSensitive(&aJson, KEY_MAX_RETRIES);
    cJSON *deviceCount = cJSON_GetObjectItemCaseSensitive(&aJson, KEY_DEVICE_COUNT);

    errormsg = KEY_MAX_AGE;
    VerifyOrExit(maxAge != nullptr && cJSON_IsNumber(maxAge));

    errormsg = KEY_MAX_RETRIES;
    VerifyOrExit(maxRetries != nullptr && cJSON_IsNumber(maxRetries));

    errormsg = KEY_DEVICE_COUNT;
    VerifyOrExit(deviceCount != nullptr && cJSON_IsNumber(deviceCount));

    errormsg = KEY_TIMEOUT;
    SuccessOrExit(ReadTimeout(aJson, timeout));

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
