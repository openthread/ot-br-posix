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

#include "coap.hpp"
#include "dtls.hpp"
#include "mdns.hpp"
#include "ncp.hpp"

namespace ot {

namespace BorderRouter {

/**
 * @addtogroup border-router-border-agent
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
     *
     * @param[in]   aNcp            A pointer to the NCP controller.
     * @param[in]   aCoap           A pointer to the TMF agent.
     *
     */
    BorderAgent(Ncp::Controller *aNcp, Coap::Agent *aCoap);

    ~BorderAgent(void);

    /**
     * This method starts border agent service.
     *
     * @retval  OTBR_ERROR_NONE     Successfully started border agent.
     * @retval  OTBR_ERROR_ERRNO    Failed to start border agent.
     * @retval  OTBR_ERROR_DTLS     Failed to start border agent for DTLS error.
     *
     */
    otbrError Start(void);

    /**
     * This method updates the fd_set and timeout for mainloop.
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
     * This method performs border agent processing.
     *
     * @param[in]   aReadFdSet   A reference to read file descriptors.
     * @param[in]   aWriteFdSet  A reference to write file descriptors.
     * @param[in]   aErrorFdSet  A reference to error file descriptors.
     *
     */
    void Process(const fd_set &aReadFdSet, const fd_set &aWriteFdSet, const fd_set &aErrorFdSet);

private:
    static void    FeedCoaps(const uint8_t *aBuffer, uint16_t aLength, void *aContext);
    static ssize_t SendCoaps(const uint8_t *aBuffer,
                             uint16_t       aLength,
                             const uint8_t *aIp6,
                             uint16_t       aPort,
                             void *         aContext);

    static void HandleDtlsSessionState(Dtls::Session &aSession, Dtls::Session::State aState, void *aContext)
    {
        static_cast<BorderAgent *>(aContext)->HandleDtlsSessionState(aSession, aState);
    }
    void HandleDtlsSessionState(Dtls::Session &aSession, Dtls::Session::State aState);

    static void HandleRelayReceive(const Coap::Resource &aResource,
                                   const Coap::Message & aMessage,
                                   Coap::Message &       aResponse,
                                   const uint8_t *       aIp6,
                                   uint16_t              aPort,
                                   void *                aContext)
    {
        (void)aResource;
        (void)aResponse;
        static_cast<BorderAgent *>(aContext)->HandleRelayReceive(aMessage, aIp6, aPort);
    }
    void HandleRelayReceive(const Coap::Message &aMessage, const uint8_t *aIp6, uint16_t aPort);

    static void HandleRelayTransmit(const Coap::Resource &aResource,
                                    const Coap::Message & aMessage,
                                    Coap::Message &       aResponse,
                                    const uint8_t *       aIp6,
                                    uint16_t              aPort,
                                    void *                aContext)
    {
        (void)aResource;
        (void)aResponse;
        static_cast<BorderAgent *>(aContext)->HandleRelayTransmit(aMessage, aIp6, aPort);
    }
    void HandleRelayTransmit(const Coap::Message &aMessage, const uint8_t *aIp6, uint16_t aPort);

    static void ForwardCommissionerRequest(const Coap::Resource &aResource,
                                           const Coap::Message & aMessage,
                                           Coap::Message &       aResponse,
                                           const uint8_t *       aIp6,
                                           uint16_t              aPort,
                                           void *                aContext)
    {
        (void)aIp6;
        (void)aResponse;
        static_cast<BorderAgent *>(aContext)->ForwardCommissionerRequest(aResource, aMessage, aIp6, aPort);
    }
    void ForwardCommissionerRequest(const Coap::Resource &aResource,
                                    const Coap::Message & aMessage,
                                    const uint8_t *       aIp6,
                                    uint16_t              aPort);

    static void ForwardCommissionerResponse(const Coap::Message &aMessage, void *aContext)
    {
        static_cast<BorderAgent *>(aContext)->ForwardCommissionerResponse(aMessage);
    }
    void ForwardCommissionerResponse(const Coap::Message &aMessage);

    static void HandleMdnsState(void *aContext, Mdns::State aState)
    {
        static_cast<BorderAgent *>(aContext)->HandleMdnsState(aState);
    }
    void HandleMdnsState(Mdns::State aState);

    void PublishService(void);
    void StartPublishService(void);
    void StopPublishService(void);
    void HandleThreadChange(void);

    void SetNetworkName(const char *aNetworkName);
    void SetExtPanId(const uint8_t *aExtPanId);
    void SetThreadStarted(bool aStarted);

    static void HandlePSKcChanged(void *aContext, int aEvent, va_list aArguments);
    static void HandleThreadState(void *aContext, int aEvent, va_list aArguments);
    static void HandleNetworkName(void *aContext, int aEvent, va_list aArguments);
    static void HandleExtPanId(void *aContext, int aEvent, va_list aArguments);

    Coap::Resource mActiveGet;
    Coap::Resource mActiveSet;
    Coap::Resource mPendingGet;
    Coap::Resource mPendingSet;

    // Border agent resources for external commissioner.
    Coap::Resource mCommissionerPetitionHandler;
    Coap::Resource mCommissionerKeepAliveHandler;
    Coap::Resource mCommissionerSetHandler;
    Coap::Resource mCommissionerRelayTransmitHandler;

    // Border agent resources for Thread network.
    Coap::Resource mCommissionerRelayReceiveHandler;

    Coap::Agent *    mCoap;
    Dtls::Server *   mDtlsServer;
    Dtls::Session *  mDtlsSession;
    Coap::Agent *    mCoaps;
    Mdns::Publisher *mPublisher;
    Ncp::Controller *mNcp;

    uint8_t mExtPanId[kSizeExtPanId];
    char    mNetworkName[kSizeNetworkName + 1];
    bool    mThreadStarted;
};

/**
 * @}
 */

} // namespace BorderRouter

} // namespace ot

#endif // BORDER_AGENT_HPP_
