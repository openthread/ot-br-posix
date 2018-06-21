/*
 *  Copyright (c) 2017, The OpenThread Authors.
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
 *   This file implements "join" Thread Network function.
 */

#ifndef DBUS_JOIN_HPP
#define DBUS_JOIN_HPP

#include <stdio.h>

#include <dbus/dbus.h>

#include "dbus_base.hpp"
#include "wpan_controller.hpp"

namespace ot {
namespace Dbus {

class DBusJoin : public DBusBase
{
public:
    const char *GetNetworkName(void) { return mNetworkName; }
    uint16_t    GetNodeType(void) { return mNodeType; }
    int16_t     GetChannel(void) { return mChannel; }
    uint64_t    GetExtPanId(void) { return mExtPanId; }
    uint16_t    GetPanId(void) { return mPanId; }

    void SetNetworkName(const char *aNetworkName) { mNetworkName = aNetworkName; }
    void SetNodeType(uint16_t aNodeType) { mNodeType = aNodeType; }
    void SetChannel(int16_t aChannel) { mChannel = aChannel; }
    void SetExtPanId(uint64_t aExtPanId) { mExtPanId = aExtPanId; }
    void SetPanId(uint16_t aPanId) { mPanId = aPanId; }

    int ProcessReply(void);

private:
    const char *mNetworkName;
    uint16_t    mNodeType;
    int16_t     mChannel;
    uint64_t    mExtPanId;
    uint16_t    mPanId;
};

} // namespace Dbus
} // namespace ot
#endif // DBUS_JOIN_HPP
