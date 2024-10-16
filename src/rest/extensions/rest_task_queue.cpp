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
 * @brief   Implements APIs related to thread creation and task handling.
 *
 */
#include "rest_task_queue.hpp"
#include "rest_task_add_thread_device.hpp"

#ifdef __cplusplus
extern "C" {
#endif

#include <assert.h>
#include <cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <openthread/logging.h>

#define EVALUATE_INTERVAL 10

task_node_t *task_queue     = NULL;
uint8_t      task_queue_len = 0;
otInstance  *mInstance;

typedef struct
{
    rest_actions_task_t type_id;
    const char        **type_name;
    task_jsonifier      jsonify;
    task_validator      validate;
    task_processor      process;
    task_evaluator      evaluate;
    task_cleaner        clean;
} task_handlers_t;

static task_handlers_t *task_handler_by_task_type_id(rest_actions_task_t type_id);

/**
 * This list contains the handlers for each type of task, it must define the
 * tasks in the same order as the defined id from `rest_actions_task_t`. It must
 * also define all of the tasks (though ACTIONS_TASKS_SIZE must not have an
 * associated task as it's a counter).
 *
 * If these contraints are not met, it will assert during startup.
 */
static task_handlers_t handlers[] = {

    {
        .type_id   = ADD_THREAD_DEVICE_TASK,
        .type_name = &taskNameAddThreadDevice,
        .jsonify   = jsonify_add_thread_device_task,
        .validate  = validate_add_thread_device_task,
        .process   = process_add_thread_device_task,
        .evaluate  = evaluate_add_thread_device_task,
        .clean     = clean_add_thread_device_task,
    },
};

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

/**
 * @brief Finds a task_handlers_t struct for a specific type id if it exists.
 *
 * @return a task_handlers_t pointer for the specified type id, or NULL if one
 *  could not be found.
 */
static task_handlers_t *task_handler_by_task_type_id(rest_actions_task_t type_id)
{
    if (type_id < ACTIONS_TASKS_SIZE)
    {
        return &handlers[type_id];
    }
    return NULL;
}

cJSON *task_to_json(task_node_t *aTaskNode)
{
    if (NULL == aTaskNode || NULL == aTaskNode->task)
    {
        return NULL;
    }
    task_handlers_t *handlers = task_handler_by_task_type_id(aTaskNode->type);
    if (NULL == handlers || NULL == handlers->jsonify)
    {
        return NULL;
    }
    return handlers->jsonify(aTaskNode);
}

task_node_t *task_node_find_by_id(uuid_t uuid)
{
    task_node_t *head = task_queue;
    while (NULL != head)
    {
        if (uuid_equals(uuid, head->id))
        {
            return head;
        }
        head = head->next;
    }
    return NULL;
}

uint8_t can_remove_task_max()
{
    uint8_t      can_remove = 0;
    task_node_t *head       = task_queue;
    while (NULL != head)
    {
        if (can_remove_task(head))
        {
            can_remove++;
        }
        head = head->next;
    }
    return can_remove;
}

static bool remove_oldest_non_running_task()
{
    int             timestamp = (int)time(NULL);
    struct timespec ts;
    ts.tv_sec  = 0;         // Seconds
    ts.tv_nsec = 10000000L; // Nanoseconds (10 millisecond)
    task_node_t *head;
    head                          = task_queue;
    task_node_t *task_node_delete = NULL;

    while (NULL != head)
    {
        // Find the oldest task by finding the smallest timestamp
        if (timestamp > head->created && can_remove_task(head))
        {
            timestamp        = head->created;
            task_node_delete = head;
        }
        head = head->next;
    }

    if (NULL != task_node_delete)
    {
        // we don't call task_update_status as the task should delete shortly
        // after this
        task_node_delete->status     = ACTIONS_TASK_STATUS_STOPPED;
        task_node_delete->deleteTask = true;
        nanosleep(&ts, NULL); // 10 millisecond delay
        return true;
    }

    return false;
}

void remove_all_task()
{
    task_node_t *head;
    head = task_queue;

    while (NULL != head)
    {
        head->deleteTask = true;
        head             = head->next;
    }
}

uint8_t validate_task(cJSON *task)
{
    if (NULL == task)
    {
        return ACTIONS_TASK_INVALID;
    }
    otbrLogWarning("Validating task: %s", cJSON_PrintUnformatted(task));

    cJSON *task_type = cJSON_GetObjectItemCaseSensitive(task, "type");
    if (NULL == task_type || !cJSON_IsString(task_type))
    {
        otbrLogWarning("%s:%d task missing type field", __FILE__, __LINE__);
        return ACTIONS_TASK_INVALID;
    }
    cJSON *attributes = cJSON_GetObjectItemCaseSensitive(task, "attributes");
    if (NULL == attributes || !cJSON_IsObject(attributes))
    {
        otbrLogWarning("%s:%d task missing attributes field", __FILE__, __LINE__);
        return ACTIONS_TASK_INVALID;
    }

    rest_actions_task_t task_type_id = ACTIONS_TASKS_SIZE;
    if (task_type_id_from_name(task_type->valuestring, &task_type_id))
    {
        if (task_type_id >= ACTIONS_TASKS_SIZE || NULL == handlers[task_type_id].validate)
        {
            otbrLogWarning("Could not find a validate handler for %d", task_type_id);
            return ACTIONS_TASK_INVALID;
        }
        // validate task specific attributes
        return handlers[task_type_id].validate(attributes);
    }

    return ACTIONS_TASK_INVALID;
}

bool queue_task(cJSON *task, uuid_t *task_id)
{
    otbrLogWarning("Queueing task: %s", cJSON_PrintUnformatted(task));
    if (TASK_QUEUE_MAX <= task_queue_len)
    {
        if (!remove_oldest_non_running_task())
        {
            // Note: This case should not be possible as we already check to see if we exceed queue max before getting
            // to this queue fcn
            // otLogWarnPlat(
            //    "Maximum number of tasks hit, and no completed task available for removal, not queueing task");
            otbrLogWarning("%s:%d - %s - %s", __FILE__, __LINE__, __func__,
                           "Maximum number of tasks hit, not queueing task.");
            return false;
        }
    }
    // Generate the task object, and copy the ID to the output
    task_node_t *task_node = task_node_new(task);
    memcpy(task_id, &(task_node->id), sizeof(uuid_t));

    if (NULL == task_queue)
    {
        task_queue     = task_node;
        task_queue_len = 1;
    }
    else
    {
        task_node_t *head = task_queue;
        while (NULL != head->next)
        {
            head = head->next;
        }
        head->next      = task_node;
        task_node->prev = head;
        task_queue_len++;
    }
    return true;
}

void process_task(task_node_t *task_node, otInstance *aInstance, task_doneCallback aDoneCallback)
{
    task_handlers_t           *handlers;
    rest_actions_task_result_t processed;

    VerifyOrExit(NULL != task_node);
    VerifyOrExit(ACTIONS_TASK_STATUS_PENDING == task_node->status);

    handlers = task_handler_by_task_type_id(task_node->type);

    VerifyOrExit((NULL != handlers) && (NULL != handlers->process));

    processed = handlers->process(task_node, aInstance, aDoneCallback);

    switch (processed)
    {
    case ACTIONS_RESULT_FAILURE:
        task_update_status(task_node, ACTIONS_TASK_STATUS_FAILED);
        break;
    case ACTIONS_RESULT_RETRY:
        // fall through
    case ACTIONS_RESULT_NO_CHANGE_REQUIRED:
        break;
    case ACTIONS_RESULT_PENDING:
        // fall through
    case ACTIONS_RESULT_SUCCESS:
        task_update_status(task_node, ACTIONS_TASK_STATUS_ACTIVE);
        break;
    case ACTIONS_RESULT_STOPPED:
        task_update_status(task_node, ACTIONS_TASK_STATUS_STOPPED);
        break;
    }

exit:
    // otLogWarnPlat("nullptr error");
    return;
}

void evaluate_task(task_node_t *task_node)
{
    task_handlers_t           *handlers;
    rest_actions_task_result_t result;

    VerifyOrExit(NULL != task_node);

    VerifyOrExit(ACTIONS_TASK_STATUS_ACTIVE == task_node->status);

    handlers = task_handler_by_task_type_id(task_node->type);
    VerifyOrExit((NULL != handlers) && (NULL != handlers->process));

    result = handlers->evaluate(task_node);

    switch (result)
    {
    case ACTIONS_RESULT_FAILURE:
        task_update_status(task_node, ACTIONS_TASK_STATUS_FAILED);
        break;
    case ACTIONS_RESULT_SUCCESS:
        task_update_status(task_node, ACTIONS_TASK_STATUS_COMPLETED);
        break;
    case ACTIONS_RESULT_STOPPED:
        task_update_status(task_node, ACTIONS_TASK_STATUS_STOPPED);
        break;
    default:
        // do nothing, wait for next evaluation
        break;
    }

    task_node->last_evaluated = (int)time(NULL);

exit:
    return;
}

cJSON *jsonCreateTaskMetaCollection(uint32_t aOffset, uint32_t aLimit, uint32_t aTotal)
{
    cJSON *meta            = cJSON_CreateObject();
    cJSON *meta_collection = cJSON_CreateObject();
    // Abort if we are unable to create the necessary JSON objects
    if (NULL == meta || NULL == meta_collection)
    {
        return NULL;
    }

    (void)cJSON_AddNumberToObject(meta_collection, "offset", aOffset);
    if (aLimit > 0)
    {
        (void)cJSON_AddNumberToObject(meta_collection, "limit", aLimit);
    }
    (void)cJSON_AddNumberToObject(meta_collection, "total", aTotal);
    // add the count of unfinished actions
    cJSON_AddItemToObject(meta_collection, "pending", cJSON_CreateNumber(task_queue_len - can_remove_task_max()));
    (void)cJSON_AddItemToObject(meta, "collection", meta_collection);
    return meta;
}

/**
 * @brief The main function that iterates through the task_queue and process each task
 *        High level processing steps:
 *
 *        1. Delete any tasks that are marked for deletion
 *        2. Process any PENDING or ACTIVE tasks
 *           3.1 If task is timed out, the task is marked STOPPED (and deleted)
 *           3.2 If task is PENDING, call its process() function to make it ACTIVE
 *           3.3 If task is ACTIVE, call its evaluate() function to see if it is PENDING|SUCCESS|FAILED
 *
 */
void rest_task_queue_handle(void)
{
    task_node_t *head = task_queue;
    /*
        struct timespec ts;
        ts.tv_sec  = 0;        // Seconds
        ts.tv_nsec = 1000000L; // Nanoseconds (1 millisecond)
    */

    while (1)
    {
        if (NULL == head)
        {
            // Hit end of queue
            break;
        }

        // Is this task marked for deletion?
        if (head->deleteTask)
        {
            task_handlers_t *handlers = task_handler_by_task_type_id(head->type);
            if (NULL == handlers || NULL == handlers->clean)
            {
                otbrLogWarning("Could not find a clean handler for %d, assuming no clean needed", head->type);
            }
            else
            {
                // calls the clean function defined in handlers[]
                handlers->clean(head, mInstance);
            }

            if (ACTIONS_TASK_STATUS_STOPPED != head->status)
            {
                // we don't call task_update_status as we're going to be
                // deleting this a few lines below.
                head->status = ACTIONS_TASK_STATUS_STOPPED;
            }

            task_node_t *next = head->next;
            if (NULL == head->prev)
            {
                // If prev is empty, then we are the start of the list
                task_queue = next;
                if (NULL != next)
                {
                    next->prev = NULL;
                }
            }
            else
            {
                head->prev->next = next;
                if (NULL != next)
                {
                    next->prev = head->prev;
                }
            }
            {
                // Delete the cJSON task as well as the task_node
                otbrLogInfo("Deleting task id %s", head->id_str);
                cJSON_Delete(head->task);
                head->task = NULL;
                free(head);
                if (task_queue_len > 0)
                {
                    task_queue_len--;
                }
            }

            head = next;
            continue;
        }

        // Is this task PENDING or ACTIVE?
        if (ACTIONS_TASK_STATUS_PENDING == head->status || ACTIONS_TASK_STATUS_ACTIVE == head->status)
        {
            // Check if task has timed out if so we need to clean it and  mark it as stopped
            // We do not delete the task because the GET handler want to keep tabs on what is happening to the tasks.
            int current_time = (int)time(NULL);
            if (head->timeout >= 0 && head->timeout < current_time)
            {
                /* Mark tasks that have timed-out without failing/being completed as "Stopped" and stop evaluating */
                otbrLogWarning("%s:%d - %s - task timed out %s.", __FILE__, __LINE__, __func__,
                               cJSON_PrintUnformatted(head->task));
                task_handlers_t *handlers = task_handler_by_task_type_id(head->type);
                if (NULL == handlers || NULL == handlers->clean)
                {
                    otbrLogWarning("Could not find a clean handler for %d, assuming no clean needed", head->type);
                }
                else
                {
                    handlers->clean(head, mInstance);
                }

                task_update_status(head, ACTIONS_TASK_STATUS_STOPPED);
            }
            // If task has not timed out, carry on with its processing
            else
            {
                // If ACTIONS_TASK_STATUS_PENDING, run its process() function to see if we can make it active
                if (ACTIONS_TASK_STATUS_PENDING == head->status)
                {
                    process_task(head, mInstance, rest_task_queue_handle);
                }
                // Else If ACTIONS_TASK_STATUS_ACTIVE, run its evaluate, to see if it is completed/failed
                else if (ACTIONS_TASK_STATUS_ACTIVE == head->status)
                {
                    evaluate_task(head);
                }
            }
        }
        // Get ready to process the next task in the queue
        head = head->next;
    }
    // otbrLogWarning("EXITING rest_task_queue_task");
    // pthread_exit(NULL);

    // return NULL;
}

void rest_task_queue_task_init(otInstance *aInstance)
{
    mInstance = aInstance;

    // As noted above, the handler list needs to have an entry for each
    // task type defined in `rest_actions_task_t
    assert(ARRAY_SIZE(handlers) > 0);
    assert(ARRAY_SIZE(handlers) == ACTIONS_TASKS_SIZE);

    // To optimize during runtime, we want to ensure that the list is ordered
    // and contains each of the entries. This allows us to just index in via
    // the task type id rather than having to iterate.
    //
    // This check iterates over the list an ensures that each entry has a
    // type_id which is exactly 1 greater than the previous entry.
    rest_actions_task_t previous_id = handlers[0].type_id;
    for (size_t idx = 1; idx < ARRAY_SIZE(handlers); idx++)
    {
        assert(previous_id + 1 == handlers[idx].type_id);
        previous_id = handlers[idx].type_id;
    }
}

bool task_type_id_from_name(const char *task_name, rest_actions_task_t *type_id)
{
    if (NULL == task_name || NULL == type_id)
    {
        return false;
    }

    for (size_t idx = 0; idx < ACTIONS_TASKS_SIZE; idx++)
    {
        size_t name_length = strlen(*handlers[idx].type_name);
        if (0 == strncmp(task_name, *handlers[idx].type_name, name_length))
        {
            *type_id = handlers[idx].type_id;
            return true;
        }
    }
    return false;
}

#ifdef __cplusplus
}
#endif
