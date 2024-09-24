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

/**
 * @file
 * This file includes definitions for the d-bus object of OpenThread service.
 */

#ifndef OTBR_DBUS_THREAD_OBJECT_RCP_HPP_
#define OTBR_DBUS_THREAD_OBJECT_RCP_HPP_

#include "openthread-br/config.h"

#include <string>

#include <openthread/link.h>

#include "border_agent/border_agent.hpp"
#include "dbus/server/dbus_object.hpp"
#include "mdns/mdns.hpp"
#include "ncp/rcp_host.hpp"

namespace otbr {
namespace DBus {

/**
 * @addtogroup border-router-dbus-server
 *
 * @brief
 *   This module includes the <a href="dbus-api.html">dbus server api</a>.
 *
 * @{
 */

class DBusThreadObjectRcp : public DBusObject
{
public:
    /**
     * This constructor of dbus thread object.
     *
     * @param[in] aConnection     The dbus connection.
     * @param[in] aInterfaceName  The dbus interface name.
     * @param[in] aHost           The Thread controller
     * @param[in] aPublisher      The Mdns::Publisher
     * @param[in] aBorderAgent    The Border Agent
     */
    DBusThreadObjectRcp(DBusConnection     &aConnection,
                        const std::string  &aInterfaceName,
                        otbr::Ncp::RcpHost &aHost,
                        Mdns::Publisher    *aPublisher,
                        otbr::BorderAgent  &aBorderAgent);

    otbrError Init(void) override;

    void RegisterGetPropertyHandler(const std::string         &aInterfaceName,
                                    const std::string         &aPropertyName,
                                    const PropertyHandlerType &aHandler) override;

private:
    void DeviceRoleHandler(otDeviceRole aDeviceRole);
    void Dhcp6PdStateHandler(otBorderRoutingDhcp6PdState aDhcp6PdState);
    void ActiveDatasetChangeHandler(const otOperationalDatasetTlvs &aDatasetTlvs);
    void NcpResetHandler(void);

    void ScanHandler(DBusRequest &aRequest);
    void EnergyScanHandler(DBusRequest &aRequest);
    void AttachHandler(DBusRequest &aRequest);
    void AttachAllNodesToHandler(DBusRequest &aRequest);
    void DetachHandler(DBusRequest &aRequest);
    void LeaveHandler(DBusRequest &aRequest);
    void FactoryResetHandler(DBusRequest &aRequest);
    void ResetHandler(DBusRequest &aRequest);
    void JoinerStartHandler(DBusRequest &aRequest);
    void JoinerStopHandler(DBusRequest &aRequest);
    void PermitUnsecureJoinHandler(DBusRequest &aRequest);
    void AddOnMeshPrefixHandler(DBusRequest &aRequest);
    void RemoveOnMeshPrefixHandler(DBusRequest &aRequest);
    void AddExternalRouteHandler(DBusRequest &aRequest);
    void RemoveExternalRouteHandler(DBusRequest &aRequest);
    void UpdateMeshCopTxtHandler(DBusRequest &aRequest);
    void GetPropertiesHandler(DBusRequest &aRequest);
    void LeaveNetworkHandler(DBusRequest &aRequest);
    void SetNat64Enabled(DBusRequest &aRequest);
    void ActivateEphemeralKeyModeHandler(DBusRequest &aRequest);
    void DeactivateEphemeralKeyModeHandler(DBusRequest &aRequest);

    void IntrospectHandler(DBusRequest &aRequest);

    otError SetMeshLocalPrefixHandler(DBusMessageIter &aIter);
    otError SetLegacyUlaPrefixHandler(DBusMessageIter &aIter);
    otError SetLinkModeHandler(DBusMessageIter &aIter);
    otError SetActiveDatasetTlvsHandler(DBusMessageIter &aIter);
    otError SetFeatureFlagListDataHandler(DBusMessageIter &aIter);
    otError SetRadioRegionHandler(DBusMessageIter &aIter);
    otError SetDnsUpstreamQueryState(DBusMessageIter &aIter);
    otError SetNat64Cidr(DBusMessageIter &aIter);
    otError SetEphemeralKeyEnabled(DBusMessageIter &aIter);

    otError GetLinkModeHandler(DBusMessageIter &aIter);
    otError GetDeviceRoleHandler(DBusMessageIter &aIter);
    otError GetNetworkNameHandler(DBusMessageIter &aIter);
    otError GetPanIdHandler(DBusMessageIter &aIter);
    otError GetExtPanIdHandler(DBusMessageIter &aIter);
    otError GetEui64Handler(DBusMessageIter &aIter);
    otError GetChannelHandler(DBusMessageIter &aIter);
    otError GetNetworkKeyHandler(DBusMessageIter &aIter);
    otError GetCcaFailureRateHandler(DBusMessageIter &aIter);
    otError GetLinkCountersHandler(DBusMessageIter &aIter);
    otError GetIp6CountersHandler(DBusMessageIter &aIter);
    otError GetSupportedChannelMaskHandler(DBusMessageIter &aIter);
    otError GetPreferredChannelMaskHandler(DBusMessageIter &aIter);
    otError GetRloc16Handler(DBusMessageIter &aIter);
    otError GetExtendedAddressHandler(DBusMessageIter &aIter);
    otError GetRouterIdHandler(DBusMessageIter &aIter);
    otError GetLeaderDataHandler(DBusMessageIter &aIter);
    otError GetNetworkDataHandler(DBusMessageIter &aIter);
    otError GetStableNetworkDataHandler(DBusMessageIter &aIter);
    otError GetLocalLeaderWeightHandler(DBusMessageIter &aIter);
    otError GetChannelMonitorSampleCountHandler(DBusMessageIter &aIter);
    otError GetChannelMonitorAllChannelQualities(DBusMessageIter &aIter);
    otError GetChildTableHandler(DBusMessageIter &aIter);
    otError GetNeighborTableHandler(DBusMessageIter &aIter);
    otError GetPartitionIDHandler(DBusMessageIter &aIter);
    otError GetInstantRssiHandler(DBusMessageIter &aIter);
    otError GetRadioTxPowerHandler(DBusMessageIter &aIter);
    otError GetExternalRoutesHandler(DBusMessageIter &aIter);
    otError GetOnMeshPrefixesHandler(DBusMessageIter &aIter);
    otError GetActiveDatasetTlvsHandler(DBusMessageIter &aIter);
    otError GetPendingDatasetTlvsHandler(DBusMessageIter &aIter);
    otError GetFeatureFlagListDataHandler(DBusMessageIter &aIter);
    otError GetRadioRegionHandler(DBusMessageIter &aIter);
    otError GetSrpServerInfoHandler(DBusMessageIter &aIter);
    otError GetMdnsTelemetryInfoHandler(DBusMessageIter &aIter);
    otError GetDnssdCountersHandler(DBusMessageIter &aIter);
    otError GetOtbrVersionHandler(DBusMessageIter &aIter);
    otError GetOtHostVersionHandler(DBusMessageIter &aIter);
    otError GetOtRcpVersionHandler(DBusMessageIter &aIter);
    otError GetThreadVersionHandler(DBusMessageIter &aIter);
    otError GetRadioSpinelMetricsHandler(DBusMessageIter &aIter);
    otError GetRcpInterfaceMetricsHandler(DBusMessageIter &aIter);
    otError GetUptimeHandler(DBusMessageIter &aIter);
    otError GetTrelInfoHandler(DBusMessageIter &aIter);
    otError GetRadioCoexMetrics(DBusMessageIter &aIter);
    otError GetBorderRoutingCountersHandler(DBusMessageIter &aIter);
    otError GetNat64State(DBusMessageIter &aIter);
    otError GetNat64Cidr(DBusMessageIter &aIter);
    otError GetNat64Mappings(DBusMessageIter &aIter);
    otError GetNat64ProtocolCounters(DBusMessageIter &aIter);
    otError GetNat64ErrorCounters(DBusMessageIter &aIter);
    otError GetEphemeralKeyEnabled(DBusMessageIter &aIter);
    otError GetInfraLinkInfo(DBusMessageIter &aIter);
    otError GetDnsUpstreamQueryState(DBusMessageIter &aIter);
    otError GetTelemetryDataHandler(DBusMessageIter &aIter);
    otError GetCapabilitiesHandler(DBusMessageIter &aIter);

    void ReplyScanResult(DBusRequest &aRequest, otError aError, const std::vector<otActiveScanResult> &aResult);
    void ReplyEnergyScanResult(DBusRequest &aRequest, otError aError, const std::vector<otEnergyScanResult> &aResult);

    otbr::Ncp::RcpHost                                  &mHost;
    std::unordered_map<std::string, PropertyHandlerType> mGetPropertyHandlers;
    otbr::Mdns::Publisher                               *mPublisher;
    otbr::BorderAgent                                   &mBorderAgent;
};

/**
 * @}
 */

} // namespace DBus
} // namespace otbr

#endif // OTBR_DBUS_THREAD_OBJECT_RCP_HPP_
