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
 * @brief   Implements api/devices collection and conversion of items and collection to json and json:api
 *
 */

#include "rest_devices_coll.hpp"
#include "timestamp.hpp"
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

std::string BasicDevices::toJsonApiItem(std::set<std::string> aKeys)
{
    return Json::jsonStr2JsonApiItem(getId(), getTypeName(), toJsonStringTs(aKeys));
}

// ThreadDevices class implementation
ThreadDevice::ThreadDevice(std::string aExtAddr)
    : BasicDevices(aExtAddr)
{
}

ThreadDevice::~ThreadDevice()
{
}

std::string ThreadDevice::getTypeName() const
{
    return std::string(DEVICE_TYPE_NAME);
}

void ThreadDevice::setEui64(otExtAddress aEui)
{
    mDeviceInfo.mEui64 = aEui;
    // set updated timestamp
    mUpdated = std::chrono::system_clock::now();
}

void ThreadDevice::setHostname(std::string aHostname)
{
    mDeviceInfo.mHostName = aHostname;
    // set updated timestamp
    mUpdated = std::chrono::system_clock::now();
}

void ThreadDevice::setIpv6Omr(otIp6Address aIpv6)
{
    mDeviceInfo.mIp6Addr = aIpv6;
    // set updated timestamp
    mUpdated = std::chrono::system_clock::now();
}

void ThreadDevice::setMlEidIid(otExtAddress aMlEidIid)
{
    mDeviceInfo.mMlEidIid = aMlEidIid;
    // set updated timestamp
    mUpdated = std::chrono::system_clock::now();
}

void ThreadDevice::setMode(otLinkModeConfig aMode)
{
    mDeviceInfo.mode = aMode;
    // set updated timestamp
    mUpdated = std::chrono::system_clock::now();
}

void ThreadDevice::setRole(std::string aRole)
{
    mDeviceInfo.mRole = aRole;
    // set updated timestamp
    mUpdated = std::chrono::system_clock::now();
}

std::string ThreadDevice::toJsonString(std::set<std::string> aKeys)
{
    return Json::SparseDeviceInfo2JsonString(mDeviceInfo, aKeys);
}

ThreadDevice *ThreadDevice::clone() // TODO have proper copy constructor
{
    ThreadDevice *cloned = new ThreadDevice(*this);
    cloned->mItemId      = this->mItemId;
    cloned->mUuid.parse(this->mUuid.toString());

    return cloned;
}

// ThisThreadDevice class implementation
ThisThreadDevice::ThisThreadDevice(std::string aExtAddr)
    : ThreadDevice(aExtAddr)
{
}

ThisThreadDevice::~ThisThreadDevice()
{
}

std::string ThisThreadDevice::getTypeName() const
{
    return std::string(DEVICE_BR_TYPE_NAME);
}

std::string ThisThreadDevice::toJsonString(std::set<std::string> aKeys)
{
    std::string str1 = Json::SparseDeviceInfo2JsonString(mDeviceInfo, aKeys);
    std::string str2 = Json::SparseNode2JsonString(mNodeInfo, aKeys);
    str1.back()      = ',';
    str2.erase(str2.begin());
    return str1 + str2;
}

ThisThreadDevice *ThisThreadDevice::clone() // TODO have proper copy constructor
{
    ThisThreadDevice *cloned = new ThisThreadDevice(*this);
    cloned->mItemId          = this->mItemId;
    cloned->mUuid.parse(this->mUuid.toString());

    return cloned;
}

// DevicesCollection class implementation
DevicesCollection::DevicesCollection() = default;

DevicesCollection::~DevicesCollection()
{
}

void DevicesCollection::addItem(BasicDevices *aItem)
{
    // do not exceed a max size of the collection
    while (mCollection.size() >= MAX_DEVICES_COLLECTION_ITEMS)
    {
        evictOldestItem();
    }

    BasicDevices *cloned                            = aItem->clone();
    DevicesCollection::mCollection[cloned->getId()] = cloned;

    // add to mHoldsTypes
    incrHoldsTypes(aItem->getTypeName());

    // add to age sorted list for easy erase, first in first erased
    mAgeSortedItemIds.push_back(aItem->getId());

    otbrLogWarning("%s:%d - %s - %s", __FILE__, __LINE__, __func__, cloned->getId().c_str());
}

BasicDevices *DevicesCollection::getItem(std::string aKey)
{
    auto it = mCollection.find(aKey);
    if (it != mCollection.end())
    {
        return static_cast<BasicDevices *>(it->second);
    }
    return nullptr;
}

DevicesCollection gDevicesCollection;

} // namespace rest
} // namespace otbr
