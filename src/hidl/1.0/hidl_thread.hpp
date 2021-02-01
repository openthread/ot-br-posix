/*
 *    Copyright (c) 2021, The OpenThread Authors.
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

#pragma once

#include <openthread-br/config.h>

#include <android/hardware/thread/1.0/IThread.h>
#include <android/hardware/thread/1.0/IThreadCallback.h>
#include <android/hardware/thread/1.0/IThreadMdns.h>
#include <hidl/MQDescriptor.h>
#include <hidl/Status.h>
#include <openthread/link.h>

#include "agent/ncp_openthread.hpp"
#include "hidl/1.0/hidl_death_recipient.hpp"

namespace otbr {
namespace Hidl {
using ::android::sp;
using ::android::hardware::hidl_array;
using ::android::hardware::hidl_memory;
using ::android::hardware::hidl_string;
using ::android::hardware::hidl_vec;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::hardware::thread::V1_0::IThread;
using ::android::hardware::thread::V1_0::IThreadCallback;
using ::android::hardware::thread::V1_0::ThreadError;

/**
 * This class implements the HIDL Thread service.
 *
 */
struct HidlThread : public IThread
{
public:
    /**
     * The constructor of a HIDL object.
     *
     * Will use the default interfacename
     *
     * @param[in] aNcp  The ncp controller.
     *
     */
    explicit HidlThread(otbr::Ncp::ControllerOpenThread *aNcp);

    /**
     * This method performs initialization for the HIDL Thread service.
     *
     */
    void Init(void);

    /**
     * This method initalizes the HIDL Thread callback object.
     *
     */
    Return<void> initialize(const sp<IThreadCallback> &aCallback) override;

    /**
     * This method deinitalizes the HIDL Thread callback object.
     *
     */
    Return<void> deinitialize(void) override;

    /**
     * This method permits unsecure join on port.
     *
     * @param[in] aPort     The port number.
     * @param[in] aSeconds  The timeout to close the port, 0 for never close.
     *
     * @retval ERROR_NONE  Successfully performed the HIDL function call.
     * @retval ERROR_HIDL  Hidl encode/decode error.
     * @retval ...         OpenThread defined error value otherwise.
     *
     */
    Return<ThreadError> permitUnsecureJoin(uint16_t aPort, uint32_t aSeconds) override;

    /**
     * This method performs a Thread network scan.
     *
     * Note: The callback function `IThreadCallback::onScan()` is called after the Active Scan has completed.
     *
     * @retval ERROR_NONE  Successfully performed the HIDL function call.
     * @retval ERROR_HIDL  Hidl encode/decode error.
     * @retval ...         OpenThread defined error value otherwise.
     *
     */
    Return<ThreadError> scan() override;

    /**
     * This method attaches the device to the Thread network.
     *
     * Note: The callback function `IThreadCallback::onAttach()` is called after the attach process has completed.
     *
     * @param[in] aNetworkName  The network name.
     * @param[in] aPanId        The pan id, UINT16_MAX for random.
     * @param[in] aExtPanId     The extended pan id, UINT64_MAX for random.
     * @param[in] aMasterKey    The master key, empty for random.
     * @param[in] aPSKc         The pre-shared commissioner key, empty for random.
     * @param[in] aChannelMask  A bitmask for valid channels, will random select one.
     *
     * @retval ERROR_NONE  Successfully performed the HIDL function call.
     * @retval ERROR_HIDL  Hidl encode/decode error.
     * @retval ...         OpenThread defined error value otherwise.
     *
     */
    Return<ThreadError> attach(const hidl_string &      aNetworkName,
                               uint16_t                 aPanId,
                               uint64_t                 aExtPanId,
                               const hidl_vec<uint8_t> &aMasterKey,
                               const hidl_vec<uint8_t> &aPSKc,
                               uint32_t                 aChannelMask) override;

    /**
     * This method attaches the device to the Thread network.
     *
     * The network parameters will be set with the active dataset under this
     * circumstance.
     *
     * Note: The callback function `IThreadCallback::onAttach()` is called after the attach process has completed.
     *
     * @retval ERROR_NONE successfully performed the dbus function call
     * @retval ERROR_HIDL Hidl encode/decode error
     * @retval ...        OpenThread defined error value otherwise
     *
     */
    Return<ThreadError> attachActiveDataset() override;

    /**
     * This method performs a factory reset.
     *
     * @retval ERROR_NONE  Successfully performed the HIDL function call.
     * @retval ERROR_HIDL  Hidl encode/decode error.
     * @retval ...         OpenThread defined error value otherwise.
     *
     */
    Return<ThreadError> factoryReset() override;

    /**
     * This method performs a soft reset.
     *
     * @retval ERROR_NONE  Successfully performed the HIDL function call.
     * @retval ERROR_HIDL  Hidl encode/decode error.
     * @retval ...         OpenThread defined error value otherwise.
     *
     */
    Return<ThreadError> reset() override;

    /**
     * This method triggers a thread join process. The joiner start and the attach proccesses are exclusive.
     *
     * Note: The callback function `IThreadCallback::onJoinerStart()` is called after the join operation has completed.
     *
     * @param[in] aPskd             The pre-shared key for device.
     * @param[in] aProvisioningUrl  The provision url.
     * @param[in] aVendorName       The vendor name.
     * @param[in] aVendorModel      The vendor model.
     * @param[in] aVendorSwVersion  The vendor software version.
     * @param[in] aVendorData       The vendor custom data.
     * @param[in] aHandler          The join result handler.
     *
     * @retval ERROR_NONE  Successfully performed the HIDL function call.
     * @retval ERROR_HIDL  Hidl encode/decode error.
     * @retval ...         OpenThread defined error value otherwise.
     *
     */
    Return<ThreadError> joinerStart(const hidl_string &aPskd,
                                    const hidl_string &aProvisioningUrl,
                                    const hidl_string &aVendorName,
                                    const hidl_string &aVendorModel,
                                    const hidl_string &aVendorSwVersion,
                                    const hidl_string &aVendorData) override;

    /**
     * This method stops the joiner process.
     *
     * @retval ERROR_NONE  Successfully performed the HIDL function call.
     * @retval ERROR_HIDL  Hidl encode/decode error.
     * @retval ...         OpenThread defined error value otherwise.
     *
     */
    Return<ThreadError> joinerStop() override;

    /**
     * This method adds a on-mesh address prefix.
     *
     * @param[in] aPrefix  The address prefix.
     *
     * @retval ERROR_NONE  Successfully performed the HIDL function call.
     * @retval ERROR_HIDL  Hidl encode/decode error.
     * @retval ...         OpenThread defined error value otherwise.
     *
     */
    Return<ThreadError> addOnMeshPrefix(const ::android::hardware::thread::V1_0::OnMeshPrefix &aPrefix) override;

    /**
     * This method removes a on-mesh address prefix.
     *
     * @param[in] aPrefix  The address prefix.
     *
     * @retval ERROR_NONE  Successfully performed the HIDL function call.
     * @retval ERROR_HIDL  Hidl encode/decode error.
     * @retval ...         OpenThread defined error value otherwise.
     *
     */
    Return<ThreadError> removeOnMeshPrefix(const ::android::hardware::thread::V1_0::Ip6Prefix &aPrefix) override;

    /**
     * This method adds an external route.
     *
     * @param[in] aExternalroute  The external route config.
     *
     * @retval ERROR_NONE  Successfully performed the HIDL function call.
     * @retval ERROR_HIDL  Hidl encode/decode error.
     * @retval ...         OpenThread defined error value otherwise.
     *
     */
    Return<ThreadError> addExternalRoute(
        const ::android::hardware::thread::V1_0::ExternalRoute &aExternalRoute) override;

    /**
     * This method removes an external route.
     *
     * @param[in] aPrefix  The route prefix.
     *
     * @retval ERROR_NONE  Successfully performed the HIDL function call.
     * @retval ERROR_HIDL  Hidl encode/decode error.
     * @retval ...         OpenThread defined error value otherwise.
     *
     */
    Return<ThreadError> removeExternalRoute(const ::android::hardware::thread::V1_0::Ip6Prefix &aPrefix) override;

    /**
     * This method sets the mesh-local prefix.
     *
     * @param[in] aPrefix  The address prefix.
     *
     * @retval ERROR_NONE  Successfully performed the HIDL function call.
     * @retval ERROR_HIDL  Hidl encode/decode error.
     * @retval ...         OpenThread defined error value otherwise.
     *
     */
    Return<ThreadError> setMeshLocalPrefix(const hidl_array<uint8_t, 8> &aPrefix) override;

    /**
     * This method sets the legacy prefix of ConnectIP.
     *
     * @param[in] aPrefix  The address prefix.
     *
     * @retval ERROR_NONE  Successfully performed the HIDL function call.
     * @retval ERROR_HIDL  Hidl encode/decode error.
     * @retval ...         OpenThread defined error value otherwise.
     *
     */
    Return<ThreadError> setLegacyUlaPrefix(const hidl_array<uint8_t, 8> &aPrefix) override;

    /**
     * This method sets the active operational dataset.
     *
     * @param[in] aDataset  The active operational dataset
     *
     * @retval ERROR_NONE successfully performed the dbus function call
     * @retval ERROR_HIDL Hidl encode/decode error
     * @retval ...        OpenThread defined error value otherwise
     *
     */
    Return<ThreadError> setActiveDatasetTlvs(
        const ::android::hardware::thread::V1_0::OperationalDatasetTlvs &aDataset) override;

    /**
     * This method sets the link operating mode.
     *
     * @param[in] aConfig  The operating mode config.
     *
     * @retval ERROR_NONE  Successfully performed the HIDL function call.
     * @retval ERROR_HIDL  Hidl encode/decode error.
     * @retval ...         OpenThread defined error value otherwise.
     *
     */
    Return<ThreadError> setLinkMode(const ::android::hardware::thread::V1_0::LinkModeConfig &aConfig) override;

    /**
     * This method sets the radio region.
     *
     * @param[in] aRegion  The region, can be CA, US or WW.
     *
     * @retval ERROR_NONE  Successfully performed the HIDL function call.
     * @retval ERROR_HIDL  Hidl encode/decode error.
     * @retval ...         OpenThread defined error value otherwise.
     *
     */
    Return<ThreadError> setRadioRegion(const hidl_string &aRegion) override;

    /**
     * This method gets the link operating mode.
     *
     * @param[in] _hidl_cb  A pointer to the get link mode callback function.
     */
    Return<void> getLinkMode(getLinkMode_cb _hidl_cb) override;

    /**
     * This method gets the current device role.
     *
     * @param[in] _hidl_cb  A pointer to the get device role callback function.
     *
     */
    Return<void> getDeviceRole(getDeviceRole_cb _hidl_cb) override;

    /**
     * This method gets the network name.
     *
     * @param[in] _hidl_cb  A pointer to the get network name callback function.
     *
     */
    Return<void> getNetworkName(getNetworkName_cb _hidl_cb) override;

    /**
     * This method gets the network pan id.
     *
     * @param[in] _hidl_cb  A pointer to the get network pan id callback function.
     *
     */
    Return<void> getPanId(getPanId_cb _hidl_cb) override;

    /**
     * This method gets the extended pan id.
     *
     * @param[in] _hidl_cb  A pointer to the get extended pan id callback function.
     *
     */
    Return<void> getExtPanId(getExtPanId_cb _hidl_cb) override;

    /**
     * This method gets the channel.
     *
     * @param[in] _hidl_cb  A pointer to the get channel callback callback function.
     *
     */
    Return<void> getChannel(getChannel_cb _hidl_cb) override;

    /**
     * This method gets the network master key.
     *
     * @param[in] _hidl_cb  A pointer to the get network master key callback function.
     *
     */
    Return<void> getMasterKey(getMasterKey_cb _hidl_cb) override;

    /**
     * This method gets the Clear Channel Assessment failure rate.
     *
     * @param[in] _hidl_cb  A pointer to the get Clear Channel Assessment failure rate callback function.
     *
     */
    Return<void> getCcaFailureRate(getCcaFailureRate_cb _hidl_cb) override;

    /**
     * This method gets the mac level statistics counters.
     *
     * @param[in] _hidl_cb  A pointer to the get mac level statistic counters callback function.
     *
     */
    Return<void> getLinkCounters(getLinkCounters_cb _hidl_cb) override;

    /**
     * This method gets the ip level statistics counters.
     *
     * @param[in] _hidl_cb  A pointer to the get ip level statistic counters callback function.
     *
     */
    Return<void> getIp6Counters(getIp6Counters_cb _hidl_cb) override;

    /**
     * This method gets the supported channel mask.
     *
     * @param[in] _hidl_cb  A pointer to the get supported channel mask callback function.
     *
     */
    Return<void> getSupportedChannelMask(getSupportedChannelMask_cb _hidl_cb) override;

    /**
     * This method gets the Thread routing locator.
     *
     * @param[in] _hidl_cb  A pointer to the get Thread routing locator callback function.
     *
     */
    Return<void> getRloc16(getRloc16_cb _hidl_cb) override;

    /**
     * This method gets 802.15.4 extended address.
     *
     * @param[in] _hidl_cb  A pointer to the get 802.15.4 extended address callback function.
     *
     */
    Return<void> getExtendedAddress(getExtendedAddress_cb _hidl_cb) override;

    /**
     * This method gets the node's router id.
     *
     * @param[in] _hidl_cb  A pointer to the get node's router id callback function.
     *
     */
    Return<void> getRouterId(getRouterId_cb _hidl_cb) override;

    /**
     * This method gets the network's leader data.
     *
     * @param[in] _hidl_cb  A pointer to the get network's leader data callback function.
     *
     */
    Return<void> getLeaderData(getLeaderData_cb _hidl_cb) override;

    /**
     * This method gets the network data.
     *
     * @param[in] _hidl_cb  A pointer to the get network data callback function.
     *
     */
    Return<void> getNetworkData(getNetworkData_cb _hidl_cb) override;

    /**
     * This method gets the stable network data.
     *
     * @param[in] _hidl_cb  A pointer to the get stable network data callback function.
     *
     */
    Return<void> getStableNetworkData(getStableNetworkData_cb _hidl_cb) override;

    /**
     * This method gets the node's local leader weight.
     *
     * @param[in] _hidl_cb  A pointer to the get node's local leader weight callback function.
     *
     */
    Return<void> getLocalLeaderWeight(getLocalLeaderWeight_cb _hidl_cb) override;

    /**
     * This method gets the channel monitor sample count.
     *
     * @param[in] _hidl_cb  A pointer to the get channel monitor sample count callback function.
     *
     */
    Return<void> getChannelMonitorSampleCount(getChannelMonitorSampleCount_cb _hidl_cb) override;

    /**
     * This method gets the channel qualities.
     *
     * @param[in] _hidl_cb  A pointer to the get channel qualities callback function.
     *
     */
    Return<void> getChannelMonitorAllChannelQualities(getChannelMonitorAllChannelQualities_cb _hidl_cb) override;

    /**
     * This method gets the child table.
     *
     * @param[in] _hidl_cb  A pointer to the get child table callback function.
     *
     */
    Return<void> getChildTable(getChildTable_cb _hidl_cb) override;

    /**
     * This method gets the neighbor table.
     *
     * @param[in] _hidl_cb  A pointer to the get neighbor table callback function.
     *
     */
    Return<void> getNeighborTable(getNeighborTable_cb _hidl_cb) override;

    /**
     * This method gets the network's parition id.
     *
     * @param[in] _hidl_cb  A pointer to the get network's parition id callback function.
     *
     */
    Return<void> getPartitionId(getPartitionId_cb _hidl_cb) override;

    /**
     * This method gets the rssi of the latest packet.
     *
     * @param[in] _hidl_cb  A pointer to the get rssi of the latest packet callback function.
     *
     */
    Return<void> getInstantRssi(getInstantRssi_cb _hidl_cb) override;

    /**
     * This method gets the radio transmit power.
     *
     * @param[in] _hidl_cb  A pointer to the get radio transmit power callback function.
     *
     */
    Return<void> getRadioTxPower(getRadioTxPower_cb _hidl_cb) override;

    /**
     * This method gets the external route table.
     *
     * @param[in] _hidl_cb  A pointer to the get external route table callback function.
     *
     */
    Return<void> getExternalRoutes(getExternalRoutes_cb _hidl_cb) override;

    /**
     * This method gets the active operational dataset
     *
     * @param[in] _hidl_cb  A pointer to the get active dataset tlvs callback function.
     */
    Return<void> getActiveDatasetTlvs(getActiveDatasetTlvs_cb _hidl_cb) override;

    /**
     * This method gets the radio region.
     *
     * @param[in] _hidl_cb  A pointer to the get region callback function.
     *
     */
    Return<void> getRadioRegion(getRadioRegion_cb _hidl_cb) override;

private:
    void ScanResultHandler(otError aError, const std::vector<otActiveScanResult> &aResult);
    void DeviceRoleHandler(otDeviceRole aDeviceRole);
    void NcpResetHandler(void);

    static void sClientDeathCallback(void *aContext)
    {
        HidlThread *hidlServer = static_cast<HidlThread *>(aContext);
        hidlServer->deinitialize();
    }

    otbr::Ncp::ControllerOpenThread *mNcp;
    sp<IThreadCallback>              mThreadCallback;
    sp<ClientDeathRecipient>         mDeathRecipient;
};

} // namespace Hidl
} // namespace otbr
