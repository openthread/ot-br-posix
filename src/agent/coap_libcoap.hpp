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

#include <map>

#include "coap.hpp"
#include "libcoap.h"

namespace ot {

namespace BorderRouter {

namespace Coap {

/**
 * @addtogroup border-router-coap
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
    MessageLibcoap(coap_pdu_t *aPdu)
        : mPdu(aPdu)
    {
    }

    virtual ~MessageLibcoap(void) {}

    /**
     * This method returns the CoAP code of this message.
     *
     * @returns the CoAP code of the message.
     *
     */
    Code GetCode(void) const;

    /**
     * This method sets the CoAP code of this message.
     *
     * @param[in]   aCode   The CoAP code.
     *
     */
    void SetCode(Code aCode);

    /**
     * This method returns the CoAP type of this message.
     *
     * @returns The CoAP type of the message.
     *
     */
    Type GetType(void) const;

    /**
     * This method sets the CoAP type of this message.
     *
     * @param[in]   aType   The CoAP type.
     *
     */
    void SetType(Type aType);

    /**
     * This method returns the token of this message.
     *
     * @param[out]    aLength       Number of bytes of the token.
     *
     * @returns A pointer to the token.
     *
     */
    const uint8_t *GetToken(uint8_t &aLength) const;

    /**
     * This method sets the token of this message.
     *
     * @param[in]   aToken          A pointer to the token.
     * @param[in]   aLength         Number of bytes of the token.
     *
     */
    void SetToken(const uint8_t *aToken, uint8_t aLength);

    /**
     * This method sets the CoAP Uri Path of this message.
     *
     * @param[in]   aPath           A pointer to to the null-terminated string of Uri Path.
     *
     */
    void SetPath(const char *aPath);

    /**
     * This method returns the payload of this message.
     *
     * @param[out]  aLength         Number of bytes of the payload.
     *
     * @returns A pointer to the payload buffer.
     */
    const uint8_t *GetPayload(uint16_t &aLength) const;

    /**
     * This method sets the payload of this message.
     *
     * @param[in]   aPayload        A pointer to the payload.
     * @param[in]   aLength         Number of bytes of the payload.
     *
     * @returns A pointer to the payload buffer.
     */
    void SetPayload(const uint8_t *aPayload, uint16_t aLength);

    /**
     * This method returns the underlying libcoap PDU.
     *
     * @returns A pointer to the underlying libcoap PDU.
     */
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
     * @param[in]   aContext            A pointer to application-specific context.
     *
     */
    AgentLibcoap(NetworkSender aNetworkSender, void *aContext);

    /**
     * This method processes this CoAP message in @p aBuffer, which can be a request or response.
     *
     * @param[in]   aBuffer         A pointer to decrypted data.
     * @param[in]   aLength         Number of bytes of @p aBuffer.
     * @param[in]   aIp6            A pointer to the source Ipv6 address of this request.
     * @param[in]   aPort           Source UDP port of this request.
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
     * @param[in]   aContext    A pointer to application-specific context.
     *
     * @retval      OTBR_ERROR_NONE     Successfully sent the message.
     * @retval      OTBR_ERROR_ERRNO    Failed to send the message.
     *                                  - EMSGSIZE No space for response handler.
     *
     */
    otbrError Send(Message &aMessage, const uint8_t *aIp6, uint16_t aPort, ResponseHandler aHandler, void *aContext);

    /**
     * This method creates a CoAP message with the given arguments.
     *
     * @param[in]   aType           The CoAP type.
     * @param[in]   aCode           The CoAP code.
     * @param[in]   aToken          The CoAP token.
     * @param[in]   aTokenLength    Number of bytes in @p aToken.
     *
     * @returns The pointer to the newly created CoAP message.
     *
     */
    Message *NewMessage(Type aType, Code aCode, const uint8_t *aToken, uint8_t aTokenLength);

    /**
     * This method frees a CoAP message.
     *
     * @param[in]   aMessage        A pointer to the message to free.
     *
     */
    virtual void FreeMessage(Message *aMessage);

    /**
     * This method registers a CoAP resource.
     *
     * @param[in]   aResource       A reference to the resource.
     *
     * @retval  OTBR_ERROR_NONE     Successfully added the resource.
     * @retval  OTBR_ERROR_ERRNO    Failed to added the resource.
     *
     */
    otbrError AddResource(const Resource &aResource);

    /**
     * This method Deregisters a CoAP resource.
     *
     * @param[in]   aResource       A reference to the resource.
     *
     * @retval  OTBR_ERROR_NONE     Successfully added the resource.
     * @retval  OTBR_ERROR_ERRNO    Failed to added the resource.
     *
     */
    otbrError RemoveResource(const Resource &aResource);

private:
    typedef std::map<struct coap_resource_t *, const Resource *> Resources;

    struct MessageMeta
    {
        ResponseHandler mHandler;
        void *          mContext;
    };

    static void HandleRequest(coap_context_t *        aCoap,
                              struct coap_resource_t *aResource,
                              const coap_endpoint_t * aEndPoint,
                              coap_address_t *        aAddress,
                              coap_pdu_t *            aRequest,
                              str *                   aToken,
                              coap_pdu_t *            aResponse);

    void HandleRequest(struct coap_resource_t *aResource,
                       coap_pdu_t *            aRequest,
                       coap_pdu_t *            aResponse,
                       const uint8_t *         aAddress,
                       uint16_t                aPort);

    static void HandleResponse(coap_context_t *       ctx,
                               const coap_endpoint_t *local_interface,
                               const coap_address_t * remote,
                               coap_pdu_t *           sent,
                               coap_pdu_t *           received,
                               const coap_tid_t       id);

    static ssize_t NetworkSend(coap_context_t *       aCoap,
                               const coap_endpoint_t *aLocalInterface,
                               const coap_address_t * aDestination,
                               unsigned char *        aBuffer,
                               size_t                 aLength);

    Resources      mResources;
    NetworkSender  mNetworkSender;
    void *         mContext;
    coap_context_t mCoap;
    coap_packet_t  mPacket;
};

/**
 * @}
 */

} // namespace Coap

} // namespace BorderRouter

} // namespace ot

#endif // COAP_LIBCOAP_HPP_
