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
 * @brief   Implements the APIs that validate, process, evaluate, jsonify and clean the task.
 *
 */
#include "rest_task_add_thread_device.hpp"
#include "rest/extensions/commissioner_allow_list.hpp"

#ifdef __cplusplus
extern "C" {
#endif

#include <cJSON.h>
#include <errno.h>
#include <time.h>
#include <openthread/commissioner.h>
#include <openthread/logging.h>
#include <openthread/platform/radio.h>

// Accommodating naming convention without refactoring whole codebase
static const char *const ATTRIBUTE_PSKD = "pskd";

const char *taskNameAddThreadDevice = "addThreadDeviceTask";

cJSON *jsonify_add_thread_device_task(task_node_t *task_node)
{
    otExtAddress eui64 = {0};

    cJSON *task_json  = task_node_to_json(task_node);
    cJSON *attributes = cJSON_GetObjectItemCaseSensitive(task_json, "attributes");
    //    cJSON_DeleteItemFromObject(attributes, ATTRIBUTE_PSKD);
    cJSON *eui = cJSON_GetObjectItemCaseSensitive(attributes, "eui");

    if ((task_node->status > ACTIONS_TASK_STATUS_PENDING) && (task_node->status != ACTIONS_TASK_STATUS_UNIMPLEMENTED))
    {
        // find allowListEntry and get more detailed status
        str_to_m8(eui64.m8, eui->valuestring, OT_EXT_ADDRESS_SIZE);

        if (entryEui64Find(&eui64) != nullptr)
        {
            cJSON_ReplaceItemInObjectCaseSensitive(attributes, "status",
                                                   cJSON_CreateString(entryEui64Find(&eui64)->getStateStr().c_str()));
        }
        else
        {
            otbrLogWarning("%s:%d - %s - eui not in allowlist: %s", __FILE__, __LINE__, __func__,
                           cJSON_Print(attributes));
        }
    }
    return task_json;
}

uint8_t validate_add_thread_device_task(cJSON *attributes)
{
    otError      error = OT_ERROR_NONE;
    otExtAddress eui64 = {0};

    cJSON *timeout = cJSON_GetObjectItemCaseSensitive(attributes, "timeout");
    cJSON *eui     = cJSON_GetObjectItemCaseSensitive(attributes, "eui");
    cJSON *pskd    = cJSON_GetObjectItemCaseSensitive(attributes, ATTRIBUTE_PSKD);

    VerifyOrExit((NULL != timeout && cJSON_IsNumber(timeout)), error = OT_ERROR_FAILED);

    VerifyOrExit(
        (NULL != eui && cJSON_IsString(eui) && 16 == strlen(eui->valuestring) && is_hex_string(eui->valuestring)),
        error = OT_ERROR_FAILED);
    // check eui is convertable
    SuccessOrExit(error = str_to_m8(eui64.m8, eui->valuestring, OT_EXT_ADDRESS_SIZE));

    VerifyOrExit((NULL != pskd && cJSON_IsString(pskd) && (WPANSTATUS_OK == joiner_verify_pskd(pskd->valuestring))),
                 error = OT_ERROR_FAILED);

exit:
    if (error != OT_ERROR_NONE)
    {
        // otLogWarnPlat("%s:%d %s %s missing or bad value in field\n%s", __FILE__, __LINE__, taskNameAddThreadDevice,
        // __func__, cJSON_Print(attributes));
        otbrLogWarning("%s:%d - %s - missing or bad value in a field: %s", __FILE__, __LINE__, __func__,
                       cJSON_Print(attributes));
        return ACTIONS_TASK_INVALID;
    }
    return ACTIONS_TASK_VALID;
}

otError addJoiner(task_node_t *task_node, otInstance *aInstance)
{
    otError      error = OT_ERROR_NONE;
    otExtAddress eui64 = {0};
    task_node_t *old_task;

    cJSON *task       = task_node->task;
    cJSON *attributes = cJSON_GetObjectItemCaseSensitive(task, "attributes");
    cJSON *eui        = cJSON_GetObjectItemCaseSensitive(attributes, "eui");
    cJSON *pskd       = cJSON_GetObjectItemCaseSensitive(attributes, ATTRIBUTE_PSKD);
    cJSON *timeout    = cJSON_GetObjectItemCaseSensitive(attributes, "timeout");

    str_to_m8(eui64.m8, eui->valuestring, OT_EXT_ADDRESS_SIZE);

    if ((entryEui64Find(&eui64) != NULL) &&
        (entryEui64Find(&eui64)->mstate < AllowListEntry::kAllowListEntryJoinFailed))
    {
        // cancel active task for same joiner, by convention has same uuid as allowListEntry
        old_task = task_node_find_by_id((entryEui64Find(&eui64)->muuid));
        task_update_status(old_task, ACTIONS_TASK_STATUS_STOPPED);
    }
    SuccessOrExit(error = allowListCommissionerJoinerAdd(eui64, (uint32_t)timeout->valueint, pskd->valuestring,
                                                         aInstance, task_node->id));

exit:
    if (error != OT_ERROR_NONE)
    {
        otbrLogWarning("%s:%d - %s - error: %s", __FILE__, __LINE__, __func__, otThreadErrorToString(error));
    }
    return error;
}

rest_actions_task_result_t process_add_thread_device_task(task_node_t      *task_node,
                                                          otInstance       *aInstance,
                                                          task_doneCallback aCallback)
{
    otError                    error             = OT_ERROR_NONE;
    otCommissionerState        commissionerState = OT_COMMISSIONER_STATE_DISABLED;
    rest_actions_task_result_t ret               = ACTIONS_RESULT_SUCCESS;

    OT_UNUSED_VARIABLE(aCallback);

    // Arg check before doing works
    VerifyOrExit((NULL != task_node && NULL != task_node->task), error = OT_ERROR_INVALID_ARGS);

    // openthread_lock_acquire(LOCK_TYPE_BLOCKING, 0);
    commissionerState = otCommissionerGetState(aInstance);
    // openthread_lock_release();

    // If the commissioner is already ACTIVE, we can add ot-joiners right away
    if (OT_COMMISSIONER_STATE_ACTIVE == commissionerState)
    {
        SuccessOrExit(error = addJoiner(task_node, aInstance));
    }
    else
    {
        // ot-commissioner is not ACTIVE yet, so we need to
        // wait for the ot-commissioner to become ACTIVE
        // and be called again from installed state_change callback
        SuccessOrExit(error = allowListCommissionerStart(aInstance));
        ret = ACTIONS_RESULT_RETRY;
    }

exit:
    if (error != OT_ERROR_NONE)
    {
        if (error == OT_ERROR_FAILED)
        {
            // otLogCritPlat("Cannot create restTaskJoinerAddConditionalTask");
            otbrLogCrit("%s:%d - %s - error %d - Cannot add Joiner.", __FILE__, __LINE__, __func__, error);
            ret = ACTIONS_RESULT_FAILURE;
        }
        else if (error == OT_ERROR_INVALID_STATE || error == OT_ERROR_ALREADY)
        {
            // otLogWarnPlat("Failed to start the commissioner, error %d", error);
            otbrLogWarning("%s:%d - %s - error %d - Failed to start the commissioner.", __FILE__, __LINE__, __func__,
                           error);
            ret = ACTIONS_RESULT_RETRY;
        }
        else
        {
            otbrLogWarning("%s: error %d", __func__, error);
            ret = ACTIONS_RESULT_FAILURE;
        }
    }
    return ret;
}

rest_actions_task_result_t evaluate_add_thread_device_task(task_node_t *task_node)
{
    otError             error = OT_ERROR_NONE;
    otExtAddress        eui64 = {0};
    const otExtAddress *addrPtr;

    cJSON *task       = task_node->task;
    cJSON *attributes = cJSON_GetObjectItemCaseSensitive(task, "attributes");
    cJSON *eui        = cJSON_GetObjectItemCaseSensitive(attributes, "eui");

    str_to_m8(eui64.m8, eui->valuestring, OT_EXT_ADDRESS_SIZE);

    addrPtr = &eui64;
    SuccessOrExit(error = allowListEntryJoinStatusGet(addrPtr));

exit:
    if (OT_ERROR_FAILED == error)
    {
        return ACTIONS_RESULT_FAILURE;
    }
    if (OT_ERROR_NONE == error)
    {
        return ACTIONS_RESULT_SUCCESS; // caller will mark it as complete in our task_node.
    }

    // Don't need to check for OT_ERROR_PENDING as the task is currently pending anyway
    return ACTIONS_RESULT_PENDING;
}

rest_actions_task_result_t clean_add_thread_device_task(task_node_t *task_node, otInstance *aInstance)
{
    cJSON *task       = task_node->task;
    cJSON *attributes = cJSON_GetObjectItemCaseSensitive(task, "attributes");
    cJSON *eui        = cJSON_GetObjectItemCaseSensitive(attributes, "eui");

    otError error = OT_ERROR_NONE;

    otExtAddress eui64 = {0};
    str_to_m8(eui64.m8, eui->valuestring, OT_EXT_ADDRESS_SIZE);

    SuccessOrExit(error = allowListCommissionerJoinerRemove(eui64, aInstance));
    SuccessOrExit(error = allowListEntryErase(eui64));

exit:
    if (OT_ERROR_NONE == error)
    {
        return ACTIONS_RESULT_SUCCESS;
    }

    otbrLogWarning("%s: error %d", __func__, error);
    return ACTIONS_RESULT_FAILURE;
}

#ifdef __cplusplus
}
#endif
