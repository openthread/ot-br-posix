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

#include <errno.h>
#include <stdio.h>

#include "code_utils.hpp"
#include "logging.hpp"
#include "types.hpp"

namespace ot {

namespace BorderRouter {

namespace Coap {

static void CoapAddressInit(coap_address_t &aAddress, const uint8_t *aIp6, uint16_t aPort)
{
    coap_address_init(&aAddress);
    aAddress.addr.sin6.sin6_family = AF_INET6;
    aAddress.addr.sin6.sin6_port   = htons(aPort);

    VerifyOrExit(aIp6 != NULL);
    memcpy(&aAddress.addr.sin6.sin6_addr, aIp6, sizeof(aAddress.addr.sin6.sin6_addr));

exit:
    return;
}

MessageLibcoap::MessageLibcoap(Type aType, Code aCode, uint16_t aMessageId, const uint8_t *aToken, uint8_t aTokenLength)
{
    mPdu          = coap_new_pdu();
    mPdu->hdr->id = aMessageId;
    SetType(aType);
    SetCode(aCode);
    SetToken(aToken, aTokenLength);
}

const uint8_t *MessageLibcoap::GetToken(uint8_t &aLength) const
{
    aLength = mPdu->hdr->token_length;
    return mPdu->hdr->token;
}

Code MessageLibcoap::GetCode(void) const
{
    return static_cast<Code>(mPdu->hdr->code);
}

void MessageLibcoap::SetCode(Code aCode)
{
    mPdu->hdr->code = aCode;
}

Type MessageLibcoap::GetType(void) const
{
    return static_cast<Type>(mPdu->hdr->type);
}

void MessageLibcoap::SetType(Type aType)
{
    mPdu->hdr->type = aType;
}

void MessageLibcoap::SetToken(const uint8_t *aToken, uint8_t aLength)
{
    mPdu->hdr->token_length = aLength;
    if (aLength)
    {
        coap_add_token(mPdu, aLength, aToken);
    }
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
    size_t         len    = sizeof(options);
    int            res;

    res = coap_split_path(reinterpret_cast<const unsigned char *>(aPath), strlen(aPath), options, &len);

    while (res--)
    {
        coap_add_option(mPdu, COAP_OPTION_URI_PATH, COAP_OPT_LENGTH(option), COAP_OPT_VALUE(option));

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
    size_t   length  = 0;

    coap_get_data(mPdu, &length, &payload);
    aLength = static_cast<uint16_t>(length);
    return payload;
}

Message *AgentLibcoap::NewMessage(Type aType, Code aCode, const uint8_t *aToken, uint8_t aTokenLength)
{
    uint16_t messageId = coap_new_message_id(&mCoap);

    return new MessageLibcoap(aType, aCode, messageId, aToken, aTokenLength);
}

void AgentLibcoap::FreeMessage(Message *aMessage)
{
    delete static_cast<Message *>(aMessage);
}

otbrError AgentLibcoap::Send(Message &       aMessage,
                             const uint8_t * aIp6,
                             uint16_t        aPort,
                             ResponseHandler aHandler,
                             void *          aContext)
{
    otbrError       ret     = OTBR_ERROR_ERRNO;
    MessageLibcoap &message = static_cast<MessageLibcoap &>(aMessage);

    coap_tid_t  tid = COAP_INVALID_TID;
    coap_pdu_t *pdu = message.GetPdu();

    coap_address_t remote;

    CoapAddressInit(remote, aIp6, aPort);

    if (pdu->hdr->type == COAP_MESSAGE_CON)
    {
        MessageMeta meta = {aHandler, aContext};

        // There is no official way to provide handler for each message,
        // we have to embed the handler into its payload.
        VerifyOrExit(pdu->length + sizeof(meta) < pdu->max_size, errno = EMSGSIZE);

        tid = coap_send_confirmed(&mCoap, mCoap.endpoint, &remote, pdu);
        memcpy(pdu->hdr + pdu->length, &meta, sizeof(meta));
    }
    else
    {
        tid = coap_send(&mCoap, mCoap.endpoint, &remote, pdu);
    }

    if (tid == COAP_INVALID_TID || pdu->hdr->type != COAP_MESSAGE_CON)
    {
        message.Free();
    }

    ret = OTBR_ERROR_NONE;

exit:
    if (ret != OTBR_ERROR_NONE)
    {
        otbrLog(OTBR_LOG_ERR, "CoAP no memory for callback!");
        message.Free();
    }

    return ret;
}

void AgentLibcoap::HandleRequest(coap_context_t *        aCoap,
                                 struct coap_resource_t *aResource,
                                 const coap_endpoint_t * aEndPoint,
                                 coap_address_t *        aAddress,
                                 coap_pdu_t *            aRequest,
                                 str *                   aToken,
                                 coap_pdu_t *            aResponse)
{
    AgentLibcoap *agent = reinterpret_cast<AgentLibcoap *>(CONTAINING_RECORD(aCoap, AgentLibcoap, mCoap));

    agent->HandleRequest(aResource, aRequest, aResponse, aAddress->addr.sin6.sin6_addr.s6_addr,
                         ntohs(aAddress->addr.sin6.sin6_port));

    (void)aEndPoint;
    (void)aToken;
}

void AgentLibcoap::HandleRequest(struct coap_resource_t *aResource,
                                 coap_pdu_t *            aRequest,
                                 coap_pdu_t *            aResponse,
                                 const uint8_t *         aAddress,
                                 uint16_t                aPort)
{
    VerifyOrExit(mResources.count(aResource), otbrLog(OTBR_LOG_WARNING, "CoAP received unexpected request!"));

    {
        const Resource &resource = *mResources[aResource];
        MessageLibcoap  req(aRequest);
        MessageLibcoap  res(aResponse);

        assert(!strncmp(reinterpret_cast<const char *>(aResource->uri.s), resource.mPath, aResource->uri.length));

        // Set code to kCoapEmpty to use separate response if no response set by handler.
        // Handler should later respond an Non-ACK response.
        res.SetCode(kCodeEmpty);
        resource.mHandler(resource, req, res, aAddress, aPort, resource.mContext);
    }

exit:
    return;
}

void AgentLibcoap::Input(const void *aBuffer, uint16_t aLength, const uint8_t *aIp6, uint16_t aPort)
{
    mPacket.length    = aLength;
    mPacket.interface = mCoap.endpoint;
    mPacket.dst       = mCoap.endpoint->addr;
    // memset(mPacket.dst.addr.sin6.sin6_addr.s6_addr, 0, sizeof(mPacket.dst.addr.sin6.sin6_addr));
    CoapAddressInit(mPacket.src, aIp6, aPort);
    memcpy(mPacket.payload, aBuffer, aLength);
    coap_handle_message(&mCoap, &mPacket);
}

void AgentLibcoap::HandleResponse(coap_context_t *       aCoap,
                                  const coap_endpoint_t *aLocalInterface,
                                  const coap_address_t * aRemote,
                                  coap_pdu_t *           aSent,
                                  coap_pdu_t *           aReceived,
                                  const coap_tid_t       aId)
{
    MessageMeta meta;

    VerifyOrExit(aSent != NULL, otbrLog(OTBR_LOG_ERR, "request not found!"));

    memcpy(&meta, aSent->hdr + aSent->length, sizeof(meta));

    if (meta.mHandler)
    {
        MessageLibcoap message(aReceived);
        meta.mHandler(message, meta.mContext);
    }

exit:
    (void)aCoap;
    (void)aLocalInterface;
    (void)aRemote;
    (void)aId;

    return;
}

otbrError AgentLibcoap::AddResource(const Resource &aResource)
{
    otbrError        ret      = OTBR_ERROR_ERRNO;
    coap_resource_t *resource = NULL;

    for (Resources::iterator it = mResources.begin(); it != mResources.end(); ++it)
    {
        if (it->second == &aResource)
        {
            otbrLog(OTBR_LOG_ERR, "CoAP resource already added!");
            ExitNow(errno = EEXIST);
            break;
        }
    }

    resource = coap_resource_init(reinterpret_cast<const unsigned char *>(aResource.mPath), strlen(aResource.mPath), 0);
    coap_register_handler(resource, COAP_REQUEST_POST, AgentLibcoap::HandleRequest);
    coap_add_resource(&mCoap, resource);
    mResources[resource] = &aResource;
    ret                  = OTBR_ERROR_NONE;

exit:
    return ret;
}

otbrError AgentLibcoap::RemoveResource(const Resource &aResource)
{
    otbrError ret = OTBR_ERROR_ERRNO;

    for (Resources::iterator it = mResources.begin(); it != mResources.end(); ++it)
    {
        if (it->second == &aResource)
        {
            coap_delete_resource(&mCoap, it->first->key);
            mResources.erase(it);
            ret = OTBR_ERROR_NONE;
            break;
        }
    }

    if (ret)
    {
        errno = ENOENT;
    }

    return ret;
}

AgentLibcoap::AgentLibcoap(NetworkSender aNetworkSender, void *aContext)
{
    mContext       = aContext;
    mNetworkSender = aNetworkSender;
    coap_clock_init();

    time_t clock_offset = time(NULL);
    memset(&mCoap, 0, sizeof(mCoap));
    prng_init(reinterpret_cast<unsigned long>(aNetworkSender) ^ static_cast<unsigned long>(clock_offset));
    prng(reinterpret_cast<unsigned char *>(&mCoap.message_id), sizeof(unsigned short));

    coap_address_t addr;
    coap_address_init(&addr);
    addr.addr.sin6.sin6_family = AF_INET6;
    mCoap.endpoint             = coap_new_endpoint(&addr, COAP_ENDPOINT_NOSEC);
    mCoap.network_send         = AgentLibcoap::NetworkSend;

    coap_register_response_handler(&mCoap, AgentLibcoap::HandleResponse);
}

ssize_t AgentLibcoap::NetworkSend(coap_context_t *       aCoap,
                                  const coap_endpoint_t *aLocalInterface,
                                  const coap_address_t * aDestination,
                                  unsigned char *        aBuffer,
                                  size_t                 aLength)
{
    (void)aLocalInterface;

    AgentLibcoap *agent = static_cast<AgentLibcoap *>(CONTAINING_RECORD(aCoap, AgentLibcoap, mCoap));

    return agent->mNetworkSender(aBuffer, static_cast<uint16_t>(aLength),
                                 reinterpret_cast<const uint8_t *>(&aDestination->addr.sin6.sin6_addr),
                                 ntohs(aDestination->addr.sin6.sin6_port), agent->mContext);
}

Agent *Agent::Create(NetworkSender aNetworkSender, void *aContext)
{
    return new AgentLibcoap(aNetworkSender, aContext);
}

void Agent::Destroy(Agent *aAgent)
{
    delete static_cast<AgentLibcoap *>(aAgent);
}

} // namespace Coap

} // namespace BorderRouter

} // namespace ot
