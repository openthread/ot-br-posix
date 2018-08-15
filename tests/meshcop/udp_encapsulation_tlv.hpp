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
 *   The file is the header for the udp encapsulation tlv
 */

#ifndef OTBR_UDP_ENCAPSULATION_TLV_HPP_
#define OTBR_UDP_ENCAPSULATION_TLV_HPP_

#include "arpa/inet.h"
#include "common/tlv.hpp"

namespace ot {
namespace BorderRouter {

class UdpEncapsulationTlv : public Tlv
{
public:
    uint16_t GetUdpSourcePort() const { return ntohs(mSourcePort); }

    uint16_t GetUdpDestionationPort() const { return ntohs(mDestPort); }

    const uint8_t *GetUdpPayload() const
    {
        return reinterpret_cast<const uint8_t *>(this) + sizeof(UdpEncapsulationTlv);
    }

    uint8_t GetUdpPayloadLength() const
    {
        uint8_t portDataLength = 2 * sizeof(uint16_t);
        if (GetLength() < portDataLength)
        {
            return 0;
        }
        else
        {
            return GetLength() - portDataLength;
        }
    }

    void SetUdpSourcePort(uint16_t aSrcPort)
    {
        mSourcePort = htons(aSrcPort);
    }

    void SetUdpDestionationPort(uint16_t aDestPort)
    {
        mDestPort = htons(aDestPort);
    }

    void SetUdpPayload(const void *aPayload, size_t aLength)
    {
        uint8_t tlvLength = aLength + sizeof(mSourcePort) + sizeof(mDestPort);
        SetLength(tlvLength, true);

        memcpy(reinterpret_cast<uint8_t *>(this) + sizeof(UdpEncapsulationTlv), aPayload, aLength);
    }

private:
    uint16_t mLengthExtended;
    uint16_t mSourcePort;
    uint16_t mDestPort;
};

} // namespace BorderRouter
} // namespace ot

#endif // OTBR_UDP_ENCAPSULATION_TLV_HPP_
