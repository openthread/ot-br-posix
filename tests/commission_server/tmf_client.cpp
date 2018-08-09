#include "tmf_client.hpp"
#include "utils.hpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "common/tlv.hpp"

namespace ot {
namespace BorderRouter {

const uint16_t TmfClient::kTmfPort   = 61631;
const char     TmfClient::kDiagUri[] = "d/dg";
const uint8_t  TmfClient::kLociid[]  = {
    0x00, 0x00, 0x00, 0xff, 0xfe, 0x00,
};

TmfClient::TmfClient(CommissionerProxy *proxy)
    : mProxy(proxy)
{
}

ssize_t TmfClient::SendCoap(const uint8_t *aBuffer,
                            uint16_t       aLength,
                            const uint8_t *aIp6,
                            uint16_t       aPort,
                            void *         aContext)
{
    (void)aIp6;
    (void)aPort;
    TmfClient *client = static_cast<TmfClient *>(aContext);
    return client->mProxy->Write(client->mDestAddr, aBuffer, aLength);
}

void TmfClient::HandleCoapResponse(const Coap::Message &aMessage, void *aContext)
{
    uint16_t       length;
    const uint8_t *payload = aMessage.GetPayload(length);
    printf("Get coap response length %u\n", length);
    TmfClient *client     = static_cast<TmfClient *>(aContext);
    size_t     copyLength = utils::min<size_t>(sizeof(client->mResponseBuffer), length);
    memcpy(client->mResponseBuffer, payload, copyLength);
    client->mResponseSize    = copyLength;
    client->mResponseHandled = true;
}

size_t TmfClient::PostCoapAndWaitForResponse(sockaddr_in6 dest, const char *uri, uint8_t *payload, size_t length)
{
    mDestAddr              = dest;
    Coap::Agent *agent     = Coap::Agent::Create(SendCoap, this);
    uint16_t     token     = rand();
    token                  = htons(token);
    Coap::Message *message = agent->NewMessage(Coap::kTypeConfirmable, Coap::kCodePost,
                                               reinterpret_cast<const uint8_t *>(&token), sizeof(token));
    message->SetPath(uri);
    message->SetPayload(payload, length);
    agent->Send(*message, NULL, 0, HandleCoapResponse, this);
    agent->FreeMessage(message);

    int          ret;
    sockaddr_in6 srcAddr;
    mResponseHandled = false;
    uint8_t buffer[kSizeMaxPacket];
    do
    {
        ret = mProxy->Recvfrom(buffer, sizeof(buffer), &srcAddr);
        if (ret > 0)
        {
            printf("Recv size %d\n", ret);
            for (int i = 0; i < ret; i++)
            {
                printf("%02x ", buffer[i]);
            }
            printf("\n");
            agent->Input(buffer, static_cast<uint16_t>(ret), NULL, 0);
            if (mResponseHandled)
            {
                mResponseHandled = false;
                break;
            }
        }
    } while (ret >= 0);

    Coap::Agent::Destroy(agent);
    return mResponseSize;
}

void TmfClient::QueryDiagnosticData(const struct in6_addr &destAddr, uint8_t queryType)
{
    sockaddr_in6 dest;
    dest.sin6_addr = destAddr;
    dest.sin6_port = htons(kTmfPort);
    uint8_t requestBuffer[kSizeMaxPacket];
    Tlv *   tlv = reinterpret_cast<Tlv *>(requestBuffer);
    tlv->SetType(kTypeListTlvType);
    tlv->SetValue(queryType);
    tlv = tlv->GetNext();
    PostCoapAndWaitForResponse(dest, kDiagUri, requestBuffer, utils::LengthOf(requestBuffer, tlv));
    printf("Get coap response\n");
}

static std::vector<struct in6_addr> ParseAddressesTLV(const uint8_t *aBuffer)
{
    std::vector<struct in6_addr> addresses;
    const ot::Tlv *              tlv = reinterpret_cast<const Tlv *>(aBuffer);
    assert(tlv->GetType() == kAddressListType);
    uint16_t payloadLength = tlv->GetLength();
    assert(payloadLength % sizeof(struct in6_addr) == 0);
    size_t                 addrCount = payloadLength / sizeof(struct in6_addr);
    const struct in6_addr *payload   = reinterpret_cast<const struct in6_addr *>(tlv->GetValue());
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

} // namespace BorderRouter
} // namespace ot

int main()
{
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);
    serverAddr.sin_port = htons(FORWARD_PORT);
    ot::BorderRouter::CommissionerProxy p(*reinterpret_cast<sockaddr *>(&serverAddr));
    ot::BorderRouter::TmfClient         client(&p);

    char            targetAddr[] = "fd11:1111:1122:0:ffb5:dfe:2828:1cfa";
    char            nameBuffer[100];
    struct in6_addr destAddr;
    inet_pton(AF_INET6, targetAddr, &destAddr);
    std::vector<struct in6_addr> addrs = client.QueryAllV6Addresses(destAddr);
    for (size_t i = 0; i < addrs.size(); i++)
    {
        inet_ntop(AF_INET6, &addrs[i], nameBuffer, sizeof(nameBuffer));
        printf("Addr %s\n", nameBuffer);
    }
    return 0;
}
