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

#include <unordered_map>

#include <openthread/border_router.h>

#include "agent/ncp_openthread.hpp"
#include "agent/thread_helper.hpp"
#include "rest/json.hpp"
#include "rest/request.hpp"
#include "rest/response.hpp"

namespace otbr {
namespace Rest {

/**
 * This class implements the Resource handler for OTBR-REST.
 *
 */
class Resource
{
public:
    /**
     * The constructor initializes the resource handler instance.
     *
     * @param[in]   aNcp  A pointer to the NCP controller.
     *
     */
    Resource(otbr::Ncp::ControllerOpenThread *aNcp);

    /**
     * This method initialize the Resource handler.
     *
     *
     */
    void Init(void);

    /**
     * This method is the main entry of resource handler, which find corresponding handler according to request url
     * find the resource and set the content of response.
     *
     * @param[in]      aRequest  A request instance referred by the Resource handler.
     * @param[inout]   aResponse  A response instance will be set by the Resource handler.
     *
     */
    void Handle(Request &aRequest, Response &aResponse) const;

    /**
     * This method distributes a callback handler for each connection needs a callback.
     *
     * @param[in]      aRequest  A request instance referred by the Resource handler.
     * @param[inout]   aResponse  A response instance will be set by the Resource handler.
     *
     */
    void HandleCallback(Request &aRequest, Response &aResponse);

    /**
     * This method provides a quick handler, which could directly set response code of a response and set error code and
     * error message to the request body.
     *
     * @param[in]      aRequest  A request instance referred by the Resource handler.
     * @param[inout]   aErrorCode  An enum class represents the status code.
     * @param[in]      aErrorDescription Detailed description of error message.
     *
     */
    void ErrorHandler(Response &aResponse, HttpStatusCode aErrorCode, std::string aErrorDescription) const;

    /**
     * This method is a pre-defined callback function used for call another private method when diagnostic information
     * arrives.
     *
     * @param[in]      aMessage  A pointer to the message buffer containing the received Network Diagnostic
     * @param[in]   aMessageInfo A pointer to the message info for @p aMessage
     * @param[in]       aContext    A pointer to application-specific context.
     */
    static void DiagnosticResponseHandler(otMessage *aMessage, const otMessageInfo *aMessageInfo, void *aContext);

    /**
     * This method handle conmmissione state change (callback function).
     *
     * @param[in]   aState      The state of commissioner.
     * @param[in]   aContext    A pointer to the ubus context.
     *
     */
    static void HandleStateChanged(otCommissionerState aState, void *aContext);

    /**
     * This method handle joiner event (callback function).
     *
     * @param[in]  aEvent       The joiner event type.
     * @param[in]  aJoinerInfo  A pointer to the Joiner Info.
     * @param[in]  aJoinerId    A pointer to the Joiner ID (if not known, it will be NULL).
     * @param[in]  aContext     A pointer to application-specific context.
     *
     */
    static void HandleJoinerEvent(otCommissionerJoinerEvent aEvent,
                                  const otJoinerInfo *      aJoinerInfo,
                                  const otExtAddress *      aJoinerId,
                                  void *                    aContext);
    /**
     * This method is a pre-defined callback function used for binding with another callback function defined by
     * thread_helper.
     *
     *
     * @param[inout]  aResponse  A pointer pointing to a response instance.
     * @param[in]     aError     otError represents error type.
     * @param[in]     aResult    Scan result.
     *
     */
    void NetworksResponseHandler(Response *aResponse, otError aError, const std::vector<otActiveScanResult> &aResult);

private:
    typedef void (Resource::*ResourceHandler)(const Request &aRequest, Response &aResponse) const;
    typedef void (Resource::*ResourceCallbackHandler)(const Request &aRequest, Response &aResponse);

    // RESTful API entry
    void NodeInfo(const Request &aRequest, Response &aResponse) const;
    void ExtendedAddr(const Request &aRequest, Response &aResponse) const;
    void State(const Request &aRequest, Response &aResponse) const;
    void NetworkName(const Request &aRequest, Response &aResponse) const;
    void LeaderData(const Request &aRequest, Response &aResponse) const;
    void NumOfRoute(const Request &aRequest, Response &aResponse) const;
    void Rloc16(const Request &aRequest, Response &aResponse) const;
    void ExtendedPanId(const Request &aRequest, Response &aResponse) const;
    void Rloc(const Request &aRequest, Response &aResponse) const;
    void Diagnostic(const Request &aRequest, Response &aResponse) const;
    void Networks(const Request &aRequest, Response &aResponse) const;
    void CurrentNetwork(const Request &aRequest, Response &aResponse) const;
    void CurrentNetworkPrefix(const Request &aRequest, Response &aResponse) const;
    void CurrentNetworkCommission(const Request &aRequest, Response &aResponse) const;

    // Callback Handler
    void HandleDiagnosticCallback(const Request &aRequest, Response &aResponse);
    void PostNetworksCallback(const Request &aRequest, Response &aResponse);
    void PutCurrentNetworkCallback(const Request &aRequest, Response &aResponse);
    void CurrentNetworkCommissionCallback(const Request &aRequest, Response &aResponse);

    void PostCurrentNetworkCommission(const Request &aRequest, Response &aResponse) const;
    void DeleteCurrentNetworkPrefix(const Request &aRequest, Response &aResponse) const;
    void GetCurrentNetworkPrefix(const Request &aRequest, Response &aResponse) const;
    void PostCurrentNetworkPrefix(const Request &aRequest, Response &aResponse) const;
    void PutCurrentNetwork(Response &aResponse) const;
    void GetCurrentNetwork(Response &aResponse) const;
    void GetNetworks(Response &aResponse) const;
    void GetNodeInfo(Response &aResponse) const;
    void GetDataExtendedAddr(Response &aResponse) const;
    void GetDataState(Response &aResponse) const;
    void GetDataNetworkName(Response &aResponse) const;
    void GetDataLeaderData(Response &aResponse) const;
    void GetDataNumOfRoute(Response &aResponse) const;
    void GetDataRloc16(Response &aResponse) const;
    void GetDataExtendedPanId(Response &aResponse) const;
    void GetDataRloc(Response &aResponse) const;
    void PostNetworks(Response &aResponse) const;

    // Methods that manipulate Diagnostic information
    void DeleteOutDatedDiagnostic(void);
    void UpdateDiag(std::string aKey, std::vector<otNetworkDiagTlv> &aDiag);

    // private funtion that is called by punlic static function
    void DiagnosticResponseHandler(otMessage *aMessage, const otMessageInfo);
    void HandleStateChanged(otCommissionerState aState) const;
    void HandleJoinerEvent(otCommissionerJoinerEvent aEvent,
                           const otJoinerInfo *      aJoinerInfo,
                           const otExtAddress *      aJoinerId) const;

    otInstance *                     mInstance;
    otbr::Ncp::ControllerOpenThread *mNcp;

    // Resource Handler Map
    std::unordered_map<std::string, ResourceHandler> mResourceMap;

    // Reource Handler Map for those need callback
    std::unordered_map<std::string, ResourceCallbackHandler> mResourceCallbackMap;

    // Map from Status Code to Status description
    std::unordered_map<HttpStatusCode, std::string, HttpStatusCodeHash> mResponseCodeMap;

    // Map that maintain Diagnostic information
    std::unordered_map<std::string, DiagInfo> mDiagSet;
};

} // namespace Rest
} // namespace otbr

#endif // OTBR_REST_RESOURCE_HPP_
