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
 * @brief   Implements collection and conversion of items and collection to json and json:api
 *
 */

#ifndef OTBR_REST_GENERIC_COLLLECTION_HPP_
#define OTBR_REST_GENERIC_COLLLECTION_HPP_

#include "uuid.hpp"
#include <chrono>
#include <list>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include "rest/types.hpp"

namespace otbr {
namespace rest {

/**
 * @brief This virtual class implements a general item of a json:api collection.
 *
 */
class BasicCollectionItem
{
public:
    //
    BasicCollectionItem();
    virtual ~BasicCollectionItem();

    virtual std::string getTypeName() const                        = 0;
    virtual std::string toJsonString(std::set<std::string> aKeys)  = 0;
    virtual std::string toJsonApiItem(std::set<std::string> aKeys) = 0;

    std::set<std::string> parseQueryFieldValues(std::string aKeys);
    // returns a JsonString including created timestamp
    std::string toJsonStringTs(std::set<std::string> aKeys);
    // std::string toJsonApiItem(std::set<std::string> aKeys);

    UUID                                  mUuid;
    std::chrono::system_clock::time_point mCreated;
    std::chrono::system_clock::time_point mUpdated;
};

/**
 * @brief This virtual class implements a general json:api collection.
 *
 */
class BasicCollection
{
    std::map<std::string, uint16_t> mHoldsTypes;

public:
    BasicCollection();
    virtual ~BasicCollection();
    virtual std::string getCollectionName() const    = 0;
    virtual uint16_t    getMaxCollectionSize() const = 0;
    // derived classes may implement
    // virtual void          addItem(BasicCollectionItem *aItem) = 0;
    // virtual BasicCollectionItem  *getItem(std::string key);

    std::set<std::string> getContainedTypes();
    std::string           toJsonStringItemId(std::string aItemId, const std::map<std::string, std::string> aFields);
    std::string           toJsonString();
    std::string           toJsonApiItemId(std::string aItemId, const std::map<std::string, std::string> aFields);
    std::string           toJsonApiItems(const std::map<std::string, std::string> aFields);
    std::string           toJsonApiColl(const std::map<std::string, std::string> aFields);

    void incrHoldsTypes(std::string aTypeName);
    void decrHoldsTypes(std::string aTypeName);
    void evictOldestItem();
    void clear();

protected:
    std::unordered_map<std::string, BasicCollectionItem *> mCollection;
    std::list<std::string>                                 mAgeSortedItemIds;
};

// the collection example. We do not want a BasicCollection instance.
// extern BasicCollection gBasicCollection;

} // namespace rest
} // namespace otbr

#endif // OTBR_REST_GENERIC_COLLLECTION_HPP_
