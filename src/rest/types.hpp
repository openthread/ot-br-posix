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

#include <chrono>
#include <string>
#include <vector>

#include "openthread/netdiag.h"

namespace otbr {
namespace Rest {

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
    kStatusOk                  = 200, ///< OK
    kStatusBadRequest          = 400, ///< Bad Request
    kStatusResourceNotFound    = 404, ///< Resource Not Found
    kStatusMethodNotAllowed    = 405, ///< Method Not Allowed
    kStatusRequestTimeout      = 408, ///< Request Timeout
    kStatusInternalServerError = 500, ///< Internal Server Error
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

struct ActiveScanResult
{
    uint8_t              mExtAddress[OT_EXT_ADDRESS_SIZE + 1];   ///< IEEE 802.15.4 Extended Address
    std::string          mNetworkName;                           ///< Thread Network Name
    uint8_t              mExtendedPanId[OT_EXT_PAN_ID_SIZE + 1]; ///< Thread Extended PAN ID
    std::vector<uint8_t> mSteeringData;                          ///< Steering Data
    uint16_t             mPanId;                                 ///< IEEE 802.15.4 PAN ID
    uint16_t             mJoinerUdpPort;                         ///< Joiner UDP Port
    uint8_t              mChannel;                               ///< IEEE 802.15.4 Channel
    int8_t               mRssi;                                  ///< RSSI (dBm)
    uint8_t              mLqi;                                   ///< LQI
    uint8_t              mVersion;                               ///< Version
    bool                 mIsNative;                              ///< Native Commissioner flag
    bool                 mIsJoinable;                            ///< Joining Permitted flag
};

struct NetworkConfiguration
{
    bool        mDefaultRoute;
    uint8_t     mChannel;
    std::string mMasterKey;
    std::string mPrefix;
    std::string mNetworkName;
    std::string mPanId;
    std::string mPassphrase;
    std::string mExtPanId;
};

struct NodeInfo
{
    std::string    mWpanService;
    uint32_t       mRole;
    uint16_t       mPanId;
    uint8_t        mChannel;
    otExtAddress   mEui64;
    const uint8_t *mExtPanId;
    const uint8_t *mMeshLocalPrefix;
    otIp6Address   mMeshLocalAddress;
    std::string    mNetworkName;
    std::string    mVersion;
};

struct DiagInfo
{
    std::chrono::steady_clock::time_point mStartTime;
    std::vector<otNetworkDiagTlv>         mDiagContent;
};

struct NetworksInfo
{
    std::chrono::steady_clock::time_point mStartTime;
    otActiveScanResult                    mNetworkContent;
};

} // namespace Rest
} // namespace otbr

#endif // OTBR_REST_TYPES_HPP_
