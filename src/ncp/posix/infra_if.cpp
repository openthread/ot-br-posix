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

#ifdef __APPLE__
#define __APPLE_USE_RFC_3542
#endif

#include "infra_if.hpp"

#include <ifaddrs.h>
#ifdef __linux__
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#endif
// clang-format off
#include <netinet/in.h>
#include <netinet/icmp6.h>
// clang-format on
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

otbrError InfraIf::Dependencies::HandleIcmp6Nd(uint32_t, const Ip6Address &, const uint8_t *, uint16_t)
{
    return OTBR_ERROR_NONE;
}

InfraIf::InfraIf(Dependencies &aDependencies)
    : mDeps(aDependencies)
    , mInfraIfIndex(0)
#ifdef __linux__
    , mNetlinkSocket(-1)
#endif
    , mInfraIfIcmp6Socket(-1)
{
}

#ifdef __linux__
// Create a Netlink socket that subscribes to link & addresses events.
int CreateNetlinkSocket(void)
{
    int                sock;
    int                rval;
    struct sockaddr_nl addr;

    sock = SocketWithCloseExec(AF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE, kSocketBlock);
    VerifyOrDie(sock != -1, strerror(errno));

    memset(&addr, 0, sizeof(addr));
    addr.nl_family = AF_NETLINK;
    addr.nl_groups = RTMGRP_LINK | RTMGRP_IPV6_IFADDR;

    rval = bind(sock, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr));
    VerifyOrDie(rval == 0, strerror(errno));

    return sock;
}
#endif // __linux__

void InfraIf::Init(void)
{
#ifdef __linux__
    mNetlinkSocket = CreateNetlinkSocket();
#endif
}

void InfraIf::Deinit(void)
{
#ifdef __linux__
    if (mNetlinkSocket != -1)
    {
        close(mNetlinkSocket);
        mNetlinkSocket = -1;
    }
#endif
    mInfraIfIndex = 0;

    if (mInfraIfIcmp6Socket != -1)
    {
        close(mInfraIfIcmp6Socket);
    }
}

void InfraIf::Process(const MainloopContext &aContext)
{
    VerifyOrExit(mInfraIfIcmp6Socket != -1);
#ifdef __linux__
    VerifyOrExit(mNetlinkSocket != -1);
#endif

    if (FD_ISSET(mInfraIfIcmp6Socket, &aContext.mReadFdSet))
    {
        ReceiveIcmp6Message();
    }
#ifdef __linux__
    if (FD_ISSET(mNetlinkSocket, &aContext.mReadFdSet))
    {
        ReceiveNetlinkMessage();
    }
#endif

exit:
    return;
}

void InfraIf::UpdateFdSet(MainloopContext &aContext)
{
    VerifyOrExit(mInfraIfIcmp6Socket != -1);
#ifdef __linux__
    VerifyOrExit(mNetlinkSocket != -1);
#endif

    FD_SET(mInfraIfIcmp6Socket, &aContext.mReadFdSet);
    aContext.mMaxFd = std::max(aContext.mMaxFd, mInfraIfIcmp6Socket);
#ifdef __linux__
    FD_SET(mNetlinkSocket, &aContext.mReadFdSet);
    aContext.mMaxFd = std::max(aContext.mMaxFd, mNetlinkSocket);
#endif

exit:
    return;
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

    if (mInfraIfIcmp6Socket != -1)
    {
        close(mInfraIfIcmp6Socket);
    }
    mInfraIfIcmp6Socket = CreateIcmp6Socket(aIfName);
    VerifyOrDie(mInfraIfIcmp6Socket != -1, "Failed to create Icmp6 socket!");

    addresses = GetAddresses();

    SuccessOrExit(mDeps.SetInfraIf(mInfraIfIndex, IsRunning(addresses), addresses), error = OTBR_ERROR_OPENTHREAD);
exit:
    otbrLogResult(error, "SetInfraIf");

    return error;
}

otbrError InfraIf::SendIcmp6Nd(uint32_t            aInfraIfIndex,
                               const otIp6Address &aDestAddress,
                               const uint8_t      *aBuffer,
                               uint16_t            aBufferLength)
{
    otbrError error = OTBR_ERROR_NONE;

    struct iovec        iov;
    struct in6_pktinfo *packetInfo;

    int                 hopLimit = 255;
    uint8_t             cmsgBuffer[CMSG_SPACE(sizeof(*packetInfo)) + CMSG_SPACE(sizeof(hopLimit))];
    struct msghdr       msgHeader;
    struct cmsghdr     *cmsgPointer;
    ssize_t             rval;
    struct sockaddr_in6 dest;

    VerifyOrExit(mInfraIfIcmp6Socket >= 0, error = OTBR_ERROR_INVALID_STATE);
    VerifyOrExit(aInfraIfIndex == mInfraIfIndex, error = OTBR_ERROR_DROPPED);

    memset(cmsgBuffer, 0, sizeof(cmsgBuffer));

    // Send the message
    memset(&dest, 0, sizeof(dest));
    dest.sin6_family = AF_INET6;
    memcpy(&dest.sin6_addr, &aDestAddress, sizeof(aDestAddress));
    if (IN6_IS_ADDR_LINKLOCAL(&dest.sin6_addr) || IN6_IS_ADDR_MC_LINKLOCAL(&dest.sin6_addr))
    {
        dest.sin6_scope_id = mInfraIfIndex;
    }

    iov.iov_base = const_cast<uint8_t *>(aBuffer);
    iov.iov_len  = aBufferLength;

    msgHeader.msg_namelen    = sizeof(dest);
    msgHeader.msg_name       = &dest;
    msgHeader.msg_iov        = &iov;
    msgHeader.msg_iovlen     = 1;
    msgHeader.msg_control    = cmsgBuffer;
    msgHeader.msg_controllen = sizeof(cmsgBuffer);

    // Specify the interface.
    cmsgPointer             = CMSG_FIRSTHDR(&msgHeader);
    cmsgPointer->cmsg_level = IPPROTO_IPV6;
    cmsgPointer->cmsg_type  = IPV6_PKTINFO;
    cmsgPointer->cmsg_len   = CMSG_LEN(sizeof(*packetInfo));
    packetInfo              = (struct in6_pktinfo *)CMSG_DATA(cmsgPointer);
    memset(packetInfo, 0, sizeof(*packetInfo));
    packetInfo->ipi6_ifindex = mInfraIfIndex;

    // Per section 6.1.2 of RFC 4861, we need to send the ICMPv6 message with IP Hop Limit 255.
    cmsgPointer             = CMSG_NXTHDR(&msgHeader, cmsgPointer);
    cmsgPointer->cmsg_level = IPPROTO_IPV6;
    cmsgPointer->cmsg_type  = IPV6_HOPLIMIT;
    cmsgPointer->cmsg_len   = CMSG_LEN(sizeof(hopLimit));
    memcpy(CMSG_DATA(cmsgPointer), &hopLimit, sizeof(hopLimit));

    rval = sendmsg(mInfraIfIcmp6Socket, &msgHeader, 0);

    if (rval < 0)
    {
        otbrLogWarning("failed to send ICMPv6 message: %s", strerror(errno));
        ExitNow(error = OTBR_ERROR_ERRNO);
    }

    if (static_cast<size_t>(rval) != iov.iov_len)
    {
        otbrLogWarning("failed to send ICMPv6 message: partially sent");
        ExitNow(error = OTBR_ERROR_ERRNO);
    }

exit:
    return error;
}

int InfraIf::CreateIcmp6Socket(const char *aInfraIfName)
{
    int                 sock;
    int                 rval;
    struct icmp6_filter filter;
    const int           kEnable             = 1;
    const int           kIpv6ChecksumOffset = 2;
    const int           kHopLimit           = 255;

    // Initializes the ICMPv6 socket.
    sock = SocketWithCloseExec(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6, kSocketBlock);
    VerifyOrDie(sock != -1, strerror(errno));

    // Only accept Router Advertisements, Router Solicitations and Neighbor Advertisements.
    ICMP6_FILTER_SETBLOCKALL(&filter);
    ICMP6_FILTER_SETPASS(ND_ROUTER_SOLICIT, &filter);
    ICMP6_FILTER_SETPASS(ND_ROUTER_ADVERT, &filter);
    ICMP6_FILTER_SETPASS(ND_NEIGHBOR_ADVERT, &filter);

    rval = setsockopt(sock, IPPROTO_ICMPV6, ICMP6_FILTER, &filter, sizeof(filter));
    VerifyOrDie(rval == 0, strerror(errno));

    // We want a source address and interface index.
    rval = setsockopt(sock, IPPROTO_IPV6, IPV6_RECVPKTINFO, &kEnable, sizeof(kEnable));
    VerifyOrDie(rval == 0, strerror(errno));

#ifdef __linux__
    rval = setsockopt(sock, IPPROTO_RAW, IPV6_CHECKSUM, &kIpv6ChecksumOffset, sizeof(kIpv6ChecksumOffset));
#else
    rval = setsockopt(sock, IPPROTO_IPV6, IPV6_CHECKSUM, &kIpv6ChecksumOffset, sizeof(kIpv6ChecksumOffset));
#endif
    VerifyOrDie(rval == 0, strerror(errno));

    // We need to be able to reject RAs arriving from off-link.
    rval = setsockopt(sock, IPPROTO_IPV6, IPV6_RECVHOPLIMIT, &kEnable, sizeof(kEnable));
    VerifyOrDie(rval == 0, strerror(errno));

    rval = setsockopt(sock, IPPROTO_IPV6, IPV6_UNICAST_HOPS, &kHopLimit, sizeof(kHopLimit));
    VerifyOrDie(rval == 0, strerror(errno));

    rval = setsockopt(sock, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &kHopLimit, sizeof(kHopLimit));
    VerifyOrDie(rval == 0, strerror(errno));

#ifdef __linux__
    rval = setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, aInfraIfName, strlen(aInfraIfName));
#else  // __NetBSD__ || __FreeBSD__ || __APPLE__
    rval = setsockopt(sock, IPPROTO_IPV6, IPV6_BOUND_IF, aInfraIfName, strlen(aInfraIfName));
#endif // __linux__
    VerifyOrDie(rval == 0, strerror(errno));

    return sock;
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

void InfraIf::ReceiveIcmp6Message(void)
{
    static constexpr size_t kIp6Mtu = 1280;

    otbrError error = OTBR_ERROR_NONE;
    uint8_t   buffer[kIp6Mtu];
    uint16_t  bufferLength;

    ssize_t         rval;
    struct msghdr   msg;
    struct iovec    bufp;
    char            cmsgbuf[128];
    struct cmsghdr *cmh;
    uint32_t        ifIndex  = 0;
    int             hopLimit = -1;

    struct sockaddr_in6 srcAddr;
    struct in6_addr     dstAddr;

    memset(&srcAddr, 0, sizeof(srcAddr));
    memset(&dstAddr, 0, sizeof(dstAddr));

    bufp.iov_base      = buffer;
    bufp.iov_len       = sizeof(buffer);
    msg.msg_iov        = &bufp;
    msg.msg_iovlen     = 1;
    msg.msg_name       = &srcAddr;
    msg.msg_namelen    = sizeof(srcAddr);
    msg.msg_control    = cmsgbuf;
    msg.msg_controllen = sizeof(cmsgbuf);

    rval = recvmsg(mInfraIfIcmp6Socket, &msg, 0);
    if (rval < 0)
    {
        otbrLogWarning("Failed to receive ICMPv6 message: %s", strerror(errno));
        ExitNow(error = OTBR_ERROR_DROPPED);
    }

    bufferLength = static_cast<uint16_t>(rval);

    for (cmh = CMSG_FIRSTHDR(&msg); cmh; cmh = CMSG_NXTHDR(&msg, cmh))
    {
        if (cmh->cmsg_level == IPPROTO_IPV6 && cmh->cmsg_type == IPV6_PKTINFO &&
            cmh->cmsg_len == CMSG_LEN(sizeof(struct in6_pktinfo)))
        {
            const struct in6_pktinfo *pktinfo = reinterpret_cast<struct in6_pktinfo *>(CMSG_DATA(cmh));
            ifIndex                           = pktinfo->ipi6_ifindex;
            dstAddr                           = pktinfo->ipi6_addr;
        }
        else if (cmh->cmsg_level == IPPROTO_IPV6 && cmh->cmsg_type == IPV6_HOPLIMIT &&
                 cmh->cmsg_len == CMSG_LEN(sizeof(int)))
        {
            hopLimit = *(int *)CMSG_DATA(cmh);
        }
    }

    VerifyOrExit(ifIndex == mInfraIfIndex, error = OTBR_ERROR_DROPPED);

    // We currently accept only RA & RS messages for the Border Router and it requires that
    // the hoplimit must be 255 and the source address must be a link-local address.
    VerifyOrExit(hopLimit == 255 && IN6_IS_ADDR_LINKLOCAL(&srcAddr.sin6_addr), error = OTBR_ERROR_DROPPED);

    mDeps.HandleIcmp6Nd(mInfraIfIndex, Ip6Address(reinterpret_cast<otIp6Address &>(srcAddr.sin6_addr)), buffer,
                        bufferLength);

exit:
    otbrLogResult(error, "InfraIf: %s", __FUNCTION__);
}

#ifdef __linux__
void InfraIf::ReceiveNetlinkMessage(void)
{
    const size_t kMaxNetlinkBufSize = 8192;
    ssize_t      len;
    union
    {
        nlmsghdr mHeader;
        uint8_t  mBuffer[kMaxNetlinkBufSize];
    } msgBuffer;

    len = recv(mNetlinkSocket, msgBuffer.mBuffer, sizeof(msgBuffer.mBuffer), /* flags */ 0);
    if (len < 0)
    {
        otbrLogCrit("Failed to receive netlink message: %s", strerror(errno));
        ExitNow();
    }

    for (struct nlmsghdr *header = &msgBuffer.mHeader; NLMSG_OK(header, static_cast<size_t>(len));
         header                  = NLMSG_NEXT(header, len))
    {
        switch (header->nlmsg_type)
        {
        // There are no effective netlink message types to get us notified
        // of interface RUNNING state changes. But addresses events are
        // usually associated with interface state changes.
        case RTM_NEWADDR:
        case RTM_DELADDR:
        case RTM_NEWLINK:
        case RTM_DELLINK:
        {
            std::vector<Ip6Address> addresses = GetAddresses();

            mDeps.SetInfraIf(mInfraIfIndex, IsRunning(addresses), addresses);
            break;
        }
        case NLMSG_ERROR:
        {
            struct nlmsgerr *errMsg = reinterpret_cast<struct nlmsgerr *>(NLMSG_DATA(header));

            OTBR_UNUSED_VARIABLE(errMsg);
            otbrLogWarning("netlink NLMSG_ERROR response: seq=%u, error=%d", header->nlmsg_seq, errMsg->error);
            break;
        }
        default:
            break;
        }
    }

exit:
    return;
}
#endif // __linux__

} // namespace otbr
