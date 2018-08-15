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
    CommissionerProxy();

    int SendTo(const struct sockaddr_in6 &aDestAddr, const void *aBuf, size_t aLength);
    int RecvFrom(void *aBuf, size_t aLength, struct sockaddr_in6 &srcAddr);

    ~CommissionerProxy();

private:
    CommissionerProxy(const CommissionerProxy &);
    CommissionerProxy &operator=(const CommissionerProxy &);

    int mClientFd;
};

} // namespace BorderRouter
} // namespace ot

#endif
