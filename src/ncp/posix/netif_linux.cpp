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

#include <openthread-br/otbr-posix-config.h>

#include <assert.h>
#include <fcntl.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <sys/ioctl.h>

#include "common/code_utils.hpp"
#include "common/logging.hpp"

#ifndef OTBR_POSIX_TUN_DEVICE
#define OTBR_POSIX_TUN_DEVICE "/dev/net/tun"
#endif

namespace otbr {
namespace Posix {

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
static struct rtattr  *AddRtAttr(nlmsghdr *aHeader, uint32_t aMaxLen, uint8_t aType, const void *aData, uint8_t aLen)
{
    uint8_t len = RTA_LENGTH(aLen);
    rtattr *rta;

    assert(NLMSG_ALIGN(aHeader->nlmsg_len) + RTA_ALIGN(len) <= aMaxLen);
    OTBR_UNUSED_VARIABLE(aMaxLen);

    rta           = (rtattr *)((char *)(aHeader) + NLMSG_ALIGN((aHeader)->nlmsg_len));
    rta->rta_type = aType;
    rta->rta_len  = len;
    if (aLen)
    {
        memcpy(RTA_DATA(rta), aData, aLen);
    }
    aHeader->nlmsg_len = NLMSG_ALIGN(aHeader->nlmsg_len) + RTA_ALIGN(len);

    return rta;
}

void Netif::PlatUpdateUnicast(const Ip6AddressInfo &aAddressInfo, bool aIsAdded)
{
    struct
    {
        nlmsghdr  nh;
        ifaddrmsg ifa;
        char      buf[512];
    } req;

    assert(mIpFd >= 0);
    memset(&req, 0, sizeof(req));

    req.nh.nlmsg_len   = NLMSG_LENGTH(sizeof(ifaddrmsg));
    req.nh.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK | (aIsAdded ? (NLM_F_CREATE | NLM_F_EXCL) : 0);
    req.nh.nlmsg_type  = aIsAdded ? RTM_NEWADDR : RTM_DELADDR;
    req.nh.nlmsg_pid   = 0;
    req.nh.nlmsg_seq   = ++mNetlinkSequence;

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

    if (send(mNetlinkFd, &req, req.nh.nlmsg_len, 0) != -1)
    {
        otbrLogInfo("Sent request#%u to %s %s/%u", mNetlinkSequence, (aIsAdded ? "add" : "remove"),
                    Ip6Address(aAddressInfo.mAddress).ToString().c_str(), aAddressInfo.mPrefixLength);
    }
    else
    {
        otbrLogWarning("Failed to send request#%u to %s %s/%u", mNetlinkSequence, (aIsAdded ? "add" : "remove"),
                       Ip6Address(aAddressInfo.mAddress).ToString().c_str(), aAddressInfo.mPrefixLength);
    }
}
#pragma GCC diagnostic pop

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

void Netif::PlatInit(void)
{
    SetAddrGenModeToNone();
}

#define ERR_RTA(errmsg, requestPayloadLength) \
    ((rtattr *)((char *)(errmsg)) + NLMSG_ALIGN(sizeof(nlmsgerr)) + NLMSG_ALIGN(requestPayloadLength))

// The format of NLMSG_ERROR is described below:
//
// ----------------------------------------------
// | struct nlmsghdr - response header          |
// ----------------------------------------------------------------
// |    int error                               |                 |
// ---------------------------------------------| struct nlmsgerr |
// | struct nlmsghdr - original request header  |                 |
// ----------------------------------------------------------------
// | ** optionally (1) payload of the request   |
// ----------------------------------------------
// | ** optionally (2) extended ACK attrs       |
// ----------------------------------------------
//
static void HandleNetlinkResponse(nlmsghdr *msg)
{
    const nlmsgerr *err;
    const char     *errorMsg;
    size_t          rtaLength;
    size_t          requestPayloadLength = 0;
    uint32_t        requestSeq           = 0;

    if (msg->nlmsg_len < NLMSG_LENGTH(sizeof(nlmsgerr)))
    {
        otbrLogWarning("Truncated netlink reply of request#%u", requestSeq);
        ExitNow();
    }

    err        = reinterpret_cast<const nlmsgerr *>(NLMSG_DATA(msg));
    requestSeq = err->msg.nlmsg_seq;

    if (err->error == 0)
    {
        otbrLogWarning("Succeeded to process request#%u", requestSeq);
        ExitNow();
    }

    // For rtnetlink, `abs(err->error)` maps to values of `errno`.
    // But this is not a requirement in RFC 3549.
    errorMsg = strerror(abs(err->error));

    // The payload of the request is omitted if NLM_F_CAPPED is set
    if (!(msg->nlmsg_flags & NLM_F_CAPPED))
    {
        requestPayloadLength = NLMSG_PAYLOAD(&err->msg, 0);
    }

    rtaLength = NLMSG_PAYLOAD(msg, sizeof(nlmsgerr)) - requestPayloadLength;

    for (rtattr *rta = ERR_RTA(err, requestPayloadLength); RTA_OK(rta, rtaLength); rta = RTA_NEXT(rta, rtaLength))
    {
        if (rta->rta_type == NLMSGERR_ATTR_MSG)
        {
            errorMsg = reinterpret_cast<const char *>(RTA_DATA(rta));
            break;
        }
        else
        {
            otbrLogWarning("Ignoring netlink response attribute %d (request#%u)", rta->rta_type, requestSeq);
        }
    }

    otbrLogWarning("Failed to process request#%u: %s", requestSeq, errorMsg);

exit:
    return;
}

void Netif::PlatConfigureNetLink(void)
{
    mNetlinkFd = SocketWithCloseExec(AF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE, kSocketNonBlock);
    VerifyOrDie(mNetlinkFd >= 0, strerror(errno));

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
        VerifyOrDie(bind(mNetlinkFd, reinterpret_cast<sockaddr *>(&sa), sizeof(sa)) == 0, strerror(errno));
    }
}

void Netif::PlatConfigureTunDevice(void)
{
    ifreq ifr;

    mTunFd = open(OTBR_POSIX_TUN_DEVICE, O_RDWR | O_CLOEXEC | O_NONBLOCK);
    VerifyOrDie(mTunFd >= 0, strerror(errno));

    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TUN | IFF_NO_PI;

    if (mInterfaceName)
    {
        VerifyOrDie(strlen(mInterfaceName) < IFNAMSIZ, OTBR_ERROR_INVALID_ARGS);

        strncpy(ifr.ifr_name, mInterfaceName, IFNAMSIZ);
    }
    else
    {
        strncpy(ifr.ifr_name, "wpan%d", IFNAMSIZ);
    }

    VerifyOrDie(ioctl(mTunFd, TUNSETIFF, static_cast<void *>(&ifr)) == 0, strerror(errno));

    strncpy(mNetifName, ifr.ifr_name, sizeof(mNetifName));
    otbrLogInfo("Netif name:%s", mNetifName);

    VerifyOrDie(ioctl(mTunFd, TUNSETLINK, ARPHRD_NONE) == 0, strerror(errno));

    ifr.ifr_mtu = static_cast<int>(kMaxIp6Size);
    VerifyOrDie(ioctl(mIpFd, SIOCSIFMTU, static_cast<void *>(&ifr)) == 0, strerror(errno));
}

void Netif::PlatProcessNetlinkEvent(void)
{
    const size_t kMaxNetifEvent = 8192;
    ssize_t      length;

    union
    {
        nlmsghdr nlMsg;
        char     buffer[kMaxNetifEvent];
    } msgBuffer;

    length = recv(mNetlinkFd, msgBuffer.buffer, sizeof(msgBuffer.buffer), 0);

    // Ensures full netlink header is received
    if (length < static_cast<ssize_t>(sizeof(nlmsghdr)))
    {
        otbrLogWarning("Unexpected netlink recv() result: %ld", static_cast<long>(length));
        ExitNow();
    }

    for (nlmsghdr *msg = &msgBuffer.nlMsg; NLMSG_OK(msg, static_cast<size_t>(length)); msg = NLMSG_NEXT(msg, length))
    {
        switch (msg->nlmsg_type)
        {
        case NLMSG_ERROR:
            HandleNetlinkResponse(msg);
            break;

        default:
            otbrLogWarning("Unhandled/Unexpected netlink/route message (%d).", (int)msg->nlmsg_type);
            break;
        }
    }

exit:
    return;
}

} // namespace Posix
} // namespace otbr

#endif // __linux__
