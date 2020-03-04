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

#include <memory>
#include <stdio.h>

#include <dbus/dbus.h>

#include "dbus/client/thread_api_dbus.hpp"
#include "dbus/common/constants.hpp"

using otbr::DBus::otbrActiveScanResult;
using otbr::DBus::OtbrClientError;
using otbr::DBus::otbrDeviceRole;
using otbr::DBus::otbrLinkModeConfig;
using otbr::DBus::ThreadApiDBus;

struct DBusConnectionDeleter
{
    void operator()(DBusConnection *aConnection) { dbus_connection_unref(aConnection); }
};

using UniqueDBusConnection = std::unique_ptr<DBusConnection, DBusConnectionDeleter>;

int main()
{
    DBusError                      err;
    UniqueDBusConnection           connection;
    std::unique_ptr<ThreadApiDBus> api;
    uint64_t                       extpanid = 0xdead00beaf00cafe;

    dbus_error_init(&err);
    connection = UniqueDBusConnection(dbus_bus_get(DBUS_BUS_SYSTEM, &err));

    VerifyOrExit(connection != nullptr);

    VerifyOrExit(dbus_bus_register(connection.get(), &err) == true);

    api = std::unique_ptr<ThreadApiDBus>(new ThreadApiDBus(connection.get()));

    api->AddDeviceRoleHandler(
        [](otbrDeviceRole aRole) { printf("Device role changed to %d\n", static_cast<uint8_t>(aRole)); });

    api->Scan([&api, extpanid](const std::vector<otbrActiveScanResult> &aResult) {
        otbrLinkModeConfig cfg = {true, true, false, true};

        for (auto &&result : aResult)
        {
            printf("%s channel %d rssi %d\n", result.mNetworkName.c_str(), result.mChannel, result.mRssi);
        }

        api->SetLinkMode(cfg);
        api->GetLinkMode(cfg);
        printf("LinkMode %d %d %d %d\n", cfg.mRxOnWhenIdle, cfg.mSecureDataRequests, cfg.mDeviceType, cfg.mNetworkData);

        cfg.mDeviceType = true;
        api->SetLinkMode(cfg);

        api->Attach("Test", 0x3456, extpanid, {}, {}, UINT32_MAX, [&api, extpanid](OtbrClientError aError) {
            printf("Attach result %d\n", static_cast<int>(aError));
            uint64_t extpanidCheck;
            if (aError == OTBR_ERROR_NONE)
            {
                std::string name;

                api->GetNetworkName(name);
                api->GetExtPanId(extpanidCheck);
                printf("Current network name %s\n", name.c_str());
                printf("Current network xpanid %lx\n", extpanidCheck);
                api->FactoryReset(nullptr);
                api->GetNetworkName(name);
            }
            if (aError != OTBR_ERROR_NONE || extpanidCheck != extpanid)
            {
                exit(-1);
            }
            api->Attach("Test", 0x3456, extpanid, {}, {}, UINT32_MAX,
                        [](OtbrClientError aErr) { exit(static_cast<uint8_t>(aErr)); });
        });
    });

    while (true)
    {
        dbus_connection_read_write_dispatch(connection.get(), 0);
    }

exit:
    dbus_error_free(&err);
    return 0;
};
