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

#include <algorithm>
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
        // Do not subscribe to multicast groups because mNetlinkFd is only used for synchronous
        // request/ACK communication and is not polled for asynchronous link or address events.
        sa.nl_groups = 0;
        VerifyOrExit(bind(mNetlinkFd, reinterpret_cast<sockaddr *>(&sa), sizeof(sa)) == 0, error = OTBR_ERROR_ERRNO);
    }

exit:
    return error;
}

void Netif::PlatformSpecificInit(void)
{
    SetAddrGenModeToNone();
}

static ssize_t SendNetlinkMessage(int aFd, const void *aBuffer, size_t aLength)
{
    ssize_t sendLen;

    while ((sendLen = send(aFd, aBuffer, aLength, 0)) == -1)
    {
        if (errno == EINTR)
        {
            continue;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            struct pollfd pfd;
            int           pollResult;

            pfd.fd      = aFd;
            pfd.events  = POLLOUT;
            pfd.revents = 0;

            while ((pollResult = poll(&pfd, 1, 10)) == -1 && errno == EINTR)
            {
                continue;
            }

            if (pollResult > 0 && (pfd.revents & POLLOUT))
            {
                continue;
            }
        }
        break;
    }

    return sendLen;
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
    req.nh.nlmsg_flags = NLM_F_REQUEST;
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

    if (SendNetlinkMessage(mNetlinkFd, &req, req.nh.nlmsg_len) != -1)
    {
        otbrLogInfo("Sent request#%u to set addr_gen_mode to %d", mNetlinkSequence, mode);
    }
    else
    {
        otbrLogWarning("Failed to send request#%u to set addr_gen_mode to %d", mNetlinkSequence, mode);
    }
}

static const char *ActionToString(Netif::UnicastAddressAction aAction)
{
    const char *str;

    switch (aAction)
    {
    case Netif::UnicastAddressAction::kAdd:
        str = "add";
        break;
    case Netif::UnicastAddressAction::kRemove:
        str = "remove";
        break;
    case Netif::UnicastAddressAction::kReplace:
        str = "replace";
        break;
    default:
        str = "unknown";
        break;
    }

    return str;
}

static bool ParseAckResponse(const nlmsghdr &aHeader, Netif::UnicastAddressAction aAction, int &aKernelErr)
{
    bool isSuccess = false;

    VerifyOrExit(aHeader.nlmsg_type == NLMSG_ERROR, aKernelErr = EPROTO);
    VerifyOrExit(aHeader.nlmsg_len >= NLMSG_LENGTH(sizeof(int)), aKernelErr = EBADMSG);

    {
        const int *err = reinterpret_cast<const int *>(NLMSG_DATA(&aHeader));

        aKernelErr = (*err < 0) ? -*err : *err;
    }

    isSuccess =
        (aKernelErr == 0 || (aAction == Netif::UnicastAddressAction::kAdd && aKernelErr == EEXIST) ||
         (aAction == Netif::UnicastAddressAction::kRemove && (aKernelErr == ENOENT || aKernelErr == EADDRNOTAVAIL)));

exit:
    return isSuccess;
}

std::vector<otbrError> Netif::ProcessUnicastAddressChanges(const std::vector<UnicastAddressChange> &aChanges)
{
    constexpr int kNetlinkAckTimeoutMs = 50;

    struct PendingAck
    {
        uint32_t mSequence;
        size_t   mChangeIndex;
        bool     mIsDone;
    };

    std::vector<otbrError>  errors(aChanges.size(), OTBR_ERROR_ERRNO);
    std::vector<PendingAck> pendingAcks;
    size_t                  pendingAckCount = 0;

    VerifyOrExit(mNetlinkFd >= 0 && mIpFd >= 0, errors.assign(aChanges.size(), OTBR_ERROR_INVALID_STATE));

    pendingAcks.reserve(aChanges.size());

    for (size_t i = 0; i < aChanges.size(); ++i)
    {
        const UnicastAddressChange &change   = aChanges[i];
        const Ip6AddressInfo       &addrInfo = change.mAddressInfo;
        struct
        {
            nlmsghdr  nh;
            ifaddrmsg ifa;
            char      buf[512];
        } req;

        memset(&req, 0, sizeof(req));

        req.nh.nlmsg_len = NLMSG_LENGTH(sizeof(ifaddrmsg));
        req.nh.nlmsg_pid = 0;
        req.nh.nlmsg_seq = ++mNetlinkSequence;

        switch (change.mAction)
        {
        case UnicastAddressAction::kAdd:
            req.nh.nlmsg_type  = RTM_NEWADDR;
            req.nh.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK | NLM_F_CREATE | NLM_F_EXCL;
            break;
        case UnicastAddressAction::kRemove:
            req.nh.nlmsg_type  = RTM_DELADDR;
            req.nh.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
            break;
        case UnicastAddressAction::kReplace:
            req.nh.nlmsg_type  = RTM_NEWADDR;
            req.nh.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK | NLM_F_REPLACE;
            break;
        }

        req.ifa.ifa_family    = AF_INET6;
        req.ifa.ifa_prefixlen = addrInfo.mPrefixLength;
        req.ifa.ifa_flags     = IFA_F_NODAD;
        req.ifa.ifa_scope     = addrInfo.mScope;
        req.ifa.ifa_index     = mNetifIndex;

        AddRtAttr(&req.nh, sizeof(req), IFA_LOCAL, &addrInfo.mAddress, sizeof(addrInfo.mAddress));

        if (change.mAction != UnicastAddressAction::kRemove)
        {
            ifa_cacheinfo cacheinfo;

            memset(&cacheinfo, 0, sizeof(cacheinfo));
            cacheinfo.ifa_valid    = UINT32_MAX;
            cacheinfo.ifa_prefered = (addrInfo.mPreferred && !addrInfo.mMeshLocal) ? UINT32_MAX : 0;

            AddRtAttr(&req.nh, sizeof(req), IFA_CACHEINFO, &cacheinfo, sizeof(cacheinfo));
        }

        if (SendNetlinkMessage(mNetlinkFd, &req, req.nh.nlmsg_len) == -1)
        {
            otbrLogWarning("Failed to send request#%u to %s %s/%u: %s", req.nh.nlmsg_seq,
                           ActionToString(change.mAction), Ip6Address(addrInfo.mAddress).ToString().c_str(),
                           addrInfo.mPrefixLength, strerror(errno));
            continue;
        }

        otbrLogInfo("Sent request#%u to %s %s/%u", req.nh.nlmsg_seq, ActionToString(change.mAction),
                    Ip6Address(addrInfo.mAddress).ToString().c_str(), addrInfo.mPrefixLength);

        pendingAcks.push_back({req.nh.nlmsg_seq, i, false});
        ++pendingAckCount;
    }

    VerifyOrExit(pendingAckCount > 0);

    {
        auto deadline =
            std::chrono::steady_clock::now() + std::chrono::milliseconds(static_cast<int>(kNetlinkAckTimeoutMs));
        int failureErrno = ETIMEDOUT;
        union
        {
            nlmsghdr mHeader;
            uint8_t  mBuffer[8192];
        } response;

        while (pendingAckCount > 0)
        {
            auto now = std::chrono::steady_clock::now();
            if (now >= deadline)
            {
                break;
            }

            int pollTimeoutMs = std::max(
                static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count()), 1);

            struct pollfd pfd;
            pfd.fd      = mNetlinkFd;
            pfd.events  = POLLIN;
            pfd.revents = 0;

            int pollResult = poll(&pfd, 1, pollTimeoutMs);
            if (pollResult == 0)
            {
                break;
            }

            if (pollResult < 0)
            {
                if (errno == EINTR)
                {
                    continue;
                }
                failureErrno = errno;
                break;
            }

            if ((pfd.revents & POLLIN) == 0)
            {
                failureErrno = EIO;
                break;
            }

            ssize_t length = recv(mNetlinkFd, response.mBuffer, sizeof(response.mBuffer), MSG_TRUNC);
            if (length <= 0)
            {
                if (length < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK))
                {
                    continue;
                }
                failureErrno = (length < 0) ? errno : EIO;
                break;
            }

            if (length > static_cast<ssize_t>(sizeof(response.mBuffer)))
            {
                otbrLogWarning("Netlink response truncated (received %zd, buffer size %zu)", length,
                               sizeof(response.mBuffer));
            }

            ssize_t msgLen = std::min(length, static_cast<ssize_t>(sizeof(response.mBuffer)));

            for (nlmsghdr *header = &response.mHeader;
                 msgLen >= static_cast<ssize_t>(sizeof(nlmsghdr)) && NLMSG_OK(header, static_cast<size_t>(msgLen));
                 header = NLMSG_NEXT(header, msgLen))
            {
                auto it = std::find_if(pendingAcks.begin(), pendingAcks.end(), [&](const PendingAck &aPending) {
                    return !aPending.mIsDone && aPending.mSequence == header->nlmsg_seq;
                });

                if (it != pendingAcks.end())
                {
                    const UnicastAddressChange &change    = aChanges[it->mChangeIndex];
                    int                         kernelErr = 0;

                    if (ParseAckResponse(*header, change.mAction, kernelErr))
                    {
                        errors[it->mChangeIndex] = OTBR_ERROR_NONE;
                    }
                    else
                    {
                        otbrLogWarning("Failed to %s address %s/%u: %s", ActionToString(change.mAction),
                                       Ip6Address(change.mAddressInfo.mAddress).ToString().c_str(),
                                       change.mAddressInfo.mPrefixLength, strerror(kernelErr));
                    }

                    it->mIsDone = true;
                    --pendingAckCount;
                }
            }
        }

        for (const PendingAck &pendingAck : pendingAcks)
        {
            if (!pendingAck.mIsDone)
            {
                const UnicastAddressChange &change = aChanges[pendingAck.mChangeIndex];

                otbrLogWarning("Failed to %s address %s/%u: %s", ActionToString(change.mAction),
                               Ip6Address(change.mAddressInfo.mAddress).ToString().c_str(),
                               change.mAddressInfo.mPrefixLength, strerror(failureErrno));
            }
        }
    }

exit:
    return errors;
}

} // namespace otbr

#endif // __linux__
