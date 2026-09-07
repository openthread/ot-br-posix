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
#include <net/if.h>
#include <netinet/icmp6.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>

#include <linux/netfilter.h>
#include <linux/netfilter/nf_tables.h>
#include <linux/netfilter/nfnetlink.h>
#include <linux/netlink.h>

#include <libmnl/libmnl.h>
#include <libnftnl/batch.h>
#include <libnftnl/chain.h>
#include <libnftnl/common.h>
#include <libnftnl/expr.h>
#include <libnftnl/rule.h>
#include <libnftnl/set.h>
#include <libnftnl/table.h>

#include "common/logging.hpp"

namespace otbr {
namespace Firewall {

Nftables::Nftables(void)
    : mOwnedSocket(MakeUnique<MnlNetlinkSocket>())
    , mSocket(*mOwnedSocket)
    , mBatch(nullptr)
    , mBatchBuffer(nullptr)
    , mSeq(0)
    , mInBatch(false)
{
}

Nftables::Nftables(INetlinkSocket &aSocket)
    : mOwnedSocket(nullptr)
    , mSocket(aSocket)
    , mBatch(nullptr)
    , mBatchBuffer(nullptr)
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

    VerifyOrExit(!mSocket.IsOpen(), error = OTBR_ERROR_INVALID_STATE);

    SuccessOrExit(error = mSocket.Open());
    mSeq = static_cast<uint32_t>(time(nullptr));

exit:
    return error;
}

otbrError Nftables::Deinit(void)
{
    // Discards any half-built batch and, importantly, the pending-handle map,
    // whose entries point at caller stack variables that are already gone.
    AbortBatch();

    mSocket.Close();
    return OTBR_ERROR_NONE;
}

otbrError Nftables::AdvanceBatch(void)
{
    otbrError error = OTBR_ERROR_NONE;

    // The buffer is twice the batch limit, so the message that crossed the limit
    // is already written safely, but nothing further may be added. Fail the whole
    // operation rather than silently truncating or overrunning the buffer.
    VerifyOrExit(mnl_nlmsg_batch_next(mBatch), errno = ENOBUFS, error = OTBR_ERROR_ERRNO);

exit:
    return error;
}

otbrError Nftables::BeginBatch(void)
{
    otbrError error = OTBR_ERROR_NONE;

    VerifyOrExit(mSocket.IsOpen(), error = OTBR_ERROR_INVALID_STATE);
    VerifyOrExit(!mInBatch, error = OTBR_ERROR_INVALID_STATE);

    mBatchBuffer = calloc(1, kBatchPageSize * 2);
    VerifyOrExit(mBatchBuffer != nullptr, error = OTBR_ERROR_ERRNO);

    mBatch = mnl_nlmsg_batch_start(mBatchBuffer, kBatchPageSize);
    VerifyOrExit(mBatch != nullptr, error = OTBR_ERROR_ERRNO);

    nftnl_batch_begin(reinterpret_cast<char *>(mnl_nlmsg_batch_current(mBatch)), mSeq++);
    SuccessOrExit(error = AdvanceBatch());

    mInBatch = true;

exit:
    // Only clean up what this call allocated. When the failure is that a batch
    // is already open, mInBatch is still set and the buffer belongs to that
    // batch, so it must be left alone.
    if (error != OTBR_ERROR_NONE && !mInBatch)
    {
        AbortBatch();
    }
    return error;
}

void Nftables::DrainSocket(void)
{
    mSocket.Drain();
}

otbrError Nftables::CommitBatch(void)
{
    otbrError                     error = OTBR_ERROR_NONE;
    alignas(struct nlmsghdr) char buf[MNL_SOCKET_BUFFER_SIZE];
    ssize_t                       recvLen;
    bool                          replied    = false;
    int                           savedErrno = 0;

    VerifyOrExit(mInBatch, error = OTBR_ERROR_INVALID_STATE);

    nftnl_batch_end(reinterpret_cast<char *>(mnl_nlmsg_batch_current(mBatch)), mSeq++);
    SuccessOrExit(error = AdvanceBatch());

    DrainSocket();

    VerifyOrExit(mSocket.Send(mnl_nlmsg_batch_head(mBatch), mnl_nlmsg_batch_size(mBatch)) >= 0,
                 error = OTBR_ERROR_ERRNO);

    // Once the kernel has processed the whole batch it answers every message
    // that asked for an acknowledgement, one datagram each and in the order the
    // messages were sent. The acknowledgement of an early message therefore
    // arrives before the error of a later one, so stopping at the first reply
    // would report a commit that failed halfway as successful and leave the
    // caller believing rules are installed that are not. Read until the socket
    // runs dry instead, keeping the first failure, as nft(8) does.
    while (true)
    {
        recvLen = replied ? mSocket.RecvNoWait(buf, sizeof(buf)) : mSocket.Recv(buf, sizeof(buf));

        if (recvLen < 0)
        {
            // Retry only when interrupted by a signal. Before the first reply an
            // empty socket is the receive timeout and must stay an error, or the
            // timeout would never fire; after it, the remaining replies are
            // already queued, so an empty socket means there are no more.
            if (errno == EINTR)
            {
                continue;
            }
            VerifyOrExit(replied && errno == EAGAIN, error = OTBR_ERROR_ERRNO);
            break;
        }

        // A zero-length reply means the loop ends without the kernel ever having
        // acknowledged the batch, so the commit cannot be reported as successful.
        VerifyOrExit(recvLen > 0, errno = EPROTO, error = OTBR_ERROR_ERRNO);
        replied = true;

        // Errors do not end the loop either: a reply left behind would be read
        // as the answer to the next transaction.
        if (mnl_cb_run(buf, recvLen, 0, mSocket.GetPortId(), &Nftables::HandleEchoReply, this) < 0 &&
            error == OTBR_ERROR_NONE)
        {
            savedErrno = errno;
            error      = OTBR_ERROR_ERRNO;
        }
    }

exit:
    AbortBatch();

    if (savedErrno != 0)
    {
        errno = savedErrno;
    }

    return error;
}

void Nftables::AbortBatch(void)
{
    if (mBatch != nullptr)
    {
        mnl_nlmsg_batch_stop(mBatch);
        mBatch = nullptr;
    }
    free(mBatchBuffer);
    mBatchBuffer = nullptr;
    mInBatch     = false;
    mPendingHandles.clear();
}

int Nftables::HandleEchoReply(const struct nlmsghdr *aNlh, void *aContext)
{
    Nftables *self = static_cast<Nftables *>(aContext);

    // ECHO replies for NFT_MSG_NEWRULE carry the kernel-assigned handle.
    if (NFNL_MSG_TYPE(aNlh->nlmsg_type) == NFT_MSG_NEWRULE)
    {
        auto it = self->mPendingHandles.find(aNlh->nlmsg_seq);
        if (it != self->mPendingHandles.end() && it->second != nullptr)
        {
            struct nftnl_rule *r = nftnl_rule_alloc();

            // The caller keeps the handle to delete the rule later. Failing to
            // learn it must fail the commit: reporting success would leave the
            // rule installed but unaddressable, so it could never be removed.
            if (r == nullptr || nftnl_rule_nlmsg_parse(aNlh, r) < 0)
            {
                if (r != nullptr)
                {
                    nftnl_rule_free(r);
                }
                errno = EBADMSG;
                return MNL_CB_ERROR;
            }

            *it->second = nftnl_rule_get_u64(r, NFTNL_RULE_HANDLE);
            nftnl_rule_free(r);
        }
    }
    return MNL_CB_OK;
}

otbrError Nftables::DelTable(const std::string &aTable)
{
    otbrError           error = OTBR_ERROR_NONE;
    struct nftnl_table *t     = nullptr;

    VerifyOrExit(mInBatch, error = OTBR_ERROR_INVALID_STATE);

    t = nftnl_table_alloc();
    VerifyOrExit(t != nullptr, error = OTBR_ERROR_ERRNO);

    nftnl_table_set_str(t, NFTNL_TABLE_NAME, aTable.c_str());
    nftnl_table_set_u32(t, NFTNL_TABLE_FAMILY, NFPROTO_INET);

    // Deleting a table that is not there fails the whole transaction with
    // ENOENT, and nf_tables gives us no "delete if it exists". Creating it
    // first does: NFT_MSG_NEWTABLE without NLM_F_EXCL is a no-op when the
    // table already exists, so the pair is a delete that always succeeds.
    // Both messages land in one transaction, so the table is never observably
    // created -- either the whole batch commits or none of it does.
    {
        struct nlmsghdr *nlh =
            nftnl_table_nlmsg_build_hdr(reinterpret_cast<char *>(mnl_nlmsg_batch_current(mBatch)), NFT_MSG_NEWTABLE,
                                        NFPROTO_INET, NLM_F_CREATE | NLM_F_ACK, mSeq++);
        nftnl_table_nlmsg_build_payload(nlh, t);
        SuccessOrExit(error = AdvanceBatch());
    }

    {
        struct nlmsghdr *nlh = nftnl_table_nlmsg_build_hdr(reinterpret_cast<char *>(mnl_nlmsg_batch_current(mBatch)),
                                                           NFT_MSG_DELTABLE, NFPROTO_INET, NLM_F_ACK, mSeq++);
        nftnl_table_nlmsg_build_payload(nlh, t);
        SuccessOrExit(error = AdvanceBatch());
    }

exit:
    if (t != nullptr)
    {
        nftnl_table_free(t);
    }
    return error;
}

// --- Object and rule construction. ------------------------------------------

namespace {

uint32_t HookToHooknum(Hook aHook)
{
    switch (aHook)
    {
    case Hook::kForward:
        return NF_INET_FORWARD;
    case Hook::kPrerouting:
        return NF_INET_PRE_ROUTING;
    case Hook::kPostrouting:
        return NF_INET_POST_ROUTING;
    }
    return 0;
}

const char *ChainTypeToString(ChainType aType)
{
    switch (aType)
    {
    case ChainType::kFilter:
        return "filter";
    case ChainType::kNat:
        return "nat";
    }
    return "filter";
}

} // namespace

otbrError Nftables::AddTable(const std::string &aTable)
{
    otbrError           error = OTBR_ERROR_NONE;
    struct nftnl_table *t     = nullptr;

    VerifyOrExit(mInBatch, error = OTBR_ERROR_INVALID_STATE);

    t = nftnl_table_alloc();
    VerifyOrExit(t != nullptr, error = OTBR_ERROR_ERRNO);

    nftnl_table_set_str(t, NFTNL_TABLE_NAME, aTable.c_str());
    nftnl_table_set_u32(t, NFTNL_TABLE_FAMILY, NFPROTO_INET);

    {
        struct nlmsghdr *nlh =
            nftnl_table_nlmsg_build_hdr(reinterpret_cast<char *>(mnl_nlmsg_batch_current(mBatch)), NFT_MSG_NEWTABLE,
                                        NFPROTO_INET, NLM_F_CREATE | NLM_F_ACK, mSeq++);
        nftnl_table_nlmsg_build_payload(nlh, t);
        SuccessOrExit(error = AdvanceBatch());
    }

exit:
    if (t != nullptr)
    {
        nftnl_table_free(t);
    }
    return error;
}

otbrError Nftables::AddChain(const std::string &aTable,
                             const std::string &aChain,
                             Hook               aHook,
                             ChainPriority      aPriority,
                             ChainType          aType)
{
    otbrError           error = OTBR_ERROR_NONE;
    struct nftnl_chain *c     = nullptr;

    VerifyOrExit(mInBatch, error = OTBR_ERROR_INVALID_STATE);

    c = nftnl_chain_alloc();
    VerifyOrExit(c != nullptr, error = OTBR_ERROR_ERRNO);

    nftnl_chain_set_str(c, NFTNL_CHAIN_TABLE, aTable.c_str());
    nftnl_chain_set_str(c, NFTNL_CHAIN_NAME, aChain.c_str());
    nftnl_chain_set_u32(c, NFTNL_CHAIN_FAMILY, NFPROTO_INET);
    nftnl_chain_set_u32(c, NFTNL_CHAIN_HOOKNUM, HookToHooknum(aHook));
    nftnl_chain_set_s32(c, NFTNL_CHAIN_PRIO, static_cast<int32_t>(aPriority));
    nftnl_chain_set_str(c, NFTNL_CHAIN_TYPE, ChainTypeToString(aType));

    {
        struct nlmsghdr *nlh =
            nftnl_chain_nlmsg_build_hdr(reinterpret_cast<char *>(mnl_nlmsg_batch_current(mBatch)), NFT_MSG_NEWCHAIN,
                                        NFPROTO_INET, NLM_F_CREATE | NLM_F_ACK, mSeq++);
        nftnl_chain_nlmsg_build_payload(nlh, c);
        SuccessOrExit(error = AdvanceBatch());
    }

exit:
    if (c != nullptr)
    {
        nftnl_chain_free(c);
    }
    return error;
}

otbrError Nftables::AddIp6PrefixSet(const std::string &aTable, const std::string &aSet)
{
    otbrError         error = OTBR_ERROR_NONE;
    struct nftnl_set *s     = nullptr;

    VerifyOrExit(mInBatch, error = OTBR_ERROR_INVALID_STATE);

    s = nftnl_set_alloc();
    VerifyOrExit(s != nullptr, error = OTBR_ERROR_ERRNO);

    nftnl_set_set_str(s, NFTNL_SET_TABLE, aTable.c_str());
    nftnl_set_set_str(s, NFTNL_SET_NAME, aSet.c_str());
    nftnl_set_set_u32(s, NFTNL_SET_FAMILY, NFPROTO_INET);
    // Interval flag gives the prefix-match semantics of `ipset hash:net family inet6`.
    nftnl_set_set_u32(s, NFTNL_SET_FLAGS, NFT_SET_INTERVAL);
    nftnl_set_set_u32(s, NFTNL_SET_KEY_LEN, 16);
    // The key type is opaque to the kernel but stored and echoed back to
    // userspace: without it, nft(8) prints "type 0x0 [invalid type]" for the
    // set and cannot render its elements. 8 is nft's TYPE_IP6ADDR.
    nftnl_set_set_u32(s, NFTNL_SET_KEY_TYPE, 8);
    // Set IDs only need to be unique within a batch; reuse mSeq for that.
    nftnl_set_set_u32(s, NFTNL_SET_ID, mSeq);

    {
        struct nlmsghdr *nlh =
            nftnl_set_nlmsg_build_hdr(reinterpret_cast<char *>(mnl_nlmsg_batch_current(mBatch)), NFT_MSG_NEWSET,
                                      NFPROTO_INET, NLM_F_CREATE | NLM_F_ACK, mSeq++);
        nftnl_set_nlmsg_build_payload(nlh, s);
        SuccessOrExit(error = AdvanceBatch());
    }

exit:
    if (s != nullptr)
    {
        nftnl_set_free(s);
    }
    return error;
}

namespace {

// Adds 2^(128-aN) to the 128-bit big-endian value in @p aOut. Used to compute
// the (exclusive) upper bound of an IPv6 prefix interval. Wraps to zero on
// overflow (only happens for aN==0, which is /0 ::/0 — not a use case here).
void AddPrefixIncrement(uint8_t aOut[16], uint8_t aN)
{
    uint16_t carry;
    uint8_t  bitFromLsb;
    uint8_t  byteFromLsb;
    int      byteIdx;

    if (aN == 0)
    {
        memset(aOut, 0, 16);
        return;
    }

    bitFromLsb  = static_cast<uint8_t>(128 - aN);
    byteFromLsb = bitFromLsb / 8;
    byteIdx     = 15 - byteFromLsb;
    carry       = static_cast<uint16_t>(1u << (bitFromLsb % 8));

    for (int i = byteIdx; i >= 0 && carry != 0; i--)
    {
        uint16_t sum = static_cast<uint16_t>(aOut[i]) + carry;
        aOut[i]      = static_cast<uint8_t>(sum & 0xff);
        carry        = static_cast<uint16_t>(sum >> 8);
    }
}

// Zero the host bits of @p aAddr so it is aligned to @p aPrefixLen.
void ApplyPrefixMask(uint8_t aAddr[16], uint8_t aPrefixLen)
{
    if (aPrefixLen >= 128)
    {
        return;
    }
    for (uint8_t byteIndex = 0; byteIndex < 16; byteIndex++)
    {
        if (byteIndex * 8 >= aPrefixLen)
        {
            aAddr[byteIndex] = 0;
        }
        else if (byteIndex * 8 + 8 > aPrefixLen)
        {
            aAddr[byteIndex] &= static_cast<uint8_t>(0xff << (8 - (aPrefixLen - byteIndex * 8)));
        }
    }
}

// Build the start/end pair for a prefix interval and attach them to @p aSet.
// The end element carries NFT_SET_ELEM_INTERVAL_END to mark it as the
// (exclusive) upper bound.
otbrError AttachPrefixInterval(struct nftnl_set *aSet, const Ip6Prefix &aPrefix)
{
    otbrError              error = OTBR_ERROR_NONE;
    struct nftnl_set_elem *startElem;
    struct nftnl_set_elem *endElem;
    uint8_t                startAddr[16];
    uint8_t                endAddr[16];

    // Callers validate this, but an out-of-range length would silently produce a
    // wrong interval here rather than a diagnosable failure, so reject it.
    VerifyOrExit(aPrefix.mLength <= 128, error = OTBR_ERROR_INVALID_ARGS);

    memcpy(startAddr, aPrefix.mPrefix.m8, 16);
    // Zero the host bits so the start element is prefix-aligned.
    ApplyPrefixMask(startAddr, aPrefix.mLength);
    memcpy(endAddr, startAddr, 16);
    AddPrefixIncrement(endAddr, aPrefix.mLength);

    startElem = nftnl_set_elem_alloc();
    VerifyOrExit(startElem != nullptr, error = OTBR_ERROR_ERRNO);
    nftnl_set_elem_set(startElem, NFTNL_SET_ELEM_KEY, startAddr, sizeof(startAddr));
    nftnl_set_elem_add(aSet, startElem);

    endElem = nftnl_set_elem_alloc();
    VerifyOrExit(endElem != nullptr, error = OTBR_ERROR_ERRNO);
    nftnl_set_elem_set(endElem, NFTNL_SET_ELEM_KEY, endAddr, sizeof(endAddr));
    nftnl_set_elem_set_u32(endElem, NFTNL_SET_ELEM_FLAGS, NFT_SET_ELEM_INTERVAL_END);
    nftnl_set_elem_add(aSet, endElem);

exit:
    return error;
}

} // namespace

otbrError Nftables::AddSetElement(const std::string &aTable, const std::string &aSet, const Ip6Prefix &aPrefix)
{
    otbrError         error = OTBR_ERROR_NONE;
    struct nftnl_set *s     = nullptr;

    VerifyOrExit(mInBatch, error = OTBR_ERROR_INVALID_STATE);

    s = nftnl_set_alloc();
    VerifyOrExit(s != nullptr, error = OTBR_ERROR_ERRNO);

    nftnl_set_set_str(s, NFTNL_SET_TABLE, aTable.c_str());
    nftnl_set_set_str(s, NFTNL_SET_NAME, aSet.c_str());
    nftnl_set_set_u32(s, NFTNL_SET_FAMILY, NFPROTO_INET);

    SuccessOrExit(error = AttachPrefixInterval(s, aPrefix));

    {
        struct nlmsghdr *nlh =
            nftnl_nlmsg_build_hdr(reinterpret_cast<char *>(mnl_nlmsg_batch_current(mBatch)), NFT_MSG_NEWSETELEM,
                                  NFPROTO_INET, NLM_F_CREATE | NLM_F_ACK, mSeq++);
        nftnl_set_elems_nlmsg_build_payload(nlh, s);
        SuccessOrExit(error = AdvanceBatch());
    }

exit:
    if (s != nullptr)
    {
        nftnl_set_free(s);
    }
    return error;
}

otbrError Nftables::DelSetElement(const std::string &aTable, const std::string &aSet, const Ip6Prefix &aPrefix)
{
    otbrError         error = OTBR_ERROR_NONE;
    struct nftnl_set *s     = nullptr;

    VerifyOrExit(mInBatch, error = OTBR_ERROR_INVALID_STATE);

    s = nftnl_set_alloc();
    VerifyOrExit(s != nullptr, error = OTBR_ERROR_ERRNO);

    nftnl_set_set_str(s, NFTNL_SET_TABLE, aTable.c_str());
    nftnl_set_set_str(s, NFTNL_SET_NAME, aSet.c_str());
    nftnl_set_set_u32(s, NFTNL_SET_FAMILY, NFPROTO_INET);

    SuccessOrExit(error = AttachPrefixInterval(s, aPrefix));

    {
        struct nlmsghdr *nlh = nftnl_nlmsg_build_hdr(reinterpret_cast<char *>(mnl_nlmsg_batch_current(mBatch)),
                                                     NFT_MSG_DELSETELEM, NFPROTO_INET, NLM_F_ACK, mSeq++);
        nftnl_set_elems_nlmsg_build_payload(nlh, s);
        SuccessOrExit(error = AdvanceBatch());
    }

exit:
    if (s != nullptr)
    {
        nftnl_set_free(s);
    }
    return error;
}

otbrError Nftables::FlushSet(const std::string &aTable, const std::string &aSet)
{
    otbrError         error = OTBR_ERROR_NONE;
    struct nftnl_set *s     = nullptr;

    VerifyOrExit(mInBatch, error = OTBR_ERROR_INVALID_STATE);

    s = nftnl_set_alloc();
    VerifyOrExit(s != nullptr, error = OTBR_ERROR_ERRNO);

    nftnl_set_set_str(s, NFTNL_SET_TABLE, aTable.c_str());
    nftnl_set_set_str(s, NFTNL_SET_NAME, aSet.c_str());
    nftnl_set_set_u32(s, NFTNL_SET_FAMILY, NFPROTO_INET);
    // No elements attached — kernel interprets DELSETELEM with empty element
    // list as "flush all".

    {
        struct nlmsghdr *nlh = nftnl_nlmsg_build_hdr(reinterpret_cast<char *>(mnl_nlmsg_batch_current(mBatch)),
                                                     NFT_MSG_DELSETELEM, NFPROTO_INET, NLM_F_ACK, mSeq++);
        nftnl_set_elems_nlmsg_build_payload(nlh, s);
        SuccessOrExit(error = AdvanceBatch());
    }

exit:
    if (s != nullptr)
    {
        nftnl_set_free(s);
    }
    return error;
}

namespace {

void ComputePrefixMask(uint8_t aMask[16], uint8_t aPrefixLen)
{
    for (uint8_t byteIndex = 0; byteIndex < 16; byteIndex++)
    {
        if (byteIndex * 8 + 8 <= aPrefixLen)
        {
            aMask[byteIndex] = 0xff;
        }
        else if (byteIndex * 8 < aPrefixLen)
        {
            aMask[byteIndex] = static_cast<uint8_t>(0xff << (8 - (aPrefixLen - byteIndex * 8)));
        }
        else
        {
            aMask[byteIndex] = 0;
        }
    }
}

struct nftnl_expr *MakeMetaLoad(uint32_t aKey, uint32_t aDreg)
{
    struct nftnl_expr *e = nftnl_expr_alloc("meta");
    if (e != nullptr)
    {
        nftnl_expr_set_u32(e, NFTNL_EXPR_META_KEY, aKey);
        nftnl_expr_set_u32(e, NFTNL_EXPR_META_DREG, aDreg);
    }
    return e;
}

struct nftnl_expr *MakeCmpEq(uint32_t aSreg, const void *aData, uint32_t aDataLen)
{
    struct nftnl_expr *e = nftnl_expr_alloc("cmp");
    if (e != nullptr)
    {
        nftnl_expr_set_u32(e, NFTNL_EXPR_CMP_OP, NFT_CMP_EQ);
        nftnl_expr_set_u32(e, NFTNL_EXPR_CMP_SREG, aSreg);
        nftnl_expr_set(e, NFTNL_EXPR_CMP_DATA, aData, aDataLen);
    }
    return e;
}

struct nftnl_expr *MakePayloadLoad(uint32_t aBase, uint32_t aOffset, uint32_t aLen, uint32_t aDreg)
{
    struct nftnl_expr *e = nftnl_expr_alloc("payload");
    if (e != nullptr)
    {
        nftnl_expr_set_u32(e, NFTNL_EXPR_PAYLOAD_BASE, aBase);
        nftnl_expr_set_u32(e, NFTNL_EXPR_PAYLOAD_OFFSET, aOffset);
        nftnl_expr_set_u32(e, NFTNL_EXPR_PAYLOAD_LEN, aLen);
        nftnl_expr_set_u32(e, NFTNL_EXPR_PAYLOAD_DREG, aDreg);
    }
    return e;
}

struct nftnl_expr *MakeBitwiseMask(uint32_t aReg, const uint8_t *aMask, uint32_t aLen)
{
    uint8_t            zeros[16] = {0};
    struct nftnl_expr *e;

    // The only caller passes an IPv6 address length, but the XOR operand below is
    // read from this fixed-size buffer, so refuse anything that would overrun it.
    VerifyOrExit(aLen <= sizeof(zeros), e = nullptr);

    e = nftnl_expr_alloc("bitwise");
    if (e != nullptr)
    {
        nftnl_expr_set_u32(e, NFTNL_EXPR_BITWISE_SREG, aReg);
        nftnl_expr_set_u32(e, NFTNL_EXPR_BITWISE_DREG, aReg);
        nftnl_expr_set_u32(e, NFTNL_EXPR_BITWISE_LEN, aLen);
        nftnl_expr_set(e, NFTNL_EXPR_BITWISE_MASK, aMask, aLen);
        nftnl_expr_set(e, NFTNL_EXPR_BITWISE_XOR, zeros, aLen);
    }

exit:
    return e;
}

struct nftnl_expr *MakeQueueBypass(uint16_t aQueueNum)
{
    struct nftnl_expr *e = nftnl_expr_alloc("queue");
    if (e != nullptr)
    {
        nftnl_expr_set_u16(e, NFTNL_EXPR_QUEUE_NUM, aQueueNum);
        nftnl_expr_set_u16(e, NFTNL_EXPR_QUEUE_TOTAL, 1);
        nftnl_expr_set_u16(e, NFTNL_EXPR_QUEUE_FLAGS, NFT_QUEUE_FLAG_BYPASS);
    }
    return e;
}

struct nftnl_expr *MakeCmpNeq(uint32_t aSreg, const void *aData, uint32_t aDataLen)
{
    struct nftnl_expr *e = nftnl_expr_alloc("cmp");
    if (e != nullptr)
    {
        nftnl_expr_set_u32(e, NFTNL_EXPR_CMP_OP, NFT_CMP_NEQ);
        nftnl_expr_set_u32(e, NFTNL_EXPR_CMP_SREG, aSreg);
        nftnl_expr_set(e, NFTNL_EXPR_CMP_DATA, aData, aDataLen);
    }
    return e;
}

struct nftnl_expr *MakeImmediateVerdict(uint32_t aVerdict)
{
    struct nftnl_expr *e = nftnl_expr_alloc("immediate");
    if (e != nullptr)
    {
        nftnl_expr_set_u32(e, NFTNL_EXPR_IMM_DREG, NFT_REG_VERDICT);
        nftnl_expr_set_u32(e, NFTNL_EXPR_IMM_VERDICT, aVerdict);
    }
    return e;
}

struct nftnl_expr *MakeSetLookup(const std::string &aSet, uint32_t aSreg)
{
    struct nftnl_expr *e = nftnl_expr_alloc("lookup");
    if (e != nullptr)
    {
        nftnl_expr_set_str(e, NFTNL_EXPR_LOOKUP_SET, aSet.c_str());
        nftnl_expr_set_u32(e, NFTNL_EXPR_LOOKUP_SREG, aSreg);
    }
    return e;
}

uint32_t VerdictToNft(Verdict aVerdict)
{
    switch (aVerdict)
    {
    case Verdict::kAccept:
        return NF_ACCEPT;
    case Verdict::kDrop:
        return NF_DROP;
    case Verdict::kReturn:
        return NFT_RETURN;
    }
    return NF_DROP;
}

uint8_t PktTypeToWire(PktType aPktType)
{
    // <linux/if_packet.h>: PACKET_HOST = 0 (unicast to us / host).
    // nftables maps `pkttype unicast` to PACKET_HOST.
    switch (aPktType)
    {
    case PktType::kUnicast:
        return 0; // PACKET_HOST
    }
    return 0;
}

/// Writes an interface name into the fixed-size field the kernel expects.
///
/// A name that does not fit is rejected rather than truncated: a truncated
/// name is still a valid name, just of a different interface, so the rule
/// would be installed against the wrong one. The check lives here rather than
/// in each caller so that it cannot be forgotten by a new one.
otbrError IfNameToWire(const std::string &aName, char (&aOut)[IFNAMSIZ])
{
    otbrError error = OTBR_ERROR_NONE;

    VerifyOrExit(aName.size() < IFNAMSIZ, error = OTBR_ERROR_INVALID_ARGS);

    memset(aOut, 0, IFNAMSIZ);
    memcpy(aOut, aName.c_str(), aName.size());

exit:
    return error;
}

/// Appends an expression to a rule, failing if the expression could not be
/// allocated. The Make* helpers above return nullptr when nftnl_expr_alloc()
/// fails, and nftnl_rule_add_expr() would dereference it.
otbrError AddExpr(struct nftnl_rule *aRule, struct nftnl_expr *aExpr)
{
    otbrError error = OTBR_ERROR_NONE;

    VerifyOrExit(aExpr != nullptr, errno = ENOMEM, error = OTBR_ERROR_ERRNO);
    nftnl_rule_add_expr(aRule, aExpr);

exit:
    return error;
}

struct nftnl_expr *MakeImmediateData(uint32_t aDreg, const void *aData, uint32_t aDataLen)
{
    struct nftnl_expr *e = nftnl_expr_alloc("immediate");
    if (e != nullptr)
    {
        nftnl_expr_set_u32(e, NFTNL_EXPR_IMM_DREG, aDreg);
        nftnl_expr_set(e, NFTNL_EXPR_IMM_DATA, aData, aDataLen);
    }
    return e;
}

struct nftnl_expr *MakeMetaSet(uint32_t aKey, uint32_t aSreg)
{
    struct nftnl_expr *e = nftnl_expr_alloc("meta");
    if (e != nullptr)
    {
        nftnl_expr_set_u32(e, NFTNL_EXPR_META_KEY, aKey);
        nftnl_expr_set_u32(e, NFTNL_EXPR_META_SREG, aSreg);
    }
    return e;
}

struct nftnl_expr *MakeMasquerade(void)
{
    return nftnl_expr_alloc("masq");
}

} // namespace

otbrError Nftables::QueueNewRule(struct nftnl_rule *aRule, uint64_t *aHandleOut)
{
    // The echo reply is what carries the kernel-assigned handle back, so it is
    // only requested when the caller wants the handle; for static rules it
    // would be generated and then thrown away.
    const uint16_t   flags = NLM_F_APPEND | NLM_F_CREATE | NLM_F_ACK | ((aHandleOut != nullptr) ? NLM_F_ECHO : 0);
    struct nlmsghdr *nlh   = nftnl_rule_nlmsg_build_hdr(reinterpret_cast<char *>(mnl_nlmsg_batch_current(mBatch)),
                                                        NFT_MSG_NEWRULE, NFPROTO_INET, flags, mSeq);
    nftnl_rule_nlmsg_build_payload(nlh, aRule);

    if (aHandleOut != nullptr)
    {
        mPendingHandles[mSeq] = aHandleOut;
    }
    mSeq++;

    return AdvanceBatch();
}

otbrError Nftables::AddRuleNdNsRedirect(const std::string &aTable,
                                        const std::string &aChain,
                                        const Ip6Prefix   &aDaddrPrefix,
                                        const std::string &aIifname,
                                        uint16_t           aQueueNum,
                                        uint64_t          *aHandleOut)
{
    otbrError          error      = OTBR_ERROR_NONE;
    struct nftnl_rule *rule       = nullptr;
    uint8_t            ipv6Family = NFPROTO_IPV6;
    uint8_t            icmp6Proto = IPPROTO_ICMPV6;
    uint8_t            nsType     = ND_NEIGHBOR_SOLICIT;
    uint8_t            maskedAddr[16];
    uint8_t            mask[16];
    char               iifBuf[IFNAMSIZ];

    VerifyOrExit(mInBatch, error = OTBR_ERROR_INVALID_STATE);

    memcpy(maskedAddr, aDaddrPrefix.mPrefix.m8, sizeof(maskedAddr));
    ApplyPrefixMask(maskedAddr, aDaddrPrefix.mLength);
    ComputePrefixMask(mask, aDaddrPrefix.mLength);

    SuccessOrExit(error = IfNameToWire(aIifname, iifBuf));

    rule = nftnl_rule_alloc();
    VerifyOrExit(rule != nullptr, error = OTBR_ERROR_ERRNO);

    nftnl_rule_set_str(rule, NFTNL_RULE_TABLE, aTable.c_str());
    nftnl_rule_set_str(rule, NFTNL_RULE_CHAIN, aChain.c_str());
    nftnl_rule_set_u32(rule, NFTNL_RULE_FAMILY, NFPROTO_INET);

    // meta nfproto ipv6 — required because the inet table can also see IPv4.
    SuccessOrExit(error = AddExpr(rule, MakeMetaLoad(NFT_META_NFPROTO, NFT_REG_1)));
    SuccessOrExit(error = AddExpr(rule, MakeCmpEq(NFT_REG_1, &ipv6Family, sizeof(ipv6Family))));

    // ip6 daddr matches prefix /N: load 16 bytes at network-header offset 24,
    // optionally bitwise-AND with the prefix mask, then compare against the
    // prefix-aligned address.
    SuccessOrExit(error = AddExpr(rule, MakePayloadLoad(NFT_PAYLOAD_NETWORK_HEADER, 24, 16, NFT_REG_1)));
    if (aDaddrPrefix.mLength < 128)
    {
        SuccessOrExit(error = AddExpr(rule, MakeBitwiseMask(NFT_REG_1, mask, 16)));
    }
    SuccessOrExit(error = AddExpr(rule, MakeCmpEq(NFT_REG_1, maskedAddr, 16)));

    // meta l4proto icmpv6
    SuccessOrExit(error = AddExpr(rule, MakeMetaLoad(NFT_META_L4PROTO, NFT_REG_1)));
    SuccessOrExit(error = AddExpr(rule, MakeCmpEq(NFT_REG_1, &icmp6Proto, sizeof(icmp6Proto))));

    // icmpv6 type == nd-neighbor-solicit (135): byte 0 of the transport header.
    SuccessOrExit(error = AddExpr(rule, MakePayloadLoad(NFT_PAYLOAD_TRANSPORT_HEADER, 0, 1, NFT_REG_1)));
    SuccessOrExit(error = AddExpr(rule, MakeCmpEq(NFT_REG_1, &nsType, sizeof(nsType))));

    // meta iifname == <ifname>
    SuccessOrExit(error = AddExpr(rule, MakeMetaLoad(NFT_META_IIFNAME, NFT_REG_1)));
    SuccessOrExit(error = AddExpr(rule, MakeCmpEq(NFT_REG_1, iifBuf, IFNAMSIZ)));

    // queue num <q> bypass
    SuccessOrExit(error = AddExpr(rule, MakeQueueBypass(aQueueNum)));

    error = QueueNewRule(rule, aHandleOut);

exit:
    if (rule != nullptr)
    {
        nftnl_rule_free(rule);
    }
    return error;
}

otbrError Nftables::AddRuleOifnameNeqReturn(const std::string &aTable,
                                            const std::string &aChain,
                                            const std::string &aOifname,
                                            uint64_t          *aHandleOut)
{
    otbrError          error = OTBR_ERROR_NONE;
    struct nftnl_rule *rule  = nullptr;
    char               oifBuf[IFNAMSIZ];

    VerifyOrExit(mInBatch, error = OTBR_ERROR_INVALID_STATE);

    SuccessOrExit(error = IfNameToWire(aOifname, oifBuf));

    rule = nftnl_rule_alloc();
    VerifyOrExit(rule != nullptr, error = OTBR_ERROR_ERRNO);

    nftnl_rule_set_str(rule, NFTNL_RULE_TABLE, aTable.c_str());
    nftnl_rule_set_str(rule, NFTNL_RULE_CHAIN, aChain.c_str());
    nftnl_rule_set_u32(rule, NFTNL_RULE_FAMILY, NFPROTO_INET);

    SuccessOrExit(error = AddExpr(rule, MakeMetaLoad(NFT_META_OIFNAME, NFT_REG_1)));
    SuccessOrExit(error = AddExpr(rule, MakeCmpNeq(NFT_REG_1, oifBuf, IFNAMSIZ)));
    SuccessOrExit(error = AddExpr(rule, MakeImmediateVerdict(NFT_RETURN)));

    error = QueueNewRule(rule, aHandleOut);

exit:
    if (rule != nullptr)
    {
        nftnl_rule_free(rule);
    }
    return error;
}

otbrError Nftables::AddRuleIifPkttypeVerdict(const std::string &aTable,
                                             const std::string &aChain,
                                             const std::string &aIifname,
                                             PktType            aPktType,
                                             Verdict            aVerdict,
                                             uint64_t          *aHandleOut)
{
    otbrError          error = OTBR_ERROR_NONE;
    struct nftnl_rule *rule  = nullptr;
    char               iifBuf[IFNAMSIZ];
    uint8_t            pktType;

    VerifyOrExit(mInBatch, error = OTBR_ERROR_INVALID_STATE);

    SuccessOrExit(error = IfNameToWire(aIifname, iifBuf));
    pktType = PktTypeToWire(aPktType);

    rule = nftnl_rule_alloc();
    VerifyOrExit(rule != nullptr, error = OTBR_ERROR_ERRNO);

    nftnl_rule_set_str(rule, NFTNL_RULE_TABLE, aTable.c_str());
    nftnl_rule_set_str(rule, NFTNL_RULE_CHAIN, aChain.c_str());
    nftnl_rule_set_u32(rule, NFTNL_RULE_FAMILY, NFPROTO_INET);

    SuccessOrExit(error = AddExpr(rule, MakeMetaLoad(NFT_META_IIFNAME, NFT_REG_1)));
    SuccessOrExit(error = AddExpr(rule, MakeCmpEq(NFT_REG_1, iifBuf, IFNAMSIZ)));
    SuccessOrExit(error = AddExpr(rule, MakeMetaLoad(NFT_META_PKTTYPE, NFT_REG_1)));
    SuccessOrExit(error = AddExpr(rule, MakeCmpEq(NFT_REG_1, &pktType, sizeof(pktType))));
    SuccessOrExit(error = AddExpr(rule, MakeImmediateVerdict(VerdictToNft(aVerdict))));

    error = QueueNewRule(rule, aHandleOut);

exit:
    if (rule != nullptr)
    {
        nftnl_rule_free(rule);
    }
    return error;
}

otbrError Nftables::AddRulePkttypeVerdict(const std::string &aTable,
                                          const std::string &aChain,
                                          PktType            aPktType,
                                          Verdict            aVerdict,
                                          uint64_t          *aHandleOut)
{
    otbrError          error = OTBR_ERROR_NONE;
    struct nftnl_rule *rule  = nullptr;
    uint8_t            pktType;

    VerifyOrExit(mInBatch, error = OTBR_ERROR_INVALID_STATE);

    pktType = PktTypeToWire(aPktType);

    rule = nftnl_rule_alloc();
    VerifyOrExit(rule != nullptr, error = OTBR_ERROR_ERRNO);

    nftnl_rule_set_str(rule, NFTNL_RULE_TABLE, aTable.c_str());
    nftnl_rule_set_str(rule, NFTNL_RULE_CHAIN, aChain.c_str());
    nftnl_rule_set_u32(rule, NFTNL_RULE_FAMILY, NFPROTO_INET);

    SuccessOrExit(error = AddExpr(rule, MakeMetaLoad(NFT_META_PKTTYPE, NFT_REG_1)));
    SuccessOrExit(error = AddExpr(rule, MakeCmpEq(NFT_REG_1, &pktType, sizeof(pktType))));
    SuccessOrExit(error = AddExpr(rule, MakeImmediateVerdict(VerdictToNft(aVerdict))));

    error = QueueNewRule(rule, aHandleOut);

exit:
    if (rule != nullptr)
    {
        nftnl_rule_free(rule);
    }
    return error;
}

otbrError Nftables::AddRuleSetLookupVerdict(const std::string &aTable,
                                            const std::string &aChain,
                                            const std::string &aSet,
                                            SetDirection       aDir,
                                            Verdict            aVerdict,
                                            uint64_t          *aHandleOut)
{
    otbrError          error      = OTBR_ERROR_NONE;
    struct nftnl_rule *rule       = nullptr;
    uint8_t            ipv6Family = NFPROTO_IPV6;
    // ip6_src is at offset 8, ip6_dst at offset 24, both 16 bytes long.
    uint32_t addrOffset = (aDir == SetDirection::kSrc) ? 8u : 24u;

    VerifyOrExit(mInBatch, error = OTBR_ERROR_INVALID_STATE);

    rule = nftnl_rule_alloc();
    VerifyOrExit(rule != nullptr, error = OTBR_ERROR_ERRNO);

    nftnl_rule_set_str(rule, NFTNL_RULE_TABLE, aTable.c_str());
    nftnl_rule_set_str(rule, NFTNL_RULE_CHAIN, aChain.c_str());
    nftnl_rule_set_u32(rule, NFTNL_RULE_FAMILY, NFPROTO_INET);

    // Constrain to IPv6 (the inet table also sees IPv4).
    SuccessOrExit(error = AddExpr(rule, MakeMetaLoad(NFT_META_NFPROTO, NFT_REG_1)));
    SuccessOrExit(error = AddExpr(rule, MakeCmpEq(NFT_REG_1, &ipv6Family, sizeof(ipv6Family))));

    SuccessOrExit(error = AddExpr(rule, MakePayloadLoad(NFT_PAYLOAD_NETWORK_HEADER, addrOffset, 16, NFT_REG_1)));
    SuccessOrExit(error = AddExpr(rule, MakeSetLookup(aSet, NFT_REG_1)));
    SuccessOrExit(error = AddExpr(rule, MakeImmediateVerdict(VerdictToNft(aVerdict))));

    error = QueueNewRule(rule, aHandleOut);

exit:
    if (rule != nullptr)
    {
        nftnl_rule_free(rule);
    }
    return error;
}

otbrError Nftables::AddRuleVerdict(const std::string &aTable,
                                   const std::string &aChain,
                                   Verdict            aVerdict,
                                   uint64_t          *aHandleOut)
{
    otbrError          error = OTBR_ERROR_NONE;
    struct nftnl_rule *rule  = nullptr;

    VerifyOrExit(mInBatch, error = OTBR_ERROR_INVALID_STATE);

    rule = nftnl_rule_alloc();
    VerifyOrExit(rule != nullptr, error = OTBR_ERROR_ERRNO);

    nftnl_rule_set_str(rule, NFTNL_RULE_TABLE, aTable.c_str());
    nftnl_rule_set_str(rule, NFTNL_RULE_CHAIN, aChain.c_str());
    nftnl_rule_set_u32(rule, NFTNL_RULE_FAMILY, NFPROTO_INET);

    SuccessOrExit(error = AddExpr(rule, MakeImmediateVerdict(VerdictToNft(aVerdict))));

    error = QueueNewRule(rule, aHandleOut);

exit:
    if (rule != nullptr)
    {
        nftnl_rule_free(rule);
    }
    return error;
}

otbrError Nftables::AddRuleIifMark(const std::string &aTable,
                                   const std::string &aChain,
                                   const std::string &aIifname,
                                   uint32_t           aMark,
                                   uint64_t          *aHandleOut)
{
    otbrError          error = OTBR_ERROR_NONE;
    struct nftnl_rule *rule  = nullptr;
    char               iifBuf[IFNAMSIZ];
    uint8_t            ipv4Family = NFPROTO_IPV4;
    // NFT_META_MARK is host-byte-order in registers; nft userspace stores
    // mark values that way too. Same byte order on set and match.
    uint32_t markValue = aMark;

    VerifyOrExit(mInBatch, error = OTBR_ERROR_INVALID_STATE);

    SuccessOrExit(error = IfNameToWire(aIifname, iifBuf));

    rule = nftnl_rule_alloc();
    VerifyOrExit(rule != nullptr, error = OTBR_ERROR_ERRNO);

    nftnl_rule_set_str(rule, NFTNL_RULE_TABLE, aTable.c_str());
    nftnl_rule_set_str(rule, NFTNL_RULE_CHAIN, aChain.c_str());
    nftnl_rule_set_u32(rule, NFTNL_RULE_FAMILY, NFPROTO_INET);

    // meta nfproto ipv4 — this is a NAT44 mark; without it, the masquerade rule
    // that matches on the mark would also NAT66 IPv6 traffic in the inet table.
    SuccessOrExit(error = AddExpr(rule, MakeMetaLoad(NFT_META_NFPROTO, NFT_REG_1)));
    SuccessOrExit(error = AddExpr(rule, MakeCmpEq(NFT_REG_1, &ipv4Family, sizeof(ipv4Family))));

    // iifname == <ifname>
    SuccessOrExit(error = AddExpr(rule, MakeMetaLoad(NFT_META_IIFNAME, NFT_REG_1)));
    SuccessOrExit(error = AddExpr(rule, MakeCmpEq(NFT_REG_1, iifBuf, IFNAMSIZ)));

    // meta mark set <aMark>
    SuccessOrExit(error = AddExpr(rule, MakeImmediateData(NFT_REG_1, &markValue, sizeof(markValue))));
    SuccessOrExit(error = AddExpr(rule, MakeMetaSet(NFT_META_MARK, NFT_REG_1)));

    error = QueueNewRule(rule, aHandleOut);

exit:
    if (rule != nullptr)
    {
        nftnl_rule_free(rule);
    }
    return error;
}

otbrError Nftables::AddRuleMarkMasquerade(const std::string &aTable,
                                          const std::string &aChain,
                                          uint32_t           aMark,
                                          uint64_t          *aHandleOut)
{
    otbrError          error     = OTBR_ERROR_NONE;
    struct nftnl_rule *rule      = nullptr;
    uint32_t           markValue = aMark;

    VerifyOrExit(mInBatch, error = OTBR_ERROR_INVALID_STATE);

    rule = nftnl_rule_alloc();
    VerifyOrExit(rule != nullptr, error = OTBR_ERROR_ERRNO);

    nftnl_rule_set_str(rule, NFTNL_RULE_TABLE, aTable.c_str());
    nftnl_rule_set_str(rule, NFTNL_RULE_CHAIN, aChain.c_str());
    nftnl_rule_set_u32(rule, NFTNL_RULE_FAMILY, NFPROTO_INET);

    // meta mark <aMark>
    SuccessOrExit(error = AddExpr(rule, MakeMetaLoad(NFT_META_MARK, NFT_REG_1)));
    SuccessOrExit(error = AddExpr(rule, MakeCmpEq(NFT_REG_1, &markValue, sizeof(markValue))));

    // masquerade
    SuccessOrExit(error = AddExpr(rule, MakeMasquerade()));

    error = QueueNewRule(rule, aHandleOut);

exit:
    if (rule != nullptr)
    {
        nftnl_rule_free(rule);
    }
    return error;
}

otbrError Nftables::AddRuleOifnameVerdict(const std::string &aTable,
                                          const std::string &aChain,
                                          const std::string &aOifname,
                                          Verdict            aVerdict,
                                          uint64_t          *aHandleOut)
{
    otbrError          error = OTBR_ERROR_NONE;
    struct nftnl_rule *rule  = nullptr;
    char               oifBuf[IFNAMSIZ];

    VerifyOrExit(mInBatch, error = OTBR_ERROR_INVALID_STATE);

    SuccessOrExit(error = IfNameToWire(aOifname, oifBuf));

    rule = nftnl_rule_alloc();
    VerifyOrExit(rule != nullptr, error = OTBR_ERROR_ERRNO);

    nftnl_rule_set_str(rule, NFTNL_RULE_TABLE, aTable.c_str());
    nftnl_rule_set_str(rule, NFTNL_RULE_CHAIN, aChain.c_str());
    nftnl_rule_set_u32(rule, NFTNL_RULE_FAMILY, NFPROTO_INET);

    SuccessOrExit(error = AddExpr(rule, MakeMetaLoad(NFT_META_OIFNAME, NFT_REG_1)));
    SuccessOrExit(error = AddExpr(rule, MakeCmpEq(NFT_REG_1, oifBuf, IFNAMSIZ)));
    SuccessOrExit(error = AddExpr(rule, MakeImmediateVerdict(VerdictToNft(aVerdict))));

    error = QueueNewRule(rule, aHandleOut);

exit:
    if (rule != nullptr)
    {
        nftnl_rule_free(rule);
    }
    return error;
}

otbrError Nftables::AddRuleIifnameVerdict(const std::string &aTable,
                                          const std::string &aChain,
                                          const std::string &aIifname,
                                          Verdict            aVerdict,
                                          uint64_t          *aHandleOut)
{
    otbrError          error = OTBR_ERROR_NONE;
    struct nftnl_rule *rule  = nullptr;
    char               iifBuf[IFNAMSIZ];

    VerifyOrExit(mInBatch, error = OTBR_ERROR_INVALID_STATE);

    SuccessOrExit(error = IfNameToWire(aIifname, iifBuf));

    rule = nftnl_rule_alloc();
    VerifyOrExit(rule != nullptr, error = OTBR_ERROR_ERRNO);

    nftnl_rule_set_str(rule, NFTNL_RULE_TABLE, aTable.c_str());
    nftnl_rule_set_str(rule, NFTNL_RULE_CHAIN, aChain.c_str());
    nftnl_rule_set_u32(rule, NFTNL_RULE_FAMILY, NFPROTO_INET);

    SuccessOrExit(error = AddExpr(rule, MakeMetaLoad(NFT_META_IIFNAME, NFT_REG_1)));
    SuccessOrExit(error = AddExpr(rule, MakeCmpEq(NFT_REG_1, iifBuf, IFNAMSIZ)));
    SuccessOrExit(error = AddExpr(rule, MakeImmediateVerdict(VerdictToNft(aVerdict))));

    error = QueueNewRule(rule, aHandleOut);

exit:
    if (rule != nullptr)
    {
        nftnl_rule_free(rule);
    }
    return error;
}

otbrError Nftables::DelRule(const std::string &aTable, const std::string &aChain, uint64_t aHandle)
{
    otbrError          error = OTBR_ERROR_NONE;
    struct nftnl_rule *rule  = nullptr;

    VerifyOrExit(mInBatch, error = OTBR_ERROR_INVALID_STATE);

    rule = nftnl_rule_alloc();
    VerifyOrExit(rule != nullptr, error = OTBR_ERROR_ERRNO);

    nftnl_rule_set_str(rule, NFTNL_RULE_TABLE, aTable.c_str());
    nftnl_rule_set_str(rule, NFTNL_RULE_CHAIN, aChain.c_str());
    nftnl_rule_set_u32(rule, NFTNL_RULE_FAMILY, NFPROTO_INET);
    nftnl_rule_set_u64(rule, NFTNL_RULE_HANDLE, aHandle);

    {
        struct nlmsghdr *nlh = nftnl_rule_nlmsg_build_hdr(reinterpret_cast<char *>(mnl_nlmsg_batch_current(mBatch)),
                                                          NFT_MSG_DELRULE, NFPROTO_INET, NLM_F_ACK, mSeq++);
        nftnl_rule_nlmsg_build_payload(nlh, rule);
        SuccessOrExit(error = AdvanceBatch());
    }

exit:
    if (rule != nullptr)
    {
        nftnl_rule_free(rule);
    }
    return error;
}

} // namespace Firewall
} // namespace otbr

#endif // OTBR_ENABLE_NFTABLES
