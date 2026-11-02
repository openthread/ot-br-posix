/*
 *  Copyright (c) 2026, The OpenThread Authors.
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

#include "traffic_stats.hpp"

#include <utility>

#include <cJSON.h>

#include "rest/rest_traffic_stats_coll.hpp"

namespace otbr {
namespace rest {
namespace actions {

TrafficStats::TrafficStats(const cJSON &aJson, Services &aServices)
    : BasicActions(aJson, aServices)
{
}

std::string TrafficStats::GetTypeName() const
{
    return TRAFFIC_STATS_ACTION_TYPE_NAME;
}

void TrafficStats::Update(void)
{
    if (mStatus != kActionStatusPending)
    {
        return;
    }

    const otSysTrafficStats *trafficStats = otSysGetTrafficStats();
    const otSysPerDestStats *perDestStats = otSysGetPerDestStats();

    if (trafficStats == nullptr || perDestStats == nullptr)
    {
        mStatus = kActionStatusFailed;
        return;
    }

    // Both counters are copied on the main-loop thread before any GET serializes them.
    std::unique_ptr<TrafficStatsSnapshot> result(new TrafficStatsSnapshot(*trafficStats, *perDestStats));
    const std::string                     resultId   = result->mUuid.ToString();
    TrafficStatsCollection               &collection = mServices.GetTrafficStatsCollection();

    if (!collection.AddItem(std::move(result)))
    {
        mStatus = kActionStatusFailed;
        return;
    }

    SetResult(collection.GetCollectionName(), resultId);
    mStatus = kActionStatusCompleted;
}

void TrafficStats::Stop(void)
{
    if (IsPendingOrActive())
    {
        mStatus = kActionStatusStopped;
    }
}

cJSON *TrafficStats::Jsonify(std::set<std::string> aFieldset)
{
    cJSON *attributes = cJSON_CreateObject();

    if (hasKey(aFieldset, KEY_STATUS))
    {
        cJSON_AddStringToObject(attributes, KEY_STATUS, GetStatusString());
    }

    return attributes;
}

bool TrafficStats::Validate(const cJSON &aJson)
{
    return cJSON_IsObject(&aJson) && aJson.child == nullptr;
}

} // namespace actions
} // namespace rest
} // namespace otbr
