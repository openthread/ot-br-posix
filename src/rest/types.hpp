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
 *   This file includes type definitions for OTBR-REST.
 */

#ifndef OTBR_REST_TYPES_HPP_
#define OTBR_REST_TYPES_HPP_

#include "openthread-br/config.h"

#include <chrono>
#include <string>
#include <vector>

#include <openthread/border_agent.h>

#include "openthread/mesh_diag.h"
#include "openthread/netdiag.h"

#define OT_REST_ACCEPT_HEADER "Accept"
#define OT_REST_ALLOW_HEADER "Allow"
#define OT_REST_CONTENT_TYPE_HEADER "Content-Type"

#define OT_REST_CONTENT_TYPE_JSON "application/json"
#define OT_REST_CONTENT_TYPE_PLAIN "text/plain"
#define OT_REST_CONTENT_TYPE_JSONAPI "application/vnd.api+json"

using std::chrono::steady_clock;

namespace otbr {
namespace rest {

enum class HttpMethod : std::uint8_t
{
    kDelete  = 0, ///< DELETE
    kGet     = 1, ///< GET
    kHead    = 2, ///< HEAD
    kPost    = 3, ///< POST
    kPut     = 4, ///< PUT
    kOptions = 6, ///< OPTIONS

};

enum class HttpStatusCode : std::uint16_t
{
    kStatusOk                   = 200,
    kStatusCreated              = 201,
    kStatusNoContent            = 204,
    kStatusBadRequest           = 400,
    kStatusResourceNotFound     = 404,
    kStatusMethodNotAllowed     = 405,
    kStatusRequestTimeout       = 408,
    kStatusConflict             = 409,
    kStatusUnsupportedMediaType = 415,
    kStatusInternalServerError  = 500,
    kStatusServiceUnavailable   = 503,
};

enum class PostError : std::uint8_t
{
    kPostErrorNone  = 0, ///< No error
    kPostBadRequest = 1, ///< Bad request for post
    kPostSetFail    = 2, ///< Fail when set value
};

enum class ConnectionState : std::uint8_t
{
    kInit          = 0, ///< Init
    kReadWait      = 1, ///< Wait to read
    kReadTimeout   = 2, ///< Reach read timeout
    kCallbackWait  = 3, ///< Wait for callback
    kWriteWait     = 4, ///< Wait for write
    kWriteTimeout  = 5, ///< Reach write timeout
    kInternalError = 6, ///< Occur internal call error
    kComplete      = 7, ///< No longer need to be processed

};
struct NodeInfo
{
    otBorderAgentId    mBaId;
    otBorderAgentState mBaState;
    std::string        mRole;
    uint32_t           mNumOfRouter;
    uint16_t           mRloc16;
    const uint8_t     *mExtPanId;
    const uint8_t     *mExtAddress;
    otIp6Address       mRlocAddress;
    otLeaderData       mLeaderData;
    std::string        mNetworkName;
};

// move into rest_task_energy_scan.hpp?

//#define MAX_ENERGYSCAN_COUNT 255
//#define MAX_CHANNELS 16

typedef struct EnergyReport
{
    uint8_t             channel;
    std::vector<int8_t> maxRssi;
} EnergyReport;

typedef struct EnergyScanReport
{
    otIp6InterfaceIdentifier origin; // deviceId ml-eidiid
    // char origin_str[16];
    uint8_t count; // the number of repeated measurements, equivalent to the length of the EnergyReport vector
    std::vector<EnergyReport> report;
    // std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds> created;
} EnergyScanReport;

struct DiagInfo
{
    steady_clock::time_point      mStartTime;
    std::vector<otNetworkDiagTlv> mDiagContent;
};

enum class DeviceSelfType
{
    kNone,
    kThisDevice,
    kThisDeviceParent,
};

struct DeviceIp6Addrs // item of TLV 30
{
    uint16_t                  mRloc16;
    std::vector<otIp6Address> mIp6Addrs;
};

struct RouterNeighborLink
{
    uint8_t mRouterId;
    uint8_t mLinkQuality;
};

struct RouterInfo
{
    otExtAddress   mExtAddress;
    uint16_t       mRloc16;
    uint8_t        mRouterId;
    uint16_t       mVersion;
    DeviceSelfType mSelfType;
    bool           mIsLeader;
    bool           mIsBorderRouter;

    std::vector<RouterNeighborLink>            mNeighborLinks;
    std::vector<otMeshDiagRouterNeighborEntry> mNeighborLinksEntry; // TLV 31
    std::vector<otMeshDiagChildInfo>           mChildren;
    std::vector<otMeshDiagChildEntry>          mChildrenEntry;    // TLV 29
    std::vector<DeviceIp6Addrs>                mChildrenIp6Addrs; // TLV 30
    std::vector<otIp6Address>                  mIpAddresses;
};

/**
 * Represents static device infos
 *
 */
struct DeviceInfo
{
    std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds> mUpdate;

    otExtAddress     mExtAddress;
    bool             mNeedsUpdate;
    std::string      mRole;
    otExtAddress     mMlEidIid;
    otExtAddress     mEui64;
    otIp6Address     mIp6Addr;
    std::string      mHostName = "";
    otLinkModeConfig mode;
};

// custom Tlvs
#define NETWORK_DIAGNOSTIC_TLVEXT_BR_COUNTER 255
#define NETWORK_DIAGNOSTIC_TLVEXT_SERVICEROLEFLAGS 254
typedef struct networkDiagTlvExtensions
{
    uint8_t mType; // custom Tlvs
    union
    {
        otBorderRoutingCounters mBrCounters;
        struct
        {
            bool mIsLeader;
            bool mHostsService;
            bool mIsPrimaryBBR;
            bool mIsBorderRouter;
        } mServiceRoleFlags;
    } mData;
} networkDiagTlvExtensions;

} // namespace rest
} // namespace otbr

#endif // OTBR_REST_TYPES_HPP_
