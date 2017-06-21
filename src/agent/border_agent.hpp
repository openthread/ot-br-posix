/*
 *    Copyright (c) 2017, The OpenThread Authors.
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
 *   This file includes definition for Thread border agent.
 */

#ifndef BORDER_AGENT_HPP_
#define BORDER_AGENT_HPP_

#include <stdint.h>

#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>

#include "coap.hpp"
#include "dtls.hpp"
#include "ncp.hpp"

namespace ot {

namespace BorderRouter {

/**
 * @addtogroup border-agent
 *
 * @brief
 *   This module includes definition for Thread border agent
 *
 * @{
 */

/**
 * This class implements Thread border agent functionality.
 *
 */
class BorderAgent
{
public:
    /**
     * The constructor to initialize the Thread border agent.
     * @param[in]   aInterfaceName  interface name string.
     *
     */
    BorderAgent(const char *aInterfaceName);

    ~BorderAgent(void);

    /**
     * This method updates the file descriptor sets with file descriptors used by border agent.
     *
     * @param[inout]  aReadFdSet   A reference to read file descriptors.
     * @param[inout]  aWriteFdSet  A reference to write file descriptors.
     * @param[inout]  aErrorFdSet  A reference to error file descriptors.
     * @param[inout]  aMaxFd       A reference to the max file descriptor.
     * @param[inout]  aTimeout     A reference to timeout.
     *
     */
    void UpdateFdSet(fd_set &aReadFdSet, fd_set &aWriteFdSet, fd_set &aErrorFdSet, int &aMaxFd, timeval &aTimeout);

    /**
     * This method perform border agent processing.
     *
     * @param[in]   aReadFdSet   A reference to read file descriptors.
     * @param[in]   aWriteFdSet  A reference to write file descriptors.
     * @param[in]   aErrorFdSet  A reference to error file descriptors.
     *
     */
    void Process(const fd_set &aReadFdSet, const fd_set &aWriteFdSet, const fd_set &aErrorFdSet);

private:
    static void FeedCoap(void *aContext, int aEvent, va_list aArguments);
    static ssize_t SendCoap(const uint8_t *aBuffer, uint16_t aLength, const uint8_t *aIp6, uint16_t aPort,
                            void *aContext);
    ssize_t SendCoap(const uint8_t *aBuffer, uint16_t aLength, const uint8_t *aIp6, uint16_t aPort);

    static void FeedCoaps(const uint8_t *aBuffer, uint16_t aLength, void *aContext);
    static ssize_t SendCoaps(const uint8_t *aBuffer, uint16_t aLength, const uint8_t *aIp6, uint16_t aPort,
                             void *aContext);

    static void HandleDtlsSessionState(Dtls::Session &aSession, Dtls::Session::State aState, void *aContext)
    {
        static_cast<BorderAgent *>(aContext)->HandleDtlsSessionState(aSession, aState);
    }
    void HandleDtlsSessionState(Dtls::Session &aSession, Dtls::Session::State aState);

    static void HandleRelayReceive(const Coap::Resource &aResource, const Coap::Message &aMessage,
                                   Coap::Message &aResponse,
                                   const uint8_t *aIp6, uint16_t aPort, void *aContext)
    {
        (void)aResource;
        (void)aResponse;
        static_cast<BorderAgent *>(aContext)->HandleRelayReceive(aMessage, aIp6, aPort);
    }
    void HandleRelayReceive(const Coap::Message &aMessage, const uint8_t *aIp6, uint16_t aPort);

    static void HandleRelayTransmit(const Coap::Resource &aResource, const Coap::Message &aMessage,
                                    Coap::Message &aResponse,
                                    const uint8_t *aIp6, uint16_t aPort, void *aContext)
    {
        (void)aResource;
        (void)aResponse;
        static_cast<BorderAgent *>(aContext)->HandleRelayTransmit(aMessage, aIp6, aPort);
    }
    void HandleRelayTransmit(const Coap::Message &aMessage, const uint8_t *aIp6, uint16_t aPort);

    static void ForwardCommissionerRequest(const Coap::Resource &aResource, const Coap::Message &aMessage,
                                           Coap::Message &aResponse,
                                           const uint8_t *aIp6, uint16_t aPort, void *aContext)
    {
        (void)aIp6;
        (void)aResponse;
        static_cast<BorderAgent *>(aContext)->ForwardCommissionerRequest(aResource, aMessage, aIp6, aPort);
    }
    void ForwardCommissionerRequest(const Coap::Resource &aResource, const Coap::Message &aMessage,
                                    const uint8_t *aIp6, uint16_t aPort);

    static void ForwardCommissionerResponse(const Coap::Message &aMessage, void *aContext)
    {
        static_cast<BorderAgent *>(aContext)->ForwardCommissionerResponse(aMessage);
    }
    void ForwardCommissionerResponse(const Coap::Message &aMessage);

    static void HandlePSKcChanged(void *aContext, int aEvent, va_list aArguments);

    /**
     * Border agent resources for Thread network.
     *
     */
    static const Coap::Resource kCoapResources[];

    /**
     * Border agent resources for external commissioner.
     *
     */
    static const Coap::Resource kCoapsResources[];

    Ncp::Controller            *mNcpController;
    Coap::Agent                *mCoap;
    Coap::Agent                *mCoaps;
    Dtls::Server               *mDtlsServer;
    Dtls::Session              *mDtlsSession;
};

/**
 * @}
 */

} // namespace BorderRouter

} // namespace ot

#endif  //BORDER_AGENT_HPP_
