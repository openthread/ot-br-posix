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
 * @brief   Implements collection of network diagnostic TLVs,
 *          including retries.
 *
 */

/**
@startuml
state "NetworkDiagnostic Action" as ND {
  [*] --> Pending : Created
  Pending --> Active : Request sent
  Active --> Completed : Diagnostics received (OT_ERROR_NONE)
  Active --> Pending : OT_ERROR_PENDING (retry/wait)
  Active --> Failed : Error/Timeout
  Pending --> Stopped : Stop/Cancel
  Active --> Stopped : Stop/Cancel
  Completed --> [*]
  Failed --> [*]
  Stopped --> [*]
}

state "NetworkDiagHandler" as Handler {
  [*] --> Idle
  Idle --> Waiting : StartDiagnosticsRequest
  Waiting --> Processing : Request sent
  Processing --> Done : Response received
  Processing --> Waiting : Retry/Partial/Timeout
  Done --> Idle : Cleanup/Reset
  Waiting --> Idle : Stop/Cancel
  Processing --> Idle : Stop/Cancel
}

ND -down-> Handler : uses
@enduml
*/

#include <algorithm>
#include <assert.h>

#include "rest/network_diag_handler.hpp"

#include "openthread/mesh_diag.h"
#include "openthread/platform/toolchain.h"
#include "rest/json.hpp"
#include "rest/rest_server_common.hpp"
#include "rest/types.hpp"

#include <cstring>
#include <memory>
#include <set>
#include <openthread/srp_server.h>
#include <openthread/thread.h>
#include "common/logging.hpp"
#include "common/task_runner.hpp"
#include "utils/string_utils.hpp"

using std::chrono::duration_cast;
using std::chrono::milliseconds;
using std::chrono::steady_clock;

namespace otbr {
namespace rest {

// MaxAge (in Milliseconds) for accepting previously collected diagnostics
static const uint32_t kDiagMaxAge            = 30000;
static const uint32_t kDiagMaxAge_UpperLimit = 10 * kDiagMaxAge;

// Timeout (in Milliseconds) for collecting diagnostics, default if not given in actionTask
static const uint32_t kDiagCollectTimeout            = 10000;
static const uint32_t kDiagCollectTimeout_UpperLimit = 10 * kDiagCollectTimeout;

// Retry delay (in Milliseconds) for retry DiagRequest to FTDs
static const uint32_t kDiagRetryDelayFTD            = 100;
static const uint32_t kDiagRetryDelayFTD_UpperLimit = 5000;

// maximum number of retries for a DiagRequest or DiagQuery
// if not configured in action task
static const uint32_t kDiagMaxRetries = 2;

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
    if (aMlPrefix != nullptr &&
        std::equal(std::begin(aMlPrefix->m8), std::end(aMlPrefix->m8), std::begin(DeviceIPPrefix.m8)))
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

NetworkDiagHandler::NetworkDiagHandler(Services &aServices, otInstance *aInstance)
    : mInstance{aInstance}
    , mServices{aServices}
    , mRequestState{RequestState::kIdle}
    , mDiagQueryRequestState{RequestState::kIdle}
{
}

bool NetworkDiagHandler::IsDiagContentIncomplete(const std::vector<otNetworkDiagTlv> &aDiagContent) const
{
    return aDiagContent.empty() || (aDiagContent.size() < (mDiagReqTlvsCount - mDiagReqTlvsOmitableCount));
}

void NetworkDiagHandler::IsDiagSetComplete(bool &complete)
{
    complete = true;
    if (mIsDiscoveryRequest)
    {
        VerifyOrExit(mDiagSet.size() >= mRouterNeighbors.size(), complete = false);
    }
    else
    {
        VerifyOrExit(!mDiagSet.empty(), complete = false);
    }

    for (const auto &it : mDiagSet)
    {
        VerifyOrExit(!IsDiagContentIncomplete(it.second.mDiagContent), complete = false);
    }

exit:
    return;
}

otError NetworkDiagHandler::StartDiagnosticsRequest(const otIp6Address &aDestination,
                                                    const uint8_t      *aTlvList,
                                                    size_t              aTlvCount,
                                                    const Seconds       aTimeout)
{
    otError error            = OT_ERROR_NONE;
    bool    rlocRequested    = false;
    bool    extAddrRequested = false;

    // we only run a single diagnostic request or query simultaneously
    VerifyOrExit(mRequestState == RequestState::kIdle, error = OT_ERROR_ALREADY);
    mRequestState = RequestState::kWaiting;

    mIsDiscoveryRequest = false;
    mResultUuid         = "";
    mRetries            = 0;
    mMaxRetries         = kDiagMaxRetries;
    mMaxAge             = steady_clock::now() - milliseconds(kDiagMaxAge);
    mTimeout            = steady_clock::now() + aTimeout;

    mDiagReqTlvsCount         = 0;
    mDiagReqTlvsOmitableCount = 0;
    mDiagQueryTlvsCount       = 0;
    for (size_t i = 0; i < aTlvCount; i++)
    {
        if (!DiagnosticTypes::RequiresQuery(aTlvList[i]))
        {
            VerifyOrExit(mDiagReqTlvsCount < DiagnosticTypes::kMaxTotalCount, error = OT_ERROR_PARSE);
            mDiagReqTlvs[mDiagReqTlvsCount++] = aTlvList[i];

            if (aTlvList[i] == OT_NETWORK_DIAGNOSTIC_TLV_SHORT_ADDRESS)
            {
                rlocRequested = true;
            }
            if (aTlvList[i] == OT_NETWORK_DIAGNOSTIC_TLV_EXT_ADDRESS)
            {
                extAddrRequested = true;
            }
            if (DiagnosticTypes::Omittable(aTlvList[i]))
            {
                mDiagReqTlvsOmitableCount++;
            }
        }
        else
        {
            VerifyOrExit(mDiagQueryTlvsCount < DiagnosticTypes::kMaxQueryCount, error = OT_ERROR_PARSE);
            mDiagQueryTlvs[mDiagQueryTlvsCount++] = aTlvList[i];
        }
    }
    if (!rlocRequested)
    {
        VerifyOrExit(mDiagReqTlvsCount < DiagnosticTypes::kMaxTotalCount, error = OT_ERROR_PARSE);
        mDiagReqTlvs[mDiagReqTlvsCount++] = OT_NETWORK_DIAGNOSTIC_TLV_SHORT_ADDRESS;
    }

    if (!extAddrRequested)
    {
        VerifyOrExit(mDiagReqTlvsCount < DiagnosticTypes::kMaxTotalCount, error = OT_ERROR_PARSE);
        mDiagReqTlvs[mDiagReqTlvsCount++] = OT_NETWORK_DIAGNOSTIC_TLV_EXT_ADDRESS;
    }

    memcpy(&mIp6address, &aDestination, sizeof(otIp6Address));

    // remove all previous entries
    ResetRouterDiag(false);
    ResetChildDiag(steady_clock::now());

    ResetChildTables(false);
    ResetChildIp6Addrs(false);
    ResetRouterNeighbors(false);

    if (mDiagQueryTlvsCount > 0)
    {
        mDiagQueryRequestState = RequestState::kWaiting;
    }
    else
    {
        mDiagQueryRequestState = RequestState::kDone;
    }
    SuccessOrExit(error = otThreadSendDiagnosticGet(mInstance, &mIp6address, mDiagReqTlvs, mDiagReqTlvsCount,
                                                    NetworkDiagHandler::DiagnosticResponseHandler, this));
    mTimeLastAttempt = steady_clock::now();

exit:
    if (error != OT_ERROR_NONE && error != OT_ERROR_ALREADY)
    {
        // something went wrong clear internal state to run another network diagnostic action
        otbrLogWarning("%s:%d - %s - %s.", __FILE__, __LINE__, __func__, otThreadErrorToString(error));
        mRequestState          = RequestState::kIdle;
        mDiagQueryRequestState = RequestState::kIdle;
    }
    return error;
}

otError NetworkDiagHandler::GetDiscoveryStatus(uint32_t &aDeviceCount)
{
    otError error = OT_ERROR_NONE;

    switch (mRequestState)
    {
    case RequestState::kIdle:
        error = OT_ERROR_INVALID_STATE;
        break;

    case RequestState::kWaiting:
    case RequestState::kPending:
        error = OT_ERROR_PENDING;
        break;

    case RequestState::kFailed:
        error = OT_ERROR_FAILED;
        break;

    case RequestState::kDone:
        FillDeviceCollection();
        aDeviceCount = mServices.GetDevicesCollection().Size();
        error        = OT_ERROR_NONE;
        break;
    }

    return error;
}

otError NetworkDiagHandler::GetDiagnosticsStatus(const char  *aAddressString,
                                                 AddressType  aType,
                                                 std::string &aResultsUuid) // TODO: add parameter for diagnostic types
{
    otError      error = OT_ERROR_NONE;
    otExtAddress extAddr;

    switch (mRequestState)
    {
    case RequestState::kIdle:
        error = OT_ERROR_INVALID_STATE;
        break;

    case RequestState::kWaiting:
    case RequestState::kPending:
        error = OT_ERROR_PENDING;
        break;

    case RequestState::kFailed:
        error = OT_ERROR_FAILED;
        break;

    case RequestState::kDone:
        switch (aType)
        {
        case kAddressTypeExt:
            IgnoreError(str_to_m8(extAddr.m8, aAddressString, OT_EXT_ADDRESS_SIZE));
        default:
            break;
        }
        FillDiagnosticCollection(extAddr);
        aResultsUuid = mResultUuid;
        error        = OT_ERROR_NONE;
        break;
    }

    return error;
}

void NetworkDiagHandler::StopDiagnosticsRequest(void)
{
    mRequestState          = RequestState::kIdle;
    mDiagQueryRequestState = RequestState::kIdle;
}

void NetworkDiagHandler::Clear(void)
{
    mDiagSet.clear();
    mChildTables.clear();
    mChildIps.clear();
    mRouterNeighbors.clear();
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
    mDiagQueryTlvs[0]   = OT_NETWORK_DIAGNOSTIC_TLV_CHILD;
    mDiagQueryTlvs[1]   = OT_NETWORK_DIAGNOSTIC_TLV_CHILD_IP6_ADDR_LIST;
    mDiagQueryTlvsCount = 2;
}

otError NetworkDiagHandler::HandleNetworkDiscoveryRequest(uint32_t aTimeout, uint32_t aMaxAge, uint8_t aRetryCount)
{
    otError error = OT_ERROR_NONE;

    VerifyOrExit(mRequestState == RequestState::kIdle, error = OT_ERROR_INVALID_STATE);

    mRequestState       = RequestState::kWaiting;
    mIsDiscoveryRequest = true;
    otbrLogWarning("%s:%d - %s - changed to state %d.", __FILE__, __LINE__, __func__, mRequestState);

    mTimeout = steady_clock::now() +
               milliseconds(std::min(std::max(kDiagCollectTimeout, aTimeout), kDiagCollectTimeout_UpperLimit));
    mMaxAge     = steady_clock::now() - milliseconds(std::min(std::max(kDiagMaxAge, aMaxAge), kDiagMaxAge_UpperLimit));
    mMaxRetries = aRetryCount;

    SetDefaultTlvs();

    // run network discovery and collect pre-defined TLVs
    if (StartDiscovery() != OT_ERROR_NONE)
    {
        mRequestState = RequestState::kIdle;
        error         = OT_ERROR_INVALID_STATE;
    }

exit:
    return error;
}

otError NetworkDiagHandler::StartDiscovery()
{
    otError error = OT_ERROR_NONE;

    const std::string ipmaddr = "ff03::2";
    IgnoreError(otIp6AddressFromString(ipmaddr.c_str(), &mIp6address));

    switch (mDiagQueryRequestState)
    {
    case RequestState::kIdle:
        // init or remove outdated entries
        // learn and update router rloc16s in mDiagSet
        ResetRouterDiag(true);
        ResetChildDiag(mMaxAge);

        // init or remove outdated entries
        ResetChildTables(true);
        ResetChildIp6Addrs(true);
        ResetRouterNeighbors(true);

        // collect fresh info and send diagnostic multicast query to all devices, in particular routers & REEDs
        otbrLogWarning("%s:%d - %s - send DiagQuery to %s.", __FILE__, __LINE__, __func__, ipmaddr.c_str());
        SuccessOrExit(error = otThreadSendDiagnosticGet(mInstance, &mIp6address, mDiagReqTlvs, mDiagReqTlvsCount,
                                                        NetworkDiagHandler::DiagnosticResponseHandler, this));
        mDiagQueryRequestState = RequestState::kWaiting;
        mTimeLastAttempt       = steady_clock::now();
        mRetries               = 0;

        // we skip waiting for DiagReq responses, as we do already have the router rloc16s
        // does this cause conflicts as DiagReq to multicast destination is actually a DiagQuery?
        // mDiagQueryRequestState = RequestState::kPending;
        // otbrLogWarning("%s:%d - %s - changed to DiagQuery state %d.", __FILE__, __LINE__, __func__,
        //                mDiagQueryRequestState);
        // give time for responses comming in and continue on next callback
    default:
        ExitNow();
    }

exit:
    return error;
}

otbrError NetworkDiagHandler::Process()
{
    otbrError error    = OTBR_ERROR_NONE;
    bool      complete = false;
    bool      timeout  = false;

    VerifyOrExit(mRequestState == RequestState::kWaiting || mRequestState == RequestState::kPending);
    VerifyOrExit(mTimeout > steady_clock::now(), timeout = true);

    complete = true;
    if (mRequestState == RequestState::kWaiting)
    {
        // check if we already have responses from all known devices
        IsDiagSetComplete(complete);

        if (complete || mRetries >= mMaxRetries)
        {
            mRequestState = RequestState::kPending;
            if (mDiagQueryRequestState == RequestState::kWaiting)
            {
                mDiagQueryRequestState = RequestState::kPending;
            }
        }
        else
        {
            // in case of unknown rloc16 or retries, we need to wait for the DiagReq response.
            if (((mTimeLastAttempt + milliseconds(std::min((1 << mRetries) * kDiagRetryDelayFTD,
                                                           kDiagRetryDelayFTD_UpperLimit))) < steady_clock::now()) ||
                mRetries == 0)
            {
                // retry
                mRetries += 1;
                mTimeLastAttempt = steady_clock::now();
                otbrLogWarning("%s:%d - %s - retry send DiagReq - %d.", __FILE__, __LINE__, __func__, mRetries);
                VerifyOrExit(otThreadSendDiagnosticGet(mInstance, &mIp6address, mDiagReqTlvs, mDiagReqTlvsCount,
                                                       NetworkDiagHandler::DiagnosticResponseHandler,
                                                       this) == OT_ERROR_NONE,
                             error = OTBR_ERROR_REST);
            }
        }
    }

    switch (mDiagQueryRequestState)
    {
    case RequestState::kIdle:
        break;

    case RequestState::kWaiting:
        // wait updated rloc16 or other tlvs from DiagReq responses
        break;

    case RequestState::kPending:
        VerifyOrExit(HandleNextDiagQuery(), complete = false);
        mDiagQueryRequestState = RequestState::kDone;
        otbrLogWarning("%s:%d - %s - changed to DiagQuery state %d.", __FILE__, __LINE__, __func__,
                       mDiagQueryRequestState);

        // check if we have FTD children = REEDs
        if (mIsDiscoveryRequest)
        {
            // we want to learn / update stable address also from REEDs
            // and get its ML-EID-IID, OMR and hostname
            for (auto &parent : mChildTables)
            {
                for (auto &child : parent.second.mChildTable)
                {
                    if (child.mDeviceTypeFtd && (mDiagSet.find(child.mRloc16) == mDiagSet.end()))
                    {
                        // prepare placeholder for expected result from REED
                        otbrLogWarning("%s:%d - %s - have REED 0x%04x.", __FILE__, __LINE__, __func__, child.mRloc16);
                        DiagInfo info           = {};
                        mDiagSet[child.mRloc16] = info;
                        mRetries                = 0;
                        complete                = false;
                        // start retrying to get DiagReq response from REEDs
                        mRequestState = RequestState::kWaiting;
                        // mDiagQueryRequestState = RequestState::kWaiting;
                        memcpy(&mIp6address, otThreadGetRloc(mInstance), sizeof(mIp6address));
                        mIp6address.mFields.m16[7] = htons(child.mRloc16);
                        // we can only have a single DiagReq pending
                        ExitNow();
                    }
                }
            }
        }

        OT_FALL_THROUGH;

    case RequestState::kDone:
        // check if we already have responses to all retries
        IsDiagSetComplete(complete);

        /*
        Use only for unicast retries during discovery or known rloc16. Move elsewhere.
        if (!complete &&
            ((mTimeLastAttempt + milliseconds(std::min((1 << mRetries) * kDiagRetryDelayFTD,
                                                       kDiagRetryDelayFTD_UpperLimit))) < steady_clock::now()))
        {
            // we only come here when we have known rloc16s
            static otIp6Address ip6address;
            // rloc16 destination
            memcpy(&ip6address, otThreadGetRloc(mInstance), sizeof(otIp6Address));

            // retry
            for (const auto &it : mDiagSet)
            {
                if (it.second.mDiagContent.empty() ||
                    (it.second.mDiagContent.size() < (mDiagReqTlvsCount - mDiagReqTlvsOmitableCount)))
                {
                    mRequestState             = RequestState::kWaiting;
                    mDiagQueryRequestState    = RequestState::kWaiting;
                    ip6address.mFields.m16[7] = htons(it.first);
                    otbrLogWarning("%s:%d - %s - retry send DiagReq to 0x%04x.", __FILE__, __LINE__, __func__,
                                   it.first);
                    VerifyOrExit(otThreadSendDiagnosticGet(mInstance, &ip6address, mDiagReqTlvs, mDiagReqTlvsCount,
                                                           NetworkDiagHandler::DiagnosticResponseHandler,
                                                           this) == OT_ERROR_NONE,
                                 error = OTBR_ERROR_REST);
                    mRetries += 1;
                    mTimeLastAttempt = steady_clock::now();
                    complete = false;
                    // wait for response and continue on next callback
                    ExitNow();
                }
            }
        }
        */

    default:
        // busy, retry on next callback
        break;
    }

exit:
    if (error == OTBR_ERROR_NONE)
    {
        if (complete || timeout)
        {
            // Transition to idle must only happen if there are no actions
            // wanting to read back our results
            if (mIsDiscoveryRequest)
            {
                // FillDeviceCollection(); // called later when read back results
                mRequestState          = RequestState::kDone;
                mDiagQueryRequestState = RequestState::kDone;
            }
            else
            {
                if (timeout)
                {
                    otbrLogWarning("%s:%d - %s - timeout.", __FILE__, __LINE__, __func__);
                }
                // FillDiagnosticCollection();  // called later when read back results
                mRequestState          = RequestState::kDone;
                mDiagQueryRequestState = RequestState::kDone;
            }
        }
    }
    else
    {
        otbrLogWarning("%s:%d - %s - otbr error: %s.", __FILE__, __LINE__, __func__, otbrErrorString(error));
    }
    return error;
}

void NetworkDiagHandler::AddSingleRloc16LookUp(uint16_t aRloc16)
{
    if ((aRloc16 & 0x1FF) == 0)
    {
        // destination is router and we may want DiagQuery TLVs
        if (mChildTables.find(aRloc16) == mChildTables.end())
        {
            RouterChildTable infoCt{};
            mChildTables[aRloc16] = infoCt;
        }

        if (mChildIps.find(aRloc16) == mChildIps.end())
        {
            RouterChildIp6Addrs infoIp{};
            mChildIps[aRloc16] = infoIp;
        }

        if (mRouterNeighbors.find(aRloc16) == mRouterNeighbors.end())
        {
            RouterNeighbors infoR{};
            mRouterNeighbors[aRloc16] = infoR;
        }
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
                mChildTables[rloc].mRetries = 0;
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
                mChildIps[rloc].mRetries = 0;
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
                mRouterNeighbors[rloc].mRetries = 0;
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
    otbrLogDebug("%s:%d - %s - updated DiagSet for 0x%04x with %zu TLVs.", __FILE__, __LINE__, __func__, aKey,
                 value.mDiagContent.size());
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
    if (!mIsDiscoveryRequest)
    {
        // we only expect a single unicast response
        VerifyOrExit(mIp6address.mFields.m32[2] == aMessageInfo->mPeerAddr.mFields.m32[2] &&
                         mIp6address.mFields.m32[3] == aMessageInfo->mPeerAddr.mFields.m32[3],
                     aError = OT_ERROR_NONE); // ignore if not matching
    }
    otbrLogDebug("%s:%d - %s - received DiagSet from 0x%04x with %zu TLVs.", __FILE__, __LINE__, __func__, keyRloc,
                 diagSet.size());
    UpdateDiag(keyRloc, diagSet);

exit:
    if (aError != OT_ERROR_NONE)
    {
        otbrLogWarning("%s:%d Failed to get diagnostic data: %s", __FILE__, __LINE__, otThreadErrorToString(aError));
    }
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
    case RequestState::kFailed:
    case RequestState::kDone:
        // Check if we can use the cached results
        if ((aChildTable->mUpdateTime > aMaxAge) || (aChildTable->mRetries > mMaxRetries))
        {
            retval = true;
            break;
        }
        aChildTable->mState = RequestState::kWaiting;
        aChildTable->mRetries++;
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
    otError error = OT_ERROR_NONE;
    auto    it    = mChildTables.find(mDiagQueryRequestRloc);

    VerifyOrExit(it != mChildTables.end(), error = OT_ERROR_INVALID_STATE);
    VerifyOrExit(it->second.mState == RequestState::kPending, error = OT_ERROR_INVALID_STATE);

    VerifyOrExit(aChildEntry != nullptr);

    it->second.mChildTable.emplace_back(*aChildEntry);

    // otbrLogWarning("%s:%d Have child table from 0x%04x for child 0x%04x", __FILE__, __LINE__, mDiagQueryRequestRloc,
    //                 aChildEntry->mRloc16);

exit:
    if (error == OT_ERROR_NONE)
    {
        if (aError == OT_ERROR_NONE)
        {
            it->second.mUpdateTime = steady_clock::now();
            it->second.mState      = RequestState::kDone;
        }
        else if (aError == OT_ERROR_RESPONSE_TIMEOUT)
        {
            it->second.mState = RequestState::kDone;
            // will be retried based on outdated timestamp and retry count
        }
    }
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
    case RequestState::kFailed:
    case RequestState::kDone:
        // Check if we can use the cached results
        if ((aChild->mUpdateTime > aMaxAge) || (aChild->mRetries > mMaxRetries))
        {
            retval = true;
            break;
        }
        aChild->mState = RequestState::kWaiting;
        aChild->mRetries++;
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
    otError        error = OT_ERROR_NONE;
    DeviceIp6Addrs new_device;
    otIp6Address   ip6Address;
    auto           it = mChildIps.find(mDiagQueryRequestRloc);

    VerifyOrExit(aError == OT_ERROR_NONE || aError == OT_ERROR_PENDING || aError == OT_ERROR_RESPONSE_TIMEOUT);
    VerifyOrExit(aIp6AddrIterator != nullptr);
    VerifyOrExit(aChildRloc16 != 65534);

    VerifyOrExit(it != mChildIps.end(), error = OT_ERROR_INVALID_STATE);
    VerifyOrExit(it->second.mState == RequestState::kPending, error = OT_ERROR_INVALID_STATE);

    // otbrLogWarning("%s:%d Have child IP from 0x%04x for child 0x%04x", __FILE__, __LINE__, mDiagQueryRequestRloc,
    //                aChildRloc16);
    new_device.mRloc16 = aChildRloc16;

    while (otMeshDiagGetNextIp6Address(aIp6AddrIterator, &ip6Address) == OT_ERROR_NONE)
    {
        new_device.mIp6Addrs.emplace_back(ip6Address);
    }

    it->second.mChildren.emplace_back(new_device);

exit:
    if (error == OT_ERROR_NONE)
    {
        if (aError == OT_ERROR_NONE)
        {
            it->second.mUpdateTime = steady_clock::now();
            it->second.mState      = RequestState::kDone;
        }
        else if (aError == OT_ERROR_RESPONSE_TIMEOUT)
        {
            it->second.mState = RequestState::kDone;
            // will be retried based on outdated timestamp and retry count
        }
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
    case RequestState::kFailed:
    case RequestState::kDone:
        // Check if we can use the cached results
        if ((aRouterNeighbor->mUpdateTime > aMaxAge) || (aRouterNeighbor->mRetries > mMaxRetries))
        {
            retval = true;
            break;
        }
        aRouterNeighbor->mState = RequestState::kWaiting;
        aRouterNeighbor->mRetries++;
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
    otError error = OT_ERROR_NONE;
    auto    it    = mRouterNeighbors.find(mDiagQueryRequestRloc);

    VerifyOrExit(it != mRouterNeighbors.end(), error = OT_ERROR_INVALID_STATE);
    VerifyOrExit(it->second.mState == RequestState::kPending, error = OT_ERROR_INVALID_STATE);

    VerifyOrExit(aNeighborEntry != nullptr);

    it->second.mNeighbors.emplace_back(*aNeighborEntry);

exit:
    if (error == OT_ERROR_NONE)
    {
        if (aError == OT_ERROR_NONE)
        {
            it->second.mUpdateTime = steady_clock::now();
            it->second.mState      = RequestState::kDone;
        }
        else if (aError == OT_ERROR_RESPONSE_TIMEOUT)
        {
            it->second.mState = RequestState::kDone;
            // will be retried based on outdated timestamp and retry count
        }
    }
    return;
}

void NetworkDiagHandler::UpdateNodeItem(ThisThreadDevice *thisItem)
{
    otError             error = OT_ERROR_NONE;
    otRouterInfo        routerInfo;
    uint8_t             maxRouterId;
    otDeviceRole        role;
    const otIp6Address *mlEid;

    IgnoreError(otBorderAgentGetId(mInstance, &thisItem->mNodeInfo.mBaId));

    if (otBorderAgentIsEnabled(mInstance))
    {
        thisItem->mNodeInfo.mBaState = otBorderAgentIsActive(mInstance) ? "active" : "enabled";
    }
    else
    {
        thisItem->mNodeInfo.mBaState = "disabled";
    }

    error = otThreadGetLeaderData(mInstance, &thisItem->mNodeInfo.mLeaderData);
    if (error != OT_ERROR_NONE)
    {
        otbrLogWarning("%s:%d cannot get LeaderData while detached", __FILE__, __LINE__);
    }

    thisItem->mNodeInfo.mNumOfRouter = 0;
    maxRouterId                      = otThreadGetMaxRouterId(mInstance);
    for (uint8_t i = 0; i <= maxRouterId; ++i)
    {
        if (otThreadGetRouterInfo(mInstance, i, &routerInfo) != OT_ERROR_NONE)
        {
            continue;
        }
        ++thisItem->mNodeInfo.mNumOfRouter;
    }

    role                            = otThreadGetDeviceRole(mInstance);
    thisItem->mNodeInfo.mRole       = GetDeviceRoleName(role);
    thisItem->mNodeInfo.mExtAddress = reinterpret_cast<const uint8_t *>(otLinkGetExtendedAddress(mInstance));
    thisItem->mNodeInfo.mExtPanId   = reinterpret_cast<const uint8_t *>(otThreadGetExtendedPanId(mInstance));

    if (!(role == OT_DEVICE_ROLE_DISABLED || role == OT_DEVICE_ROLE_DETACHED))
    {
        thisItem->mNodeInfo.mNetworkName = otThreadGetNetworkName(mInstance);
        thisItem->mNodeInfo.mRloc16      = otThreadGetRloc16(mInstance);
        thisItem->mNodeInfo.mRlocAddress = *otThreadGetRloc(mInstance);

        mlEid = otThreadGetMeshLocalEid(mInstance);
        if (mlEid)
        {
            memcpy(&thisItem->mDeviceInfo.mMlEidIid, &mlEid->mFields.m8[8], sizeof(thisItem->mDeviceInfo.mMlEidIid));
        }
    }
    else
    {
        thisItem->mNodeInfo.mNetworkName = "";
        thisItem->mNodeInfo.mRloc16      = 0;
        thisItem->mNodeInfo.mRlocAddress = {{0}};
    }
}

void NetworkDiagHandler::SetDeviceItemAttributes(std::string aExtAddr, DeviceInfo &aDeviceInfo)
{
    // if this device`s extAddr equals aDeviceInfo.extaddr
    // add a item of type ThisThreadDevice and set also nodeInfo
    // otherwise add a generic item of type ThreadDevice to the collection of devices

    const otExtAddress *thisextaddr = otLinkGetExtendedAddress(mInstance);
    std::string         thisextaddr_str;

    thisextaddr_str = otbr::Utils::Bytes2Hex(thisextaddr->m8, OT_EXT_ADDRESS_SIZE);
    thisextaddr_str = StringUtils::ToLowercase(thisextaddr_str);

    if (mServices.GetDevicesCollection().GetItem(aExtAddr) == nullptr)
    {
        aDeviceInfo.mNeedsUpdate = !isDeviceComplete(aDeviceInfo);
        if (aDeviceInfo.mNeedsUpdate)
        {
            otbrLogWarning("%s:%d lacking some attributes for deviceId %s", __FILE__, __LINE__, aExtAddr.c_str());
        }

        if (thisextaddr_str == aExtAddr)
        {
            // create ThisThreadDevice with additional NodeInfo
            // auto thisItem = std::make_unique<ThisThreadDevice>(aExtAddr); // with C++14
            std::unique_ptr<ThisThreadDevice> thisItem =
                std::unique_ptr<ThisThreadDevice>(new ThisThreadDevice(aExtAddr));

            thisItem->mDeviceInfo = aDeviceInfo;
            otLinkGetFactoryAssignedIeeeEui64(mInstance, &thisItem->mDeviceInfo.mEui64);
            UpdateNodeItem(thisItem.get());

            mServices.GetDevicesCollection().AddItem(std::move(thisItem));
        }
        else
        {
            // create a general ThreadDevice
            // auto generalItem         = std::make_unique<ThreadDevice>(aExtAddr); // with C++14
            std::unique_ptr<ThreadDevice> generalItem = std::unique_ptr<ThreadDevice>(new ThreadDevice(aExtAddr));
            generalItem->mDeviceInfo                  = aDeviceInfo;
            mServices.GetDevicesCollection().AddItem(std::move(generalItem));
        }
    }
    else // update existing deviceItem
    {
        ThreadDevice *item = dynamic_cast<ThreadDevice *>(mServices.GetDevicesCollection().GetItem(aExtAddr));
        if (item)
        {
            if (thisextaddr_str == aExtAddr)
            {
                UpdateNodeItem(static_cast<ThisThreadDevice *>(item));
            }

            // check eui64 value is valid before updating it
            if (!isOtExtAddrEmpty(aDeviceInfo.mEui64))
            {
                item->SetEui64(aDeviceInfo.mEui64);
                otbrLogWarning("%s:%d updated eui64 for deviceId %s", __FILE__, __LINE__, aExtAddr.c_str());
            }

            // check ipv6 value is valid before updating it
            if (!isOtIp6AddrEmpty(aDeviceInfo.mIp6Addr))
            {
                item->SetIpv6Omr(aDeviceInfo.mIp6Addr);
                otbrLogWarning("%s:%d updated ipv6 for deviceId %s", __FILE__, __LINE__, aExtAddr.c_str());
            }

            // check mleidiid value is valid before updating it
            if (!isOtExtAddrEmpty(aDeviceInfo.mMlEidIid))
            {
                item->SetMlEidIid(aDeviceInfo.mMlEidIid);
                otbrLogWarning("%s:%d updated mlEidIid for deviceId %s", __FILE__, __LINE__, aExtAddr.c_str());
            }
            // update hostname
            if (aDeviceInfo.mHostName.size() > 0)
            {
                item->SetHostname(aDeviceInfo.mHostName);
                otbrLogWarning("%s:%d updated hostname %s for deviceId %s", __FILE__, __LINE__,
                               aDeviceInfo.mHostName.c_str(), aExtAddr.c_str());
            }
            // update role
            if (aDeviceInfo.mRole.size() > 0)
            {
                item->SetRole(aDeviceInfo.mRole);
            }
            // update mode
            if ((aDeviceInfo.mode.mRxOnWhenIdle != item->mDeviceInfo.mode.mRxOnWhenIdle) ||
                (aDeviceInfo.mode.mDeviceType != item->mDeviceInfo.mode.mDeviceType))
            {
                item->SetMode(aDeviceInfo.mode);
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
    otbrError                         error      = OTBR_ERROR_NONE;
    DeviceInfo                        deviceInfo = {};
    std::string                       extAddr    = "";
    std::vector<otMeshDiagChildEntry> child_table;
    std::vector<DeviceIp6Addrs>       child_ip6_lists;

    auto childTableItr = mChildTables.find(aParentRloc16);
    auto childIpsItr   = mChildIps.find(aParentRloc16);

    VerifyOrExit(childTableItr != mChildTables.end(), error = OTBR_ERROR_NOT_FOUND);

    child_table = childTableItr->second.mChildTable;
    for (const auto &item : child_table)
    {
        deviceInfo.mUpdateTime  = steady_clock::now();
        deviceInfo.mExtAddress  = {{0}};
        deviceInfo.mMlEidIid    = {{0}};
        deviceInfo.mEui64       = {{0}};
        deviceInfo.mIp6Addr     = {{{0}}};
        deviceInfo.mHostName    = "";
        deviceInfo.mRole        = "child";
        deviceInfo.mNeedsUpdate = true;

        memcpy(deviceInfo.mExtAddress.m8, item.mExtAddress.m8, OT_EXT_ADDRESS_SIZE);
        deviceInfo.mode = {item.mDeviceTypeFtd, item.mRxOnWhenIdle, item.mFullNetData};

        extAddr = otbr::Utils::Bytes2Hex(item.mExtAddress.m8, OT_EXT_ADDRESS_SIZE);
        extAddr = StringUtils::ToLowercase(extAddr);
        otbrLogDebug("%s:%d - %s - Child %s", __FILE__, __LINE__, __func__, extAddr.c_str());

        if (childIpsItr != mChildIps.end())
        {
            child_ip6_lists = childIpsItr->second.mChildren;

            // get the MTD child`s ipv6 addresses from the children ipv6address list
            for (const auto &device : child_ip6_lists)
            {
                if (device.mRloc16 == item.mRloc16)
                {
                    for (const auto &ip6Addr : device.mIp6Addrs)
                    {
                        // iterate through the device´s ipv6 adresses and
                        // extract OMR ipv6 address and MlEidIid
                        filterIpv6(deviceInfo, ip6Addr, otThreadGetMeshLocalPrefix(mInstance));
                    }
                    GetHostName(deviceInfo);
                    break;
                }
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

exit:
    if (error == OTBR_ERROR_NOT_FOUND)
    {
        otbrLogWarning("%s:%d - %s - Parent RLOC not found", __FILE__, __LINE__, __func__);
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

        // Only assign if the corresponding TLV is present in mDiagQueryTlvs
        if (std::find(mDiagQueryTlvs, mDiagQueryTlvs + mDiagQueryTlvsCount, OT_NETWORK_DIAGNOSTIC_TLV_CHILD) !=
            mDiagQueryTlvs + mDiagQueryTlvsCount)
        {
            aDeviceDiag->mChildren.assign(child_table.begin(), child_table.end());
            networkDiagTlvExtensions diagTlvFlag;
            diagTlvFlag.mType = NETWORK_DIAGNOSTIC_TLVEXT_CHILDREN;
            aDeviceDiag->mDeviceTlvSetExtension.push_back(diagTlvFlag);
        }
        if (std::find(mDiagQueryTlvs, mDiagQueryTlvs + mDiagQueryTlvsCount,
                      OT_NETWORK_DIAGNOSTIC_TLV_CHILD_IP6_ADDR_LIST) != mDiagQueryTlvs + mDiagQueryTlvsCount)
        {
            aDeviceDiag->mChildrenIp6Addrs.assign(child_ip6_lists.begin(), child_ip6_lists.end());
            networkDiagTlvExtensions diagTlvFlag;
            diagTlvFlag.mType = NETWORK_DIAGNOSTIC_TLVEXT_CHILDRENIP6;
            aDeviceDiag->mDeviceTlvSetExtension.push_back(diagTlvFlag);
        }
        if (std::find(mDiagQueryTlvs, mDiagQueryTlvs + mDiagQueryTlvsCount,
                      OT_NETWORK_DIAGNOSTIC_TLV_ROUTER_NEIGHBOR) != mDiagQueryTlvs + mDiagQueryTlvsCount)
        {
            aDeviceDiag->mNeighbors.assign(router_neighbors.begin(), router_neighbors.end());
            networkDiagTlvExtensions diagTlvFlag;
            diagTlvFlag.mType = NETWORK_DIAGNOSTIC_TLVEXT_ROUTERNEIGHBORS;
            aDeviceDiag->mDeviceTlvSetExtension.push_back(diagTlvFlag);
        }
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
        otbrLogWarning("%s:%d Have data from 0x%04x", __FILE__, __LINE__, diag.first);
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
                extAddr = otbr::Utils::Bytes2Hex(diagTlv.mData.mExtAddress.m8, OT_EXT_ADDRESS_SIZE);
                extAddr = StringUtils::ToLowercase(extAddr);
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

void NetworkDiagHandler::FillDiagnosticCollection(otExtAddress aExtAddr) // TODO: use mDiagReqTlvs and mDiagQueryTlvs
{
    bool                match = false;
    const otExtAddress *thisExtAddr;
    if (mDiagSet.empty())
    {
        otbrLogWarning("%s:%d error : Diag set is empty", __FILE__, __LINE__);
    }

    for (auto &diag : mDiagSet)
    {
        if (diag.second.mDiagContent.empty())
        {
            otbrLogWarning("%s:%d error : no response from 0x%04x", __FILE__, __LINE__, diag.first);
            continue;
        }
        otbrLogWarning("%s:%d Have data from 0x%04x", __FILE__, __LINE__, diag.first);

        // check we have desired extAddr corresponding to requested extAddr
        // this should be the case for unicast requests
        // and we should skip the item if it does not match
        // this is a workaround to keep request-response a 1-1 mapping
        for (const auto &diagTlv : diag.second.mDiagContent)
        {
            if (diagTlv.mType == OT_NETWORK_DIAGNOSTIC_TLV_EXT_ADDRESS &&
                std::memcmp(diagTlv.mData.mExtAddress.m8, aExtAddr.m8, OT_EXT_ADDRESS_SIZE) == 0)
            {
                otbrLogWarning("%s:%d - %s - extAddr match to request", __FILE__, __LINE__, __func__);
                match = true;
                break;
            }
        }
        if (!match)
        {
            continue;
        }

        // create a new diagnostic item
        // otbr::rest::NetworkDiagnostics *deviceDiag = new NetworkDiagnostics(); // TODO
        std::unique_ptr<otbr::rest::NetworkDiagnostics> deviceDiag =
            std::unique_ptr<otbr::rest::NetworkDiagnostics>(new otbr::rest::NetworkDiagnostics());

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
                    GetLocalCounters(deviceDiag.get());
                }
                break;
            case OT_NETWORK_DIAGNOSTIC_TLV_SHORT_ADDRESS:
                SetDiagQueryTlvs(deviceDiag.get(), diagTlv.mData.mAddr16);
                break;
            case OT_NETWORK_DIAGNOSTIC_TLV_IP6_ADDR_LIST:
                SetServiceRoleFlags(deviceDiag.get(), diagTlv);
                break;
            default:
                break;
            }

            // Only add diagTlv if its type is in mDiagReqTlvs
            if (std::find(mDiagReqTlvs, mDiagReqTlvs + mDiagReqTlvsCount, diagTlv.mType) !=
                mDiagReqTlvs + mDiagReqTlvsCount)
            {
                deviceDiag->mDeviceTlvSet.push_back(diagTlv);
            }
        }

        // keep a reference to the Uuid of the device
        if (!mResultUuid.empty())
        {
            // this is a workaround to keep the Uuid of multiple response items which should not
            // happen when we have a 1-1 mapping of request-response with unicast requests
            // and destination a single device only identified by its extAddr
            mResultUuid += ",";
        }
        mResultUuid += deviceDiag->mUuid.ToString();
        // store diagnostic item in the collection
        mServices.GetDiagnosticsCollection().AddItem(std::move(deviceDiag));
    }
}

void NetworkDiagHandler::GetHostName(DeviceInfo &aDeviceInfo)
{
#if OTBR_ENABLE_SRP_ADVERTISING_PROXY
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
            if (memcmp(&aDeviceInfo.mIp6Addr, &addresses[i], OT_IP6_ADDRESS_SIZE) == 0)
            {
                auto hostname = std::string(otSrpServerHostGetFullName(host));
                otbrLogWarning("%s:%d - %s - Hostname %s", __FILE__, __LINE__, __func__, hostname.c_str());
                auto pos = hostname.find('.');
                if (pos != std::string::npos)
                {
                    aDeviceInfo.mHostName = hostname.substr(0, pos);
                }
                else
                {
                    aDeviceInfo.mHostName = hostname;
                }
                break;
            }
        }
    }
#else
    OTBR_UNUSED_VARIABLE(aDeviceInfo);
#endif // OTBR_ENABLE_SRP_ADVERTISING_PROXY
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

    uint16_t              rloc16   = 0xffff; // this should be the rloc16 learned from aTlv
    otNetworkDataIterator iterator = OT_NETWORK_DATA_ITERATOR_INIT;
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
            // Rloc is below FC00
            if (ntohs(ipv6Addr.mFields.m16[7]) < 0xfc00)
            {
                rloc16 = ntohs(ipv6Addr.mFields.m16[7]);
            }

            // Leader Aloc is FC00
            diagTlvExt.mData.mServiceRoleFlags.mIsLeader |= (ntohs(ipv6Addr.mFields.m16[7]) == 0xfc00);

            // Primary BBR Aloc is FC38
            diagTlvExt.mData.mServiceRoleFlags.mIsPrimaryBBR |= (ntohs(ipv6Addr.mFields.m16[7]) == 0xfc38);

            // Service Aloc is in range FC10 to FC2F
            diagTlvExt.mData.mServiceRoleFlags.mHostsService |=
                (ntohs(ipv6Addr.mFields.m16[7]) >= 0xfc10 && ntohs(ipv6Addr.mFields.m16[7]) <= 0xfc2f);
        }
    }

    while (otNetDataGetNextRoute(mInstance, &iterator, &config) == OT_ERROR_NONE)
    {
        // check if the given RLOC of aDeviceDiag is a Border Router
        if (config.mRloc16 == rloc16)
        {
            diagTlvExt.mData.mServiceRoleFlags.mIsBorderRouter = true;
            break; // we can stop here, we found the Border Router
        }

        ++iterator;
    }

    aDeviceDiag->mDeviceTlvSetExtension.push_back(diagTlvExt);
}

} // namespace rest
} // namespace otbr
