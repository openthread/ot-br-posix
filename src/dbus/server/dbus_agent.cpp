/*
 *    Copyright (c) 2020, The OpenThread Authors.
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

#include "dbus/server/dbus_agent.hpp"
#include "common/logging.hpp"
#include "dbus/common/constants.hpp"

namespace otbr {
namespace DBus {

const struct timeval DBusAgent::kPollTimeout = {0, 0};

DBusAgent::DBusAgent(const std::string &aInterfaceName, otbr::Ncp::ControllerOpenThread *aNcp)
    : mInterfaceName(aInterfaceName)
    , mNcp(aNcp)
{
}

otbrError DBusAgent::Init(void)
{
    DBusError   dbusError;
    otbrError   error = OTBR_ERROR_NONE;
    int         requestReply;
    std::string serverName = OTBR_DBUS_SERVER_PREFIX + mInterfaceName;

    dbus_error_init(&dbusError);
    DBusConnection *conn = dbus_bus_get(DBUS_BUS_SYSTEM, &dbusError);
    mConnection          = std::unique_ptr<DBusConnection, std::function<void(DBusConnection *)>>(
        conn, [](DBusConnection *aConnection) { dbus_connection_unref(aConnection); });
    VerifyOrExit(mConnection != nullptr, error = OTBR_ERROR_DBUS);
    dbus_bus_register(mConnection.get(), &dbusError);
    requestReply =
        dbus_bus_request_name(mConnection.get(), serverName.c_str(), DBUS_NAME_FLAG_REPLACE_EXISTING, &dbusError);
    VerifyOrExit(requestReply == DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER ||
                     requestReply == DBUS_REQUEST_NAME_REPLY_ALREADY_OWNER,
                 error = OTBR_ERROR_DBUS);
    VerifyOrExit(dbus_connection_set_watch_functions(mConnection.get(), AddDBusWatch, RemoveDBusWatch, ToggleDBusWatch,
                                                     this, nullptr));
    mThreadObject = std::unique_ptr<DBusThreadObject>(new DBusThreadObject(mConnection.get(), mInterfaceName, mNcp));
    error         = mThreadObject->Init();
exit:
    if (error != OTBR_ERROR_NONE)
    {
        otbrLog(OTBR_LOG_ERR, "dbus error %s: %s", dbusError.name, dbusError.message);
    }
    dbus_error_free(&dbusError);
    return error;
}

dbus_bool_t DBusAgent::AddDBusWatch(struct DBusWatch *aWatch, void *aContext)
{
    static_cast<DBusAgent *>(aContext)->mWatches[aWatch] = true;
    return TRUE;
}

void DBusAgent::RemoveDBusWatch(struct DBusWatch *aWatch, void *aContext)
{
    static_cast<DBusAgent *>(aContext)->mWatches.erase(aWatch);
}

void DBusAgent::ToggleDBusWatch(struct DBusWatch *aWatch, void *aContext)
{
    static_cast<DBusAgent *>(aContext)->mWatches[aWatch] = (dbus_watch_get_enabled(aWatch) ? true : false);
}

void DBusAgent::UpdateFdSet(fd_set &        aReadFdSet,
                            fd_set &        aWriteFdSet,
                            fd_set &        aErrorFdSet,
                            int &           aMaxFd,
                            struct timeval &aTimeOut)
{
    DBusWatch *  watch = nullptr;
    unsigned int flags;
    int          fd;

    if (dbus_connection_get_dispatch_status(mConnection.get()) == DBUS_DISPATCH_DATA_REMAINS)
    {
        aTimeOut = {0, 0};
    }

    for (const auto &p : mWatches)
    {
        if (!p.second)
        {
            continue;
        }

        watch = p.first;
        flags = dbus_watch_get_flags(watch);
        fd    = dbus_watch_get_unix_fd(watch);

        if (fd < 0)
        {
            continue;
        }

        if (flags & DBUS_WATCH_READABLE)
        {
            FD_SET(fd, &aReadFdSet);
        }

        if ((flags & DBUS_WATCH_WRITABLE) && dbus_connection_has_messages_to_send(mConnection.get()))
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

void DBusAgent::Process(const fd_set &aReadFdSet, const fd_set &aWriteFdSet, const fd_set &aErrorFdSet)
{
    DBusWatch *  watch = nullptr;
    unsigned int flags;
    int          fd;

    for (const auto &p : mWatches)
    {
        if (!p.second)
        {
            continue;
        }

        watch = p.first;
        flags = dbus_watch_get_flags(watch);
        fd    = dbus_watch_get_unix_fd(watch);

        if (fd < 0)
        {
            continue;
        }

        if ((flags & DBUS_WATCH_READABLE) && !FD_ISSET(fd, &aReadFdSet))
        {
            flags &= static_cast<unsigned int>(~DBUS_WATCH_READABLE);
        }

        if ((flags & DBUS_WATCH_WRITABLE) && !FD_ISSET(fd, &aWriteFdSet))
        {
            flags &= static_cast<unsigned int>(~DBUS_WATCH_WRITABLE);
        }

        if (FD_ISSET(fd, &aErrorFdSet))
        {
            flags |= DBUS_WATCH_ERROR;
        }

        dbus_watch_handle(watch, flags);
    }

    while (DBUS_DISPATCH_DATA_REMAINS == dbus_connection_get_dispatch_status(mConnection.get()) &&
           dbus_connection_read_write_dispatch(mConnection.get(), 0))
        ;
}

} // namespace DBus
} // namespace otbr
