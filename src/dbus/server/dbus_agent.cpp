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

#define OTBR_LOG_TAG "DBUS"

#include "dbus/server/dbus_agent.hpp"

#include "common/logging.hpp"
#include "dbus/common/constants.hpp"

namespace otbr {
namespace DBus {

const struct timeval DBusAgent::kPollTimeout = {0, 0};

DBusAgent::DBusAgent(otbr::Ncp::ControllerOpenThread &aNcp)
    : mInterfaceName(aNcp.GetInterfaceName())
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
    VerifyOrExit(
        dbus_connection_set_watch_functions(mConnection.get(), AddDBusWatch, RemoveDBusWatch, nullptr, this, nullptr));
    mThreadObject = std::unique_ptr<DBusThreadObject>(new DBusThreadObject(mConnection.get(), mInterfaceName, &mNcp));
    error         = mThreadObject->Init();
exit:
    if (error != OTBR_ERROR_NONE)
    {
        otbrLogErr("Dbus error %s: %s", dbusError.name, dbusError.message);
    }
    dbus_error_free(&dbusError);
    return error;
}

dbus_bool_t DBusAgent::AddDBusWatch(struct DBusWatch *aWatch, void *aContext)
{
    static_cast<DBusAgent *>(aContext)->mWatches.insert(aWatch);
    return TRUE;
}

void DBusAgent::RemoveDBusWatch(struct DBusWatch *aWatch, void *aContext)
{
    static_cast<DBusAgent *>(aContext)->mWatches.erase(aWatch);
}

void DBusAgent::Update(MainloopContext &aMainloop)
{
    unsigned int flags;
    int          fd;

    if (dbus_connection_get_dispatch_status(mConnection.get()) == DBUS_DISPATCH_DATA_REMAINS)
    {
        aMainloop.mTimeout = {0, 0};
    }

    for (const auto &watch : mWatches)
    {
        if (!dbus_watch_get_enabled(watch))
        {
            continue;
        }

        flags = dbus_watch_get_flags(watch);
        fd    = dbus_watch_get_unix_fd(watch);

        if (fd < 0)
        {
            continue;
        }

        if (flags & DBUS_WATCH_READABLE)
        {
            FD_SET(fd, &aMainloop.mReadFdSet);
        }

        if ((flags & DBUS_WATCH_WRITABLE))
        {
            FD_SET(fd, &aMainloop.mWriteFdSet);
        }

        FD_SET(fd, &aMainloop.mErrorFdSet);

        aMainloop.mMaxFd = std::max(aMainloop.mMaxFd, fd);
    }
}

void DBusAgent::Process(const MainloopContext &aMainloop)
{
    unsigned int flags;
    int          fd;

    for (const auto &watch : mWatches)
    {
        if (!dbus_watch_get_enabled(watch))
        {
            continue;
        }

        flags = dbus_watch_get_flags(watch);
        fd    = dbus_watch_get_unix_fd(watch);

        if (fd < 0)
        {
            continue;
        }

        if ((flags & DBUS_WATCH_READABLE) && !FD_ISSET(fd, &aMainloop.mReadFdSet))
        {
            flags &= static_cast<unsigned int>(~DBUS_WATCH_READABLE);
        }

        if ((flags & DBUS_WATCH_WRITABLE) && !FD_ISSET(fd, &aMainloop.mWriteFdSet))
        {
            flags &= static_cast<unsigned int>(~DBUS_WATCH_WRITABLE);
        }

        if (FD_ISSET(fd, &aMainloop.mErrorFdSet))
        {
            flags |= DBUS_WATCH_ERROR;
        }

        dbus_watch_handle(watch, flags);
    }

    while (DBUS_DISPATCH_DATA_REMAINS == dbus_connection_dispatch(mConnection.get()))
        ;
}

} // namespace DBus
} // namespace otbr
