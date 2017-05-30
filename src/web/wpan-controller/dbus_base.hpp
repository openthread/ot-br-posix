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
 *   This file implements the basic DBus operations
 */

#ifndef DBUS_BASE_HPP
#define DBUS_BASE_HPP

#include <syslog.h>

#include <dbus/dbus.h>

namespace ot {
namespace Dbus {

class DBusBase
{
public:
    DBusConnection *GetConnection(void);
    DBusMessage *GetMessage(void);
    DBusMessage *GetReply(void);
    DBusPendingCall *GetPending(void);
    DBusError GetError(void);
    int ProcessReply(void);
    char *GetDBusName(void);
    void free(void);

    void SetDestination(const char *aDestination);
    void SetPath(const char *aPath);
    void SetIface(const char *aIface);
    void SetMethod(const char *aMethod);
    void SetInterfaceName(const char *aInterfaceName);
    void SetDBusName(const char *aDBusName);

    char mDBusName[DBUS_MAXIMUM_NAME_LENGTH + 1];
    char mInterfaceName[DBUS_MAXIMUM_NAME_LENGTH + 1];

private:
    DBusConnection  *mConnection;
    DBusMessage     *mMessage;
    DBusMessage     *mReply;
    DBusError        error;
    DBusPendingCall *mPending;

    const char      *mDestination;
    const char      *mPath;
    const char      *mIface;
    const char      *mMethod;
};

} //namespace Dbus
} //namespace ot
#endif  // DBUS_BASE_HPP
