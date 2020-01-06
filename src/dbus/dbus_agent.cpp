#include "dbus/dbus_agent.hpp"
#include "dbus/constants.hpp"

namespace otbr {
namespace dbus {

const struct timeval DBusAgent::kPollTimeout = {0, 0};

DBusAgent::DBusAgent(const std::string &aInterfaceName, ot::BorderRouter::Ncp::ControllerOpenThread *aNcp)
    : mInterfaceName(aInterfaceName)
    , mNcp(aNcp)
{
}

otbrError DBusAgent::Init(void)
{
    DBusError dbusError;
    otbrError err = OTBR_ERROR_NONE;
    int       requestReply;

    dbus_error_init(&dbusError);
    DBusConnection *conn = dbus_bus_get(DBUS_BUS_SYSTEM, &dbusError);
    mConnection          = std::unique_ptr<DBusConnection, std::function<void(DBusConnection *)>>(
        conn, [](DBusConnection *aConnection) { dbus_connection_unref(aConnection); });
    VerifyOrExit(mConnection != nullptr, err = OTBR_ERROR_DBUS);
    dbus_bus_register(mConnection.get(), &dbusError);
    requestReply =
        dbus_bus_request_name(mConnection.get(), kOtbrDbusPrefix.c_str(), DBUS_NAME_FLAG_REPLACE_EXISTING, &dbusError);
    VerifyOrExit(requestReply == DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER ||
                     requestReply == DBUS_REQUEST_NAME_REPLY_ALREADY_OWNER,
                 err = OTBR_ERROR_DBUS);
    mThreadObject = std::unique_ptr<DBusThreadObject>(new DBusThreadObject(mConnection.get(), mInterfaceName, mNcp));
    mThreadObject->Init();
exit:
    return err;
}

void DBusAgent::UpdateFdSet(fd_set &        aReadFdSet,
                            fd_set &        aWriteFdSet,
                            fd_set &        aErrorFdSet,
                            int &           aMaxFd,
                            struct timeval &aTimeOut)
{
    int dbusFd;

    VerifyOrExit(dbus_connection_get_unix_fd(mConnection.get(), &dbusFd));
    aMaxFd = std::max(dbusFd, aMaxFd);
    FD_SET(dbusFd, &aReadFdSet);
    FD_SET(dbusFd, &aErrorFdSet);
    if (dbus_connection_has_messages_to_send(mConnection.get()))
    {
        FD_SET(dbusFd, &aWriteFdSet);
        aTimeOut = {0, 0};
    }
    if (dbus_connection_get_dispatch_status(mConnection.get()) == DBUS_DISPATCH_DATA_REMAINS)
    {
        aTimeOut = {0, 0};
    }

exit:
    return;
}

void DBusAgent::Process(const fd_set &aReadFdSet, const fd_set &aWriteFdSet, const fd_set &aErrorFdSet)
{
    (void)aReadFdSet;
    (void)aWriteFdSet;
    (void)aErrorFdSet;

    dbus_connection_read_write_dispatch(mConnection.get(), 0);
}

void DBusAgent::MainLoop(void)
{
    while (true)
    {
        fd_set         readFds, writeFds, errFds;
        int            maxFd = -1;
        int            ret;
        struct timeval timeout = kPollTimeout;

        FD_ZERO(&readFds);
        FD_ZERO(&writeFds);
        FD_ZERO(&errFds);

        UpdateFdSet(readFds, writeFds, errFds, maxFd, timeout);
        ret = select(maxFd + 1, &readFds, &writeFds, &errFds, &timeout);
        VerifyOrExit(ret >= 0);
        Process(readFds, writeFds, errFds);
    }

exit:
    return;
}

} // namespace dbus
} // namespace otbr
