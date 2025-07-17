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

#include "energy_scan.hpp"

#include "common/code_utils.hpp"

#include "rest/commissioner_manager.hpp"
#include "rest/json.hpp"
#include "rest/rest_diagnostics_coll.hpp"
#include "rest/rest_server_common.hpp"
#include "rest/services.hpp"

#include <cJSON.h>

namespace otbr {
namespace rest {
namespace actions {

// EnergyScanAction class implementation
EnergyScan::EnergyScan(const cJSON &aJson, Services &aServices)
    : BasicActions(aJson, aServices)
{
    cJSON *item;

    cJSON *mask         = cJSON_GetObjectItemCaseSensitive(mJson, "channelMask");
    cJSON *count        = cJSON_GetObjectItemCaseSensitive(mJson, "count");
    cJSON *period       = cJSON_GetObjectItemCaseSensitive(mJson, "period");
    cJSON *scanDuration = cJSON_GetObjectItemCaseSensitive(mJson, "scanDuration");

    // Were guaranteed that Validate has run already
    mDestination = ReadDestination(*mJson, mDestinationType);

    mMask = 0;
    cJSON_ArrayForEach(item, mask)
    {
        mMask |= 1 << item->valueint;
    }

    mCount        = count->valueint;
    mPeriod       = period->valueint;
    mScanDuration = scanDuration->valueint;
}

EnergyScan::~EnergyScan(void)
{
    // Make sure we always deregister properly
    Stop();
}

std::string EnergyScan::GetTypeName() const
{
    return std::string(ENERGY_SCAN_ACTION_TYPE_NAME);
}

void EnergyScan::Update(void)
{
    otError error;

    if (mStatus == kActionStatusPending)
    {
        otIp6Address address;
        SuccessOrExit(mServices.LookupAddress(mDestination, mDestinationType, address));

        error = mServices.GetCommissionerManager().StartEnergyScan(mMask, mCount, mPeriod, mScanDuration, &address);

        if (error == OT_ERROR_NONE)
        {
            mStatus = kActionStatusActive;
        }
        else
        {
            otbrLogWarning("Failed to activate %s", otThreadErrorToString(error));
        }
    }

    if (mStatus == kActionStatusActive)
    {
        error = mServices.GetCommissionerManager().GetEnergyScanStatus();
        switch (error)
        {
        case OT_ERROR_NONE:
        {
            std::unique_ptr<otbr::rest::EnergyScanDiagnostics> res =
                std::unique_ptr<otbr::rest::EnergyScanDiagnostics>(new otbr::rest::EnergyScanDiagnostics());
            res->mReport = mServices.GetCommissionerManager().GetEnergyScanResult();

            SetResult(mServices.GetDiagnosticsCollection().GetCollectionName(), res->mUuid.ToString());

            mServices.GetDiagnosticsCollection().AddItem(std::move(res));
            mStatus = kActionStatusCompleted;
        }
        break;

        case OT_ERROR_PENDING:
            break;

        default:
            mStatus = kActionStatusFailed;
            break;
        }

        // Make sure we always deregister properly
        if (mStatus != kActionStatusActive)
        {
            mServices.GetCommissionerManager().StopEnergyScan();
        }
    }

exit:
    return;
}

void EnergyScan::Stop(void)
{
    switch (mStatus)
    {
    case kActionStatusPending:
        mStatus = kActionStatusStopped;
        break;

    case kActionStatusActive:
        mServices.GetCommissionerManager().StopEnergyScan();
        mStatus = kActionStatusStopped;
        break;

    default:
        break;
    }
}

cJSON *EnergyScan::Jsonify(std::set<std::string> aFieldset)
{
    cJSON *attributes  = cJSON_CreateObject();
    cJSON *channelMask = cJSON_CreateArray();

    if (hasKey(aFieldset, KEY_DESTINATION))
    {
        cJSON_AddItemToObject(attributes, KEY_DESTINATION, cJSON_CreateString(mDestination));
    }

    if (mDestinationType != kAddressTypeExt)
    {
        if (hasKey(aFieldset, KEY_DESTINATION_TYPE))
        {
            cJSON_AddItemToObject(attributes, KEY_DESTINATION_TYPE,
                                  cJSON_CreateString(AddressTypeToString(mDestinationType)));
        }
    }

    if (hasKey(aFieldset, KEY_CHANNELMASK))
    {
        for (uint32_t i = 0; i < 32; i++)
        {
            if ((mMask & (1 << i)) != 0)
            {
                cJSON_AddItemToArray(channelMask, cJSON_CreateNumber(i));
            }
        }
        cJSON_AddItemToObject(attributes, KEY_CHANNELMASK, channelMask);
    }

    if (hasKey(aFieldset, KEY_COUNT))
    {
        cJSON_AddItemToObject(attributes, KEY_COUNT, cJSON_CreateNumber(mCount));
    }
    if (hasKey(aFieldset, KEY_PERIOD))
    {
        cJSON_AddItemToObject(attributes, KEY_PERIOD, cJSON_CreateNumber(mPeriod));
    }
    if (hasKey(aFieldset, KEY_SCANDURATION))
    {
        cJSON_AddItemToObject(attributes, KEY_SCANDURATION, cJSON_CreateNumber(mScanDuration));
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

bool EnergyScan::Validate(const cJSON &aJson)
{
    bool        ok       = false;
    std::string errormsg = "ok";
    AddressType type;
    Seconds     timeout;
    cJSON      *item;

    cJSON *mask         = cJSON_GetObjectItemCaseSensitive(&aJson, KEY_CHANNELMASK);
    cJSON *count        = cJSON_GetObjectItemCaseSensitive(&aJson, KEY_COUNT);
    cJSON *period       = cJSON_GetObjectItemCaseSensitive(&aJson, KEY_PERIOD);
    cJSON *scanDuration = cJSON_GetObjectItemCaseSensitive(&aJson, KEY_SCANDURATION);

    errormsg = KEY_TIMEOUT;
    SuccessOrExit(ReadTimeout(aJson, timeout));

    errormsg = KEY_DESTINATION;
    VerifyOrExit(ReadDestination(aJson, type) != nullptr);

    errormsg = KEY_CHANNELMASK;
    VerifyOrExit(mask != nullptr);
    VerifyOrExit(cJSON_IsArray(mask));
    cJSON_ArrayForEach(item, mask)
    {
        VerifyOrExit(cJSON_IsNumber(item));
        VerifyOrExit(item->valueint >= 11 && item->valueint <= 26);
    }

    errormsg = KEY_COUNT;
    VerifyOrExit(count != nullptr);
    VerifyOrExit(cJSON_IsNumber(count));

    errormsg = KEY_PERIOD;
    VerifyOrExit(period != nullptr);
    VerifyOrExit(cJSON_IsNumber(period));

    errormsg = KEY_SCANDURATION;
    VerifyOrExit(scanDuration != nullptr);
    VerifyOrExit(cJSON_IsNumber(scanDuration));

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
