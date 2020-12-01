/*
 *  Copyright (c) 2017, The OpenThread Authors.
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
 *   This file includes definitions for NCP service.
 */

#ifndef OTBR_AGENT_NCP_HPP_
#define OTBR_AGENT_NCP_HPP_

#include "openthread-br/config.h"

#include <netinet/in.h>
#include <stddef.h>

#include "common/mainloop.h"
#include "common/types.hpp"
#include "utils/event_emitter.hpp"

namespace otbr {

namespace Ncp {

/**
 * @addtogroup border-router-ncp
 *
 * @brief
 *   This module includes definition for NCP service.
 *
 * @{
 */

/**
 * NCP Events definition according to spinel protocol.
 *
 */
enum
{
    kEventExtPanId,                        ///< Extended PAN ID arrived.
    kEventNetworkName,                     ///< Network name arrived.
    kEventPSKc,                            ///< PSKc arrived.
    kEventThreadState,                     ///< Thread State.
    kEventThreadVersion,                   ///< Thread Version.
    kEventUdpForwardStream,                ///< UDP forward stream arrived.
    kEventBackboneRouterState,             ///< Backbone Router State.
    kEventBackboneRouterDomainPrefixEvent, ///< Backbone Router Domain Prefix event.
    kEventBackboneRouterNdProxyEvent,      ///< Backbone Router ND Proxy event arrived.
};

/**
 * This interface defines NCP Controller functionality.
 *
 */
class Controller : public EventEmitter
{
public:
    /**
     * This method initalize the NCP controller.
     *
     * @retval  OTBR_ERROR_NONE     Successfully initialized NCP controller.
     * @retval  OTBR_ERROR_DBUS     Failed due to dbus error.
     *
     */
    virtual otbrError Init(void) = 0;

    /**
     * This method updates the fd_set to poll.
     *
     * @param[inout]    aMainloop   A reference to OpenThread mainloop context.
     *
     */
    virtual void UpdateFdSet(otSysMainloopContext &aMainloop) = 0;

    /**
     * This method performs the Thread processing.
     *
     * @param[in]       aMainloop   A reference to OpenThread mainloop context.
     *
     */
    virtual void Process(const otSysMainloopContext &aMainloop) = 0;

    /**
     * This method reest the Ncp Controller.
     *
     */
    virtual void Reset(void) = 0;

    /**
     * This method return whether reset is requested.
     *
     * @retval  TRUE  reset is requested.
     * @retval  FALSE reset isn't requested.
     *
     */
    virtual bool IsResetRequested(void) = 0;

    /**
     * This method request the event.
     *
     * @param[in]   aEvent  The event id to request.
     *
     * @retval  OTBR_ERROR_NONE         Successfully requested the event.
     * @retval  OTBR_ERROR_ERRNO        Failed to request the event.
     *
     */
    virtual otbrError RequestEvent(int aEvent) = 0;

    /**
     * This method creates a NCP Controller.
     *
     * @param[in]   aInterfaceName          A string of the NCP interface name.
     * @param[in]   aRadioUrl               The URL describes the radio chip.
     * @param[in]   aBackboneInterfaceName  The Backbone network interface name.
     *
     */
    static Controller *Create(const char *aInterfaceName,
                              const char *aRadioUrl,
                              const char *aBackboneInterfaceName = nullptr);

    /**
     * This method destroys a NCP Controller.
     *
     * @param[in]   aController     A pointer to the NCP contorller.
     *
     */
    static void Destroy(Controller *aController) { delete aController; }

    virtual ~Controller(void) {}
};

} // namespace Ncp

} // namespace otbr

#endif // OTBR_AGENT_NCP_HPP_
