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
 * @brief   Implements collection and conversion of items and collection to json and json:api
 */

#ifndef OTBR_REST_GENERIC_COLLLECTION_HPP_
#define OTBR_REST_GENERIC_COLLLECTION_HPP_

#include "uuid.hpp"
#include <chrono>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include "rest/types.hpp"

struct cJSON;

namespace otbr {
namespace rest {

/**
 * @brief This virtual class implements a general item of a json:api collection.
 */
class BasicCollectionItem
{
public:
    /**
     * Constructor for BasicCollectionItem.
     */
    BasicCollectionItem();

    /**
     * Destructor for BasicCollectionItem.
     */
    virtual ~BasicCollectionItem();

    /**
     * Get the type name of the collection item.
     *
     * @returns The type name.
     */
    virtual std::string GetTypeName() const = 0;

    /**
     * Convert the collection item to a JSON string.
     *
     * @param[in] aKeys  The set of keys to include in the JSON string.
     *
     * @returns The JSON string representation of the item.
     */
    virtual std::string ToJsonString(std::set<std::string> aKeys) = 0;

    /**
     * Convert the collection item to a JSON:API item string.
     *
     * @param[in] aKeys  The set of keys to include in the JSON:API item string.
     *
     * @returns The JSON:API item string representation of the item, including the created timestamp.
     */
    virtual std::string ToJsonApiItem(std::set<std::string> aKeys) = 0;

    /**
     * @brief Concat str1, '.', and str2.
     *
     * @param str1 A string.
     * @param str2 Another string.
     * @return std::string str1.str2
     */
    std::string concat(const char *str1, const char *str2);

    /**
     * @brief Check if aKey is part of aSet.
     *
     * @param aSet A set of strings representing keys.
     * @param aKey A key
     * @return true  If aSet is empty (default, for return any key),
     *               aKey is in aSet, or both,
     *               aKey = 'atoplevelKey.asecondlevelKey' AND 'atoplevelKey.' are in aSet.
     * @return false Otherwise.
     *
     * the logic in this implementation relates to parsing the query to the set aSet
     * as implemented by BasicCollectionItem::parseQueryFieldValues
     */
    bool hasKey(std::set<std::string> aSet, std::string aKey);

    /**
     * @brief Check if aKey or aKey. is in aSet.
     *
     * @param aSet A set of strings representing keys.
     * @param aKey A potential toplevel key.
     * @return true if 'aKey' or 'aKey.' is in aSet
     * @return false Otherwise.
     */
    bool hasToplevelKey(std::set<std::string> aSet, std::string aKey);

    /**
     * Parse the query field values from a string.
     *
     * @param[in] aKeys  The string containing the keys.
     *
     * @returns The set of parsed keys.
     */
    std::set<std::string> ParseQueryFieldValues(std::string aKeys);

    /**
     * Convert the collection item to a JSON string including the created timestamp.
     *
     * @param[in] aKeys  The set of keys to include in the JSON string.
     *
     * @returns The JSON string representation of the item with timestamp.
     */
    std::string ToJsonStringTs(std::set<std::string> aKeys);

    Uuid                                  mUuid;
    std::chrono::system_clock::time_point mCreated;
    std::chrono::system_clock::time_point mUpdated;
};

/**
 * @brief This virtual class implements a general json:api collection.
 */
class BasicCollection
{
    std::map<std::string, uint16_t> mHoldsTypes;

public:
    /**
     * Constructor for BasicCollection.
     */
    BasicCollection();

    /**
     * Destructor for BasicCollection.
     */
    virtual ~BasicCollection();

    /**
     * Returns the collection name, which is used in the path.
     *
     * @returns The collection name.
     */
    virtual std::string GetCollectionName() const = 0;

    /**
     * Get the maximum allowed size the collection can grow to.
     *
     * Once the max size is reached, the oldest item
     * is going to be evicted for each new item added.
     *
     * @returns The maximum collection size.
     */
    virtual uint16_t GetMaxCollectionSize() const = 0;

    /**
     * Creates the meta data of the collection.
     *
     * @return  The cJSON object representing the meta data for the collection.
     *          Must be cleaned up by the caller.
     */
    virtual cJSON *GetCollectionMeta() = 0;

    // derived classes may implement
    // virtual void          AddItem(BasicCollectionItem *aItem) = 0;
    // virtual BasicCollectionItem  *getItem(std::string key);

    /**
     * Get the types of items contained in the collection.
     *
     * @return The set of item types.
     */
    std::set<std::string> GetContainedTypes();

    /**
     * Find a item with aItemId and return its attributes as json formatted string,
     * and filtered according aFields
     *
     * @return A json string with the item attributes.
     */
    std::string ToJsonStringItemId(std::string aItemId, const std::map<std::string, std::string> aFields);

    /**
     * Return the class specific item attributes as json string.
     */
    std::string ToJsonString();

    /**
     * Find a item with aItemId and returns it as json:api item (id, type, attributes, ...) formatted as json string,
     * and filtered according aFields
     *
     * @return A json:api string with the item id, type, attributes, ...
     */
    std::string ToJsonApiItemId(std::string aItemId, const std::map<std::string, std::string> aFields);

    /**
     * Returns all items as json:api item (id, type, attributes, ...) formatted as json string,
     * and filtered according aFields
     *
     * @return A json:api string with a list of all items (id, type, attributes, ...)
     */
    std::string ToJsonApiItems(const std::map<std::string, std::string> aFields);

    /**
     * Returns the collection including meta data,
     * and filtered according aFields
     *
     * @return A json:api collection string with data field comprising a list of all items (id, type, attributes, ...)
     * and meta field.
     */
    std::string ToJsonApiColl(const std::map<std::string, std::string> aFields);

    /**
     * Add a item type to the collections list of types.
     *
     * @param[in] aTypeName  The item type to add.
     */
    void IncrHoldsTypes(std::string aTypeName);

    /**
     * Remove a item type from the collections list of types.
     *
     * @param[in] aTypeName  The item type to remove.
     */
    void DecrHoldsTypes(std::string aTypeName);

    /**
     * Evict the oldest item from the collection.
     */
    void EvictOldestItem();

    /**
     * Delete a item from the collection.
     *
     * @param[in] aItem  The item to delete.
     */
    otError Delete(std::string aItemId);

    /**
     * Clear the collection.
     */
    void DeleteAll();

    /**
     * Get the actual size of the collection.
     *
     * @return The actual size of the collection.
     */
    uint32_t Size() { return mCollection.size(); };

protected:
    std::unordered_map<std::string, std::unique_ptr<BasicCollectionItem>> mCollection;
    std::list<std::string>                                                mAgeSortedItemIds;
};

} // namespace rest
} // namespace otbr

#endif // OTBR_REST_GENERIC_COLLLECTION_HPP_
