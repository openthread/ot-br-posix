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

/**
 * @brief   Implements api/diagnostics collection and conversion of items and collection to json and json:api
 */

#ifndef OTBR_REST_DIAGNOSTICS_COLLLECTION_HPP_
#define OTBR_REST_DIAGNOSTICS_COLLLECTION_HPP_

#include "uuid.hpp"
#include <chrono>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include "rest/types.hpp"

#include "rest_generic_collection.hpp"

struct cJSON;

namespace otbr {
namespace rest {

#define MAX_DIAG_COLLECTION_ITEMS 200
#define DIAG_COLLECTION_NAME "diagnostics"
#define NWK_DIAG_TYPE_NAME "networkDiagnostics"
#define ENERGYSCAN_TYPE_NAME "energyScanReport"

/**
 * @brief This virtual class implements a general json:api item for holding diagnostic attributes.
 */
class BasicDiagnostics : public BasicCollectionItem
{
public:
    /**
     * Constructor for BasicDiagnostics.
     */
    BasicDiagnostics();

    /**
     * Destructor for BasicDiagnostics.
     */
    virtual ~BasicDiagnostics();

    /**
     * Convert the item to a JSON:API item string.
     *
     * @param[in] aKeys  The set of keys to include in the JSON:API item string.
     *
     * @returns The JSON:API item string representation of the item, including the created timestamp.
     */
    std::string ToJsonApiItem(std::set<std::string> aKeys) override;
};

/**
 * @brief This class implements a json:api item for holding network diagnostic attributes.
 */
class NetworkDiagnostics : public BasicDiagnostics
{
public:
    /**
     * Constructor for NetworkDiagnostics Item.
     */
    NetworkDiagnostics();

    /**
     * Destructor for NetworkDiagnostics Item.
     */
    ~NetworkDiagnostics();

    std::vector<otNetworkDiagTlv>              mDeviceTlvSet;
    std::vector<networkDiagTlvExtensions>      mDeviceTlvSetExtension;
    std::vector<otMeshDiagChildEntry>          mChildren;
    std::vector<DeviceIp6Addrs>                mChildrenIp6Addrs;
    std::vector<otMeshDiagRouterNeighborEntry> mNeighbors;

    /**
     * Get the type name of the item.
     *
     * @returns The type name.
     */
    std::string GetTypeName() const override;

    /**
     * Convert the item to a JSON string.
     *
     * @param[in] aKeys  The set of keys to include in the JSON string.
     *
     * @returns The JSON string representation of the item.
     */
    std::string ToJsonString(std::set<std::string> aKeys) override;
};

/**
 * @brief This class implements a json:api item for holding energy scan diagnostic attributes.
 */
class EnergyScanDiagnostics : public BasicDiagnostics
{
public:
    /**
     * Constructor for EnergyScanDiagnostics Item.
     */
    EnergyScanDiagnostics();

    /**
     * Destructor for EnergyScanDiagnostics Item.
     */
    ~EnergyScanDiagnostics();

    otbr::rest::EnergyScanReport mReport;

    /**
     * Get the type name of the item.
     *
     * @returns The type name.
     */
    std::string GetTypeName() const override;

    /**
     * Convert the item to a JSON string.
     *
     * @param[in] aKeys  The set of keys to include in the JSON string.
     *
     * @returns The JSON string representation of the item.
     */
    std::string ToJsonString(std::set<std::string> aKeys) override;
};

/**
 * @brief This class implements a json:api collection for holding diagnostic items.
 */
class DiagnosticsCollection : public BasicCollection
{
    std::map<std::string, uint16_t> mHoldsTypes;

public:
    /**
     * Constructor for DiagnosticsCollection.
     */
    DiagnosticsCollection();

    /**
     * Destructor for DiagnosticsCollection.
     */
    ~DiagnosticsCollection();

    /**
     * Get the collection name.
     *
     * @returns The collection name.
     */
    std::string GetCollectionName() const override { return std::string(DIAG_COLLECTION_NAME); }

    /**
     * Get the maximum allowed size the collection can grow to.
     *
     * Once the max size is reached, the oldest item
     * is going to be evicted for each new item added.
     *
     * @returns The maximum collection size.
     */
    uint16_t GetMaxCollectionSize() const override { return MAX_DIAG_COLLECTION_ITEMS; }

    /**
     * Get the meta data of the collection.
     *
     * @returns The meta data objects of the collection.
     */
    cJSON *GetCollectionMeta() override;

    /**
     * Add a item to the collection.
     *
     * @param[in] aItem  The item to add.
     */
    void AddItem(std::unique_ptr<BasicDiagnostics> aItem);

    /**
     * Get the item with the given id.
     *
     * @param[in] aItemId  The id of the item to get.
     *
     * @returns The item with the given id.
     */
    BasicDiagnostics *GetItem(std::string aItemId);
};

} // namespace rest
} // namespace otbr

#endif // OTBR_REST_DIAGNOSTICS_COLLLECTION_HPP_
