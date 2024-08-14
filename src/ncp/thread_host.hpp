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
     *
     */
    virtual otDeviceRole GetDeviceRole(void) const = 0;

    /**
     * The destructor.
     *
     */
    virtual ~NetworkProperties(void) = default;
};

/**
 * This class is an interface which provides a set of async APIs to control the
 * Thread network.
 *
 * The APIs are unified for both NCP and RCP cases.
 *
 */
class ThreadHost : virtual public NetworkProperties
{
public:
    using AsyncResultReceiver = std::function<void(otError, const std::string &)>;
    using DeviceRoleHandler   = std::function<void(otError, otDeviceRole)>;

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
     *
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
     *
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
     * 3. Then Operational Dataset will be removed from persistent storage.
     * 4. If everything goes fine, @p aReceiver will be invoked with OT_ERROR_NONE. Otherwise, other errors
     *    will be passed to @p aReceiver when the error happens.
     *
     * @param[in] aReceiver  A receiver to get the async result of this operation.
     *
     */
    virtual void Leave(const AsyncResultReceiver &aRecevier) = 0;

    /**
     * This method migrates this device to the new network specified by @p aPendingOpDatasetTlvs.
     *
     * @param[in] aPendingOpDatasetTlvs  A reference to the pending operational dataset of the Thread network.
     * @param[in] aReceiver              A receiver to get the async result of this operation.
     *
     */
    virtual void ScheduleMigration(const otOperationalDatasetTlvs &aPendingOpDatasetTlvs,
                                   const AsyncResultReceiver       aReceiver) = 0;

    /**
     * Returns the co-processor type.
     *
     */
    virtual CoprocessorType GetCoprocessorType(void) = 0;

    /**
     * Returns the co-processor version string.
     *
     */
    virtual const char *GetCoprocessorVersion(void) = 0;

    /**
     * This method returns the Thread network interface name.
     *
     * @returns A pointer to the Thread network interface name string.
     *
     */
    virtual const char *GetInterfaceName(void) const = 0;

    /**
     * Initializes the Thread controller.
     *
     */
    virtual void Init(void) = 0;

    /**
     * Deinitializes the Thread controller.
     *
     */
    virtual void Deinit(void) = 0;

    /**
     * The destructor.
     *
     */
    virtual ~ThreadHost(void) = default;

protected:
    static otLogLevel ConvertToOtLogLevel(otbrLogLevel aLevel);
};

} // namespace Ncp
} // namespace otbr

#endif // OTBR_AGENT_THREAD_HOST_HPP_
