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

#ifndef OTBR_THREAD_API_DBUS_HPP_
#define OTBR_THREAD_API_DBUS_HPP_

#include <functional>

#include <dbus/dbus.h>

#include "dbus/common/constants.hpp"
#include "dbus/common/dbus_message_helper.hpp"
#include "dbus/common/error.hpp"
#include "dbus/common/types.hpp"

namespace otbr {
namespace DBus {

bool IsThreadActive(otbrDeviceRole aRole);

class ThreadApiDBus
{
public:
    using DeviceRoleHandler = std::function<void(otbrDeviceRole)>;
    using ScanHandler       = std::function<void(const std::vector<otbrActiveScanResult> &)>;
    using OtResultHandler   = std::function<void(OtbrClientError)>;

    /**
     * The constructor of a d-bus object.
     *
     * Will use the default interfacename
     *
     * @param[in]   aConnection     The dbus connection.
     *
     */
    ThreadApiDBus(DBusConnection *aConnection);

    /**
     * The constructor of a d-bus object.
     *
     * @param[in]   aConnection     The dbus connection.
     * @param[in]   aInterfaceName  The network interface name.
     *
     */
    ThreadApiDBus(DBusConnection *aConnection, const std::string &aInterfaceName);

    /**
     * This method adds a callback for device role change.
     *
     * @param[in]   aHandler  The device role handler.
     *
     */
    void AddDeviceRoleHandler(const DeviceRoleHandler &aHandler);

    /**
     * This method adds an unsecure thread port.
     *
     * @param[in]   aPort     The port number.
     * @param[in]   aSeconds  The timeout to close the port, 0 for never close.
     *
     * @retval OT_ERROR_NONE successfully performed the dbus function call
     * @returns Error value otherwise
     *
     */
    OtbrClientError AddUnsecurePort(uint16_t aPort, uint32_t aSeconds);

    /**
     * This method performs a Thread network scan.
     *
     * @param[in]   aHandler  The scan result handler.
     *
     * @retval OT_ERROR_NONE successfully performed the dbus function call
     * @returns Error value otherwise
     *
     */
    OtbrClientError Scan(const ScanHandler &aHandler);

    /**
     * This method attaches the device to the Thread network.
     * @param[in]   aNetworkName    The network name.
     * @param[in]   aPanId          The pan id, UINT16_MAX for random.
     * @param[in]   aExtPanId       The extended pan id, UINT64_MAX for random.
     * @param[in]   aMasterKey      The master key, empty for random.
     * @param[in]   aPSKc           The pre-shared commissioner key, empty for random.
     * @param[in]   aChannelMask    A bitmask for valid channels, will random select one.
     * @param[in]   aHandler        The attach result handler.
     *
     * @retval OT_ERROR_NONE successfully performed the dbus function call
     * @returns Error value otherwise
     *
     */
    OtbrClientError Attach(const std::string &         aNetworkName,
                           uint16_t                    aPanId,
                           uint64_t                    aExtPanId,
                           const std::vector<uint8_t> &aMasterKey,
                           const std::vector<uint8_t> &aPSKc,
                           uint32_t                    aChannelMask,
                           const OtResultHandler &     aHandler);

    /**
     * This method performs a factory reset.
     *
     * @param[in]   aHandler        The reset result handler.
     *
     * @retval OT_ERROR_NONE successfully performed the dbus function call
     * @returns Error value otherwise
     *
     */
    OtbrClientError FactoryReset(const OtResultHandler &aHandler);

    /**
     * This method performs a soft reset.
     *
     * @retval OT_ERROR_NONE successfully performed the dbus function call
     * @returns Error value otherwise
     *
     */
    OtbrClientError Reset(void);

    /**
     * This method triggers a thread join process.
     *
     * @note The joiner start and the attach proccesses are exclusive
     *
     * @param[in]   aPskd             The pre-shared key for device.
     * @param[in]   aProvisioningUrl  The provision url.
     * @param[in]   aVendorName       The vendor name.
     * @param[in]   aVendorModel      The vendor model.
     * @param[in]   aVendorSwVersion  The vendor software version.
     * @param[in]   aVendorData       The vendor custom data.
     * @param[in]   aHandler          The join result handler.
     *
     * @retval OT_ERROR_NONE successfully performed the dbus function call
     * @returns Error value otherwise
     *
     */
    OtbrClientError JoinerStart(const std::string &    aPskd,
                                const std::string &    aProvisioningUrl,
                                const std::string &    aVendorName,
                                const std::string &    aVendorModel,
                                const std::string &    aVendorSwVersion,
                                const std::string &    aVendorData,
                                const OtResultHandler &aHandler);

    /**
     * This method stops the joiner process
     *
     * @retval OT_ERROR_NONE successfully performed the dbus function call
     * @returns Error value otherwise
     *
     */
    OtbrClientError JoinerStop(void);

    /**
     * This method adds a on-mesh address prefix.
     *
     * @param[in]   aPrefix     The address prefix.
     *
     * @retval OT_ERROR_NONE successfully performed the dbus function call
     * @returns Error value otherwise
     *
     */
    OtbrClientError AddOnMeshPrefix(const otbrOnMeshPrefix &aPrefix);

    /**
     * This method removes a on-mesh address prefix.
     *
     * @param[in]   aPrefix     The address prefix.
     *
     * @retval OT_ERROR_NONE successfully performed the dbus function call
     * @returns Error value otherwise
     *
     */
    OtbrClientError RemoveOnMeshPrefix(const otbrIp6Prefix &aPrefix);

    /**
     * This method sets the mesh-local prefix.
     *
     * @param[in]   aPrefix     The address prefix.
     *
     * @retval OT_ERROR_NONE successfully performed the dbus function call
     * @returns Error value otherwise
     *
     */
    OtbrClientError SetMeshLocalPrefix(const std::array<uint8_t, OTBR_IP6_PREFIX_SIZE> &aPrefix);

    /**
     * This method sets the legacy prefix of ConnectIP.
     *
     * @param[in]   aPrefix     The address prefix.
     *
     * @retval OT_ERROR_NONE successfully performed the dbus function call
     * @returns Error value otherwise
     *
     */
    OtbrClientError SetLegacyUlaPrefix(const std::array<uint8_t, OTBR_IP6_PREFIX_SIZE> &aPrefix);

    /**
     * This method sets the link operating mode.
     *
     * @param[in]   aConfig   The operating mode config.
     *
     * @retval OT_ERROR_NONE successfully performed the dbus function call
     * @returns Error value otherwise
     *
     */
    OtbrClientError SetLinkMode(const otbrLinkModeConfig &aConfig);

    /**
     * This method gets the link operating mode.
     *
     * @param[out]  aConfig   The operating mode config.
     *
     * @retval OT_ERROR_NONE successfully performed the dbus function call
     * @returns Error value otherwise
     *
     */
    OtbrClientError GetLinkMode(otbrLinkModeConfig &aConfig);

    /**
     * This method gets the current device role.
     *
     * @param[out]  aDeviceRole   The device role
     *
     * @retval OT_ERROR_NONE successfully performed the dbus function call
     * @returns Error value otherwise
     *
     */
    OtbrClientError GetDeviceRole(otbrDeviceRole &aDeviceRole);

    /**
     * This method gets the network name.
     *
     * @param[out]  aName   The network name.
     *
     * @retval OT_ERROR_NONE successfully performed the dbus function call
     * @returns Error value otherwise
     *
     */
    OtbrClientError GetNetworkName(std::string &aName);

    /**
     * This method gets the network pan id.
     *
     * @param[out]  aPanId  The pan id.
     *
     * @retval OT_ERROR_NONE successfully performed the dbus function call
     * @returns Error value otherwise
     *
     */
    OtbrClientError GetPanId(uint16_t &aPanId);

    /**
     * This method gets the extended pan id.
     *
     * @param[out]  aExtPanId   The extended pan id.
     *
     * @retval OT_ERROR_NONE successfully performed the dbus function call
     * @returns Error value otherwise
     *
     */
    OtbrClientError GetExtPanId(uint64_t &aExtPanId);

    /**
     * This method gets the extended pan id.
     *
     * @param[out]  aChannel   The extended pan id.
     *
     * @retval OT_ERROR_NONE successfully performed the dbus function call
     * @returns Error value otherwise
     *
     */
    OtbrClientError GetChannel(uint16_t &aChannel);

    /**
     * This method gets the network master key.
     *
     * @param[out]  aMasterKey   The network master key.
     *
     * @retval OT_ERROR_NONE successfully performed the dbus function call
     * @returns Error value otherwise
     *
     */
    OtbrClientError GetMasterKey(std::vector<uint8_t> &aMasterKey);

    /**
     * This method gets the Clear Channel Assessment failure rate.
     *
     * @param[out]  aFailureRate   The failure rate.
     *
     * @retval OT_ERROR_NONE successfully performed the dbus function call
     * @returns Error value otherwise
     *
     */
    OtbrClientError GetCcaFailureRate(uint16_t &aFailureRate);

    /**
     * This method gets the mac level statistics counters.
     *
     * @param[out]  aCounters    The statistic counters.
     *
     * @retval OT_ERROR_NONE successfully performed the dbus function call
     * @returns Error value otherwise
     *
     */
    OtbrClientError GetLinkCounters(otbrMacCounters &aCounters); // For telemetry

    /**
     * This method gets the ip level statistics counters.
     *
     * @param[out]  aCounters    The statistic counters.
     *
     * @retval OT_ERROR_NONE successfully performed the dbus function call
     * @returns Error value otherwise
     *
     */
    OtbrClientError GetIp6Counters(otbrIpCounters &aCounters); // For telemetry

    /**
     * This method gets the supported channel mask.
     *
     * @param[out]  aChannelMask   The channel mask.
     *
     * @retval OT_ERROR_NONE successfully performed the dbus function call
     * @returns Error value otherwise
     *
     */
    OtbrClientError GetSupportedChannelMask(uint32_t &aChannelMask);

    OtbrClientError GetRloc16(uint16_t &aRloc16);

    OtbrClientError GetExtendedAddress(uint64_t &aExtendedAddress);

    OtbrClientError GetRouterId(uint8_t &aRouterId);

    OtbrClientError GetLeaderData(otbrLeaderData &aLeaderData);

    OtbrClientError GetNetworkData(std::vector<uint8_t> &aNetworkData);

    OtbrClientError GetStableNetworkData(std::vector<uint8_t> &aNetworkData);

    OtbrClientError GetLocalLeaderWeight(uint8_t &aWeight);

    OtbrClientError GetChannelMonitorSampleCount(uint32_t &aSampleCount);

    OtbrClientError GetChannelMonitorChannelQualityMap(std::vector<otbrChannelQuality> &aQualityMap);

    OtbrClientError GetChildTable(std::vector<otbrChildInfo> &aChildTable);

    OtbrClientError GetNeighborTable(std::vector<otbrNeighborInfo> &aNeighborTable);

    OtbrClientError GetPartitionId(uint32_t &aPartitionId);

    OtbrClientError GetInstantRssi(int8_t &aRssi);

    OtbrClientError GetRadioTxPower(int8_t &aTxPower);

    /**
     * This method returns the network interface name the client is bound to.
     *
     * @returns The network interface name.
     *
     */
    std::string GetInterfaceName(void);

private:
    OtbrClientError CallDBusMethodSync(const std::string &aMethodName);
    OtbrClientError CallDBusMethodAsync(const std::string &aMethodName, DBusPendingCallNotifyFunction aFunction);

    template <typename ArgType>
    OtbrClientError CallDBusMethodSync(const std::string &aMethodName, const ArgType &aArgs);

    template <typename ArgType>
    OtbrClientError CallDBusMethodAsync(const std::string &           aMethodName,
                                        const ArgType &               aArgs,
                                        DBusPendingCallNotifyFunction aFunction);

    template <typename ValType> OtbrClientError SetProperty(const std::string &aPropertyName, const ValType &aValue);

    template <typename ValType> OtbrClientError GetProperty(const std::string &aPropertyName, ValType &aValue);

    OtbrClientError          SubscribeDeviceRoleSignal(void);
    static DBusHandlerResult sDBusMessageFilter(DBusConnection *aConnection, DBusMessage *aMessage, void *aData);
    DBusHandlerResult        DBusMessageFilter(DBusConnection *aConnection, DBusMessage *aMessage);

    template <void (ThreadApiDBus::*Handler)(DBusPendingCall *aPending)>
    static void sHandleDBusPendingCall(DBusPendingCall *aPending, void *aThreadApiDBus);

    void        AttachPendingCallHandler(DBusPendingCall *aPending);
    void        FactoryResetPendingCallHandler(DBusPendingCall *aPending);
    void        JoinerStartPendingCallHandler(DBusPendingCall *aPending);
    static void sScanPendingCallHandler(DBusPendingCall *aPending, void *aThreadApiDBus);
    void        ScanPendingCallHandler(DBusPendingCall *aPending);

    static void EmptyFree(void *aData) { (void)aData; }

    std::string mInterfaceName;

    DBusConnection *mConnection;

    ScanHandler     mScanHandler;
    OtResultHandler mAttachHandler;
    OtResultHandler mFactoryResetHandler;
    OtResultHandler mJoinerHandler;

    std::vector<DeviceRoleHandler> mDeviceRoleHandlers;
};

} // namespace DBus
} // namespace otbr

#endif // OTBR_THREAD_API_DBUS_HPP_
