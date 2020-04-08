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

#include <assert.h>
#include <stdio.h>

#include <memory>

#include <dbus/dbus.h>

#include "dbus/client/thread_api_dbus.hpp"
#include "dbus/common/constants.hpp"

using otbr::dbus::ActiveScanResult;
using otbr::dbus::ClientError;
using otbr::dbus::DeviceRole;
using otbr::dbus::LinkModeConfig;
using otbr::dbus::ThreadApiDBus;

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
        [](DeviceRole aRole) { printf("Device role changed to %d\n", static_cast<uint8_t>(aRole)); });

    api->Scan([&api, extpanid](const std::vector<ActiveScanResult> &aResult) {
        LinkModeConfig cfg = {true, true, false, true};

        for (auto &&result : aResult)
        {
            printf("%s channel %d rssi %d\n", result.mNetworkName.c_str(), result.mChannel, result.mRssi);
        }

        api->SetLinkMode(cfg);
        api->GetLinkMode(cfg);
        printf("LinkMode %d %d %d %d\n", cfg.mRxOnWhenIdle, cfg.mSecureDataRequests, cfg.mDeviceType, cfg.mNetworkData);

        cfg.mDeviceType = true;
        api->SetLinkMode(cfg);

        api->Attach("Test", 0x3456, extpanid, {}, {}, UINT32_MAX, [&api, extpanid](ClientError aError) {
            printf("Attach result %d\n", static_cast<int>(aError));
            uint64_t extpanidCheck;
            if (aError == OTBR_ERROR_NONE)
            {
                std::string                           name;
                uint64_t                              extAddress;
                uint16_t                              rloc16;
                uint8_t                               routerId;
                std::vector<uint8_t>                  networkData;
                std::vector<uint8_t>                  stableNetworkData;
                otbr::dbus::LeaderData                leaderData;
                uint8_t                               leaderWeight;
                int8_t                                rssi;
                int8_t                                txPower;
                std::vector<otbr::dbus::ChildInfo>    childTable;
                std::vector<otbr::dbus::NeighborInfo> neighborTable;
                uint32_t                              partitionId;
                otbr::dbus::Ip6Prefix                 prefix;
                otbr::dbus::OnMeshPrefix              onMeshPrefix;

                prefix.mPrefix = {0xfd, 0xcd, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
                prefix.mLength = 64;

                onMeshPrefix.mPrefix     = prefix;
                onMeshPrefix.mPreference = 0;
                onMeshPrefix.mStable     = true;

                assert(api->GetNetworkName(name) == OTBR_ERROR_NONE);
                assert(api->GetExtPanId(extpanidCheck) == OTBR_ERROR_NONE);
                assert(api->GetRloc16(rloc16) == OTBR_ERROR_NONE);
                assert(api->GetExtendedAddress(extAddress) == OTBR_ERROR_NONE);
                assert(api->GetRouterId(routerId) == OTBR_ERROR_NONE);
                assert(api->GetLeaderData(leaderData) == OTBR_ERROR_NONE);
                assert(api->GetNetworkData(networkData) == OTBR_ERROR_NONE);
                assert(api->GetStableNetworkData(stableNetworkData) == OTBR_ERROR_NONE);
                assert(api->GetLocalLeaderWeight(leaderWeight) == OTBR_ERROR_NONE);
                assert(api->GetChildTable(childTable) == OTBR_ERROR_NONE);
                assert(api->GetNeighborTable(neighborTable) == OTBR_ERROR_NONE);
                assert(api->GetPartitionId(partitionId) == OTBR_ERROR_NONE);
                assert(api->GetInstantRssi(rssi) == OTBR_ERROR_NONE);
                assert(api->GetRadioTxPower(txPower) == OTBR_ERROR_NONE);
                assert(api->AddExternalRoute(prefix, 0, true) == OTBR_ERROR_NONE);
                assert(api->RemoveExternalRoute(prefix) == OTBR_ERROR_NONE);
                assert(api->AddOnMeshPrefix(onMeshPrefix) == OTBR_ERROR_NONE);
                assert(api->RemoveOnMeshPrefix(onMeshPrefix.mPrefix) == OTBR_ERROR_NONE);
                api->FactoryReset(nullptr);
                assert(api->GetNetworkName(name) == OTBR_ERROR_NONE);
                assert(rloc16 != 0);
                assert(extAddress != 0);
                assert(partitionId != 0);
                assert(routerId == leaderData.mLeaderRouterId);
                assert(!networkData.empty());
                assert(childTable.empty());
                assert(neighborTable.empty());
            }
            if (aError != OTBR_ERROR_NONE || extpanidCheck != extpanid)
            {
                exit(-1);
            }
            api->Attach("Test", 0x3456, extpanid, {}, {}, UINT32_MAX,
                        [](ClientError aErr) { exit(static_cast<uint8_t>(aErr)); });
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
