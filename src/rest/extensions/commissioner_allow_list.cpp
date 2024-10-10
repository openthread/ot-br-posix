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
 * @brief Implements functionality related to commisioner APIs.
 *
 */
#include "commissioner_allow_list.hpp"
#include "rest/extensions/rest_task_add_thread_device.hpp"
#include "utils/hex.hpp"
#include "utils/thread_helper.hpp"

#ifdef __cplusplus
extern "C" {
#endif

#include "assert.h"
#include "cJSON.h"
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <openthread/commissioner.h>
#include <openthread/logging.h>
#include "openthread/instance.h"

#define ALLOW_LIST_NAME "allowlist"
#define ALLOW_LIST_MOUNT "/" ALLOW_LIST_NAME
#define ALLOW_LIST_BASE_DIR ALLOW_LIST_MOUNT "/"

#define COMMISSIONER_START_WAIT_TIME_MS 100
#define COMMISSIONER_START_MAX_ATTEMPTS 5

static allow_list::LinkedList<AllowListEntry> AllowListEntryList;
static void                                   consoleEntryPrint(AllowListEntry *aEntry);

bool otExtAddressMatch(const otExtAddress *aAddress1, const otExtAddress *aAddress2)
{
    for (int i = 0; i < OT_EXT_ADDRESS_SIZE; i++)
    {
        if (aAddress1->m8[i] != aAddress2->m8[i])
        {
            return false;
        }
    }
    return true;
}

bool eui64IsNull(const otExtAddress aEui64)
{
    for (int i = 0; i < OT_EXT_ADDRESS_SIZE; i++)
    {
        if (aEui64.m8[i] != 0)
        {
            return false;
        }
    }
    return true;
}

AllowListEntry *entryEui64Find(const otExtAddress *aEui64)
{
    AllowListEntry *entry = nullptr;

    if (nullptr == aEui64)
    {
        return entry;
    }
    entry = AllowListEntryList.GetHead();
    while (entry)
    {
        if (otExtAddressMatch(&entry->meui64, aEui64))
        {
            break;
        }
        entry = entry->GetNext();
    }
    return entry;
}

otError allowListCommissionerJoinerAdd(otExtAddress aEui64,
                                       uint32_t     aTimeout,
                                       char        *aPskd,
                                       otInstance  *aInstance,
                                       uuid_t       uuid)
{
    otError             error;
    AllowListEntry     *entry   = nullptr;
    const otExtAddress *addrPtr = &aEui64;

    if (eui64IsNull(aEui64))
    {
#ifdef OPENTHREAD_COMMISSIONER_ALLOW_ANY_JOINER
        return OT_ERROR_INVALID_ARGS;
#else
        addrPtr = nullptr;
#endif
    }

    allowListAddDevice(aEui64, aTimeout, aPskd, uuid);
    entry = entryEui64Find(addrPtr);

    error = otCommissionerAddJoiner(aInstance, addrPtr, aPskd, aTimeout);

    if (OT_ERROR_NONE == error && nullptr != entry)
    {
        entry->update_state(AllowListEntry::kAllowListEntryPendingJoiner);
    }

    if (OT_ERROR_NONE != error)
    {
        otbrLogWarning("otCommissionerAddJoiner error=%d %s", error, otThreadErrorToString(error));
    }
    return error;
}

otError allowListEntryErase(otExtAddress aEui64)
{
    otError         error = OT_ERROR_FAILED;
    AllowListEntry *entry = nullptr;

    const otExtAddress *addrPtr = &aEui64;

    entry = AllowListEntryList.GetHead();
    while (entry)
    {
        if (otExtAddressMatch(&entry->meui64, addrPtr))
        {
            error = AllowListEntryList.Remove(*entry);
            break;
        }
        entry = entry->GetNext();
    }
    return error;
}

otError allowListCommissionerJoinerRemove(otExtAddress aEui64, otInstance *aInstance)
{
    otError             error = OT_ERROR_FAILED;
    otCommissionerState state = OT_COMMISSIONER_STATE_DISABLED;

    const otExtAddress *addrPtr = &aEui64;

    if (eui64IsNull(aEui64))
    {
        addrPtr = nullptr;
    }

    state = otCommissionerGetState(aInstance);
    if (OT_COMMISSIONER_STATE_DISABLED == state)
    {
        return OT_ERROR_NONE;
    }

    error = otCommissionerRemoveJoiner(aInstance, addrPtr);

    if (OT_ERROR_NONE != error)
    {
        otLogWarnPlat("otCommissionerRemoveJoiner error=%d %s", error, otThreadErrorToString(error));
    }
    return error;
}

AllowListEntry *parse_buf_as_json(char *aBuf)
{
    // Need all vars to be declared here to use goto for graceful exit
    cJSON *allow_entry_json = nullptr;
    cJSON *attributesJSON   = nullptr;
    // cJSON                              *hasActivationKeyJSON = nullptr;
    AllowListEntry                     *pEntry = nullptr;
    otExtAddress                        eui64;
    uint32_t                            timeout = 0;
    AllowListEntry::AllowListEntryState state   = AllowListEntry::kAllowListEntryNew;
    UUID                                uuid_obj;
    uuid_t                              uuid;
    char                               *uuid_str     = nullptr;
    char                               *eui64_str    = nullptr;
    char                               *pskdValue    = nullptr;
    size_t                              pskdValueLen = 0;
    char                               *pskd         = nullptr;

    allow_entry_json = cJSON_Parse(aBuf);
    if (nullptr == allow_entry_json)
    {
        otbrLogErr("%s: Err cJSON_Parse", __func__);
        goto exit;
    }

    attributesJSON = cJSON_GetObjectItemCaseSensitive(allow_entry_json, JSON_ATTRIBUTES);
    if (nullptr == attributesJSON)
    {
        otbrLogErr("%s: Err cJSON Get %s", __func__, JSON_ATTRIBUTES);
        goto exit;
    }
    eui64_str = cJSON_GetObjectItem(attributesJSON, JSON_EUI)->valuestring;
    if (nullptr == eui64_str)
    {
        otbrLogErr("%s: Err cJSON Get eui64", __func__);
        goto exit;
    }

    otbr::Utils::Hex2Bytes(eui64_str, eui64.m8, sizeof(eui64));

    uuid_str = cJSON_GetObjectItem(allow_entry_json, JSON_UUID)->valuestring;
    if (nullptr == uuid_str)
    {
        otbrLogErr("%s: Err cJSON Get uuid", __func__);
        goto exit;
    }
    uuid_obj.parse(std::string(uuid_str));
    uuid_obj.getUuid(uuid);
    // uuid_parse(uuid_str, &uuid);

    pskdValue = cJSON_GetObjectItem(attributesJSON, JSON_PSKD)->valuestring;
    if (nullptr == pskdValue || strlen(pskdValue) > OT_JOINER_MAX_PSKD_LENGTH)
    {
        otbrLogErr("%s: Err cJSON Get pskd", __func__);
        goto exit;
    }

    pskdValueLen = strlen(pskdValue) + 1; // account for NULL
    pskd         = (char *)malloc(pskdValueLen);
    if (nullptr == pskd)
    {
        otbrLogErr("%s: Err no mem for pskd, need %d bytes", __func__, pskdValueLen);
        goto exit;
    }
    memset(pskd, 0, pskdValueLen);
    memcpy(pskd, pskdValue, strlen(pskdValue));

    timeout = cJSON_GetObjectItem(allow_entry_json, JSON_TIMEOUT)->valueint;
    state   = (AllowListEntry::AllowListEntryState)cJSON_GetObjectItem(allow_entry_json, JSON_ALLOW_STATE)->valueint;
    pEntry  = new AllowListEntry(eui64, uuid, timeout, state, pskd);

exit:
    if (nullptr != allow_entry_json)
    {
        cJSON_Delete(allow_entry_json);
    }
    if (nullptr == pEntry)
    {
        otbrLogErr("%s: Err creating a new AllowListEntry", __func__);
        if (nullptr != pskd)
        {
            free(pskd);
        }
    }
    return pEntry;
}

void allowListAddDevice(otExtAddress aEui64, uint32_t aTimeout, char *aPskd, uuid_t aUuid)
{
    assert(nullptr != aPskd);

    AllowListEntry *pEntry = entryEui64Find(&aEui64);

    int   pskd_len = strlen(aPskd);
    char *pskd_new = (char *)malloc(pskd_len + 1);
    assert(nullptr != pskd_new);
    memset(pskd_new, 0, pskd_len + 1);
    memcpy(pskd_new, aPskd, pskd_len);

    if (nullptr != pEntry)
    {
        pEntry->mPSKd    = pskd_new;
        pEntry->mTimeout = aTimeout;
        pEntry->muuid    = aUuid;
    }
    else
    {
        // TODO if (aUuid == NULL)
        //{
        //     //  this may not be needed
        //     uuid_generate_random(&aUuid);
        // }
        pEntry = new AllowListEntry(aEui64, aUuid, aTimeout, pskd_new);
        if (nullptr == pEntry)
        {
            //  otbrLogErr("%s: Err creating a new AllowListEntry", __func__);
            free(pskd_new);
            return;
        }
        AllowListEntryList.Add(*pEntry);
    }

    consoleEntryPrint(pEntry);
}

/**
 * @brief Given an entry, prints its content to the console
 *
 * @param aEntry the entry to print
 */
static void consoleEntryPrint(AllowListEntry *aEntry)
{
    assert(nullptr != aEntry);
    // char uuidStr[UUID_STR_LEN] = {0};

    UUID uuid_obj = UUID();
    uuid_obj.setUuid(aEntry->muuid);

    // uuid_unparse(aEntry->muuid, uuidStr);
    otbrLogInfo(
        "Entry uuid: %s\n\tEUI64: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n\tJoined: %s\n\tState: %d\n\tTimeout: %d",
        uuid_obj.toString().c_str(), aEntry->meui64.m8[0], aEntry->meui64.m8[1], aEntry->meui64.m8[2],
        aEntry->meui64.m8[3], aEntry->meui64.m8[4], aEntry->meui64.m8[5], aEntry->meui64.m8[6], aEntry->meui64.m8[7],
        aEntry->is_joined() ? "TRUE" : "FALSE", aEntry->mstate, aEntry->mTimeout);
}

void allow_list_print_all_entries_to_console(void)
{
    AllowListEntry *entry = AllowListEntryList.GetHead();
    while (entry)
    {
        consoleEntryPrint(entry);
        entry = entry->GetNext();
    }
}

int allowListJsonifyAll(cJSON *input_object)
{
    assert(nullptr != input_object);

    if (AllowListEntryList.IsEmpty())
    {
        return 0;
    }

    // Add each entry into an array called allow_list
    cJSON *json_array = cJSON_CreateArray();
    if (nullptr == json_array)
    {
        otbrLogErr("%s: Err: cJSON_CreateArray", __func__);
        return 0;
    }

    AllowListEntry *entry = nullptr;
    // cJSON* entryJson = nullptr;
    int entry_count = 0;
    for (entry = AllowListEntryList.GetHead(); nullptr != entry; entry = entry->GetNext())
    {
        // Typically allow list entry uses type as JSON_ALLOW_LIST_TYPE.
        // This call is one of the exception to reuse the Allow_list_entry_as_CJSON method
        // entryJson = entry->Allow_list_entry_as_CJSON(DEVICE_TYPE);
        // cJSON_AddItemToArray(json_array, entryJson);
        // entry_count++;
    }

    if (entry_count > 0)
    {
        cJSON_AddItemToObject(input_object, "allow_list", json_array);
    }
    else
    {
        otbrLogErr("%s: Err: cJSON Array is empty", __func__);
        // Something is wrong, delete our array to not leak memory
        cJSON_Delete(json_array);
        return 0;
    }

    // @note: Caller responsible to clean json_array, usually via cJSON_Delete(input_object);
    return entry_count;
}

void allowListEraseAll(void)
{
    if (!AllowListEntryList.IsEmpty())
    {
        AllowListEntry *entry = nullptr;
        // Free all malloc-ed pskd to not leak memory
        for (entry = AllowListEntryList.GetHead(); nullptr != entry; entry = entry->GetNext())
        {
            if (nullptr != entry->mPSKd)
            {
                free(entry->mPSKd);
            }
        }
        AllowListEntryList.Clear(); // Mark Linked List as empty
    }
}

void HandleStateChanged(otCommissionerState aState, void *aContext)
{
    OT_UNUSED_VARIABLE(aContext);
    otbrLogWarning("%s:%d - %s - commissioner state: %d", __FILE__, __LINE__, __func__, aState);

    switch (aState)
    {
    case OT_COMMISSIONER_STATE_ACTIVE:
        rest_task_queue_handle();
        break;
    case OT_COMMISSIONER_STATE_DISABLED:
        break;
    case OT_COMMISSIONER_STATE_PETITION:
        break;
    default:
        break;
    }
}

uint8_t allowListGetPendingJoinersCount(void)
{
    uint8_t pendingJoinersCount = 0;

    AllowListEntry *entry = AllowListEntryList.GetHead();

    while (entry)
    {
        if ((AllowListEntry::kAllowListEntryJoined != entry->mstate) ||
            (AllowListEntry::kAllowListEntryJoinFailed != entry->mstate))
        {
            pendingJoinersCount++;
        }
        entry = entry->GetNext();
    }

    return pendingJoinersCount;
}

void HandleJoinerEvent(otCommissionerJoinerEvent aEvent,
                       const otJoinerInfo       *aJoinerInfo,
                       const otExtAddress       *aJoinerId,
                       void                     *aContext)
{
    (void)aEvent;
    (void)aContext;
    OT_UNUSED_VARIABLE(aJoinerId);
    AllowListEntry *entry               = nullptr;
    uint8_t         pendingDevicesCount = 0;

    // @note: Thread may call this for joiners that we are not supposed to join
    //        do not assume `entry` is not null in the rest of the code.
    entry = entryEui64Find(&aJoinerInfo->mSharedId.mEui64);
    if (nullptr == entry && eui64IsNull(aJoinerInfo->mSharedId.mEui64))
    {
        otbrLogWarning("Unauthorized device %s join attempt", aJoinerInfo->mSharedId.mEui64);
        return;
    }

    switch (aEvent)
    {
    case OT_COMMISSIONER_JOINER_START:
        otbrLogWarning("Start Joiner");
        if (nullptr != entry)
        {
            entry->update_state(AllowListEntry::kAllowListEntryJoinAttempted);
            consoleEntryPrint(entry);
        }
        break;
    case OT_COMMISSIONER_JOINER_CONNECTED:
        otbrLogWarning("Connect Joiner");
        break;
    case OT_COMMISSIONER_JOINER_FINALIZE:
        otbrLogWarning("Finalize Joiner");
        if (nullptr != entry)
        {
            entry->update_state(AllowListEntry::kAllowListEntryJoined);
            consoleEntryPrint(entry);
        }
        break;
    case OT_COMMISSIONER_JOINER_END:
        otbrLogWarning("End Joiner");
        break;
    case OT_COMMISSIONER_JOINER_REMOVED:
        otbrLogWarning("Removed Joiner");

        if (nullptr != entry)
        {
            // If this get called on a one of our joiners that has never attempted yet, then we mark this as expired
            if (AllowListEntry::kAllowListEntryPendingJoiner == entry->mstate)
            {
                entry->update_state(AllowListEntry::kAllowListEntryExpired);
            }
            // If this get called on a one of our joiners that is not joined yet, then we need to mark this as failed
            else if (AllowListEntry::kAllowListEntryJoined != entry->mstate)
            {
                entry->update_state(AllowListEntry::kAllowListEntryJoinFailed);
            }
        }

        // Scan allow list see if there are still pending joiners to process
        pendingDevicesCount = allowListGetPendingJoinersCount();

        // If all entries have been attempted and nothing is pending, stop the commissioner
        if (0 == pendingDevicesCount)
        {
            allowListCommissionerStopPost();
        }
        else
        {
            // Tracer print
            otbrLogWarning("%u Pending Joiners", pendingDevicesCount);
        }

        break;
    }
}

otError allowListCommissionerStart(otInstance *aInstance)
{
    otError error = OT_ERROR_FAILED;

    error = otCommissionerStart(aInstance, &HandleStateChanged, &HandleJoinerEvent, NULL);

    return error;
}

otError allowListCommissionerStopPost(void)
{
    return OT_ERROR_NONE;
}

cJSON *AllowListEntry::Allow_list_entry_as_CJSON(const char *entryType)
{
    assert(nullptr != entryType);

    cJSON *entry_json_obj = nullptr;
    // cJSON *hasActivationKey    = nullptr;
    cJSON *attributes_json_obj = nullptr;
    char   eui64_str[17]       = {0};
    // char   uuid_str[UUID_STR_LEN] = {0};

    UUID uuid_obj = UUID();

    // hasActivationKey = cJSON_CreateObject();

    memset(eui64_str, 0, sizeof(eui64_str));
    sprintf(eui64_str, "%02x%02x%02x%02x%02x%02x%02x%02x", meui64.m8[0], meui64.m8[1], meui64.m8[2], meui64.m8[3],
            meui64.m8[4], meui64.m8[5], meui64.m8[6], meui64.m8[7]);

    // memset(uuid_str, 0, sizeof(uuid_str));
    // uuid_unparse(muuid, uuid_str);
    uuid_obj.setUuid(muuid);

    attributes_json_obj = cJSON_CreateObject();
    // cJSON_AddItemToObject(attributes_json_obj, "hasActivationKey", hasActivationKey);
    cJSON_AddItemToObject(attributes_json_obj, "eui", cJSON_CreateString(eui64_str));
    cJSON_AddItemToObject(attributes_json_obj, "pskd", cJSON_CreateString(mPSKd));

    entry_json_obj = cJSON_CreateObject();

    cJSON_AddItemToObject(entry_json_obj, JSON_UUID, cJSON_CreateString(uuid_obj.toString().c_str()));
    cJSON_AddItemToObject(entry_json_obj, JSON_TYPE, cJSON_CreateString(entryType));
    cJSON_AddItemToObject(entry_json_obj, JSON_ATTRIBUTES, attributes_json_obj);
    cJSON_AddNumberToObject(entry_json_obj, JSON_TIMEOUT, mTimeout);
    cJSON_AddNumberToObject(entry_json_obj, JSON_ALLOW_STATE, mstate);

    return entry_json_obj;
}

/**
 * @brief I (kludegy) map joiner status to otError code
 *
 * @param eui64 the Joiner eui64 to check
 * @return otError OT_ERROR_NONE     == eui64 joiner joined
 *                 OT_ERROR_FAILED   == eui64 joiner failed
 *                 OT_ERROR_PENDING  == eui64 joiner still being processed
 */
otError allowListEntryJoinStatusGet(const otExtAddress *eui64)
{
    AllowListEntry *entry = entryEui64Find(eui64);

    if ((NULL == entry) || (entry->is_failed()))
    {
        return OT_ERROR_FAILED;
    }

    if (true == entry->is_joined())
    {
        return OT_ERROR_NONE;
    }

    return OT_ERROR_PENDING;
}

#ifdef __cplusplus
}
#endif
