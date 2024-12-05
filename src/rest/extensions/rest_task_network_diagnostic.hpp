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
#ifndef REST_TASK_NETWORK_DIAGNOSTIC_HPP_
#define REST_TASK_NETWORK_DIAGNOSTIC_HPP_

#include "rest_diagnostics_coll.hpp"
#include "rest_server_common.hpp"
#include "rest_task_handler.hpp"
#include "rest_task_queue.hpp"
#include "rest/json.hpp"

struct cJSON;

#ifdef __cplusplus
extern "C" {
#endif
#include <stdbool.h>
#include <string.h>

#define MAX_TLV_COUNT 27 // Maximal amount of TLVs

extern const char *taskNameNetworkDiagnostic;
extern const char *taskNameNetworkDiagnosticReset;

const char *const ATTRIBUTE_DESTINATION = "destination"; // the on-mesh-mleid address, or a {deviceid}
const char *const ATTRIBUTE_TYPES       = "types";       // TLVs
const char *const ATTRIBUTE_TIMEOUT     = "timeout";     // timeout of the task

/**
 *
 * Converts a network diagnostic task node into its JSON representation.
 *
 * Takes a `task_node_t` pointer that represents a network diagnostic task
 * and converts it into a JSON representation.
 *
 * @param[in] aTaskNode  The node which is to be jsonified.
 * @return A pointer to a cJSON object representing the task node, or `nullptr` if the conversion fails.
 */
cJSON *jsonify_network_diagnostic_task(task_node_t *aTaskNode);

/**
 * Validates the attributes of a network diagnostic task to ensure they meet API schema requirements.
 *
 * Checks the provided JSON structure, verifying that all necessary fields and their values
 * adhere to the expected format and constraints. If the attributes do not meet the criteria,
 * it returns an appropriate validation status.
 *
 * @param[in] aAttributes  The JSON structure containing the attributes to be validated.
 * @return The validation result, which can be one of the following:
 *         - ACTIONS_TASK_VALID
 *         - ACTIONS_TASK_INVALID
 *         - ACTIONS_TASK_NOT_IMPLEMENTED
 */
uint8_t validate_network_diagnostic_task(cJSON *aAttributes);

/**
 * Processes a network diagnostic task by initiating its execution and monitoring its progress.
 *
 * Starts the execution of the network diagnostic task. Once the task begins,
 * it continuously evaluates the task's state, calling the provided callback function when the task
 * is completed. Returns an appropriate status indicating whether the task should be retried,
 * has failed, or is successful.
 *
 * @param[in]  aTaskNode    The task node containing the network diagnostic task to be executed.
 * @param[in]  aInstance    The OpenThread instance associated with the task.
 * @param[in]  aCallback    The callback function to be called when the task is completed.
 *
 * @return The status of the task, which can be one of the following:
 *         - ACTIONS_RESULT_SUCCESS
 *         - ACTIONS_RESULT_FAILURE
 *         - ACTIONS_RESULT_RETRY
 *         - ACTIONS_RESULT_PENDING
 */
rest_actions_task_result_t process_network_diagnostic_task(task_node_t      *aTaskNode,
                                                           otInstance       *aInstance,
                                                           task_doneCallback aCallback);

/**
 * Monitors and evaluates the ongoing execution of a network diagnostic task.
 *
 * This function is responsible for the continuous processing of a network diagnostic task.
 * It monitors the execution of the task and reports if the task has successfully completed,
 * needs to continue running, or has failed.
 *
 * @param[in] aTaskNode  The task node representing the network diagnostic task being evaluated.
 *
 * @return The status of the task, which can be one of the following:
 *         - ACTIONS_RESULT_SUCCESS
 *         - ACTIONS_RESULT_FAILURE
 *         - ACTIONS_RESULT_PENDING
 *         - ACTIONS_RESULT_NO_CHANGE_REQUIRED
 *         - ACTIONS_RESULT_STOPPED
 */
rest_actions_task_result_t evaluate_network_diagnostic_task(task_node_t *aTaskNode);

/**
 * Cleans up and releases resources associated with a network diagnostic task.
 *
 * Frees any resources held by the network diagnostic task, ensuring that it can be
 * safely removed from the task queue.
 *
 * @param[in] aTaskNode   The task node representing the network diagnostic task to be cleaned.
 * @param[in] aInstance   The OpenThread instance associated with the task.
 *
 * @return The status of the cleaning operation, which can be one of the following:
 *         - ACTIONS_RESULT_SUCCESS
 *         - ACTIONS_RESULT_FAILURE
 *         - ACTIONS_RESULT_NO_CHANGE_REQUIRED
 *         - ACTIONS_RESULT_STOPPED
 */
rest_actions_task_result_t clean_network_diagnostic_task(task_node_t *aTaskNode, otInstance *aInstance);

/**
 * Validates the attributes of a network diagnostic reset task to ensure they meet API schema requirements.
 *
 * Checks the provided JSON structure, verifying that all necessary fields and their values
 * adhere to the expected format and constraints. If the attributes do not meet the criteria,
 * it returns an appropriate validation status.
 *
 * @param[in] aAttributes  The JSON structure containing the attributes to be validated.
 * @return The validation result, which can be one of the following:
 *         - ACTIONS_TASK_VALID
 *         - ACTIONS_TASK_INVALID
 *         - ACTIONS_TASK_NOT_IMPLEMENTED
 */
uint8_t validate_network_diagnostic_reset_task(cJSON *aAttributes);

/**
 * Processes a network diagnostic reset task by initiating its execution and monitoring its progress.
 *
 * @param[in]  aTaskNode    The task node containing the network diagnostic task to be executed.
 * @param[in]  aInstance    The OpenThread instance associated with the task.
 * @param[in]  aCallback    (not used)
 *
 * @return The status of the task, which can be one of the following:
 *         - ACTIONS_RESULT_SUCCESS
 *         - ACTIONS_RESULT_FAILURE
 */
rest_actions_task_result_t process_network_diagnostic_reset_task(task_node_t      *aTaskNode,
                                                                 otInstance       *aInstance,
                                                                 task_doneCallback aCallback);

/**
 * @brief Evaluate success of the task.
 *
 * This must be called from rest_task_queue handler,
 * only when aTaskNode->status = ACTIONS_TASK_STATUS_ACTIVE.
 *
 * @param aTaskNode
 * @return ACTIONS_RESULT_SUCCESS when completing successfully
 */
rest_actions_task_result_t evaluate_network_diagnostic_reset_task(task_node_t *aTaskNode);

/**
 * @brief Clean resources before deleting the task.
 *
 * This must be called from rest_task_queue handler, e.g. on timeout of the task.
 *
 * @param aTaskNode
 * @param aInstance
 * @return ACTIONS_RESULT_NO_CHANGE_REQUIRED  task status was not changed, it was stopped already.
 *         ACTIONS_RESULT_STOPPED   when task was stopped
 */
rest_actions_task_result_t clean_network_diagnostic_reset_task(task_node_t *aTaskNode, otInstance *aInstance);

#ifdef __cplusplus
} // end of extern "C"
#endif

#endif // REST_TASK_NETWORK_DIAGNOSTIC_HPP_
