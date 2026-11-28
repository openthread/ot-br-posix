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

#include "rest_traffic_stats_coll.hpp"

#include <utility>

#include "rest/json.hpp"

namespace otbr {
namespace rest {

TrafficStatsSnapshot::TrafficStatsSnapshot(const otSysTrafficStats &aTrafficStats,
                                           const otSysPerDestStats &aPerDestStats)
    : mTrafficStats(aTrafficStats)
    , mPerDestStats(aPerDestStats)
{
}

std::string TrafficStatsSnapshot::GetTypeName() const
{
    return TRAFFIC_STATS_TYPE_NAME;
}

std::string TrafficStatsSnapshot::ToJsonString(std::set<std::string> aKeys)
{
    return Json::TrafficStats2JsonString(mTrafficStats, mPerDestStats, aKeys);
}

std::string TrafficStatsSnapshot::ToJsonApiItem(std::set<std::string> aKeys)
{
    return Json::jsonStr2JsonApiItem(mUuid.ToString(), GetTypeName(), ToJsonStringTs(aKeys));
}

TrafficStatsCollection::TrafficStatsCollection()
{
    mCollection.reserve(GetMaxCollectionSize());
}

cJSON *TrafficStatsCollection::GetCollectionMeta()
{
    return Json::CreateMetaCollection(0, GetMaxCollectionSize(), mCollection.size());
}

bool TrafficStatsCollection::AddItem(std::unique_ptr<TrafficStatsSnapshot> aItem)
{
    if (!aItem)
    {
        return false;
    }

    const std::string id   = aItem->mUuid.ToString();
    const std::string type = aItem->GetTypeName();

    if (id.empty() || mCollection.find(id) != mCollection.end())
    {
        return false;
    }

    while (mCollection.size() >= GetMaxCollectionSize())
    {
        EvictOldestItem();
    }

    auto result = mCollection.emplace(id, std::move(aItem));
    if (!result.second)
    {
        return false;
    }

    IncrHoldsTypes(type);
    mAgeSortedItemIds.push_back(id);
    return true;
}

TrafficStatsSnapshot *TrafficStatsCollection::GetItem(std::string aItemId)
{
    auto it = mCollection.find(aItemId);
    return it == mCollection.end() ? nullptr : static_cast<TrafficStatsSnapshot *>(it->second.get());
}

} // namespace rest
} // namespace otbr
