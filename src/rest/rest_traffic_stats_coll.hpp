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

/**
 * @file
 *   Traffic snapshot storage and collection for the REST API.
 */

#ifndef OTBR_REST_TRAFFIC_STATS_COLLECTION_HPP_
#define OTBR_REST_TRAFFIC_STATS_COLLECTION_HPP_

#include "rest_generic_collection.hpp"

#include <openthread/openthread-system.h>

namespace otbr {
namespace rest {

#define MAX_TRAFFIC_STATS_COLLECTION_ITEMS 10
#define TRAFFIC_STATS_COLLECTION_NAME "traffic-stats"
#define TRAFFIC_STATS_TYPE_NAME "trafficStats"

/**
 * Holds an immutable copy of the local traffic counters.
 */
class TrafficStatsSnapshot : public BasicCollectionItem
{
public:
    /**
     * Copies the global and per-flow counters at construction time.
     *
     * @param[in] aTrafficStats  The global traffic counters.
     * @param[in] aPerDestStats  The per-source/destination traffic counters.
     */
    TrafficStatsSnapshot(const otSysTrafficStats &aTrafficStats, const otSysPerDestStats &aPerDestStats);

    std::string GetTypeName() const override;
    std::string ToJsonString(std::set<std::string> aKeys) override;
    std::string ToJsonApiItem(std::set<std::string> aKeys) override;

private:
    const otSysTrafficStats mTrafficStats;
    const otSysPerDestStats mPerDestStats;
};

/**
 * Maintains a bounded collection of traffic snapshots.
 */
class TrafficStatsCollection : public BasicCollection
{
public:
    TrafficStatsCollection();

    std::string GetCollectionName() const override { return TRAFFIC_STATS_COLLECTION_NAME; }
    uint16_t    GetMaxCollectionSize() const override { return MAX_TRAFFIC_STATS_COLLECTION_ITEMS; }
    cJSON      *GetCollectionMeta() override;

    /**
     * Takes ownership of a snapshot, evicting the oldest one when full.
     *
     * @param[in] aItem  The snapshot to store.
     * @returns Whether the snapshot was stored successfully.
     */
    bool AddItem(std::unique_ptr<TrafficStatsSnapshot> aItem);

    /** Returns the snapshot with the given ID, or nullptr if absent. */
    TrafficStatsSnapshot *GetItem(std::string aItemId);
};

} // namespace rest
} // namespace otbr

#endif // OTBR_REST_TRAFFIC_STATS_COLLECTION_HPP_
