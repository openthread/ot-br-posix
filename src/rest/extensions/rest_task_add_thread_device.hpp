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
#ifndef REST_TASK_ADD_THREAD_DEVICE_HPP_
#define REST_TASK_ADD_THREAD_DEVICE_HPP_

#include "rest_server_common.hpp"
#include "rest_task_handler.hpp"
#include "rest_task_queue.hpp"

struct cJSON;

#ifdef __cplusplus
extern "C" {
#endif
#include <stdbool.h>
#include <string.h>

extern const char         *taskNameAddThreadDevice;
cJSON                     *jsonify_add_thread_device_task(task_node_t *task_node);
uint8_t                    validate_add_thread_device_task(cJSON *task);
rest_actions_task_result_t process_add_thread_device_task(task_node_t      *task_node,
                                                          otInstance       *aInstance,
                                                          task_doneCallback aCallback);
rest_actions_task_result_t evaluate_add_thread_device_task(task_node_t *task_node);
rest_actions_task_result_t clean_add_thread_device_task(task_node_t *task_node, otInstance *aInstance);

#ifdef __cplusplus
} // end of extern "C"
#endif

#endif // REST_TASK_ADD_THREAD_DEVICE_HPP_
