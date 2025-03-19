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

#ifndef EXTENSIONS_ACTIONS_LIST_HPP_
#define EXTENSIONS_ACTIONS_LIST_HPP_

#include <map>
#include <memory>

#include <openthread/error.h>

#include "rest_generic_collection.hpp"
#include "uuid.hpp"
#include "actions/action.hpp"

struct cJSON;

namespace otbr {
namespace rest {

#define MAX_ACTIONS_COLLECTION_ITEMS 200
#define ACTIONS_COLLECTION_NAME "actions"

class Services;

/**
 * @brief This class implements a json:api collection for holding action items.
 *
 */
class ActionsCollection : public BasicCollection
{
    std::map<std::string, uint16_t> mHoldsTypes;

public:
    /**
     * Constructor for a Actions Collection.
     *
     */
    ActionsCollection();

    /**
     * Destructor for a Actions Collection.
     *
     */
    ~ActionsCollection();

    /**
     * Get the name of the collection.
     *
     * @returns The name of the collection.
     */
    std::string GetCollectionName() const override { return std::string(ACTIONS_COLLECTION_NAME); }

    /**
     * Get the maximum allowed size the collection can grow to.
     *
     * Once the max size is reached, the oldest item
     * is going to be evicted for each new item added.
     *
     * @returns The maximum collection size.
     */
    uint16_t GetMaxCollectionSize() const override { return MAX_ACTIONS_COLLECTION_ITEMS; }

    /**
     * Creates the meta data of the collection.
     *
     * @return  The cJSON object representing the meta data for the collection.
     */
    cJSON *GetCollectionMeta() override;

    /**
     * Get the count of pending or active items contained in the collection.
     *
     * @return The count of pending or active items.
     */
    void GetPendingOrActive(uint32_t &aPending) const;

    /**
     * Add a item to the collection.
     *
     * @param[in] aItem  The item to add.
     */
    void AddItem(std::unique_ptr<actions::BasicActions> aItem);

    /**
     * Get a item from the collection.
     *
     * @param[in] aItemId  The itemId of the item to get.
     *
     * @return The item.
     */
    actions::BasicActions *GetItem(std::string aItemId);
};

// the collection
extern ActionsCollection gActionsCollection;

/**
 * @brief This class implements a json:api collection for holding action items with action-specific extensions.
 *
 */
class ActionsList : public ActionsCollection
{
public:
    /**
     * Constructor for a Actions List.
     *
     * @param[in] aServices  The shared services object.
     */
    ActionsList(Services &aServices);

    /**
     * Destructor for a Actions List.
     *
     */
    ~ActionsList();

    /**
     * The remaining capacity of the collection.
     */
    size_t FreeCapacity(void) const;

    /**
     * Validate a json object.
     *
     * Returns true if the json object is valid, false otherwise.
     */
    bool ValidateRequest(const cJSON *aJson);

    /**
     * Create an action from a json object.
     *
     * Returns OT_ERROR_NONE on success, or an error code on failure.
     * aUuid is the UUID of the action that was created.
     */
    otError CreateAction(const cJSON *aJson, std::string &aUuid);

    /**
     * Update an action.
     *
     * Returns OT_ERROR_NONE on success, or an error code on failure.
     */
    otError UpdateAction(const std::string aUuid);
    otError UpdateAction(const UUID &aUUID);

    /**
     * Update all actions.
     */
    void UpdateAllActions(void);

    /**
     * Get a json object representing an action.
     */
    cJSON *JsonifyAction(const std::string aUuid) const;
    cJSON *JsonifyAction(const UUID &aUUID) const;

    /**
     * Get a json object representing all actions.
     *
     * @param[out] aArray is the cJSON onbject representing the actions.
     * @param[out] aPending is the count of pending or active actions.
     */
    void JsonifyAllActions(cJSON *aArray, uint32_t &aPending) const;

    /**
     * Stop an action.
     *
     * Returns OT_ERROR_NONE on success, or an error code on failure.
     */
    otError StopAction(const UUID &aUUID);

    /**
     * Stop all actions.
     */
    otError DeleteAction(const UUID &aUUID);

    /**
     * Clear all actions.
     */
    void DeleteAllActions(void);

private:
    size_t mMaxActions;

    Services &mServices;
};

} // namespace rest
} // namespace otbr

#endif // EXTENSIONS_ACTIONS_LIST_HPP_
