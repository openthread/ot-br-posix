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

#ifndef COAP_LIBCOAP_HPP_
#define COAP_LIBCOAP_HPP_

#include "libcoap.h"
#include "coap.hpp"

namespace ot {

namespace BorderAgent {

namespace Coap {

/**
 * @addtogroup border-agent-coap
 *
 * @brief
 *   This module includes definition for libcoap-based CoAP service.
 *
 * @{
 */

/**
 * This class implements CoAP message functionality based on libcoap.
 *
 */
class MessageLibcoap : public Message
{
public:
    /**
     * The constructor to initialize a CoAP message.
     *
     * @param[in]   aType           The CoAP type.
     * @param[in]   aCode           The CoAP code.
     * @param[in]   aMessageId      The CoAP message id.
     * @param[in]   aToken          The CoAP token.
     * @param[in]   aTokenLength    Number of bytes in @p aToken.
     *
     */
    MessageLibcoap(Type aType, Code aCode, uint16_t aMessageId, const uint8_t *aToken, uint8_t aTokenLength);

    /**
     * The constructor to wrap an libcoap pdu.
     *
     * @param[in]   aPdu    A pointer to the libcoap pdu.
     *
     */
    MessageLibcoap(coap_pdu_t *aPdu) :
        mPdu(aPdu) {}

    virtual ~MessageLibcoap(void) {};

    Code GetCode(void) const;

    Type GetType(void) const;

    const uint8_t *GetToken(uint8_t &aLength) const;

    void SetPath(const char *aPath);

    const uint8_t *GetPayload(uint16_t &aLength) const;

    void SetPayload(const uint8_t *aPayload, uint16_t aLength);

    coap_pdu_t *GetPdu(void) { return mPdu; }

    /**
     * This method frees the wrapped libcoap pdu.
     *
     * @warning This method will not be called by destructor. libcoap will free the pdu on conditions.
     *
     */
    void Free(void);

private:
    enum
    {
        kMaxOptionSize = 128, ///< Maximum bytes allowed for all CoAP options.
    };
    coap_pdu_t *mPdu;
};

/**
 * This class implements CoAP agent based on libcoap.
 *
 */
class AgentLibcoap : public Agent
{
public:
    /**
     * The constructor to initialize a CoAP agent.
     *
     * @param[in]   aNetworkSender      A pointer to the function that actually sends the data.
     * @param[in]   aResources          A pointer to the Resource array. The last resource must be {0, 0}.
     * @param[in]   aContext    A pointer to application-specific context.
     *
     */
    AgentLibcoap(NetworkSender aNetworkSender, const Resource *aResources, void *aContext);

    /**
     * This method processes this CoAP message in @p aBuffer, which can be a request or response.
     *
     * @param[in]   aBuffer     A pointer to decrypted data.
     * @param[in]   aLength     Number of bytes of @p aBuffer.
     * @param[in]   aIp6        A pointer to the source Ipv6 address of this request.
     * @param[in]   aPort       Source UDP port of this request.
     *
     */
    void Input(const void *aBuffer, uint16_t aLength, const uint8_t *aIp6, uint16_t aPort);

    /**
     * This method sends the CoAP message, which can be a request or response.
     *
     * @param[in]   aMessage    A reference to the message to send.
     * @param[in]   aIp6        A pointer to the source Ipv6 address of this request.
     * @param[in]   aPort       Source UDP port of this request.
     * @param[in]   aHandler    A function poiner to be called when response is received if the message is a request.
     *
     */
    void Send(Message &aMessage, const uint8_t *aIp6, uint16_t aPort, ResponseHandler aHandler);

    /**
     * This method creates a CoAP message with the given arguments.
     *
     * @param[in]   aType           The CoAP type.
     * @param[in]   aCode           The CoAP code.
     * @param[in]   aToken          The CoAP token.
     * @param[in]   aTokenLength    Number of bytes in @p aToken.
     *
     * @returns The newly CoAP message.
     *
     */
    Message *NewMessage(Message::Type aType, Message::Code aCode, const uint8_t *aToken, uint8_t aTokenLength);

    /**
     * This method frees a CoAP message.
     *
     * @param[in]   aMessage    A pointer to the message to free.
     *
     */
    virtual void FreeMessage(Message *aMessage);

private:

    static void HandleRequest(coap_context_t *aCoap,
                              struct coap_resource_t *aResource,
                              const coap_endpoint_t *aEndPoint,
                              coap_address_t *aAddress,
                              coap_pdu_t *aRequest,
                              str *aToken,
                              coap_pdu_t *aResponse);

    static void HandleResponse(coap_context_t *ctx,
                               const coap_endpoint_t *local_interface,
                               const coap_address_t *remote,
                               coap_pdu_t *sent,
                               coap_pdu_t *received,
                               const coap_tid_t id);

    static ssize_t NetworkSend(coap_context_t *aCoap,
                               const coap_endpoint_t *aLocalInterface,
                               const coap_address_t *aDestination,
                               unsigned char *aBuffer, size_t aLength);

    const Resource *mResources;
    NetworkSender   mNetworkSender;
    void           *mContext;
    coap_context_t  mCoap;
    coap_packet_t   mPacket;
};

/**
 * @}
 */

} // namespace Coap

} // namespace BorderAgent

} // namespace ot

#endif  // COAP_LIBCOAP_HPP_
