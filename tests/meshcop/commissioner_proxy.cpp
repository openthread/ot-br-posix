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

    printf("Send to size %d\n", utils::LengthOf(buffer, tlv));
    proxyServerAddr.sin_family      = AF_INET;
    proxyServerAddr.sin_port        = htons(kCommissionerProxyPort);
    proxyServerAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    int ret =  sendto(mClientFd, buffer, utils::LengthOf(buffer, tlv), 0,
                  reinterpret_cast<struct sockaddr *>(&proxyServerAddr), sizeof(proxyServerAddr));
    printf("Sent %d\n", ret);
    return ret;
}

int CommissionerProxy::RecvFrom(void *aBuf, size_t aLength, struct sockaddr_in6 &aSrcAddr)
{
    int ret, len;
    uint8_t readBuffer[kSizeMaxPacket];

    VerifyOrExit(len = ret = recv(mClientFd, readBuffer, aLength, 0) > 0);
    printf("Proxy get length %d, buf size %zu\n", len, aLength);
    for (size_t i = 0; i < 83; i++) {
        printf("%02x ", ((uint8_t*)readBuffer)[i]);
    }
    printf("\n");
    for (const Tlv *tlv = reinterpret_cast<const Tlv *>(readBuffer); utils::LengthOf(readBuffer, tlv) < len;
         tlv            = tlv->GetNext())
    {
        int tlvType = tlv->GetType();

        if (tlvType == Meshcop::kIPv6AddressList)
        {
            printf("Get from address\n");
            assert(tlv->GetLength() == 16); // IPv6 address size
            memcpy(&aSrcAddr.sin6_addr, tlv->GetValue(), sizeof(aSrcAddr.sin6_addr));
        }
        else if (tlvType == Meshcop::kUdpEncapsulation)
        {
            printf("Get udp encapsulation\n");
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
