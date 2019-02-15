/*
 *    Copyright (c) 2019, The OpenThread Authors.
 *    All rights reserved.
 *
 *    Redistribution and use in source and binary forms, with or without
 *    modification, are permitted provided that the following conditions are met:
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *    3. Neither the name of the copyright holder nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 *    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *    ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *    LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *    CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *    ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *    POSSIBILITY OF SUCH DAMAGE.
 */

#include "mdns_avahi_dbus.hpp"

#include <algorithm>

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

#include "common/code_utils.hpp"
#include "common/logging.hpp"

static const char kAvahiDBusName[]                = "org.freedesktop.Avahi";
static const char kAvahiDBusPath[]                = "/";
static const char kAvahiDBusInterfaceServer[]     = "org.freedesktop.Avahi.Server";
static const char kAvahiDBusInterfaceEntryGroup[] = "org.freedesktop.Avahi.EntryGroup";

static const int kDefaultTimeout = 2 * 1000;

namespace ot {

namespace BorderRouter {

namespace Mdns {

enum
{
    AVAHI_PROTO_UNSPEC = -1,
    AVAHI_PROTO_INET   = 0,
    AVAHI_PROTO_INET6  = 1,
};

enum
{
    AVAHI_IF_UNSPEC = -1
};

static void HandleDBusError(DBusError &aError)
{
    otbrLog(OTBR_LOG_ERR, "MDNS DBus error %s: %s!", aError.name, aError.message);
    dbus_error_free(&aError);
}

PublisherAvahiDBus::PublisherAvahiDBus(int          aProtocol,
                                       const char * aHost,
                                       const char * aDomain,
                                       StateHandler aHandler,
                                       void *       aContext)
    : mDBus(NULL)
    , mProtocol(aProtocol == AF_INET6 ? AVAHI_PROTO_INET6
                                      : aProtocol == AF_INET ? AVAHI_PROTO_INET : AVAHI_PROTO_UNSPEC)
    , mDomain(aDomain == NULL ? "" : aDomain)
    , mHost(aHost == NULL ? "" : aHost)
    , mState(kStateIdle)
    , mStateHandler(aHandler)
    , mContext(aContext)
{
    mEntryGroupPath[0] = '\0';
}

PublisherAvahiDBus::~PublisherAvahiDBus(void)
{
    Stop();

    if (mDBus != NULL)
    {
        dbus_connection_unref(mDBus);
        mDBus = NULL;
    }
}

otbrError PublisherAvahiDBus::Start(void)
{
    otbrError       ret            = OTBR_ERROR_DBUS;
    char *          entryGroupPath = NULL;
    DBusError       error;
    DBusMessage *   message = NULL;
    DBusMessage *   reply   = NULL;
    DBusMessageIter iter;

    dbus_error_init(&error);
    mDBus = dbus_bus_get(DBUS_BUS_SYSTEM, &error);
    VerifyOrExit(mDBus != NULL);

    VerifyOrExit(dbus_bus_register(mDBus, &error));

    VerifyOrExit((message = dbus_message_new_method_call(kAvahiDBusName, kAvahiDBusPath, kAvahiDBusInterfaceServer,
                                                         "EntryGroupNew")) != NULL,
                 errno = ENOMEM);

    reply = dbus_connection_send_with_reply_and_block(mDBus, message, kDefaultTimeout, &error);
    VerifyOrExit(reply != NULL, HandleDBusError(error), ret = OTBR_ERROR_DBUS);

    VerifyOrExit(dbus_message_iter_init(reply, &iter), errno = ENOENT);
    VerifyOrExit(DBUS_TYPE_OBJECT_PATH == dbus_message_iter_get_arg_type(&iter), errno = EINVAL);
    dbus_message_iter_get_basic(&iter, &entryGroupPath);

    strcpy(mEntryGroupPath, entryGroupPath);

    mState = kStateReady;
    mStateHandler(mContext, kStateReady);
    SuccessOrExit(ret = SendCommit());

    ret = OTBR_ERROR_NONE;

exit:
    if (dbus_error_is_set(&error))
    {
        HandleDBusError(error);
    }

    if (reply != NULL)
    {
        dbus_message_unref(reply);
    }

    if (message != NULL)
    {
        dbus_message_unref(message);
    }

    if (ret != OTBR_ERROR_NONE)
    {
        if (mDBus)
        {
            dbus_connection_unref(mDBus);
            mDBus = NULL;
        }
        otbrLog(OTBR_LOG_ERR, "NCP failed to initialize!");
    }

    return ret;
}

bool PublisherAvahiDBus::IsStarted(void) const
{
    return mState == kStateReady;
}

void PublisherAvahiDBus::Stop(void)
{
    VerifyOrExit(mState == kStateReady);

    mServices.clear();

    if (mEntryGroupPath[0] != '\0')
    {
        DBusMessage *message =
            dbus_message_new_method_call(kAvahiDBusName, mEntryGroupPath, kAvahiDBusInterfaceEntryGroup, "Free");

        if (message != NULL)
        {
            dbus_connection_send(mDBus, message, NULL);
            dbus_message_unref(message);
        }

        mEntryGroupPath[0] = '\0';
    }

    mState = kStateIdle;
    mStateHandler(mContext, mState);

exit:
    return;
}

void PublisherAvahiDBus::UpdateFdSet(fd_set & aReadFdSet,
                                     fd_set & aWriteFdSet,
                                     fd_set & aErrorFdSet,
                                     int &    aMaxFd,
                                     timeval &aTimeout)
{
    (void)aErrorFdSet;
    (void)aReadFdSet;
    (void)aWriteFdSet;
    (void)aTimeout;
    (void)aMaxFd;
}

otbrError PublisherAvahiDBus::SendCommit(void)
{
    DBusMessage *message = NULL;
    DBusMessage *reply   = NULL;
    otbrError    ret     = OTBR_ERROR_ERRNO;
    DBusError    error;

    message = dbus_message_new_method_call(kAvahiDBusName, mEntryGroupPath, kAvahiDBusInterfaceEntryGroup, "Commit");

    dbus_error_init(&error);
    reply = dbus_connection_send_with_reply_and_block(mDBus, message, kDefaultTimeout, &error);
    VerifyOrExit(!dbus_error_is_set(&error));
    assert(reply != NULL);

    ret = OTBR_ERROR_NONE;

exit:
    if (reply != NULL)
    {
        dbus_message_unref(reply);
    }

    if (message != NULL)
    {
        dbus_message_unref(message);
    }

    return ret;
}
otbrError PublisherAvahiDBus::PublishService(uint16_t aPort, const char *aName, const char *aType, ...)
{
    bool         isAdd = false;
    DBusError    error;
    DBusMessage *message = NULL;
    DBusMessage *reply   = NULL;
    va_list      args;
    otbrError    ret = OTBR_ERROR_DBUS;

    va_start(args, aType);

    VerifyOrExit(mState == kStateReady, errno = EAGAIN);

    isAdd   = (std::find(mServices.begin(), mServices.end(), aPort) == mServices.end());
    message = dbus_message_new_method_call(kAvahiDBusName, mEntryGroupPath, kAvahiDBusInterfaceEntryGroup,
                                           isAdd ? "AddService" : "UpdateServiceTxt");
    VerifyOrExit(message != NULL, errno = ENOMEM);

    {
        int32_t interface = AVAHI_IF_UNSPEC;
        int32_t flags     = 0;

        VerifyOrExit(dbus_message_append_args(message, DBUS_TYPE_INT32, &interface, DBUS_TYPE_INT32, &mProtocol,
                                              DBUS_TYPE_UINT32, &flags, DBUS_TYPE_STRING, &aName, DBUS_TYPE_STRING,
                                              &aType, DBUS_TYPE_STRING, &mDomain, // domain
                                              DBUS_TYPE_INVALID),
                     errno = EINVAL);
    }

    if (isAdd)
    {
        VerifyOrExit(
            dbus_message_append_args(message, DBUS_TYPE_STRING, &mHost, DBUS_TYPE_UINT16, &aPort, DBUS_TYPE_INVALID),
            errno = EINVAL);
    }

    {
        DBusMessageIter iter;
        DBusMessageIter iter2;
        DBusMessageIter iter3;

        dbus_message_iter_init_append(message, &iter);
        dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, DBUS_TYPE_ARRAY_AS_STRING DBUS_TYPE_BYTE_AS_STRING,
                                         &iter2);

        for (const char *key = va_arg(args, const char *); key != NULL; key = va_arg(args, const char *))
        {
            const char *value = va_arg(args, const char *);
            char        text[kMaxSizeOfTxtRecord];
            const char *entry = NULL;

            VerifyOrExit(value != NULL, errno = EINVAL);

            snprintf(text, sizeof(text), "%s=%s", key, value);
            entry = &text[0];
            dbus_message_iter_open_container(&iter2, DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE_AS_STRING, &iter3);
            dbus_message_iter_append_fixed_array(&iter3, DBUS_TYPE_BYTE, &entry, strlen(entry));
            dbus_message_iter_close_container(&iter2, &iter3);
        }

        dbus_message_iter_close_container(&iter, &iter2);
    }

    dbus_error_init(&error);
    reply = dbus_connection_send_with_reply_and_block(mDBus, message, kDefaultTimeout, &error);
    VerifyOrExit(!dbus_error_is_set(&error));
    assert(reply != NULL);

    mServices.push_back(aPort);

    ret = OTBR_ERROR_NONE;

exit:
    if (dbus_error_is_set(&error))
    {
        HandleDBusError(error);
    }

    if (reply != NULL)
    {
        dbus_message_unref(reply);
    }

    if (message != NULL)
    {
        dbus_message_unref(message);
    }

    va_end(args);

    return ret;
}

void PublisherAvahiDBus::Process(const fd_set &aReadFdSet, const fd_set &aWriteFdSet, const fd_set &aErrorFdSet)
{
    (void)aReadFdSet;
    (void)aWriteFdSet;
    (void)aErrorFdSet;
}

Publisher *Publisher::Create(int aFamily, const char *aHost, const char *aDomain, StateHandler aHandler, void *aContext)
{
    return new PublisherAvahiDBus(aFamily, aHost, aDomain, aHandler, aContext);
}

} // namespace Mdns

} // namespace BorderRouter

} // namespace ot
