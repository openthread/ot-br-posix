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
 * @brief   Implements generic collection and conversion of items and collection to json and json:api
 *
 */
#include "rest_generic_collection.hpp"
#include "timestamp.hpp"
#include <assert.h>
#include "common/code_utils.hpp"
#include "rest/json.hpp"

extern "C" {
#include <cJSON.h>
}

namespace otbr {
namespace rest {

BasicCollectionItem::BasicCollectionItem()
{
    mUuid.GenerateRandom();
    mCreated = std::chrono::system_clock::now();
    mUpdated = mCreated;
}

BasicCollectionItem::~BasicCollectionItem()
{
}

std::set<std::string> BasicCollectionItem::ParseQueryFieldValues(std::string aKeys)
{
    std::set<std::string> keys;
    char                 *token;
    const std::string     separators = " ,";
    char                 *ckeys      = strdup(aKeys.c_str());

    token = strtok(ckeys, separators.c_str());
    while (token != nullptr)
    {
        std::string key(token);
        keys.emplace(key); // TODO: check key is allowed

        // if key contains dot, split after dot and emplace
        // 'key.' to indicate partial subkeys are wanted
        // currently limited to one level of subkeys
        std::size_t pos = key.find(".");
        if (pos != std::string::npos)
        {
            std::string sub  = key.substr(0, pos + 1);
            std::size_t pos2 = key.find(".", pos + 1);
            if (pos2 == std::string::npos)
            {
                keys.emplace(sub); // TODO: check key is allowed
            }
        }

        token = strtok(nullptr, separators.c_str());
    }

    return keys;
}

std::string BasicCollectionItem::ToJsonStringTs(std::set<std::string> aKeys)
{
    cJSON      *root = cJSON_Parse(ToJsonString(aKeys).c_str());
    std::string ret;

    cJSON_AddStringToObject(root, "created", toRfc3339(mCreated).c_str());

    if (mUpdated.time_since_epoch() != mCreated.time_since_epoch())
    {
        cJSON_AddStringToObject(root, "updated", toRfc3339(mUpdated).c_str());
    }

    ret = Json::Json2String(root);
    cJSON_Delete(root);

    return ret;
}

// BasicCollection class implementation
BasicCollection::BasicCollection() = default;

BasicCollection::~BasicCollection()
{
}

// example, should be overriden with proper class of attributes
/*
void BasicCollection::AddItem(std::unique_ptr<BasicCollectionItem>aItem)
{
    IncrHoldsTypes(aItem->GetTypeName());

    otbrLogDebug("%s:%d - %s - %s", __FILE__, __LINE__, __func__, aItem->mUuid.toString().c_str());
}
*/

void BasicCollection::IncrHoldsTypes(std::string aTypeName)
{
    // add to mHoldsTypes
    auto it = mHoldsTypes.find(aTypeName);
    if (it == mHoldsTypes.end())
    {
        mHoldsTypes[aTypeName] = 1;
    }
    else
    {
        it->second++;
    }
}

void BasicCollection::DecrHoldsTypes(std::string aTypeName)
{
    // add to mHoldsTypes
    auto it = mHoldsTypes.find(aTypeName);
    if (it == mHoldsTypes.end())
    {
        // already removed
        return;
    }
    else
    {
        if (it->second > 0)
        {
            it->second--;
        }
        if (it->second == 0)
        {
            mHoldsTypes.erase(it);
        }
    }
}

void BasicCollection::Clear()
{
    mCollection.clear();
}

std::set<std::string> BasicCollection::GetContainedTypes()
{
    std::set<std::string> typeNames;

    for (auto &it : mCollection)
    {
        typeNames.emplace(it.second.get()->GetTypeName());
    }
    return typeNames;
}

std::string BasicCollection::ToJsonStringItemId(std::string aItemId, const std::map<std::string, std::string> aFields)
{
    std::unordered_map<std::string, std::unique_ptr<BasicCollectionItem>>::const_iterator itemItr;
    BasicCollectionItem                                                                  *item;
    std::set<std::string>                                                                 keySet;
    std::map<std::string, std::string>::const_iterator                                    field;

    itemItr = mCollection.find(aItemId);
    if ((!aItemId.empty()) && (itemItr != mCollection.end()))
    {
        item = dynamic_cast<BasicCollectionItem *>(itemItr->second.get());
        assert(item);
        if (!aFields.empty())
        {
            field = aFields.find(item->GetTypeName());
            if (field == aFields.end())
            {
                // typeName not requested
                ExitNow();
            }
            // else parse requested attribute keys, e.g. '?fields[<typeName>]=<key1>,<key2>'
            keySet = item->ParseQueryFieldValues(field->second);
        }
        return item->ToJsonString(keySet);
    }
    else
    {
        otbrLogDebug("%s:%d - %s - Collection has %d items, without itemId %s", __FILE__, __LINE__, __func__,
                     mCollection.size(), aItemId.c_str());
    }
exit:
    return "";
}

std::string BasicCollection::ToJsonString()
{
    cJSON                *root = cJSON_CreateArray();
    std::string           ret;
    std::set<std::string> keySet;

    for (auto &item : mCollection)
    {
        cJSON_AddItemToArray(
            root, cJSON_Parse(dynamic_cast<BasicCollectionItem *>(item.second.get())->ToJsonString(keySet).c_str()));
    }
    ret = Json::Json2String(root);
    cJSON_Delete(root);

    return ret;
}

std::string BasicCollection::ToJsonApiItemId(std::string aItemId, const std::map<std::string, std::string> aFields)
{
    cJSON                                             *root;
    std::string                                        ret = "";
    std::set<std::string>                              keySet;
    std::map<std::string, std::string>::const_iterator field;

    BasicCollectionItem *item;

    auto itemItr = mCollection.find(aItemId);
    if (itemItr != mCollection.end())
    {
        item = dynamic_cast<BasicCollectionItem *>(itemItr->second.get());
        assert(item);
        // check if item type in query, e.g. '?fields[<typeName>]'
        if (!aFields.empty())
        {
            field = aFields.find(item->GetTypeName());
            if (field == aFields.end())
            {
                // typeName not requested
                ExitNow();
            }
            // else parse requested attribute keys, e.g. '?fields[<typeName>]=<key1>,<key2>'
            keySet = item->ParseQueryFieldValues(field->second);
        }
        root = cJSON_CreateObject();
        cJSON_AddItemToObject(root, "data", cJSON_Parse(item->ToJsonApiItem(keySet).c_str()));
        ret = Json::Json2String(root);
        cJSON_Delete(root);
    }
    else
    {
        otbrLogDebug("%s:%d - %s - Collection has %d items, without itemId %s", __FILE__, __LINE__, __func__,
                     mCollection.size(), aItemId.c_str());
    }
exit:
    return ret;
}

std::string BasicCollection::ToJsonApiItems(const std::map<std::string, std::string> aFields)
{
    cJSON                                             *root = cJSON_CreateArray();
    std::string                                        ret;
    std::set<std::string>                              keySet;
    std::map<std::string, std::string>::const_iterator field;
    BasicCollectionItem                               *item;

    for (auto &itemItr : mCollection)
    {
        item = dynamic_cast<BasicCollectionItem *>(itemItr.second.get());
        assert(item);
        // check if item type in query, e.g. '?fields[<typeName>]'
        keySet.clear();
        if (!aFields.empty())
        {
            field = aFields.find(item->GetTypeName());
            if (field == aFields.end())
            {
                // typeName not requested
                continue;
            }
            // else parse requested attribute keys, e.g. '?fields[<typeName>]=<key1>,<key2>'
            keySet = item->ParseQueryFieldValues(field->second);
        }
        cJSON_AddItemToArray(root, cJSON_Parse(item->ToJsonApiItem(keySet).c_str()));
    }
    ret = Json::Json2String(root);
    cJSON_Delete(root);

    return ret;
}

std::string BasicCollection::ToJsonApiColl(const std::map<std::string, std::string> aFields)
{
    std::string meta, data;

    data       = ToJsonApiItems(aFields);
    cJSON *obj = GetCollectionMeta();
    meta       = Json::Json2String(obj);
    cJSON_Delete(obj);

    return Json::jsonStr2JsonApiColl(data, meta);
}

void BasicCollection::EvictOldestItem(void)
{
    if (!mAgeSortedItemIds.empty())
    {
        std::unordered_map<std::string, std::unique_ptr<BasicCollectionItem>>::const_iterator it;

        // Get the oldest key from the list
        std::string oldestKey = mAgeSortedItemIds.front();

        it = mCollection.find(oldestKey);
        if (it != mCollection.end())
        {
            // Decrement the count of types
            DecrHoldsTypes(it->second.get()->GetTypeName());
            // Remove from the map
            mCollection.erase(oldestKey);
        }
        // Remove the oldest key from the list
        mAgeSortedItemIds.pop_front();
        otbrLogWarning("%s:%d - %s - %s: %s from %s", __FILE__, __LINE__, __func__, "Evicted Item", oldestKey.c_str(),
                       this->GetCollectionName().c_str());
    }
}

} // namespace rest
} // namespace otbr
