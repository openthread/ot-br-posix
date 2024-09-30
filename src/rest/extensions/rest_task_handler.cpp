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
 * @brief   Implements additional functionailty like `task_node` creation,
 *          update task status and conversion of `task_node` to `JSON` format.
 *
 */
#include "rest_task_handler.hpp"
#include "rest_task_queue.hpp"
#include "uuid.hpp"

#ifdef __cplusplus
extern "C" {
#endif

#include <cJSON.h>
#include <stdlib.h>
#include <time.h>

static CJSON_PUBLIC(cJSON_bool)
    cJSON_AddOrReplaceItemInObjectCaseSensitive(cJSON *object, const char *string, cJSON *newitem)
{
    if (cJSON_HasObjectItem(object, string))
    {
        return cJSON_ReplaceItemInObjectCaseSensitive(object, string, newitem);
    }
    else
    {
        return cJSON_AddItemToObject(object, string, newitem);
    }
}

task_node_t *task_node_new(cJSON *task)
{
    task_node_t *task_node = (task_node_t *)calloc(1, sizeof(task_node_t)); // TODO: free memory
    assert(NULL != task_node);

    UUID uuid = UUID();

    // Duplicate the client data associated with this task
    task_node->task = cJSON_Duplicate(task, cJSON_True);

    // Initialize the data for this new task to known defaults
    //
    task_node->prev       = NULL;  // Task queue management will update this
    task_node->next       = NULL;  // Task queue management will update this
    task_node->deleteTask = false; // New tasks are not marked for deletion

    // Populate UUID
    uuid.generateRandom();
    uuid.getUuid(task_node->id);
    snprintf(task_node->id_str, sizeof(task_node->id_str), uuid.toString().c_str());
    otbrLogWarning("creating new task with id %s", task_node->id_str);
    cJSON_AddStringToObject(task_node->task, "id", task_node->id_str);

    // Populated task type by name matching
    cJSON *task_type = cJSON_GetObjectItemCaseSensitive(task_node->task, "type");
    task_type_id_from_name(task_type->valuestring, &task_node->type);

    // Populate task creation time
    int timestamp      = (int)time(NULL);
    task_node->created = timestamp;

    // Setup task timeout if provided
    cJSON *attributes = cJSON_GetObjectItemCaseSensitive(task_node->task, "attributes");
    cJSON *timeout    = cJSON_GetObjectItemCaseSensitive(attributes, "timeout");

    if (cJSON_IsNumber(timeout))
    {
        // Set Up Timeout
        task_node->timeout = timestamp + (int)(timeout->valueint);
    }
    else
    {
        task_node->timeout = ACTIONS_TASK_NO_TIMEOUT;
    }

    // @note: While we can call task_update_status() here, we maybe locking
    // using taskNodeLockAcquire needlessly just to initialize the status
    // of a new task to known defaults (i.e. pending)
    //
    // Setup task status to pending (both the enum and the string version)
    task_update_status(task_node, ACTIONS_TASK_STATUS_PENDING);
    if (NULL != attributes)
    {
        (void)cJSON_AddItemToObject(attributes, "status",
                                    cJSON_CreateString(rest_actions_task_status_s[ACTIONS_TASK_STATUS_PENDING]));
    }
    // Return the prepared task node
    return task_node;
}

void task_update_status(task_node_t *aTaskNode, rest_actions_task_status_t status)
{
    assert(NULL != aTaskNode);

    // taskNodeLockAcquire(LOCK_TYPE_BLOCKING, 0);
    aTaskNode->status = status;
    // taskNodeLockRelease();
}

bool can_remove_task(task_node_t *aTaskNode)
{
    assert(NULL != aTaskNode);

    return (ACTIONS_TASK_STATUS_COMPLETED == aTaskNode->status || ACTIONS_TASK_STATUS_STOPPED == aTaskNode->status ||
            ACTIONS_TASK_STATUS_FAILED == aTaskNode->status);
}

cJSON *task_node_to_json(task_node_t *task_node)
{
    if (NULL == task_node)
    {
        return NULL;
    }
    cJSON *task_json  = cJSON_Duplicate(task_node->task, cJSON_True);
    cJSON *task_attrs = cJSON_GetObjectItemCaseSensitive(task_json, "attributes");
    cJSON_AddOrReplaceItemInObjectCaseSensitive(task_attrs, "status",
                                                cJSON_CreateString(rest_actions_task_status_s[task_node->status]));
    // add relationship
    // relationships:{
    //   result:{
    //     data: {type: diagnostics, id: diagnosticsId}
    //   }
    // }
    if ((task_node->status == ACTIONS_TASK_STATUS_COMPLETED) && (strlen(task_node->relationship.mType) > 0))
    {
        cJSON *relation    = cJSON_CreateObject();
        cJSON *result      = cJSON_CreateObject();
        cJSON *result_data = cJSON_CreateObject();

        cJSON_AddStringToObject(result_data, "type", task_node->relationship.mType);
        cJSON_AddStringToObject(result_data, "id", task_node->relationship.mId);
        cJSON_AddItemToObject(result, "data", result_data);
        cJSON_AddItemToObject(relation, "result", result);
        cJSON_AddItemToObject(task_json, "relationships", relation);
    }
    return task_json;
}

#ifdef __cplusplus
}
#endif
