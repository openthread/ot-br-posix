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
 *   A scriptable INetlinkSocket for exercising Nftables without a kernel.
 *
 *   Every reply the backend can see is expressed as a queued Reply: a byte
 *   count with an optional payload, or a negative return with an errno. That
 *   makes the paths which otherwise need a live netlink socket reachable —
 *   signal interruption, receive timeouts, short replies, and replies left over
 *   from an earlier transaction.
 */

#ifndef OTBR_TEST_FAKE_NETLINK_SOCKET_HPP_
#define OTBR_TEST_FAKE_NETLINK_SOCKET_HPP_

#include <errno.h>
#include <string.h>

#include <deque>
#include <vector>

#include <libmnl/libmnl.h>
#include <linux/netlink.h>

#include "firewall/netlink_socket.hpp"

namespace otbr {
namespace Firewall {

class FakeNetlinkSocket : public INetlinkSocket
{
public:
    struct Reply
    {
        ssize_t              mResult; ///< What Recv() returns; negative means failure.
        int                  mErrno;  ///< errno to set when mResult is negative.
        std::vector<uint8_t> mData;   ///< Bytes handed back when mResult is positive.
    };

    otbrError Open(void) override
    {
        mOpen = true;
        return OTBR_ERROR_NONE;
    }

    void     Close(void) override { mOpen = false; }
    bool     IsOpen(void) const override { return mOpen; }
    uint32_t GetPortId(void) const override { return kPortId; }

    ssize_t Send(const void *aBuffer, size_t aLength) override
    {
        const uint8_t *bytes = static_cast<const uint8_t *>(aBuffer);

        mSent.push_back(std::vector<uint8_t>(bytes, bytes + aLength));
        mDrainsBeforeThisSend.push_back(mDrainCount);

        if (mSendResult < 0)
        {
            errno = mSendErrno;
        }

        return (mSendResult < 0) ? mSendResult : static_cast<ssize_t>(aLength);
    }

    ssize_t Recv(void *aBuffer, size_t aLength) override
    {
        Reply reply;

        // Nothing scripted: behave like an idle socket that timed out, so a test
        // that forgets to queue a reply fails rather than hanging or succeeding.
        if (mReplies.empty())
        {
            errno = EAGAIN;
            return -1;
        }

        reply = mReplies.front();
        mReplies.pop_front();

        if (reply.mResult < 0)
        {
            errno = reply.mErrno;
            return reply.mResult;
        }

        if (!reply.mData.empty())
        {
            size_t copied = (reply.mData.size() < aLength) ? reply.mData.size() : aLength;

            memcpy(aBuffer, reply.mData.data(), copied);
            return static_cast<ssize_t>(copied);
        }

        return reply.mResult;
    }

    // The queue already reports an empty socket as EAGAIN, which is what a
    // non-blocking receive does, so the two behave identically here.
    ssize_t RecvNoWait(void *aBuffer, size_t aLength) override { return Recv(aBuffer, aLength); }

    void Drain(void) override { mDrainCount++; }

    /// Queues a netlink ACK, which is what makes mnl_cb_run() report completion.
    void QueueAck(uint32_t aSeq)
    {
        std::vector<uint8_t> buf(NLMSG_SPACE(sizeof(struct nlmsgerr)), 0);
        struct nlmsghdr     *nlh = reinterpret_cast<struct nlmsghdr *>(buf.data());
        struct nlmsgerr     *err;

        nlh->nlmsg_len   = NLMSG_LENGTH(sizeof(struct nlmsgerr));
        nlh->nlmsg_type  = NLMSG_ERROR;
        nlh->nlmsg_flags = 0;
        nlh->nlmsg_seq   = aSeq;
        nlh->nlmsg_pid   = kPortId;

        err        = static_cast<struct nlmsgerr *>(NLMSG_DATA(nlh));
        err->error = 0; // zero means ACK rather than error
        err->msg   = *nlh;

        mReplies.push_back(Reply{static_cast<ssize_t>(buf.size()), 0, buf});
    }

    /// Queues the kernel's rejection of one message of a batch: the same
    /// NLMSG_ERROR shape as an ack, but carrying a non-zero error.
    void QueueNetlinkError(int aErrno, uint32_t aSeq)
    {
        std::vector<uint8_t> buf(NLMSG_SPACE(sizeof(struct nlmsgerr)), 0);
        struct nlmsghdr     *nlh = reinterpret_cast<struct nlmsghdr *>(buf.data());
        struct nlmsgerr     *err;

        nlh->nlmsg_len   = NLMSG_LENGTH(sizeof(struct nlmsgerr));
        nlh->nlmsg_type  = NLMSG_ERROR;
        nlh->nlmsg_flags = 0;
        nlh->nlmsg_seq   = aSeq;
        nlh->nlmsg_pid   = kPortId;

        err        = static_cast<struct nlmsgerr *>(NLMSG_DATA(nlh));
        err->error = -aErrno;
        err->msg   = *nlh;

        mReplies.push_back(Reply{static_cast<ssize_t>(buf.size()), 0, buf});
    }

    void QueueFailure(int aErrno) { mReplies.push_back(Reply{-1, aErrno, {}}); }
    void QueueZeroLengthReply(void) { mReplies.push_back(Reply{0, 0, {}}); }

    void FailNextSend(int aErrno)
    {
        mSendResult = -1;
        mSendErrno  = aErrno;
    }

    size_t DrainCount(void) const { return mDrainCount; }
    size_t SendCount(void) const { return mSent.size(); }
    size_t PendingReplies(void) const { return mReplies.size(); }

    /// How many drains had happened by the time send @p aIndex was issued.
    size_t DrainsBeforeSend(size_t aIndex) const { return mDrainsBeforeThisSend.at(aIndex); }

    static constexpr uint32_t kPortId = 4242;

private:
    bool                              mOpen       = false;
    ssize_t                           mSendResult = 0;
    int                               mSendErrno  = 0;
    size_t                            mDrainCount = 0;
    std::deque<Reply>                 mReplies;
    std::vector<std::vector<uint8_t>> mSent;
    std::vector<size_t>               mDrainsBeforeThisSend;
};

} // namespace Firewall
} // namespace otbr

#endif // OTBR_TEST_FAKE_NETLINK_SOCKET_HPP_
