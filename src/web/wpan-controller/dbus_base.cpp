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
 * @file:
 *   This file implements the basic DBus operations
 *
 */

#include "common/code_utils.hpp"

#include "dbus_base.hpp"
#include "wpan_controller.hpp"


#define DEFAULT_TIMEOUT_IN_MILLISECONDS 60 * 1000

namespace ot {
namespace Dbus {

DBusConnection *DBusBase::GetConnection(void)
{
    dbus_error_init(&error);
    mConnection = dbus_bus_get(DBUS_BUS_STARTER, &error);
    if (!mConnection)
    {
        syslog(LOG_ERR, "mConnection is NULL.\n");

        dbus_error_free(&error);
        dbus_error_init(&error);
        mConnection = dbus_bus_get(DBUS_BUS_SYSTEM, &error);
    }
    return mConnection;
}

DBusMessage *DBusBase::GetMessage(void)
{
    int ret = kWpantundStatus_Ok;

    mMessage = dbus_message_new_method_call(mDestination, mPath, mIface,
                                            mMethod);
    VerifyOrExit(mMessage != NULL, ret = kWpantundStatus_GetNullMessage);

exit:
    if (ret != kWpantundStatus_Ok)
    {
        syslog(LOG_ERR, "mMessage is NULL\n");
    }
    return mMessage;
}

DBusMessage *DBusBase::GetReply(void)
{
    int ret = kWpantundStatus_Ok;

    mReply = dbus_connection_send_with_reply_and_block(mConnection, mMessage,
                                                       DEFAULT_TIMEOUT_IN_MILLISECONDS, &error);

    VerifyOrExit(mReply != NULL, ret = kWpantundStatus_GetNullReply);

exit:
    if (ret != kWpantundStatus_Ok)
    {
        syslog(LOG_ERR, "mReply is NULL; error: %s\n", error.message);
    }
    return mReply;
}

DBusPendingCall *DBusBase::GetPending(void)
{
    int ret = kWpantundStatus_Ok;

    dbus_connection_send_with_reply(mConnection, mMessage, &mPending, DEFAULT_TIMEOUT_IN_MILLISECONDS);

    VerifyOrExit(mPending != NULL, ret = kWpantundStatus_GetNullPending);

exit:
    if (ret != kWpantundStatus_Ok)
    {
        syslog(LOG_ERR, "mPending is NULL; error: %s\n", error.message);
    }
    return mPending;
}

void DBusBase::free()
{
    if (mConnection)
        dbus_connection_unref(mConnection);

    if (mMessage)
        dbus_message_unref(mMessage);

    if (mReply)
        dbus_message_unref(mReply);

    dbus_error_free(&error);
}

DBusError DBusBase::GetError(void)
{
    return error;
}

int DBusBase::ProcessReply(void)
{
    return kWpantundStatus_Ok;
}

char *DBusBase::GetDBusName(void)
{
    return mDBusName;
}

void DBusBase::SetDestination(const char *aDestination)
{
    mDestination = aDestination;
}
void DBusBase::SetPath(const char *aPath)
{
    mPath = aPath;
}
void DBusBase::SetIface(const char *aIface)
{
    mIface = aIface;
}
void DBusBase::SetMethod(const char *aMethod)
{
    mMethod = aMethod;
}
void DBusBase::SetInterfaceName(const char *aInterfaceName)
{
    strncpy(mInterfaceName, aInterfaceName, strlen(aInterfaceName));
    mInterfaceName[strlen(aInterfaceName)] = '\0';
}
void DBusBase::SetDBusName(const char *aDBusName)
{
    strncpy(mDBusName, aDBusName, strlen(aDBusName));
    mInterfaceName[strlen(aDBusName)] = '\0';
}

} //namespace Dbus
} //namespace ot
