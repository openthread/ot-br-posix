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
 *   This file includes all needed types definitions for OTBR-REST.
 */

#ifndef OTBR_REST_TYPES_HPP_
#define OTBR_REST_TYPES_HPP_

#include <chrono>
#include <vector>

#include "openthread/netdiag.h"

using std::chrono::steady_clock;

namespace otbr {
namespace rest {

enum HttpMethod
{
    OTBR_REST_METHOD_DELETE = 0, ///< DELETE
    OTBR_REST_METHOD_GET    = 1, ///< GET
    OTBR_REST_METHOD_HEAD   = 2, ///< HEAD
    OTBR_REST_METHOD_POST   = 3, ///< POST
    OTBR_REST_METHOD_PUT    = 4, ///< PUT
    OTBR_REST_METHOD_OPTIONS = 6, ///< OPTIONS

};

enum PostError
{
    OTBR_REST_POST_ERROR_NONE = 0, ///< No error
    OTBR_REST_POST_BAD_REQUEST= 1, ///< Bad request for post
    OTBR_REST_POST_SET_FAIL   = 2, ///< Fail when set value
};

enum ConnectionState
{
    OTBR_REST_CONNECTION_INIT          = 0, ///< Init
    OTBR_REST_CONNECTION_READWAIT      = 1, ///< Wait to read
    OTBR_REST_CONNECTION_READTIMEOUT   = 2, ///< Reach read timeout
    OTBR_REST_CONNECTION_CALLBACKWAIT  = 3, ///< Wait for callback
    OTBR_REST_CONNECTION_WRITEWAIT     = 4, ///< Wait for write
    OTBR_REST_CONNECTION_WRITETIMEOUT  = 5, ///< Reach write timeout
    OTBR_REST_CONNECTION_INTERNALERROR = 6, ///< occur internal call error
    OTBR_REST_CONNECTION_COMPLETE      = 7, ///< no longer need to be processed

};

struct Node
{
    uint32_t       mRole; 
    uint32_t       mNumOfRouter;
    uint16_t       mRloc16;
    const uint8_t *mExtPanId;
    const uint8_t *mExtAddress;
    otIp6Address   mRlocAddress;
    otLeaderData   mLeaderData;
    std::string    mNetworkName;
};

struct DiagInfo
{
    steady_clock::time_point      mStartTime;
    std::vector<otNetworkDiagTlv> mDiagContent;
};

} // namespace rest
} // namespace otbr

#endif // OTBR_REST_TYPES_HPP_