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
#ifndef REST_TASK_QUEUE_HPP_
#define REST_TASK_QUEUE_HPP_

#include "rest_server_common.hpp"
#include "rest_task_handler.hpp"
#include "utils/thread_helper.hpp"

struct cJSON;

#ifdef __cplusplus
extern "C" {
#endif

#define TASK_QUEUE_MAX 100

typedef void (*task_doneCallback)(void);

/**
 * @brief Specifies the function signature that the task jsonifier function
 *        must adhere to.
 *
 * A task jsonifier is responsible for taking a `task_node_t` pointer
 * and creating a JSON representation which can be returned to a
 * client.
 *
 * @param task_node the node which is to be jsonified
 * @return a cJSON pointer representing the task node.
 */
typedef cJSON *(*task_jsonifier)(task_node_t *task_node);

/**
 * @brief Specifies the function signature that the task validator function
 *        must adhere to.
 *
 * A task validator is responsible for taking the input cJSON from a user and
 * ensuring all the various fields and structures meet the requirements set out
 * in the API schema.
 *
 * See the `validate_task` function below for more info.
 *
 * @param task the JSON structure to return
 * @return the value must be one of ACTIONS_TASK_VALID ACTIONS_TASK_INVALID, or
 *         ACTIONS_TASK_NOT_IMPLEMENTED.
 */
typedef uint8_t (*task_validator)(cJSON *task);

/**
 * @brief Specifies the function signature that the task processor function
 *        must adhere to.
 *
 * A task processor is responsible for starting the execution of a task. Once
 * the execution has started, the evaluation function is called regularly for
 * updates.
 *
 * @param task the task which is to be executed.
 * @param aInstance openthread instance
 * @param aDoneCallback is called when process has completed.
 * @return the status of the task, which should ACTIONS_RESULT_SUCCESS,
 *         ACTIONS_RESULT_FAILURE, ACTIONS_RESULT_RETRY, or
 *         ACTIONS_RESULT_PENDING.
 */
typedef rest_actions_task_result_t (*task_processor)(task_node_t      *task_node,
                                                     otInstance       *aInstance,
                                                     task_doneCallback aDoneCallback);

/**
 * @brief Specifies the function signature that the task processor function
 *        must adhere to.
 *
 * A task evaluator is responsible for continued execution and processing of
 * a task. This is responsible for monitoring the execution of a task and
 * reporting when the execution has finished (either successfully or in
 * failure).
 *
 * @param task the task which is being evaluated.
 * @return the status of the task, which should be ACTIONS_RESULT_SUCCESS,
 *         ACTIONS_RESULT_FAILURE, ACTIONS_RESULT_PENDING, or
 *         ACTIONS_RESULT_NO_CHANGE_REQUIRED.
 */
typedef rest_actions_task_result_t (*task_evaluator)(task_node_t *task_node);

/**
 * @brief Specifies the function signature that the task processor function
 *        must adhere to.
 *
 * A task cleaner is responsible for releasing any resources that the task
 * is holding so that it can be removed from the queue.
 *
 * @param task the task which is being cleaned
 * @param aInstance openthread instance
 * @return the status of the cleaning operation, which should be
 *         ACTIONS_RESULT_SUCCESS or ACTIONS_RESULT_FAILURE.
 */
typedef rest_actions_task_result_t (*task_cleaner)(task_node_t *task_node, otInstance *aInstance);

/**
 * @brief Validate the REST POST Action Task with the given JSON array
 *
 * @param task Pointer to cJSON task to be validated
 * @return ACTIONS_TASK_VALID if the task is valid,
 *         ACTIONS_TASK_INVALID if the task is invalid,
 *         ACTIONS_TASK_NOT_IMPLEMENTED if the task has not been implemented
 */
uint8_t validate_task(cJSON *task);

/**
 * @brief Generates the new task object (task_node) of type 'task_node_t'.
 *        Initializes task_queue with the newly created task object which will
 *        be proccessed on different thread.
 *
 * @param task A pointer to JSON array item.
 * @param uuid_t *task_id A reference to get the task_id
 * @return true     Task queued
 *  @return false    Not able to queue task
 */
bool         queue_task(cJSON *task, uuid_t *task_id);
cJSON       *task_to_json(task_node_t *task_node);
task_node_t *task_node_find_by_id(uuid_t uuid);

/**
 * @brief When called, I generate a CJSON object for the task metadata
 * as specified in the openapi.yaml
 *
 * sample output:
 *
 * meta:
 *    collection:
 *        offset: 0 // based on the args passed to this function
 *        limit:  4 // based on the args passed to this function
 *        total:  4 // determined by the total number of tasks in the queue
 *
 * @param aOffset the value to use for meta.collection.offset
 * @param aLimit  the value to use for meta.collection.limit
 * @param aTotal  the value to use for meta.collection.total
 *
 * @return cJSON* a populated meta.collection json object, NULL on error
 */
cJSON *jsonCreateTaskMetaCollection(uint32_t aOffset, uint32_t aLimit, uint32_t aTotal);

/**
 * @brief Number of tasks that have finished processing.
 *
 * @return uint8_t   Count of inactive tasks, that are 'completed', 'stopped' or 'failed'.
 */
uint8_t can_remove_task_max();

void remove_all_task();

void rest_task_queue_task_init(otInstance *aInstance);

/**
 * @brief Iterates through list of tasks for processing and evaluation
 *
 */
void rest_task_queue_handle(void);

void evaluate_task(task_node_t *task_node);
void process_task(task_node_t *task_node, otInstance *aInstance, task_doneCallback aDoneCallback);

/**
 * @brief Looks up the type id for a given task name and updates the `type_id`
 *        argument if found.
 *
 * @param task_name the task name to look up the id for
 * @param type_id [out] a pointer to place the result into
 * @return true if found, false otherwise.
 */
bool task_type_id_from_name(const char *task_name, rest_actions_task_t *type_id);

#ifdef __cplusplus
} // end of extern "C"
#endif

#endif
