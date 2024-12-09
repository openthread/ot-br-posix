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

#include <openthread/dataset.h>
#include <openthread/error.h>
#include <openthread/thread.h>

#include "lib/spinel/coprocessor_type.h"

#include "common/logging.hpp"

namespace otbr {
namespace Ncp {

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
 * Some cases in ThreadHost APIs cannot be denoted by otError. This enumeration is a superset of otErrors
 * and contains some error code specific to ThreadHost APIs.
 */
typedef int16_t Error;

// otErrors: 0 ~ 255
constexpr Error kErrorNone                       = OT_ERROR_NONE;
constexpr Error kErrorFailed                     = OT_ERROR_FAILED;
constexpr Error kErrorDrop                       = OT_ERROR_DROP;
constexpr Error kErrorNoBufs                     = OT_ERROR_NO_BUFS;
constexpr Error kErrorNoRoute                    = OT_ERROR_NO_ROUTE;
constexpr Error kErrorBusy                       = OT_ERROR_BUSY;
constexpr Error kErrorParse                      = OT_ERROR_PARSE;
constexpr Error kErrorInvalidArgs                = OT_ERROR_INVALID_ARGS;
constexpr Error kErrorSecurity                   = OT_ERROR_SECURITY;
constexpr Error kErrorAddressQuery               = OT_ERROR_ADDRESS_QUERY;
constexpr Error kErrorNoAddress                  = OT_ERROR_NO_ADDRESS;
constexpr Error kErrorAbort                      = OT_ERROR_ABORT;
constexpr Error kErrorNotImplemented             = OT_ERROR_NOT_IMPLEMENTED;
constexpr Error kErrorInvalidState               = OT_ERROR_INVALID_STATE;
constexpr Error kErrorNoAck                      = OT_ERROR_NO_ACK;
constexpr Error kErrorChannelAccessFailure       = OT_ERROR_CHANNEL_ACCESS_FAILURE;
constexpr Error kErrorDetached                   = OT_ERROR_DETACHED;
constexpr Error kErrorFcs                        = OT_ERROR_FCS;
constexpr Error kErrorNoFrameReceived            = OT_ERROR_NO_FRAME_RECEIVED;
constexpr Error kErrorUnknownNeighbor            = OT_ERROR_UNKNOWN_NEIGHBOR;
constexpr Error kErrorInvalidSourceAddress       = OT_ERROR_INVALID_SOURCE_ADDRESS;
constexpr Error kErrorAddressFiltered            = OT_ERROR_ADDRESS_FILTERED;
constexpr Error kErrorDestinationAddressFiltered = OT_ERROR_DESTINATION_ADDRESS_FILTERED;
constexpr Error kErrorNotFound                   = OT_ERROR_NOT_FOUND;
constexpr Error kErrorAlready                    = OT_ERROR_ALREADY;
constexpr Error kErrorIp6AddressCreationFailure  = OT_ERROR_IP6_ADDRESS_CREATION_FAILURE;
constexpr Error kErrorNotCapable                 = OT_ERROR_NOT_CAPABLE;
constexpr Error kErrorResponseTimeout            = OT_ERROR_RESPONSE_TIMEOUT;
constexpr Error kErrorDuplicated                 = OT_ERROR_DUPLICATED;
constexpr Error kErrorReassemblyTimeout          = OT_ERROR_REASSEMBLY_TIMEOUT;
constexpr Error kErrorNotTmf                     = OT_ERROR_NOT_TMF;
constexpr Error kErrorNotLowpanDataFrame         = OT_ERROR_NOT_LOWPAN_DATA_FRAME;
constexpr Error kErrorLinkMarginLow              = OT_ERROR_LINK_MARGIN_LOW;
constexpr Error kErrorInvalidCommand             = OT_ERROR_INVALID_COMMAND;
constexpr Error kErrorPending                    = OT_ERROR_PENDING;
constexpr Error kErrorRejected                   = OT_ERROR_REJECTED;
constexpr Error kErrorGeneric                    = OT_ERROR_GENERIC;

// Proprietary errors: -32768 ~ -1
constexpr Error kErrorUnsupportedChannel = -1; // The channel provided is not supported.
constexpr Error kErrorDisabled           = -2; // The action didn't succeed because Thread is in Disabled state.
constexpr Error kErrorFailedPrecondition = -3; // The action didn't succeed because some precondition isn't satisfied.

/**
 * This class is an interface which provides a set of async APIs to control the
 * Thread network.
 *
 * The APIs are unified for both NCP and RCP cases.
 */
class ThreadHost : virtual public NetworkProperties
{
public:
    using AsyncResultReceiver = std::function<void(Error, const std::string &)>;
    using ChannelMasksReceiver =
        std::function<void(uint32_t /*aSupportedChannelMask*/, uint32_t /*aPreferredChannelMask*/)>;
    using ThreadStateChangedCallback = std::function<void(otChangedFlags aFlags)>;
    using ThreadEnabledStateCallback = std::function<void(ThreadEnabledState aState)>;

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
     *
     * @returns Non-null OpenThread Controller instance.
     */
    static std::unique_ptr<ThreadHost> Create(const char                      *aInterfaceName,
                                              const std::vector<const char *> &aRadioUrls,
                                              const char                      *aBackboneInterfaceName,
                                              bool                             aDryRun,
                                              bool                             aEnableAutoAttach);

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

    /**
     * Initializes the Thread controller.
     */
    virtual void Init(void) = 0;

    /**
     * Deinitializes the Thread controller.
     */
    virtual void Deinit(void) = 0;

    /**
     * The destructor.
     */
    virtual ~ThreadHost(void) = default;
};

} // namespace Ncp
} // namespace otbr

#endif // OTBR_AGENT_THREAD_HOST_HPP_
