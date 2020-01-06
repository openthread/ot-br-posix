#include <stdio.h>

#include <dbus/dbus.h>

#include "client/dbus/thread_api_dbus.hpp"
#include "dbus/constants.hpp"

using namespace otbr::dbus;
using namespace otbr::client;

int main()
{
    DBusError       err;
    DBusConnection *connection;
    // int                            requestReply;
    std::unique_ptr<ThreadApiDBus> api;

    dbus_error_init(&err);
    connection = dbus_bus_get(DBUS_BUS_SYSTEM, &err);

    VerifyOrExit(connection != nullptr);

    VerifyOrExit(dbus_bus_register(connection, &err) == true);

    api = std::unique_ptr<ThreadApiDBus>(new ThreadApiDBus(connection));

    api->Scan([](const std::vector<otActiveScanResult> &aResult) {
        printf("Found %zd networks\n", aResult.size());
        exit(0);
    });

    while (true)
    {
        dbus_connection_read_write_dispatch(connection, 0);
    }

exit:
    if (connection != nullptr)
    {
        dbus_connection_unref(connection);
    }

    return 0;
};
