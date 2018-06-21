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
 *   This file implements the function of "configure gateway" function.
 */

#ifndef DBUS_GATEWAY_HPP
#define DBUS_GATEWAY_HPP

#define OT_INET6_ADDR_STR_LENGTH 46

#include <arpa/inet.h>
#include <stdio.h>

#include <dbus/dbus.h>

#include "dbus_base.hpp"
#include "wpan_controller.hpp"

namespace ot {
namespace Dbus {

class DBusGateway : public DBusBase
{
public:
    DBusGateway(void);

    int  ProcessReply(void);
    void SetDefaultRoute(dbus_bool_t aDefaultRoute) { mDefaultRoute = aDefaultRoute; }
    void SetValidLifeTime(uint32_t aValidLifetime) { mValidLifetime = aValidLifetime; }
    void SetPreferredLifetime(uint32_t aPreferredLifetime) { mPreferredLifetime = aPreferredLifetime; }
    void SetPrefix(const char *aPrefix) { mPrefix = aPrefix; }
    void SetAddressString(const char *aAddressString)
    {
        strncpy(mAddressString, aAddressString, strlen(aAddressString));
        mAddressString[strlen(aAddressString)] = '\0';
    }
    void SetPrefixBytes(uint8_t *aPrefixBytes) { memcpy(mPrefixBytes, aPrefixBytes, sizeof(mPrefixBytes)); }
    void SetAddr(uint8_t *aAddr) { mAddr = aAddr; }

private:
    dbus_bool_t mDefaultRoute;
    uint32_t    mPreferredLifetime;
    uint32_t    mValidLifetime;
    const char *mPrefix;
    uint8_t     mPrefixLength;
    char        mAddressString[OT_INET6_ADDR_STR_LENGTH];
    uint8_t     mPrefixBytes[16];
    uint8_t *   mAddr;
};

} // namespace Dbus
} // namespace ot
#endif // DBUS_GATEWAY_HPP
