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
#include "rest_task_energy_scan.hpp"
#include "rest_diagnostics_coll.hpp"
#include "rest/extensions/commissioner_allow_list.hpp"
#include "rest/extensions/rest_devices_coll.hpp"
#include "rest/extensions/uuid.hpp"
#include "rest/types.hpp"

#ifdef __cplusplus
extern "C" {
#endif

#include <cJSON.h>
#include <errno.h>
#include <time.h>
#include <openthread/commissioner.h>
#include <openthread/logging.h>
#include <openthread/platform/radio.h>

const char *taskNameEnergyScan = "getEnergyScanTask";

// @note: Accommodating customer naming convention without refactoring whole codebase
static const char *const ATTRIBUTE_DESTINATION  = "destination";  // the on-mesh-mleid address, or a {deviceid}
static const char *const ATTRIBUTE_COUNT        = "count";        // count of (repeated) scans
static const char *const ATTRIBUTE_MASK         = "channelMask";  // channel mask
static const char *const ATTRIBUTE_PERIOD       = "period";       // scan period
static const char *const ATTRIBUTE_SCANDURATION = "scanDuration"; // scan duration

// Current OT-API does not seem to support concurrent energy scans,
// therefore we handle just one energy scan request at a time.
enum class EnergyScanState : std::uint8_t
{
    kIdle,
    kSendReq,
    kCallbackWait,
    kHandleCb,
    kComplete,
};

// mutex to block concurrent energy scans
static EnergyScanState sState;
// a reference to the action that is in processing
static task_node_t *sAction;

// a struct for collecting the measure results from one scan
otbr::rest::EnergyScanReport mEsr;
uint8_t                      meas_count_received_total = 0;

static task_doneCallback sDoneCallback; // a function pointer to be called when task is finished.

static void init_energyscanreport(otIp6InterfaceIdentifier mlEidIid, uint8_t count, cJSON *mask)
{
    cJSON *item = nullptr;

    meas_count_received_total = 0;
    mEsr.count                = count;
    mEsr.origin               = mlEidIid;
    for (auto &it : mEsr.report)
    {
        it.maxRssi.clear();
    }
    mEsr.report.clear();

    cJSON_ArrayForEach(item, mask)
    {
        otbr::rest::EnergyReport report;
        report.channel = cJSON_GetNumberValue(item);
        mEsr.report.push_back(report);
    }

    cJSON_Delete(item);
}

cJSON *jsonify_energy_scan_task(task_node_t *aTaskNode)
{
    cJSON *task_json = task_node_to_json(aTaskNode);

    return task_json;
}

uint8_t validate_energy_scan_task(cJSON *attributes)
{
    otError      error    = OT_ERROR_NONE;
    otExtAddress mlEidIid = {0};
    cJSON       *item     = nullptr;
    uint8_t      channel;

    cJSON *timeout      = cJSON_GetObjectItemCaseSensitive(attributes, "timeout");
    cJSON *destination  = cJSON_GetObjectItemCaseSensitive(attributes, ATTRIBUTE_DESTINATION);
    cJSON *mask         = cJSON_GetObjectItemCaseSensitive(attributes, ATTRIBUTE_MASK);
    cJSON *count        = cJSON_GetObjectItemCaseSensitive(attributes, ATTRIBUTE_COUNT);
    cJSON *period       = cJSON_GetObjectItemCaseSensitive(attributes, ATTRIBUTE_PERIOD);
    cJSON *scanDuration = cJSON_GetObjectItemCaseSensitive(attributes, ATTRIBUTE_SCANDURATION);

    VerifyOrExit(nullptr != timeout && cJSON_IsNumber(timeout), error = OT_ERROR_FAILED);

    VerifyOrExit(nullptr != destination && cJSON_IsString(destination) && 16 == strlen(destination->valuestring) &&
                     is_hex_string(destination->valuestring),
                 error = OT_ERROR_FAILED);
    // check destination is convertable
    SuccessOrExit(error = str_to_m8(mlEidIid.m8, destination->valuestring, OT_EXT_ADDRESS_SIZE));

    otbrLogWarning("%s:%d - %s - cjson destination: %s", "EnergyScan", __LINE__, __func__, cJSON_Print(destination));

    // TODO: check limits
    //  && (0xFFFFFFFF <= cJSON_GetNumberValue(mask))
    //   && (0xFF <= cJSON_GetNumberValue(count))
    //   && (0xFFFF <= cJSON_GetNumberValue(period))
    //   && (0xFFFF <= cJSON_GetNumberValue(scanDuration))

    VerifyOrExit(nullptr != mask && cJSON_IsArray(mask), error = OT_ERROR_FAILED);

    cJSON_ArrayForEach(item, mask)
    {
        if (cJSON_IsNumber(item))
        {
            channel = item->valueint;
            VerifyOrExit(channel >= 11 && channel <= 26, error = OT_ERROR_FAILED);
        }
    }

    VerifyOrExit(nullptr != count && cJSON_IsNumber(count), error = OT_ERROR_FAILED);
    VerifyOrExit(nullptr != period && cJSON_IsNumber(period), error = OT_ERROR_FAILED);
    VerifyOrExit(nullptr != scanDuration && cJSON_IsNumber(scanDuration), error = OT_ERROR_FAILED);

exit:
    if (item != nullptr)
    {
        cJSON_Delete(item);
    }
    if (error != OT_ERROR_NONE)
    {
        otbrLogWarning("%s:%d - %s - missing or bad value in a field: %s", "EnergyScan", __LINE__, __func__,
                       cJSON_Print(attributes));
        return ACTIONS_TASK_INVALID;
    }
    return ACTIONS_TASK_VALID;
}

/**
 * @brief A otCommissionerEnergyReportCallback.
 *
 * @param aChannelMask
 * @param aEnergyList
 * @param aEnergyListLength
 * @param aContext
 *
 * Note: can be called several times for a larger chunk of results, when results are transmitted in multiple packets
 *       expect results in aEnergyList [chA, chB, chC, ... chA, chB, chC, ...]
 * Note: If we terminate an active task before all chunks are received, and also have a new task started and active,
 *       we cannot distinguish a delayed chunk of results from the previous energy scan.
 */
void HandleEnergyReport(uint32_t aChannelMask, const uint8_t *aEnergyList, uint8_t aEnergyListLength, void *aContext)
{
    otError error = OT_ERROR_NONE;

    uint8_t i, j;
    uint8_t channel_count, meas_count;

    OT_UNUSED_VARIABLE(aContext);
    // task contains the task description used for validation of matching
    // task_node_t *aTaskNode = static_cast<task_node_t *>(aContext);

    // cJSON *attributes = cJSON_GetObjectItemCaseSensitive(aTaskNode->task, "attributes");
    // cJSON *destination     = cJSON_GetObjectItemCaseSensitive(attributes, ATTRIBUTE_DESTINATION);
    //  check results match to prepared report
    // SuccessOrExit(strcmp(mEsr.origin_str, destination->valuestring));

    VerifyOrExit(sState == EnergyScanState::kCallbackWait, error = OT_ERROR_INVALID_STATE);

    channel_count = my_count_ones(aChannelMask);
    // check sizes match expectations
    VerifyOrExit(aEnergyListLength % channel_count == 0, error = OT_ERROR_PARSE);

    meas_count = aEnergyListLength / channel_count;

    for (j = 0; j < meas_count; j++)
    {
        for (i = 0; i < channel_count; i++)
        {
            mEsr.report[i].maxRssi.push_back((int8_t)(aEnergyList[i + j]));
        }
    }

    meas_count_received_total += meas_count;

    // otbrLogWarning("%s:%d - %s - listLength %d.", "EnergyScan", __LINE__, __func__, aEnergyListLength);

    if (meas_count_received_total == mEsr.count)
    {
        // store results
        otbr::rest::EnergyScanDiagnostics res;
        res.mReport = mEsr; // TODO check if copy constructor needed

        otbr::rest::gDiagnosticsCollection.addItem(&res);

        // aTaskNode keeps reference to result
        snprintf(sAction->relationship.mType, MAX_TYPELENGTH,
                 otbr::rest::gDiagnosticsCollection.getCollectionName().c_str());
        snprintf(sAction->relationship.mId, UUID_STR_LEN, res.mUuid.toString().c_str());

        sState  = EnergyScanState::kComplete;
        sAction = nullptr;

        otbrLogWarning("%s:%d - %s - changed to state %d.", "EnergyScan", __LINE__, __func__, sState);
        if (sDoneCallback != nullptr)
        {
            sDoneCallback();
        }
    }
    // else we expect more results to come in another packet
    else
    {
        otbrLogWarning("%s:%d - %s - received total %d measurements, expect %d.", "EnergyScan", __LINE__, __func__,
                       meas_count_received_total, mEsr.count);
    }

exit:
    if (error != OT_ERROR_NONE)
    {
        otbrLogWarning("%s:%d - %s - error: %s", "EnergyScan", __LINE__, __func__, otThreadErrorToString(error));
    }
    return;
}

otError startEnergyScan(task_node_t *aTaskNode, otInstance *aInstance)
{
    otError error                              = OT_ERROR_NONE;
    char    buffer[OT_IP6_ADDRESS_STRING_SIZE] = {'\0'};
    cJSON  *item                               = nullptr;
    uint8_t channel;

    otbr::rest::ThreadDevice *destinationDevice;

    otIp6InterfaceIdentifier mlEidIid;
    otIp6Address             ip6address;
    const otMeshLocalPrefix *prefix;

    cJSON *task       = aTaskNode->task;
    cJSON *attributes = cJSON_GetObjectItemCaseSensitive(task, "attributes");
    // cJSON *timeout    = cJSON_GetObjectItemCaseSensitive(attributes, "timeout");

    cJSON *destination  = cJSON_GetObjectItemCaseSensitive(attributes, ATTRIBUTE_DESTINATION);
    cJSON *mask         = cJSON_GetObjectItemCaseSensitive(attributes, ATTRIBUTE_MASK);
    cJSON *count        = cJSON_GetObjectItemCaseSensitive(attributes, ATTRIBUTE_COUNT);
    cJSON *period       = cJSON_GetObjectItemCaseSensitive(attributes, ATTRIBUTE_PERIOD);
    cJSON *scanDuration = cJSON_GetObjectItemCaseSensitive(attributes, ATTRIBUTE_SCANDURATION);

    // construct uint32_t mask from list of channels
    otChannelMask bitmask = 0;
    cJSON_ArrayForEach(item, mask)
    {
        channel = cJSON_GetNumberValue(item);
        bitmask |= (1 << channel);
    }

    uint8_t  count_val   = cJSON_GetNumberValue(count);
    uint16_t period_val  = cJSON_GetNumberValue(period);
    uint16_t scanDur_val = cJSON_GetNumberValue(scanDuration);

    VerifyOrExit(sState == EnergyScanState::kIdle, error = OT_ERROR_INVALID_STATE);
    sState = EnergyScanState::kSendReq;
    otbrLogWarning("%s:%d - %s - changed to state %d.", "EnergyScan", __LINE__, __func__, sState);

    otbrLogWarning("%s:%d - %s - channelMask 0x%08x.", "EnergyScan", __LINE__, __func__, bitmask);

    // check if destination can be mapped to a mleidiid,
    //       then destination has to be a known deviceId in gDevicesCollection with a known mleidiid.
    //       if not, we use destination as a mleidiid
    //       TODO: check mleidiid is valid.

    destinationDevice =
        dynamic_cast<otbr::rest::ThreadDevice *>(otbr::rest::gDevicesCollection.getItem(destination->valuestring));
    if (destinationDevice != nullptr)
    {
        // destination is not a known deviceId
        memcpy(mlEidIid.mFields.m8, destinationDevice->mDeviceInfo.mMlEidIid.m8, OT_EXT_ADDRESS_SIZE);
    }
    else
    {
        str_to_m8(mlEidIid.mFields.m8, destination->valuestring, OT_EXT_ADDRESS_SIZE);
    }
    // construct full ip6address of destination
    prefix = otThreadGetMeshLocalPrefix(aInstance);
    combineMeshLocalPrefixAndIID(prefix, &mlEidIid, &ip6address);

    otIp6AddressToString(&ip6address, buffer, OT_IP6_ADDRESS_STRING_SIZE);
    otbrLogWarning("%s:%d - %s - destination %s.", "EnergyScan", __LINE__, __func__, buffer);

    error = otCommissionerEnergyScan(aInstance, bitmask, count_val, period_val, scanDur_val, &ip6address,
                                     &HandleEnergyReport, (void *)aTaskNode);
    // commissioner state should not be invalid here, as we checked before
    VerifyOrExit(error == OT_ERROR_NONE);

    // init container for results
    init_energyscanreport(mlEidIid, count_val, mask);

    sState  = EnergyScanState::kCallbackWait;
    sAction = aTaskNode;
    otbrLogWarning("%s:%d - %s - changed to state %d.", "EnergyScan", __LINE__, __func__, sState);

exit:
    if (error != OT_ERROR_NONE)
    {
        if (error == OT_ERROR_INVALID_STATE)
        {
            // otbrLogWarning("%s:%d - %s - invalid EnergyScanState %d.", "EnergyScan", __LINE__, __func__, sState);
            //  rewrite error
            error = OT_ERROR_BUSY;
        }
        else
        {
            otbrLogWarning("%s:%d - %s - error: %s", "EnergyScan", __LINE__, __func__, otThreadErrorToString(error));
        }
    }
    return error;
}

rest_actions_task_result_t process_energy_scan_task(task_node_t      *aTaskNode,
                                                    otInstance       *aInstance,
                                                    task_doneCallback aCallback)
{
    otError                    error             = OT_ERROR_NONE;
    otCommissionerState        commissionerState = OT_COMMISSIONER_STATE_DISABLED;
    rest_actions_task_result_t ret               = ACTIONS_RESULT_SUCCESS;

    // Arg check before doing works
    VerifyOrExit((nullptr != aTaskNode && nullptr != aTaskNode->task), error = OT_ERROR_INVALID_ARGS);
    VerifyOrExit(aTaskNode->status == ACTIONS_TASK_STATUS_PENDING, error = OT_ERROR_INVALID_STATE);

    // call callback to execute other tasks after this task is completed
    sDoneCallback = aCallback;

    commissionerState = otCommissionerGetState(aInstance);

    // If the commissioner is already ACTIVE, we can query the energy scan right away
    if (OT_COMMISSIONER_STATE_ACTIVE == commissionerState)
    {
        SuccessOrExit(error = startEnergyScan(aTaskNode, aInstance));
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
        if (error == OT_ERROR_INVALID_STATE)
        {
            otbrLogWarning("%s:%d - %s - error %s - Commissioner not available.", "EnergyScan", __LINE__, __func__,
                           otThreadErrorToString(error));
            ret = ACTIONS_RESULT_RETRY;
        }
        else if ((error == OT_ERROR_ALREADY) or (error == OT_ERROR_BUSY))
        {
            ret = ACTIONS_RESULT_RETRY;
        }
        else
        {
            otbrLogWarning("%s: error %d", __func__, otThreadErrorToString(error));
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
rest_actions_task_result_t evaluate_energy_scan_task(task_node_t *aTaskNode)
{
    OTBR_UNUSED_VARIABLE(aTaskNode);

    otError                    error = OT_ERROR_NONE;
    rest_actions_task_result_t ret   = ACTIONS_RESULT_SUCCESS;

    VerifyOrExit(sState == EnergyScanState::kComplete, error = OT_ERROR_INVALID_STATE);

    sState  = EnergyScanState::kIdle;
    sAction = nullptr;
    otbrLogWarning("%s:%d - %s - changed to state %d.", "EnergyScan", __LINE__, __func__, sState);

exit:
    if (error != OT_ERROR_NONE)
    {
        otbrLogWarning("%s:%d - %s - error: %s", "EnergyScan", __LINE__, __func__, otThreadErrorToString(error));
        switch (error)
        {
        case OT_ERROR_INVALID_STATE:
            ret = ACTIONS_RESULT_PENDING;
            break;
        default:
            otbrLogWarning("%s:%d - %s - error in state %d.", "EnergyScan", __LINE__, __func__, sState);
            sState  = EnergyScanState::kIdle;
            sAction = nullptr;

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
rest_actions_task_result_t clean_energy_scan_task(task_node_t *aTaskNode, otInstance *aInstance)
{
    rest_actions_task_result_t ret = ACTIONS_RESULT_NO_CHANGE_REQUIRED;

    OT_UNUSED_VARIABLE(aInstance);

    if (aTaskNode->status == ACTIONS_TASK_STATUS_ACTIVE)
    {
        // this task must be stopped when clean is called
        sState  = EnergyScanState::kIdle;
        sAction = nullptr;
        otbrLogWarning("%s:%d - %s - changed to state %d.", "EnergyScan", __LINE__, __func__, sState);
    }
    ret               = ACTIONS_RESULT_STOPPED; // marks task as stopped
    aTaskNode->status = ACTIONS_TASK_STATUS_STOPPED;

    return ret;
}

#ifdef __cplusplus
}
#endif
