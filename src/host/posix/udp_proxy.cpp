/*
 *  Copyright (c) 2025, The OpenThread Authors.
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

#define OTBR_LOG_TAG "UDPProxy"

#ifdef __APPLE__
#define __APPLE_USE_RFC_3542
#endif

#include "host/posix/udp_proxy.hpp"

#include <assert.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <unistd.h>

#include "common/code_utils.hpp"
#include "common/logging.hpp"
#include "host/posix/dnssd.hpp"
#include "utils/socket_utils.hpp"

namespace otbr {

otbrError UdpProxy::Dependencies::UdpForward(const uint8_t      *aUdpPayload,
                                             uint16_t            aLength,
                                             const otIp6Address &aRemoteAddr,
                                             uint16_t            aRemotePort,
                                             const UdpProxy     &aUdpProxy)
{
    OTBR_UNUSED_VARIABLE(aUdpPayload);
    OTBR_UNUSED_VARIABLE(aLength);
    OTBR_UNUSED_VARIABLE(aRemoteAddr);
    OTBR_UNUSED_VARIABLE(aRemotePort);
    OTBR_UNUSED_VARIABLE(aUdpProxy);

    return OTBR_ERROR_NONE;
}

UdpProxy::UdpProxy(Dependencies &aDeps)
    : mFd(-1)
    , mHostPort(0)
    , mThreadPort(0)
    , mDeps(aDeps)
{
}

void UdpProxy::Start(uint16_t aPort)
{
    VerifyOrExit(!IsStarted());

    BindToEphemeralPort();
    mThreadPort = aPort;

exit:
    return;
}

void UdpProxy::Stop(void)
{
    VerifyOrExit(IsStarted());

    mHostPort = 0;

    if (mFd >= 0)
    {
        close(mFd);
        mFd = -1;
    }

exit:
    return;
}

void UdpProxy::Process(const MainloopContext &aContext)
{
    constexpr size_t kMaxUdpSize = 1280;

    uint8_t      payload[kMaxUdpSize];
    uint16_t     length = sizeof(payload);
    otIp6Address remoteAddr;
    uint16_t     remotePort;

    VerifyOrExit(mFd != -1 && IsStarted());
    VerifyOrExit(FD_ISSET(mFd, &aContext.mReadFdSet));

    SuccessOrExit(ReceivePacket(payload, length, remoteAddr, remotePort));

    // UDP Forward to NCPq
    mDeps.UdpForward(payload, length, remoteAddr, remotePort, *this);

exit:
    return;
}

void UdpProxy::Update(MainloopContext &aContext)
{
    VerifyOrExit(mFd != -1);
    VerifyOrExit(IsStarted());

    aContext.AddFdToReadSet(mFd);

exit:
    return;
}

void UdpProxy::SendToPeer(const uint8_t      *aUdpPayload,
                          uint16_t            aLength,
                          const otIp6Address &aPeerAddr,
                          uint16_t            aPeerPort)
{
#ifdef __APPLE__
    // use fixed value for CMSG_SPACE is not a constant expression on macOS
    constexpr size_t kBufferSize = 128;
#else
    constexpr size_t kBufferSize = CMSG_SPACE(sizeof(struct in6_pktinfo)) + CMSG_SPACE(sizeof(int));
#endif
    struct sockaddr_in6 peerAddr;
    uint8_t             control[kBufferSize];
    size_t              controlLength = 0;
    struct iovec        iov;
    struct msghdr       msg;
    struct cmsghdr     *cmsg;
    ssize_t             rval;

    memset(&peerAddr, 0, sizeof(peerAddr));
    peerAddr.sin6_port   = htons(aPeerPort);
    peerAddr.sin6_family = AF_INET6;
    memcpy(&peerAddr.sin6_addr, &aPeerAddr, sizeof(aPeerAddr));
    memset(control, 0, sizeof(control));

    iov.iov_base = reinterpret_cast<void *>(const_cast<uint8_t *>(aUdpPayload));
    iov.iov_len  = aLength;

    msg.msg_name       = &peerAddr;
    msg.msg_namelen    = sizeof(peerAddr);
    msg.msg_control    = control;
    msg.msg_controllen = static_cast<decltype(msg.msg_controllen)>(sizeof(control));
    msg.msg_iov        = &iov;
    msg.msg_iovlen     = 1;
    msg.msg_flags      = 0;

    {
        constexpr int kIp6HopLimit = 64;

        int hopLimit = kIp6HopLimit;

        cmsg             = CMSG_FIRSTHDR(&msg);
        cmsg->cmsg_level = IPPROTO_IPV6;
        cmsg->cmsg_type  = IPV6_HOPLIMIT;
        cmsg->cmsg_len   = CMSG_LEN(sizeof(int));

        memcpy(CMSG_DATA(cmsg), &hopLimit, sizeof(int));

        controlLength += CMSG_SPACE(sizeof(int));
    }

#ifdef __APPLE__
    msg.msg_controllen = static_cast<socklen_t>(controlLength);
#else
    msg.msg_controllen           = controlLength;
#endif

    rval = sendmsg(mFd, &msg, 0);

    if (rval == -1)
    {
        otbrLogWarning("Failed to sendmsg: %s", strerror(errno));
    }
}

otbrError UdpProxy::BindToEphemeralPort(void)
{
    otbrError error = OTBR_ERROR_NONE;
    mFd             = SocketWithCloseExec(AF_INET6, SOCK_DGRAM, IPPROTO_UDP, kSocketNonBlock);

    VerifyOrExit(mFd != 0, error = OTBR_ERROR_ERRNO);

    {
        struct sockaddr_in6 sin6;

        memset(&sin6, 0, sizeof(sin6));
        sin6.sin6_family = AF_INET6;
        sin6.sin6_addr   = in6addr_any;
        sin6.sin6_port   = 0;

        VerifyOrExit(0 == bind(mFd, reinterpret_cast<struct sockaddr *>(&sin6), sizeof(sin6)),
                     error = OTBR_ERROR_ERRNO);
    }

    {
        int on = 1;
        VerifyOrExit(0 == setsockopt(mFd, IPPROTO_IPV6, IPV6_RECVHOPLIMIT, &on, sizeof(on)), error = OTBR_ERROR_ERRNO);
        VerifyOrExit(0 == setsockopt(mFd, IPPROTO_IPV6, IPV6_RECVPKTINFO, &on, sizeof(on)), error = OTBR_ERROR_ERRNO);
    }

    {
        struct sockaddr_in bound_addr;
        socklen_t          addr_len = sizeof(bound_addr);
        getsockname(mFd, (struct sockaddr *)&bound_addr, &addr_len);

        mHostPort = ntohs(bound_addr.sin_port);
        otbrLogInfo("Ephemeral port: %u", mHostPort);
    }

exit:
    otbrLogResult(error, "Bind to ephemeral port");
    if (error != OTBR_ERROR_NONE)
    {
        Stop();
    }
    return error;
}

otbrError UdpProxy::ReceivePacket(uint8_t      *aPayload,
                                  uint16_t     &aLength,
                                  otIp6Address &aRemoteAddr,
                                  uint16_t     &aRemotePort)
{
    constexpr size_t kMaxUdpSize = 1280;

    struct sockaddr_in6 peerAddr;
    uint8_t             control[kMaxUdpSize];
    struct iovec        iov;
    struct msghdr       msg;
    ssize_t             rval;

    iov.iov_base = aPayload;
    iov.iov_len  = aLength;

    msg.msg_name       = &peerAddr;
    msg.msg_namelen    = sizeof(peerAddr);
    msg.msg_control    = control;
    msg.msg_controllen = sizeof(control);
    msg.msg_iov        = &iov;
    msg.msg_iovlen     = 1;
    msg.msg_flags      = 0;

    rval = recvmsg(mFd, &msg, 0);
    VerifyOrExit(rval > 0, perror("recvmsg"));
    aLength = static_cast<uint16_t>(rval);

    aRemotePort = ntohs(peerAddr.sin6_port);
    memcpy(&aRemoteAddr, &peerAddr.sin6_addr, sizeof(otIp6Address));

    otbrLogDebug("Receive a packet, remote address:%s, remote port:%d", Ip6Address(aRemoteAddr).ToString().c_str(),
                 aRemotePort);

exit:
    return rval > 0 ? OTBR_ERROR_NONE : OTBR_ERROR_ERRNO;
}

} // namespace otbr
