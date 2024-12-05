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
#include "rest_diagnostics_coll.hpp"
#include "timestamp.hpp"
#include "common/code_utils.hpp"
#include "rest/json.hpp"

extern "C" {
#include <cJSON.h>
}

namespace otbr {
namespace rest {

// BasicDiagnostics class implementation
BasicDiagnostics::BasicDiagnostics()
    : BasicCollectionItem()
{
}

BasicDiagnostics::~BasicDiagnostics()
{
}

std::string BasicDiagnostics::toJsonApiItem(std::set<std::string> aKeys)
{
    return Json::jsonStr2JsonApiItem(mUuid.toString(), getTypeName(), toJsonStringTs(aKeys));
}

// NetworkDiagnostics class implementation
NetworkDiagnostics::NetworkDiagnostics()
    : BasicDiagnostics()
{
}

NetworkDiagnostics::~NetworkDiagnostics()
{
}

std::string NetworkDiagnostics::getTypeName() const
{
    return std::string(NWK_DIAG_TYPE_NAME);
}

std::string NetworkDiagnostics::toJsonString(std::set<std::string> aKeys)
{
    return Json::DiagSet2JsonString(mDeviceTlvSet, mChildren, mChildrenIp6Addrs, mNeighbors, mDeviceTlvSetExtension,
                                    aKeys);
}

NetworkDiagnostics *NetworkDiagnostics::clone() // TODO have proper copy constructor
{
    NetworkDiagnostics *cloned = new NetworkDiagnostics(*this);
    cloned->mUuid.parse(this->mUuid.toString());

    return cloned;
}

// EnergyScanDiagnostics class implementation
EnergyScanDiagnostics::EnergyScanDiagnostics()
    : BasicDiagnostics()
{
    mReport.report.clear();
}

EnergyScanDiagnostics::~EnergyScanDiagnostics()
{
}

std::string EnergyScanDiagnostics::getTypeName() const
{
    return std::string(ENERGYSCAN_TYPE_NAME);
}

std::string EnergyScanDiagnostics::toJsonString(std::set<std::string> aKeys)
{
    return Json::SparseEnergyReport2JsonString(mReport, aKeys);
}

EnergyScanDiagnostics *EnergyScanDiagnostics::clone() // TODO have proper copy constructor
{
    EnergyScanDiagnostics *cloned = new EnergyScanDiagnostics(*this);
    cloned->mUuid.parse(this->mUuid.toString());

    return cloned;
}

// DiagnosticsCollection class implementation
DiagnosticsCollection::DiagnosticsCollection() = default;

DiagnosticsCollection::~DiagnosticsCollection()
{
}

void DiagnosticsCollection::addItem(BasicDiagnostics *aItem)
{
    // do not exceed a max size of the collection
    while (mCollection.size() >= MAX_DIAG_COLLECTION_ITEMS)
    {
        evictOldestItem();
    }

    DiagnosticsCollection::mCollection[aItem->mUuid.toString()] = aItem->clone();

    // add to mHoldsTypes
    incrHoldsTypes(aItem->getTypeName());

    // add to age sorted list for easy erase, first in first erased
    mAgeSortedItemIds.push_back(aItem->mUuid.toString());

    otbrLogWarning("%s:%d - %s - %s", __FILE__, __LINE__, __func__, aItem->mUuid.toString().c_str());
}

BasicDiagnostics *DiagnosticsCollection::getItem(std::string aKey)
{
    auto it = mCollection.find(aKey);
    if (it != mCollection.end())
    {
        return static_cast<BasicDiagnostics *>(it->second);
    }
    return nullptr;
}

DiagnosticsCollection gDiagnosticsCollection;

} // namespace rest
} // namespace otbr
