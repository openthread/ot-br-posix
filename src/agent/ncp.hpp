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

namespace ot {

namespace BorderRouter {

namespace Ncp {

/**
 * @addtogroup border-agent-ncp
 *
 * @brief
 *   This module includes definition for NCP service.
 *
 * @{
 */

/**
 * This interface defines NCP Controller functionality.
 *
 */
class Controller
{
public:
    /**
     * This function pointer is called when a CoAP message ready for border agent.
     *
     * @param[in]   aBuffer     A pointer to packet data.
     * @param[in]   aLength     Number of bytes in this packet.
     * @param[in]   aLocator    Source locator of this packet.
     * @param[in]   aPort       Source port of this packet.
     * @param[in]   aContext    A pointer to application-specific context.
     *
     */
    typedef void (*PacketHandler)(const uint8_t *aBuffer, uint16_t aLength, uint16_t aLocator, uint16_t aPort,
                                  void *aContext);
    /**
     * This function pointer is called when PSKc is changed.
     *
     * @param[in]   aPSKc       A pointer to PSKc.
     * @param[in]   aContext    A pointer to application-specific context.
     *
     */
    typedef void (*PSKcHandler)(const uint8_t *aPSKc, void *aContext);

    /**
     * This method request the NCP to start the border agent proxy service.
     *
     * @returns 0 on success, otherwise failure.
     *
     */
    virtual int BorderAgentProxyStart(void) = 0;

    /**
     * This method request the NCP to stop the border agent proxy service.
     *
     * @returns 0 on success, otherwise failure.
     *
     */
    virtual int BorderAgentProxyStop(void) = 0;

    /**
     * This method sends a packet through border agent proxy service.
     *
     * @returns 0 on success, otherwise failure.
     *
     */
    virtual int BorderAgentProxySend(const uint8_t *aBuffer, uint16_t aLength, uint16_t aLocator, uint16_t aPort) = 0;

    /**
     * This method updates the fd_set to poll.
     *
     * @param[inout]    aReadFdSet      A reference to fd_set for polling read.
     * @param[inout]    aWriteFdSet     A reference to fd_set for polling read.
     * @param[inout]    aMaxFd          A reference to the current max fd in @p aReadFdSet and @p aWriteFdSet.
     *
     */
    virtual void UpdateFdSet(fd_set &aReadFdSet, fd_set &aWriteFdSet, fd_set &aErrorFdSet, int &aMaxFd) = 0;

    /**
     * This method performs the DTLS processing.
     *
     * @param[in]   aReadFdSet      A reference to fd_set ready for reading.
     * @param[in]   aWriteFdSet     A reference to fd_set ready for writing.
     */
    virtual void Process(const fd_set &aReadFdSet, const fd_set &aWriteFdSet, const fd_set &aErrorFdSet) = 0;

    /**
     * This method retrieves the current PSKc.
     *
     * @returns The current PSKc.
     *
     */
    virtual const uint8_t *GetPSKc(void) = 0;

    /**
     * This method retrieves the Eui64.
     *
     * @returns The hardware address.
     *
     */
    virtual const uint8_t *GetEui64(void) = 0;

    /**
     * This method creates a NCP Controller.
     *
     * @param[in]   aInterfaceName  A string of the NCP interface.
     * @param[in]   aPSKcHandler    A pointer to the function that receives the PSKc.
     * @param[in]   aPacketHandler  A pointer to the function that handles the packet.
     * @param[in]   aContext    A pointer to application-specific context.
     *
     */
    static Controller *Create(const char *aInterfaceName, PSKcHandler aPSKcHandler, PacketHandler aPacketHandler,
                              void *aContext);

    /**
     * This method destroys a NCP Controller.
     *
     * @param[in]   aController     A pointer to the NCP contorller.
     *
     */
    static void Destroy(Controller *aController);

    virtual ~Controller(void) {}
};

} // Ncp

} // namespace BorderRouter

} // namespace ot

#endif  // NCP_HPP_
