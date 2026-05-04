/*
 *    Copyright (c) 2026, The OpenThread Authors.
 *    All rights reserved.
 *
 *    Redistribution and use in source and binary forms, with or without
 *    modification, are permitted provided that the following conditions are met:
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *    3. Neither the name of the copyright holder nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 *    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *    ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *    LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *    CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *    ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *    POSSIBILITY OF SUCH DAMAGE.
 */

#define OTBR_LOG_TAG "FIREWALL"

#include "firewall/nftables_impl.hpp"

#if OTBR_ENABLE_NFTABLES

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <linux/netfilter.h>
#include <linux/netfilter/nf_tables.h>
#include <linux/netlink.h>

#include <libmnl/libmnl.h>
#include <libnftnl/batch.h>
#include <libnftnl/common.h>
#include <libnftnl/table.h>

#include "common/logging.hpp"

namespace otbr {
namespace Firewall {

Nftables::Nftables(void)
    : mSocket(nullptr)
    , mBatch(nullptr)
    , mBatchBuffer(nullptr)
    , mPortId(0)
    , mSeq(0)
    , mInBatch(false)
{
}

Nftables::~Nftables(void)
{
    Deinit();
}

otbrError Nftables::Init(void)
{
    otbrError error = OTBR_ERROR_NONE;

    VerifyOrExit(mSocket == nullptr, error = OTBR_ERROR_INVALID_STATE);

    mSocket = mnl_socket_open(NETLINK_NETFILTER);
    VerifyOrExit(mSocket != nullptr, error = OTBR_ERROR_ERRNO);

    VerifyOrExit(mnl_socket_bind(mSocket, 0, MNL_SOCKET_AUTOPID) >= 0, error = OTBR_ERROR_ERRNO);
    mPortId = mnl_socket_get_portid(mSocket);
    mSeq    = static_cast<uint32_t>(time(nullptr));

exit:
    if (error != OTBR_ERROR_NONE && mSocket != nullptr)
    {
        mnl_socket_close(mSocket);
        mSocket = nullptr;
    }
    return error;
}

otbrError Nftables::Deinit(void)
{
    if (mBatch != nullptr)
    {
        mnl_nlmsg_batch_stop(mBatch);
        mBatch = nullptr;
    }
    free(mBatchBuffer);
    mBatchBuffer = nullptr;
    mInBatch     = false;

    if (mSocket != nullptr)
    {
        mnl_socket_close(mSocket);
        mSocket = nullptr;
    }
    return OTBR_ERROR_NONE;
}

otbrError Nftables::BeginBatch(void)
{
    otbrError error = OTBR_ERROR_NONE;

    VerifyOrExit(mSocket != nullptr, error = OTBR_ERROR_INVALID_STATE);
    VerifyOrExit(!mInBatch, error = OTBR_ERROR_INVALID_STATE);

    mBatchBuffer = calloc(1, kBatchPageSize * 2);
    VerifyOrExit(mBatchBuffer != nullptr, error = OTBR_ERROR_ERRNO);

    mBatch = mnl_nlmsg_batch_start(mBatchBuffer, kBatchPageSize);
    VerifyOrExit(mBatch != nullptr, error = OTBR_ERROR_ERRNO);

    nftnl_batch_begin(reinterpret_cast<char *>(mnl_nlmsg_batch_current(mBatch)), mSeq++);
    mnl_nlmsg_batch_next(mBatch);

    mInBatch = true;

exit:
    if (error != OTBR_ERROR_NONE)
    {
        free(mBatchBuffer);
        mBatchBuffer = nullptr;
        mBatch       = nullptr;
    }
    return error;
}

otbrError Nftables::CommitBatch(void)
{
    otbrError error    = OTBR_ERROR_NONE;
    uint32_t  endSeq   = 0;
    char      buf[MNL_SOCKET_BUFFER_SIZE];
    ssize_t   recvLen;

    VerifyOrExit(mInBatch, error = OTBR_ERROR_INVALID_STATE);

    endSeq = mSeq++;
    nftnl_batch_end(reinterpret_cast<char *>(mnl_nlmsg_batch_current(mBatch)), endSeq);
    mnl_nlmsg_batch_next(mBatch);

    VerifyOrExit(mnl_socket_sendto(mSocket, mnl_nlmsg_batch_head(mBatch), mnl_nlmsg_batch_size(mBatch)) >= 0,
                 error = OTBR_ERROR_ERRNO);

    while ((recvLen = mnl_socket_recvfrom(mSocket, buf, sizeof(buf))) > 0)
    {
        int cb = mnl_cb_run(buf, recvLen, 0, mPortId, nullptr, nullptr);
        if (cb < 0)
        {
            error = OTBR_ERROR_ERRNO;
            break;
        }
        if (cb == MNL_CB_STOP)
        {
            break;
        }
    }
    if (recvLen < 0 && error == OTBR_ERROR_NONE)
    {
        error = OTBR_ERROR_ERRNO;
    }

exit:
    if (mBatch != nullptr)
    {
        mnl_nlmsg_batch_stop(mBatch);
        mBatch = nullptr;
    }
    free(mBatchBuffer);
    mBatchBuffer = nullptr;
    mInBatch     = false;
    return error;
}

otbrError Nftables::DelTable(const std::string &aTable)
{
    otbrError       error = OTBR_ERROR_NONE;
    char            buf[MNL_SOCKET_BUFFER_SIZE];
    struct nlmsghdr *nlh;
    struct nftnl_table *t = nullptr;

    VerifyOrExit(mSocket != nullptr, error = OTBR_ERROR_INVALID_STATE);
    VerifyOrExit(!mInBatch, error = OTBR_ERROR_INVALID_STATE);

    t = nftnl_table_alloc();
    VerifyOrExit(t != nullptr, error = OTBR_ERROR_ERRNO);

    nftnl_table_set_str(t, NFTNL_TABLE_NAME, aTable.c_str());

    nlh = nftnl_table_nlmsg_build_hdr(buf, NFT_MSG_DELTABLE, NFPROTO_INET, NLM_F_ACK, mSeq++);
    nftnl_table_nlmsg_build_payload(nlh, t);

    error = SendStandalone(buf, nlh->nlmsg_len, /* aAllowEnoent */ true);

exit:
    if (t != nullptr)
    {
        nftnl_table_free(t);
    }
    return error;
}

otbrError Nftables::SendStandalone(const void *aMessage, size_t aLength, bool aAllowEnoent)
{
    otbrError error = OTBR_ERROR_NONE;
    char      recvBuf[MNL_SOCKET_BUFFER_SIZE];
    ssize_t   recvLen;
    uint32_t  seq = reinterpret_cast<const struct nlmsghdr *>(aMessage)->nlmsg_seq;

    VerifyOrExit(mnl_socket_sendto(mSocket, aMessage, aLength) >= 0, error = OTBR_ERROR_ERRNO);

    recvLen = mnl_socket_recvfrom(mSocket, recvBuf, sizeof(recvBuf));
    VerifyOrExit(recvLen > 0, error = OTBR_ERROR_ERRNO);

    if (mnl_cb_run(recvBuf, recvLen, seq, mPortId, nullptr, nullptr) < 0)
    {
        if (aAllowEnoent && errno == ENOENT)
        {
            // Idempotent path: object already absent.
        }
        else
        {
            error = OTBR_ERROR_ERRNO;
        }
    }

exit:
    return error;
}

// --- Phase 2/3: rule and set-element construction lives here. -----------------
//
// Each method below builds a libnftnl object (nftnl_chain, nftnl_set,
// nftnl_rule + expressions), serializes it into the current batch via the
// matching nftnl_*_nlmsg_build_payload helper, and advances the batch. Echo
// flags on rule-add messages let us read back the kernel-assigned handle in
// CommitBatch().
//
// Phase 1 stubs these so the abstraction can land independently. The mock
// INftables in tests/gtest/test_firewall_manager.cpp covers the call patterns;
// full kernel-side validation comes with the call-site cutover in Phase 2/3.

otbrError Nftables::AddTable(const std::string &)
{
    return OTBR_ERROR_NOT_IMPLEMENTED;
}

otbrError Nftables::AddChain(const std::string &, const std::string &, Hook, int)
{
    return OTBR_ERROR_NOT_IMPLEMENTED;
}

otbrError Nftables::AddIp6PrefixSet(const std::string &, const std::string &)
{
    return OTBR_ERROR_NOT_IMPLEMENTED;
}

otbrError Nftables::AddSetElement(const std::string &, const std::string &, const Ip6Net &)
{
    return OTBR_ERROR_NOT_IMPLEMENTED;
}

otbrError Nftables::DelSetElement(const std::string &, const std::string &, const Ip6Net &)
{
    return OTBR_ERROR_NOT_IMPLEMENTED;
}

otbrError Nftables::FlushSet(const std::string &, const std::string &)
{
    return OTBR_ERROR_NOT_IMPLEMENTED;
}

otbrError Nftables::AddRuleOifnameNeqReturn(const std::string &,
                                            const std::string &,
                                            const std::string &,
                                            uint64_t *)
{
    return OTBR_ERROR_NOT_IMPLEMENTED;
}

otbrError Nftables::AddRuleIifPkttypeVerdict(const std::string &,
                                             const std::string &,
                                             const std::string &,
                                             PktType,
                                             Verdict,
                                             uint64_t *)
{
    return OTBR_ERROR_NOT_IMPLEMENTED;
}

otbrError Nftables::AddRulePkttypeVerdict(const std::string &,
                                          const std::string &,
                                          PktType,
                                          Verdict,
                                          uint64_t *)
{
    return OTBR_ERROR_NOT_IMPLEMENTED;
}

otbrError Nftables::AddRuleSetLookupVerdict(const std::string &,
                                            const std::string &,
                                            const std::string &,
                                            SetDirection,
                                            Verdict,
                                            uint64_t *)
{
    return OTBR_ERROR_NOT_IMPLEMENTED;
}

otbrError Nftables::AddRuleVerdict(const std::string &, const std::string &, Verdict, uint64_t *)
{
    return OTBR_ERROR_NOT_IMPLEMENTED;
}

otbrError Nftables::AddRuleNdNsRedirect(const std::string &,
                                        const std::string &,
                                        const Ip6Net &,
                                        const std::string &,
                                        uint16_t,
                                        uint64_t *)
{
    return OTBR_ERROR_NOT_IMPLEMENTED;
}

otbrError Nftables::DelRule(const std::string &, const std::string &, uint64_t)
{
    return OTBR_ERROR_NOT_IMPLEMENTED;
}

} // namespace Firewall
} // namespace otbr

#endif // OTBR_ENABLE_NFTABLES
