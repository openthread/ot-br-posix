/*
 *  Copyright (c) 2024, The OpenThread Authors.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 *   This file includes definitions of Thead Controller Interface.
 */

#ifndef OTBR_AGENT_THREAD_HOST_HPP_
#define OTBR_AGENT_THREAD_HOST_HPP_

#include <functional>
#include <memory>

#include <openthread/backbone_router_ftd.h>
#include <openthread/border_agent.h>
#include <openthread/dataset.h>
#include <openthread/error.h>
#include <openthread/thread.h>

#include "lib/spinel/coprocessor_type.h"

#include "common/logging.hpp"
#include "posix/udp_proxy.hpp"

namespace otbr {
namespace Host {

/**
 * This interface provides access to some Thread network properties in a sync way.
 *
 * The APIs are unified for both NCP and RCP cases.
 */
class NetworkProperties
{
public:
    /**
     * Returns the device role.
     *
     * @returns the device role.
     */
    virtual otDeviceRole GetDeviceRole(void) const = 0;

    /**
     * Returns whether or not the IPv6 interface is up.
     *
     * @retval TRUE   The IPv6 interface is enabled.
     * @retval FALSE  The IPv6 interface is disabled.
     */
    virtual bool Ip6IsEnabled(void) const = 0;

    /**
     * Returns the Partition ID.
     *
     * @returns The Partition ID.
     */
    virtual uint32_t GetPartitionId(void) const = 0;

    /**
     * Returns the active operational dataset tlvs.
     *
     * @param[out] aDatasetTlvs  A reference to where the Active Operational Dataset will be placed.
     */
    virtual void GetDatasetActiveTlvs(otOperationalDatasetTlvs &aDatasetTlvs) const = 0;

    /**
     * Returns the pending operational dataset tlvs.
     *
     * @param[out] aDatasetTlvs  A reference to where the Pending Operational Dataset will be placed.
     */
    virtual void GetDatasetPendingTlvs(otOperationalDatasetTlvs &aDatasetTlvs) const = 0;

    /**
     * Returns the meshlocal prefix.
     *
     * @returns The mesh local prefix.
     */
    virtual const otMeshLocalPrefix *GetMeshLocalPrefix(void) const = 0;

    /**
     * The destructor.
     */
    virtual ~NetworkProperties(void) = default;
};

enum ThreadEnabledState
{
    kStateDisabled  = 0,
    kStateEnabled   = 1,
    kStateDisabling = 2,
    kStateInvalid   = 255,
};

/**
 * This class is an interface which provides a set of async APIs to control the
 * Thread network.
 *
 * The APIs are unified for both NCP and RCP cases.
 */
class ThreadHost : virtual public NetworkProperties, public UdpProxy::Dependencies
{
public:
    using AsyncResultReceiver = std::function<void(otError, const std::string &)>;
    using ChannelMasksReceiver =
        std::function<void(uint32_t /*aSupportedChannelMask*/, uint32_t /*aPreferredChannelMask*/)>;
    using DeviceRoleHandler                        = std::function<void(otError, otDeviceRole)>;
    using ThreadStateChangedCallback               = std::function<void(otChangedFlags)>;
    using ThreadEnabledStateCallback               = std::function<void(ThreadEnabledState)>;
    using ThreadRoleChangedCallback                = std::function<void(otDeviceRole)>;
    using BorderAgentMeshCoPServiceChangedCallback = std::function<void(bool, uint16_t, const uint8_t *, uint16_t)>;
    using EphemeralKeyStateChangedCallback         = std::function<void(otBorderAgentEphemeralKeyState, uint16_t)>;
    /**
     * Callback for forwarding UDP packets to the host.
     *
     * @param[in] aPayload         Pointer to the UDP payload.
     * @param[in] aPayloadLength   Length of the UDP payload.
     * @param[in] aPeerAddress     IPv6 address of the peer.
     * @param[in] aPeerPort        UDP port of the peer.
     * @param[in] aLocalPort       Local UDP port (on the Thread side).
     */
    using UdpForwardToHostCallback =
        std::function<void(const uint8_t *, uint16_t, const otIp6Address &, uint16_t, uint16_t)>;
    using BackboneRouterMulticastListenerCallback =
        std::function<void(otBackboneRouterMulticastListenerEvent, Ip6Address)>;
    using BackboneRouterStateChangedCallback = std::function<void(otBackboneRouterState)>;

    struct ChannelMaxPower
    {
        uint16_t mChannel;
        int16_t  mMaxPower; // INT16_MAX indicates that the corresponding channel is disabled.
    };

    /**
     * Create a Thread Controller Instance.
     *
     * This is a factory method that will decide which implementation class will be created.
     *
     * @param[in]   aInterfaceName          A string of the Thread interface name.
     * @param[in]   aRadioUrls              The radio URLs (can be IEEE802.15.4 or TREL radio).
     * @param[in]   aBackboneInterfaceName  The Backbone network interface name.
     * @param[in]   aDryRun                 TRUE to indicate dry-run mode. FALSE otherwise.
     * @param[in]   aEnableAutoAttach       Whether or not to automatically attach to the saved network.
     * @param[in]   aDataPath               Path of directory to store data.
     *
     * @returns Non-null OpenThread Controller instance.
     */
    static std::unique_ptr<ThreadHost> Create(const char                      *aInterfaceName,
                                              const std::vector<const char *> &aRadioUrls,
                                              const char                      *aBackboneInterfaceName,
                                              bool                             aDryRun,
                                              bool                             aEnableAutoAttach,
                                              const char                      *aDataPath = "");

    /**
     * This method joins this device to the network specified by @p aActiveOpDatasetTlvs.
     *
     * If there is an ongoing 'Join' operation, no action will be taken and @p aReceiver will be
     * called after the request is completed. The previous @p aReceiver will also be called.
     *
     * @param[in] aActiveOpDatasetTlvs  A reference to the active operational dataset of the Thread network.
     * @param[in] aReceiver             A receiver to get the async result of this operation.
     */
    virtual void Join(const otOperationalDatasetTlvs &aActiveOpDatasetTlvs, const AsyncResultReceiver &aRecevier) = 0;

    /**
     * This method instructs the device to leave the current network gracefully.
     *
     * 1. If there is already an ongoing 'Leave' operation, no action will be taken and @p aReceiver
     *    will be called after the previous request is completed. The previous @p aReceiver will also
     *    be called.
     * 2. If this device is not in disabled state, OTBR sends Address Release Notification (i.e. ADDR_REL.ntf)
     *    to gracefully detach from the current network and it takes 1 second to finish.
     * 3. Then Operational Dataset will be removed from persistent storage if @p aEraseDataset is true.
     * 4. If everything goes fine, @p aReceiver will be invoked with OT_ERROR_NONE. Otherwise, other errors
     *    will be passed to @p aReceiver when the error happens.
     *
     * @param[in] aReceiver  A receiver to get the async result of this operation.
     */
    virtual void Leave(bool aEraseDataset, const AsyncResultReceiver &aRecevier) = 0;

    /**
     * This method migrates this device to the new network specified by @p aPendingOpDatasetTlvs.
     *
     * @param[in] aPendingOpDatasetTlvs  A reference to the pending operational dataset of the Thread network.
     * @param[in] aReceiver              A receiver to get the async result of this operation.
     */
    virtual void ScheduleMigration(const otOperationalDatasetTlvs &aPendingOpDatasetTlvs,
                                   const AsyncResultReceiver       aReceiver) = 0;

    /**
     * This method enables/disables the Thread network.
     *
     * 1. If there is an ongoing 'SetThreadEnabled' operation, no action will be taken and @p aReceiver
     *    will be invoked with error OT_ERROR_BUSY.
     * 2. If the host hasn't been initialized, @p aReceiver will be invoked with error OT_ERROR_INVALID_STATE.
     * 3. When @p aEnabled is false, this method will first trigger a graceful detach and then disable Thread
     *    network interface and the stack.
     *
     * @param[in] aEnabled  true to enable and false to disable.
     * @param[in] aReceiver  A receiver to get the async result of this operation.
     */
    virtual void SetThreadEnabled(bool aEnabled, const AsyncResultReceiver aReceiver) = 0;

    /**
     * This method sets the country code.
     *
     * The country code refers to the 2-alpha code defined in ISO-3166.
     *
     * 1. If @p aCountryCode isn't valid, @p aReceiver will be invoked with error OT_ERROR_INVALID_ARGS.
     * 2. If the host hasn't been initialized, @p aReceiver will be invoked with error OT_ERROR_INVALID_STATE.
     *
     * @param[in] aCountryCode  The country code.
     */
    virtual void SetCountryCode(const std::string &aCountryCode, const AsyncResultReceiver &aReceiver) = 0;

    /**
     * Gets the supported and preferred channel masks.
     *
     * If the operation succeeded, @p aReceiver will be invoked with the supported and preferred channel masks.
     * Otherwise, @p aErrReceiver will be invoked with the error and @p aReceiver won't be invoked in this case.
     *
     * @param aReceiver     A receiver to get the channel masks.
     * @param aErrReceiver  A receiver to get the error if the operation fails.
     */
    virtual void GetChannelMasks(const ChannelMasksReceiver &aReceiver, const AsyncResultReceiver &aErrReceiver) = 0;

#if OTBR_ENABLE_POWER_CALIBRATION
    /**
     * Sets the max power of each channel.
     *
     * 1. If the host hasn't been initialized, @p aReceiver will be invoked with error OT_ERROR_INVALID_STATE.
     * 2. If any value in @p aChannelMaxPowers is invalid, @p aReceiver will be invoked with error
     * OT_ERROR_INVALID_ARGS.
     *
     * @param[in] aChannelMaxPowers  A vector of ChannelMaxPower.
     * @param[in] aReceiver          A receiver to get the async result of this operation.
     */
    virtual void SetChannelMaxPowers(const std::vector<ChannelMaxPower> &aChannelMaxPowers,
                                     const AsyncResultReceiver          &aReceiver) = 0;
#endif

    /**
     * This method adds a event listener for Thread state changes.
     *
     * @param[in] aCallback  The callback to receive Thread state changed events.
     */
    virtual void AddThreadStateChangedCallback(ThreadStateChangedCallback aCallback) = 0;

    /**
     * This method adds a event listener for Thread Enabled state changes.
     *
     * @param[in] aCallback  The callback to receive Thread Enabled state changed events.
     */
    virtual void AddThreadEnabledStateChangedCallback(ThreadEnabledStateCallback aCallback) = 0;

    /**
     * This method sets a callback that will be invoked when there are any changes on the MeshCoP service from
     * Thread core.
     *
     * @param[in] aCallback  The callback function.
     */
    virtual void SetBorderAgentMeshCoPServiceChangedCallback(BorderAgentMeshCoPServiceChangedCallback aCallback) = 0;

    /**
     * This method adds a callback that will be invoked when there are any changes related to the ephemeral key.
     *
     * @param[in] aCallback  The callback function.
     */
    virtual void AddEphemeralKeyStateChangedCallback(EphemeralKeyStateChangedCallback aCallback) = 0;

#if OTBR_ENABLE_BORDER_AGENT && !OTBR_ENABLE_BORDER_AGENT_MESHCOP_SERVICE
    /**
     * This method sets or updates the Border Agent Vendor TXT Data.
     *
     * This is then passed to OpenThread stack to be included in the advertised `meshcop` service TXT Data.
     *
     * @param[in] aVendorTxtData   The encoded vendor TXT Data.
     */
    virtual void SetBorderAgentVendorTxtData(const std::vector<uint8_t> &aVendorTxtData) = 0;
#endif

    /**
     * This methods a callback for the Thread stack to forward UDP packet to the host.
     *
     * @param[in] aCallback  The callback function.
     */
    virtual void SetUdpForwardToHostCallback(UdpForwardToHostCallback aCallback) = 0;

#if OTBR_ENABLE_BACKBONE_ROUTER
    /**
     * This method enables/disables the Backbone Router.
     *
     * @param[in] aEnabled  Whether to enable or disable the Backbone router.
     */
    virtual void SetBackboneRouterEnabled(bool aEnabled) = 0;

    /**
     * This method sets the Backbone Router Multicast Listener callback.
     *
     * @param[in] aCallback  The Multicast Listener callback.
     */
    virtual void SetBackboneRouterMulticastListenerCallback(BackboneRouterMulticastListenerCallback aCallback) = 0;

    /**
     * This method sets the Backbone Router state change callback.
     *
     * @param[in] aCallback  The Backbone Router state change callback.
     */
    virtual void SetBackboneRouterStateChangedCallback(BackboneRouterStateChangedCallback aCallback) = 0;
#endif // OTBR_ENABLE_BACKBONE_ROUTER

    /**
     * Returns the co-processor type.
     */
    virtual CoprocessorType GetCoprocessorType(void) = 0;

    /**
     * Returns the co-processor version string.
     */
    virtual const char *GetCoprocessorVersion(void) = 0;

    /**
     * This method returns the Thread network interface name.
     *
     * @returns A pointer to the Thread network interface name string.
     */
    virtual const char *GetInterfaceName(void) const = 0;

#ifndef OTBR_VENDOR_NAME
    /**
     * This method sets the vendor name.
     *
     * @param[in] aVendorName   The vendor name.
     *
     * @retval OT_ERROR_NONE             Successfully set the vendor name.
     * @retval OT_ERROR_INVALID_ARGS     @p aVendorName is not valid (too long or not UTF8).
     * @retval OT_ERROR_INVALID_STATE    otInstance is in invalid state.
     * @retval OT_ERROR_NOT_IMPLEMENTED  Not implemented.
     */
    virtual otError SetVendorName(const char *aVendorName) = 0;
#endif

#ifndef OTBR_PRODUCT_NAME
    /**
     * This method sets the vendor model.
     *
     * @param[in] aVendorModel   The vendor model.
     *
     * @retval OT_ERROR_NONE             Successfully set the vendor model.
     * @retval OT_ERROR_INVALID_ARGS     @p aVendorModel is not valid (too long or not UTF8).
     * @retval OT_ERROR_INVALID_STATE    otInstance is in invalid state.
     * @retval OT_ERROR_NOT_IMPLEMENTED  Not implemented.
     */
    virtual otError SetVendorModel(const char *aVendorModel) = 0;
#endif

    /**
     * Initializes the Thread controller.
     */
    virtual void Init(void) = 0;

    /**
     * Deinitializes the Thread controller.
     */
    virtual void Deinit(void) = 0;

    /**
     * Whether the Thread controller has been initialized.
     *
     * All the functional APIs MUST be called when the Thread controller is initialized.
     *
     * @retval true   The Thread controller has been initialized.
     * @retval false  The Thread controller hasn't been initialized.
     */
    virtual bool IsInitialized(void) const = 0;

    /**
     * The destructor.
     */
    virtual ~ThreadHost(void) = default;
};

} // namespace Host
} // namespace otbr

#endif // OTBR_AGENT_THREAD_HOST_HPP_
