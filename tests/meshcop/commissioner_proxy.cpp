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
 *   The file is the header of commissioner proxy class
 */

#include "commissioner_common.hpp"
#include "commissioner_proxy.hpp"
#include "commissioner_utils.hpp"
#include <boost/bind/bind.hpp>
#include <cassert>
#include "common/logging.hpp"
#include "common/code_utils.hpp"
#include "udp_encapsulation_tlv.hpp"

namespace ot {
namespace BorderRouter {

CommissionerProxy::CommissionerProxy()
{
    sockaddr_in addr;

    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port        = htons(0);

    mClientFd = socket(AF_INET, SOCK_DGRAM, 0);
    if (bind(mClientFd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr) != 0))
    {
        otbrLog(OTBR_LOG_ERR, "Fail to bind udp proxy client");
    }
}

int CommissionerProxy::SendTo(const struct sockaddr_in6 &aDestAddr, const void *aBuf, size_t aLength)
{
    (void)aBuf;
    (void)aLength;
    uint8_t     buffer[kSizeMaxPacket];
    Tlv *       tlv = reinterpret_cast<Tlv *>(buffer);
    sockaddr_in proxyServerAddr;

    tlv->SetType(Meshcop::kIPv6AddressList);
    tlv->SetLength(16); // 16 for ipv6 address length
    tlv->SetValue(&aDestAddr.sin6_addr, 16);
    tlv = tlv->GetNext();

    {
        UdpEncapsulationTlv *udpTlv = static_cast<UdpEncapsulationTlv *>(tlv);

        uint16_t srcPort  = 0;
        uint16_t destPort = ntohs(aDestAddr.sin6_port);
        udpTlv->SetType(Meshcop::kUdpEncapsulation);
        udpTlv->SetUdpSourcePort(srcPort);
        udpTlv->SetUdpDestionationPort(destPort);
        udpTlv->SetUdpPayload(aBuf, aLength);
        tlv = tlv->GetNext();
    }

    proxyServerAddr.sin_family      = AF_INET;
    proxyServerAddr.sin_port        = htons(kCommissionerProxyPort);
    proxyServerAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    int ret =  sendto(mClientFd, buffer, utils::LengthOf(buffer, tlv), 0,
                  reinterpret_cast<struct sockaddr *>(&proxyServerAddr), sizeof(proxyServerAddr));
    return ret;
}

int CommissionerProxy::RecvFrom(void *aBuf, size_t aLength, struct sockaddr_in6 &aSrcAddr)
{
    int ret = 0, len;
    uint8_t readBuffer[kSizeMaxPacket];

    VerifyOrExit((len = ret = recv(mClientFd, readBuffer, aLength, 0)) > 0);
    for (const Tlv *tlv = reinterpret_cast<const Tlv *>(readBuffer); utils::LengthOf(readBuffer, tlv) < len;
         tlv            = tlv->GetNext())
    {
        int tlvType = tlv->GetType();

        if (tlvType == Meshcop::kIPv6AddressList)
        {
            assert(tlv->GetLength() == 16); // IPv6 address size
            memcpy(&aSrcAddr.sin6_addr, tlv->GetValue(), sizeof(aSrcAddr.sin6_addr));
        }
        else if (tlvType == Meshcop::kUdpEncapsulation)
        {
            const UdpEncapsulationTlv *udpTlv = static_cast<const UdpEncapsulationTlv*>(tlv);
            size_t   readLength = utils::Min<size_t>(udpTlv->GetUdpPayloadLength(), aLength);
            uint16_t srcPort    = udpTlv->GetUdpSourcePort();
            aSrcAddr.sin6_port = srcPort;
            const uint8_t *udpPayload      = udpTlv->GetUdpPayload();
            memcpy(aBuf, udpPayload, readLength);
            ret = readLength;
        }
    }
exit:
    return ret;
}

CommissionerProxy::~CommissionerProxy()
{
    close(mClientFd);
}

} // namespace BorderRouter
} // namespace ot
