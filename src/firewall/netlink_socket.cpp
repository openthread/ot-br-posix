/*
 *  Copyright (c) 2026, The OpenThread Authors.
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

#include "firewall/netlink_socket.hpp"

#include <errno.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <libmnl/libmnl.h>
#include <linux/netlink.h>

namespace otbr {
namespace Firewall {

MnlNetlinkSocket::MnlNetlinkSocket(void)
    : mSocket(nullptr)
    , mPortId(0)
{
}

MnlNetlinkSocket::~MnlNetlinkSocket(void)
{
    Close();
}

otbrError MnlNetlinkSocket::Open(void)
{
    otbrError error = OTBR_ERROR_NONE;

    VerifyOrExit(mSocket == nullptr, error = OTBR_ERROR_INVALID_STATE);

    mSocket = mnl_socket_open(NETLINK_NETFILTER);
    VerifyOrExit(mSocket != nullptr, error = OTBR_ERROR_ERRNO);

    VerifyOrExit(mnl_socket_bind(mSocket, 0, MNL_SOCKET_AUTOPID) >= 0, error = OTBR_ERROR_ERRNO);

    // otbr-agent is single threaded, so a netlink reply that never arrives would
    // block the whole daemon. Bound the wait and let the caller fail instead.
    {
        struct timeval timeout;

        timeout.tv_sec  = kRecvTimeoutSeconds;
        timeout.tv_usec = 0;
        VerifyOrExit(setsockopt(mnl_socket_get_fd(mSocket), SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == 0,
                     error = OTBR_ERROR_ERRNO);
    }

    mPortId = mnl_socket_get_portid(mSocket);

exit:
    if (error != OTBR_ERROR_NONE)
    {
        Close();
    }
    return error;
}

void MnlNetlinkSocket::Close(void)
{
    if (mSocket != nullptr)
    {
        mnl_socket_close(mSocket);
        mSocket = nullptr;
    }
    mPortId = 0;
}

bool MnlNetlinkSocket::IsOpen(void) const
{
    return mSocket != nullptr;
}

uint32_t MnlNetlinkSocket::GetPortId(void) const
{
    return mPortId;
}

ssize_t MnlNetlinkSocket::Send(const void *aBuffer, size_t aLength)
{
    ssize_t rval = -1;

    VerifyOrExit(mSocket != nullptr, errno = EBADF);
    rval = mnl_socket_sendto(mSocket, aBuffer, aLength);

exit:
    return rval;
}

ssize_t MnlNetlinkSocket::Recv(void *aBuffer, size_t aLength)
{
    ssize_t rval = -1;

    VerifyOrExit(mSocket != nullptr, errno = EBADF);
    rval = mnl_socket_recvfrom(mSocket, aBuffer, aLength);

exit:
    return rval;
}

void MnlNetlinkSocket::Drain(void)
{
    uint8_t discard[MNL_SOCKET_BUFFER_SIZE];

    VerifyOrExit(mSocket != nullptr);

    while (true)
    {
        ssize_t rval = recv(mnl_socket_get_fd(mSocket), discard, sizeof(discard), MSG_DONTWAIT);

        // Stop when the queue is empty (EAGAIN), but a signal must not cut the
        // drain short and leave stale replies behind.
        if (rval < 0 && errno == EINTR)
        {
            continue;
        }

        VerifyOrExit(rval > 0);
    }

exit:
    return;
}

} // namespace Firewall
} // namespace otbr
