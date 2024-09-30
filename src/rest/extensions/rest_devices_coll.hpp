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

namespace otbr {
namespace rest {

#define MAX_DEVICES_COLLECTION_ITEMS 200
#define DEVICE_COLLECTION_NAME "devices"         // name of the device collection, should corespond to url api/devices
#define DEVICE_TYPE_NAME "threadDevice"          // general Thread device
#define DEVICE_BR_TYPE_NAME "threadBorderRouter" // also contains NodeInfo if it is this node.

/**
 * @brief This virtual class implements a general json:api item for holding device attributes.
 *
 */
class BasicDevices : public BasicCollectionItem
{
public:
    //
    BasicDevices(std::string aExtAddr);
    virtual BasicDevices *clone() = 0;
    virtual ~BasicDevices();
    std::string getId() { return mItemId; }
    std::string toJsonApiItem(std::set<std::string> aKeys) override;

protected:
    std::string mItemId;
};

/**
 * @brief This class implements a general json:api item for holding static (or mostly static) device attributes.
 *
 */
class ThreadDevice : public BasicDevices
{
public:
    ThreadDevice(std::string aExtAddr);
    ~ThreadDevice();
    otbr::rest::DeviceInfo mDeviceInfo;
    std::string            getTypeName() const override;
    void                   setEui64(otExtAddress aEui);
    void                   setHostname(std::string aHostname);
    void                   setIpv6Omr(otIp6Address aIpv6);
    void                   setMlEidIid(otExtAddress aIid);
    void                   setMode(otLinkModeConfig aMode);
    void                   setRole(std::string aRole);
    std::string            toJsonString(std::set<std::string> aKeys);
    ThreadDevice          *clone() override;
};

/**
 * @brief This class implements a json:api item for holding static (or mostly static) device attributes of 'this'
 * device.
 *
 */
class ThisThreadDevice : public ThreadDevice
{
public:
    ThisThreadDevice(std::string aExtAddr);
    ~ThisThreadDevice();
    otbr::rest::NodeInfo mNodeInfo;
    std::string          getTypeName() const override;
    std::string          toJsonString(std::set<std::string> aKeys);
    ThisThreadDevice    *clone() override;
};

/**
 * @brief This class implements a json:api collection for holding device items.
 *
 */
class DevicesCollection : public BasicCollection
{
    std::map<std::string, uint16_t> mHoldsTypes;

public:
    DevicesCollection();
    ~DevicesCollection();
    std::string   getCollectionName() const override { return std::string(DEVICE_COLLECTION_NAME); }
    uint16_t      getMaxCollectionSize() const override { return MAX_DEVICES_COLLECTION_ITEMS; }
    void          addItem(BasicDevices *aItem);
    BasicDevices *getItem(std::string aKey);
};

// the collection
extern DevicesCollection gDevicesCollection;

} // namespace rest
} // namespace otbr

#endif // OTBR_REST_DEVICES_COLLLECTION_HPP_
