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
 *   This file includes definition for CoAP service.
 */

#ifndef COAP_HPP_
#define COAP_HPP_

#include <stdint.h>
#include <unistd.h>

#include "types.hpp"

namespace ot {

namespace BorderRouter {

namespace Coap {

/**
 * @addtogroup border-router-coap
 *
 * @brief
 *   This module includes definition for CoAP service.
 *
 * @{
 */

/**
 * CoAP Type values.
 *
 */
enum Type
{
    kTypeConfirmable    = 0x00, ///< Confirmable
    kTypeNonConfirmable = 0x01, ///< Non-confirmable
    kTypeAcknowledgment = 0x02, ///< Acknowledgment
    kTypeReset          = 0x03, ///< Reset
};

/**
 * CoAP Code values.
 *
 */
enum Code
{
    kCodeEmpty   = 0x00, ///< Empty message code
    kCodeGet     = 0x01, ///< Get
    kCodePost    = 0x02, ///< Post
    kCodePut     = 0x03, ///< Put
    kCodeDelete  = 0x04, ///< Delete
    kCodeCodeMin = 0x40, ///< 2.00
    kCodeCreated = 0x41, ///< Created
    kCodeDeleted = 0x42, ///< Deleted
    kCodeValid   = 0x43, ///< Valid
    kCodeChanged = 0x44, ///< Changed
    kCodeContent = 0x45, ///< Content
};

/**
 * This interface defines CoAP message functionality.
 *
 */
class Message
{
public:
    virtual ~Message(void){};

    /**
     * This method returns the CoAP code of this message.
     *
     * @returns the CoAP code of the message.
     *
     */
    virtual Code GetCode(void) const = 0;

    /**
     * This method sets the CoAP code of this message.
     *
     * @param[in]   aCode       The CoAP code.
     *
     */
    virtual void SetCode(Code aCode) = 0;

    /**
     * This method returns the CoAP type of this message.
     *
     * @returns The CoAP type of the message.
     *
     */
    virtual Type GetType(void) const = 0;

    /**
     * This method sets the CoAP type of this message.
     *
     * @param[in]   aType       The CoAP type.
     *
     */
    virtual void SetType(Type aType) = 0;

    /**
     * This method returns the token of this message.
     *
     * @param[out]  aLength     Number of bytes of the token.
     *
     * @returns A pointer to the token.
     *
     */
    virtual const uint8_t *GetToken(uint8_t &aLength) const = 0;

    /**
     * This method sets the token of this message.
     *
     * @param[in]   aToken      A pointer to the token.
     * @param[in]   aLength     Number of bytes of the token.
     *
     */
    virtual void SetToken(const uint8_t *aToken, uint8_t aLength) = 0;

    /**
     * This method sets the CoAP Uri Path of this message.
     *
     * @param[in]   aPath       A pointer to to the null-terminated string of Uri Path.
     *
     */
    virtual void SetPath(const char *aPath) = 0;

    /**
     * This method returns the payload of this message.
     *
     * @param[out]  aLength     Number of bytes of the payload.
     *
     * @returns A pointer to the payload buffer.
     */
    virtual const uint8_t *GetPayload(uint16_t &aLength) const = 0;

    /**
     * This method sets the CoAP payload of this message.
     *
     * @param[in]   aPayload    A pointer to the payload buffer.
     * @param[in]   aLength     Number of bytes in @p aPayload.
     *
     */
    virtual void SetPayload(const uint8_t *aPayload, uint16_t aLength) = 0;
};

/**
 * @typedef struct Resource
 *
 * This declares the CoAP resource.
 *
 */
typedef struct Resource Resource;

/**
 * This function pointer is called when a CoAP request received.
 *
 * @param[in]   aResource       A reference to the resource requested.
 * @param[in]   aRequest        A reference to the CoAP request message.
 * @param[in]   aResponse       A reference to the CoAP response message.
 * @param[in]   aIp6            A reference to the source Ipv6 address of this request.
 * @param[in]   aPort           Source UDP port of this request.
 * @param[in]   aContext        A pointer to application-specific context.
 *
 */
typedef void (*RequestHandler)(const Resource &aResource,
                               const Message & aRequest,
                               Message &       aResponse,
                               const uint8_t * aIp6,
                               uint16_t        aPort,
                               void *          aContext);

/**
 * This function pointer is called when a CoAP response received.
 *
 * @param[in]   aMessage        A pointer to the response message.
 * @param[in]   aContext        A pointer to application-specific context.
 *
 */
typedef void (*ResponseHandler)(const Message &aMessage, void *aContext);

/**
 * This struct defines a CoAP resource and its handler.
 */
struct Resource
{
    void *         mContext; ///< A pointer to application-specific context.
    const char *   mPath;    ///< The CoAP Uri Path.
    RequestHandler mHandler; ///< The function to handle request to mPath.

    /**
     * The constructor to initialize a CoAP resource.
     *
     * @param[in]   aPath       The resource path.
     * @param[in]   aHandler    The function to be called when received request to this resource.
     * @param[in]   aContext        A pointer to application-specific context.
     *
     */
    Resource(const char *aPath, RequestHandler aHandler, void *aContext)
        : mContext(aContext)
        , mPath(aPath)
        , mHandler(aHandler)
    {
    }
};

/**
 * This interface defines the functionality of CoAP Agent.
 *
 */
class Agent
{
public:
    /**
     * This function poiner is called when the agent needs to send data out.
     *
     * @param[in]   aBuffer     A pointer to decrypted data.
     * @param[in]   aLength     Number of bytes of @p aBuffer.
     * @param[in]   aIp6        A pointer to the source Ipv6 address of this request.
     * @param[in]   aPort       Source UDP port of this request.
     * @param[in]   aContext    A pointer to application-specific context.
     *
     * @returns number of bytes successfully sended, a negative value indicates failure.
     *
     */
    typedef ssize_t (
        *NetworkSender)(const uint8_t *aBuffer, uint16_t aLength, const uint8_t *aIp6, uint16_t aPort, void *aContext);

    /**
     * This method processes this CoAP message in @p aBuffer, which can be a request or response.
     *
     * @param[in]   aBuffer     A pointer to decrypted data.
     * @param[in]   aLength     Number of bytes of @p aBuffer.
     * @param[in]   aIp6        A pointer to the source Ipv6 address of this request.
     * @param[in]   aPort       Source UDP port of this request.
     *
     */
    virtual void Input(const void *aBuffer, uint16_t aLength, const uint8_t *aIp6, uint16_t aPort) = 0;

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
    virtual Message *NewMessage(Type aType, Code aCode, const uint8_t *aToken, uint8_t aTokenLength) = 0;

    /**
     * This method frees a CoAP message.
     *
     * @param[in]   aMessage    A pointer to the message to free.
     *
     */
    virtual void FreeMessage(Message *aMessage) = 0;

    /**
     * This method registers a CoAP resource.
     *
     * @param[in]   aResource       A reference to the resource.
     *
     * @retval  OTBR_ERROR_NONE     Successfully added the resource.
     * @retval  OTBR_ERROR_ERRNO    Failed to added the resource.
     *
     */
    virtual otbrError AddResource(const Resource &aResource) = 0;

    /**
     * This method Deregisters a CoAP resource.
     *
     * @param[in]   aResource       A reference to the resource.
     *
     * @retval  OTBR_ERROR_NONE     Successfully added the resource.
     * @retval  OTBR_ERROR_ERRNO    Failed to added the resource.
     *
     */
    virtual otbrError RemoveResource(const Resource &aResource) = 0;

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
    virtual otbrError Send(Message &       aMessage,
                           const uint8_t * aIp6,
                           uint16_t        aPort,
                           ResponseHandler aHandler,
                           void *          aContext) = 0;

    /**
     * This method creates a CoAP agent.
     *
     * @param[in]   aNetworkSender  A pointer to the function that actually sends the data.
     * @param[in]   aContext        A pointer to application-specific context.
     *
     * @returns The pointer to CoAP agent.
     */
    static Agent *Create(NetworkSender aNetworkSender, void *aContext = NULL);

    /**
     * This method destroys a CoAP agent.
     *
     * @param[in]   aAgent      A pointer to the agent to be destroyed.
     *
     */
    static void Destroy(Agent *aAgent);

    virtual ~Agent(void) {}
};

/**
 * @}
 */

} // namespace Coap

} // namespace BorderRouter

} // namespace ot

#endif // COAP_HPP_
