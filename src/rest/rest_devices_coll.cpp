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
 * @brief   Implements api/devices collection and conversion of items and collection to json and json:api
 */

#include "rest_devices_coll.hpp"
#include "timestamp.hpp"
#include <algorithm>
#include <assert.h>
#include <memory>
#include "common/code_utils.hpp"
#include "rest/json.hpp"

extern "C" {
#include <cJSON.h>
}

namespace otbr {
namespace rest {

// BasicDevices class implementation
BasicDevices::BasicDevices(std::string aExtAddr)
    : BasicCollectionItem()
{
    mItemId = aExtAddr;
}

BasicDevices::~BasicDevices()
{
}

std::string BasicDevices::ToJsonApiItem(std::set<std::string> aKeys)
{
    return Json::jsonStr2JsonApiItem(GetId(), GetTypeName(), ToJsonStringTs(aKeys));
}

// ThreadDevices class implementation
ThreadDevice::ThreadDevice(std::string aExtAddr)
    : BasicDevices(aExtAddr)
{
}

ThreadDevice::~ThreadDevice()
{
}

std::string ThreadDevice::GetTypeName() const
{
    return std::string(DEVICE_TYPE_NAME);
}

void ThreadDevice::SetEui64(otExtAddress aEui)
{
    mDeviceInfo.mEui64 = aEui;
    // set updated timestamp
    mUpdated = std::chrono::system_clock::now();
}

void ThreadDevice::SetHostname(std::string aHostname)
{
    mDeviceInfo.mHostName = aHostname;
    // set updated timestamp
    mUpdated = std::chrono::system_clock::now();
}

void ThreadDevice::SetIpv6Omr(otIp6Address aIpv6)
{
    mDeviceInfo.mIp6Addr = aIpv6;
    // set updated timestamp
    mUpdated = std::chrono::system_clock::now();
}

void ThreadDevice::SetMlEidIid(otExtAddress aMlEidIid)
{
    mDeviceInfo.mMlEidIid = aMlEidIid;
    // set updated timestamp
    mUpdated = std::chrono::system_clock::now();
}

void ThreadDevice::SetMode(otLinkModeConfig aMode)
{
    mDeviceInfo.mode = aMode;
    // set updated timestamp
    mUpdated = std::chrono::system_clock::now();
}

void ThreadDevice::SetRole(std::string aRole)
{
    mDeviceInfo.mRole = aRole;
    // set updated timestamp
    mUpdated = std::chrono::system_clock::now();
}

std::string ThreadDevice::ToJsonString(std::set<std::string> aKeys)
{
    return Json::SparseDeviceInfo2JsonString(mDeviceInfo, aKeys);
}

// ThisThreadDevice class implementation
ThisThreadDevice::ThisThreadDevice(std::string aExtAddr)
    : ThreadDevice(aExtAddr)
{
}

ThisThreadDevice::~ThisThreadDevice()
{
}

std::string ThisThreadDevice::GetTypeName() const
{
    return std::string(DEVICE_BR_TYPE_NAME);
}

std::string ThisThreadDevice::ToJsonString(std::set<std::string> aKeys)
{
    std::string str1 = Json::SparseDeviceInfo2JsonString(mDeviceInfo, aKeys);
    std::string str2 = Json::SparseNode2JsonString(mNodeInfo, aKeys);

    // Parse str1 and str2 into cJSON objects
    cJSON *json1 = str1.empty() ? cJSON_CreateObject() : cJSON_Parse(str1.c_str());
    cJSON *json2 = str2.empty() ? cJSON_CreateObject() : cJSON_Parse(str2.c_str());

    if (json1 == nullptr || json2 == nullptr)
    {
        // Handle parsing error
        if (json1 != nullptr)
        {
            cJSON_Delete(json1);
        }
        if (json2 != nullptr)
        {
            cJSON_Delete(json2);
        }
        return ""; // Return an empty JSON object in case of error
    }

    // Merge json2 into json1
    cJSON *json1_child = nullptr;
    cJSON_ArrayForEach(json1_child, json2)
    {
        cJSON *json1_item = cJSON_GetObjectItem(json1, json1_child->string);
        if (json1_item == nullptr)
        {
            cJSON_AddItemToObject(json1, json1_child->string, cJSON_Duplicate(json1_child, 1));
        }
        else
        {
            // Handle duplicate keys if necessary
            cJSON_ReplaceItemInObject(json1, json1_child->string, cJSON_Duplicate(json1_child, 1));
        }
    }

    // Serialize the merged cJSON object back into a JSON string
    char       *merged_str = cJSON_PrintUnformatted(json1);
    std::string result(merged_str);

    // Clean up
    cJSON_Delete(json1);
    cJSON_Delete(json2);
    cJSON_free(merged_str);

    return result;
}

// DevicesCollection class implementation
DevicesCollection::DevicesCollection()
{
    // reserve space for the maximum number of items in the collection
    // to avoid rehashing
    mCollection.reserve(MAX_DEVICES_COLLECTION_ITEMS);
}

DevicesCollection::~DevicesCollection()
{
}

cJSON *DevicesCollection::GetCollectionMeta()
{
    return Json::CreateMetaCollection(0, GetMaxCollectionSize(), mCollection.size());
}

void DevicesCollection::AddItem(std::unique_ptr<BasicDevices> aItem)
{
    if (!aItem)
    {
        otbrLogWarning("%s:%d - %s - Received nullptr, item not added", __FILE__, __LINE__, __func__);
        return;
    }

    // do not exceed a max size of the collection
    while (mCollection.size() >= MAX_DEVICES_COLLECTION_ITEMS)
    {
        EvictOldestItem();
    }

    // add to mHoldsTypes
    IncrHoldsTypes(aItem->GetTypeName());

    // add to age sorted list for easy erase, first in first erased
    if (std::find(mAgeSortedItemIds.begin(), mAgeSortedItemIds.end(), aItem->GetId()) == mAgeSortedItemIds.end())
    {
        mAgeSortedItemIds.push_back(aItem->GetId());
    }

    otbrLogWarning("%s:%d - %s - %s", __FILE__, __LINE__, __func__, aItem->GetId().c_str());
    auto result = mCollection.emplace(aItem->GetId(), std::move(aItem));
    if (!result.second)
    {
        otbrLogWarning("%s:%d - %s - failed.", __FILE__, __LINE__, __func__);
    }
}

BasicDevices *DevicesCollection::GetItem(std::string aItemId)
{
    auto it = mCollection.find(aItemId);
    if (it != mCollection.end())
    {
        return static_cast<BasicDevices *>(it->second.get());
    }
    return nullptr;
}

} // namespace rest
} // namespace otbr
