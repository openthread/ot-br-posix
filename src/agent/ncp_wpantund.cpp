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

#include "ncp_wpantund.hpp"

#include <stdexcept>
#include <vector>

#include <string.h>
#include <stdio.h>
#include <syslog.h>
#include <sys/time.h>

extern "C" {
#include "wpanctl-utils.h"
#include "wpan-dbus-v1.h"
}

#include "spinel.h"

#include "common/code_utils.hpp"

namespace ot {

namespace BorderAgent {

namespace Ncp {

/**
 * This string is used to filter the property changed signal from wpantund.
 */
const char *kDBusMatchPropChanged = "type='signal',interface='"WPANTUND_DBUS_APIv1_INTERFACE "',"
                                    "member='"WPANTUND_IF_SIGNAL_PROP_CHANGED "'";

#define BORDER_AGENT_DBUS_NAME      "otbr.agent"

DBusHandlerResult ControllerWpantund::HandleProperyChangedSignal(DBusConnection *aConnection, DBusMessage *aMessage,
                                                                 void *aContext)
{
    return static_cast<ControllerWpantund *>(aContext)->HandleProperyChangedSignal(*aConnection, *aMessage);
}

DBusHandlerResult ControllerWpantund::HandleProperyChangedSignal(DBusConnection &aConnection, DBusMessage &aMessage)
{
    DBusMessageIter   iter;
    DBusHandlerResult result = DBUS_HANDLER_RESULT_HANDLED;
    const char       *key = NULL;
    const uint8_t    *pskc = NULL;
    const char       *sender = dbus_message_get_sender(&aMessage);
    const char       *path = dbus_message_get_path(&aMessage);

    syslog(LOG_DEBUG, "dbus message received");
    if (sender && path && strcmp(sender, mInterfaceDBusName) && strstr(path, mInterfaceName))
    {
        // DBus name of the interface has changed, possibly caused by wpantund restarted,
        // We have to restart the border agent proxy.
        syslog(LOG_DEBUG, "dbus name changed");

        BorderAgentProxyStart();
    }

    VerifyOrExit(dbus_message_is_signal(&aMessage, WPANTUND_DBUS_APIv1_INTERFACE, WPANTUND_IF_SIGNAL_PROP_CHANGED),
                 result = DBUS_HANDLER_RESULT_NOT_YET_HANDLED);

    VerifyOrExit(dbus_message_iter_init(&aMessage, &iter));
    dbus_message_iter_get_basic(&iter, &key);
    VerifyOrExit(key != NULL, result = DBUS_HANDLER_RESULT_NOT_YET_HANDLED);
    dbus_message_iter_next(&iter);
    syslog(LOG_INFO, "property %s changed", key);

    if (!strcmp(key, kWPANTUNDProperty_NetworkPSKc))
    {
        int             count = 0;
        DBusMessageIter subIter;

        dbus_message_iter_recurse(&iter, &subIter);
        dbus_message_iter_get_fixed_array(&subIter, &pskc, &count);
        mPSKcHandler(pskc, mContext);
    }
    else if (!strcmp(key, kWPANTUNDProperty_BorderAgentProxyStream))
    {
        const uint8_t *buf = NULL;
        uint16_t       locator = 0;
        uint16_t       port = 0;
        uint16_t       len = 0;
        {
            DBusMessageIter sub_iter;
            int             nelements = 0;

            dbus_message_iter_recurse(&iter, &sub_iter);
            dbus_message_iter_get_fixed_array(&sub_iter, &buf, &nelements);
            len = (uint16_t)nelements;
        }

        // both port and locator are encoded in network endian.
        port = buf[--len];
        port |= buf[--len] << 8;
        locator = buf[--len];
        locator |= buf[--len] << 8;

        mPacketHandler(buf, len, locator, port, mContext);
    }
    else
    {
        result = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

exit:
    return result;
}

dbus_bool_t ControllerWpantund::AddDBusWatch(struct DBusWatch *aWatch, void *aContext)
{
    static_cast<ControllerWpantund *>(aContext)->mWatches[aWatch] = true;
    return TRUE;
}

void ControllerWpantund::RemoveDBusWatch(struct DBusWatch *aWatch, void *aContext)
{
    static_cast<ControllerWpantund *>(aContext)->mWatches.erase(aWatch);
}

void ControllerWpantund::ToggleDBusWatch(struct DBusWatch *aWatch, void *aContext)
{
    static_cast<ControllerWpantund *>(aContext)->mWatches[aWatch] = (dbus_watch_get_enabled(aWatch) ? true : false);
}

int ControllerWpantund::BorderAgentProxyEnable(dbus_bool_t aEnable)
{
    int          ret = 0;
    DBusMessage *message = NULL;
    const char  *key = kWPANTUNDProperty_BorderAgentProxyEnabled;

    message = dbus_message_new_method_call(
        mInterfaceDBusName,
        mInterfaceDBusPath,
        WPANTUND_DBUS_APIv1_INTERFACE,
        WPANTUND_IF_CMD_PROP_SET);

    VerifyOrExit(message != NULL, ret = -1);

    VerifyOrExit(dbus_message_append_args(
                     message,
                     DBUS_TYPE_STRING, &key,
                     DBUS_TYPE_BOOLEAN, &aEnable,
                     DBUS_TYPE_INVALID), ret = -1);

    VerifyOrExit(dbus_connection_send(mDBus, message, NULL), ret = -1);

exit:
    if (message != NULL)
    {
        dbus_message_unref(message);
    }

    return ret;
}

ControllerWpantund::ControllerWpantund(const char *aInterfaceName, PSKcHandler aPSKcHandler,
                                       PacketHandler aPacketHandler, void *aContext) :
    mPacketHandler(aPacketHandler),
    mPSKcHandler(aPSKcHandler),
    mContext(aContext)
{
    int       ret = 0;
    DBusError error;

    strncpy(mInterfaceName, aInterfaceName, sizeof(mInterfaceName));

    dbus_error_init(&error);
    mDBus = dbus_bus_get(DBUS_BUS_STARTER, &error);
    if (!mDBus)
    {
        dbus_error_free(&error);
        mDBus = dbus_bus_get(DBUS_BUS_SYSTEM, &error);
    }
    VerifyOrExit(mDBus != NULL, ret = -1);

    VerifyOrExit(dbus_bus_register(mDBus, &error), ret = -1);

    VerifyOrExit(dbus_bus_request_name(mDBus,
                                       BORDER_AGENT_DBUS_NAME,
                                       DBUS_NAME_FLAG_DO_NOT_QUEUE,
                                       &error) == DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER,
                 ret = -1);

    VerifyOrExit(dbus_connection_set_watch_functions(
                     mDBus,
                     AddDBusWatch,
                     RemoveDBusWatch,
                     ToggleDBusWatch,
                     this, NULL), ret = -1);

    dbus_bus_add_match(mDBus, kDBusMatchPropChanged, &error);
    VerifyOrExit(!dbus_error_is_set(&error), ret = -1);

    VerifyOrExit(dbus_connection_add_filter(mDBus, HandleProperyChangedSignal, this, NULL),
                 ret = -1);

exit:
    if (dbus_error_is_set(&error))
    {
        syslog(LOG_ERR, "DBus error: %s", error.message);
        dbus_error_free(&error);
    }

    if (ret)
    {
        if (mDBus)
        {
            dbus_connection_unref(mDBus);
        }
        syslog(LOG_ERR, "Failed to initialize ncp controller. error=%d", ret);
        throw std::runtime_error("Failed to create ncp controller");
    }
}

ControllerWpantund::~ControllerWpantund(void)
{
    BorderAgentProxyStop();
}

int ControllerWpantund::BorderAgentProxyStart(void)
{
    int ret = 0;

    SuccessOrExit(ret = lookup_dbus_name_from_interface(mInterfaceDBusName, mInterfaceName));

    // according to source code of wpanctl, better to export a function.
    snprintf(mInterfaceDBusPath,
             sizeof(mInterfaceDBusPath),
             "%s/%s",
             WPANTUND_DBUS_PATH,
             mInterfaceName);

    BorderAgentProxyEnable(TRUE);

exit:

    return ret;
}

int ControllerWpantund::BorderAgentProxySend(const uint8_t *aBuffer, uint16_t aLength, uint16_t aLocator,
                                             uint16_t aPort)
{
    int          ret = 0;
    DBusMessage *message = NULL;

    std::vector<uint8_t> data(aLength + sizeof(aLocator) + sizeof(aPort));
    const uint8_t       *value = data.data();
    const char          *key = kWPANTUNDProperty_BorderAgentProxyStream;

    memcpy(data.data(), aBuffer, aLength);
    data[aLength] = (aLocator >> 8);
    data[aLength + 1] = (aLocator & 0xff);
    data[aLength + 2] = (aPort >> 8);
    data[aLength + 3] = (aPort & 0xff);

    message = dbus_message_new_method_call(
        mInterfaceDBusName,
        mInterfaceDBusPath,
        WPANTUND_DBUS_APIv1_INTERFACE,
        WPANTUND_IF_CMD_PROP_SET);

    VerifyOrExit(message != NULL, ret = -1);

    VerifyOrExit(dbus_message_append_args(
                     message,
                     DBUS_TYPE_STRING, &key,
                     DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE,
                     &value, data.size(), DBUS_TYPE_INVALID), ret = -1);

    VerifyOrExit(dbus_connection_send(mDBus, message, NULL), ret = -1);

exit:

    if (message)
    {
        dbus_message_unref(message);
    }

    return ret;
}

int ControllerWpantund::BorderAgentProxyStop(void)
{
    BorderAgentProxyEnable(FALSE);
    if (mDBus)
    {
        dbus_connection_unref(mDBus);
        mDBus = NULL;
    }

    return 0;
}

void ControllerWpantund::UpdateFdSet(fd_set &aReadFdSet, fd_set &aWriteFdSet, fd_set &aErrorFdSet, int &aMaxFd)
{
    for (WatchMap::iterator it = mWatches.begin(); it != mWatches.end(); ++it)
    {
        if (!it->second)
        {
            continue;
        }

        DBusWatch   *watch = it->first;
        unsigned int flags = dbus_watch_get_flags(watch);
        int          fd = dbus_watch_get_unix_fd(watch);

        if (fd < 0)
        {
            continue;
        }

        if (flags & DBUS_WATCH_READABLE)
        {
            FD_SET(fd, &aReadFdSet);
        }

        if ((flags & DBUS_WATCH_WRITABLE) &&
            dbus_connection_has_messages_to_send(mDBus))
        {
            FD_SET(fd, &aWriteFdSet);
        }

        FD_SET(fd, &aErrorFdSet);

        if (fd > aMaxFd)
        {
            aMaxFd = fd;
        }
    }
}

void ControllerWpantund::Process(const fd_set &aReadFdSet, const fd_set &aWriteFdSet, const fd_set &aErrorFdSet)
{
    for (WatchMap::iterator it = mWatches.begin(); it != mWatches.end(); ++it)
    {
        if (!it->second)
        {
            continue;
        }

        DBusWatch   *watch = it->first;
        unsigned int flags = dbus_watch_get_flags(watch);
        int          fd = dbus_watch_get_unix_fd(watch);

        if (fd < 0)
        {
            continue;
        }

        if ((flags & DBUS_WATCH_READABLE) && !FD_ISSET(fd, &aReadFdSet))
        {
            flags &= ~DBUS_WATCH_READABLE;
        }

        if ((flags & DBUS_WATCH_WRITABLE) && !FD_ISSET(fd, &aWriteFdSet))
        {
            flags &= ~DBUS_WATCH_WRITABLE;
        }

        if (FD_ISSET(fd, &aErrorFdSet))
        {
            flags |= DBUS_WATCH_ERROR;
        }

        dbus_watch_handle(watch, flags);
    }

    while (DBUS_DISPATCH_DATA_REMAINS == dbus_connection_get_dispatch_status(mDBus) &&
           dbus_connection_read_write_dispatch(mDBus, 0)) ;
}

DBusMessage *ControllerWpantund::RequestProperty(const char *aKey)
{
    DBusMessage *message = NULL;
    DBusMessage *reply = NULL;
    int          timeout = DEFAULT_TIMEOUT_IN_SECONDS * 1000;
    DBusError    error;

    dbus_error_init(&error);
    VerifyOrExit((message = dbus_message_new_method_call(mInterfaceDBusName,
                                                         mInterfaceDBusPath,
                                                         WPANTUND_DBUS_APIv1_INTERFACE,
                                                         WPANTUND_IF_CMD_PROP_GET)) != NULL);

    VerifyOrExit(dbus_message_append_args(message, DBUS_TYPE_STRING, &aKey, DBUS_TYPE_INVALID));

    reply = dbus_connection_send_with_reply_and_block(mDBus, message, timeout, &error);

exit:

    if (dbus_error_is_set(&error))
    {
        syslog(LOG_ERR, "DBus error: %s", error.message);
        dbus_error_free(&error);
    }

    if (message)
    {
        dbus_message_unref(message);
    }

    return reply;
}

int ControllerWpantund::GetProperty(const char *aKey, uint8_t *aBuffer, size_t &aSize)
{
    int          ret = 0;
    DBusMessage *reply = NULL;
    DBusError    error;

    VerifyOrExit(reply = RequestProperty(aKey),
                 ret = -1, syslog(LOG_ERR, "No reply"));

    {
        uint8_t *buffer = NULL;
        int      count = 0;

        dbus_error_init(&error);
        VerifyOrExit(dbus_message_get_args(reply, &error,
                                           DBUS_TYPE_INT32, &ret,
                                           DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE, &buffer, &count,
                                           DBUS_TYPE_INVALID),
                     ret = -1, syslog(LOG_ERR, "Failed to parse"));
        VerifyOrExit(ret == 0);

        aSize = static_cast<size_t>(count);
        memcpy(aBuffer, buffer, count);
    }

exit:

    if (reply)
    {
        dbus_message_unref(reply);
    }

    return ret;
}

const uint8_t *ControllerWpantund::GetPSKc(void)
{
    size_t size = 0;

    VerifyOrExit(0 == GetProperty(kWPANTUNDProperty_NetworkPSKc, mPSKc, size),
                 memset(mPSKc, 0, sizeof(mPSKc)));

exit:
    return mPSKc;
}

const uint8_t *ControllerWpantund::GetEui64(void)
{
    size_t size = 0;

    VerifyOrExit(0 == GetProperty(kWPANTUNDProperty_NCPHardwareAddress, mEui64, size),
                 memset(mEui64, 0, sizeof(mEui64)));

exit:

    return mEui64;
}

Controller *Controller::Create(const char *aInterfaceName, PSKcHandler aPSKcHandler, PacketHandler aPacketHandler,
                               void *aContext)
{
    return new ControllerWpantund(aInterfaceName, aPSKcHandler, aPacketHandler, aContext);
}

void Controller::Destroy(Controller *aController)
{
    delete static_cast<ControllerWpantund *>(aController);
}

} // Ncp

} // namespace BorderAgent

} // namespace ot
