/*
 *  Copyright (c) 2024, The OpenThread Authors.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

#define OTBR_LOG_TAG "INFRAIF"

#include "infra_if.hpp"

#include <ifaddrs.h>
#ifdef __linux__
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#endif
#include <netinet/icmp6.h>
#include <sys/ioctl.h>

#include "utils/socket_utils.hpp"

namespace otbr {

otbrError InfraIf::Dependencies::SetInfraIf(unsigned int                   aInfraIfIndex,
                                            bool                           aIsRunning,
                                            const std::vector<Ip6Address> &aIp6Addresses)
{
    OTBR_UNUSED_VARIABLE(aInfraIfIndex);
    OTBR_UNUSED_VARIABLE(aIsRunning);
    OTBR_UNUSED_VARIABLE(aIp6Addresses);

    return OTBR_ERROR_NONE;
}

InfraIf::InfraIf(Dependencies &aDependencies)
    : mDeps(aDependencies)
    , mInfraIfIndex(0)
{
}

void InfraIf::Init(void)
{
}

void InfraIf::Deinit(void)
{
    mInfraIfIndex = 0;
}

otbrError InfraIf::SetInfraIf(const char *aIfName)
{
    otbrError               error = OTBR_ERROR_NONE;
    std::vector<Ip6Address> addresses;

    VerifyOrExit(aIfName != nullptr && strlen(aIfName) > 0, error = OTBR_ERROR_INVALID_ARGS);
    VerifyOrExit(strnlen(aIfName, IFNAMSIZ) < IFNAMSIZ, error = OTBR_ERROR_INVALID_ARGS);
    strcpy(mInfraIfName, aIfName);

    mInfraIfIndex = if_nametoindex(aIfName);
    VerifyOrExit(mInfraIfIndex != 0, error = OTBR_ERROR_INVALID_STATE);

    addresses = GetAddresses();

    SuccessOrExit(mDeps.SetInfraIf(mInfraIfIndex, IsRunning(addresses), addresses), error = OTBR_ERROR_OPENTHREAD);
exit:
    otbrLogResult(error, "SetInfraIf");

    return error;
}

bool InfraIf::IsRunning(const std::vector<Ip6Address> &aAddrs) const
{
    return mInfraIfIndex ? ((GetFlags() & IFF_RUNNING) && HasLinkLocalAddress(aAddrs)) : false;
}

short InfraIf::GetFlags(void) const
{
    int          sock;
    struct ifreq ifReq;

    sock = SocketWithCloseExec(AF_INET6, SOCK_DGRAM, IPPROTO_IP, kSocketBlock);
    VerifyOrDie(sock != -1, otbrErrorString(OTBR_ERROR_ERRNO));

    memset(&ifReq, 0, sizeof(ifReq));
    strcpy(ifReq.ifr_name, mInfraIfName);

    if (ioctl(sock, SIOCGIFFLAGS, &ifReq) == -1)
    {
        otbrLogCrit("The infra link %s may be lost. Exiting.", mInfraIfName);
        DieNow(otbrErrorString(OTBR_ERROR_ERRNO));
    }

    close(sock);

    return ifReq.ifr_flags;
}

std::vector<Ip6Address> InfraIf::GetAddresses(void)
{
    struct ifaddrs         *ifAddrs = nullptr;
    std::vector<Ip6Address> addrs;

    if (getifaddrs(&ifAddrs) < 0)
    {
        otbrLogCrit("failed to get netif addresses: %s", strerror(errno));
        ExitNow();
    }

    for (struct ifaddrs *addr = ifAddrs; addr != nullptr; addr = addr->ifa_next)
    {
        struct sockaddr_in6 *ip6Addr;

        if (strncmp(addr->ifa_name, mInfraIfName, sizeof(mInfraIfName)) != 0 || addr->ifa_addr == nullptr ||
            addr->ifa_addr->sa_family != AF_INET6)
        {
            continue;
        }

        ip6Addr = reinterpret_cast<sockaddr_in6 *>(addr->ifa_addr);
        addrs.emplace_back(*reinterpret_cast<otIp6Address *>(&ip6Addr->sin6_addr));
    }

    freeifaddrs(ifAddrs);

exit:
    return addrs;
}

bool InfraIf::HasLinkLocalAddress(const std::vector<Ip6Address> &aAddrs)
{
    bool hasLla = false;

    for (const Ip6Address &otAddr : aAddrs)
    {
        if (IN6_IS_ADDR_LINKLOCAL(reinterpret_cast<const in6_addr *>(&otAddr)))
        {
            hasLla = true;
            break;
        }
    }

    return hasLla;
}

} // namespace otbr
