/*
 *    Copyright (c) 2018, The OpenThread Authors.
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
 *   The file implements tmf client
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "tmf_client.hpp"
#include "common/udp_encapsulation_tlv.hpp"
#include "utils/addr.hpp"
#include "utils/misc.hpp"

namespace ot {
namespace BorderRouter {

const uint16_t TmfClient::kTmfPort   = 61631;
const char     TmfClient::kDiagUri[] = "d/dg";

TmfClient::TmfClient(CommissionerProxy *proxy)
    : mCoapAgent(Coap::Agent::Create(&TmfClient::SendCoap, this))
    , mCoapToken(rand())
    , mProxy(proxy)
{
}

ssize_t TmfClient::SendCoap(const uint8_t *aBuffer,
                            uint16_t       aLength,
                            const uint8_t *aIp6,
                            uint16_t       aPort,
                            void *         aContext)
{
    TmfClient *client = static_cast<TmfClient *>(aContext);

    return client->mProxy->SendTo(client->mDestAddr, aBuffer, aLength);

    (void)aIp6;
    (void)aPort;
}

void TmfClient::HandleCoapResponse(const Coap::Message &aMessage, void *aContext)
{
    uint16_t       length;
    const uint8_t *payload    = aMessage.GetPayload(length);
    TmfClient *    client     = static_cast<TmfClient *>(aContext);
    size_t         copyLength = Utils::Min<size_t>(sizeof(client->mResponseBuffer), length);

    memcpy(client->mResponseBuffer, payload, copyLength);
    client->mResponseSize    = copyLength;
    client->mResponseHandled = true;
}

size_t TmfClient::PostCoapAndWaitForResponse(sockaddr_in6 dest, const char *uri, uint8_t *payload, size_t length)
{
    int          ret;
    uint8_t      buffer[kSizeMaxPacket];
    sockaddr_in6 srcAddr;
    uint16_t     token     = ++mCoapToken;
    token                  = htons(token);
    Coap::Message *message = mCoapAgent->NewMessage(Coap::kTypeConfirmable, Coap::kCodePost,
                                                    reinterpret_cast<const uint8_t *>(&token), sizeof(token));

    message->SetPath(uri);
    message->SetPayload(payload, length);
    mDestAddr = dest;

    mCoapAgent->Send(*message, NULL, 0, HandleCoapResponse, this);
    mCoapAgent->FreeMessage(message);

    mResponseHandled = false;
    do
    {
        ret = mProxy->RecvFrom(buffer, sizeof(buffer), srcAddr);
        if (ret > 0)
        {
            mCoapAgent->Input(buffer, static_cast<uint16_t>(ret), NULL, 0);
            if (mResponseHandled)
            {
                mResponseHandled = false;
                break;
            }
        }
    } while (ret >= 0);

    return mResponseSize;
}

void TmfClient::QueryDiagnosticData(const struct in6_addr &destAddr, uint8_t queryType)
{
    uint8_t      requestBuffer[kSizeMaxPacket];
    sockaddr_in6 dest;
    Tlv *        tlv = reinterpret_cast<Tlv *>(requestBuffer);

    dest.sin6_addr = destAddr;
    dest.sin6_port = htons(kTmfPort);
    tlv->SetType(kTypeListTlvType);
    tlv->SetValue(queryType);
    tlv = tlv->GetNext();
    PostCoapAndWaitForResponse(dest, kDiagUri, requestBuffer, Utils::LengthOf(requestBuffer, tlv));
}

static std::vector<struct in6_addr> ParseAddressesTLV(const uint8_t *aBuffer)
{
    std::vector<struct in6_addr> addresses;
    const ot::Tlv *              tlv           = reinterpret_cast<const Tlv *>(aBuffer);
    uint16_t                     payloadLength = tlv->GetLength();
    size_t                       addrCount     = payloadLength / sizeof(struct in6_addr);

    assert(tlv->GetType() == kAddressListType);
    assert(payloadLength % sizeof(struct in6_addr) == 0);
    const struct in6_addr *payload = reinterpret_cast<const struct in6_addr *>(tlv->GetValue());
    for (size_t i = 0; i < addrCount; i++)
    {
        addresses.push_back(payload[i]);
    }
    return addresses;
}

std::vector<struct in6_addr> TmfClient::QueryAllV6Addresses(const in6_addr &aAddr)
{
    QueryDiagnosticData(aAddr, kAddressListType);
    return ParseAddressesTLV(mResponseBuffer);
}

TmfClient::~TmfClient()
{
    Coap::Agent::Destroy(mCoapAgent);
}

} // namespace BorderRouter
} // namespace ot
