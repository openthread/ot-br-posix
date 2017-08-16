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
 *   The file implements the Thread border agent.
 */

#include "border_agent.hpp"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "border_agent.hpp"
#include "common/code_utils.hpp"
#include "common/logging.hpp"
#include "common/types.hpp"
#include "common/tlv.hpp"
#include "dtls.hpp"
#include "ncp.hpp"
#include "uris.hpp"

namespace ot {

namespace BorderRouter {

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

void BorderAgent::ForwardCommissionerResponse(const Coap::Message &aMessage)
{
    uint8_t        tokenLength = 0;
    const uint8_t *token = aMessage.GetToken(tokenLength);
    uint16_t       length = 0;
    const uint8_t *payload = NULL;

    Coap::Code     code = aMessage.GetCode();
    Coap::Message *message = mCoaps->NewMessage(Coap::kTypeNonConfirmable, code, token, tokenLength);

    payload = aMessage.GetPayload(length);
    message->SetPayload(payload, length);

    mCoaps->Send(*message, NULL, 0, NULL, NULL);
    mCoaps->FreeMessage(message);
}

void BorderAgent::ForwardCommissionerRequest(const Coap::Resource &aResource, const Coap::Message &aMessage,
                                             const uint8_t *aIp6, uint16_t aPort)
{
    uint8_t        tokenLength = 0;
    const uint8_t *token = aMessage.GetToken(tokenLength);
    const char    *path = aResource.mPath;

    Coap::Message *message = mCoap->NewMessage(Coap::kTypeConfirmable, Coap::kCodePost, aMessage.GetMessageId(),
                                               token, tokenLength);
    Ip6Address     addr(kAloc16Leader);
    uint16_t       length = 0;
    const uint8_t *payload = aMessage.GetPayload(length);



    otbrLog(OTBR_LOG_INFO, "Forwarding request %s...", path);

    if (!strcmp(OT_URI_PATH_COMMISSIONER_PETITION, path))
    {
        path = OT_URI_PATH_LEADER_PETITION;
    }
    else if (!strcmp(OT_URI_PATH_COMMISSIONER_KEEP_ALIVE, path))
    {
        path = OT_URI_PATH_LEADER_KEEP_ALIVE;
    }

    message->SetPath(path);

    message->SetPayload(payload, length);

    otbrDump(OTBR_LOG_DEBUG, "    Payload:", payload, length);

    mCoap->Send(*message, addr.m8, kCoapUdpPort, BorderAgent::ForwardCommissionerResponse, this);
    mCoap->FreeMessage(message);

    (void)aIp6;
    (void)aPort;
}

void BorderAgent::HandleRelayReceive(const Coap::Message &aMessage, const uint8_t *aIp6, uint16_t aPort)
{
    uint8_t        tokenLength = 0;
    const uint8_t *token = aMessage.GetToken(tokenLength);
    uint16_t       length = 0;
    const uint8_t *payload = aMessage.GetPayload(length);

    Coap::Message *message = mCoaps->NewMessage(Coap::kTypeNonConfirmable, Coap::kCodePost,
                                                token, tokenLength);
    message->SetPath(OT_URI_PATH_RELAY_RX);
    message->SetPayload(payload, length);

    mCoaps->Send(*message, NULL, 0, NULL, NULL);
    mCoaps->FreeMessage(message);

    (void)aIp6;
    (void)aPort;
}

void BorderAgent::HandleRelayTransmit(const Coap::Message &aMessage, const uint8_t *aIp6, uint16_t aPort)
{
    uint16_t       length = 0;
    const uint8_t *payload = aMessage.GetPayload(length);
    uint16_t       rloc = kInvalidLocator;

    otbrDump(OTBR_LOG_DEBUG, "Relay transmit:", payload, length);

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
        otbrLog(OTBR_LOG_ERR, "Joiner Router Locator not found!");
        ExitNow();
    }

    {
        Ip6Address     addr(rloc);
        uint8_t        tokenLength = 0;
        const uint8_t *token = aMessage.GetToken(tokenLength);
        Coap::Message *message = mCoap->NewMessage(Coap::kTypeNonConfirmable, Coap::kCodePost,
                                                   aMessage.GetMessageId(), token, tokenLength);

        message->SetPath(OT_URI_PATH_RELAY_TX);
        message->SetPayload(payload, length);
        mCoap->Send(*message, addr.m8, kCoapUdpPort, NULL, NULL);
        mCoap->FreeMessage(message);
    }

exit:

    (void)aIp6;
    (void)aPort;

    return;
}

BorderAgent::BorderAgent(Ncp::Controller *aNcp, Coap::Agent *aCoap) :
    mCommissionerPetitionHandler(OT_URI_PATH_COMMISSIONER_PETITION, ForwardCommissionerRequest, this),
    mCommissionerKeepAliveHandler(OT_URI_PATH_COMMISSIONER_KEEP_ALIVE, ForwardCommissionerRequest, this),
    mCommissionerSetHandler(OT_URI_PATH_COMMISSIONER_SET, ForwardCommissionerRequest, this),
    mCommissionerRelayTransmitHandler(OT_URI_PATH_RELAY_TX, HandleRelayTransmit, this),
    mCommissionerRelayReceiveHandler(OT_URI_PATH_RELAY_RX, BorderAgent::HandleRelayReceive, this),
    mCoap(aCoap),
    mDtlsServer(Dtls::Server::Create(kBorderAgentUdpPort, HandleDtlsSessionState, this)),
    mCoaps(Coap::Agent::Create(SendCoaps, this)),
    mNcp(aNcp) {}

otbrError BorderAgent::Start(void)
{
    otbrError error = OTBR_ERROR_NONE;

    SuccessOrExit(error = mCoaps->AddResource(mCommissionerPetitionHandler));
    SuccessOrExit(error = mCoaps->AddResource(mCommissionerKeepAliveHandler));
    SuccessOrExit(error = mCoaps->AddResource(mCommissionerSetHandler));
    SuccessOrExit(error = mCoaps->AddResource(mCommissionerRelayTransmitHandler));

    SuccessOrExit(error = mCoap->AddResource(mCommissionerRelayReceiveHandler));

    mNcp->On(Ncp::kEventPSKc, HandlePSKcChanged, this);

    {
        const uint8_t *pskc = mNcp->GetPSKc();
        VerifyOrExit(pskc != NULL, error = OTBR_ERROR_ERRNO);
        mDtlsServer->SetPSK(pskc, kSizePSKc);
    }

    {
        const uint8_t *eui64 = mNcp->GetEui64();
        VerifyOrExit(eui64 != NULL, error = OTBR_ERROR_ERRNO);
        mDtlsServer->SetSeed(eui64, kSizeEui64);
    }

    SuccessOrExit(error = mDtlsServer->Start());

exit:
    if (error != OTBR_ERROR_NONE)
    {
        otbrLog(OTBR_LOG_ERR, "Failed to start border agent: %d!", error);
    }

    return error;
}

BorderAgent::~BorderAgent(void)
{
    Dtls::Server::Destroy(mDtlsServer);
    Coap::Agent::Destroy(mCoaps);
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
        otbrLog(OTBR_LOG_WARNING, "DTLS session ended.");
        break;

    default:
        break;
    }
}

ssize_t BorderAgent::SendCoaps(const uint8_t *aBuffer, uint16_t aLength, const uint8_t *aIp6, uint16_t aPort,
                               void *aContext)
{
    // TODO verify the ip and port
    (void)aIp6;
    (void)aPort;
    return static_cast<BorderAgent *>(aContext)->mDtlsSession->Write(aBuffer, aLength);
}

void BorderAgent::FeedCoaps(const uint8_t *aBuffer, uint16_t aLength, void *aContext)
{
    BorderAgent *borderAgent = static_cast<BorderAgent *>(aContext);

    borderAgent->mCoaps->Input(aBuffer, aLength, NULL, 0);
}

void BorderAgent::UpdateFdSet(fd_set &aReadFdSet, fd_set &aWriteFdSet, fd_set &aErrorFdSet, int &aMaxFd,
                              timeval &aTimeout)
{
    mDtlsServer->UpdateFdSet(aReadFdSet, aWriteFdSet, aErrorFdSet, aMaxFd, aTimeout);
}

void BorderAgent::Process(const fd_set &aReadFdSet, const fd_set &aWriteFdSet, const fd_set &aErrorFdSet)
{
    mDtlsServer->Process(aReadFdSet, aWriteFdSet, aErrorFdSet);
}

void BorderAgent::HandlePSKcChanged(void *aContext, int aEvent, va_list aArguments)
{
    assert(aEvent == Ncp::kEventPSKc);

    uint8_t *pskc = va_arg(aArguments, uint8_t *);
    static_cast<BorderAgent *>(aContext)->mDtlsServer->SetPSK(pskc, kSizePSKc);
}

} // namespace BorderRouter

} // namespace ot
