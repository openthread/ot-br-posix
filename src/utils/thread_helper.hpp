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
 *   This file includes definitions for Thread helper.
 */

#ifndef OTBR_THREAD_HELPER_HPP_
#define OTBR_THREAD_HELPER_HPP_

#include "openthread-br/config.h"

#include <chrono>
#include <functional>
#include <map>
#include <random>
#include <string>
#include <vector>

#include <openthread/border_routing.h>
#include <openthread/instance.h>
#include <openthread/ip6.h>
#include <openthread/jam_detection.h>
#include <openthread/joiner.h>
#include <openthread/netdata.h>
#include <openthread/thread.h>
#include "mdns/mdns.hpp"
#if OTBR_ENABLE_TELEMETRY_DATA_API
#include "proto/thread_telemetry.pb.h"
#endif

namespace otbr {
namespace Ncp {
class RcpHost;
}
} // namespace otbr

namespace otbr {
namespace agent {

/**
 * This class implements Thread helper.
 */
class ThreadHelper
{
public:
    using DeviceRoleHandler       = std::function<void(otDeviceRole)>;
    using ScanHandler             = std::function<void(otError, const std::vector<otActiveScanResult> &)>;
    using EnergyScanHandler       = std::function<void(otError, const std::vector<otEnergyScanResult> &)>;
    using ResultHandler           = std::function<void(otError)>;
    using AttachHandler           = std::function<void(otError, int64_t)>;
    using UpdateMeshCopTxtHandler = std::function<void(std::map<std::string, std::vector<uint8_t>>)>;
    using DatasetChangeHandler    = std::function<void(const otOperationalDatasetTlvs &)>;
#if OTBR_ENABLE_DHCP6_PD
    using Dhcp6PdStateCallback = std::function<void(otBorderRoutingDhcp6PdState)>;
#endif

    /**
     * The constructor of a Thread helper.
     *
     * @param[in] aInstance  The Thread instance.
     * @param[in] aHost      The Thread controller.
     */
    ThreadHelper(otInstance *aInstance, otbr::Ncp::RcpHost *aHost);

    /**
     * This method adds a callback for device role change.
     *
     * @param[in] aHandler  The device role handler.
     */
    void AddDeviceRoleHandler(DeviceRoleHandler aHandler);

#if OTBR_ENABLE_DHCP6_PD
    /**
     * This method adds a callback for DHCPv6 PD state change.
     *
     * @param[in] aCallback  The DHCPv6 PD state change callback.
     */
    void SetDhcp6PdStateCallback(Dhcp6PdStateCallback aCallback);
#endif

    /**
     * This method adds a callback for active dataset change.
     *
     * @param[in]  aHandler   The active dataset change handler.
     */
    void AddActiveDatasetChangeHandler(DatasetChangeHandler aHandler);

    /**
     * This method permits unsecure join on port.
     *
     * @param[in] aPort     The port number.
     * @param[in] aSeconds  The timeout to close the port, 0 for never close.
     *
     * @returns The error value of underlying OpenThread api calls.
     */
    otError PermitUnsecureJoin(uint16_t aPort, uint32_t aSeconds);

    /**
     * This method performs a Thread network scan.
     *
     * @param[in] aHandler  The scan result handler.
     */
    void Scan(ScanHandler aHandler);

    /**
     * This method performs an IEEE 802.15.4 Energy Scan.
     *
     * @param[in] aScanDuration  The duration for the scan, in milliseconds.
     * @param[in] aHandler       The scan result handler.
     */
    void EnergyScan(uint32_t aScanDuration, EnergyScanHandler aHandler);

    /**
     * This method attaches the device to the Thread network.
     *
     * @note The joiner start and the attach proccesses are exclusive
     *
     * @param[in] aNetworkName  The network name.
     * @param[in] aPanId        The pan id, UINT16_MAX for random.
     * @param[in] aExtPanId     The extended pan id, UINT64_MAX for random.
     * @param[in] aNetworkKey   The network key, empty for random.
     * @param[in] aPSKc         The pre-shared commissioner key, empty for random.
     * @param[in] aChannelMask  A bitmask for valid channels, will random select one.
     * @param[in] aHandler      The attach result handler.
     */
    void Attach(const std::string          &aNetworkName,
                uint16_t                    aPanId,
                uint64_t                    aExtPanId,
                const std::vector<uint8_t> &aNetworkKey,
                const std::vector<uint8_t> &aPSKc,
                uint32_t                    aChannelMask,
                AttachHandler               aHandler);

    /**
     * This method detaches the device from the Thread network.
     *
     * @returns The error value of underlying OpenThread API calls.
     */
    otError Detach(void);

    /**
     * This method attaches the device to the Thread network.
     *
     * @note The joiner start and the attach proccesses are exclusive, and the
     *       network parameter will be set through the active dataset.
     *
     * @param[in] aHandler  The attach result handler.
     */
    void Attach(AttachHandler aHandler);

    /**
     * This method makes all nodes in the current network attach to the network specified by the dataset TLVs.
     *
     * @param[in] aDatasetTlvs  The dataset TLVs.
     * @param[in] aHandler      The result handler.
     */
    void AttachAllNodesTo(const std::vector<uint8_t> &aDatasetTlvs, AttachHandler aHandler);

    /**
     * This method resets the OpenThread stack.
     *
     * @returns The error value of underlying OpenThread api calls.
     */
    otError Reset(void);

    /**
     * This method triggers a thread join process.
     *
     * @note The joiner start and the attach proccesses are exclusive
     *
     * @param[in] aPskd             The pre-shared key for device.
     * @param[in] aProvisioningUrl  The provision url.
     * @param[in] aVendorName       The vendor name.
     * @param[in] aVendorModel      The vendor model.
     * @param[in] aVendorSwVersion  The vendor software version.
     * @param[in] aVendorData       The vendor custom data.
     * @param[in] aHandler          The join result handler.
     */
    void JoinerStart(const std::string &aPskd,
                     const std::string &aProvisioningUrl,
                     const std::string &aVendorName,
                     const std::string &aVendorModel,
                     const std::string &aVendorSwVersion,
                     const std::string &aVendorData,
                     ResultHandler      aHandler);

    /**
     * This method tries to restore the network after reboot
     *
     * @returns The error value of underlying OpenThread api calls.
     */
    otError TryResumeNetwork(void);

    /**
     * This method returns the underlying OpenThread instance.
     *
     * @returns The underlying instance.
     */
    otInstance *GetInstance(void)
    {
        return mInstance;
    }

    /**
     * This method handles OpenThread state changed notification.
     *
     * @param[in] aFlags    A bit-field indicating specific state that has changed.  See `OT_CHANGED_*` definitions.
     */
    void StateChangedCallback(otChangedFlags aFlags);

#if OTBR_ENABLE_DBUS_SERVER
    /**
     * This method sets a callback for calls of UpdateVendorMeshCopTxtEntries D-Bus API.
     *
     * @param[in] aHandler  The handler on MeshCoP TXT changes.
     */
    void SetUpdateMeshCopTxtHandler(UpdateMeshCopTxtHandler aHandler)
    {
        mUpdateMeshCopTxtHandler = std::move(aHandler);
    }

    /**
     * This method handles MeshCoP TXT updates done by UpdateVendorMeshCopTxtEntries D-Bus API.
     *
     * @param[in] aUpdate  The key-value pairs to be updated in the TXT record.
     */
    void OnUpdateMeshCopTxt(std::map<std::string, std::vector<uint8_t>> aUpdate);
#endif

    void DetachGracefully(ResultHandler aHandler);

#if OTBR_ENABLE_TELEMETRY_DATA_API
    /**
     * This method populates the telemetry data with best effort. The best effort means, for a given
     * telemetry, if its retrieval has error, it is left unpopulated and the process continues to
     * retrieve the remaining telemetries instead of the immediately return. The error code
     * OT_ERRROR_FAILED will be returned if there is one or more error(s) happened in the process.
     *
     * @param[in] aPublisher     The Mdns::Publisher to provide MDNS telemetry if it is not `nullptr`.
     * @param[in] telemetryData  The telemetry data to be populated.
     *
     * @retval OTBR_ERROR_NONE  There is no error happened in the process.
     * @retval OT_ERRROR_FAILED There is one or more error(s) happened in the process.
     */
    otError RetrieveTelemetryData(Mdns::Publisher *aPublisher, threadnetwork::TelemetryData &telemetryData);
#endif // OTBR_ENABLE_TELEMETRY_DATA_API

    /**
     * This method logs OpenThread action result.
     *
     * @param[in] aAction  The action OpenThread performs.
     * @param[in] aError   The action result.
     */
    static void LogOpenThreadResult(const char *aAction, otError aError);

    /**
     * This method validates and updates a pending dataset do Thread network migration.
     *
     * This method validates that:
     * 1. the given dataset doesn't contain a meshcop Pending Timestamp TLV or a meshcop Delay Timer TLV.
     * 2. the given dataset has sufficient space to append a Pending Timestamp TLV and a Delay Timer TLV.
     *
     * If it's valid, the method will append a meshcop Pending Timestamp TLV with value being the current unix
     * timestamp and a meshcop Delay Timer TLV with value being @p aDelayMilli.
     *
     * @param[in/out] aDatasetTlvs  The dataset to validate and process in TLVs format.
     * @param[in]     aDelayMilli   The delay time for migration in milliseconds.
     *
     * @retval OT_ERROR_NONE          Dataset is valid to do Thread network migration.
     * @retval OT_ERROR_INVALID_ARGS  Dataset is invalid to do Thread network migration.
     */
    static otError ProcessDatasetForMigration(otOperationalDatasetTlvs &aDatasetTlvs, uint32_t aDelayMilli);

private:
    static void ActiveScanHandler(otActiveScanResult *aResult, void *aThreadHelper);
    void        ActiveScanHandler(otActiveScanResult *aResult);

    static void EnergyScanCallback(otEnergyScanResult *aResult, void *aThreadHelper);
    void        EnergyScanCallback(otEnergyScanResult *aResult);

    static void JoinerCallback(otError aError, void *aThreadHelper);
    void        JoinerCallback(otError aResult);

    static void MgmtSetResponseHandler(otError aResult, void *aContext);
    void        MgmtSetResponseHandler(otError aResult);

    static void DetachGracefullyCallback(void *aContext);
    void        DetachGracefullyCallback(void);

    void    RandomFill(void *aBuf, size_t size);
    uint8_t RandomChannelFromChannelMask(uint32_t aChannelMask);

    void ActiveDatasetChangedCallback(void);

#if OTBR_ENABLE_DHCP6_PD
    static void BorderRoutingDhcp6PdCallback(otBorderRoutingDhcp6PdState aState, void *aThreadHelper);
    void        BorderRoutingDhcp6PdCallback(otBorderRoutingDhcp6PdState aState);
#endif
#if OTBR_ENABLE_TELEMETRY_DATA_API
#if OTBR_ENABLE_BORDER_ROUTING
    void RetrieveInfraLinkInfo(threadnetwork::TelemetryData::InfraLinkInfo &aInfraLinkInfo);
    void RetrieveExternalRouteInfo(threadnetwork::TelemetryData::ExternalRoutes &aExternalRouteInfo);
#endif
#if OTBR_ENABLE_DHCP6_PD
    void RetrievePdInfo(threadnetwork::TelemetryData::WpanBorderRouter *aWpanBorderRouter);
    void RetrieveHashedPdPrefix(std::string *aHashedPdPrefix);
    void RetrievePdProcessedRaInfo(threadnetwork::TelemetryData::PdProcessedRaInfo *aPdProcessedRaInfo);
#endif
#if OTBR_ENABLE_BORDER_AGENT
    void RetrieveBorderAgentInfo(threadnetwork::TelemetryData::BorderAgentInfo *aBorderAgentInfo);
#endif
#endif // OTBR_ENABLE_TELEMETRY_DATA_API

    otInstance *mInstance;

    otbr::Ncp::RcpHost *mHost;

    ScanHandler                     mScanHandler;
    std::vector<otActiveScanResult> mScanResults;
    EnergyScanHandler               mEnergyScanHandler;
    std::vector<otEnergyScanResult> mEnergyScanResults;

    std::vector<DeviceRoleHandler>    mDeviceRoleHandlers;
    std::vector<DatasetChangeHandler> mActiveDatasetChangeHandlers;

    std::map<uint16_t, size_t> mUnsecurePortRefCounter;

    bool mWaitingMgmtSetResponse =
        false; // During waiting for mgmt set response, calls to AttachHandler by StateChangedCallback will be ignored
    int64_t       mAttachDelayMs = 0;
    AttachHandler mAttachHandler;
    ResultHandler mJoinerHandler;

    ResultHandler mDetachGracefullyHandler = nullptr;

    otOperationalDatasetTlvs mAttachPendingDatasetTlvs = {};

    std::random_device mRandomDevice;

#if OTBR_ENABLE_DHCP6_PD
    Dhcp6PdStateCallback mDhcp6PdCallback;
#endif

#if OTBR_ENABLE_DBUS_SERVER
    UpdateMeshCopTxtHandler mUpdateMeshCopTxtHandler;
#endif

#if OTBR_ENABLE_TELEMETRY_DATA_API && (OTBR_ENABLE_NAT64 || OTBR_ENABLE_DHCP6_PD)
    static constexpr uint8_t kNat64PdCommonHashSaltLength = 16;
    uint8_t                  mNat64PdCommonSalt[kNat64PdCommonHashSaltLength];
#endif
};

} // namespace agent
} // namespace otbr

#endif // OTBR_THREAD_HELPER_HPP_
