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

/**
 * @file
 *   This file defines the netlink socket seam used by the nftables backend.
 *
 *   Nftables talks to the kernel only through INetlinkSocket. In production
 *   that is MnlNetlinkSocket, a thin wrapper over libmnl. In tests it can be a
 *   fake, which is what makes the batch and reply handling in Nftables
 *   (EINTR retries, receive timeouts, short or malformed replies, partially
 *   built batches) reachable without a kernel.
 */

#ifndef OTBR_FIREWALL_NETLINK_SOCKET_HPP_
#define OTBR_FIREWALL_NETLINK_SOCKET_HPP_

#include "openthread-br/config.h"

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#include "common/code_utils.hpp"
#include "common/types.hpp"

// Declared at global scope so the member below names ::mnl_socket rather than
// implicitly declaring otbr::Firewall::mnl_socket, which would not match the
// type returned by libmnl.
struct mnl_socket;

namespace otbr {
namespace Firewall {

/**
 * Abstracts the netlink socket used to talk to nf_tables.
 *
 * Send() and Recv() mirror sendto()/recvfrom(): they return the byte count, or
 * a negative value with errno set. Callers rely on errno to distinguish EINTR
 * (retry) from EAGAIN (receive timeout) and other failures.
 */
class INetlinkSocket
{
public:
    virtual ~INetlinkSocket(void) = default;

    /**
     * Opens and binds the socket and applies the receive timeout.
     */
    virtual otbrError Open(void) = 0;

    /**
     * Closes the socket. Safe to call when not open.
     */
    virtual void Close(void) = 0;

    virtual bool IsOpen(void) const = 0;

    /**
     * Returns the port id assigned at bind time, used to match replies.
     */
    virtual uint32_t GetPortId(void) const = 0;

    virtual ssize_t Send(const void *aBuffer, size_t aLength) = 0;
    virtual ssize_t Recv(void *aBuffer, size_t aLength)       = 0;

    /**
     * Discards anything already queued for reading.
     *
     * Used before sending, so replies left behind by a transaction that timed
     * out or failed are not mistaken for replies to the next one.
     */
    virtual void Drain(void) = 0;
};

/**
 * The production INetlinkSocket, backed by libmnl.
 */
class MnlNetlinkSocket : public INetlinkSocket, private NonCopyable
{
public:
    MnlNetlinkSocket(void);
    ~MnlNetlinkSocket(void) override;

    otbrError Open(void) override;
    void      Close(void) override;
    bool      IsOpen(void) const override;
    uint32_t  GetPortId(void) const override;
    ssize_t   Send(const void *aBuffer, size_t aLength) override;
    ssize_t   Recv(void *aBuffer, size_t aLength) override;
    void      Drain(void) override;

private:
    /// Bound on waiting for a netlink reply, so a lost ack cannot hang the
    /// single-threaded daemon.
    static constexpr time_t kRecvTimeoutSeconds = 5;

    struct mnl_socket *mSocket;
    uint32_t           mPortId;
};

} // namespace Firewall
} // namespace otbr

#endif // OTBR_FIREWALL_NETLINK_SOCKET_HPP_
