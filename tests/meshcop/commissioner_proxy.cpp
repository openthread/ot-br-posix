#include "commission_common.hpp"
#include "commissioner_proxy.hpp"
#include "utils.hpp"
#include <boost/bind/bind.hpp>
#include <cassert>
#include "common/tlv.hpp"

namespace ot {
namespace BorderRouter {

const char *CommissionerProxy::kmUdpRxUrl = "c/ur";
const char *CommissionerProxy::kmUdpTxUrl = "c/ut";

ssize_t CommissionerProxy::SendCoapProxy(const uint8_t *aBuffer,
                                         uint16_t       aLength,
                                         const uint8_t *aIp6,
                                         uint16_t       aPort,
                                         void *         aContext)
{
    CommissionerProxy *proxy = reinterpret_cast<CommissionerProxy *>(aContext);
    (void)aIp6;
    (void)aPort;
    return write(proxy->mClientFd, aBuffer, aLength);
}

void CommissionerProxy::HandleUDPRx(const Coap::Resource &aResource,
                                    const Coap::Message & aPost,
                                    Coap::Message &       aResponse,
                                    const uint8_t *       aIp6,
                                    uint16_t              aPort,
                                    void *                aContext)
{
    UdpBuffer &    udpBuffer = *static_cast<UdpBuffer *>(aContext);
    uint16_t       length;
    const uint8_t *payload    = aPost.GetPayload(length);
    const uint8_t *nowPayload = payload;
    while (nowPayload - payload < length)
    {
        const Tlv *tlv = reinterpret_cast<const Tlv *>(nowPayload);
        if (tlv->GetType() == Meshcop::kIPv6AddressType)
        {
            assert(tlv->GetLength() == 16); // IPv6 address size
            memcpy(&udpBuffer.srcAddress, tlv->GetValue(), 16);
        }
        else if (tlv->GetType() == Meshcop::kUdpEncapusulationType)
        {
            int      encapusulationLength = tlv->GetLength();
            size_t   readLength = utils::min<size_t>(encapusulationLength - 2 * sizeof(uint16_t), udpBuffer.length);
            uint16_t srcPort    = *reinterpret_cast<const uint16_t *>(tlv->GetValue());
            udpBuffer.srcAddress.sin6_port = srcPort;
            const uint8_t *udpPayload      = reinterpret_cast<const uint8_t *>(tlv->GetValue()) + 2 * sizeof(uint16_t);
            memcpy(udpBuffer.buf, udpPayload, readLength);
            udpBuffer.length = readLength;
        }
        nowPayload = reinterpret_cast<const uint8_t *>(tlv->GetNext());
    }
    udpBuffer.recieved = true;
    (void)aResource;
    (void)aResponse;
    (void)aIp6;
    (void)aPort;
}

CommissionerProxy::CommissionerProxy(const sockaddr &aServerAddr)
    : mCoapAgent(Coap::Agent::Create(SendCoapProxy, this))
    , mCoapToken(rand())
    , mSourcePort(0)
    , mUdpRxHandler(kmUdpRxUrl, HandleUDPRx, &mUdpBuffer)
{
    mCoapAgent->AddResource(mUdpRxHandler);
    mClientFd = socket(AF_INET, SOCK_STREAM, 0);
    connect(mClientFd, &aServerAddr, sizeof(aServerAddr));
}

int CommissionerProxy::BindProxyUdpSocket(uint16_t aSourcePort)
{
    mSourcePort = aSourcePort;
    return 0;
}

int CommissionerProxy::Write(const struct sockaddr_in6 &destAddress, const void *buf, size_t len)
{
    uint16_t token         = ++mCoapToken;
    token                  = htons(token);
    Coap::Message *message = mCoapAgent->NewMessage(Coap::kTypeNonConfirmable, Coap::kCodePost,
                                                    reinterpret_cast<const uint8_t *>(&token), sizeof(token));
    uint8_t        buffer[kSizeMaxPacket];
    Tlv *          tlv = reinterpret_cast<Tlv *>(buffer);
    tlv->SetType(Meshcop::kIPv6AddressType);
    tlv->SetLength(16); // 16 for ipv6 address length
    tlv->SetValue(&destAddress.sin6_addr, 16);
    tlv = tlv->GetNext();

    uint16_t srcPort  = htons(mSourcePort);
    uint16_t destPort = destAddress.sin6_port;
    size_t   _len     = len + sizeof(srcPort) + sizeof(destPort);
    assert(_len < ((1 << 16) - 1));
    uint16_t length = _len;
    tlv->SetType(Meshcop::kUdpEncapusulationType);
    tlv->SetLength(length, true);
    tlv->SetUdpSourcePort(srcPort);
    tlv->SetUdpDestionationPort(destPort);
    tlv->SetUdpPayload(buf, len);
    tlv = tlv->GetNext();
    message->SetPath(kmUdpTxUrl);
    message->SetPayload(buffer, utils::LengthOf(buffer, tlv));
    mCoapAgent->Send(*message, NULL, 0, NULL, this);
    mCoapAgent->FreeMessage(message);
    return len;
}

int CommissionerProxy::Recvfrom(void *buf, size_t len, struct sockaddr_in6 *srcAddress)
{
    assert(len <= kSizeMaxPacket);
    uint8_t buffer[kSizeMaxPacket];
    mUdpBuffer.buf      = buf;
    mUdpBuffer.length   = len;
    mUdpBuffer.recieved = false;
    while (!mUdpBuffer.recieved)
    {
        int ret = read(mClientFd, buffer, len);
        if (ret > 0)
        {
            mCoapAgent->Input(buffer, static_cast<uint16_t>(ret), NULL, 0);
            if (mUdpBuffer.recieved)
            {
                *srcAddress = mUdpBuffer.srcAddress;
            }
        }
    }
    return mUdpBuffer.length;
}

CommissionerProxy::~CommissionerProxy()
{
    mCoapAgent->RemoveResource(mUdpRxHandler);
    close(mClientFd);
}

} // namespace BorderRouter
} // namespace ot
