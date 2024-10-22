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
#include "rest_task_network_diagnostic.hpp"
#include "unordered_map"
#include "rest/extensions/commissioner_allow_list.hpp"
#include "rest/extensions/rest_devices_coll.hpp"
#include "rest/extensions/uuid.hpp"
#include "rest/network_diag_handler.hpp"

// Default TlvTypes for Diagnostic information
static const std::unordered_map<std::string, uint8_t> kTlvTypeMap = {
    {KEY_EXTADDRESS, 0},          ///< MAC Extended Address TLV
    {KEY_RLOC16, 1},              ///< Address16 TLV
    {KEY_MODE, 2},                ///< Mode TLV
    {KEY_TIMEOUT, 3},             ///< Timeout TLV (max polling time period for SEDs)
    {KEY_CONNECTIVITY, 4},        ///< Connectivity TLV
    {KEY_ROUTE, 5},               ///< Route64 TLV
    {KEY_LEADERDATA, 6},          ///< Leader Data TLV
    {KEY_NETWORKDATA, 7},         ///< Network Data TLV
    {KEY_IP6ADDRESSLIST, 8},      ///< IPv6 Address List TLV
    {KEY_MACCOUNTERS, 9},         ///< MAC Counters TLV
    {KEY_BATTERYLEVEL, 14},       ///< Battery Level TLV
    {KEY_SUPPLYVOLTAGE, 15},      ///< Supply Voltage TLV
    {KEY_CHILDTABLE, 16},         ///< Child Table TLV
    {KEY_CHANNELPAGES, 17},       ///< Channel Pages TLV
    {KEY_MAXCHILDTIMEOUT, 19},    ///< Max Child Timeout TLV
    {KEY_LDEVID, 20},             ///< LDevID subject public key info TLV
    {KEY_IDEV, 21},               ///< IDevID Certificate TLV
    {KEY_EUI64, 23},              ///< EUI64 TLV
    {KEY_VERSION, 24},            ///< Thread Version TLV
    {KEY_VENDORNAME, 25},         ///< Vendor Name TLV
    {KEY_VENDORMODEL, 26},        ///< Vendor Model TLV
    {KEY_VENDORSWVERSION, 27},    ///< Vendor SW Version TLV
    {KEY_THREADSTACKVERSION, 28}, ///< Thread Stack Version TLV (codebase/commit version)
    {KEY_CHILDREN, 29},           ///< Child TLV
    {KEY_CHILDRENIP6, 30},        ///< Child IPv6 Address List TLV
    {KEY_NEIGHBORS, 31},          ///< Router Neighbor TLV
    {KEY_MLECOUNTERS, 34}         ///< MLE Counters TLV
};

#ifdef __cplusplus
extern "C" {
#endif

#include <cJSON.h>
#include <errno.h>
#include <time.h>
#include <openthread/commissioner.h>
#include <openthread/instance.h>
#include <openthread/logging.h>
#include <openthread/netdiag.h>
#include <openthread/platform/radio.h>

const char *taskNameNetworkDiagnostic      = "getNetworkDiagnosticTask";
const char *taskNameNetworkDiagnosticReset = "resetNetworkDiagCounterTask";

// a reference to the otInstance
static otInstance *sInstance;

// a function pointer to be called when task is finished.
static task_doneCallback sDoneCallback;

cJSON *jsonify_network_diagnostic_task(task_node_t *aTaskNode)
{
    cJSON *taskJson = task_node_to_json(aTaskNode);

    return taskJson;
}

static bool validTLV(const char *aTlv)
{
    std::string tlvStr(aTlv);
    return kTlvTypeMap.find(tlvStr) != kTlvTypeMap.end();
}

static bool validResetableTLV(const char *aTlv)
{
    return ((std::string(aTlv) == std::string(KEY_MACCOUNTERS)) || (std::string(aTlv) == std::string(KEY_MLECOUNTERS)));
}

uint8_t validate_network_diagnostic_task(cJSON *aAttributes)
{
    otError      error    = OT_ERROR_NONE;
    otExtAddress mlEidIid = {0};
    cJSON       *item     = nullptr;

    cJSON *destination = cJSON_GetObjectItemCaseSensitive(aAttributes, ATTRIBUTE_DESTINATION);
    cJSON *types       = cJSON_GetObjectItemCaseSensitive(aAttributes, ATTRIBUTE_TYPES);
    cJSON *timeout     = cJSON_GetObjectItemCaseSensitive(aAttributes, ATTRIBUTE_TIMEOUT);

    VerifyOrExit(nullptr != timeout && cJSON_IsNumber(timeout), error = OT_ERROR_FAILED);

    // for now we enforce a valid destination string representing a deviceId or a mlEidIid.
    // if no destination is provided, we may also default to collecting diagnostics from all deviceIds in the device
    // collection, and/or update also the device collection (TODO).
    VerifyOrExit(nullptr != destination && cJSON_IsString(destination) && 16 == strlen(destination->valuestring) &&
                     is_hex_string(destination->valuestring),
                 error = OT_ERROR_FAILED);

    // check destination is convertable
    SuccessOrExit(error = str_to_m8(mlEidIid.m8, destination->valuestring, OT_EXT_ADDRESS_SIZE));

    // TODO: check if destination exists in mDeviceSet/mDiagSet

    otbrLogWarning("%s:%d - %s - cjson destination: %s", __FILE__, __LINE__, __func__, cJSON_Print(destination));

    VerifyOrExit(nullptr != types && cJSON_IsArray(types), error = OT_ERROR_FAILED);

    // Check if requested tlv types are valid types
    cJSON_ArrayForEach(item, types)
    {
        VerifyOrExit(cJSON_IsString(item), error = OT_ERROR_INVALID_ARGS);
        VerifyOrExit(validTLV(item->valuestring), error = OT_ERROR_INVALID_ARGS);
    }

exit:
    if (error != OT_ERROR_NONE)
    {
        otbrLogWarning("%s:%d - %s - [%s] missing or bad value in a field: %s", __FILE__, __LINE__, __func__,
                       otThreadErrorToString(error), cJSON_Print(aAttributes));
        return ACTIONS_TASK_INVALID;
    }
    return ACTIONS_TASK_VALID;
}

rest_actions_task_result_t process_network_diagnostic_task(task_node_t      *aTaskNode,
                                                           otInstance       *aInstance,
                                                           task_doneCallback aCallback)
{
    otbrError                  error = OTBR_ERROR_NONE;
    rest_actions_task_result_t ret   = ACTIONS_RESULT_SUCCESS;

    // get handler for network diagnostics
    otbr::rest::NetworkDiagHandler &netDiagHandler = otbr::rest::NetworkDiagHandler::getInstance(aInstance);

    // Arg check before doing works
    VerifyOrExit((nullptr != aTaskNode && nullptr != aTaskNode->task), error = OTBR_ERROR_INVALID_ARGS);
    VerifyOrExit(aTaskNode->status == ACTIONS_TASK_STATUS_PENDING, error = OTBR_ERROR_INVALID_STATE);

    sDoneCallback = aCallback; // call callback to execute other tasks after this task is completed
    sInstance     = aInstance; // to find the NetworkDiagHandler Instance

    // set some default values for NetworkDiagHandler, make configurable later from a "updateDeviceCollection" task
    VerifyOrExit((error = netDiagHandler.configRequest(10000, 30000, 1, aCallback)) == OTBR_ERROR_NONE);
    error = netDiagHandler.handleNetworkDiagnosticsAction(aTaskNode);

exit:
    if (error != OTBR_ERROR_NONE)
    {
        if (error == OTBR_ERROR_INVALID_STATE)
        {
            ret = ACTIONS_RESULT_RETRY;
        }
        else
        {
            otbrLogWarning("%s:%d - %s - task failed. error %s", __FILE__, __LINE__, __func__, otbrErrorString(error));
            ret = ACTIONS_RESULT_FAILURE;
        }
    }
    return ret;
}

/**
 * @brief Evaluate success of the task.
 *
 * This must be called from rest_task_queue handler,
 * only when aTaskNode->status = ACTIONS_TASK_STATUS_ACTIVE.
 *
 * @param aTaskNode
 * @return ACTIONS_RESULT_SUCCESS when completing successfully
 *         ACTIONS_RESULT_PENDING when not yet completed
 *         ACTIONS_RESULT_FAILURE on other failures
 */
rest_actions_task_result_t evaluate_network_diagnostic_task(task_node_t *aTaskNode)
{
    OTBR_UNUSED_VARIABLE(aTaskNode);

    otbrError                  error = OTBR_ERROR_NONE;
    rest_actions_task_result_t ret   = ACTIONS_RESULT_SUCCESS;

    // get handler for network diagnostics
    otbr::rest::NetworkDiagHandler &netDiagHandler = otbr::rest::NetworkDiagHandler::getInstance(sInstance);
    error                                          = netDiagHandler.continueHandleRequest();

    if (error != OTBR_ERROR_NONE)
    {
        switch (error)
        {
        case OTBR_ERROR_ERRNO:
            OT_FALL_THROUGH;
        case OTBR_ERROR_INVALID_STATE:
            ret = ACTIONS_RESULT_PENDING;
            break;
        case OTBR_ERROR_ABORTED:
            ret = ACTIONS_RESULT_STOPPED;
            break;
        default:
            ret = ACTIONS_RESULT_FAILURE;
        }
    }
    return ret;
}

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
rest_actions_task_result_t clean_network_diagnostic_task(task_node_t *aTaskNode, otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);
    rest_actions_task_result_t ret = ACTIONS_RESULT_NO_CHANGE_REQUIRED;

    if (aTaskNode->status == ACTIONS_TASK_STATUS_ACTIVE)
    {
        // this task must be stopped when clean is called
        // get handler for network diagnostics
        otbr::rest::NetworkDiagHandler &netDiagHandler = otbr::rest::NetworkDiagHandler::getInstance(sInstance);
        netDiagHandler.cancelRequest();

        ret               = ACTIONS_RESULT_STOPPED; // marks task as stopped
        aTaskNode->status = ACTIONS_TASK_STATUS_STOPPED;
    }

    return ret;
}

uint8_t validate_network_diagnostic_reset_task(cJSON *aAttributes)
{
    /* Example Post Request Body
    {
      "data": [
        {
          "type": "resetNetworkDiagCounterTask",
          "attributes": {
            "types": ["mleCounter", "macCounter"],
            "timeout": 60
          }
        }
      ]
    }
    */
    otError error = OT_ERROR_NONE;
    cJSON  *item  = nullptr;

    cJSON *destination = cJSON_GetObjectItemCaseSensitive(aAttributes, ATTRIBUTE_DESTINATION);
    cJSON *types       = cJSON_GetObjectItemCaseSensitive(aAttributes, ATTRIBUTE_TYPES);
    cJSON *timeout     = cJSON_GetObjectItemCaseSensitive(aAttributes, ATTRIBUTE_TIMEOUT);

    if (destination != nullptr)
    {
        // unicast requests
        error = OT_ERROR_NOT_IMPLEMENTED;
    }

    VerifyOrExit(nullptr != types && cJSON_IsArray(types), error = OT_ERROR_FAILED);

    // Check if requested tlv types are valid types
    cJSON_ArrayForEach(item, types)
    {
        VerifyOrExit(cJSON_IsString(item), error = OT_ERROR_INVALID_ARGS);
        VerifyOrExit(validResetableTLV(item->valuestring), error = OT_ERROR_INVALID_ARGS);
    }

    VerifyOrExit(nullptr != timeout && cJSON_IsNumber(timeout), error = OT_ERROR_FAILED);

exit:
    if (error != OT_ERROR_NONE)
    {
        otbrLogWarning("%s:%d - %s - [%s] missing or bad value in a field: %s", __FILE__, __LINE__, __func__,
                       otThreadErrorToString(error), cJSON_Print(aAttributes));
        return ACTIONS_TASK_INVALID;
    }
    return ACTIONS_TASK_VALID;
}

rest_actions_task_result_t process_network_diagnostic_reset_task(task_node_t      *aTaskNode,
                                                                 otInstance       *aInstance,
                                                                 task_doneCallback aCallback)
{
    OT_UNUSED_VARIABLE(aCallback);
    otbrError                  error = OTBR_ERROR_NONE;
    rest_actions_task_result_t ret   = ACTIONS_RESULT_SUCCESS;

    cJSON       *task;
    cJSON       *attributes;
    cJSON       *types;
    cJSON       *item;
    uint8_t      tlvTypes[2];
    otIp6Address destination;

    uint8_t count = 0;

    // Arg check before doing works
    VerifyOrExit((nullptr != aTaskNode && nullptr != aTaskNode->task), error = OTBR_ERROR_INVALID_ARGS);
    VerifyOrExit(aTaskNode->status == ACTIONS_TASK_STATUS_PENDING, error = OTBR_ERROR_INVALID_STATE);

    task       = aTaskNode->task;
    attributes = cJSON_GetObjectItemCaseSensitive(task, "attributes");
    types      = cJSON_GetObjectItemCaseSensitive(attributes, ATTRIBUTE_TYPES);

    cJSON_ArrayForEach(item, types)
    {
        count++;
        tlvTypes[count] = kTlvTypeMap.find(std::string(item->valuestring))->second;
    }

    // reset counters at all devices via multicast
    destination = *otThreadGetRealmLocalAllThreadNodesMulticastAddress(aInstance);
    otThreadSendDiagnosticReset(aInstance, &destination, tlvTypes, count);

exit:
    if (error != OTBR_ERROR_NONE)
    {
        otbrLogWarning("%s:%d - %s - task failed. error %s", __FILE__, __LINE__, __func__, otbrErrorString(error));
        ret = ACTIONS_RESULT_FAILURE;
    }
    return ret;
}

rest_actions_task_result_t evaluate_network_diagnostic_reset_task(task_node_t *aTaskNode)
{
    OT_UNUSED_VARIABLE(aTaskNode);

    rest_actions_task_result_t ret = ACTIONS_RESULT_SUCCESS;

    return ret;
}

rest_actions_task_result_t clean_network_diagnostic_reset_task(task_node_t *aTaskNode, otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);
    rest_actions_task_result_t ret = ACTIONS_RESULT_NO_CHANGE_REQUIRED;

    if (aTaskNode->status == ACTIONS_TASK_STATUS_ACTIVE)
    {
        ret = ACTIONS_RESULT_STOPPED;
    }

    return ret;
}

#ifdef __cplusplus
}
#endif
