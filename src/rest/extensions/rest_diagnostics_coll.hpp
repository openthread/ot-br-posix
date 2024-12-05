/*
 *  Copyright (c) 2024, The OpenThread Authors.
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
 *
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

namespace otbr {
namespace rest {

#define MAX_DIAG_COLLECTION_ITEMS 200
#define DIAG_COLLECTION_NAME "diagnostics"
#define NWK_DIAG_TYPE_NAME "networkDiagnostics"
#define ENERGYSCAN_TYPE_NAME "energyScanReport"

/**
 * @brief This virtual class implements a general json:api item for holding diagnostic attributes.
 *
 */
class BasicDiagnostics : public BasicCollectionItem
{
public:
    //
    BasicDiagnostics();
    virtual BasicDiagnostics *clone() = 0;
    virtual ~BasicDiagnostics();
    std::string toJsonApiItem(std::set<std::string> aKeys) override;
};

/**
 * @brief This class implements a json:api item for holding network diagnostic attributes.
 *
 */
class NetworkDiagnostics : public BasicDiagnostics
{
public:
    NetworkDiagnostics();
    ~NetworkDiagnostics();

    std::vector<otNetworkDiagTlv>              mDeviceTlvSet;
    std::vector<networkDiagTlvExtensions>      mDeviceTlvSetExtension;
    std::vector<otMeshDiagChildEntry>          mChildren;
    std::vector<DeviceIp6Addrs>                mChildrenIp6Addrs;
    std::vector<otMeshDiagRouterNeighborEntry> mNeighbors;

    std::string         getTypeName() const override;
    std::string         toJsonString(std::set<std::string> aKeys);
    NetworkDiagnostics *clone() override;
};

/**
 * @brief This class implements a json:api item for holding energy scan diagnostic attributes.
 *
 */

class EnergyScanDiagnostics : public BasicDiagnostics
{
public:
    EnergyScanDiagnostics();
    ~EnergyScanDiagnostics();
    otbr::rest::EnergyScanReport mReport;
    std::string                  getTypeName() const override;
    std::string                  toJsonString(std::set<std::string> aKeys);
    EnergyScanDiagnostics       *clone() override;
};

/**
 * @brief This class implements a json:api collection for holding diagnostic items.
 *
 */
class DiagnosticsCollection : public BasicCollection
{
    std::map<std::string, uint16_t> mHoldsTypes;

public:
    DiagnosticsCollection();
    ~DiagnosticsCollection();
    std::string       getCollectionName() const override { return std::string(DIAG_COLLECTION_NAME); }
    uint16_t          getMaxCollectionSize() const override { return MAX_DIAG_COLLECTION_ITEMS; }
    void              addItem(BasicDiagnostics *aItem);
    BasicDiagnostics *getItem(std::string key);
};

// the collection
extern DiagnosticsCollection gDiagnosticsCollection;

} // namespace rest
} // namespace otbr

#endif // OTBR_REST_DIAGNOSTICS_COLLLECTION_HPP_
