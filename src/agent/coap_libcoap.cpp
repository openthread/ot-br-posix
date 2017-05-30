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
 *   The file implements the CoAP service.
 */

#include "coap_libcoap.hpp"

#include <stdio.h>

#include <syslog.h>

#include "common/types.hpp"
#include "common/code_utils.hpp"

namespace ot {

namespace BorderAgent {

namespace Coap {

static void CoapAddressInit(coap_address_t &aAddress, const uint8_t *aIp6, uint16_t aPort)
{
    coap_address_init(&aAddress);
    aAddress.addr.sin6.sin6_family = AF_INET6;

    VerifyOrExit(aIp6 != NULL);

    memcpy(&aAddress.addr.sin6.sin6_addr, aIp6, sizeof(aAddress.addr.sin6.sin6_addr));
    aAddress.addr.sin6.sin6_port = htons(aPort);

exit:
    return;
}

MessageLibcoap::MessageLibcoap(Message::Type aType, Message::Code aCode, uint16_t aMessageId, const uint8_t *aToken,
                               uint8_t aTokenLength)
{
    coap_pdu_t *pdu = coap_new_pdu();

    pdu->hdr->type = aType;
    pdu->hdr->id = aMessageId;
    pdu->hdr->code = aCode;
    pdu->hdr->token_length = aTokenLength;

    if (aTokenLength > 0)
    {
        coap_add_token(pdu, aTokenLength, aToken);
    }

    mPdu = pdu;
}

const uint8_t *MessageLibcoap::GetToken(uint8_t &aLength) const
{
    aLength = mPdu->hdr->token_length;
    return mPdu->hdr->token;
}

Message::Code MessageLibcoap::GetCode(void) const
{
    return static_cast<Message::Code>(mPdu->hdr->code);
}

Message::Type MessageLibcoap::GetType(void) const
{
    return static_cast<Message::Type>(mPdu->hdr->type);
}

void MessageLibcoap::Free(void)
{
    if (mPdu)
    {
        coap_delete_pdu(mPdu);
        mPdu = NULL;
    }
}

void MessageLibcoap::SetPath(const char *aPath)
{
    uint8_t        options[kMaxOptionSize];
    const uint8_t *option = options;
    size_t         len = sizeof(options);
    int            res;

    res = coap_split_path(reinterpret_cast<const unsigned char *>(aPath), strlen(aPath), options, &len);

    while (res--)
    {
        coap_add_option(mPdu, COAP_OPTION_URI_PATH,
                        COAP_OPT_LENGTH(option),
                        COAP_OPT_VALUE(option));

        option += COAP_OPT_SIZE(option);
    }
}

void MessageLibcoap::SetPayload(const uint8_t *aPayload, uint16_t aLength)
{
    coap_add_data(mPdu, aLength, aPayload);
}

const uint8_t *MessageLibcoap::GetPayload(uint16_t &aLength) const
{
    uint8_t *payload = NULL;
    size_t   length = 0;

    coap_get_data(mPdu, &length, &payload);
    aLength = length;
    return payload;
}

Message *AgentLibcoap::NewMessage(Message::Type aType, Message::Code aCode, const uint8_t *aToken, uint8_t aTokenLength)
{
    uint16_t messageId = coap_new_message_id(&mCoap);

    return new MessageLibcoap(aType, aCode, messageId, aToken, aTokenLength);
}

void AgentLibcoap::FreeMessage(Message *aMessage)
{
    delete static_cast<Message *>(aMessage);
}

void AgentLibcoap::Send(Message &aMessage, const uint8_t *aIp6, uint16_t aPort, ResponseHandler aHandler)
{
    MessageLibcoap &message = static_cast<MessageLibcoap &>(aMessage);

    coap_tid_t  tid = COAP_INVALID_TID;
    coap_pdu_t *pdu = message.GetPdu();

    coap_address_t remote;

    CoapAddressInit(remote, aIp6, aPort);

    if (pdu->hdr->type == COAP_MESSAGE_CON)
    {
        tid = coap_send_confirmed(&mCoap, mCoap.endpoint, &remote, pdu);

        // There is no official way to provide handler for each message,
        // we have to embed the handler to its payload.
        if (pdu->length + sizeof(&aHandler) < pdu->max_size)
        {
            memcpy(pdu->data + pdu->length, &aHandler, sizeof(aHandler));
        }
        else
        {
            syslog(LOG_ERR, "no memory for callback");
        }
    }
    else
    {
        tid = coap_send(&mCoap, mCoap.endpoint, &remote, pdu);
    }

    if (tid == COAP_INVALID_TID || pdu->hdr->type != COAP_MESSAGE_CON)
    {
        message.Free();
    }
}

void AgentLibcoap::HandleRequest(coap_context_t *aCoap,
                                 struct coap_resource_t *aResource,
                                 const coap_endpoint_t *aEndPoint,
                                 coap_address_t *aAddress,
                                 coap_pdu_t *aRequest,
                                 str *aToken,
                                 coap_pdu_t *aResponse)
{
    AgentLibcoap *agent = (AgentLibcoap *)CONTAINING_RECORD(aCoap, AgentLibcoap, mCoap);

    for (const Resource *resource = agent->mResources; resource && resource->mPath; resource++)
    {
        if (!strncmp(reinterpret_cast<const char *>(aResource->uri.s), resource->mPath, aResource->uri.length))
        {
            MessageLibcoap message(aRequest);
            resource->mHandler(*resource, message, reinterpret_cast<const uint8_t *>(&aAddress->addr.sin6.sin6_addr),
                               ntohs(aAddress->addr.sin6.sin6_port), agent->mContext);
            aResponse->hdr->code = 0;
        }
    }
}

void AgentLibcoap::Input(const void *aBuffer, uint16_t aLength, const uint8_t *aIp6, uint16_t aPort)
{
    mPacket.length = aLength;
    mPacket.interface = mCoap.endpoint;
    CoapAddressInit(mPacket.src, aIp6, aPort);
    memcpy(mPacket.payload, aBuffer, aLength);
    coap_handle_message(&mCoap, &mPacket);
}

void AgentLibcoap::HandleResponse(coap_context_t *aCoap,
                                  const coap_endpoint_t *aLocalInterface,
                                  const coap_address_t *aRemote,
                                  coap_pdu_t *aSent,
                                  coap_pdu_t *aReceived,
                                  const coap_tid_t aId)
{
    AgentLibcoap *agent = (AgentLibcoap *)CONTAINING_RECORD(aCoap, AgentLibcoap, mCoap);

    VerifyOrExit(aSent != NULL, syslog(LOG_ERR, "request not found!"));

    {
        ResponseHandler handler;
        memcpy(&handler, aSent->data + aSent->length, sizeof(handler));

        MessageLibcoap message(aReceived);
        handler(message, agent->mContext);
    }

exit:

    return;
}

AgentLibcoap::AgentLibcoap(NetworkSender aNetworkSender, const Resource *aResources, void *aContext)
{
    mContext = aContext;
    mResources = aResources;
    mNetworkSender = aNetworkSender;
    coap_clock_init();

    time_t clock_offset = time(NULL);
    memset(&mCoap, 0, sizeof(mCoap));
    prng_init(reinterpret_cast<unsigned long>(aNetworkSender) ^ clock_offset);
    prng(reinterpret_cast<unsigned char *>(&mCoap.message_id), sizeof(unsigned short));

    coap_address_t addr;
    coap_address_init(&addr);
    addr.addr.sin6.sin6_family = AF_INET6;
    mCoap.endpoint = coap_new_endpoint(&addr, COAP_ENDPOINT_NOSEC);
    mCoap.network_send = AgentLibcoap::NetworkSend;

    for (const Resource *resource = aResources; resource && resource->mPath; resource++)
    {
        coap_resource_t *r = coap_resource_init(reinterpret_cast<const unsigned char *>(resource->mPath),
                                                strlen(resource->mPath), 0);
        coap_register_handler(r, COAP_REQUEST_POST, AgentLibcoap::HandleRequest);
        coap_add_resource(&mCoap, r);
    }

    coap_register_response_handler(&mCoap, AgentLibcoap::HandleResponse);
}

ssize_t AgentLibcoap::NetworkSend(coap_context_t *aCoap,
                                  const coap_endpoint_t *aLocalInterface,
                                  const coap_address_t *aDestination,
                                  unsigned char *aBuffer, size_t aLength)
{
    AgentLibcoap *agent = static_cast<AgentLibcoap *>(CONTAINING_RECORD(aCoap, AgentLibcoap, mCoap));

    return agent->mNetworkSender(aBuffer, aLength,
                                 reinterpret_cast<const uint8_t *>(&aDestination->addr.sin6.sin6_addr),
                                 ntohs(aDestination->addr.sin6.sin6_port), agent->mContext);
}

Agent *Agent::Create(NetworkSender aNetworkSender, const Resource *aResources, void *aContext)
{
    return new AgentLibcoap(aNetworkSender, aResources, aContext);
}

void Agent::Destroy(Agent *aAgent)
{
    delete static_cast<AgentLibcoap *>(aAgent);
}

} // namespace Coap

} // namespace BorderAgent

} // namespace ot
