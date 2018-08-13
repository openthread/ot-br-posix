#ifndef OTBR_COMMISSIONER_PROXY_HPP_
#define OTBR_COMMISSIONER_PROXY_HPP_

#include <arpa/inet.h>
#include <set>
#include <sys/socket.h>
#include <sys/types.h>
#include <vector>
#include "agent/coap.hpp"

namespace ot {
namespace BorderRouter {

// for the time being the usecase would be one client only
class CommissionerProxy
{
public:
    CommissionerProxy(const sockaddr &aServerAddr);

    int BindProxyUdpSocket(uint16_t aSourcePort); // source_port=0 for

    int Write(const struct sockaddr_in6 &destAddress, const void *buf, size_t len);
    int Recvfrom(void *buf, size_t len, struct sockaddr_in6 *srcAddress);

    ~CommissionerProxy();

private:
    CommissionerProxy(const CommissionerProxy &);
    CommissionerProxy &operator=(const CommissionerProxy &);

    Coap::Agent *mCoapAgent;
    uint16_t     mCoapToken;

    uint16_t mSourcePort;

    static const char *kmUdpRxUrl;
    static const char *kmUdpTxUrl;
    Coap::Resource     mUdpRxHandler;

    int mClientFd;

    struct UdpBuffer
    {
        void *       buf;
        size_t       length;
        sockaddr_in6 srcAddress;
        bool         recieved;
    } mUdpBuffer;

    static void HandleUDPRx(const Coap::Resource &aResource,
                            const Coap::Message & aPost,
                            Coap::Message &       aResponse,
                            const uint8_t *       aIp6,
                            uint16_t              aPort,
                            void *                aContext);

    static ssize_t SendCoapProxy(const uint8_t *aBuffer,
                                 uint16_t       aLength,
                                 const uint8_t *aIp6,
                                 uint16_t       aPort,
                                 void *         aContext);
};

} // namespace BorderRouter
} // namespace ot

#endif
