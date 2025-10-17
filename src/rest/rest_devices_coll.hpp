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

#ifndef OTBR_REST_DEVICES_COLLLECTION_HPP_
#define OTBR_REST_DEVICES_COLLLECTION_HPP_

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

#define MAX_DEVICES_COLLECTION_ITEMS 200
#define DEVICE_COLLECTION_NAME "devices"         // name of the device collection, should corespond to url api/devices
#define DEVICE_TYPE_NAME "threadDevice"          // general Thread device
#define DEVICE_BR_TYPE_NAME "threadBorderRouter" // also contains NodeInfo if it is this node.

/**
 * @brief This virtual class implements a general json:api item for holding device attributes.
 */
class BasicDevices : public BasicCollectionItem
{
public:
    /**
     * Constructor for BasicDevices.
     */
    BasicDevices(std::string aExtAddr);

    /**
     * Destructor for BasicDevices.
     */
    virtual ~BasicDevices();

    /**
     * Get the id of the item.
     *
     * @returns The items id.
     */
    std::string GetId() { return mItemId; }

    /**
     * Convert the collection item to a JSON:API item string.
     *
     * @param[in] aKeys  The set of keys to include in the JSON:API item string.
     *
     * @returns The JSON:API item string representation of the item, including the created timestamp.
     */
    std::string ToJsonApiItem(std::set<std::string> aKeys) override;

protected:
    std::string mItemId;
};

/**
 * @brief This class implements a general json:api item for holding static (or mostly static) device attributes.
 */
class ThreadDevice : public BasicDevices
{
public:
    /**
     * Constructor for ThreadDevice.
     */
    ThreadDevice(std::string aExtAddr);

    /**
     * Destructor for ThreadDevice.
     */
    ~ThreadDevice();

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

    /**
     * Set the eui64 of the device item.
     */
    void SetEui64(otExtAddress aEui);

    /**
     * Set the hostname of the device item.
     */
    void SetHostname(std::string aHostname);

    /**
     * Set the Ipv6 off-mesh-routeable address of the device item.
     */
    void SetIpv6Omr(otIp6Address aIpv6);

    /**
     * Set the mesh-local interfaceId of the device item.
     */
    void SetMlEidIid(otExtAddress aIid);

    /**
     * Set the mode of the device item.
     */
    void SetMode(otLinkModeConfig aMode);

    /**
     * Set the role of the device item.
     */
    void SetRole(std::string aRole);

    otbr::rest::DeviceInfo mDeviceInfo;
};

/**
 * @brief This class implements a json:api item for holding static (or mostly static) device attributes of 'this'
 * device.
 */
class ThisThreadDevice : public ThreadDevice
{
public:
    /**
     * Constructor for ThisThreadDevice.
     */
    ThisThreadDevice(std::string aExtAddr);

    /**
     * Destructor for ThisThreadDevice.
     */
    ~ThisThreadDevice();

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

    otbr::rest::NodeInfo mNodeInfo;
};

/**
 * @brief This class implements a json:api collection for holding device items.
 */
class DevicesCollection : public BasicCollection
{
    std::map<std::string, uint16_t> mHoldsTypes;

public:
    /**
     * Constructor for DevicesCollection.
     */
    DevicesCollection();

    /**
     * Destructor for DevicesCollection.
     */
    ~DevicesCollection();

    /**
     * Get the collection name.
     *
     * @returns The collection name.
     */
    std::string GetCollectionName() const override { return std::string(DEVICE_COLLECTION_NAME); }

    /**
     * Get the maximum allowed size the collection can grow to.
     *
     * Once the max size is reached, the oldest item
     * is going to be evicted for each new item added.
     *
     * @returns The maximum collection size.
     */
    uint16_t GetMaxCollectionSize() const override { return MAX_DEVICES_COLLECTION_ITEMS; }

    /**
     * Creates the meta data of the collection.
     *
     * @returns The meta data cJSON object.
     */
    cJSON *GetCollectionMeta() override;

    /**
     * Add a item to the collection.
     *
     * @param[in] aItem  The item to add.
     */
    void AddItem(std::unique_ptr<BasicDevices> aItem);

    /**
     * Get a item from the collection.
     *
     * @param[in] aItemId  The key of the item to get.
     *
     * @returns The item.
     */
    BasicDevices *GetItem(std::string aItemId);
};

} // namespace rest
} // namespace otbr

#endif // OTBR_REST_DEVICES_COLLLECTION_HPP_
