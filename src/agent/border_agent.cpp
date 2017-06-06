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
 *   The file implements the Thread border agent interface.
 */

#include "border_agent.hpp"

#include <stdexcept>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <syslog.h>

#include "border_agent.hpp"
#include "common/types.hpp"
#include "common/tlv.hpp"
#include "common/code_utils.hpp"
#include "dtls.hpp"
#include "ncp.hpp"
#include "uris.hpp"

namespace ot {

namespace BorderAgent {

/**
 * Locators
 *
 */
enum
{
    kAloc16Leader   = 0xfc00, ///< leader anycast locator.
    kInvalidLocator = 0xffff, ///< invalid locator.
};

/**
 * UDP ports
 *
 */
enum
{
    kCoapUdpPort        = 61631, ///< Thread management UDP port.
    kBorderAgentUdpPort = 49191, ///< Thread commissioning port.
};

/**
 * TLV types
 *
 */
enum
{
    kJoinerRouterLocator = 20, ///< meshcop Joiner Router Locator TLV
};


const Coap::Resource BorderAgent::kCoapResources[] =
{
    { OPENTHREAD_URI_RELAY_RX, BorderAgent::HandleRelayReceive },
    {NULL, NULL}
};

const Coap::Resource BorderAgent::kCoapsResources[] =
{
    { OPENTHREAD_URI_COMMISSIONER_PETITION, BorderAgent::ForwardCommissionerRequest },
    { OPENTHREAD_URI_COMMISSIONER_KEEP_ALIVE, BorderAgent::ForwardCommissionerRequest },
    { OPENTHREAD_URI_COMMISSIONER_SET, BorderAgent::ForwardCommissionerRequest },
    { OPENTHREAD_URI_RELAY_TX, BorderAgent::HandleRelayTransmit },
    {NULL, NULL},
};

void BorderAgent::ForwardCommissionerResponse(const Coap::Message &aMessage)
{
    uint8_t        tokenLength = 0;
    const uint8_t *token = aMessage.GetToken(tokenLength);

    uint16_t       length = 0;
    const uint8_t *payload = NULL;

    Coap::Message::Code code = aMessage.GetCode();

    Coap::Message *message = mCoaps->NewMessage(
        Coap::Message::kCoapTypeNonConfirmable, code,
        token, tokenLength);

    payload = aMessage.GetPayload(length);
    message->SetPayload(payload, length);

    mCoaps->Send(*message, NULL, 0, NULL);
    mCoaps->FreeMessage(message);
}

void BorderAgent::ForwardCommissionerRequest(const Coap::Resource &aResource, const Coap::Message &aMessage,
                                             const uint8_t *aIp6, uint16_t aPort)
{
    uint8_t        tokenLength = 0;
    const uint8_t *token = aMessage.GetToken(tokenLength);

    Coap::Message *message = mCoap->NewMessage(Coap::Message::kCoapTypeConfirmable, Coap::Message::kCoapRequestPost,
                                               token, tokenLength);

    const char *path = aResource.mPath;

    syslog(LOG_INFO, "forwarding request %s", path);

    if (!strcmp(OPENTHREAD_URI_COMMISSIONER_PETITION, path))
    {
        path = OPENTHREAD_URI_LEADER_PETITION;
    }
    else if (!strcmp(OPENTHREAD_URI_COMMISSIONER_KEEP_ALIVE, path))
    {
        path = OPENTHREAD_URI_LEADER_KEEP_ALIVE;
    }

    message->SetPath(path);

    uint16_t       length = 0;
    const uint8_t *payload = aMessage.GetPayload(length);
    message->SetPayload(payload, length);

    Ip6Address addr(kAloc16Leader);

    mCoap->Send(*message, addr.m8, kCoapUdpPort, BorderAgent::ForwardCommissionerResponse);
    mCoap->FreeMessage(message);
}

void BorderAgent::HandleRelayReceive(const Coap::Message &aMessage, const uint8_t *aIp6, uint16_t aPort)
{
    uint8_t        tokenLength = 0;
    const uint8_t *token = aMessage.GetToken(tokenLength);

    Coap::Message *message = mCoaps->NewMessage(Coap::Message::kCoapTypeNonConfirmable, Coap::Message::kCoapRequestPost,
                                                token, tokenLength);
    message->SetPath(OPENTHREAD_URI_RELAY_RX);

    uint16_t       length = 0;
    const uint8_t *payload = aMessage.GetPayload(length);
    message->SetPayload(payload, length);

    mCoaps->Send(*message, NULL, 0, NULL);
    mCoaps->FreeMessage(message);
}

void BorderAgent::HandleRelayTransmit(const Coap::Message &aMessage, const uint8_t *aIp6, uint16_t aPort)
{
    uint16_t       length = 0;
    const uint8_t *payload = aMessage.GetPayload(length);
    uint16_t       rloc = kInvalidLocator;

    for (const Tlv *tlv = reinterpret_cast<const Tlv *>(payload); tlv < reinterpret_cast<const Tlv *>(payload + length);
         tlv = tlv->GetNext())
    {
        if (tlv->GetType() == kJoinerRouterLocator)
        {
            rloc = tlv->GetValueUInt16();
            break;
        }
    }

    if (rloc == kInvalidLocator)
    {
        syslog(LOG_ERR, "joiner rloc not found");
        ExitNow();
    }

    {
        Ip6Address     addr(rloc);
        uint8_t        tokenLength = 0;
        const uint8_t *token = aMessage.GetToken(tokenLength);
        Coap::Message *message = mCoap->NewMessage(Coap::Message::kCoapTypeNonConfirmable,
                                                   Coap::Message::kCoapRequestPost,
                                                   token, tokenLength);

        message->SetPath(OPENTHREAD_URI_RELAY_TX);
        message->SetPayload(payload, length);
        mCoap->Send(*message, addr.m8, kCoapUdpPort, NULL);
        mCoap->FreeMessage(message);
    }

exit:
    return;
}

BorderAgent::BorderAgent(const char *aInterfaceName) :
    mNcpController(Ncp::Controller::Create(aInterfaceName, HandlePSKcChanged, FeedCoap, this)),
    mCoap(Coap::Agent::Create(SendCoap, kCoapResources, this)),
    mCoaps(Coap::Agent::Create(SendCoaps, kCoapsResources, this)),
    mDtlsServer(Dtls::Server::Create(kBorderAgentUdpPort, HandleDtlsSessionState, this))
{
    int error = 0;

    error = mNcpController->BorderAgentProxyStart();

    if (error)
    {
        syslog(LOG_ERR, "failed to enable border agent proxy");
        throw std::runtime_error("Failed to start border agent proxy");
    }

    const uint8_t *pskc = mNcpController->GetPSKc();
    if (pskc == NULL)
    {
        syslog(LOG_ERR, "failed to get PSKc");
        throw std::runtime_error("Failed to get PSKc");
    }
    mDtlsServer->SetPSK(pskc, kSizePSKc);

    const uint8_t *eui64 = mNcpController->GetEui64();
    if (eui64 == NULL)
    {
        syslog(LOG_ERR, "failed to get Eui64");
        throw std::runtime_error("Failed to get Eui64");
    }
    mDtlsServer->SetSeed(eui64, kSizeEui64);
}

BorderAgent::~BorderAgent(void)
{
    Dtls::Server::Destroy(mDtlsServer);
    Coap::Agent::Destroy(mCoaps);
    Coap::Agent::Destroy(mCoap);
    Ncp::Controller::Destroy(mNcpController);
}

void BorderAgent::HandleDtlsSessionState(Dtls::Session &aSession, Dtls::Session::State aState)
{
    switch (aState)
    {
    case Dtls::Session::kStateReady:
        aSession.SetDataHandler(FeedCoaps, this);
        mDtlsSession = &aSession;
        break;

    case Dtls::Session::kStateEnd:
    case Dtls::Session::kStateError:
    case Dtls::Session::kStateExpired:
        mDtlsSession = NULL;
        syslog(LOG_WARNING, "Dtls session ended");
        break;

    default:
        break;
    }
}

ssize_t BorderAgent::SendCoap(const uint8_t *aBuffer, uint16_t aLength, const uint8_t *aIp6, uint16_t aPort,
                              void *aContext)
{
    return static_cast<BorderAgent *>(aContext)->SendCoap(aBuffer, aLength, aIp6, aPort);
}

ssize_t BorderAgent::SendCoap(const uint8_t *aBuffer, uint16_t aLength, const uint8_t *aIp6, uint16_t aPort)
{
    const Ip6Address *addr = reinterpret_cast<const Ip6Address *>(aIp6);
    uint16_t          rloc = addr->ToLocator();

    mNcpController->BorderAgentProxySend(aBuffer, aLength, rloc, aPort);
    return aLength;
}

ssize_t BorderAgent::SendCoaps(const uint8_t *aBuffer, uint16_t aLength, const uint8_t *aIp6, uint16_t aPort,
                               void *aContext)
{
    // TODO verify the ip and port
    (void)aIp6;
    (void)aPort;
    return static_cast<BorderAgent *>(aContext)->mDtlsSession->Write(aBuffer, aLength);
}

void BorderAgent::FeedCoap(const uint8_t *aBuffer, uint16_t aLength, uint16_t aLocator, uint16_t aPort, void *aContext)
{
    BorderAgent *borderAgent = static_cast<BorderAgent *>(aContext);
    Ip6Address   addr(aLocator);

    borderAgent->mCoap->Input(aBuffer, aLength, addr.m8, aPort);
}

void BorderAgent::FeedCoaps(const uint8_t *aBuffer, uint16_t aLength, void *aContext)
{
    BorderAgent *borderAgent = static_cast<BorderAgent *>(aContext);

    borderAgent->mCoaps->Input(aBuffer, aLength, NULL, 0);
}

void BorderAgent::UpdateFdSet(fd_set &aReadFdSet, fd_set &aWriteFdSet, fd_set &aErrorFdSet, int &aMaxFd,
                              timeval &aTimeout)
{
    mNcpController->UpdateFdSet(aReadFdSet, aWriteFdSet, aErrorFdSet, aMaxFd);
    mDtlsServer->UpdateFdSet(aReadFdSet, aWriteFdSet, aMaxFd, aTimeout);
}

void BorderAgent::Process(const fd_set &aReadFdSet, const fd_set &aWriteFdSet, const fd_set &aErrorFdSet)
{
    mNcpController->Process(aReadFdSet, aWriteFdSet, aErrorFdSet);
    mDtlsServer->Process(aReadFdSet, aWriteFdSet);
}

void BorderAgent::HandlePSKcChanged(const uint8_t *aPSKc, void *aContext)
{
    static_cast<BorderAgent *>(aContext)->mDtlsServer->SetPSK(aPSKc, kSizePSKc);
}

} // namespace BorderAgent

} // namespace ot
