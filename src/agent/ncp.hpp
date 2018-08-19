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

#ifndef NCP_HPP_
#define NCP_HPP_

#include <netinet/in.h>

#include "common/event_emitter.hpp"
#include "common/types.hpp"

namespace ot {

namespace BorderRouter {

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
    kEventExtPanId,       ///< Extended PAN ID arrived.
    kEventNetworkName,    ///< Network name arrived.
    kEventPSKc,           ///< PSKc arrived.
    kEventThreadState,    ///< Thread State.
    kEventUdpProxyStream, ///< UDP proxy stream arrived.
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
     * This method sends a packet through UDP proxy service.
     *
     * @retval  OTBR_ERROR_NONE         Successfully sent the packet.
     * @retval  OTBR_ERROR_ERRNO        Failed to send the packet.
     *
     */
    virtual otbrError UdpProxySend(const uint8_t * aBuffer,
                                   uint16_t        aLength,
                                   uint16_t        aPeerPort,
                                   const in6_addr &aPeerAddr,
                                   uint16_t        aSockPort) = 0;

    /**
     * This method updates the fd_set to poll.
     *
     * @param[inout]    aReadFdSet      A reference to fd_set for polling read.
     * @param[inout]    aWriteFdSet     A reference to fd_set for polling read.
     * @param[inout]    aErrorFdSet     A reference to fd_set for polling error.
     * @param[inout]    aMaxFd          A reference to the current max fd.
     *
     */
    virtual void UpdateFdSet(fd_set &aReadFdSet, fd_set &aWriteFdSet, fd_set &aErrorFdSet, int &aMaxFd) = 0;

    /**
     * This method performs the DTLS processing.
     *
     * @param[in]   aReadFdSet          A reference to fd_set ready for reading.
     * @param[in]   aWriteFdSet         A reference to fd_set ready for writing.
     * @param[in]   aErrorFdSet         A reference to fd_set with error occurred.
     *
     */
    virtual void Process(const fd_set &aReadFdSet, const fd_set &aWriteFdSet, const fd_set &aErrorFdSet) = 0;

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
     * @param[in]   aInterfaceName  A string of the NCP interface.
     *
     */
    static Controller *Create(const char *aInterfaceName);

    /**
     * This method destroys a NCP Controller.
     *
     * @param[in]   aController     A pointer to the NCP contorller.
     *
     */
    static void Destroy(Controller *aController);

    virtual ~Controller(void) {}
};

} // namespace Ncp

} // namespace BorderRouter

} // namespace ot

#endif // NCP_HPP_
