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

#ifdef __linux__

#define OTBR_LOG_TAG "NETIF"

#include "netif.hpp"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <chrono>

#include "common/code_utils.hpp"
#include "common/logging.hpp"
#include "common/types.hpp"
#include "utils/socket_utils.hpp"

#ifndef OTBR_POSIX_TUN_DEVICE
#define OTBR_POSIX_TUN_DEVICE "/dev/net/tun"
#endif

namespace otbr {

static struct rtattr *AddRtAttr(nlmsghdr *aHeader, uint32_t aMaxLen, uint8_t aType, const void *aData, uint8_t aLen)
{
    uint8_t len = RTA_LENGTH(aLen);
    rtattr *rta;

    assert(NLMSG_ALIGN(aHeader->nlmsg_len) + RTA_ALIGN(len) <= aMaxLen);
    OTBR_UNUSED_VARIABLE(aMaxLen);

    rta           = reinterpret_cast<rtattr *>(reinterpret_cast<char *>(aHeader) + NLMSG_ALIGN((aHeader)->nlmsg_len));
    rta->rta_type = aType;
    rta->rta_len  = len;
    if (aLen)
    {
        memcpy(RTA_DATA(rta), aData, aLen);
    }
    aHeader->nlmsg_len = NLMSG_ALIGN(aHeader->nlmsg_len) + RTA_ALIGN(len);

    return rta;
}

otbrError Netif::CreateTunDevice(const std::string &aInterfaceName)
{
    ifreq     ifr;
    otbrError error = OTBR_ERROR_NONE;

    VerifyOrExit(aInterfaceName.size() < IFNAMSIZ, error = OTBR_ERROR_INVALID_ARGS);

    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
    if (aInterfaceName.size() > 0)
    {
        strncpy(ifr.ifr_name, aInterfaceName.c_str(), aInterfaceName.size());
    }
    else
    {
        strncpy(ifr.ifr_name, "wpan%d", IFNAMSIZ);
    }

    mTunFd = open(OTBR_POSIX_TUN_DEVICE, O_RDWR | O_CLOEXEC | O_NONBLOCK);
    VerifyOrExit(mTunFd >= 0, error = OTBR_ERROR_ERRNO);

    VerifyOrExit(ioctl(mTunFd, TUNSETIFF, &ifr) == 0, error = OTBR_ERROR_ERRNO);

    mNetifName.assign(ifr.ifr_name, strlen(ifr.ifr_name));
    otbrLogInfo("Netif name: %s", mNetifName.c_str());

    VerifyOrExit(ioctl(mTunFd, TUNSETLINK, ARPHRD_NONE) == 0, error = OTBR_ERROR_ERRNO);

    ifr.ifr_mtu = static_cast<int>(kIp6Mtu);
    VerifyOrExit(ioctl(mIpFd, SIOCSIFMTU, &ifr) == 0, error = OTBR_ERROR_ERRNO);

exit:
    return error;
}

otbrError Netif::InitNetlink(void)
{
    otbrError error = OTBR_ERROR_NONE;

    mNetlinkFd = SocketWithCloseExec(AF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE, kSocketNonBlock);
    VerifyOrExit(mNetlinkFd >= 0, error = OTBR_ERROR_ERRNO);

#if defined(SOL_NETLINK)
    {
        int enable = 1;

#if defined(NETLINK_EXT_ACK)
        if (setsockopt(mNetlinkFd, SOL_NETLINK, NETLINK_EXT_ACK, &enable, sizeof(enable)) != 0)
        {
            otbrLogWarning("Failed to enable NETLINK_EXT_ACK: %s", strerror(errno));
        }
#endif
#if defined(NETLINK_CAP_ACK)
        if (setsockopt(mNetlinkFd, SOL_NETLINK, NETLINK_CAP_ACK, &enable, sizeof(enable)) != 0)
        {
            otbrLogWarning("Failed to enable NETLINK_CAP_ACK: %s", strerror(errno));
        }
#endif
    }
#endif

    {
        sockaddr_nl sa;

        memset(&sa, 0, sizeof(sa));
        sa.nl_family = AF_NETLINK;
        sa.nl_groups = RTMGRP_LINK | RTMGRP_IPV6_IFADDR;
        VerifyOrExit(bind(mNetlinkFd, reinterpret_cast<sockaddr *>(&sa), sizeof(sa)) == 0, error = OTBR_ERROR_ERRNO);
    }

exit:
    return error;
}

void Netif::PlatformSpecificInit(void)
{
    SetAddrGenModeToNone();
}

void Netif::SetAddrGenModeToNone(void)
{
    struct
    {
        nlmsghdr  nh;
        ifinfomsg ifi;
        char      buf[512];
    } req;

    const uint8_t mode = IN6_ADDR_GEN_MODE_NONE;

    memset(&req, 0, sizeof(req));

    req.nh.nlmsg_len   = NLMSG_LENGTH(sizeof(ifinfomsg));
    req.nh.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
    req.nh.nlmsg_type  = RTM_NEWLINK;
    req.nh.nlmsg_pid   = 0;
    req.nh.nlmsg_seq   = ++mNetlinkSequence;

    req.ifi.ifi_index  = static_cast<int>(mNetifIndex);
    req.ifi.ifi_change = 0xffffffff;
    req.ifi.ifi_flags  = IFF_MULTICAST | IFF_NOARP;

    {
        rtattr *afSpec           = AddRtAttr(&req.nh, sizeof(req), IFLA_AF_SPEC, 0, 0);
        rtattr *afInet6          = AddRtAttr(&req.nh, sizeof(req), AF_INET6, 0, 0);
        rtattr *inet6AddrGenMode = AddRtAttr(&req.nh, sizeof(req), IFLA_INET6_ADDR_GEN_MODE, &mode, sizeof(mode));

        afInet6->rta_len += inet6AddrGenMode->rta_len;
        afSpec->rta_len += afInet6->rta_len;
    }

    if (send(mNetlinkFd, &req, req.nh.nlmsg_len, 0) != -1)
    {
        otbrLogInfo("Sent request#%u to set addr_gen_mode to %d", mNetlinkSequence, mode);
    }
    else
    {
        otbrLogWarning("Failed to send request#%u to set addr_gen_mode to %d", mNetlinkSequence, mode);
    }
}

otbrError Netif::ProcessUnicastAddressChange(const Ip6AddressInfo &aAddressInfo, bool aIsAdded)
{
    constexpr int kNetlinkAckTimeoutMs = 1000;

    struct
    {
        nlmsghdr  nh;
        ifaddrmsg ifa;
        char      buf[512];
    } req;
    uint32_t  seq;
    otbrError error = OTBR_ERROR_NONE;
    auto      deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(static_cast<int>(kNetlinkAckTimeoutMs));

    assert(mIpFd >= 0);
    assert(mNetlinkFd >= 0);
    memset(&req, 0, sizeof(req));

    req.nh.nlmsg_len   = NLMSG_LENGTH(sizeof(ifaddrmsg));
    req.nh.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK | (aIsAdded ? (NLM_F_CREATE | NLM_F_EXCL) : 0);
    req.nh.nlmsg_type  = aIsAdded ? RTM_NEWADDR : RTM_DELADDR;
    req.nh.nlmsg_pid   = 0;
    req.nh.nlmsg_seq   = ++mNetlinkSequence;
    seq                = req.nh.nlmsg_seq;

    req.ifa.ifa_family    = AF_INET6;
    req.ifa.ifa_prefixlen = aAddressInfo.mPrefixLength;
    req.ifa.ifa_flags     = IFA_F_NODAD;
    req.ifa.ifa_scope     = aAddressInfo.mScope;
    req.ifa.ifa_index     = mNetifIndex;

    AddRtAttr(&req.nh, sizeof(req), IFA_LOCAL, &aAddressInfo.mAddress, sizeof(aAddressInfo.mAddress));

    if (!aAddressInfo.mPreferred || aAddressInfo.mMeshLocal)
    {
        ifa_cacheinfo cacheinfo;

        memset(&cacheinfo, 0, sizeof(cacheinfo));
        cacheinfo.ifa_valid = UINT32_MAX;

        AddRtAttr(&req.nh, sizeof(req), IFA_CACHEINFO, &cacheinfo, sizeof(cacheinfo));
    }

    VerifyOrExit(send(mNetlinkFd, &req, req.nh.nlmsg_len, 0) != -1, error = OTBR_ERROR_ERRNO);

    otbrLogInfo("Sent request#%u to %s %s/%u", seq, (aIsAdded ? "add" : "remove"),
                Ip6Address(aAddressInfo.mAddress).ToString().c_str(), aAddressInfo.mPrefixLength);

    for (;;)
    {
        struct pollfd pfd;
        int           pollResult;
        auto          now = std::chrono::steady_clock::now();
        int           pollTimeoutMs;

        if (now >= deadline)
        {
            errno = ETIMEDOUT;
            ExitNow(error = OTBR_ERROR_ERRNO);
        }

        pollTimeoutMs = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count());

        pfd.fd      = mNetlinkFd;
        pfd.events  = POLLIN;
        pfd.revents = 0;

        pollResult = poll(&pfd, 1, pollTimeoutMs);
        if (pollResult == 0)
        {
            errno = ETIMEDOUT;
            ExitNow(error = OTBR_ERROR_ERRNO);
        }
        if (pollResult < 0 && errno == EINTR)
        {
            continue;
        }
        VerifyOrExit(pollResult >= 0, error = OTBR_ERROR_ERRNO);

        if ((pfd.revents & POLLIN) != 0)
        {
            union
            {
                nlmsghdr mHeader;
                uint8_t  mBuffer[2048];
            } response;
            ssize_t length;

            length = recv(mNetlinkFd, response.mBuffer, sizeof(response.mBuffer), /* flags */ 0);
            if (length < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK))
            {
                continue;
            }
            VerifyOrExit(length >= 0, error = OTBR_ERROR_ERRNO);

            for (nlmsghdr *header = &response.mHeader; NLMSG_OK(header, static_cast<size_t>(length));
                 header           = NLMSG_NEXT(header, length))
            {
                if (header->nlmsg_seq != seq)
                {
                    continue;
                }

                VerifyOrExit(header->nlmsg_type == NLMSG_ERROR, errno = EPROTO; error = OTBR_ERROR_ERRNO);
                VerifyOrExit(header->nlmsg_len >= NLMSG_LENGTH(sizeof(nlmsgerr)), errno = EBADMSG;
                             error = OTBR_ERROR_ERRNO);

                {
                    nlmsgerr *errMsg    = reinterpret_cast<nlmsgerr *>(NLMSG_DATA(header));
                    int       kernelErr = (errMsg->error < 0) ? -errMsg->error : errMsg->error;

                    if (kernelErr == 0 || (aIsAdded && kernelErr == EEXIST) ||
                        (!aIsAdded && (kernelErr == ENOENT || kernelErr == EADDRNOTAVAIL)))
                    {
                        ExitNow(error = OTBR_ERROR_NONE);
                    }

                    errno = kernelErr;
                    ExitNow(error = OTBR_ERROR_ERRNO);
                }
            }
        }
        else
        {
            errno = EIO;
            ExitNow(error = OTBR_ERROR_ERRNO);
        }
    }

exit:
    if (error != OTBR_ERROR_NONE)
    {
        otbrLogWarning("Failed request#%u to %s %s/%u: %s", seq, (aIsAdded ? "add" : "remove"),
                       Ip6Address(aAddressInfo.mAddress).ToString().c_str(), aAddressInfo.mPrefixLength,
                       strerror(errno));
    }

    return error;
}

} // namespace otbr

#endif // __linux__
