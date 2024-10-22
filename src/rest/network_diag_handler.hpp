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

#include <unordered_map>

#include <openthread/border_agent.h>
#include <openthread/border_router.h>

#include "common/api_strings.hpp"
#include "openthread/dataset.h"
#include "openthread/dataset_ftd.h"
#include "openthread/mesh_diag.h"
#include "rest/extensions/rest_devices_coll.hpp"
#include "rest/extensions/rest_diagnostics_coll.hpp"
#include "rest/extensions/rest_task_handler.hpp"
#include "rest/extensions/rest_task_queue.hpp"
#include "rest/json.hpp"
#include "rest/request.hpp"
#include "utils/thread_helper.hpp"

extern "C" {
#include <cJSON.h>
}

using std::chrono::steady_clock;

namespace otbr {
namespace rest {

#define MAX_TLV_COUNT 27 // Maximal amount of TLVs

/**
 * This class implements the handlers for collecting diagnostic requests (DiagReq) and diagnostic queries (DiagQuery)
 * for OTBR-REST.
 *
 */
class NetworkDiagHandler
{
public:
    static NetworkDiagHandler &getInstance(otInstance *aOtInstance)
    {
        static NetworkDiagHandler instance;
        instance.setOtInstance(aOtInstance);
        return instance;
    };

    /**
     * @brief Handle a diagnostic action request.
     *
     * Parses the destination into an ipv6 address and sorts the aTlvTypes into types for DiagRequests and types for
     * DiagQuerys. Then begins collecting the DiagRequest TLVs sequentially via unicast request, if not a multicast
     * destination was given. Next, unicast DiagQuerys are started.
     *
     * A call to continueHandleRequest must follow in order to continue and complete the request.
     *
     * @param destination       The destination of the diagnostic request. Can be empty (default, unicast to all
     * routers), or (TODO) an existing deviceId, a ML-EID-IID, a rloc16, or a multicast address.
     * @param relationshipType  The type of result, that should be created after a successful request.
     *                          'devices'     -> creates results in api/devices collection
     *                          'diagnostics' -> creates results in api/diagnostics collection
     *
     * @retval OTBR_ERROR_NONE            Completed successfully.
     * @retval OTBR_ERROR_INVALID_STATE   Another request is in processing.
     * @retval OTBR_ERROR_ABORT           Timeout
     * @retval OTBR_ERROR_REST            Insufficient message buffers available to send DIAG_GET.req.
     */
    otbrError handleNetworkDiscoveryRequest(std::string destination, std::string relationshipType);

    /**
     * @brief Processes an action of type 'getNetworkDiagnosticTask'.
     *
     * A call to continueHandleRequest must follow in order to continue and complete the request.
     *
     * @param aTaskNode
     * @retval OTBR_ERROR_NONE            Completed successfully.
     * @retval OTBR_ERROR_INVALID_STATE   Another request is in processing.
     * @retval OTBR_ERROR_ABORT           Timeout
     * @retval OTBR_ERROR_REST            Insufficient message buffers available to send DIAG_GET.req.
     */
    otbrError handleNetworkDiagnosticsAction(task_node_t *aTaskNode);

    /**
     * @brief Continue a ongoing request assuring retries and completeness of responses.
     *
     * @retval OTBR_ERROR_NONE            Completed successfully.
     * @retval OTBR_ERROR_ERRNO           Result pending, call again later.
     * @retval OTBR_ERROR_ABORT           Timeout, processing stopped.
     * @retval OTBR_ERROR_REST            Insufficient message buffers available to send DIAG_GET.req.
     */
    otbrError continueHandleRequest(void);

    /**
     * @brief Configures internal parameters for next request.
     *
     * @param aTimeout      (0) Defaults to now() + 10s.
     * @param aMaxAge       (0) Defaults to now() - 30s.
     * @param aRetryCount   Max retries for DiagReq and (TODO) DiagQuery.
     *
     * @retval OTBR_ERROR_INVALID_STATE  Change of configuration denied, while another request is in processing.
     * @retval OTBR_ERROR_NONE           Changed configuration successfully.
     */
    otbrError configRequest(uint32_t aTimeout, uint32_t aMaxAge, uint8_t aRetryCount, task_doneCallback aCallback);

    /**
     * @brief Cancels an active request.
     *
     * @retval OTBR_ERROR_NONE
     */
    otbrError cancelRequest(void);

    /**
     * @brief Clear internal buffer.
     *
     */
    void clear(void);

private:
    enum class RequestState : uint8_t
    {
        kIdle,
        kWaiting,
        kPending,
        kDone,
    };

    otInstance *mInstance;

    // private constructor to keep this a singleton class and prevent multiple instantiation
    NetworkDiagHandler()
    {
        mRequestState          = RequestState::kIdle;
        mDiagQueryRequestState = RequestState::kIdle;
    }

    // disable copy constructor
    NetworkDiagHandler(const NetworkDiagHandler &) = delete;

    // disable the copy assignment operator
    NetworkDiagHandler &operator=(const NetworkDiagHandler &) = delete;

    void setOtInstance(otInstance *aOtInstance) { mInstance = aOtInstance; }

    struct RouterChildTable
    {
        steady_clock::time_point          mUpdateTime;
        RequestState                      mState;
        std::vector<otMeshDiagChildEntry> mChildTable;
    };

    struct RouterChildIp6Addrs
    {
        steady_clock::time_point    mUpdateTime;
        RequestState                mState;
        std::vector<DeviceIp6Addrs> mChildren;
    };

    struct RouterNeighbors
    {
        steady_clock::time_point                   mUpdateTime;
        RequestState                               mState;
        std::vector<otMeshDiagRouterNeighborEntry> mNeighbors;
    };

    // oldest timestamp of previous diagnostic responses considered still valid
    steady_clock::time_point mMaxAge;
    steady_clock::time_point mTimeout;
    steady_clock::time_point mTimeLastAttempt; // time of last attempt

    uint8_t mMaxRetries;
    uint8_t mRetries; // actual retry count

    /**
     * Buffer for DiagRequest Responses.
     *
     * May be filled with rloc16s from which responses are expected. See 'ResetRouterDiag()' and 'ResetChildDiag()'
     */
    std::unordered_map<uint64_t, DiagInfo> mDiagSet;

    RequestState mRequestState; // overall state of a DiagRequest
    otIp6Address mIp6address;   // destination of a request. Used for retry when no rloc16 is known.

    uint8_t mDiagReqTlvs[MAX_TLV_COUNT];
    size_t  mDiagReqTlvsCount;

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

    std::vector<uint8_t> mDiagQueryTlvs;         // TLVs for DiagQuery
    RequestState         mDiagQueryRequestState; // state of the DiagQuery
    uint16_t             mDiagQueryRequestRloc;  // destination of the DiagQuery

    std::string       mRelationshipType; // type of relationship requested, e.g. "devices" or "diagnostics"
    task_node_t      *mActionTask;       // reference to the task in the action collection that has started the request
    task_doneCallback mCallback;

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
     * @brief Convert destination string into an ip6address.
     *
     * Lookup destination in device collection to find ML-Eid-Iid and construct ML-Eid address,
     * or if not found converts a valid string of length 16 to ML-Eid address,
     * or if string of length 6 converts to ML rloc16 address.
     *
     * @param aDestination string of length 16 or 6.
     * @param aIp6address result of conversion.
     *
     * @retval OTBR_ERROR_PARSE if not successfull
     * @retval OTBR_ERROR_NONE on success
     */
    otbrError LookupDestinationAddr(std::string aDestination, otIp6Address *aIp6address);

    /**
     * @brief Translate list of string TLV type names to list of integer TLV types
     *        to handle DiagRequests and DiagQueries separately.
     *
     * @param aTypes types from a action of type 'getNetworkDiagnosticTask'.
     *
     * @retval OTBR_ERROR_NONE
     * @retval OTBR_ERROR_INVALID_ARGS  Unknown DiagQuery TLV type
     */
    otbrError extractTlvSet(cJSON *aTypes);

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
    otbrError startDiscovery(void);

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

    // transfer responses in mDiagSet buffer into device collection
    void FillDeviceCollection(void);

    // add or update item in device collection
    void SetDeviceItemAttributes(std::string aExtAddr, DeviceInfo &aDevice);

    // add local border router counters
    void GetLocalCounters(otbr::rest::NetworkDiagnostics *aDeviceDiag);

    // add service flags
    void SetServiceRoleFlags(otbr::rest::NetworkDiagnostics *aDeviceDiag, otNetworkDiagTlv aTlv);

    // transfer DiagQuery responses to aDeviceDiag item
    void SetDiagQueryTlvs(otbr::rest::NetworkDiagnostics *aDeviceDiag, const uint16_t &aParentRloc16);

    // transfer responses in mDiagSet buffer into diagnostic collection
    void FillDiagnosticCollection(void);

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
