/*
 *  Copyright (c) 2020, The OpenThread Authors.
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
 * @file
 *   This file includes Handler definition for RESTful HTTP server.
 */

#ifndef OTBR_REST_RESOURCE_HPP_
#define OTBR_REST_RESOURCE_HPP_

/*
Include necessary headers for OpenThread functions, REST utilities, and JSON processing.
*/

#include "openthread-br/config.h"

#include <unordered_map>

#include <openthread/border_agent.h>
#include <openthread/border_router.h>

#include "common/api_strings.hpp"
#include "ncp/rcp_host.hpp"
#include "openthread/dataset.h"
#include "openthread/dataset_ftd.h"
#include "openthread/mesh_diag.h"
#include "rest/extensions/rest_devices_coll.hpp"
#include "rest/extensions/rest_diagnostics_coll.hpp"
#include "rest/extensions/rest_task_add_thread_device.hpp"
#include "rest/extensions/rest_task_handler.hpp"
#include "rest/extensions/rest_task_queue.hpp"
#include "rest/json.hpp"
#include "rest/request.hpp"
#include "rest/response.hpp"
#include "utils/thread_helper.hpp"

using otbr::Ncp::RcpHost;
using std::chrono::steady_clock;

namespace otbr {
namespace rest {

/**
 * This class implements the Resource handler for OTBR-REST.
 */
class Resource
{
public:
    /**
     * The constructor initializes the resource handler instance.
     *
     * @param[in] aHost  A pointer to the Thread controller.
     */
    Resource(RcpHost *aHost);

    /**
     * This method initialize the Resource handler.
     */
    void Init(void);

    /**
     * This method is the main entry of resource handler, which find corresponding handler according to request url
     * find the resource and set the content of response.
     *
     * @param[in]     aRequest  A request instance referred by the Resource handler.
     * @param[in,out] aResponse  A response instance will be set by the Resource handler.
     */
    void Handle(Request &aRequest, Response &aResponse);

    /**
     * This method distributes a callback handler for each connection needs a callback.
     *
     * @param[in]     aRequest   A request instance referred by the Resource handler.
     * @param[in,out] aResponse  A response instance will be set by the Resource handler.
     */
    void HandleCallback(Request &aRequest, Response &aResponse);

    /**
     * This method provides a quick handler, which could directly set response code of a response and set error code and
     * error message to the request body.
     *
     * @param[in]     aRequest    A request instance referred by the Resource handler.
     * @param[in,out] aErrorCode  An enum class represents the status code.
     */
    void ErrorHandler(Response &aResponse, HttpStatusCode aErrorCode) const;

private:
    enum class RequestState : uint8_t
    {
        kIdle,
        kWaiting,
        kPending,
        kDone,
    };

    /**
     * This enumeration represents the Dataset type (active or pending).
     */
    enum class DatasetType : uint8_t
    {
        kActive,  ///< Active Dataset
        kPending, ///< Pending Dataset
    };

    typedef void (Resource::*ResourceHandler)(const Request &aRequest, Response &aResponse);
    typedef void (Resource::*ResourceCallbackHandler)(const Request &aRequest, Response &aResponse);
    void NodeInfo(const Request &aRequest, Response &aResponse);
    void BaId(const Request &aRequest, Response &aResponse);
    void ExtendedAddr(const Request &aRequest, Response &aResponse);
    void State(const Request &aRequest, Response &aResponse);
    void NetworkName(const Request &aRequest, Response &aResponse);
    void LeaderData(const Request &aRequest, Response &aResponse);
    void NumOfRoute(const Request &aRequest, Response &aResponse);
    void Rloc16(const Request &aRequest, Response &aResponse);
    void ExtendedPanId(const Request &aRequest, Response &aResponse);
    void Rloc(const Request &aRequest, Response &aResponse);
    void Dataset(DatasetType aDatasetType, const Request &aRequest, Response &aResponse);
    void DatasetActive(const Request &aRequest, Response &aResponse);
    void DatasetPending(const Request &aRequest, Response &aResponse);

    void GetNodeInfo(Response &aResponse) const;
    void DeleteNodeInfo(Response &aResponse) const;
    void GetDataBaId(Response &aResponse) const;
    void GetDataExtendedAddr(Response &aResponse) const;
    void GetDataState(Response &aResponse) const;
    void SetDataState(const Request &aRequest, Response &aResponse) const;
    void GetDataNetworkName(Response &aResponse) const;
    void GetDataLeaderData(Response &aResponse) const;
    void GetDataNumOfRoute(Response &aResponse) const;
    void GetDataRloc16(Response &aResponse) const;
    void GetDataExtendedPanId(Response &aResponse) const;
    void GetDataRloc(Response &aResponse) const;
    void GetDataset(DatasetType aDatasetType, const Request &aRequest, Response &aResponse) const;
    void SetDataset(DatasetType aDatasetType, const Request &aRequest, Response &aResponse) const;

    /**
     * Redirects requests from '/api/{collection}/{collectionItem}' to the corresponding collection.
     *
     * @param[in]     aRequest A request instance to be checked and redirected.
     * @returns       A string containing the redirected url.
     */
    std::string redirectToCollection(Request &aRequest);

    /**
     * Rewrites the URL in the request to redirect to the corresponding deviceItem.
     *
     * @param[in,out] aRequest  A request instance containing the URL to be rewritten.
     */
    void redirectNodeToDeviceItem(Request &aRequest);

    /**
     * Handles all requests received on 'api/action'. [POST|GET|DELETE]
     *
     * Routes POST, GET, and DELETE requests to their respective handlers.
     *
     * @param[in]  aRequest   A request instance.
     * @param[out] aResponse  A response instance that will be populated based on the request.
     */
    void ApiActionHandler(const Request &aRequest, Response &aResponse);

    /**
     * Repeatedly iterates through the list of tasks.
     *
     * Calls `rest_task_queue_handle` after a specified delay, continuing the iteration.
     * TODO: post repeatedly
     *
     * @param[in] delay_ms  The delay in milliseconds between each iteration.
     */
    void ApiActionRepeatedTaskRunner(uint16_t delay_ms);

    /**
     * Handles the POST request received on 'api/actions'.
     *
     * This method parses the received JSON data, validates it, and updates the task status
     * for further processing. The request will be processed and evaluated by the
     * "rest_task_queue_task" thread function.
     *
     * @param[in]  aRequest   A request instance containing the POST data.
     * @param[out] aResponse  A response instance that will be set by an appropriate rest task action.
     */
    void ApiActionPostHandler(const Request &aRequest, Response &aResponse);

    /**
     * Handles the GET request received on 'api/actions'.
     *
     * This method retrieves the collection of actions.
     *
     * @param[in]  aRequest   A request instance.
     * @param[out] aResponse  A response instance that will be populated with the collection of actions.
     */
    void ApiActionGetHandler(const Request &aRequest, Response &aResponse);

    /**
     * Handles the DELETE request received on 'api/actions'.
     *
     * This method clears all items in the collection of actions.
     *
     * @param[in]  aRequest   A request instance.
     * @param[out] aResponse  A response instance populated with the outcome of the request.
     */
    void ApiActionDeleteHandler(const Request &aRequest, Response &aResponse);

    /**
     * Handles all requests received on 'api/diagnostics'. [GET|DELETE]
     *
     * Routes GET, and DELETE requests to their respective handlers.
     *
     * @param[in]  aRequest   A request instance.
     * @param[out] aResponse  A response instance that will be populated based on the request.
     */
    void ApiDiagnosticHandler(const Request &aRequest, Response &aResponse);

    /**
     * Handles the GET request received on 'api/diagnostics'.
     *
     * This method returns the collection of diagnostics. The items in this collection are created
     * as a result of a POST request with type 'getNetworkDiagnosticTask' to 'api/actions'.
     *
     * @param[in]  aRequest   A request instance.
     * @param[out] aResponse  A response instance that will be populated with the collection of diagnostics.
     */
    void ApiDiagnosticGetHandler(const Request &aRequest, Response &aResponse);

    /**
     * Handles the DELETE request received on 'api/diagnostics'.
     *
     * This method clears all items in the collection of diagnostics.
     *
     * @param[in]  aRequest   A request instance.
     * @param[out] aResponse  A response instance that will be populated to confirm the deletion.
     */
    void ApiDiagnosticDeleteHandler(const Request &aRequest, Response &aResponse);

    /**
     * Handles all requests received on 'api/devices'. [GET|POST|DELETE]
     *
     * Routes POST, GET, and DELETE requests to their respective handlers.
     *
     * @param[in]  aRequest   A request instance.
     * @param[out] aResponse  A response instance that will be populated based on the request.
     */
    void ApiDeviceHandler(const Request &aRequest, Response &aResponse);

    /**
     * Handles the GET request received on 'api/devices'.
     *
     * This method returns the collection of devices.
     *
     * @param[in]  aRequest   A request instance.
     * @param[out] aResponse  A response instance that will be populated with the collection of devices.
     */
    void ApiDeviceGetHandler(const Request &aRequest, Response &aResponse);

    /**
     * Handles the DELETE request received on 'api/devices'.
     *
     * This method clears all items in the collection of devices.
     *
     * @param[in]  aRequest   A request instance.
     * @param[out] aResponse  A response instance that will be populated to confirm the deletion of the device
     * collection.
     */
    void ApiDeviceDeleteHandler(const Request &aRequest, Response &aResponse);

    /*********************************************************************************************************************
     * TODO: redirect a POST request from api/devices to actions to fill up the device collection
     *********************************************************************************************************************/
    /**
     * Handles the POST request received on 'api/devices'.
     *
     * This method discovers devices in the network and updates the collection of devices.
     *
     * @param[in]  aRequest   A request instance containing details for device discovery.
     * @param[out] aResponse  A response instance that will be populated to confirm the update of the device collection.
     */
    void ApiDevicePostHandler(const Request &aRequest, Response &aResponse);
    void ApiDevicePostCallbackHandler(const Request &aRequest, Response &aResponse);

    otInstance *mInstance;
    RcpHost    *mHost;

    std::unordered_map<std::string, ResourceHandler>         mResourceMap;
    std::unordered_map<std::string, ResourceCallbackHandler> mResourceCallbackMap;

    /**
     * @brief A state change handler. Handle list of actions waiting for commissioner
     *
     * @param aFlags
     * @return * void
     */
    static void HandleThreadStateChanges(otChangedFlags aFlags);
};

} // namespace rest
} // namespace otbr

#endif // OTBR_REST_RESOURCE_HPP_
