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
#include <sys/ioctl.h>
#include <unistd.h>

#include <algorithm>

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

void Netif::ProcessPendingNetlinkTx(void)
{
    VerifyOrExit(mNetlinkFd >= 0);

    while (!mPendingNetlinkTxQueue.IsEmpty())
    {
        const PendingNetlinkTxQueue::Entry &entry = mPendingNetlinkTxQueue.Front();
        ssize_t                             rval;

        do
        {
            rval = send(mNetlinkFd, entry.mBuffer, entry.mLength, 0);
        } while (rval < 0 && errno == EINTR);

        if (rval < 0)
        {
            int      savedErrno = errno;
            uint32_t seq        = 0;

            if (savedErrno == EAGAIN || savedErrno == EWOULDBLOCK)
            {
                ExitNow();
            }

            if (entry.mLength >= sizeof(nlmsghdr))
            {
                seq = reinterpret_cast<const nlmsghdr *>(entry.mBuffer)->nlmsg_seq;
            }

            otbrLogWarning("Failed to send queued netlink message#%u: %s", seq, strerror(savedErrno));
            mPendingNetlinkTxQueue.PopFront();
            HandleNetlinkAck(seq, savedErrno);
            continue;
        }
        else if (entry.mLength >= sizeof(nlmsghdr))
        {
            uint32_t seq = reinterpret_cast<const nlmsghdr *>(entry.mBuffer)->nlmsg_seq;
            auto     it  = std::find_if(mPendingNetlinkRequests.begin(), mPendingNetlinkRequests.end(),
                                        [seq](const PendingNetlinkRequest &aPending) { return aPending.mSequence == seq; });

            if (it != mPendingNetlinkRequests.end())
            {
                it->mExpireTime =
                    std::chrono::steady_clock::now() + std::chrono::milliseconds(PendingNetlinkRequest::kTimeoutMs);
            }
        }

        mPendingNetlinkTxQueue.PopFront();
    }

exit:
    return;
}

otbrError Netif::SendNetlinkMessage(const void *aBuffer, size_t aLength)
{
    otbrError error = OTBR_ERROR_NONE;

    VerifyOrExit(mNetlinkFd >= 0, error = OTBR_ERROR_INVALID_STATE);
    VerifyOrExit(aLength <= PendingNetlinkTxQueue::kMaxBufferSize, error = OTBR_ERROR_INVALID_ARGS);

    ProcessPendingNetlinkTx();

    if (mPendingNetlinkTxQueue.IsEmpty())
    {
        ssize_t rval;

        do
        {
            rval = send(mNetlinkFd, aBuffer, aLength, 0);
        } while (rval < 0 && errno == EINTR);

        if (rval >= 0)
        {
            ExitNow();
        }

        if (errno != EAGAIN && errno != EWOULDBLOCK)
        {
            ExitNow(error = OTBR_ERROR_ERRNO);
        }
    }

    VerifyOrExit(!mPendingNetlinkTxQueue.IsFull(), errno = ENOBUFS, error = OTBR_ERROR_ERRNO);

    mPendingNetlinkTxQueue.PushBack(aBuffer, aLength);

exit:
    return error;
}

void Netif::RecordPendingNetlinkRequest(uint32_t              aSeq,
                                        UnicastAddressAction  aAction,
                                        const Ip6AddressInfo &aAddressInfo,
                                        const void           *aPayload,
                                        size_t                aPayloadLen)
{
    static constexpr size_t kMaxPendingNetlinkRequests = 16;
    PendingNetlinkRequest   entry{};

    for (auto it = mPendingNetlinkRequests.begin(); it != mPendingNetlinkRequests.end();)
    {
        if (memcmp(&it->mAddressInfo.mAddress, &aAddressInfo.mAddress, sizeof(otIp6Address)) == 0)
        {
            it = mPendingNetlinkRequests.erase(it);
        }
        else
        {
            ++it;
        }
    }

    if (mPendingNetlinkRequests.size() >= kMaxPendingNetlinkRequests)
    {
        otbrLogWarning("Pending netlink request queue full, evicting request#%u",
                       mPendingNetlinkRequests.front().mSequence);
        mPendingNetlinkRequests.erase(mPendingNetlinkRequests.begin());
    }

    entry.mSequence   = aSeq;
    entry.mAction     = aAction;
    entry.mRetryCount = 0;
    entry.mExpireTime = std::chrono::steady_clock::now() + std::chrono::milliseconds(PendingNetlinkRequest::kTimeoutMs);
    entry.mAddressInfo = aAddressInfo;

    if (aPayloadLen <= sizeof(entry.mPayload))
    {
        entry.mPayloadLen = static_cast<uint16_t>(aPayloadLen);
        memcpy(entry.mPayload, aPayload, aPayloadLen);
    }
    else
    {
        otbrLogWarning("Netlink request payload too large to record (%zu bytes)", aPayloadLen);
    }

    mPendingNetlinkRequests.push_back(entry);
}

void Netif::HandleNetlinkAck(uint32_t aSeq, int aKernelErr)
{
    static constexpr uint8_t kMaxNetlinkRequestRetries = 3;

    auto it = std::find_if(mPendingNetlinkRequests.begin(), mPendingNetlinkRequests.end(),
                           [aSeq](const PendingNetlinkRequest &aPending) { return aPending.mSequence == aSeq; });

    VerifyOrExit(it != mPendingNetlinkRequests.end());

    {
        PendingNetlinkRequest &pending = *it;
        bool isSuccess = (aKernelErr == 0 || (pending.mAction == UnicastAddressAction::kAdd && aKernelErr == EEXIST) ||
                          (pending.mAction == UnicastAddressAction::kRemove &&
                           (aKernelErr == ENOENT || aKernelErr == EADDRNOTAVAIL)));

        if (isSuccess)
        {
            if (aKernelErr != 0)
            {
                otbrLogInfo("Netlink request#%u (%s) completed with idempotent code %d (%s), treating as success", aSeq,
                            ActionToString(pending.mAction), aKernelErr, strerror(aKernelErr));
            }
            CommitUnicastAddressChange(pending);
            mPendingNetlinkRequests.erase(it);
            ExitNow();
        }

        if ((aKernelErr == EBUSY || aKernelErr == EAGAIN) && pending.mRetryCount < kMaxNetlinkRequestRetries &&
            pending.mPayloadLen >= sizeof(nlmsghdr))
        {
            uint32_t  oldSeq = pending.mSequence;
            uint32_t  newSeq = ++mNetlinkSequence;
            uint8_t   payload[PendingNetlinkRequest::kMaxPayloadLen];
            uint16_t  payloadLen = pending.mPayloadLen;
            nlmsghdr *reqHdr     = reinterpret_cast<nlmsghdr *>(pending.mPayload);

            pending.mRetryCount++;
            pending.mSequence = newSeq;
            pending.mExpireTime =
                std::chrono::steady_clock::now() + std::chrono::milliseconds(PendingNetlinkRequest::kTimeoutMs);

            reqHdr->nlmsg_seq = newSeq;

            memcpy(payload, pending.mPayload, payloadLen);

            otbrLogInfo("Retrying netlink request#%u as #%u (attempt %u/%u) after transient error %d (%s)", oldSeq,
                        newSeq, pending.mRetryCount, kMaxNetlinkRequestRetries, aKernelErr, strerror(aKernelErr));

            if (SendNetlinkMessage(payload, payloadLen) != OTBR_ERROR_NONE)
            {
                otbrLogWarning("Failed to re-send netlink request#%u: %s", newSeq, strerror(errno));
                auto retryIt = std::find_if(
                    mPendingNetlinkRequests.begin(), mPendingNetlinkRequests.end(),
                    [newSeq](const PendingNetlinkRequest &aPending) { return aPending.mSequence == newSeq; });
                if (retryIt != mPendingNetlinkRequests.end())
                {
                    mPendingNetlinkRequests.erase(retryIt);
                }
            }
            ExitNow();
        }

        otbrLogWarning("Failed to %s address %s/%u (request#%u): %s", ActionToString(pending.mAction),
                       Ip6Address(pending.mAddressInfo.mAddress).ToString().c_str(), pending.mAddressInfo.mPrefixLength,
                       aSeq, strerror(aKernelErr));

        mPendingNetlinkRequests.erase(it);
    }

exit:
    return;
}

void Netif::ProcessNetlinkEvent(void)
{
    union
    {
        nlmsghdr mHeader;
        uint8_t  mBuffer[8192];
    } response;

    VerifyOrExit(mNetlinkFd >= 0);

    for (;;)
    {
        ssize_t length = recv(mNetlinkFd, response.mBuffer, sizeof(response.mBuffer), MSG_DONTWAIT | MSG_TRUNC);
        ssize_t msgLen;

        if (length <= 0)
        {
            if (length < 0 && errno == EINTR)
            {
                continue;
            }
            break;
        }

        if (length > static_cast<ssize_t>(sizeof(response.mBuffer)))
        {
            otbrLogWarning("Netlink response truncated (received %zd, buffer size %zu)", length,
                           sizeof(response.mBuffer));
        }

        msgLen = std::min(length, static_cast<ssize_t>(sizeof(response.mBuffer)));

        for (nlmsghdr *header = &response.mHeader;
             msgLen >= static_cast<ssize_t>(sizeof(nlmsghdr)) && NLMSG_OK(header, static_cast<size_t>(msgLen));
             header = NLMSG_NEXT(header, msgLen))
        {
            if (header->nlmsg_type == NLMSG_ERROR && header->nlmsg_len >= NLMSG_LENGTH(sizeof(int)))
            {
                const int *err       = reinterpret_cast<const int *>(NLMSG_DATA(header));
                int        kernelErr = (*err < 0) ? -*err : *err;

                HandleNetlinkAck(header->nlmsg_seq, kernelErr);
            }
        }
    }

    PruneExpiredNetlinkRequests();

exit:
    return;
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

    if (SendNetlinkMessage(&req, req.nh.nlmsg_len) == OTBR_ERROR_NONE)
    {
        otbrLogInfo("Sent request#%u to set addr_gen_mode to %d", mNetlinkSequence, mode);
    }
    else
    {
        otbrLogWarning("Failed to send request#%u to set addr_gen_mode to %d", mNetlinkSequence, mode);
    }
}

std::vector<otbrError> Netif::ProcessUnicastAddressChanges(const std::vector<UnicastAddressChange> &aChanges)
{
    std::vector<otbrError> errors(aChanges.size(), OTBR_ERROR_ERRNO);

    VerifyOrExit(mNetlinkFd >= 0 && mIpFd >= 0, errors.assign(aChanges.size(), OTBR_ERROR_INVALID_STATE));

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

        otbrError sendError = SendNetlinkMessage(&req, req.nh.nlmsg_len);
        if (sendError != OTBR_ERROR_NONE)
        {
            otbrLogWarning("Failed to send request#%u to %s %s/%u: %s", req.nh.nlmsg_seq,
                           ActionToString(change.mAction), Ip6Address(addrInfo.mAddress).ToString().c_str(),
                           addrInfo.mPrefixLength, strerror(errno));
            errors[i] = sendError;
            continue;
        }

        otbrLogInfo("Sent request#%u to %s %s/%u", req.nh.nlmsg_seq, ActionToString(change.mAction),
                    Ip6Address(addrInfo.mAddress).ToString().c_str(), addrInfo.mPrefixLength);

        RecordPendingNetlinkRequest(req.nh.nlmsg_seq, change.mAction, change.mAddressInfo, &req, req.nh.nlmsg_len);
        errors[i] = OTBR_ERROR_NONE;
    }

exit:
    return errors;
}

} // namespace otbr

#endif // __linux__
