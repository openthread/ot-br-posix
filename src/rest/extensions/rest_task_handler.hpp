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
#ifndef REST_TASK_HANDLER_HPP_
#define REST_TASK_HANDLER_HPP_

#include "uuid.hpp"

struct cJSON;

#ifdef __cplusplus
extern "C" {
#endif

#include <assert.h>
#include <stdbool.h>

/**
 * @brief Other api/action types can be added with the following enum.
 *
 */
typedef enum
{
    // DISCOVER_THREAD_NETWORKS_TASK = 0,
    // FORM_THREAD_NETWORK_TASK,
    ADD_THREAD_DEVICE_TASK = 0,
    GET_THREAD_NETWORK_DIAGNOSTIC_TASK,
    RESET_THREAD_NETWORK_DIAGNOSTIC_COUNTERS_TASK,
    GET_THREAD_ENERGY_SCAN_TASK,
    // IDENTIFY_BORDER_ROUTER_TASK,
    // THREAD_BORDER_ROUTER_JOIN_TASK,
    // GENERATE_LDEVID_TASK,
    ACTIONS_TASKS_SIZE,
} rest_actions_task_t;

static const char *const gRestActionsTaskNames[] = {"addThreadDeviceTask", "getThreadNetworkDiagnosticTask",
                                                    "getThreadEnergyScanTask"};

typedef enum
{
    ACTIONS_TASK_STATUS_PENDING = 0,
    ACTIONS_TASK_STATUS_ACTIVE,
    ACTIONS_TASK_STATUS_COMPLETED,
    ACTIONS_TASK_STATUS_STOPPED,
    ACTIONS_TASK_STATUS_FAILED,
    ACTIONS_TASK_STATUS_UNIMPLEMENTED,
} rest_actions_task_status_t;

static const char *const rest_actions_task_status_s[] = {
    "pending", "active", "completed", "stopped", "failed", "unimplemented",
};

#define ACTIONS_TASK_VALID 1 << 0
#define ACTIONS_TASK_INVALID 1 << 1
#define ACTIONS_TASK_NOT_IMPLEMENTED 1 << 2

typedef enum
{
    ACTIONS_RESULT_SUCCESS,
    ACTIONS_RESULT_PENDING,
    ACTIONS_RESULT_RETRY,
    ACTIONS_RESULT_FAILURE,
    ACTIONS_RESULT_STOPPED,
    ACTIONS_RESULT_NO_CHANGE_REQUIRED,
} rest_actions_task_result_t;

// keep a reference to the result of an action
#define MAX_TYPELENGTH 20
typedef struct relationship
{
    char mType[MAX_TYPELENGTH];
    char mId[UUID_STR_LEN];
} relationship_t;

typedef struct task_node_s
{
    cJSON                     *task;
    uuid_t                     id;
    char                       id_str[UUID_STR_LEN];
    rest_actions_task_t        type;
    rest_actions_task_status_t status;
    int                        created;
    int                        timeout;
    int                        last_evaluated;
    struct task_node_s        *prev;
    struct task_node_s        *next;
    bool                       deleteTask;
    relationship_t             relationship;
} task_node_t;

#define ACTIONS_TASK_NO_TIMEOUT -1

/**
 * @brief Allocate and duplicate a new JSON task to be pushed into the REST action queue.
 *        The JSON task should be validated and no error checking is performed in this function.
 *
 * @param task Pointer to cJSON task to be queued
 * @return The task_node_t pointer to the newly duplicated and allocated for the given
 *         JSON task. The pointer is ready to be assigned into a task_node_t queue as needed.
 */
task_node_t *task_node_new(cJSON *task);

/**
 * @brief   This function updates the state to one of the value from
 *          rest_actions_task_status_t
 *
 * @param task_node  pointer of a task to be updated with new status
 * @param status    Intended status value from rest_actions_task_status_t
 */
void task_update_status(task_node_t *task_node, rest_actions_task_status_t status);

/**
 * @brief   This function converts the data from task node into JSON format.
 *
 * @param task_node A pointer of task node that we want to jsonify.
 * @return cJSON*   Returns the task node that is convered into JSON format.
 */
cJSON *task_node_to_json(task_node_t *task_node);

/**
 * @brief  Checks if a task is completed, failed or stopped.
 *
 * @param aTaskNode A pointer of task that is being checked for stop, complete or fail condition.
 * @return true     If one of the condition gets satisfied.
 * @return false    None of the condition is satisfied.
 */
bool can_remove_task(task_node_t *aTaskNode);

#ifdef __cplusplus
} // end of extern "C"
#endif

#endif
