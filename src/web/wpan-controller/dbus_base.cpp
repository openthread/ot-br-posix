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
    DBusError error;

    dbus_error_init(&error);
    mConnection = dbus_bus_get(DBUS_BUS_STARTER, &error);
    if (!mConnection)
    {
        syslog(LOG_ERR, "connection is NULL.\n");

        dbus_error_free(&error);
        dbus_error_init(&error);
        mConnection = dbus_bus_get(DBUS_BUS_SYSTEM, &error);
    }
    if (dbus_error_is_set(&error))
    {
        syslog(LOG_ERR, "connection error: %s", error.message);
    }
    dbus_error_free(&error);
    return mConnection;
}

DBusMessage *DBusBase::GetMessage(void)
{
    int ret = kWpantundStatus_Ok;

    VerifyOrExit(mDestination != NULL, ret = kWpantundStatus_InvalidArgument);
    VerifyOrExit(mPath != NULL, ret = kWpantundStatus_InvalidArgument);
    VerifyOrExit(mIface != NULL, ret = kWpantundStatus_InvalidArgument);
    VerifyOrExit(mMethod != NULL, ret = kWpantundStatus_InvalidArgument);
    mMessage = dbus_message_new_method_call(mDestination, mPath, mIface,
                                            mMethod);
    VerifyOrExit(mMessage != NULL, ret = kWpantundStatus_GetNullMessage);

exit:
    if (ret != kWpantundStatus_Ok)
    {
        syslog(LOG_ERR, "message is NULL\n");
    }
    return mMessage;
}

DBusMessage *DBusBase::GetReply(void)
{
    int       ret = kWpantundStatus_Ok;
    DBusError error;

    dbus_error_init(&error);
    mReply = dbus_connection_send_with_reply_and_block(mConnection, mMessage,
                                                       DEFAULT_TIMEOUT_IN_MILLISECONDS, &error);

    VerifyOrExit(mReply != NULL, ret = kWpantundStatus_GetNullReply);

exit:
    if (ret != kWpantundStatus_Ok)
    {
        syslog(LOG_ERR, "reply is NULL; error: %s\n", error.message);
    }
    if (dbus_error_is_set(&error))
    {
        syslog(LOG_ERR, "reply error: %s", error.message);
    }
    dbus_error_free(&error);
    return mReply;
}

DBusPendingCall *DBusBase::GetPending(void)
{
    int       ret = kWpantundStatus_Ok;
    DBusError error;

    dbus_error_init(&error);
    dbus_connection_send_with_reply(mConnection, mMessage, &mPending, DEFAULT_TIMEOUT_IN_MILLISECONDS);

    VerifyOrExit(mPending != NULL, ret = kWpantundStatus_GetNullPending);

exit:
    if (ret != kWpantundStatus_Ok)
    {
        syslog(LOG_ERR, "pending is NULL; error: %s\n", error.message);
    }
    if (dbus_error_is_set(&error))
    {
        syslog(LOG_ERR, "pending error: %s", error.message);
    }
    dbus_error_free(&error);
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
    int       ret = kWpantundStatus_Ok;
    DBusError error;

    dbus_error_init(&error);
    VerifyOrExit(aDestination != NULL, ret = kWpantundStatus_InvalidArgument);
    strcpy(mDestination, aDestination);
exit:
    if (ret != kWpantundStatus_Ok)
    {
        syslog(LOG_ERR, "destination is NULL; error: %s\n", error.message);
    }
    if (dbus_error_is_set(&error))
    {
        syslog(LOG_ERR, "destination error: %s", error.message);
    }
    dbus_error_free(&error);
}

void DBusBase::SetIface(const char *aIface)
{
    int       ret = kWpantundStatus_Ok;
    DBusError error;

    dbus_error_init(&error);
    VerifyOrExit(aIface != NULL, ret = kWpantundStatus_InvalidArgument);
    strcpy(mIface, aIface);
exit:
    if (ret != kWpantundStatus_Ok)
    {
        syslog(LOG_ERR, "interface is NULL; error: %s\n", error.message);
    }
    if (dbus_error_is_set(&error))
    {
        syslog(LOG_ERR, "SetIface error: %s", error.message);
    }
    dbus_error_free(&error);
}

void DBusBase::SetMethod(const char *aMethod)
{
    mMethod = aMethod;
}

void DBusBase::SetInterfaceName(const char *aInterfaceName)
{
    int       ret = kWpantundStatus_Ok;
    DBusError error;

    dbus_error_init(&error);
    VerifyOrExit(aInterfaceName != NULL, ret = kWpantundStatus_InvalidArgument);
    strcpy(mInterfaceName, aInterfaceName);
exit:
    if (ret != kWpantundStatus_Ok)
    {
        syslog(LOG_ERR, "interface name is NULL; error: %s\n", error.message);
    }
    if (dbus_error_is_set(&error))
    {
        syslog(LOG_ERR, "set interface name error: %s", error.message);
    }
    dbus_error_free(&error);
}

void DBusBase::SetPath(const char *aPath)
{
    int       ret = kWpantundStatus_Ok;
    DBusError error;

    dbus_error_init(&error);
    VerifyOrExit(aPath != NULL, ret = kWpantundStatus_InvalidArgument);
    strcpy(mPath, aPath);
exit:
    if (ret != kWpantundStatus_Ok)
    {
        syslog(LOG_ERR, "path is NULL; error: %s\n", error.message);
    }
    if (dbus_error_is_set(&error))
    {
        syslog(LOG_ERR, "set path error: %s", error.message);
    }
    dbus_error_free(&error);
}

void DBusBase::SetDBusName(const char *aDBusName)
{
    int       ret = kWpantundStatus_Ok;
    DBusError error;

    dbus_error_init(&error);
    VerifyOrExit(aDBusName != NULL, ret = kWpantundStatus_InvalidDBusName);
    strcpy(mDBusName, aDBusName);
exit:
    if (ret != kWpantundStatus_Ok)
    {
        syslog(LOG_ERR, "pending is NULL; error: %d\n", ret);
    }
    if (dbus_error_is_set(&error))
    {
        syslog(LOG_ERR, "set dbus name error: %s", error.message);
    }
    dbus_error_free(&error);
}

} //namespace Dbus
} //namespace ot
