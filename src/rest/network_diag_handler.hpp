/*
 *  Copyright (c) 2025, The OpenThread Authors.
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
 *   This file includes Handler definition for network diagnostics collector.
 */

#ifndef OTBR_REST_NETWORK_DIAG_HANDLER_HPP_
#define OTBR_REST_NETWORK_DIAG_HANDLER_HPP_

/*
Include necessary headers for OpenThread functions, REST utilities, and JSON processing.
*/

#include "openthread-br/config.h"

#include <bitset>
#include <unordered_map>

#include <openthread/border_agent.h>
#include <openthread/border_router.h>

#include "json.hpp"
#include "rest_devices_coll.hpp"
#include "rest_diagnostics_coll.hpp"
#include "services.hpp"
#include "common/api_strings.hpp"
#include "host/thread_helper.hpp"
#include "openthread/dataset.h"
#include "openthread/dataset_ftd.h"
#include "openthread/mesh_diag.h"

#include "diagnostic_types.hpp"

// Forward declare cJSON in the global scope
struct cJSON;

using std::chrono::steady_clock;

namespace otbr {
namespace rest {

/**
 * This class implements the handlers for collecting diagnostic requests (DiagReq) and diagnostic queries (DiagQuery)
 * for OTBR-REST.
 *
 */
class NetworkDiagHandler
{
public:
    NetworkDiagHandler(Services &aServices, otInstance *aInstance);

    /**
     * Starts a network discovery to update the devices collection.
     *
     * @param  aTimeout     Timeout in milliseconds
     * @param  aMaxAge      MaxAge in milliseconds
     * @param  aRetryCount
     *
     * @retval OTBR_ERROR_NONE            Completed successfully.
     * @retval OTBR_ERROR_INVALID_STATE   Another request is in processing.
     */
    otError HandleNetworkDiscoveryRequest(uint32_t aTimeout, uint32_t aMaxAge, uint8_t aRetryCount);

    /**
     *  Starts a diagnostics request, results are added to diagnostic collection.
     *
     * @param[in] aDestination Destination address of the diagnostics request.
     * @param[in] aTlvList     List of TLVs to be requested.
     * @param[in] aTlvCount    Number of TLVs in the list.
     * @param[in] aTimeout     Timeout in Seconds.
     */
    otError StartDiagnosticsRequest(const otIp6Address &aDestination,
                                    const uint8_t      *aTlvList,
                                    size_t              aTlvCount,
                                    Seconds             aTimeout);

    /**
     * Get the status of the ongoing diagnostics request.
     *
     * @param[in] aAddressString Destination address of the diagnostics request.
     * @param[in] aType          Type of the address.
     * @param[out] aResultsUuid Uuid of the results of the diagnostics request, only valid if the request is done.
     *
     * @retval OTBR_ERROR_NONE            Request is done and completed successfully.
     */
    otError GetDiagnosticsStatus(const char *aAddressString, AddressType aType, std::string &aResultsUuid);

    /**
     * Get the status of the ongoing discovery request.
     * @param[out] aDeviceCount Number of devices discovered.
     *
     * @retval OTBR_ERROR_NONE            Request is done and completed successfully.
     */
    otError GetDiscoveryStatus(uint32_t &aDeviceCount);

    /**
     *
     */
    void StopDiagnosticsRequest(void);

    /**
     * @brief Continue a ongoing request assuring retries and completeness of responses.
     *
     * @retval OTBR_ERROR_NONE            Completed successfully.
     * @retval OTBR_ERROR_ERRNO           Result pending, call again later.
     * @retval OTBR_ERROR_ABORT           Timeout, processing stopped.
     * @retval OTBR_ERROR_REST            Insufficient message buffers available to send DIAG_GET.req.
     */
    otbrError Process(void);

    /**
     * @brief Do we have the expected count of TLV responses from the at least one known device(s)?
     */
    void IsDiagSetComplete(bool &complete);

    /**
     * @brief Clear internal buffer.
     *
     */
    void Clear(void);

    // add or update item in device collection
    void SetDeviceItemAttributes(std::string aExtAddr, DeviceInfo &aDevice);

private:
    enum class RequestState : uint8_t
    {
        kIdle,
        kWaiting,
        kPending,
        kFailed,
        kDone,
    };

    otInstance *mInstance;
    Services   &mServices;

    // disable copy constructor
    NetworkDiagHandler(const NetworkDiagHandler &) = delete;

    // disable the copy assignment operator
    NetworkDiagHandler &operator=(const NetworkDiagHandler &) = delete;

    struct RouterChildTable
    {
        steady_clock::time_point          mUpdateTime; // timestamp of successfull update
        RequestState                      mState;
        uint16_t                          mRetries; // actual retry count
        std::vector<otMeshDiagChildEntry> mChildTable;
    };

    struct RouterChildIp6Addrs
    {
        steady_clock::time_point    mUpdateTime; // timestamp of successfull update
        RequestState                mState;
        uint16_t                    mRetries; // actual retry count
        std::vector<DeviceIp6Addrs> mChildren;
    };

    struct RouterNeighbors
    {
        steady_clock::time_point                   mUpdateTime; // timestamp of successfull update
        RequestState                               mState;
        uint16_t                                   mRetries; // actual retry count
        std::vector<otMeshDiagRouterNeighborEntry> mNeighbors;
    };

    // oldest timestamp of previous diagnostic responses considered still valid
    steady_clock::time_point mMaxAge;
    steady_clock::time_point mTimeout;
    steady_clock::time_point mTimeLastAttempt; // time of last attempt

    uint8_t mMaxRetries; // applies to DiagReq and DiagQuery
    uint8_t mRetries;    // actual retry count

    /**
     * Buffer for DiagRequest Responses.
     *
     * May be filled with rloc16s from which responses are expected. See 'ResetRouterDiag()' and 'ResetChildDiag()'
     */
    std::unordered_map<uint16_t, DiagInfo> mDiagSet;

    RequestState mRequestState; // overall state of a DiagRequest
    otIp6Address mIp6address;   // destination of a request

    bool mIsDiscoveryRequest; // If true we are processing a discovery request

    uint8_t  mDiagReqTlvs[DiagnosticTypes::kMaxTotalCount];
    uint32_t mDiagReqTlvsCount;
    uint32_t mDiagReqTlvsOmitableCount; // count of TLVs that may be omitted by the destination

    /**
     * Buffer for DiagQuery Responses.
     *
     * May be filled with rloc16s from which responses are expected. See 'ResetChildTables()'.
     */
    std::unordered_map<uint16_t, RouterChildTable> mChildTables;

    /**
     * Buffer for DiagQuery Responses.
     *
     * May be filled with rloc16s from which responses are expected. See 'ResetChildIp6Addrs()'.
     */
    std::unordered_map<uint16_t, RouterChildIp6Addrs> mChildIps;

    /**
     * Buffer for DiagQuery Responses.
     *
     * May be filled with rloc16s from which responses are expected. See 'ResetRouterNeighbors()'.
     */
    std::unordered_map<uint16_t, RouterNeighbors> mRouterNeighbors;

    uint8_t      mDiagQueryTlvs[DiagnosticTypes::kMaxQueryCount]; // TLVs for DiagQuery
    uint32_t     mDiagQueryTlvsCount;
    RequestState mDiagQueryRequestState; // state of the DiagQuery
    uint16_t     mDiagQueryRequestRloc;  // destination of the DiagQuery

    std::string mResultUuid;

    /**
     * @brief Reset router entries in mDiagSet buffer.
     *
     * @param aLearnRloc16 Remove old entries (false), remove old entries and learn rloc16 address from router table
     * (true).
     */
    void ResetRouterDiag(bool aLearnRloc16);

    /**
     * @brief Reset child entries in mDiagSet buffer.
     *
     * Reset entries in mDiagSet that are not a router rloc16
     * and delete empty entries or entries older than aMaxAge
     *
     * @param aMaxAge Oldest timestamp of previous diagnostic responses considered still valid.
     */
    void ResetChildDiag(steady_clock::time_point aMaxAge);

    /**
     * Check if all expected TLVs are present in aDiagContent.
     */
    bool IsDiagContentIncomplete(const std::vector<otNetworkDiagTlv> &aDiagContent) const;

    /**
     * @brief Add or update existing item in mDiagSet with new responses
     *
     * @param aKey  a rloc16
     * @param aDiag a vector of TLVs received from rloc16
     */
    void UpdateDiag(uint16_t aKey, std::vector<otNetworkDiagTlv> &aDiag);

    /**
     * @brief Reset entries in mChildTables buffer.
     *
     * @param aLearnRloc16 Remove old entries (false), remove old entries and learn rloc16 address from router table
     * (true).
     */
    void ResetChildTables(bool aLearnRloc16);

    /**
     * @brief Reset entries in mChildIps buffer.
     *
     * @param aLearnRloc16 Remove old entries (false), remove old entries and learn rloc16 address from router table
     * (true).
     */
    void ResetChildIp6Addrs(bool aLearnRloc16);

    /**
     * @brief Reset entries in mRouterNeighbors buffer.
     *
     * @param aLearnRloc16 Remove old entries (false), remove old entries and learn rloc16 address from router table
     * (true).
     */
    void ResetRouterNeighbors(bool aLearnRloc16);

    // set buffer and address for unicast to a single destination
    void AddSingleRloc16LookUp(uint16_t aRloc16);

    /**
     * @brief Set minimal TLVs needed to collect for filling device collection
     *
     */
    void SetDefaultTlvs(void);

    /**
     * @brief Translate list of string TLV type names to list of integer TLV types
     *        to handle DiagRequests and DiagQueries separately.
     *
     * @param aTypes types from a action of type 'getNetworkDiagnosticTask'.
     *
     * @retval OTBR_ERROR_NONE
     * @retval OTBR_ERROR_INVALID_ARGS  Unknown DiagQuery TLV type
     */
    // otbrError extractTlvSet(cJSON *aTypes);

    /**
     * @brief Discover thread devices and fill or update the device collection.
     *
     * Called from 'handleNetworkDiscoveryRequest' when destination is empty and relationshipType == "devices".
     * Or called directly from tbd 'updateDeviceCollectionTask'.
     *
     * @retval OTBR_ERROR_NONE
     *         OTBR_ERROR_INVALID_STATE   Another request is in processing.
     *         OTBR_ERROR_REST            Insufficient message buffers available to send DIAG_GET.req.
     */
    otError StartDiscovery(void);

    // Handlers for DiagRequests
    static void DiagnosticResponseHandler(otError              aError,
                                          otMessage           *aMessage,
                                          const otMessageInfo *aMessageInfo,
                                          void                *aContext);
    void        DiagnosticResponseHandler(otError aError, const otMessage *aMessage, const otMessageInfo *aMessageInfo);

    // look-up hostname registered for ipv6 of the device in SRP server
    void GetHostName(DeviceInfo &aDeviceInfo);

    // transfer responses in mChildTables and mChildIps buffer into device collection
    void GetChildren(const uint16_t &aParentRloc16);

    // update NodeInfo of this device in device collection
    void UpdateNodeItem(ThisThreadDevice *thisItem);

    // transfer responses in mDiagSet buffer into device collection
    void FillDeviceCollection(void);

    // add local border router counters
    void GetLocalCounters(otbr::rest::NetworkDiagnostics *aDeviceDiag);

    // add service flags
    void SetServiceRoleFlags(otbr::rest::NetworkDiagnostics *aDeviceDiag, otNetworkDiagTlv aTlv);

    // transfer DiagQuery responses to aDeviceDiag item
    void SetDiagQueryTlvs(otbr::rest::NetworkDiagnostics *aDeviceDiag, const uint16_t &aParentRloc16);

    // transfer responses in mDiagSet buffer into diagnostic collection
    void FillDiagnosticCollection(otExtAddress aExtAddr);

    /**
     * @brief Send Diagnostic Query to get the child table TLV 'OT_NETWORK_DIAGNOSTIC_TLV_CHILD'.
     *
     * @param aRloc16     short address of a router
     * @param aMaxAge     oldest timestamp of previous diagnostic responses considered still valid
     * @param aChildTable RouterChildTable state of request and content of request responses.
     * @retval true       Query completed and response received.
     * @retval false      Waiting for results.
     */
    bool RequestChildTable(uint16_t aRloc16, steady_clock::time_point aMaxAge, RouterChildTable *aChildTable);

    // Handlers for DiagQuery Requests for TLV 'OT_NETWORK_DIAGNOSTIC_TLV_CHILD'
    static void MeshChildTableResponseHandler(otError aError, const otMeshDiagChildEntry *aChildEntry, void *aContext);
    void        MeshChildTableResponseHandler(otError aError, const otMeshDiagChildEntry *aChildEntry);

    /**
     * @brief Send Diagnostic Query to get the child ipv6 address TLV 'OT_NETWORK_DIAGNOSTIC_TLV_CHILD_IP6_ADDR_LIST'.
     *
     * @param aParentRloc16     short address of a router
     * @param aMaxAge     oldest timestamp of previous diagnostic responses considered still valid
     * @param aChild RouterChildIp6Addr state of request and content of request responses.
     * @retval true       Query completed and response received.
     * @retval false      Waiting for results.
     */
    bool RequestChildIp6Addrs(uint16_t aParentRloc16, steady_clock::time_point aMaxAge, RouterChildIp6Addrs *aChild);

    // Handlers for DiagQuery Requests for TLV 'OT_NETWORK_DIAGNOSTIC_TLV_CHILD_IP6_ADDR_LIST'
    static void MeshChildIp6AddrResponseHandler(otError                    aError,
                                                uint16_t                   aChildRloc16,
                                                otMeshDiagIp6AddrIterator *aIp6AddrIterator,
                                                void                      *aContext);
    void        MeshChildIp6AddrResponseHandler(otError                    aError,
                                                uint16_t                   aChildRloc16,
                                                otMeshDiagIp6AddrIterator *aIp6AddrIterator);

    /**
     * @brief Send Diagnostic Query to get the child ipv6 address TLV 'OT_NETWORK_DIAGNOSTIC_TLV_ROUTER_NEIGHBOR'.
     *
     * @param aRloc16     short address of a router
     * @param aMaxAge     oldest timestamp of previous diagnostic responses considered still valid
     * @param aRouterNeighbor RouterNeighbor state of request and content of request responses.
     * @retval true       Query completed and response received.
     * @retval false      Waiting for results.
     */
    bool RequestRouterNeighbors(uint16_t aRloc16, steady_clock::time_point aMaxAge, RouterNeighbors *aRouterNeighbor);

    // Handlers for DiagQuery Requests for TLV 'OT_NETWORK_DIAGNOSTIC_TLV_ROUTER_NEIGHBOR'
    static void MeshRouterNeighborsResponseHandler(otError                              aError,
                                                   const otMeshDiagRouterNeighborEntry *aNeighborEntry,
                                                   void                                *aContext);
    void        MeshRouterNeighborsResponseHandler(otError aError, const otMeshDiagRouterNeighborEntry *aNeighborEntry);

    /**
     * @brief Iterate through mDiagQueryTlvs and sequentially collect Query TLVs.
     *
     * @retval true  Completed all DiagQuery Requests.
     * @retval false Waiting for more results.
     */
    bool HandleNextDiagQuery(void);
};

} // namespace rest
} // namespace otbr

#endif // OTBR_REST_NETWORK_DIAG_HANDLER_HPP_
