#include "rest/network_diag_handler.hpp"

#include "openthread/mesh_diag.h"
#include "openthread/platform/toolchain.h"
#include "rest/json.hpp"
#include "rest/types.hpp"

#include <cstring>
#include <set>
#include <openthread/srp_server.h>
#include "common/logging.hpp"
#include "common/task_runner.hpp"
#include "rest/extensions/rest_task_network_diagnostic.hpp"
#include "rest/resource.hpp"
#include "utils/string_utils.hpp"

extern "C" {
#include <cJSON.h>
extern task_node_t *task_queue;
extern uint8_t      task_queue_len;
}

using std::chrono::duration_cast;
using std::chrono::milliseconds;
using std::chrono::steady_clock;

using std::placeholders::_1;
using std::placeholders::_2;

namespace otbr {
namespace rest {

// MaxAge (in Milliseconds) for accepting previously collected diagnostics
static const uint32_t kDiagMaxAge            = 30000;
static const uint32_t kDiagMaxAge_UpperLimit = 10 * kDiagMaxAge;

// Timeout (in Milliseconds) for collecting diagnostics, default if not given in actionTask
static const uint32_t kDiagCollectTimeout            = 10000;
static const uint32_t kDiagCollectTimeout_UpperLimit = 10 * kDiagCollectTimeout;

static const uint32_t kDiagMaxRetries = 3;

// Retry delay (in Milliseconds) for retry DiagRequest to FTDs
static const uint32_t kDiagRetryDelayFTD = 100;

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

// check all fields of DeviceInfo are set
bool isDeviceComplete(DeviceInfo &aDeviceInfo);

// check aExtAddr has a plausible value
bool isOtExtAddrEmpty(otExtAddress aExtAddr);

// check aAddr has a plausible value
bool isOtIp6AddrEmpty(otIp6Address aIpv6Addr);

// extract the OMR Ipv6 address and the MlEidIid from ipv6Addr
void filterIpv6(DeviceInfo &aDeviceInfo, const otIp6Address &aIpv6Addr, const otIp6NetworkPrefix *aMlPrefix);

bool isOtExtAddrEmpty(otExtAddress aExtAddr)
{
    for (int i = 0; i < OT_EXT_ADDRESS_SIZE; i++)
    {
        if (aExtAddr.m8[i] != 0)
        {
            return false;
        }
    }

    return true;
}

bool isOtIp6AddrEmpty(otIp6Address aIpv6Addr)
{
    for (int i = 0; i < OT_IP6_ADDRESS_SIZE; ++i)
    {
        if (aIpv6Addr.mFields.m8[i] != 0)
        {
            return false;
        }
    }

    return true;
}

bool isDeviceComplete(DeviceInfo &aDeviceInfo)
{
    VerifyOrExit(aDeviceInfo.mRole != "");
    // VerifyOrExit(device.mHostName != "");

    VerifyOrExit(!isOtExtAddrEmpty(aDeviceInfo.mMlEidIid));
    VerifyOrExit(!isOtExtAddrEmpty(aDeviceInfo.mEui64));
    VerifyOrExit(!isOtIp6AddrEmpty(aDeviceInfo.mIp6Addr));

    return true;

exit:
    return false;
}

void filterIpv6(DeviceInfo &aDeviceInfo, const otIp6Address &aIpv6Addr, const otIp6NetworkPrefix *aMlPrefix)
{
    otIp6NetworkPrefix DeviceIPPrefix;

    // rloc and aloc prefix == 0000:00FF:FE00 -> 0000:FF00:00FE == "0:65280:254"
    if (aIpv6Addr.mFields.m16[4] == 0 && aIpv6Addr.mFields.m16[5] == 65280 && aIpv6Addr.mFields.m16[6] == 254)
    {
        return;
    }

    DeviceIPPrefix = aIpv6Addr.mFields.mComponents.mNetworkPrefix;
    if (std::equal(std::begin(aMlPrefix->m8), std::end(aMlPrefix->m8), std::begin(DeviceIPPrefix.m8)))
    {
        for (uint16_t i = 8; i < 16; ++i)
        {
            aDeviceInfo.mMlEidIid.m8[i - 8] = aIpv6Addr.mFields.m8[i];
        }
    }

    // link local prefix == fe80 -> 00fe == 33022, Off-Mesh-Routable Multicast prefix ff00 == 65280 and ff0f == 65295
    else if (aIpv6Addr.mFields.m16[0] != 33022 &&
             (htons(aIpv6Addr.mFields.m16[0]) < 65280 || htons(aIpv6Addr.mFields.m16[0]) > 65295))
    {
        aDeviceInfo.mIp6Addr = aIpv6Addr;
    }
}

void NetworkDiagHandler::clear(void)
{
    mDiagSet.clear();
    mChildTables.clear();
    mChildIps.clear();
    mRouterNeighbors.clear();
}

otbrError NetworkDiagHandler::cancelRequest(void)
{
    mRequestState          = RequestState::kIdle;
    mDiagQueryRequestState = RequestState::kIdle;
    mCallback              = nullptr;
    return OTBR_ERROR_NONE;
};

otbrError NetworkDiagHandler::configRequest(uint32_t          aTimeout,
                                            uint32_t          aMaxAge,
                                            uint8_t           aRetryCount,
                                            task_doneCallback aCallback)
{
    otbrError error = OTBR_ERROR_NONE;

    if (mRequestState == RequestState::kIdle)
    {
        mTimeout = steady_clock::now() +
                   milliseconds(std::min(std::max(kDiagCollectTimeout, aTimeout), kDiagCollectTimeout_UpperLimit));
        mMaxAge = steady_clock::now() - milliseconds(std::min(std::max(kDiagMaxAge, aMaxAge), kDiagMaxAge_UpperLimit));

        mMaxRetries = aRetryCount;
        mCallback   = aCallback;
    }
    else
    {
        error = OTBR_ERROR_INVALID_STATE;
    }
    return error;
}

// minimal TLVs required to fill device collection
// call if no TLV types are given in the action task
void NetworkDiagHandler::SetDefaultTlvs(void)
{
    // pre-defined DiagRequest TLVs
    mDiagReqTlvs[0] = OT_NETWORK_DIAGNOSTIC_TLV_EXT_ADDRESS;
    mDiagReqTlvs[1] = OT_NETWORK_DIAGNOSTIC_TLV_SHORT_ADDRESS;
    mDiagReqTlvs[2] = OT_NETWORK_DIAGNOSTIC_TLV_IP6_ADDR_LIST;
    // mDiagReqTlvs[3] = OT_NETWORK_DIAGNOSTIC_TLV_VERSION;
    mDiagReqTlvsCount = 3;

    // pre-defined DiagQuery TLVs
    mDiagQueryTlvs.clear();
    mDiagQueryTlvs.emplace_back(OT_NETWORK_DIAGNOSTIC_TLV_CHILD);
    mDiagQueryTlvs.emplace_back(OT_NETWORK_DIAGNOSTIC_TLV_CHILD_IP6_ADDR_LIST);
}

otbrError NetworkDiagHandler::LookupDestinationAddr(std::string aDestination, otIp6Address *aIp6address)
{
    otbrError error = OTBR_ERROR_NONE;

    otbr::rest::ThreadDevice *deviceItem;
    const otMeshLocalPrefix  *prefix;
    otIp6InterfaceIdentifier  mlEidIid;
    uint16_t                  rloc                               = 0xfffe;
    char                      buffer[OT_IP6_ADDRESS_STRING_SIZE] = {'\0'};

    // check if destination is a known deviceId
    // and if this deviceItem has already a learned mleidiid attribute
    deviceItem = dynamic_cast<otbr::rest::ThreadDevice *>(otbr::rest::gDevicesCollection.getItem(aDestination));
    if (deviceItem != nullptr)
    {
        memcpy(mlEidIid.mFields.m8, deviceItem->mDeviceInfo.mMlEidIid.m8, OT_EXT_ADDRESS_SIZE);
        // construct full ip6address of destination
        prefix = otThreadGetMeshLocalPrefix(mInstance);
        combineMeshLocalPrefixAndIID(prefix, &mlEidIid, aIp6address);

        if (isOtExtAddrEmpty(deviceItem->mDeviceInfo.mMlEidIid))
        {
            error = OTBR_ERROR_PARSE;
        }
    }
    else if (aDestination.size() == 16)
    {
        // come here, when ML-Eid-Iid had not been learned
        // assume destination is ML-Eid-IID
        // TODO: check mleidiid is valid.
        str_to_m8(mlEidIid.mFields.m8, aDestination.c_str(), OT_EXT_ADDRESS_SIZE);
        // construct full ip6address of destination
        prefix = otThreadGetMeshLocalPrefix(mInstance);
        combineMeshLocalPrefixAndIID(prefix, &mlEidIid, aIp6address);
    }
    else if (aDestination.size() == 6)
    {
        // come here, when ML-Eid-Iid had not been learned
        // assume destination is rloc16, eg. 0x0800
        sscanf(aDestination.c_str(), "%hx", &rloc);
        memcpy(aIp6address, otThreadGetRloc(mInstance), sizeof(otIp6Address));
        aIp6address->mFields.m16[7] = htons(rloc);
    }
    else
    {
        error = OTBR_ERROR_PARSE;
    }

    otIp6AddressToString(aIp6address, buffer, OT_IP6_ADDRESS_STRING_SIZE);
    otbrLogWarning("%s:%d - %s - destination %s, error: %s.", __FILE__, __LINE__, __func__, buffer,
                   otbrErrorString(error));

    return error;
}

otbrError NetworkDiagHandler::handleNetworkDiscoveryRequest(std::string aDestination, std::string aRelationshipType)
{
    otbrError error = OTBR_ERROR_NONE;

    if (aDestination.empty() && (aRelationshipType == gDevicesCollection.getCollectionName()) &&
        (mRequestState == RequestState::kIdle))
    {
        mRequestState = RequestState::kWaiting;
        otbrLogWarning("%s:%d - %s - changed to state %d.", __FILE__, __LINE__, __func__, mRequestState);

        SetDefaultTlvs();

        // run network discovery and collect pre-defined TLVs
        error             = startDiscovery();
        mRelationshipType = aRelationshipType;
    }
    else
    {
        error = OTBR_ERROR_INVALID_STATE;
    }

    return error;
}

otbrError NetworkDiagHandler::startDiscovery()
{
    otbrError error = OTBR_ERROR_NONE;

    otIp6Address ip6address;
    memcpy(&ip6address, otThreadGetRloc(mInstance), sizeof(otIp6Address));

    switch (mDiagQueryRequestState)
    {
    case RequestState::kIdle:
        // init or remove outdated entries
        // learn and update router rloc16s in mDiagSet
        ResetRouterDiag(true);
        ResetChildDiag(mMaxAge);

        // collect fresh info and send diagnostic requests to all routers
        for (const auto &it : mDiagSet)
        {
            ip6address.mFields.m16[7] = htons(it.first);
            otbrLogWarning("%s:%d - %s - send DiagReq to 0x%04x.", __FILE__, __LINE__, __func__, it.first);
            VerifyOrExit(otThreadSendDiagnosticGet(mInstance, &ip6address, mDiagReqTlvs, mDiagReqTlvsCount,
                                                   NetworkDiagHandler::DiagnosticResponseHandler,
                                                   this) == OT_ERROR_NONE,
                         error = OTBR_ERROR_REST);
        }

        // init or remove outdated entries
        ResetChildTables(true);
        ResetChildIp6Addrs(true);
        ResetRouterNeighbors(true);

        // we skip waiting for DiagReq responses, as we do already have the router rloc16s
        mDiagQueryRequestState = RequestState::kPending;
        otbrLogWarning("%s:%d - %s - changed to DiagQuery state %d.", __FILE__, __LINE__, __func__,
                       mDiagQueryRequestState);
        // give time for responses comming in and continue on next callback
    default:
        ExitNow();
    }

exit:
    return error;
}

otbrError NetworkDiagHandler::continueHandleRequest()
{
    otbrError error    = OTBR_ERROR_NONE;
    bool      complete = true;
    bool      timeout  = false;

    // rloc16 destination
    otIp6Address ip6address;
    memcpy(&ip6address, otThreadGetRloc(mInstance), sizeof(otIp6Address));

    VerifyOrExit(mTimeout > steady_clock::now(), timeout = true);

    switch (mDiagQueryRequestState)
    {
    case RequestState::kIdle:
        break;
    case RequestState::kWaiting:
        // in case of unknown rloc16, we need to wait for the DiagReq response first.
        if (((mTimeLastAttempt + milliseconds(kDiagRetryDelayFTD)) < steady_clock::now()) && (mMaxRetries <= mRetries))
        {
            timeout = true;
        }
        if ((mTimeLastAttempt + milliseconds(kDiagRetryDelayFTD)) < steady_clock::now())
        {
            // retry
            mRetries += 1;
            mTimeLastAttempt = steady_clock::now();
            otbrLogWarning("%s:%d - %s - retry send DiagReq.", __FILE__, __LINE__, __func__);
            VerifyOrExit(otThreadSendDiagnosticGet(mInstance, &mIp6address, mDiagReqTlvs, mDiagReqTlvsCount,
                                                   NetworkDiagHandler::DiagnosticResponseHandler,
                                                   this) == OT_ERROR_NONE,
                         error = OTBR_ERROR_REST);
        }
        complete = false;
        break;
    case RequestState::kPending:
        VerifyOrExit(HandleNextDiagQuery(), complete = false);
        mDiagQueryRequestState = RequestState::kDone;
        otbrLogWarning("%s:%d - %s - changed to DiagQuery state %d.", __FILE__, __LINE__, __func__,
                       mDiagQueryRequestState);
        OT_FALL_THROUGH;
    case RequestState::kDone:
        // check if we have FTD children = REEDs
        if (mRelationshipType == gDevicesCollection.getCollectionName())
        {
            // we want to learn / update stable address also from REEDs
            // and get its ML-EID-IID, OMR and hostname
            for (auto &parent : mChildTables)
            {
                for (auto &child : parent.second.mChildTable)
                {
                    if (child.mDeviceTypeFtd && (mDiagSet.find(child.mRloc16) == mDiagSet.end()))
                    {
                        // prepare placeholder for expected result from REEDs
                        otbrLogWarning("%s:%d - %s - have REED 0x%04x.", __FILE__, __LINE__, __func__, child.mRloc16);
                        DiagInfo info           = {};
                        mDiagSet[child.mRloc16] = info;
                        mRetries                = 0;
                        complete                = false;
                    }
                }
            }
        }

        // check we have at least response from every node
        // including from the eventually found rx-on-when-idle children
        // otherwise start retries
        if (((mTimeLastAttempt + milliseconds(kDiagRetryDelayFTD)) < steady_clock::now()) && (mMaxRetries <= mRetries))
        {
            timeout = true;
        }
        if ((mTimeLastAttempt + milliseconds(kDiagRetryDelayFTD)) < steady_clock::now())
        {
            // retry
            mRetries += 1;
            mTimeLastAttempt = steady_clock::now();
            for (const auto &it : mDiagSet)
            {
                if (it.second.mDiagContent.empty()) // TODO: check all expected TLVs are present
                {
                    complete                  = false;
                    ip6address.mFields.m16[7] = htons(it.first);
                    otbrLogWarning("%s:%d - %s - retry send DiagReq to 0x%04x.", __FILE__, __LINE__, __func__,
                                   it.first);
                    VerifyOrExit(otThreadSendDiagnosticGet(mInstance, &ip6address, mDiagReqTlvs, mDiagReqTlvsCount,
                                                           NetworkDiagHandler::DiagnosticResponseHandler,
                                                           this) == OT_ERROR_NONE,
                                 error = OTBR_ERROR_REST);
                }
            }
        }

        // check if we already have responses to all retries
        VerifyOrExit(complete);
        for (const auto &it : mDiagSet)
        {
            if (it.second.mDiagContent.empty())
            {
                complete = false;
                break;
            }
        }

    default:
        // busy, retry on next callback
        break;
    }

exit:
    if (error == OTBR_ERROR_NONE)
    {
        if (mActionTask != nullptr)
        {
            if (timeout)
            {
                mActionTask->status = ACTIONS_TASK_STATUS_STOPPED;
            }
            if (complete)
            {
                mActionTask->status = ACTIONS_TASK_STATUS_COMPLETED;
            }
        }

        if ((complete || timeout) && (mRequestState != RequestState::kIdle))
        {
            if (mRelationshipType == gDevicesCollection.getCollectionName())
            {
                FillDeviceCollection();
            }
            else if (mRelationshipType == gDiagnosticsCollection.getCollectionName())
            {
                FillDiagnosticCollection();
            }
            mRelationshipType      = "";
            mActionTask            = nullptr;
            mRequestState          = RequestState::kIdle;
            mDiagQueryRequestState = RequestState::kIdle;
            otbrLogWarning("%s:%d - %s - changed to state %d.", __FILE__, __LINE__, __func__, mRequestState);
            if (mCallback != nullptr)
            {
                mCallback();
            }
        }

        if (timeout)
        {
            error = OTBR_ERROR_ABORTED;
        }
        else if (!complete)
        {
            // indicate result pending
            error = OTBR_ERROR_ERRNO;
        }
    }
    else
    {
        otbrLogWarning("%s:%d - %s - otbr error: %s.", __FILE__, __LINE__, __func__, otbrErrorString(error));
    }
    return error;
}

// start unicast NetworkDiagnostic requests
otbrError NetworkDiagHandler::handleNetworkDiagnosticsAction(task_node_t *aTaskNode)
{
    otbrError                      error = OTBR_ERROR_NONE;
    otbr::rest::NetworkDiagnostics deviceDiag;

    cJSON *task       = aTaskNode->task;
    cJSON *task_type  = cJSON_GetObjectItemCaseSensitive(task, "type");
    cJSON *attributes = cJSON_GetObjectItemCaseSensitive(task, "attributes");

    cJSON *destination = cJSON_GetObjectItemCaseSensitive(attributes, ATTRIBUTE_DESTINATION);
    cJSON *types       = cJSON_GetObjectItemCaseSensitive(attributes, ATTRIBUTE_TYPES);

    // we only run a single diagnostic request or query simultaneously
    VerifyOrExit(mRequestState == RequestState::kIdle, error = OTBR_ERROR_INVALID_STATE);
    mRequestState     = RequestState::kWaiting;
    aTaskNode->status = ACTIONS_TASK_STATUS_ACTIVE;
    otbrLogWarning("%s:%d - %s - changed to state %d.", __FILE__, __LINE__, __func__, mRequestState);

    if (std::string(task_type->valuestring) == std::string(taskNameNetworkDiagnostic))
    {
        // results should go into api/diagnostics collection
        mRelationshipType = gDiagnosticsCollection.getCollectionName();
    }
    // else if (std::string(task_type->valuestring) == std::string(taskNameUpdateDeviceCollection))
    // {
    //     // results should go into api/devices collection
    //     mRelationshipType = gDevicesCollection.getCollectionName();
    //     SetDefaultTlvs();
    // }

    // keep a reference to the task to add relationship details when done.
    mActionTask = aTaskNode;

    VerifyOrExit((error = extractTlvSet(types)) == OTBR_ERROR_NONE);

    otbrLogWarning("%s:%d - %s - Following tlv types will be requested: %s from %s", __FILE__, __LINE__, __func__,
                   cJSON_Print(types), cJSON_Print(destination));

    if (strlen(destination->valuestring) == 0)
    {
        // no destination given, so we discover the network
        error = startDiscovery();
        ExitNow();
    }
    else // unicast requests to a single destination
    {
        // remove all previous entries
        ResetRouterDiag(false);
        ResetChildDiag(steady_clock::now());

        ResetChildTables(false);
        ResetChildIp6Addrs(false);
        ResetRouterNeighbors(false);

        VerifyOrExit((error = LookupDestinationAddr(std::string(destination->valuestring), &mIp6address)) ==
                     OTBR_ERROR_NONE);

        mRetries               = 0;
        mDiagQueryRequestState = RequestState::kWaiting;
        otbrLogWarning("%s:%d - %s - changed to DiagQuery state %d.", __FILE__, __LINE__, __func__,
                       mDiagQueryRequestState);
        VerifyOrExit(otThreadSendDiagnosticGet(mInstance, &mIp6address, mDiagReqTlvs, mDiagReqTlvsCount,
                                               NetworkDiagHandler::DiagnosticResponseHandler, this) == OT_ERROR_NONE,
                     error = OTBR_ERROR_REST);
        // wait for response
    }

exit:
    if (error != OTBR_ERROR_NONE)
    {
        if (error == OTBR_ERROR_ABORTED)
        {
            otbrLogWarning("%s:%d - %s - TIMEOUT -> set a valid timeout.", __FILE__, __LINE__, __func__);
        }
        else if (error != OTBR_ERROR_INVALID_STATE)
        {
            // something went wrong clear internal state to run another network diagnostic action
            mRequestState          = RequestState::kIdle;
            mDiagQueryRequestState = RequestState::kIdle;
            // aTaskNode->status      = ACTIONS_TASK_STATUS_FAILED;
            // otbrLogWarning("%s:%d - %s - changed to state %d, error: %s.", __FILE__, __LINE__, __func__,
            // mRequestState, otbrErrorString(error));
        }
    }
    return error;
}

otbrError NetworkDiagHandler::extractTlvSet(cJSON *aTypes)
{
    std::string tlvStr;
    cJSON      *item = nullptr;

    otbrError error = OTBR_ERROR_NONE;
    uint8_t   tlvType;
    bool      rlocRequested;

    mDiagQueryTlvs.clear();

    cJSON_ArrayForEach(item, aTypes)
    {
        // loop for each item
        if (cJSON_IsString(item) && mDiagReqTlvsCount < MAX_TLV_COUNT)
        {
            tlvStr  = std::string(item->valuestring);
            tlvType = kTlvTypeMap.at(tlvStr);

            if (tlvType < 29 || tlvType > 33) // diagnostic requests
            {
                if (tlvType == OT_NETWORK_DIAGNOSTIC_TLV_SHORT_ADDRESS)
                {
                    rlocRequested = true;
                }

                mDiagReqTlvs[mDiagReqTlvsCount++] = kTlvTypeMap.at(tlvStr);
            }
            else // diagnostic queries
            {
                switch (tlvType)
                {
                case OT_NETWORK_DIAGNOSTIC_TLV_CHILD:
                    mDiagQueryTlvs.emplace_back(OT_NETWORK_DIAGNOSTIC_TLV_CHILD);
                    break;

                case OT_NETWORK_DIAGNOSTIC_TLV_CHILD_IP6_ADDR_LIST:
                    mDiagQueryTlvs.emplace_back(OT_NETWORK_DIAGNOSTIC_TLV_CHILD_IP6_ADDR_LIST);
                    break;

                case OT_NETWORK_DIAGNOSTIC_TLV_ROUTER_NEIGHBOR:
                    mDiagQueryTlvs.emplace_back(OT_NETWORK_DIAGNOSTIC_TLV_ROUTER_NEIGHBOR);
                    break;

                default:
                    error = OTBR_ERROR_INVALID_ARGS;
                }
            }
        }
    }
    if (!rlocRequested)
    {
        mDiagReqTlvs[mDiagReqTlvsCount++] = kTlvTypeMap.at(KEY_RLOC16);
    }
    return error;
}

void NetworkDiagHandler::AddSingleRloc16LookUp(uint16_t aRloc16)
{
    if ((aRloc16 & 0x1FF) == 0)
    {
        // destination is router and we may want DiagQuery TLVs
        RouterChildTable infoCt{};
        mChildTables[aRloc16] = infoCt;

        RouterChildIp6Addrs infoIp{};
        mChildIps[aRloc16] = infoIp;

        RouterNeighbors infoR{};
        mRouterNeighbors[aRloc16] = infoR;
    }
}

void NetworkDiagHandler::ResetRouterDiag(bool aLearnRloc16)
{
    for (uint16_t id = 0; id <= OT_NETWORK_MAX_ROUTER_ID; id++)
    {
        uint16_t     rloc = id << 10;
        otRouterInfo routerInfo;

        if ((otThreadGetRouterInfo(mInstance, rloc, &routerInfo) == OT_ERROR_NONE) && aLearnRloc16)
        {
            if (mDiagSet.find(rloc) == mDiagSet.end())
            {
                DiagInfo info;
                mDiagSet[rloc] = info;
            }
        }
        else
        {
            if (mDiagSet.erase(rloc) > 0)
            {
                otbrLogWarning("%s:%d Deleted outdated router diag from 0x%04x", __FILE__, __LINE__, rloc);
            }
        }
    }
}

void NetworkDiagHandler::ResetChildDiag(steady_clock::time_point aMaxAge)
{
    // reset entries that are not a router rloc
    // delete empty entries or entries older than mMaxAge
    std::vector<uint16_t> remove;
    for (const auto &it : mDiagSet)
    {
        if ((it.first & 0x1FF) > 0)
        {
            // from child
            if (it.second.mStartTime < aMaxAge)
            {
                remove.emplace_back(it.first);
            }
        }
    }
    for (const auto &item : remove)
    {
        mDiagSet.erase(item);
        otbrLogWarning("%s:%d Deleted outdated child diag from 0x%04x", __FILE__, __LINE__, item);
    }
}

void NetworkDiagHandler::ResetChildTables(bool aLearnRloc16)
{
    for (uint16_t id = 0; id <= OT_NETWORK_MAX_ROUTER_ID; id++)
    {
        uint16_t     rloc = id << 10;
        otRouterInfo routerInfo;

        if ((otThreadGetRouterInfo(mInstance, rloc, &routerInfo) == OT_ERROR_NONE) && aLearnRloc16)
        {
            if (mChildTables.find(rloc) == mChildTables.end())
            {
                RouterChildTable info{};
                mChildTables[rloc] = info;
            }
            else
            {
                mChildTables[rloc].mChildTable.clear();
            }
        }
        else
        {
            mChildTables.erase(rloc);
        }
    }
}

void NetworkDiagHandler::ResetChildIp6Addrs(bool aLearnRloc16)
{
    for (uint16_t id = 0; id <= OT_NETWORK_MAX_ROUTER_ID; id++)
    {
        uint16_t     rloc = id << 10;
        otRouterInfo routerInfo;

        if ((otThreadGetRouterInfo(mInstance, rloc, &routerInfo) == OT_ERROR_NONE) && aLearnRloc16)
        {
            if (mChildIps.find(rloc) == mChildIps.end())
            {
                RouterChildIp6Addrs info{};
                mChildIps[rloc] = info;
            }
            else
            {
                mChildIps[rloc].mChildren.clear();
            }
        }
        else
        {
            mChildIps.erase(rloc);
        }
    }
}

void NetworkDiagHandler::ResetRouterNeighbors(bool aLearnRloc16)
{
    for (uint16_t id = 0; id <= OT_NETWORK_MAX_ROUTER_ID; id++)
    {
        uint16_t     rloc = id << 10;
        otRouterInfo routerInfo;

        if ((otThreadGetRouterInfo(mInstance, rloc, &routerInfo) == OT_ERROR_NONE) && aLearnRloc16)
        {
            if (mRouterNeighbors.find(rloc) == mRouterNeighbors.end())
            {
                RouterNeighbors info{};
                mRouterNeighbors[rloc] = info;
            }
            else
            {
                mRouterNeighbors[rloc].mNeighbors.clear();
            }
        }
        else
        {
            mRouterNeighbors.erase(rloc);
        }
    }
}

void NetworkDiagHandler::UpdateDiag(uint16_t aKey, std::vector<otNetworkDiagTlv> &aDiag)
{
    DiagInfo value;
    value.mStartTime = steady_clock::now();

    bool                                    replaced;
    std::vector<otNetworkDiagTlv>::iterator newit;

    // Check if mDiagSet contains aKey and if mDiagContent is not empty
    auto it = mDiagSet.find(aKey);
    if (it != mDiagSet.end() && !it->second.mDiagContent.empty())
    {
        auto &existingDiag = mDiagSet[aKey].mDiagContent;
        // we expect this called multiple times.
        // Thus we only update the tlvs in aDiag ...
        for (auto &existingTlv : existingDiag)
        {
            replaced = false;
            for (newit = aDiag.begin(); newit != aDiag.end(); ++newit)
            {
                if (existingTlv.mType == newit->mType)
                {
                    value.mDiagContent.push_back(*newit); // update existing TLV
                    replaced = true;
                    break;
                }
            }
            if (replaced)
            {
                aDiag.erase(newit); // remove processed TLV
            }
            else
            {
                value.mDiagContent.push_back(existingTlv); // retain old TLV
            }
        }
    }
    if (it == mDiagSet.end())
    {
        // we have a single unicast request and may want to also get DiagQuery TLVs
        AddSingleRloc16LookUp(aKey);
    }
    // Add remaining new TLVs that weren't present in the original set
    value.mDiagContent.insert(value.mDiagContent.end(), aDiag.begin(), aDiag.end());

    mDiagSet[aKey] = value;
}

bool NetworkDiagHandler::HandleNextDiagQuery()
{
    for (auto &query_tlv : mDiagQueryTlvs)
    {
        switch (query_tlv)
        {
        case OT_NETWORK_DIAGNOSTIC_TLV_CHILD:
            for (auto &item : mChildTables)
            {
                VerifyOrExit(RequestChildTable(item.first, mMaxAge, &item.second));
            }
            break;
        case OT_NETWORK_DIAGNOSTIC_TLV_CHILD_IP6_ADDR_LIST:
            for (auto &item : mChildIps)
            {
                VerifyOrExit(RequestChildIp6Addrs(item.first, mMaxAge, &item.second));
            }
            break;
        case OT_NETWORK_DIAGNOSTIC_TLV_ROUTER_NEIGHBOR:
            for (auto &item : mRouterNeighbors)
            {
                VerifyOrExit(RequestRouterNeighbors(item.first, mMaxAge, &item.second));
            }
            break;
        default:
            break;
        }
    }
    return true;
exit:
    return false;
}

void NetworkDiagHandler::DiagnosticResponseHandler(otError              aError,
                                                   otMessage           *aMessage,
                                                   const otMessageInfo *aMessageInfo,
                                                   void                *aContext)
{
    static_cast<NetworkDiagHandler *>(aContext)->DiagnosticResponseHandler(aError, aMessage, aMessageInfo);
}

void NetworkDiagHandler::DiagnosticResponseHandler(otError              aError,
                                                   const otMessage     *aMessage,
                                                   const otMessageInfo *aMessageInfo)
{
    std::vector<otNetworkDiagTlv> diagSet;
    otNetworkDiagTlv              diagTlv;
    otNetworkDiagIterator         iterator = OT_NETWORK_DIAGNOSTIC_ITERATOR_INIT;
    otError                       error;
    uint16_t                      keyRloc = 0xfffe;

    SuccessOrExit(aError);

    OTBR_UNUSED_VARIABLE(aMessageInfo);

    while ((error = otThreadGetNextDiagnosticTlv(aMessage, &iterator, &diagTlv)) == OT_ERROR_NONE)
    {
        if (diagTlv.mType == OT_NETWORK_DIAGNOSTIC_TLV_SHORT_ADDRESS)
        {
            keyRloc = diagTlv.mData.mAddr16;
        }
        diagSet.push_back(diagTlv);
    }
    VerifyOrExit(keyRloc != 0xfffe, aError = OT_ERROR_FAILED);
    UpdateDiag(keyRloc, diagSet);

    if (mDiagQueryRequestState == RequestState::kWaiting)
    {
        mDiagQueryRequestState = RequestState::kPending;
        otbrLogWarning("%s:%d - %s - changed to DiagQuery state %d.", __FILE__, __LINE__, __func__,
                       mDiagQueryRequestState);
    }

exit:
    if (aError != OT_ERROR_NONE)
    {
        otbrLogWarning("%s:%d Failed to get diagnostic data: %s", __FILE__, __LINE__, otThreadErrorToString(aError));
    }
    continueHandleRequest();
}

bool NetworkDiagHandler::RequestChildTable(uint16_t                 aRloc16,
                                           steady_clock::time_point aMaxAge,
                                           RouterChildTable        *aChildTable)
{
    bool    retval = false;
    otError aError;

    switch (aChildTable->mState)
    {
    case RequestState::kIdle:
    case RequestState::kDone:
        // Check if we can use the cached results
        if (aChildTable->mUpdateTime > aMaxAge)
        {
            retval = true;
            break;
        }
        aChildTable->mState = RequestState::kWaiting;
        OT_FALL_THROUGH;
    case RequestState::kWaiting:
        aError = otMeshDiagQueryChildTable(mInstance, aRloc16, NetworkDiagHandler::MeshChildTableResponseHandler, this);

        switch (aError)
        {
        case OT_ERROR_NONE:
            mDiagQueryRequestRloc = aRloc16;
            aChildTable->mState   = RequestState::kPending;
            break;

        case OT_ERROR_BUSY:
        case OT_ERROR_NO_BUFS:
        case OT_ERROR_INVALID_ARGS:
            otbrLogWarning("%s:%d Failed to get diagnostic data: %s", __FILE__, __LINE__,
                           otThreadErrorToString(aError));
            break;

        default:
            aChildTable->mState = RequestState::kDone;
            retval              = true;
            break;
        }
        break;

    case RequestState::kPending:
        break;
    }

    return retval;
}

void NetworkDiagHandler::MeshChildTableResponseHandler(otError                     aError,
                                                       const otMeshDiagChildEntry *aChildEntry,
                                                       void                       *aContext)
{
    static_cast<NetworkDiagHandler *>(aContext)->MeshChildTableResponseHandler(aError, aChildEntry);
}

void NetworkDiagHandler::MeshChildTableResponseHandler(otError aError, const otMeshDiagChildEntry *aChildEntry)
{
    auto it = mChildTables.find(mDiagQueryRequestRloc);

    VerifyOrExit(it != mChildTables.end());
    VerifyOrExit(it->second.mState == RequestState::kPending);

    if (aError == OT_ERROR_NONE || aError == OT_ERROR_RESPONSE_TIMEOUT)
    {
        it->second.mUpdateTime = steady_clock::now();
        it->second.mState      = RequestState::kDone;
        continueHandleRequest();
    }

    VerifyOrExit(aChildEntry != nullptr);

    it->second.mChildTable.emplace_back(*aChildEntry);

    // otbrLogWarning("%s:%d Have child table from 0x%04x for child 0x%04x", __FILE__, __LINE__, mDiagQueryRequestRloc,
    //                 aChildEntry->mRloc16);

exit:
    return;
}

bool NetworkDiagHandler::RequestChildIp6Addrs(uint16_t                 aParentRloc16,
                                              steady_clock::time_point aMaxAge,
                                              RouterChildIp6Addrs     *aChild)
{
    bool    retval = false;
    otError aError;

    switch (aChild->mState)
    {
    case RequestState::kIdle:
    case RequestState::kDone:
        // Check if we can use the cached results
        if (aChild->mUpdateTime > aMaxAge)
        {
            retval = true;
            break;
        }
        aChild->mState = RequestState::kWaiting;
        OT_FALL_THROUGH;
    case RequestState::kWaiting:
        aError = otMeshDiagQueryChildrenIp6Addrs(mInstance, aParentRloc16,
                                                 NetworkDiagHandler::MeshChildIp6AddrResponseHandler, this);
        switch (aError)
        {
        case OT_ERROR_NONE:
            mDiagQueryRequestRloc = aParentRloc16;
            aChild->mState        = RequestState::kPending;
            break;

        case OT_ERROR_BUSY:
        case OT_ERROR_NO_BUFS:
        case OT_ERROR_INVALID_ARGS:
            otbrLogWarning("%s:%d Failed to get diagnostic data: %s", __FILE__, __LINE__,
                           otThreadErrorToString(aError));
            break;

        default:
            aChild->mState = RequestState::kDone;
            retval         = true;
            break;
        }
        break;

    case RequestState::kPending:
        break;
    }

    return retval;
}

void NetworkDiagHandler::MeshChildIp6AddrResponseHandler(otError                    aError,
                                                         uint16_t                   aChildRloc16,
                                                         otMeshDiagIp6AddrIterator *aIp6AddrIterator,
                                                         void                      *aContext)
{
    static_cast<NetworkDiagHandler *>(aContext)->MeshChildIp6AddrResponseHandler(aError, aChildRloc16,
                                                                                 aIp6AddrIterator);
}

void NetworkDiagHandler::MeshChildIp6AddrResponseHandler(otError                    aError,
                                                         uint16_t                   aChildRloc16,
                                                         otMeshDiagIp6AddrIterator *aIp6AddrIterator)
{
    DeviceIp6Addrs new_device;
    otIp6Address   ip6Address;
    auto           it = mChildIps.find(mDiagQueryRequestRloc);

    VerifyOrExit(aError == OT_ERROR_NONE || aError == OT_ERROR_PENDING);
    VerifyOrExit(aIp6AddrIterator != nullptr);
    VerifyOrExit(aChildRloc16 != 65534);

    VerifyOrExit(it != mChildIps.end());
    VerifyOrExit(it->second.mState == RequestState::kPending);

    // otbrLogWarning("%s:%d Have child IP from 0x%04x for child 0x%04x", __FILE__, __LINE__, mDiagQueryRequestRloc,
    //                aChildRloc16);
    new_device.mRloc16 = aChildRloc16;

    while (otMeshDiagGetNextIp6Address(aIp6AddrIterator, &ip6Address) == OT_ERROR_NONE)
    {
        new_device.mIp6Addrs.emplace_back(ip6Address);
    }

    it->second.mChildren.emplace_back(new_device);

exit:
    if (aError == OT_ERROR_NONE || aError == OT_ERROR_RESPONSE_TIMEOUT)
    {
        it->second.mUpdateTime = steady_clock::now();
        it->second.mState      = RequestState::kDone;
        continueHandleRequest();
    }
    return;
}

bool NetworkDiagHandler::RequestRouterNeighbors(uint16_t                 aRloc16,
                                                steady_clock::time_point aMaxAge,
                                                RouterNeighbors         *aRouterNeighbor)
{
    bool    retval = false;
    otError aError;

    switch (aRouterNeighbor->mState)
    {
    case RequestState::kIdle:
    case RequestState::kDone:
        // Check if we can use the cached results
        if (aRouterNeighbor->mUpdateTime > aMaxAge)
        {
            retval = true;
            break;
        }
        aRouterNeighbor->mState = RequestState::kWaiting;
        OT_FALL_THROUGH;
    case RequestState::kWaiting:
        aError = otMeshDiagQueryRouterNeighborTable(mInstance, aRloc16,
                                                    NetworkDiagHandler::MeshRouterNeighborsResponseHandler, this);
        switch (aError)
        {
        case OT_ERROR_NONE:
            mDiagQueryRequestRloc   = aRloc16;
            aRouterNeighbor->mState = RequestState::kPending;
            break;

        case OT_ERROR_BUSY:
        case OT_ERROR_NO_BUFS:
        case OT_ERROR_INVALID_ARGS:
            otbrLogWarning("%s:%d Failed to get diagnostic data: %s", __FILE__, __LINE__,
                           otThreadErrorToString(aError));
            break;

        default:
            aRouterNeighbor->mState = RequestState::kDone;
            retval                  = true;
            break;
        }
        break;

    case RequestState::kPending:
        break;
    }

    return retval;
}

void NetworkDiagHandler::MeshRouterNeighborsResponseHandler(otError                              aError,
                                                            const otMeshDiagRouterNeighborEntry *aNeighborEntry,
                                                            void                                *aContext)
{
    static_cast<NetworkDiagHandler *>(aContext)->MeshRouterNeighborsResponseHandler(aError, aNeighborEntry);
}

void NetworkDiagHandler::MeshRouterNeighborsResponseHandler(otError                              aError,
                                                            const otMeshDiagRouterNeighborEntry *aNeighborEntry)
{
    auto it = mRouterNeighbors.find(mDiagQueryRequestRloc);

    VerifyOrExit(it->second.mState == RequestState::kPending);

    if (aError == OT_ERROR_NONE || aError == OT_ERROR_RESPONSE_TIMEOUT)
    {
        it->second.mUpdateTime = steady_clock::now();
        it->second.mState      = RequestState::kDone;
        continueHandleRequest();
    }

    VerifyOrExit(aNeighborEntry != nullptr);

    it->second.mNeighbors.emplace_back(*aNeighborEntry);

exit:
    return;
}

void NetworkDiagHandler::SetDeviceItemAttributes(std::string aExtAddr, DeviceInfo &aDeviceInfo)
{
    // if this device`s extAddr equals aDeviceInfo.extaddr
    // add a item of type ThisThreadDevice and set also nodeInfo
    // otherwise add a generic item of type ThreadDevice to the collection of devices

    const otExtAddress *thisextaddr = otLinkGetExtendedAddress(mInstance);
    std::string         thisextaddr_str;

    char hex[2 * OT_EXT_ADDRESS_SIZE + 1];
    otbr::Utils::Bytes2Hex(thisextaddr->m8, OT_EXT_ADDRESS_SIZE, hex);
    hex[2 * OT_EXT_ADDRESS_SIZE] = '\0';
    thisextaddr_str              = std::string(StringUtils::ToLowercase(hex));

    otRouterInfo routerInfo;
    uint8_t      maxRouterId;

    if (gDevicesCollection.getItem(aExtAddr) == nullptr)
    {
        aDeviceInfo.mNeedsUpdate = !isDeviceComplete(aDeviceInfo);
        if (aDeviceInfo.mNeedsUpdate)
        {
            otbrLogWarning("%s:%d lacking some attributes for deviceId %s", __FILE__, __LINE__, aExtAddr.c_str());
        }

        if (thisextaddr_str == aExtAddr)
        {
            // create ThisThreadDevice with additional NodeInfo
            ThisThreadDevice thisItem = ThisThreadDevice(aExtAddr);

            otBorderAgentGetId(mInstance, &thisItem.mNodeInfo.mBaId);
            thisItem.mNodeInfo.mBaState = otBorderAgentGetState(mInstance);
            otThreadGetLeaderData(mInstance, &thisItem.mNodeInfo.mLeaderData);

            thisItem.mNodeInfo.mNumOfRouter = 0;
            maxRouterId                     = otThreadGetMaxRouterId(mInstance);
            for (uint8_t i = 0; i <= maxRouterId; ++i)
            {
                if (otThreadGetRouterInfo(mInstance, i, &routerInfo) != OT_ERROR_NONE)
                {
                    continue;
                }
                ++thisItem.mNodeInfo.mNumOfRouter;
            }

            thisItem.mNodeInfo.mRole        = GetDeviceRoleName(otThreadGetDeviceRole(mInstance));
            thisItem.mNodeInfo.mExtAddress  = reinterpret_cast<const uint8_t *>(otLinkGetExtendedAddress(mInstance));
            thisItem.mNodeInfo.mNetworkName = otThreadGetNetworkName(mInstance);
            thisItem.mNodeInfo.mRloc16      = otThreadGetRloc16(mInstance);
            thisItem.mNodeInfo.mExtPanId    = reinterpret_cast<const uint8_t *>(otThreadGetExtendedPanId(mInstance));
            thisItem.mNodeInfo.mRlocAddress = *otThreadGetRloc(mInstance);

            thisItem.mDeviceInfo = aDeviceInfo;
            // GetLocalCounters(&thisItem);
            gDevicesCollection.addItem(&thisItem);
        }
        else
        {
            // create a general ThreadDevice
            ThreadDevice generalItem = ThreadDevice(aExtAddr);
            generalItem.mDeviceInfo  = aDeviceInfo;
            gDevicesCollection.addItem(&generalItem);
        }
    }
    else // update existing deviceItem
    {
        ThreadDevice *item = dynamic_cast<ThreadDevice *>(gDevicesCollection.getItem(aExtAddr));
        if (item)
        {
            // check eui64 value is valid before updating it
            if (!isOtExtAddrEmpty(aDeviceInfo.mEui64))
            {
                item->setEui64(aDeviceInfo.mEui64);
                otbrLogWarning("%s:%d updated eui64 for deviceId %s", __FILE__, __LINE__, aExtAddr.c_str());
            }

            // check ipv6 value is valid before updating it
            if (!isOtIp6AddrEmpty(aDeviceInfo.mIp6Addr))
            {
                item->setIpv6Omr(aDeviceInfo.mIp6Addr);
                otbrLogWarning("%s:%d updated ipv6 for deviceId %s", __FILE__, __LINE__, aExtAddr.c_str());
            }

            // check mleidiid value is valid before updating it
            if (!isOtExtAddrEmpty(aDeviceInfo.mMlEidIid))
            {
                item->setMlEidIid(aDeviceInfo.mMlEidIid);
                otbrLogWarning("%s:%d updated mlEidIid for deviceId %s", __FILE__, __LINE__, aExtAddr.c_str());
            }
            // update hostname
            if (aDeviceInfo.mHostName.size() > 0)
            {
                item->setHostname(aDeviceInfo.mHostName);
            }
            // update role
            if (aDeviceInfo.mRole.size() > 0)
            {
                item->setRole(aDeviceInfo.mRole);
            }
            // update mode
            if ((aDeviceInfo.mode.mRxOnWhenIdle != item->mDeviceInfo.mode.mRxOnWhenIdle) ||
                (aDeviceInfo.mode.mDeviceType != item->mDeviceInfo.mode.mDeviceType))
            {
                item->setMode(aDeviceInfo.mode);
            }
        }
        else
        {
            otbrLogWarning("%s:%d error : dynamic_cast failed.", __FILE__, __LINE__);
        }
    }
}

void NetworkDiagHandler::GetChildren(const uint16_t &aParentRloc16)
{
    DeviceInfo  deviceInfo;
    std::string extAddr;

    std::vector<otMeshDiagChildEntry> child_table     = mChildTables.find(aParentRloc16)->second.mChildTable;
    std::vector<DeviceIp6Addrs>       child_ip6_lists = mChildIps.find(aParentRloc16)->second.mChildren;

    for (const auto &item : child_table)
    {
        deviceInfo.mMlEidIid    = {{0}};
        deviceInfo.mEui64       = {{0}};
        deviceInfo.mIp6Addr     = {{{0}}};
        deviceInfo.mHostName    = "";
        deviceInfo.mRole        = "child";
        deviceInfo.mNeedsUpdate = true;

        deviceInfo.mode.mDeviceType   = item.mDeviceTypeFtd;
        deviceInfo.mode.mRxOnWhenIdle = item.mRxOnWhenIdle;
        deviceInfo.mode.mNetworkData  = item.mFullNetData;

        char hex[2 * OT_EXT_ADDRESS_SIZE + 1];
        otbr::Utils::Bytes2Hex(item.mExtAddress.m8, OT_EXT_ADDRESS_SIZE, hex);
        hex[2 * OT_EXT_ADDRESS_SIZE] = '\0';
        extAddr                      = std::string(StringUtils::ToLowercase(hex));

        otbrLogWarning("%s:%d - %s - %s", __FILE__, __LINE__, __func__, extAddr.c_str());

        memcpy(deviceInfo.mExtAddress.m8, item.mExtAddress.m8, OT_EXT_ADDRESS_SIZE);

        // get the MTD child`s ipv6 addresses from the children ipv6address list
        for (const auto &device : child_ip6_lists)
        {
            if (device.mRloc16 == item.mRloc16)
            {
                for (uint16_t i = 0; i < device.mIp6Addrs.size(); ++i)
                {
                    // iterate through the device´s ipv6 adresses and
                    // extract OMR ipv6 address and MlEidIid
                    filterIpv6(deviceInfo, device.mIp6Addrs[i], otThreadGetMeshLocalPrefix(mInstance));
                }
                GetHostName(deviceInfo);
                break;
            }
        }
        if (!extAddr.empty())
        {
            SetDeviceItemAttributes(extAddr, deviceInfo);
        }
        else
        {
            otbrLogWarning("%s:%d error : missing extAddr", __FILE__, __LINE__);
        }
    }
}

void NetworkDiagHandler::SetDiagQueryTlvs(otbr::rest::NetworkDiagnostics *aDeviceDiag, const uint16_t &aParentRloc16)
{
    if (((aParentRloc16 & 0x1FF) == 0) && (mChildTables.find(aParentRloc16) != mChildTables.end()))
    {
        std::vector<otMeshDiagChildEntry>          child_table = mChildTables.find(aParentRloc16)->second.mChildTable;
        std::vector<DeviceIp6Addrs>                child_ip6_lists = mChildIps.find(aParentRloc16)->second.mChildren;
        std::vector<otMeshDiagRouterNeighborEntry> router_neighbors =
            mRouterNeighbors.find(aParentRloc16)->second.mNeighbors;

        aDeviceDiag->mChildren.assign(child_table.begin(), child_table.end());
        aDeviceDiag->mChildrenIp6Addrs.assign(child_ip6_lists.begin(), child_ip6_lists.end());
        aDeviceDiag->mNeighbors.assign(router_neighbors.begin(), router_neighbors.end());
    }
}

void NetworkDiagHandler::FillDeviceCollection(void)
{
    DeviceInfo  deviceInfo;
    std::string extAddr;

    for (auto &diag : mDiagSet)
    {
        if (diag.second.mDiagContent.empty())
        {
            otbrLogWarning("%s:%d error : no response from 0x%04x", __FILE__, __LINE__, diag.first);
            continue;
        }
        deviceInfo.mMlEidIid    = {{0}};
        deviceInfo.mEui64       = {{0}};
        deviceInfo.mIp6Addr     = {{{0}}};
        deviceInfo.mHostName    = "";
        deviceInfo.mRole        = "";
        deviceInfo.mNeedsUpdate = true;
        extAddr                 = "";

        for (const auto &diagTlv : diag.second.mDiagContent)
        {
            switch (diagTlv.mType)
            {
            case OT_NETWORK_DIAGNOSTIC_TLV_EXT_ADDRESS:

                char hex[2 * OT_EXT_ADDRESS_SIZE + 1];
                otbr::Utils::Bytes2Hex(diagTlv.mData.mExtAddress.m8, OT_EXT_ADDRESS_SIZE, hex);
                hex[2 * OT_EXT_ADDRESS_SIZE] = '\0';
                extAddr                      = StringUtils::ToLowercase(hex);

                memcpy(deviceInfo.mExtAddress.m8, diagTlv.mData.mExtAddress.m8, OT_EXT_ADDRESS_SIZE);
                break;

            case OT_NETWORK_DIAGNOSTIC_TLV_SHORT_ADDRESS:
                if ((diagTlv.mData.mAddr16 & 0x1FF) > 0)
                {
                    deviceInfo.mRole = "child";
                }
                else
                {
                    deviceInfo.mRole              = "router";
                    deviceInfo.mode.mDeviceType   = true;
                    deviceInfo.mode.mRxOnWhenIdle = true;
                    deviceInfo.mode.mNetworkData  = true;
                    deviceInfo.mNeedsUpdate       = false;
                    GetChildren(diagTlv.mData.mAddr16);
                }
                break;

            case OT_NETWORK_DIAGNOSTIC_TLV_EUI64:
                deviceInfo.mEui64 = diagTlv.mData.mEui64;
                break;

            case OT_NETWORK_DIAGNOSTIC_TLV_IP6_ADDR_LIST:
                for (uint16_t i = 0; i < diagTlv.mData.mIp6AddrList.mCount; ++i)
                {
                    // iterate through the device´s ipv6 adresses and
                    // extract OMR ipv6 address and MlEidIid
                    filterIpv6(deviceInfo, diagTlv.mData.mIp6AddrList.mList[i], otThreadGetMeshLocalPrefix(mInstance));
                }
                GetHostName(deviceInfo);
                break;

            default:
                break;
            }
        }

        if (!extAddr.empty())
        {
            SetDeviceItemAttributes(extAddr, deviceInfo);
        }
        else
        {
            otbrLogWarning("%s:%d error : missing extAddr", __FILE__, __LINE__);
        }
    }
}

void NetworkDiagHandler::FillDiagnosticCollection(void)
{
    const otExtAddress *thisExtAddr;

    for (auto &diag : mDiagSet)
    {
        if (diag.second.mDiagContent.empty())
        {
            otbrLogWarning("%s:%d error : no response from 0x%04x", __FILE__, __LINE__, diag.first);
            continue;
        }
        else
        {
            otbrLogWarning("%s:%d Have data from 0x%04x", __FILE__, __LINE__, diag.first);
        }

        // create a new diagnostic item
        otbr::rest::NetworkDiagnostics deviceDiag;

        // copy data to diagnostic item
        for (const auto &diagTlv : diag.second.mDiagContent)
        {
            switch (diagTlv.mType)
            {
            case OT_NETWORK_DIAGNOSTIC_TLV_EXT_ADDRESS:
                // if we have `this` node
                thisExtAddr = otLinkGetExtendedAddress(mInstance);
                if (std::memcmp(diagTlv.mData.mExtAddress.m8, thisExtAddr->m8, OT_EXT_ADDRESS_SIZE) == 0)
                {
                    // add BrCounters
                    GetLocalCounters(&deviceDiag);
                }
                break;
            case OT_NETWORK_DIAGNOSTIC_TLV_SHORT_ADDRESS:
                SetDiagQueryTlvs(&deviceDiag, diagTlv.mData.mAddr16);
                break;
            case OT_NETWORK_DIAGNOSTIC_TLV_IP6_ADDR_LIST:
                SetServiceRoleFlags(&deviceDiag, diagTlv);
                break;
            default:
                break;
            }

            deviceDiag.mDeviceTlvSet.push_back(diagTlv);
        }

        // store diagnostic item
        otbr::rest::gDiagnosticsCollection.addItem(&deviceDiag);

        // mActionTask keeps reference to result
        // TODO: keep reference to multiple result items
        snprintf(mActionTask->relationship.mType, MAX_TYPELENGTH,
                 otbr::rest::gDiagnosticsCollection.getCollectionName().c_str());
        snprintf(mActionTask->relationship.mId, UUID_STR_LEN, deviceDiag.mUuid.toString().c_str());
    }

    otbrLogWarning("%s:%d - %s - done", __FILE__, __LINE__, __func__);
}

void NetworkDiagHandler::GetHostName(DeviceInfo &aDeviceInfo)
{
    const otSrpServerHost *host = nullptr;
    const otIp6Address    *addresses;
    uint8_t                addressesNum;

    while ((host = otSrpServerGetNextHost(mInstance, host)) != nullptr)
    {
        if (otSrpServerHostIsDeleted(host))
        {
            continue;
        }

        addresses = otSrpServerHostGetAddresses(host, &addressesNum);

        for (uint8_t i = 0; i < addressesNum; ++i)
        {
            if (std::equal(std::begin(aDeviceInfo.mIp6Addr.mFields.m8), std::end(aDeviceInfo.mIp6Addr.mFields.m8),
                           std::begin(addresses[i].mFields.m8)))
            {
                std::string hostname(otSrpServerHostGetFullName(host));
                aDeviceInfo.mHostName = hostname.substr(0, hostname.find('.'));
                break;
            }
        }
    }
}

void NetworkDiagHandler::GetLocalCounters(otbr::rest::NetworkDiagnostics *aDeviceDiag)
{
    otbr::rest::networkDiagTlvExtensions localCounter;
    const otBorderRoutingCounters       *brCounters = otIp6GetBorderRoutingCounters(mInstance);

    localCounter.mType             = NETWORK_DIAGNOSTIC_TLVEXT_BR_COUNTER;
    localCounter.mData.mBrCounters = *brCounters;
    aDeviceDiag->mDeviceTlvSetExtension.push_back(localCounter);
}

void NetworkDiagHandler::SetServiceRoleFlags(otbr::rest::NetworkDiagnostics *aDeviceDiag, otNetworkDiagTlv aTlv)
{
    if (aTlv.mType != OT_NETWORK_DIAGNOSTIC_TLV_IP6_ADDR_LIST)
    {
        return;
    }
    otbr::rest::networkDiagTlvExtensions diagTlvExt;
    otIp6Address                         ipv6Addr;

    uint16_t              localRloc16 = otThreadGetRloc16(mInstance);
    otNetworkDataIterator iterator    = OT_NETWORK_DATA_ITERATOR_INIT;
    otExternalRouteConfig config;

    diagTlvExt.mType                                   = NETWORK_DIAGNOSTIC_TLVEXT_SERVICEROLEFLAGS;
    diagTlvExt.mData.mServiceRoleFlags.mIsLeader       = false;
    diagTlvExt.mData.mServiceRoleFlags.mIsPrimaryBBR   = false;
    diagTlvExt.mData.mServiceRoleFlags.mHostsService   = false;
    diagTlvExt.mData.mServiceRoleFlags.mIsBorderRouter = false;

    // iterate through the device´s ipv6 adresses
    for (uint16_t i = 0; i < aTlv.mData.mIp6AddrList.mCount; ++i)
    {
        ipv6Addr = aTlv.mData.mIp6AddrList.mList[i];

        // rloc and aloc prefix == 0000:00FF:FE00 -> 0000:FF00:00FE
        if (ipv6Addr.mFields.m16[4] == 0x0000 && ipv6Addr.mFields.m16[5] == 0xff00 && ipv6Addr.mFields.m16[6] == 0x00fe)
        {
            // Leader Aloc is FC00 -> 00FC
            diagTlvExt.mData.mServiceRoleFlags.mIsLeader |= (ipv6Addr.mFields.m16[7] == 0x00fc);

            // Primary BBR Aloc is FC38 -> 38FC
            diagTlvExt.mData.mServiceRoleFlags.mIsPrimaryBBR |= (ipv6Addr.mFields.m16[7] == 0x38fc);

            // Service Aloc is in range FC10 to FC2F
            diagTlvExt.mData.mServiceRoleFlags.mHostsService |=
                (htons(ipv6Addr.mFields.m16[7]) >= 0xfc10 && htons(ipv6Addr.mFields.m16[7]) <= 0xfc2f);
            continue;
        }
    }

    while (otNetDataGetNextRoute(mInstance, &iterator, &config) == OT_ERROR_NONE)
    {
        if (config.mRloc16 == localRloc16)
        {
            diagTlvExt.mData.mServiceRoleFlags.mIsBorderRouter = true;
        }

        ++iterator;
    }

    aDeviceDiag->mDeviceTlvSetExtension.push_back(diagTlvExt);
}

} // namespace rest
} // namespace otbr
