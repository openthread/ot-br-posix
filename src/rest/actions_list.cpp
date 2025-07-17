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

#include "actions_list.hpp"

#include <common/code_utils.hpp>

#include <algorithm>
#include <assert.h>
#include <cJSON.h>
#include <mutex>

#include "actions/action.hpp"
#include "actions/handler.hpp"
#include "rest/json.hpp"

namespace otbr {
namespace rest {

// ActionsCollection class implementation
ActionsCollection::ActionsCollection() = default;

ActionsCollection::~ActionsCollection()
{
}

cJSON *ActionsCollection::GetCollectionMeta()
{
    uint32_t pending = 0;
    GetPendingOrActive(pending);
    return Json::CreateMetaCollection(0, GetMaxCollectionSize(), mCollection.size(), pending);
}

void ActionsCollection::GetPendingOrActive(uint32_t &aPending) const
{
    otbr::rest::actions::BasicActions *action;
    aPending = 0;

    for (auto &it : mCollection)
    {
        action = static_cast<otbr::rest::actions::BasicActions *>(it.second.get());
        if (action->IsPendingOrActive())
        {
            aPending++;
        }
    }
}

void ActionsCollection::AddItem(std::unique_ptr<otbr::rest::actions::BasicActions> aItem)
{
    if (!aItem)
    {
        otbrLogWarning("%s:%d - %s - Received nullptr, item not added", __FILE__, __LINE__, __func__);
        return;
    }

    // do not exceed a max size of the collection
    while (mCollection.size() >= MAX_ACTIONS_COLLECTION_ITEMS)
    {
        EvictOldestItem();
    }

    // add to mHoldsTypes
    IncrHoldsTypes(aItem->GetTypeName());

    // add to age sorted list for easy erase, first in first erased
    if (std::find(mAgeSortedItemIds.begin(), mAgeSortedItemIds.end(), aItem->mUuid.ToString()) ==
        mAgeSortedItemIds.end())
    {
        mAgeSortedItemIds.push_back(aItem->mUuid.ToString());
    }

    otbrLogWarning("%s:%d - %s - %s", __FILE__, __LINE__, __func__, aItem->mUuid.ToString().c_str());
    auto result = mCollection.emplace(aItem->mUuid.ToString(), std::move(aItem));
    if (!result.second)
    {
        otbrLogWarning("%s:%d - %s - failed.", __FILE__, __LINE__, __func__);
    }
}

otbr::rest::actions::BasicActions *ActionsCollection::GetItem(std::string aItemId)
{
    auto it = mCollection.find(aItemId);
    if (it != mCollection.end())
    {
        return static_cast<otbr::rest::actions::BasicActions *>(it->second.get());
    }
    return nullptr;
}

// ActionsCollection gActionsCollection;

ActionsList::ActionsList(Services &aServices)
    : mMaxActions{200}
    , mServices{aServices}
{
    // reserve space for the maximum number of items in the collection
    // to avoid rehashing
    mCollection.reserve(mMaxActions);
}

ActionsList::~ActionsList()
{
    DeleteAllActions();
}

size_t ActionsList::FreeCapacity(void) const
{
    return mMaxActions > mCollection.size() ? mMaxActions - mCollection.size() : 0;
}

bool ActionsList::ValidateRequest(const cJSON *aJson)
{
    bool                    success = false;
    cJSON                  *type;
    cJSON                  *attributes;
    const actions::Handler *handler;

    std::string errormsg = "ok";

    VerifyOrExit(aJson != nullptr, errormsg = "invalid");

    type = cJSON_GetObjectItemCaseSensitive(aJson, "type");
    VerifyOrExit(type != nullptr, errormsg = "type missing");
    VerifyOrExit(cJSON_IsString(type), errormsg = "type not a string");

    attributes = cJSON_GetObjectItemCaseSensitive(aJson, "attributes");
    VerifyOrExit(attributes != nullptr, errormsg = "attributes missing");
    VerifyOrExit(cJSON_IsObject(attributes), errormsg = "attributes not an object");

    handler = actions::FindHandler(type->valuestring);
    VerifyOrExit(handler != nullptr, errormsg = "unknown type");
    VerifyOrExit(handler->Validate(*attributes), errormsg = "unexpected attributes");

    success = true;

exit:
    if (!success)
    {
        otbrLogWarning("%s:%d Error (%s)", __FILE__, __LINE__, errormsg.c_str());
    }
    return success;
}

otError ActionsList::CreateAction(const cJSON *aJson, std::string &aUuid)
{
    otError                                error = OT_ERROR_NONE;
    cJSON                                 *type;
    cJSON                                 *attributes;
    const actions::Handler                *handler;
    std::unique_ptr<actions::BasicActions> action;
    std::string                            actionId;

    VerifyOrExit(aJson != nullptr, error = OT_ERROR_INVALID_ARGS);

    type = cJSON_GetObjectItemCaseSensitive(aJson, "type");
    VerifyOrExit(type != nullptr, error = OT_ERROR_INVALID_ARGS);
    VerifyOrExit(cJSON_IsString(type), error = OT_ERROR_INVALID_ARGS);

    attributes = cJSON_GetObjectItemCaseSensitive(aJson, "attributes");
    VerifyOrExit(attributes != nullptr, error = OT_ERROR_INVALID_ARGS);
    VerifyOrExit(cJSON_IsObject(attributes), error = OT_ERROR_INVALID_ARGS);

    handler = actions::FindHandler(type->valuestring);
    VerifyOrExit(handler != nullptr, error = OT_ERROR_INVALID_ARGS);

    action = handler->Create(*attributes, mServices);
    aUuid  = action.get()->mUuid.ToString();
    this->AddItem(std::move(action));

    /*
    if (strcmp(type->valuestring, ADD_DEVICE_ACTION_TYPE_NAME) == 0)
    {
        std::unique_ptr<otbr::rest::actions::AddDeviceAction> res =
            std::unique_ptr<otbr::rest::actions::AddDeviceAction>(new otbr::rest::actions::AddDeviceAction());
        res->mEui = std::string(cJSON_GetObjectItemCaseSensitive(attributes, "eui")->valuestring);
        res->mPskd = std::string(cJSON_GetObjectItemCaseSensitive(attributes, "pskd")->valuestring);
        gActionsCollection.AddItem(std::move(res));
    }
    */

    error = UpdateAction(aUuid);

exit:
    return error;
}

otError ActionsList::UpdateAction(const std::string aUuid)
{
    otError                            error = OT_ERROR_NONE;
    auto                               it    = mCollection.find(aUuid);
    otbr::rest::actions::BasicActions *action;

    VerifyOrExit(it != mCollection.end(), error = OT_ERROR_NOT_FOUND);
    action = static_cast<otbr::rest::actions::BasicActions *>(it->second.get());
    VerifyOrExit(!action->IsPendingOrActive());

    action->Update();

    if (action->IsPendingOrActive() && action->IsBeyondTimeout())
    {
        action->Stop();
    }

exit:
    return error;
}

otError ActionsList::UpdateAction(const Uuid &aUuid)
{
    otError                            error = OT_ERROR_NONE;
    auto                               it    = mCollection.find(aUuid.ToString());
    otbr::rest::actions::BasicActions *action;

    VerifyOrExit(it != mCollection.end(), error = OT_ERROR_NOT_FOUND);
    action = static_cast<otbr::rest::actions::BasicActions *>(it->second.get());
    VerifyOrExit(!action->IsPendingOrActive());

    action->Update();

    if (action->IsPendingOrActive() && action->IsBeyondTimeout())
    {
        action->Stop();
    }

exit:
    return error;
}

void ActionsList::UpdateAllActions(void)
{
    otbr::rest::actions::BasicActions *action;

    for (auto &it : mCollection)
    {
        action = static_cast<otbr::rest::actions::BasicActions *>(it.second.get());
        if (!action->IsPendingOrActive())
        {
            continue;
        }

        action->Update();

        if (action->IsPendingOrActive() && action->IsBeyondTimeout())
        {
            action->Stop();
        }
    }
}

cJSON *ActionsList::JsonifyAction(const std::string aUuid) const
{
    cJSON                             *result = nullptr;
    otbr::rest::actions::BasicActions *action;

    auto it = mCollection.find(aUuid);

    VerifyOrExit(it != mCollection.end());

    action = static_cast<otbr::rest::actions::BasicActions *>(it->second.get());

    result = action->Jsonify({});

exit:
    return result;
}

otError ActionsList::StopAction(const Uuid &aUuid)
{
    otError                            error = OT_ERROR_NONE;
    auto                               it    = mCollection.find(aUuid.ToString());
    otbr::rest::actions::BasicActions *action;

    VerifyOrExit(it != mCollection.end(), error = OT_ERROR_NOT_FOUND);
    action = static_cast<otbr::rest::actions::BasicActions *>(it->second.get());
    VerifyOrExit(action->IsPendingOrActive());

    action->Update();
    if (action->IsPendingOrActive())
    {
        action->Stop();
    }

exit:
    return error;
}

otError ActionsList::DeleteAction(const Uuid &aUuid)
{
    otError error = OT_ERROR_NONE;
    auto    it    = mCollection.find(aUuid.ToString());

    VerifyOrExit(it != mCollection.end(), error = OT_ERROR_NOT_FOUND);
    mCollection.erase(it);

exit:
    return error;
}

void ActionsList::DeleteAllActions(void)
{
    DeleteAll();
}

} // namespace rest
} // namespace otbr
