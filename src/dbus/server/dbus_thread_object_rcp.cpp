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
#include <net/if.h>
#include <string.h>

#include <openthread/border_agent.h>
#include <openthread/border_router.h>
#include <openthread/channel_monitor.h>
#include <openthread/dnssd_server.h>
#include <openthread/instance.h>
#include <openthread/joiner.h>
#include <openthread/link_raw.h>
#include <openthread/nat64.h>
#include <openthread/ncp.h>
#include <openthread/netdata.h>
#include <openthread/openthread-system.h>
#include <openthread/srp_server.h>
#include <openthread/thread_ftd.h>
#include <openthread/trel.h>
#include <openthread/platform/radio.h>

#include "common/api_strings.hpp"
#include "common/byteswap.hpp"
#include "common/code_utils.hpp"
#include "dbus/common/constants.hpp"
#include "dbus/server/dbus_agent.hpp"
#include "dbus/server/dbus_thread_object_rcp.hpp"
#if OTBR_ENABLE_FEATURE_FLAGS
#include "proto/feature_flag.pb.h"
#endif
#if OTBR_ENABLE_TELEMETRY_DATA_API
#include "proto/thread_telemetry.pb.h"
#endif
#include "proto/capabilities.pb.h"

/**
 * @def OTBR_CONFIG_BORDER_AGENT_MESHCOP_E_UDP_PORT
 *
 * Specifies the border agent UDP port for meshcop-e service.
 * If zero, an ephemeral port will be used.
 */
#ifndef OTBR_CONFIG_BORDER_AGENT_MESHCOP_E_UDP_PORT
#define OTBR_CONFIG_BORDER_AGENT_MESHCOP_E_UDP_PORT 0
#endif

using std::placeholders::_1;
using std::placeholders::_2;

#if OTBR_ENABLE_NAT64
static std::string GetNat64StateName(otNat64State aState)
{
    std::string stateName;

    switch (aState)
    {
    case OT_NAT64_STATE_DISABLED:
        stateName = OTBR_NAT64_STATE_NAME_DISABLED;
        break;
    case OT_NAT64_STATE_NOT_RUNNING:
        stateName = OTBR_NAT64_STATE_NAME_NOT_RUNNING;
        break;
    case OT_NAT64_STATE_IDLE:
        stateName = OTBR_NAT64_STATE_NAME_IDLE;
        break;
    case OT_NAT64_STATE_ACTIVE:
        stateName = OTBR_NAT64_STATE_NAME_ACTIVE;
        break;
    }

    return stateName;
}
#endif // OTBR_ENABLE_NAT64

namespace otbr {
namespace DBus {

DBusThreadObjectRcp::DBusThreadObjectRcp(DBusConnection     &aConnection,
                                         const std::string  &aInterfaceName,
                                         otbr::Ncp::RcpHost &aHost,
                                         Mdns::Publisher    *aPublisher,
                                         otbr::BorderAgent  &aBorderAgent)
    : DBusObject(&aConnection, OTBR_DBUS_OBJECT_PREFIX + aInterfaceName)
    , mHost(aHost)
    , mPublisher(aPublisher)
    , mBorderAgent(aBorderAgent)
{
}

otbrError DBusThreadObjectRcp::Init(void)
{
    otbrError error        = OTBR_ERROR_NONE;
    auto      threadHelper = mHost.GetThreadHelper();

    SuccessOrExit(error = DBusObject::Initialize(false));

    threadHelper->AddDeviceRoleHandler(std::bind(&DBusThreadObjectRcp::DeviceRoleHandler, this, _1));
#if OTBR_ENABLE_DHCP6_PD && OTBR_ENABLE_BORDER_ROUTING
    threadHelper->SetDhcp6PdStateCallback(std::bind(&DBusThreadObjectRcp::Dhcp6PdStateHandler, this, _1));
#endif
    threadHelper->AddActiveDatasetChangeHandler(std::bind(&DBusThreadObjectRcp::ActiveDatasetChangeHandler, this, _1));
    mHost.RegisterResetHandler(std::bind(&DBusThreadObjectRcp::NcpResetHandler, this));

    RegisterMethod(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_SCAN_METHOD,
                   std::bind(&DBusThreadObjectRcp::ScanHandler, this, _1));
    RegisterMethod(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_ENERGY_SCAN_METHOD,
                   std::bind(&DBusThreadObjectRcp::EnergyScanHandler, this, _1));
    RegisterMethod(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_ATTACH_METHOD,
                   std::bind(&DBusThreadObjectRcp::AttachHandler, this, _1));
    RegisterMethod(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_DETACH_METHOD,
                   std::bind(&DBusThreadObjectRcp::DetachHandler, this, _1));
    RegisterMethod(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_FACTORY_RESET_METHOD,
                   std::bind(&DBusThreadObjectRcp::FactoryResetHandler, this, _1));
    RegisterMethod(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_RESET_METHOD,
                   std::bind(&DBusThreadObjectRcp::ResetHandler, this, _1));
    RegisterMethod(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_JOINER_START_METHOD,
                   std::bind(&DBusThreadObjectRcp::JoinerStartHandler, this, _1));
    RegisterMethod(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_JOINER_STOP_METHOD,
                   std::bind(&DBusThreadObjectRcp::JoinerStopHandler, this, _1));
    RegisterMethod(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PERMIT_UNSECURE_JOIN_METHOD,
                   std::bind(&DBusThreadObjectRcp::PermitUnsecureJoinHandler, this, _1));
    RegisterMethod(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_ADD_ON_MESH_PREFIX_METHOD,
                   std::bind(&DBusThreadObjectRcp::AddOnMeshPrefixHandler, this, _1));
    RegisterMethod(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_REMOVE_ON_MESH_PREFIX_METHOD,
                   std::bind(&DBusThreadObjectRcp::RemoveOnMeshPrefixHandler, this, _1));
    RegisterMethod(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_ADD_EXTERNAL_ROUTE_METHOD,
                   std::bind(&DBusThreadObjectRcp::AddExternalRouteHandler, this, _1));
    RegisterMethod(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_REMOVE_EXTERNAL_ROUTE_METHOD,
                   std::bind(&DBusThreadObjectRcp::RemoveExternalRouteHandler, this, _1));
    RegisterMethod(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_ATTACH_ALL_NODES_TO_METHOD,
                   std::bind(&DBusThreadObjectRcp::AttachAllNodesToHandler, this, _1));
    RegisterMethod(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_UPDATE_VENDOR_MESHCOP_TXT_METHOD,
                   std::bind(&DBusThreadObjectRcp::UpdateMeshCopTxtHandler, this, _1));
    RegisterMethod(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_GET_PROPERTIES_METHOD,
                   std::bind(&DBusThreadObjectRcp::GetPropertiesHandler, this, _1));
    RegisterMethod(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_LEAVE_NETWORK_METHOD,
                   std::bind(&DBusThreadObjectRcp::LeaveNetworkHandler, this, _1));
    RegisterMethod(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_SET_NAT64_ENABLED_METHOD,
                   std::bind(&DBusThreadObjectRcp::SetNat64Enabled, this, _1));
    RegisterMethod(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_ACTIVATE_EPHEMERAL_KEY_MODE_METHOD,
                   std::bind(&DBusThreadObjectRcp::ActivateEphemeralKeyModeHandler, this, _1));
    RegisterMethod(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_DEACTIVATE_EPHEMERAL_KEY_MODE_METHOD,
                   std::bind(&DBusThreadObjectRcp::DeactivateEphemeralKeyModeHandler, this, _1));

    RegisterMethod(DBUS_INTERFACE_INTROSPECTABLE, DBUS_INTROSPECT_METHOD,
                   std::bind(&DBusThreadObjectRcp::IntrospectHandler, this, _1));

    RegisterSetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_MESH_LOCAL_PREFIX,
                               std::bind(&DBusThreadObjectRcp::SetMeshLocalPrefixHandler, this, _1));
    RegisterSetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_LINK_MODE,
                               std::bind(&DBusThreadObjectRcp::SetLinkModeHandler, this, _1));
    RegisterSetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_ACTIVE_DATASET_TLVS,
                               std::bind(&DBusThreadObjectRcp::SetActiveDatasetTlvsHandler, this, _1));
    RegisterSetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_FEATURE_FLAG_LIST_DATA,
                               std::bind(&DBusThreadObjectRcp::SetFeatureFlagListDataHandler, this, _1));
    RegisterSetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_RADIO_REGION,
                               std::bind(&DBusThreadObjectRcp::SetRadioRegionHandler, this, _1));
    RegisterSetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_DNS_UPSTREAM_QUERY_STATE,
                               std::bind(&DBusThreadObjectRcp::SetDnsUpstreamQueryState, this, _1));
    RegisterSetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_NAT64_CIDR,
                               std::bind(&DBusThreadObjectRcp::SetNat64Cidr, this, _1));
    RegisterSetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_EPHEMERAL_KEY_ENABLED,
                               std::bind(&DBusThreadObjectRcp::SetEphemeralKeyEnabled, this, _1));

    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_LINK_MODE,
                               std::bind(&DBusThreadObjectRcp::GetLinkModeHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_DEVICE_ROLE,
                               std::bind(&DBusThreadObjectRcp::GetDeviceRoleHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_NETWORK_NAME,
                               std::bind(&DBusThreadObjectRcp::GetNetworkNameHandler, this, _1));

    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_PANID,
                               std::bind(&DBusThreadObjectRcp::GetPanIdHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_EXTPANID,
                               std::bind(&DBusThreadObjectRcp::GetExtPanIdHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_EUI64,
                               std::bind(&DBusThreadObjectRcp::GetEui64Handler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_CHANNEL,
                               std::bind(&DBusThreadObjectRcp::GetChannelHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_NETWORK_KEY,
                               std::bind(&DBusThreadObjectRcp::GetNetworkKeyHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_CCA_FAILURE_RATE,
                               std::bind(&DBusThreadObjectRcp::GetCcaFailureRateHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_LINK_COUNTERS,
                               std::bind(&DBusThreadObjectRcp::GetLinkCountersHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_IP6_COUNTERS,
                               std::bind(&DBusThreadObjectRcp::GetIp6CountersHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_SUPPORTED_CHANNEL_MASK,
                               std::bind(&DBusThreadObjectRcp::GetSupportedChannelMaskHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_PREFERRED_CHANNEL_MASK,
                               std::bind(&DBusThreadObjectRcp::GetPreferredChannelMaskHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_RLOC16,
                               std::bind(&DBusThreadObjectRcp::GetRloc16Handler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_EXTENDED_ADDRESS,
                               std::bind(&DBusThreadObjectRcp::GetExtendedAddressHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_ROUTER_ID,
                               std::bind(&DBusThreadObjectRcp::GetRouterIdHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_LEADER_DATA,
                               std::bind(&DBusThreadObjectRcp::GetLeaderDataHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_NETWORK_DATA_PRPOERTY,
                               std::bind(&DBusThreadObjectRcp::GetNetworkDataHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_STABLE_NETWORK_DATA_PRPOERTY,
                               std::bind(&DBusThreadObjectRcp::GetStableNetworkDataHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_LOCAL_LEADER_WEIGHT,
                               std::bind(&DBusThreadObjectRcp::GetLocalLeaderWeightHandler, this, _1));
#if OPENTHREAD_CONFIG_CHANNEL_MONITOR_ENABLE
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_CHANNEL_MONITOR_SAMPLE_COUNT,
                               std::bind(&DBusThreadObjectRcp::GetChannelMonitorSampleCountHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_CHANNEL_MONITOR_ALL_CHANNEL_QUALITIES,
                               std::bind(&DBusThreadObjectRcp::GetChannelMonitorAllChannelQualities, this, _1));
#endif
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_CHILD_TABLE,
                               std::bind(&DBusThreadObjectRcp::GetChildTableHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_NEIGHBOR_TABLE_PROEPRTY,
                               std::bind(&DBusThreadObjectRcp::GetNeighborTableHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_PARTITION_ID_PROEPRTY,
                               std::bind(&DBusThreadObjectRcp::GetPartitionIDHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_INSTANT_RSSI,
                               std::bind(&DBusThreadObjectRcp::GetInstantRssiHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_RADIO_TX_POWER,
                               std::bind(&DBusThreadObjectRcp::GetRadioTxPowerHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_EXTERNAL_ROUTES,
                               std::bind(&DBusThreadObjectRcp::GetExternalRoutesHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_ON_MESH_PREFIXES,
                               std::bind(&DBusThreadObjectRcp::GetOnMeshPrefixesHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_ACTIVE_DATASET_TLVS,
                               std::bind(&DBusThreadObjectRcp::GetActiveDatasetTlvsHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_PENDING_DATASET_TLVS,
                               std::bind(&DBusThreadObjectRcp::GetPendingDatasetTlvsHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_FEATURE_FLAG_LIST_DATA,
                               std::bind(&DBusThreadObjectRcp::GetFeatureFlagListDataHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_RADIO_REGION,
                               std::bind(&DBusThreadObjectRcp::GetRadioRegionHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_SRP_SERVER_INFO,
                               std::bind(&DBusThreadObjectRcp::GetSrpServerInfoHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_MDNS_TELEMETRY_INFO,
                               std::bind(&DBusThreadObjectRcp::GetMdnsTelemetryInfoHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_DNSSD_COUNTERS,
                               std::bind(&DBusThreadObjectRcp::GetDnssdCountersHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_OTBR_VERSION,
                               std::bind(&DBusThreadObjectRcp::GetOtbrVersionHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_OT_HOST_VERSION,
                               std::bind(&DBusThreadObjectRcp::GetOtHostVersionHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_OT_RCP_VERSION,
                               std::bind(&DBusThreadObjectRcp::GetOtRcpVersionHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_THREAD_VERSION,
                               std::bind(&DBusThreadObjectRcp::GetThreadVersionHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_RADIO_SPINEL_METRICS,
                               std::bind(&DBusThreadObjectRcp::GetRadioSpinelMetricsHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_RCP_INTERFACE_METRICS,
                               std::bind(&DBusThreadObjectRcp::GetRcpInterfaceMetricsHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_UPTIME,
                               std::bind(&DBusThreadObjectRcp::GetUptimeHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_RADIO_COEX_METRICS,
                               std::bind(&DBusThreadObjectRcp::GetRadioCoexMetrics, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_BORDER_ROUTING_COUNTERS,
                               std::bind(&DBusThreadObjectRcp::GetBorderRoutingCountersHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_NAT64_STATE,
                               std::bind(&DBusThreadObjectRcp::GetNat64State, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_NAT64_MAPPINGS,
                               std::bind(&DBusThreadObjectRcp::GetNat64Mappings, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_NAT64_PROTOCOL_COUNTERS,
                               std::bind(&DBusThreadObjectRcp::GetNat64ProtocolCounters, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_NAT64_ERROR_COUNTERS,
                               std::bind(&DBusThreadObjectRcp::GetNat64ErrorCounters, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_NAT64_CIDR,
                               std::bind(&DBusThreadObjectRcp::GetNat64Cidr, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_EPHEMERAL_KEY_ENABLED,
                               std::bind(&DBusThreadObjectRcp::GetEphemeralKeyEnabled, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_INFRA_LINK_INFO,
                               std::bind(&DBusThreadObjectRcp::GetInfraLinkInfo, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_TREL_INFO,
                               std::bind(&DBusThreadObjectRcp::GetTrelInfoHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_DNS_UPSTREAM_QUERY_STATE,
                               std::bind(&DBusThreadObjectRcp::GetDnsUpstreamQueryState, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_TELEMETRY_DATA,
                               std::bind(&DBusThreadObjectRcp::GetTelemetryDataHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_CAPABILITIES,
                               std::bind(&DBusThreadObjectRcp::GetCapabilitiesHandler, this, _1));

    SuccessOrExit(error = Signal(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_SIGNAL_READY, std::make_tuple()));

exit:
    return error;
}

void DBusThreadObjectRcp::DeviceRoleHandler(otDeviceRole aDeviceRole)
{
    SignalPropertyChanged(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_DEVICE_ROLE, GetDeviceRoleName(aDeviceRole));
}

#if OTBR_ENABLE_DHCP6_PD
void DBusThreadObjectRcp::Dhcp6PdStateHandler(otBorderRoutingDhcp6PdState aDhcp6PdState)
{
    SignalPropertyChanged(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_DHCP6_PD_STATE,
                          GetDhcp6PdStateName(aDhcp6PdState));
}
#endif

void DBusThreadObjectRcp::NcpResetHandler(void)
{
    mHost.GetThreadHelper()->AddDeviceRoleHandler(std::bind(&DBusThreadObjectRcp::DeviceRoleHandler, this, _1));
    mHost.GetThreadHelper()->AddActiveDatasetChangeHandler(
        std::bind(&DBusThreadObjectRcp::ActiveDatasetChangeHandler, this, _1));
    SignalPropertyChanged(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_DEVICE_ROLE,
                          GetDeviceRoleName(OT_DEVICE_ROLE_DISABLED));
}

void DBusThreadObjectRcp::ScanHandler(DBusRequest &aRequest)
{
    auto threadHelper = mHost.GetThreadHelper();
    threadHelper->Scan(std::bind(&DBusThreadObjectRcp::ReplyScanResult, this, aRequest, _1, _2));
}

void DBusThreadObjectRcp::ReplyScanResult(DBusRequest                           &aRequest,
                                          otError                                aError,
                                          const std::vector<otActiveScanResult> &aResult)
{
    std::vector<ActiveScanResult> results;

    if (aError != OT_ERROR_NONE)
    {
        aRequest.ReplyOtResult(aError);
    }
    else
    {
        for (const auto &r : aResult)
        {
            ActiveScanResult result = {};

            result.mExtAddress = ConvertOpenThreadUint64(r.mExtAddress.m8);
            result.mPanId      = r.mPanId;
            result.mChannel    = r.mChannel;
            result.mRssi       = r.mRssi;
            result.mLqi        = r.mLqi;

            results.emplace_back(result);
        }

        aRequest.Reply(std::tie(results));
    }
}

void DBusThreadObjectRcp::EnergyScanHandler(DBusRequest &aRequest)
{
    otError  error        = OT_ERROR_NONE;
    auto     threadHelper = mHost.GetThreadHelper();
    uint32_t scanDuration;

    auto args = std::tie(scanDuration);

    VerifyOrExit(DBusMessageToTuple(*aRequest.GetMessage(), args) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);
    threadHelper->EnergyScan(scanDuration,
                             std::bind(&DBusThreadObjectRcp::ReplyEnergyScanResult, this, aRequest, _1, _2));

exit:
    if (error != OT_ERROR_NONE)
    {
        aRequest.ReplyOtResult(error);
    }
}

void DBusThreadObjectRcp::ReplyEnergyScanResult(DBusRequest                           &aRequest,
                                                otError                                aError,
                                                const std::vector<otEnergyScanResult> &aResult)
{
    std::vector<EnergyScanResult> results;

    if (aError != OT_ERROR_NONE)
    {
        aRequest.ReplyOtResult(aError);
    }
    else
    {
        for (const auto &r : aResult)
        {
            EnergyScanResult result;

            result.mChannel = r.mChannel;
            result.mMaxRssi = r.mMaxRssi;

            results.emplace_back(result);
        }

        aRequest.Reply(std::tie(results));
    }
}

void DBusThreadObjectRcp::AttachHandler(DBusRequest &aRequest)
{
    auto                 threadHelper = mHost.GetThreadHelper();
    std::string          name;
    uint16_t             panid;
    uint64_t             extPanId;
    std::vector<uint8_t> networkKey;
    std::vector<uint8_t> pskc;
    uint32_t             channelMask;

    auto args = std::tie(networkKey, panid, name, extPanId, pskc, channelMask);

    if (IsDBusMessageEmpty(*aRequest.GetMessage()))
    {
        threadHelper->Attach([aRequest](otError aError, int64_t aAttachDelayMs) mutable {
            OT_UNUSED_VARIABLE(aAttachDelayMs);

            aRequest.ReplyOtResult(aError);
        });
    }
    else if (DBusMessageToTuple(*aRequest.GetMessage(), args) != OTBR_ERROR_NONE)
    {
        aRequest.ReplyOtResult(OT_ERROR_INVALID_ARGS);
    }
    else
    {
        threadHelper->Attach(name, panid, extPanId, networkKey, pskc, channelMask,
                             [aRequest](otError aError, int64_t aAttachDelayMs) mutable {
                                 OT_UNUSED_VARIABLE(aAttachDelayMs);

                                 aRequest.ReplyOtResult(aError);
                             });
    }
}

void DBusThreadObjectRcp::AttachAllNodesToHandler(DBusRequest &aRequest)
{
    std::vector<uint8_t> dataset;
    otError              error = OT_ERROR_NONE;

    auto args = std::tie(dataset);

    VerifyOrExit(DBusMessageToTuple(*aRequest.GetMessage(), args) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

    mHost.GetThreadHelper()->AttachAllNodesTo(dataset, [aRequest](otError error, int64_t aAttachDelayMs) mutable {
        aRequest.ReplyOtResult<int64_t>(error, aAttachDelayMs);
    });

exit:
    if (error != OT_ERROR_NONE)
    {
        aRequest.ReplyOtResult(error);
    }
}

void DBusThreadObjectRcp::DetachHandler(DBusRequest &aRequest)
{
    aRequest.ReplyOtResult(mHost.GetThreadHelper()->Detach());
}

void DBusThreadObjectRcp::FactoryResetHandler(DBusRequest &aRequest)
{
    otError error = OT_ERROR_NONE;

    SuccessOrExit(error = mHost.GetThreadHelper()->Detach());
    SuccessOrExit(otInstanceErasePersistentInfo(mHost.GetThreadHelper()->GetInstance()));
    mHost.Reset();

exit:
    aRequest.ReplyOtResult(error);
}

void DBusThreadObjectRcp::ResetHandler(DBusRequest &aRequest)
{
    mHost.Reset();
    aRequest.ReplyOtResult(OT_ERROR_NONE);
}

void DBusThreadObjectRcp::JoinerStartHandler(DBusRequest &aRequest)
{
    auto        threadHelper = mHost.GetThreadHelper();
    std::string pskd, provisionUrl, vendorName, vendorModel, vendorSwVersion, vendorData;
    auto        args = std::tie(pskd, provisionUrl, vendorName, vendorModel, vendorSwVersion, vendorData);

    if (DBusMessageToTuple(*aRequest.GetMessage(), args) != OTBR_ERROR_NONE)
    {
        aRequest.ReplyOtResult(OT_ERROR_INVALID_ARGS);
    }
    else
    {
        threadHelper->JoinerStart(pskd, provisionUrl, vendorName, vendorModel, vendorSwVersion, vendorData,
                                  [aRequest](otError aError) mutable { aRequest.ReplyOtResult(aError); });
    }
}

void DBusThreadObjectRcp::JoinerStopHandler(DBusRequest &aRequest)
{
    auto threadHelper = mHost.GetThreadHelper();

    otJoinerStop(threadHelper->GetInstance());
    aRequest.ReplyOtResult(OT_ERROR_NONE);
}

void DBusThreadObjectRcp::PermitUnsecureJoinHandler(DBusRequest &aRequest)
{
#ifdef OTBR_ENABLE_UNSECURE_JOIN
    auto     threadHelper = mHost.GetThreadHelper();
    uint16_t port;
    uint32_t timeout;
    auto     args = std::tie(port, timeout);

    if (DBusMessageToTuple(*aRequest.GetMessage(), args) != OTBR_ERROR_NONE)
    {
        aRequest.ReplyOtResult(OT_ERROR_INVALID_ARGS);
    }
    else
    {
        aRequest.ReplyOtResult(threadHelper->PermitUnsecureJoin(port, timeout));
    }
#else
    aRequest.ReplyOtResult(OT_ERROR_NOT_IMPLEMENTED);
#endif
}

void DBusThreadObjectRcp::AddOnMeshPrefixHandler(DBusRequest &aRequest)
{
    auto                 threadHelper = mHost.GetThreadHelper();
    OnMeshPrefix         onMeshPrefix;
    auto                 args  = std::tie(onMeshPrefix);
    otError              error = OT_ERROR_NONE;
    otBorderRouterConfig config;

    VerifyOrExit(DBusMessageToTuple(*aRequest.GetMessage(), args) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

    // size is guaranteed by parsing
    std::copy(onMeshPrefix.mPrefix.mPrefix.begin(), onMeshPrefix.mPrefix.mPrefix.end(),
              &config.mPrefix.mPrefix.mFields.m8[0]);
    config.mPrefix.mLength = onMeshPrefix.mPrefix.mLength;
    config.mPreference     = onMeshPrefix.mPreference;
    config.mSlaac          = onMeshPrefix.mSlaac;
    config.mDhcp           = onMeshPrefix.mDhcp;
    config.mConfigure      = onMeshPrefix.mConfigure;
    config.mDefaultRoute   = onMeshPrefix.mDefaultRoute;
    config.mOnMesh         = onMeshPrefix.mOnMesh;
    config.mStable         = onMeshPrefix.mStable;

    SuccessOrExit(error = otBorderRouterAddOnMeshPrefix(threadHelper->GetInstance(), &config));
    SuccessOrExit(error = otBorderRouterRegister(threadHelper->GetInstance()));

exit:
    aRequest.ReplyOtResult(error);
}

void DBusThreadObjectRcp::RemoveOnMeshPrefixHandler(DBusRequest &aRequest)
{
    auto        threadHelper = mHost.GetThreadHelper();
    Ip6Prefix   onMeshPrefix;
    auto        args  = std::tie(onMeshPrefix);
    otError     error = OT_ERROR_NONE;
    otIp6Prefix prefix;

    VerifyOrExit(DBusMessageToTuple(*aRequest.GetMessage(), args) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);
    // size is guaranteed by parsing
    std::copy(onMeshPrefix.mPrefix.begin(), onMeshPrefix.mPrefix.end(), &prefix.mPrefix.mFields.m8[0]);
    prefix.mLength = onMeshPrefix.mLength;

    SuccessOrExit(error = otBorderRouterRemoveOnMeshPrefix(threadHelper->GetInstance(), &prefix));
    SuccessOrExit(error = otBorderRouterRegister(threadHelper->GetInstance()));

exit:
    aRequest.ReplyOtResult(error);
}

void DBusThreadObjectRcp::AddExternalRouteHandler(DBusRequest &aRequest)
{
    auto                  threadHelper = mHost.GetThreadHelper();
    ExternalRoute         route;
    auto                  args  = std::tie(route);
    otError               error = OT_ERROR_NONE;
    otExternalRouteConfig otRoute;
    otIp6Prefix          &prefix = otRoute.mPrefix;

    VerifyOrExit(DBusMessageToTuple(*aRequest.GetMessage(), args) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

    // size is guaranteed by parsing
    std::copy(route.mPrefix.mPrefix.begin(), route.mPrefix.mPrefix.end(), &prefix.mPrefix.mFields.m8[0]);
    prefix.mLength      = route.mPrefix.mLength;
    otRoute.mPreference = route.mPreference;
    otRoute.mStable     = route.mStable;

    SuccessOrExit(error = otBorderRouterAddRoute(threadHelper->GetInstance(), &otRoute));
    if (route.mStable)
    {
        SuccessOrExit(error = otBorderRouterRegister(threadHelper->GetInstance()));
    }

exit:
    aRequest.ReplyOtResult(error);
}

void DBusThreadObjectRcp::RemoveExternalRouteHandler(DBusRequest &aRequest)
{
    auto        threadHelper = mHost.GetThreadHelper();
    Ip6Prefix   routePrefix;
    auto        args  = std::tie(routePrefix);
    otError     error = OT_ERROR_NONE;
    otIp6Prefix prefix;

    VerifyOrExit(DBusMessageToTuple(*aRequest.GetMessage(), args) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

    // size is guaranteed by parsing
    std::copy(routePrefix.mPrefix.begin(), routePrefix.mPrefix.end(), &prefix.mPrefix.mFields.m8[0]);
    prefix.mLength = routePrefix.mLength;

    SuccessOrExit(error = otBorderRouterRemoveRoute(threadHelper->GetInstance(), &prefix));
    SuccessOrExit(error = otBorderRouterRegister(threadHelper->GetInstance()));

exit:
    aRequest.ReplyOtResult(error);
}

void DBusThreadObjectRcp::IntrospectHandler(DBusRequest &aRequest)
{
    std::string xmlString(
#include "dbus/server/introspect.hpp"
    );

    aRequest.Reply(std::tie(xmlString));
}

otError DBusThreadObjectRcp::SetMeshLocalPrefixHandler(DBusMessageIter &aIter)
{
    auto                                      threadHelper = mHost.GetThreadHelper();
    otMeshLocalPrefix                         prefix;
    std::array<uint8_t, OTBR_IP6_PREFIX_SIZE> data{};
    otError                                   error = OT_ERROR_NONE;

    VerifyOrExit(DBusMessageExtractFromVariant(&aIter, data) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);
    memcpy(&prefix.m8, &data.front(), sizeof(prefix.m8));
    error = otThreadSetMeshLocalPrefix(threadHelper->GetInstance(), &prefix);

exit:
    return error;
}

otError DBusThreadObjectRcp::SetLinkModeHandler(DBusMessageIter &aIter)
{
    auto             threadHelper = mHost.GetThreadHelper();
    LinkModeConfig   cfg;
    otLinkModeConfig otCfg;
    otError          error = OT_ERROR_NONE;

    VerifyOrExit(DBusMessageExtractFromVariant(&aIter, cfg) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);
    otCfg.mDeviceType   = cfg.mDeviceType;
    otCfg.mNetworkData  = cfg.mNetworkData;
    otCfg.mRxOnWhenIdle = cfg.mRxOnWhenIdle;
    error               = otThreadSetLinkMode(threadHelper->GetInstance(), otCfg);

exit:
    return error;
}

otError DBusThreadObjectRcp::GetLinkModeHandler(DBusMessageIter &aIter)
{
    auto             threadHelper = mHost.GetThreadHelper();
    otLinkModeConfig otCfg        = otThreadGetLinkMode(threadHelper->GetInstance());
    LinkModeConfig   cfg;
    otError          error = OT_ERROR_NONE;

    cfg.mDeviceType   = otCfg.mDeviceType;
    cfg.mNetworkData  = otCfg.mNetworkData;
    cfg.mRxOnWhenIdle = otCfg.mRxOnWhenIdle;

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, cfg) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

exit:
    return error;
}

otError DBusThreadObjectRcp::GetDeviceRoleHandler(DBusMessageIter &aIter)
{
    auto         threadHelper = mHost.GetThreadHelper();
    otDeviceRole role         = otThreadGetDeviceRole(threadHelper->GetInstance());
    std::string  roleName     = GetDeviceRoleName(role);
    otError      error        = OT_ERROR_NONE;

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, roleName) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

exit:
    return error;
}

otError DBusThreadObjectRcp::GetNetworkNameHandler(DBusMessageIter &aIter)
{
    auto        threadHelper = mHost.GetThreadHelper();
    std::string networkName  = otThreadGetNetworkName(threadHelper->GetInstance());
    otError     error        = OT_ERROR_NONE;

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, networkName) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

exit:
    return error;
}

otError DBusThreadObjectRcp::GetPanIdHandler(DBusMessageIter &aIter)
{
    auto     threadHelper = mHost.GetThreadHelper();
    uint16_t panId        = otLinkGetPanId(threadHelper->GetInstance());
    otError  error        = OT_ERROR_NONE;

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, panId) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

exit:
    return error;
}

otError DBusThreadObjectRcp::GetExtPanIdHandler(DBusMessageIter &aIter)
{
    auto                   threadHelper = mHost.GetThreadHelper();
    const otExtendedPanId *extPanId     = otThreadGetExtendedPanId(threadHelper->GetInstance());
    uint64_t               extPanIdVal;
    otError                error = OT_ERROR_NONE;

    extPanIdVal = ConvertOpenThreadUint64(extPanId->m8);

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, extPanIdVal) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

exit:
    return error;
}

otError DBusThreadObjectRcp::GetChannelHandler(DBusMessageIter &aIter)
{
    auto     threadHelper = mHost.GetThreadHelper();
    uint16_t channel      = otLinkGetChannel(threadHelper->GetInstance());
    otError  error        = OT_ERROR_NONE;

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, channel) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

exit:
    return error;
}

otError DBusThreadObjectRcp::GetNetworkKeyHandler(DBusMessageIter &aIter)
{
    auto         threadHelper = mHost.GetThreadHelper();
    otNetworkKey networkKey;
    otError      error = OT_ERROR_NONE;

    otThreadGetNetworkKey(threadHelper->GetInstance(), &networkKey);
    std::vector<uint8_t> keyVal(networkKey.m8, networkKey.m8 + sizeof(networkKey.m8));
    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, keyVal) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

exit:
    return error;
}

otError DBusThreadObjectRcp::GetCcaFailureRateHandler(DBusMessageIter &aIter)
{
    auto     threadHelper = mHost.GetThreadHelper();
    uint16_t failureRate  = otLinkGetCcaFailureRate(threadHelper->GetInstance());
    otError  error        = OT_ERROR_NONE;

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, failureRate) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

exit:
    return error;
}

otError DBusThreadObjectRcp::GetLinkCountersHandler(DBusMessageIter &aIter)
{
    auto                 threadHelper = mHost.GetThreadHelper();
    const otMacCounters *otCounters   = otLinkGetCounters(threadHelper->GetInstance());
    MacCounters          counters;
    otError              error = OT_ERROR_NONE;

    counters.mTxTotal              = otCounters->mTxTotal;
    counters.mTxUnicast            = otCounters->mTxUnicast;
    counters.mTxBroadcast          = otCounters->mTxBroadcast;
    counters.mTxAckRequested       = otCounters->mTxAckRequested;
    counters.mTxAcked              = otCounters->mTxAcked;
    counters.mTxNoAckRequested     = otCounters->mTxNoAckRequested;
    counters.mTxData               = otCounters->mTxData;
    counters.mTxDataPoll           = otCounters->mTxDataPoll;
    counters.mTxBeacon             = otCounters->mTxBeacon;
    counters.mTxBeaconRequest      = otCounters->mTxBeaconRequest;
    counters.mTxOther              = otCounters->mTxOther;
    counters.mTxRetry              = otCounters->mTxRetry;
    counters.mTxErrCca             = otCounters->mTxErrCca;
    counters.mTxErrAbort           = otCounters->mTxErrAbort;
    counters.mTxErrBusyChannel     = otCounters->mTxErrBusyChannel;
    counters.mRxTotal              = otCounters->mRxTotal;
    counters.mRxUnicast            = otCounters->mRxUnicast;
    counters.mRxBroadcast          = otCounters->mRxBroadcast;
    counters.mRxData               = otCounters->mRxData;
    counters.mRxDataPoll           = otCounters->mRxDataPoll;
    counters.mRxBeacon             = otCounters->mRxBeacon;
    counters.mRxBeaconRequest      = otCounters->mRxBeaconRequest;
    counters.mRxOther              = otCounters->mRxOther;
    counters.mRxAddressFiltered    = otCounters->mRxAddressFiltered;
    counters.mRxDestAddrFiltered   = otCounters->mRxDestAddrFiltered;
    counters.mRxDuplicated         = otCounters->mRxDuplicated;
    counters.mRxErrNoFrame         = otCounters->mRxErrNoFrame;
    counters.mRxErrUnknownNeighbor = otCounters->mRxErrUnknownNeighbor;
    counters.mRxErrInvalidSrcAddr  = otCounters->mRxErrInvalidSrcAddr;
    counters.mRxErrSec             = otCounters->mRxErrSec;
    counters.mRxErrFcs             = otCounters->mRxErrFcs;
    counters.mRxErrOther           = otCounters->mRxErrOther;

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, counters) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

exit:
    return error;
}

otError DBusThreadObjectRcp::GetIp6CountersHandler(DBusMessageIter &aIter)
{
    auto                threadHelper = mHost.GetThreadHelper();
    const otIpCounters *otCounters   = otThreadGetIp6Counters(threadHelper->GetInstance());
    IpCounters          counters;
    otError             error = OT_ERROR_NONE;

    counters.mTxSuccess = otCounters->mTxSuccess;
    counters.mTxFailure = otCounters->mTxFailure;
    counters.mRxSuccess = otCounters->mRxSuccess;
    counters.mRxFailure = otCounters->mRxFailure;

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, counters) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

exit:
    return error;
}

otError DBusThreadObjectRcp::GetSupportedChannelMaskHandler(DBusMessageIter &aIter)
{
    auto     threadHelper = mHost.GetThreadHelper();
    uint32_t channelMask  = otLinkGetSupportedChannelMask(threadHelper->GetInstance());
    otError  error        = OT_ERROR_NONE;

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, channelMask) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

exit:
    return error;
}

otError DBusThreadObjectRcp::GetPreferredChannelMaskHandler(DBusMessageIter &aIter)
{
    auto     threadHelper = mHost.GetThreadHelper();
    uint32_t channelMask  = otPlatRadioGetPreferredChannelMask(threadHelper->GetInstance());
    otError  error        = OT_ERROR_NONE;

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, channelMask) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

exit:
    return error;
}

otError DBusThreadObjectRcp::GetRloc16Handler(DBusMessageIter &aIter)
{
    auto     threadHelper = mHost.GetThreadHelper();
    otError  error        = OT_ERROR_NONE;
    uint16_t rloc16       = otThreadGetRloc16(threadHelper->GetInstance());

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, rloc16) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

exit:
    return error;
}

otError DBusThreadObjectRcp::GetExtendedAddressHandler(DBusMessageIter &aIter)
{
    auto                threadHelper    = mHost.GetThreadHelper();
    otError             error           = OT_ERROR_NONE;
    const otExtAddress *addr            = otLinkGetExtendedAddress(threadHelper->GetInstance());
    uint64_t            extendedAddress = ConvertOpenThreadUint64(addr->m8);

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, extendedAddress) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

exit:
    return error;
}

otError DBusThreadObjectRcp::GetRouterIdHandler(DBusMessageIter &aIter)
{
    auto         threadHelper = mHost.GetThreadHelper();
    otError      error        = OT_ERROR_NONE;
    uint16_t     rloc16       = otThreadGetRloc16(threadHelper->GetInstance());
    otRouterInfo info;

    VerifyOrExit(otThreadGetRouterInfo(threadHelper->GetInstance(), rloc16, &info) == OT_ERROR_NONE,
                 error = OT_ERROR_INVALID_STATE);
    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, info.mRouterId) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

exit:
    return error;
}

otError DBusThreadObjectRcp::GetLeaderDataHandler(DBusMessageIter &aIter)
{
    auto                threadHelper = mHost.GetThreadHelper();
    otError             error        = OT_ERROR_NONE;
    struct otLeaderData data;
    LeaderData          leaderData;

    SuccessOrExit(error = otThreadGetLeaderData(threadHelper->GetInstance(), &data));
    leaderData.mPartitionId       = data.mPartitionId;
    leaderData.mWeighting         = data.mWeighting;
    leaderData.mDataVersion       = data.mDataVersion;
    leaderData.mStableDataVersion = data.mStableDataVersion;
    leaderData.mLeaderRouterId    = data.mLeaderRouterId;
    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, leaderData) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

exit:
    return error;
}

otError DBusThreadObjectRcp::GetNetworkDataHandler(DBusMessageIter &aIter)
{
    static constexpr size_t kNetworkDataMaxSize = 255;
    auto                    threadHelper        = mHost.GetThreadHelper();
    otError                 error               = OT_ERROR_NONE;
    uint8_t                 data[kNetworkDataMaxSize];
    uint8_t                 len = sizeof(data);
    std::vector<uint8_t>    networkData;

    SuccessOrExit(error = otNetDataGet(threadHelper->GetInstance(), /*stable=*/false, data, &len));
    networkData = std::vector<uint8_t>(&data[0], &data[len]);
    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, networkData) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

exit:
    return error;
}

otError DBusThreadObjectRcp::GetStableNetworkDataHandler(DBusMessageIter &aIter)
{
    static constexpr size_t kNetworkDataMaxSize = 255;
    auto                    threadHelper        = mHost.GetThreadHelper();
    otError                 error               = OT_ERROR_NONE;
    uint8_t                 data[kNetworkDataMaxSize];
    uint8_t                 len = sizeof(data);
    std::vector<uint8_t>    networkData;

    SuccessOrExit(error = otNetDataGet(threadHelper->GetInstance(), /*stable=*/true, data, &len));
    networkData = std::vector<uint8_t>(&data[0], &data[len]);
    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, networkData) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

exit:
    return error;
}

otError DBusThreadObjectRcp::GetLocalLeaderWeightHandler(DBusMessageIter &aIter)
{
    auto    threadHelper = mHost.GetThreadHelper();
    otError error        = OT_ERROR_NONE;
    uint8_t weight       = otThreadGetLocalLeaderWeight(threadHelper->GetInstance());

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, weight) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

exit:
    return error;
}

otError DBusThreadObjectRcp::GetChannelMonitorSampleCountHandler(DBusMessageIter &aIter)
{
#if OPENTHREAD_CONFIG_CHANNEL_MONITOR_ENABLE
    auto     threadHelper = mHost.GetThreadHelper();
    otError  error        = OT_ERROR_NONE;
    uint32_t cnt          = otChannelMonitorGetSampleCount(threadHelper->GetInstance());

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, cnt) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

exit:
    return error;
#else  // OPENTHREAD_CONFIG_CHANNEL_MONITOR_ENABLE
    OTBR_UNUSED_VARIABLE(aIter);
    return OT_ERROR_NOT_IMPLEMENTED;
#endif // OPENTHREAD_CONFIG_CHANNEL_MONITOR_ENABLE
}

otError DBusThreadObjectRcp::GetChannelMonitorAllChannelQualities(DBusMessageIter &aIter)
{
#if OPENTHREAD_CONFIG_CHANNEL_MONITOR_ENABLE
    auto                        threadHelper = mHost.GetThreadHelper();
    otError                     error        = OT_ERROR_NONE;
    uint32_t                    channelMask  = otLinkGetSupportedChannelMask(threadHelper->GetInstance());
    constexpr uint8_t           kNumChannels = sizeof(channelMask) * 8; // 8 bit per byte
    std::vector<ChannelQuality> quality;

    for (uint8_t i = 0; i < kNumChannels; i++)
    {
        if (channelMask & (1U << i))
        {
            uint16_t occupancy = otChannelMonitorGetChannelOccupancy(threadHelper->GetInstance(), i);

            quality.emplace_back(ChannelQuality{i, occupancy});
        }
    }

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, quality) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

exit:
    return error;
#else  // OPENTHREAD_CONFIG_CHANNEL_MONITOR_ENABLE
    OTBR_UNUSED_VARIABLE(aIter);
    return OT_ERROR_NOT_IMPLEMENTED;
#endif // OPENTHREAD_CONFIG_CHANNEL_MONITOR_ENABLE
}

otError DBusThreadObjectRcp::GetChildTableHandler(DBusMessageIter &aIter)
{
    auto                   threadHelper = mHost.GetThreadHelper();
    otError                error        = OT_ERROR_NONE;
    uint16_t               childIndex   = 0;
    otChildInfo            childInfo;
    std::vector<ChildInfo> childTable;

    while (otThreadGetChildInfoByIndex(threadHelper->GetInstance(), childIndex, &childInfo) == OT_ERROR_NONE)
    {
        ChildInfo info;

        info.mExtAddress         = ConvertOpenThreadUint64(childInfo.mExtAddress.m8);
        info.mTimeout            = childInfo.mTimeout;
        info.mAge                = childInfo.mAge;
        info.mChildId            = childInfo.mChildId;
        info.mNetworkDataVersion = childInfo.mNetworkDataVersion;
        info.mLinkQualityIn      = childInfo.mLinkQualityIn;
        info.mAverageRssi        = childInfo.mAverageRssi;
        info.mLastRssi           = childInfo.mLastRssi;
        info.mFrameErrorRate     = childInfo.mFrameErrorRate;
        info.mMessageErrorRate   = childInfo.mMessageErrorRate;
        info.mRxOnWhenIdle       = childInfo.mRxOnWhenIdle;
        info.mFullThreadDevice   = childInfo.mFullThreadDevice;
        info.mFullNetworkData    = childInfo.mFullNetworkData;
        info.mIsStateRestoring   = childInfo.mIsStateRestoring;
        childTable.push_back(info);
        childIndex++;
    }

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, childTable) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

exit:
    return error;
}

otError DBusThreadObjectRcp::GetNeighborTableHandler(DBusMessageIter &aIter)
{
    auto                      threadHelper = mHost.GetThreadHelper();
    otError                   error        = OT_ERROR_NONE;
    otNeighborInfoIterator    iter         = OT_NEIGHBOR_INFO_ITERATOR_INIT;
    otNeighborInfo            neighborInfo;
    std::vector<NeighborInfo> neighborTable;

    while (otThreadGetNextNeighborInfo(threadHelper->GetInstance(), &iter, &neighborInfo) == OT_ERROR_NONE)
    {
        NeighborInfo info;

        info.mExtAddress       = ConvertOpenThreadUint64(neighborInfo.mExtAddress.m8);
        info.mAge              = neighborInfo.mAge;
        info.mRloc16           = neighborInfo.mRloc16;
        info.mLinkFrameCounter = neighborInfo.mLinkFrameCounter;
        info.mMleFrameCounter  = neighborInfo.mMleFrameCounter;
        info.mLinkQualityIn    = neighborInfo.mLinkQualityIn;
        info.mAverageRssi      = neighborInfo.mAverageRssi;
        info.mLastRssi         = neighborInfo.mLastRssi;
        info.mFrameErrorRate   = neighborInfo.mFrameErrorRate;
        info.mMessageErrorRate = neighborInfo.mMessageErrorRate;
        info.mVersion          = neighborInfo.mVersion;
        info.mRxOnWhenIdle     = neighborInfo.mRxOnWhenIdle;
        info.mFullThreadDevice = neighborInfo.mFullThreadDevice;
        info.mFullNetworkData  = neighborInfo.mFullNetworkData;
        info.mIsChild          = neighborInfo.mIsChild;
        neighborTable.push_back(info);
    }

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, neighborTable) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

exit:
    return error;
}

otError DBusThreadObjectRcp::GetPartitionIDHandler(DBusMessageIter &aIter)
{
    auto     threadHelper = mHost.GetThreadHelper();
    otError  error        = OT_ERROR_NONE;
    uint32_t partitionId  = otThreadGetPartitionId(threadHelper->GetInstance());

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, partitionId) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

exit:
    return error;
}

otError DBusThreadObjectRcp::GetInstantRssiHandler(DBusMessageIter &aIter)
{
    auto    threadHelper = mHost.GetThreadHelper();
    otError error        = OT_ERROR_NONE;
    int8_t  rssi         = otPlatRadioGetRssi(threadHelper->GetInstance());

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, rssi) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

exit:
    return error;
}

otError DBusThreadObjectRcp::GetRadioTxPowerHandler(DBusMessageIter &aIter)
{
    auto    threadHelper = mHost.GetThreadHelper();
    otError error        = OT_ERROR_NONE;
    int8_t  txPower;

    SuccessOrExit(error = otPlatRadioGetTransmitPower(threadHelper->GetInstance(), &txPower));

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, txPower) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

exit:
    return error;
}

otError DBusThreadObjectRcp::GetExternalRoutesHandler(DBusMessageIter &aIter)
{
    auto                       threadHelper = mHost.GetThreadHelper();
    otError                    error        = OT_ERROR_NONE;
    otNetworkDataIterator      iter         = OT_NETWORK_DATA_ITERATOR_INIT;
    otExternalRouteConfig      config;
    std::vector<ExternalRoute> externalRouteTable;

    while (otNetDataGetNextRoute(threadHelper->GetInstance(), &iter, &config) == OT_ERROR_NONE)
    {
        ExternalRoute route;

        route.mPrefix.mPrefix      = std::vector<uint8_t>(&config.mPrefix.mPrefix.mFields.m8[0],
                                                     &config.mPrefix.mPrefix.mFields.m8[OTBR_IP6_PREFIX_SIZE]);
        route.mPrefix.mLength      = config.mPrefix.mLength;
        route.mRloc16              = config.mRloc16;
        route.mPreference          = config.mPreference;
        route.mStable              = config.mStable;
        route.mNextHopIsThisDevice = config.mNextHopIsThisDevice;
        externalRouteTable.push_back(route);
    }

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, externalRouteTable) == OTBR_ERROR_NONE,
                 error = OT_ERROR_INVALID_ARGS);

exit:
    return error;
}

otError DBusThreadObjectRcp::GetOnMeshPrefixesHandler(DBusMessageIter &aIter)
{
    auto                      threadHelper = mHost.GetThreadHelper();
    otError                   error        = OT_ERROR_NONE;
    otNetworkDataIterator     iter         = OT_NETWORK_DATA_ITERATOR_INIT;
    otBorderRouterConfig      config;
    std::vector<OnMeshPrefix> onMeshPrefixes;

    while (otNetDataGetNextOnMeshPrefix(threadHelper->GetInstance(), &iter, &config) == OT_ERROR_NONE)
    {
        OnMeshPrefix prefix;

        prefix.mPrefix.mPrefix = std::vector<uint8_t>(&config.mPrefix.mPrefix.mFields.m8[0],
                                                      &config.mPrefix.mPrefix.mFields.m8[OTBR_IP6_PREFIX_SIZE]);
        prefix.mPrefix.mLength = config.mPrefix.mLength;
        prefix.mRloc16         = config.mRloc16;
        prefix.mPreference     = config.mPreference;
        prefix.mPreferred      = config.mPreferred;
        prefix.mSlaac          = config.mSlaac;
        prefix.mDhcp           = config.mDhcp;
        prefix.mConfigure      = config.mConfigure;
        prefix.mDefaultRoute   = config.mDefaultRoute;
        prefix.mOnMesh         = config.mOnMesh;
        prefix.mStable         = config.mStable;
        prefix.mNdDns          = config.mNdDns;
        prefix.mDp             = config.mDp;
        onMeshPrefixes.push_back(prefix);
    }
    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, onMeshPrefixes) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

exit:
    return error;
}

otError DBusThreadObjectRcp::SetActiveDatasetTlvsHandler(DBusMessageIter &aIter)
{
    auto                     threadHelper = mHost.GetThreadHelper();
    std::vector<uint8_t>     data;
    otOperationalDatasetTlvs datasetTlvs;
    otError                  error = OT_ERROR_NONE;

    VerifyOrExit(DBusMessageExtractFromVariant(&aIter, data) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);
    VerifyOrExit(data.size() <= sizeof(datasetTlvs.mTlvs));
    std::copy(std::begin(data), std::end(data), std::begin(datasetTlvs.mTlvs));
    datasetTlvs.mLength = data.size();
    error               = otDatasetSetActiveTlvs(threadHelper->GetInstance(), &datasetTlvs);

exit:
    return error;
}

otError DBusThreadObjectRcp::GetActiveDatasetTlvsHandler(DBusMessageIter &aIter)
{
    auto                     threadHelper = mHost.GetThreadHelper();
    otError                  error        = OT_ERROR_NONE;
    std::vector<uint8_t>     data;
    otOperationalDatasetTlvs datasetTlvs;

    SuccessOrExit(error = otDatasetGetActiveTlvs(threadHelper->GetInstance(), &datasetTlvs));
    data = std::vector<uint8_t>{std::begin(datasetTlvs.mTlvs), std::begin(datasetTlvs.mTlvs) + datasetTlvs.mLength};

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, data) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

exit:
    return error;
}

otError DBusThreadObjectRcp::GetPendingDatasetTlvsHandler(DBusMessageIter &aIter)
{
    auto                     threadHelper = mHost.GetThreadHelper();
    otError                  error        = OT_ERROR_NONE;
    std::vector<uint8_t>     data;
    otOperationalDatasetTlvs datasetTlvs;

    SuccessOrExit(error = otDatasetGetPendingTlvs(threadHelper->GetInstance(), &datasetTlvs));
    data = std::vector<uint8_t>{std::begin(datasetTlvs.mTlvs), std::begin(datasetTlvs.mTlvs) + datasetTlvs.mLength};

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, data) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

exit:
    return error;
}

otError DBusThreadObjectRcp::SetFeatureFlagListDataHandler(DBusMessageIter &aIter)
{
#if OTBR_ENABLE_FEATURE_FLAGS
    otError              error = OT_ERROR_NONE;
    std::vector<uint8_t> data;
    FeatureFlagList      featureFlagList;

    VerifyOrExit(DBusMessageExtractFromVariant(&aIter, data) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);
    VerifyOrExit(featureFlagList.ParseFromString(std::string(data.begin(), data.end())), error = OT_ERROR_INVALID_ARGS);
    // TODO: implement the feature flag handler at every component
    mBorderAgent.SetEphemeralKeyEnabled(featureFlagList.enable_ephemeralkey());
    otbrLogInfo("Border Agent Ephemeral Key Feature has been %s by feature flag",
                (featureFlagList.enable_ephemeralkey() ? "enable" : "disable"));
    VerifyOrExit((error = mHost.ApplyFeatureFlagList(featureFlagList)) == OT_ERROR_NONE);
exit:
    return error;
#else
    OTBR_UNUSED_VARIABLE(aIter);
    return OT_ERROR_NOT_IMPLEMENTED;
#endif
}

otError DBusThreadObjectRcp::GetFeatureFlagListDataHandler(DBusMessageIter &aIter)
{
#if OTBR_ENABLE_FEATURE_FLAGS
    otError              error                       = OT_ERROR_NONE;
    const std::string    appliedFeatureFlagListBytes = mHost.GetAppliedFeatureFlagListBytes();
    std::vector<uint8_t> data(appliedFeatureFlagListBytes.begin(), appliedFeatureFlagListBytes.end());

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, data) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

exit:
    return error;
#else
    OTBR_UNUSED_VARIABLE(aIter);
    return OT_ERROR_NOT_IMPLEMENTED;
#endif
}

otError DBusThreadObjectRcp::SetRadioRegionHandler(DBusMessageIter &aIter)
{
    auto        threadHelper = mHost.GetThreadHelper();
    std::string radioRegion;
    uint16_t    regionCode;
    otError     error = OT_ERROR_NONE;

    VerifyOrExit(DBusMessageExtractFromVariant(&aIter, radioRegion) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);
    VerifyOrExit(radioRegion.size() == sizeof(uint16_t), error = OT_ERROR_INVALID_ARGS);
    regionCode = radioRegion[0] << 8 | radioRegion[1];

    error = otPlatRadioSetRegion(threadHelper->GetInstance(), regionCode);

exit:
    return error;
}

void DBusThreadObjectRcp::UpdateMeshCopTxtHandler(DBusRequest &aRequest)
{
    auto                                        threadHelper = mHost.GetThreadHelper();
    otError                                     error        = OT_ERROR_NONE;
    std::map<std::string, std::vector<uint8_t>> update;
    std::vector<TxtEntry>                       updatedTxtEntries;
    auto                                        args = std::tie(updatedTxtEntries);

    VerifyOrExit(DBusMessageToTuple(*aRequest.GetMessage(), args) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);
    for (const auto &entry : updatedTxtEntries)
    {
        update[entry.mKey] = entry.mValue;
    }
    for (const auto reservedKey : {"rv", "tv", "sb", "nn", "xp", "at", "pt", "dn", "sq", "bb", "omr"})
    {
        VerifyOrExit(!update.count(reservedKey), error = OT_ERROR_INVALID_ARGS);
    }
    threadHelper->OnUpdateMeshCopTxt(std::move(update));

exit:
    aRequest.ReplyOtResult(error);
}

otError DBusThreadObjectRcp::GetRadioRegionHandler(DBusMessageIter &aIter)
{
    auto        threadHelper = mHost.GetThreadHelper();
    otError     error        = OT_ERROR_NONE;
    std::string radioRegion;
    uint16_t    regionCode;

    SuccessOrExit(error = otPlatRadioGetRegion(threadHelper->GetInstance(), &regionCode));
    radioRegion.resize(sizeof(uint16_t), '\0');
    radioRegion[0] = static_cast<char>((regionCode >> 8) & 0xff);
    radioRegion[1] = static_cast<char>(regionCode & 0xff);

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, radioRegion) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

exit:
    return error;
}

otError DBusThreadObjectRcp::GetSrpServerInfoHandler(DBusMessageIter &aIter)
{
#if OTBR_ENABLE_SRP_ADVERTISING_PROXY
    auto                               threadHelper = mHost.GetThreadHelper();
    auto                               instance     = threadHelper->GetInstance();
    otError                            error        = OT_ERROR_NONE;
    SrpServerInfo                      srpServerInfo{};
    otSrpServerLeaseInfo               leaseInfo;
    const otSrpServerHost             *host             = nullptr;
    const otSrpServerResponseCounters *responseCounters = otSrpServerGetResponseCounters(instance);

    srpServerInfo.mState       = SrpServerState(static_cast<uint8_t>(otSrpServerGetState(instance)));
    srpServerInfo.mPort        = otSrpServerGetPort(instance);
    srpServerInfo.mAddressMode = SrpServerAddressMode(static_cast<uint8_t>(otSrpServerGetAddressMode(instance)));

    while ((host = otSrpServerGetNextHost(instance, host)))
    {
        const otSrpServerService *service = nullptr;

        if (otSrpServerHostIsDeleted(host))
        {
            ++srpServerInfo.mHosts.mDeletedCount;
        }
        else
        {
            ++srpServerInfo.mHosts.mFreshCount;
            otSrpServerHostGetLeaseInfo(host, &leaseInfo);
            srpServerInfo.mHosts.mLeaseTimeTotal += leaseInfo.mLease;
            srpServerInfo.mHosts.mKeyLeaseTimeTotal += leaseInfo.mKeyLease;
            srpServerInfo.mHosts.mRemainingLeaseTimeTotal += leaseInfo.mRemainingLease;
            srpServerInfo.mHosts.mRemainingKeyLeaseTimeTotal += leaseInfo.mRemainingKeyLease;
        }

        while ((service = otSrpServerHostGetNextService(host, service)))
        {
            if (otSrpServerServiceIsDeleted(service))
            {
                ++srpServerInfo.mServices.mDeletedCount;
            }
            else
            {
                ++srpServerInfo.mServices.mFreshCount;
                otSrpServerServiceGetLeaseInfo(service, &leaseInfo);
                srpServerInfo.mServices.mLeaseTimeTotal += leaseInfo.mLease;
                srpServerInfo.mServices.mKeyLeaseTimeTotal += leaseInfo.mKeyLease;
                srpServerInfo.mServices.mRemainingLeaseTimeTotal += leaseInfo.mRemainingLease;
                srpServerInfo.mServices.mRemainingKeyLeaseTimeTotal += leaseInfo.mRemainingKeyLease;
            }
        }
    }

    srpServerInfo.mResponseCounters.mSuccess       = responseCounters->mSuccess;
    srpServerInfo.mResponseCounters.mServerFailure = responseCounters->mServerFailure;
    srpServerInfo.mResponseCounters.mFormatError   = responseCounters->mFormatError;
    srpServerInfo.mResponseCounters.mNameExists    = responseCounters->mNameExists;
    srpServerInfo.mResponseCounters.mRefused       = responseCounters->mRefused;
    srpServerInfo.mResponseCounters.mOther         = responseCounters->mOther;

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, srpServerInfo) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

exit:
    return error;
#else  // OTBR_ENABLE_SRP_ADVERTISING_PROXY
    OTBR_UNUSED_VARIABLE(aIter);

    return OT_ERROR_NOT_IMPLEMENTED;
#endif // OTBR_ENABLE_SRP_ADVERTISING_PROXY
}

otError DBusThreadObjectRcp::GetMdnsTelemetryInfoHandler(DBusMessageIter &aIter)
{
    otError error = OT_ERROR_NONE;

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, mPublisher->GetMdnsTelemetryInfo()) == OTBR_ERROR_NONE,
                 error = OT_ERROR_INVALID_ARGS);
exit:
    return error;
}

otError DBusThreadObjectRcp::GetDnssdCountersHandler(DBusMessageIter &aIter)
{
#if OTBR_ENABLE_DNSSD_DISCOVERY_PROXY
    auto            threadHelper = mHost.GetThreadHelper();
    auto            instance     = threadHelper->GetInstance();
    otError         error        = OT_ERROR_NONE;
    DnssdCounters   dnssdCounters;
    otDnssdCounters otDnssdCounters = *otDnssdGetCounters(instance);

    dnssdCounters.mSuccessResponse        = otDnssdCounters.mSuccessResponse;
    dnssdCounters.mServerFailureResponse  = otDnssdCounters.mServerFailureResponse;
    dnssdCounters.mFormatErrorResponse    = otDnssdCounters.mFormatErrorResponse;
    dnssdCounters.mNameErrorResponse      = otDnssdCounters.mNameErrorResponse;
    dnssdCounters.mNotImplementedResponse = otDnssdCounters.mNotImplementedResponse;
    dnssdCounters.mOtherResponse          = otDnssdCounters.mOtherResponse;

    dnssdCounters.mResolvedBySrp = otDnssdCounters.mResolvedBySrp;

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, dnssdCounters) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

exit:
    return error;
#else  // OTBR_ENABLE_DNSSD_DISCOVERY_PROXY
    OTBR_UNUSED_VARIABLE(aIter);

    return OT_ERROR_NOT_IMPLEMENTED;
#endif // OTBR_ENABLE_DNSSD_DISCOVERY_PROXY
}

otError DBusThreadObjectRcp::GetTrelInfoHandler(DBusMessageIter &aIter)
{
#if OTBR_ENABLE_TREL
    auto           instance = mHost.GetThreadHelper()->GetInstance();
    otError        error    = OT_ERROR_NONE;
    TrelInfo       trelInfo;
    otTrelCounters otTrelCounters = *otTrelGetCounters(instance);

    trelInfo.mTrelCounters.mTxPackets = otTrelCounters.mTxPackets;
    trelInfo.mTrelCounters.mTxBytes   = otTrelCounters.mTxBytes;
    trelInfo.mTrelCounters.mTxFailure = otTrelCounters.mTxFailure;
    trelInfo.mTrelCounters.mRxPackets = otTrelCounters.mRxPackets;
    trelInfo.mTrelCounters.mRxBytes   = otTrelCounters.mRxBytes;

    trelInfo.mNumTrelPeers = otTrelGetNumberOfPeers(instance);
    trelInfo.mEnabled      = otTrelIsEnabled(instance);

    SuccessOrExit(DBusMessageEncodeToVariant(&aIter, trelInfo), error = OT_ERROR_INVALID_ARGS);
exit:
    return error;
#else  // OTBR_ENABLE_TREL
    OTBR_UNUSED_VARIABLE(aIter);

    return OT_ERROR_NOT_IMPLEMENTED;
#endif // OTBR_ENABLE_TREL
}

otError DBusThreadObjectRcp::GetTelemetryDataHandler(DBusMessageIter &aIter)
{
#if OTBR_ENABLE_TELEMETRY_DATA_API
    otError                      error = OT_ERROR_NONE;
    threadnetwork::TelemetryData telemetryData;
    auto                         threadHelper = mHost.GetThreadHelper();

    if (threadHelper->RetrieveTelemetryData(mPublisher, telemetryData) != OT_ERROR_NONE)
    {
        otbrLogWarning("Some metrics were not populated in RetrieveTelemetryData");
    }

    {
        const std::string    telemetryDataBytes = telemetryData.SerializeAsString();
        std::vector<uint8_t> data(telemetryDataBytes.begin(), telemetryDataBytes.end());

        VerifyOrExit(DBusMessageEncodeToVariant(&aIter, data) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);
    }

exit:
    return error;
#else
    OTBR_UNUSED_VARIABLE(aIter);
    return OT_ERROR_NOT_IMPLEMENTED;
#endif
}

otError DBusThreadObjectRcp::GetCapabilitiesHandler(DBusMessageIter &aIter)
{
    otError            error = OT_ERROR_NONE;
    otbr::Capabilities capabilities;

    capabilities.set_nat64(OTBR_ENABLE_NAT64);
    capabilities.set_dhcp6_pd(OTBR_ENABLE_DHCP6_PD);

    {
        const std::string    dataBytes = capabilities.SerializeAsString();
        std::vector<uint8_t> data(dataBytes.begin(), dataBytes.end());

        VerifyOrExit(DBusMessageEncodeToVariant(&aIter, data) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);
    }

exit:
    return error;
}

void DBusThreadObjectRcp::GetPropertiesHandler(DBusRequest &aRequest)
{
    UniqueDBusMessage        reply(dbus_message_new_method_return(aRequest.GetMessage()));
    DBusMessageIter          iter;
    DBusMessageIter          replyIter;
    DBusMessageIter          replySubIter;
    std::vector<std::string> propertyNames;
    otError                  error = OT_ERROR_NONE;

    VerifyOrExit(reply != nullptr, error = OT_ERROR_NO_BUFS);
    VerifyOrExit(dbus_message_iter_init(aRequest.GetMessage(), &iter), error = OT_ERROR_FAILED);
    VerifyOrExit(DBusMessageExtract(&iter, propertyNames) == OTBR_ERROR_NONE, error = OT_ERROR_PARSE);

    dbus_message_iter_init_append(reply.get(), &replyIter);
    VerifyOrExit(
        dbus_message_iter_open_container(&replyIter, DBUS_TYPE_ARRAY, DBUS_TYPE_VARIANT_AS_STRING, &replySubIter),
        error = OT_ERROR_NO_BUFS);

    for (const std::string &propertyName : propertyNames)
    {
        auto handlerIter = mGetPropertyHandlers.find(propertyName);

        otbrLogInfo("GetPropertiesHandler getting property: %s", propertyName.c_str());
        VerifyOrExit(handlerIter != mGetPropertyHandlers.end(), error = OT_ERROR_NOT_FOUND);

        SuccessOrExit(error = handlerIter->second(replySubIter));
    }

    VerifyOrExit(dbus_message_iter_close_container(&replyIter, &replySubIter), error = OT_ERROR_NO_BUFS);

exit:
    if (error == OT_ERROR_NONE)
    {
        dbus_connection_send(aRequest.GetConnection(), reply.get(), nullptr);
    }
    else
    {
        aRequest.ReplyOtResult(error);
    }
}

void DBusThreadObjectRcp::RegisterGetPropertyHandler(const std::string         &aInterfaceName,
                                                     const std::string         &aPropertyName,
                                                     const PropertyHandlerType &aHandler)
{
    DBusObject::RegisterGetPropertyHandler(aInterfaceName, aPropertyName, aHandler);
    mGetPropertyHandlers[aPropertyName] = aHandler;
}

otError DBusThreadObjectRcp::GetOtbrVersionHandler(DBusMessageIter &aIter)
{
    otError     error   = OT_ERROR_NONE;
    std::string version = OTBR_PACKAGE_VERSION;

    SuccessOrExit(DBusMessageEncodeToVariant(&aIter, version), error = OT_ERROR_FAILED);

exit:
    return error;
}

otError DBusThreadObjectRcp::GetOtHostVersionHandler(DBusMessageIter &aIter)
{
    otError     error   = OT_ERROR_NONE;
    std::string version = otGetVersionString();

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, version) == OTBR_ERROR_NONE, error = OT_ERROR_FAILED);

exit:
    return error;
}

otError DBusThreadObjectRcp::GetEui64Handler(DBusMessageIter &aIter)
{
    auto         threadHelper = mHost.GetThreadHelper();
    otError      error        = OT_ERROR_NONE;
    otExtAddress extAddr;
    uint64_t     eui64;

    otLinkGetFactoryAssignedIeeeEui64(threadHelper->GetInstance(), &extAddr);

    eui64 = ConvertOpenThreadUint64(extAddr.m8);

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, eui64) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

exit:
    return error;
}

otError DBusThreadObjectRcp::GetOtRcpVersionHandler(DBusMessageIter &aIter)
{
    auto        threadHelper = mHost.GetThreadHelper();
    otError     error        = OT_ERROR_NONE;
    std::string version      = otGetRadioVersionString(threadHelper->GetInstance());

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, version) == OTBR_ERROR_NONE, error = OT_ERROR_FAILED);

exit:
    return error;
}

otError DBusThreadObjectRcp::GetThreadVersionHandler(DBusMessageIter &aIter)
{
    otError error = OT_ERROR_NONE;

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, otThreadGetVersion()) == OTBR_ERROR_NONE, error = OT_ERROR_FAILED);

exit:
    return error;
}

otError DBusThreadObjectRcp::GetRadioSpinelMetricsHandler(DBusMessageIter &aIter)
{
    otError              error = OT_ERROR_NONE;
    RadioSpinelMetrics   radioSpinelMetrics;
    otRadioSpinelMetrics otRadioSpinelMetrics = *otSysGetRadioSpinelMetrics();

    radioSpinelMetrics.mRcpTimeoutCount         = otRadioSpinelMetrics.mRcpTimeoutCount;
    radioSpinelMetrics.mRcpUnexpectedResetCount = otRadioSpinelMetrics.mRcpUnexpectedResetCount;
    radioSpinelMetrics.mRcpRestorationCount     = otRadioSpinelMetrics.mRcpRestorationCount;
    radioSpinelMetrics.mSpinelParseErrorCount   = otRadioSpinelMetrics.mSpinelParseErrorCount;

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, radioSpinelMetrics) == OTBR_ERROR_NONE,
                 error = OT_ERROR_INVALID_ARGS);

exit:
    return error;
}

otError DBusThreadObjectRcp::GetRcpInterfaceMetricsHandler(DBusMessageIter &aIter)
{
    otError               error = OT_ERROR_NONE;
    RcpInterfaceMetrics   rcpInterfaceMetrics;
    otRcpInterfaceMetrics otRcpInterfaceMetrics = *otSysGetRcpInterfaceMetrics();

    rcpInterfaceMetrics.mRcpInterfaceType             = otRcpInterfaceMetrics.mRcpInterfaceType;
    rcpInterfaceMetrics.mTransferredFrameCount        = otRcpInterfaceMetrics.mTransferredFrameCount;
    rcpInterfaceMetrics.mTransferredValidFrameCount   = otRcpInterfaceMetrics.mTransferredValidFrameCount;
    rcpInterfaceMetrics.mTransferredGarbageFrameCount = otRcpInterfaceMetrics.mTransferredGarbageFrameCount;
    rcpInterfaceMetrics.mRxFrameCount                 = otRcpInterfaceMetrics.mRxFrameCount;
    rcpInterfaceMetrics.mRxFrameByteCount             = otRcpInterfaceMetrics.mRxFrameByteCount;
    rcpInterfaceMetrics.mTxFrameCount                 = otRcpInterfaceMetrics.mTxFrameCount;
    rcpInterfaceMetrics.mTxFrameByteCount             = otRcpInterfaceMetrics.mTxFrameByteCount;

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, rcpInterfaceMetrics) == OTBR_ERROR_NONE,
                 error = OT_ERROR_INVALID_ARGS);

exit:
    return error;
}

otError DBusThreadObjectRcp::GetUptimeHandler(DBusMessageIter &aIter)
{
    otError error = OT_ERROR_NONE;

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, otInstanceGetUptime(mHost.GetThreadHelper()->GetInstance())) ==
                     OTBR_ERROR_NONE,
                 error = OT_ERROR_INVALID_ARGS);

exit:
    return error;
}

otError DBusThreadObjectRcp::GetRadioCoexMetrics(DBusMessageIter &aIter)
{
    otError            error = OT_ERROR_NONE;
    otRadioCoexMetrics otRadioCoexMetrics;
    RadioCoexMetrics   radioCoexMetrics;

    SuccessOrExit(error = otPlatRadioGetCoexMetrics(mHost.GetInstance(), &otRadioCoexMetrics));

    radioCoexMetrics.mNumGrantGlitch                     = otRadioCoexMetrics.mNumGrantGlitch;
    radioCoexMetrics.mNumTxRequest                       = otRadioCoexMetrics.mNumTxRequest;
    radioCoexMetrics.mNumTxGrantImmediate                = otRadioCoexMetrics.mNumTxGrantImmediate;
    radioCoexMetrics.mNumTxGrantWait                     = otRadioCoexMetrics.mNumTxGrantWait;
    radioCoexMetrics.mNumTxGrantWaitActivated            = otRadioCoexMetrics.mNumTxGrantWaitActivated;
    radioCoexMetrics.mNumTxGrantWaitTimeout              = otRadioCoexMetrics.mNumTxGrantWaitTimeout;
    radioCoexMetrics.mNumTxGrantDeactivatedDuringRequest = otRadioCoexMetrics.mNumTxGrantDeactivatedDuringRequest;
    radioCoexMetrics.mNumTxDelayedGrant                  = otRadioCoexMetrics.mNumTxDelayedGrant;
    radioCoexMetrics.mAvgTxRequestToGrantTime            = otRadioCoexMetrics.mAvgTxRequestToGrantTime;
    radioCoexMetrics.mNumRxRequest                       = otRadioCoexMetrics.mNumRxRequest;
    radioCoexMetrics.mNumRxGrantImmediate                = otRadioCoexMetrics.mNumRxGrantImmediate;
    radioCoexMetrics.mNumRxGrantWait                     = otRadioCoexMetrics.mNumRxGrantWait;
    radioCoexMetrics.mNumRxGrantWaitActivated            = otRadioCoexMetrics.mNumRxGrantWaitActivated;
    radioCoexMetrics.mNumRxGrantWaitTimeout              = otRadioCoexMetrics.mNumRxGrantWaitTimeout;
    radioCoexMetrics.mNumRxGrantDeactivatedDuringRequest = otRadioCoexMetrics.mNumRxGrantDeactivatedDuringRequest;
    radioCoexMetrics.mNumRxDelayedGrant                  = otRadioCoexMetrics.mNumRxDelayedGrant;
    radioCoexMetrics.mAvgRxRequestToGrantTime            = otRadioCoexMetrics.mAvgRxRequestToGrantTime;
    radioCoexMetrics.mNumRxGrantNone                     = otRadioCoexMetrics.mNumRxGrantNone;
    radioCoexMetrics.mStopped                            = otRadioCoexMetrics.mStopped;

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, radioCoexMetrics) == OTBR_ERROR_NONE,
                 error = OT_ERROR_INVALID_ARGS);

exit:
    return error;
}

otError DBusThreadObjectRcp::GetBorderRoutingCountersHandler(DBusMessageIter &aIter)
{
#if OTBR_ENABLE_BORDER_ROUTING_COUNTERS
    auto                           threadHelper = mHost.GetThreadHelper();
    auto                           instance     = threadHelper->GetInstance();
    otError                        error        = OT_ERROR_NONE;
    BorderRoutingCounters          borderRoutingCounters;
    const otBorderRoutingCounters *otBorderRoutingCounters = otIp6GetBorderRoutingCounters(instance);

    borderRoutingCounters.mInboundUnicast.mPackets    = otBorderRoutingCounters->mInboundUnicast.mPackets;
    borderRoutingCounters.mInboundUnicast.mBytes      = otBorderRoutingCounters->mInboundUnicast.mBytes;
    borderRoutingCounters.mInboundMulticast.mPackets  = otBorderRoutingCounters->mInboundMulticast.mPackets;
    borderRoutingCounters.mInboundMulticast.mBytes    = otBorderRoutingCounters->mInboundMulticast.mBytes;
    borderRoutingCounters.mOutboundUnicast.mPackets   = otBorderRoutingCounters->mOutboundUnicast.mPackets;
    borderRoutingCounters.mOutboundUnicast.mBytes     = otBorderRoutingCounters->mOutboundUnicast.mBytes;
    borderRoutingCounters.mOutboundMulticast.mPackets = otBorderRoutingCounters->mOutboundMulticast.mPackets;
    borderRoutingCounters.mOutboundMulticast.mBytes   = otBorderRoutingCounters->mOutboundMulticast.mBytes;
    borderRoutingCounters.mRaRx                       = otBorderRoutingCounters->mRaRx;
    borderRoutingCounters.mRaTxSuccess                = otBorderRoutingCounters->mRaTxSuccess;
    borderRoutingCounters.mRaTxFailure                = otBorderRoutingCounters->mRaTxFailure;
    borderRoutingCounters.mRsRx                       = otBorderRoutingCounters->mRsRx;
    borderRoutingCounters.mRsTxSuccess                = otBorderRoutingCounters->mRsTxSuccess;
    borderRoutingCounters.mRsTxFailure                = otBorderRoutingCounters->mRsTxFailure;

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, borderRoutingCounters) == OTBR_ERROR_NONE,
                 error = OT_ERROR_INVALID_ARGS);

exit:
    return error;
#else
    OTBR_UNUSED_VARIABLE(aIter);

    return OT_ERROR_NOT_IMPLEMENTED;
#endif
}

void DBusThreadObjectRcp::ActiveDatasetChangeHandler(const otOperationalDatasetTlvs &aDatasetTlvs)
{
    std::vector<uint8_t> value(aDatasetTlvs.mLength);
    std::copy(aDatasetTlvs.mTlvs, aDatasetTlvs.mTlvs + aDatasetTlvs.mLength, value.begin());
    SignalPropertyChanged(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_ACTIVE_DATASET_TLVS, value);
}

void DBusThreadObjectRcp::LeaveNetworkHandler(DBusRequest &aRequest)
{
    constexpr int kExitCodeShouldRestart = 7;

    mHost.GetThreadHelper()->DetachGracefully([aRequest, this](otError error) mutable {
        SuccessOrExit(error);
        mPublisher->Stop();
        SuccessOrExit(error = otInstanceErasePersistentInfo(mHost.GetThreadHelper()->GetInstance()));

    exit:
        aRequest.ReplyOtResult(error);
        if (error == OT_ERROR_NONE)
        {
            Flush();
            exit(kExitCodeShouldRestart);
        }
    });
}

#if OTBR_ENABLE_NAT64
void DBusThreadObjectRcp::SetNat64Enabled(DBusRequest &aRequest)
{
    otError error = OT_ERROR_NONE;
    bool    enable;
    auto    args = std::tie(enable);

    VerifyOrExit(DBusMessageToTuple(*aRequest.GetMessage(), args) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);
    otNat64SetEnabled(mHost.GetThreadHelper()->GetInstance(), enable);

exit:
    aRequest.ReplyOtResult(error);
}

otError DBusThreadObjectRcp::GetNat64State(DBusMessageIter &aIter)
{
    otError error = OT_ERROR_NONE;

    Nat64ComponentState state;

    state.mPrefixManagerState = GetNat64StateName(otNat64GetPrefixManagerState(mHost.GetThreadHelper()->GetInstance()));
    state.mTranslatorState    = GetNat64StateName(otNat64GetTranslatorState(mHost.GetThreadHelper()->GetInstance()));

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, state) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

exit:
    return error;
}

otError DBusThreadObjectRcp::GetNat64Mappings(DBusMessageIter &aIter)
{
    otError error = OT_ERROR_NONE;

    std::vector<Nat64AddressMapping> mappings;
    otNat64AddressMappingIterator    iterator;
    otNat64AddressMapping            otMapping;
    Nat64AddressMapping              mapping;

    otNat64InitAddressMappingIterator(mHost.GetThreadHelper()->GetInstance(), &iterator);
    while (otNat64GetNextAddressMapping(mHost.GetThreadHelper()->GetInstance(), &iterator, &otMapping) == OT_ERROR_NONE)
    {
        mapping.mId = otMapping.mId;
        std::copy(std::begin(otMapping.mIp4.mFields.m8), std::end(otMapping.mIp4.mFields.m8), mapping.mIp4.data());
        std::copy(std::begin(otMapping.mIp6.mFields.m8), std::end(otMapping.mIp6.mFields.m8), mapping.mIp6.data());
        mapping.mRemainingTimeMs = otMapping.mRemainingTimeMs;

        mapping.mCounters.mTotal.m4To6Packets = otMapping.mCounters.mTotal.m4To6Packets;
        mapping.mCounters.mTotal.m4To6Bytes   = otMapping.mCounters.mTotal.m4To6Bytes;
        mapping.mCounters.mTotal.m6To4Packets = otMapping.mCounters.mTotal.m6To4Packets;
        mapping.mCounters.mTotal.m6To4Bytes   = otMapping.mCounters.mTotal.m6To4Bytes;

        mapping.mCounters.mIcmp.m4To6Packets = otMapping.mCounters.mIcmp.m4To6Packets;
        mapping.mCounters.mIcmp.m4To6Bytes   = otMapping.mCounters.mIcmp.m4To6Bytes;
        mapping.mCounters.mIcmp.m6To4Packets = otMapping.mCounters.mIcmp.m6To4Packets;
        mapping.mCounters.mIcmp.m6To4Bytes   = otMapping.mCounters.mIcmp.m6To4Bytes;

        mapping.mCounters.mUdp.m4To6Packets = otMapping.mCounters.mUdp.m4To6Packets;
        mapping.mCounters.mUdp.m4To6Bytes   = otMapping.mCounters.mUdp.m4To6Bytes;
        mapping.mCounters.mUdp.m6To4Packets = otMapping.mCounters.mUdp.m6To4Packets;
        mapping.mCounters.mUdp.m6To4Bytes   = otMapping.mCounters.mUdp.m6To4Bytes;

        mapping.mCounters.mTcp.m4To6Packets = otMapping.mCounters.mTcp.m4To6Packets;
        mapping.mCounters.mTcp.m4To6Bytes   = otMapping.mCounters.mTcp.m4To6Bytes;
        mapping.mCounters.mTcp.m6To4Packets = otMapping.mCounters.mTcp.m6To4Packets;
        mapping.mCounters.mTcp.m6To4Bytes   = otMapping.mCounters.mTcp.m6To4Bytes;

        mappings.push_back(mapping);
    }

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, mappings) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

exit:
    return error;
}

otError DBusThreadObjectRcp::GetNat64ProtocolCounters(DBusMessageIter &aIter)
{
    otError error = OT_ERROR_NONE;

    otNat64ProtocolCounters otCounters;
    Nat64ProtocolCounters   counters;
    otNat64GetCounters(mHost.GetThreadHelper()->GetInstance(), &otCounters);

    counters.mTotal.m4To6Packets = otCounters.mTotal.m4To6Packets;
    counters.mTotal.m4To6Bytes   = otCounters.mTotal.m4To6Bytes;
    counters.mTotal.m6To4Packets = otCounters.mTotal.m6To4Packets;
    counters.mTotal.m6To4Bytes   = otCounters.mTotal.m6To4Bytes;
    counters.mIcmp.m4To6Packets  = otCounters.mIcmp.m4To6Packets;
    counters.mIcmp.m4To6Bytes    = otCounters.mIcmp.m4To6Bytes;
    counters.mIcmp.m6To4Packets  = otCounters.mIcmp.m6To4Packets;
    counters.mIcmp.m6To4Bytes    = otCounters.mIcmp.m6To4Bytes;
    counters.mUdp.m4To6Packets   = otCounters.mUdp.m4To6Packets;
    counters.mUdp.m4To6Bytes     = otCounters.mUdp.m4To6Bytes;
    counters.mUdp.m6To4Packets   = otCounters.mUdp.m6To4Packets;
    counters.mUdp.m6To4Bytes     = otCounters.mUdp.m6To4Bytes;
    counters.mTcp.m4To6Packets   = otCounters.mTcp.m4To6Packets;
    counters.mTcp.m4To6Bytes     = otCounters.mTcp.m4To6Bytes;
    counters.mTcp.m6To4Packets   = otCounters.mTcp.m6To4Packets;
    counters.mTcp.m6To4Bytes     = otCounters.mTcp.m6To4Bytes;

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, counters) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

exit:
    return error;
}

otError DBusThreadObjectRcp::GetNat64ErrorCounters(DBusMessageIter &aIter)
{
    otError error = OT_ERROR_NONE;

    otNat64ErrorCounters otCounters;
    Nat64ErrorCounters   counters;
    otNat64GetErrorCounters(mHost.GetThreadHelper()->GetInstance(), &otCounters);

    counters.mUnknown.m4To6Packets          = otCounters.mCount4To6[OT_NAT64_DROP_REASON_UNKNOWN];
    counters.mUnknown.m6To4Packets          = otCounters.mCount6To4[OT_NAT64_DROP_REASON_UNKNOWN];
    counters.mIllegalPacket.m4To6Packets    = otCounters.mCount4To6[OT_NAT64_DROP_REASON_ILLEGAL_PACKET];
    counters.mIllegalPacket.m6To4Packets    = otCounters.mCount6To4[OT_NAT64_DROP_REASON_ILLEGAL_PACKET];
    counters.mUnsupportedProto.m4To6Packets = otCounters.mCount4To6[OT_NAT64_DROP_REASON_UNSUPPORTED_PROTO];
    counters.mUnsupportedProto.m6To4Packets = otCounters.mCount6To4[OT_NAT64_DROP_REASON_UNSUPPORTED_PROTO];
    counters.mNoMapping.m4To6Packets        = otCounters.mCount4To6[OT_NAT64_DROP_REASON_NO_MAPPING];
    counters.mNoMapping.m6To4Packets        = otCounters.mCount6To4[OT_NAT64_DROP_REASON_NO_MAPPING];

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, counters) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

exit:
    return error;
}

otError DBusThreadObjectRcp::GetNat64Cidr(DBusMessageIter &aIter)
{
    otError error = OT_ERROR_NONE;

    otIp4Cidr cidr;
    char      cidrString[OT_IP4_CIDR_STRING_SIZE];

    SuccessOrExit(error = otNat64GetCidr(mHost.GetThreadHelper()->GetInstance(), &cidr));
    otIp4CidrToString(&cidr, cidrString, sizeof(cidrString));

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, std::string(cidrString)) == OTBR_ERROR_NONE,
                 error = OT_ERROR_INVALID_ARGS);

exit:
    return error;
}

otError DBusThreadObjectRcp::SetNat64Cidr(DBusMessageIter &aIter)
{
    otError     error = OT_ERROR_NONE;
    std::string cidrString;
    otIp4Cidr   cidr;

    VerifyOrExit(DBusMessageExtractFromVariant(&aIter, cidrString) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);
    SuccessOrExit(error = otIp4CidrFromString(cidrString.c_str(), &cidr));
    SuccessOrExit(error = otNat64SetIp4Cidr(mHost.GetThreadHelper()->GetInstance(), &cidr));

exit:
    return error;
}
#else  // OTBR_ENABLE_NAT64
void DBusThreadObjectRcp::SetNat64Enabled(DBusRequest &aRequest)
{
    OTBR_UNUSED_VARIABLE(aRequest);
    aRequest.ReplyOtResult(OT_ERROR_NOT_IMPLEMENTED);
}

otError DBusThreadObjectRcp::GetNat64State(DBusMessageIter &aIter)
{
    OTBR_UNUSED_VARIABLE(aIter);
    return OT_ERROR_NOT_IMPLEMENTED;
}

otError DBusThreadObjectRcp::GetNat64Mappings(DBusMessageIter &aIter)
{
    OTBR_UNUSED_VARIABLE(aIter);
    return OT_ERROR_NOT_IMPLEMENTED;
}

otError DBusThreadObjectRcp::GetNat64ProtocolCounters(DBusMessageIter &aIter)
{
    OTBR_UNUSED_VARIABLE(aIter);
    return OT_ERROR_NOT_IMPLEMENTED;
}

otError DBusThreadObjectRcp::GetNat64ErrorCounters(DBusMessageIter &aIter)
{
    OTBR_UNUSED_VARIABLE(aIter);
    return OT_ERROR_NOT_IMPLEMENTED;
}

otError DBusThreadObjectRcp::GetNat64Cidr(DBusMessageIter &aIter)
{
    OTBR_UNUSED_VARIABLE(aIter);
    return OT_ERROR_NOT_IMPLEMENTED;
}

otError DBusThreadObjectRcp::SetNat64Cidr(DBusMessageIter &aIter)
{
    OTBR_UNUSED_VARIABLE(aIter);
    return OT_ERROR_NOT_IMPLEMENTED;
}
#endif // OTBR_ENABLE_NAT64

otError DBusThreadObjectRcp::GetEphemeralKeyEnabled(DBusMessageIter &aIter)
{
    otError error = OT_ERROR_NONE;

    SuccessOrExit(DBusMessageEncodeToVariant(&aIter, mBorderAgent.GetEphemeralKeyEnabled()),
                  error = OT_ERROR_INVALID_ARGS);

exit:
    return error;
}

otError DBusThreadObjectRcp::SetEphemeralKeyEnabled(DBusMessageIter &aIter)
{
    otError error = OT_ERROR_NONE;
    bool    enable;

    SuccessOrExit(DBusMessageExtractFromVariant(&aIter, enable), error = OT_ERROR_INVALID_ARGS);
    mBorderAgent.SetEphemeralKeyEnabled(enable);

exit:
    return error;
}

void DBusThreadObjectRcp::DeactivateEphemeralKeyModeHandler(DBusRequest &aRequest)
{
    otError error        = OT_ERROR_NONE;
    auto    threadHelper = mHost.GetThreadHelper();
    bool    retain_active_session;
    auto    args = std::tie(retain_active_session);

    VerifyOrExit(mBorderAgent.GetEphemeralKeyEnabled(), error = OT_ERROR_NOT_CAPABLE);

    SuccessOrExit(DBusMessageToTuple(*aRequest.GetMessage(), args), error = OT_ERROR_INVALID_ARGS);
    if (!retain_active_session)
    {
        otBorderAgentDisconnect(threadHelper->GetInstance());
    }
    otBorderAgentClearEphemeralKey(threadHelper->GetInstance());

exit:
    aRequest.ReplyOtResult(error);
}

void DBusThreadObjectRcp::ActivateEphemeralKeyModeHandler(DBusRequest &aRequest)
{
    otError     error        = OT_ERROR_NONE;
    auto        threadHelper = mHost.GetThreadHelper();
    uint32_t    lifetime     = 0;
    auto        args         = std::tie(lifetime);
    std::string ePskc;

    VerifyOrExit(mBorderAgent.GetEphemeralKeyEnabled(), error = OT_ERROR_NOT_CAPABLE);

    SuccessOrExit(DBusMessageToTuple(*aRequest.GetMessage(), args), error = OT_ERROR_INVALID_ARGS);
    VerifyOrExit(lifetime <= OT_BORDER_AGENT_MAX_EPHEMERAL_KEY_TIMEOUT, error = OT_ERROR_INVALID_ARGS);

    SuccessOrExit(mBorderAgent.CreateEphemeralKey(ePskc), error = OT_ERROR_INVALID_ARGS);
    otbrLogInfo("Created Ephemeral Key: %s", ePskc.c_str());

    SuccessOrExit(error = otBorderAgentSetEphemeralKey(threadHelper->GetInstance(), ePskc.c_str(), lifetime,
                                                       OTBR_CONFIG_BORDER_AGENT_MESHCOP_E_UDP_PORT));

exit:
    if (error == OT_ERROR_NONE)
    {
        aRequest.Reply(std::tie(ePskc));
    }
    else
    {
        aRequest.ReplyOtResult(error);
    }
}

otError DBusThreadObjectRcp::GetInfraLinkInfo(DBusMessageIter &aIter)
{
#if OTBR_ENABLE_BORDER_ROUTING
    otError                        error = OT_ERROR_NONE;
    otSysInfraNetIfAddressCounters addressCounters;
    uint32_t                       ifrFlags;
    InfraLinkInfo                  infraLinkInfo;

    ifrFlags = otSysGetInfraNetifFlags();
    otSysCountInfraNetifAddresses(&addressCounters);

    infraLinkInfo.mName                      = otSysGetInfraNetifName();
    infraLinkInfo.mIsUp                      = (ifrFlags & IFF_UP) != 0;
    infraLinkInfo.mIsRunning                 = (ifrFlags & IFF_RUNNING) != 0;
    infraLinkInfo.mIsMulticast               = (ifrFlags & IFF_MULTICAST) != 0;
    infraLinkInfo.mLinkLocalAddressCount     = addressCounters.mLinkLocalAddresses;
    infraLinkInfo.mUniqueLocalAddressCount   = addressCounters.mUniqueLocalAddresses;
    infraLinkInfo.mGlobalUnicastAddressCount = addressCounters.mGlobalUnicastAddresses;

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, infraLinkInfo) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

exit:
    return error;
#else
    OTBR_UNUSED_VARIABLE(aIter);

    return OT_ERROR_NOT_IMPLEMENTED;
#endif
}

otError DBusThreadObjectRcp::SetDnsUpstreamQueryState(DBusMessageIter &aIter)
{
#if OTBR_ENABLE_DNS_UPSTREAM_QUERY
    otError error = OT_ERROR_NONE;
    bool    enable;

    VerifyOrExit(DBusMessageExtractFromVariant(&aIter, enable) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);
    otDnssdUpstreamQuerySetEnabled(mHost.GetThreadHelper()->GetInstance(), enable);

exit:
    return error;
#else
    OTBR_UNUSED_VARIABLE(aIter);

    return OT_ERROR_NOT_IMPLEMENTED;
#endif
}

otError DBusThreadObjectRcp::GetDnsUpstreamQueryState(DBusMessageIter &aIter)
{
#if OTBR_ENABLE_DNS_UPSTREAM_QUERY
    otError error = OT_ERROR_NONE;

    VerifyOrExit(DBusMessageEncodeToVariant(
                     &aIter, otDnssdUpstreamQueryIsEnabled(mHost.GetThreadHelper()->GetInstance())) == OTBR_ERROR_NONE,
                 error = OT_ERROR_INVALID_ARGS);

exit:
    return error;
#else
    OTBR_UNUSED_VARIABLE(aIter);

    return OT_ERROR_NOT_IMPLEMENTED;
#endif
}

static_assert(OTBR_SRP_SERVER_STATE_DISABLED == static_cast<uint8_t>(OT_SRP_SERVER_STATE_DISABLED),
              "OTBR_SRP_SERVER_STATE_DISABLED value is incorrect");
static_assert(OTBR_SRP_SERVER_STATE_RUNNING == static_cast<uint8_t>(OT_SRP_SERVER_STATE_RUNNING),
              "OTBR_SRP_SERVER_STATE_RUNNING value is incorrect");
static_assert(OTBR_SRP_SERVER_STATE_STOPPED == static_cast<uint8_t>(OT_SRP_SERVER_STATE_STOPPED),
              "OTBR_SRP_SERVER_STATE_STOPPED value is incorrect");

static_assert(OTBR_SRP_SERVER_ADDRESS_MODE_UNICAST == static_cast<uint8_t>(OT_SRP_SERVER_ADDRESS_MODE_UNICAST),
              "OTBR_SRP_SERVER_ADDRESS_MODE_UNICAST value is incorrect");
static_assert(OTBR_SRP_SERVER_ADDRESS_MODE_ANYCAST == static_cast<uint8_t>(OT_SRP_SERVER_ADDRESS_MODE_ANYCAST),
              "OTBR_SRP_SERVER_ADDRESS_MODE_ANYCAST value is incorrect");

} // namespace DBus
} // namespace otbr
