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

/**
 * @file
 *   Concrete implementation of INftables on top of libnftnl/libmnl.
 *
 *   This header is only included by the implementation file and by code that
 *   needs to instantiate the real backend. Test code includes only nftables.hpp.
 */

#ifndef OTBR_FIREWALL_NFTABLES_IMPL_HPP_
#define OTBR_FIREWALL_NFTABLES_IMPL_HPP_

#include "openthread-br/config.h"

#if OTBR_ENABLE_NFTABLES

#include <stdint.h>

#include <map>

#include "common/code_utils.hpp"
#include "firewall/nftables.hpp"

struct mnl_socket;
struct mnl_nlmsg_batch;
struct nlmsghdr;

namespace otbr {
namespace Firewall {

/**
 * Talks to the kernel nftables subsystem via libnftnl over an mnl_socket.
 *
 * Phase 1 implements the socket lifecycle and idempotent table deletion;
 * higher-level rule construction is filled in by phases that exercise it
 * (Phase 2: ND-proxy redirect; Phase 3: ingress filter rules).
 */
class Nftables : public INftables, private NonCopyable
{
public:
    Nftables(void);
    ~Nftables(void) override;

    otbrError Init(void) override;
    otbrError Deinit(void) override;

    otbrError BeginBatch(void) override;
    otbrError CommitBatch(void) override;

    otbrError DelTable(const std::string &aTable) override;

    otbrError AddTable(const std::string &aTable) override;
    otbrError AddChain(const std::string &aTable,
                       const std::string &aChain,
                       Hook               aHook,
                       int                aPriority) override;
    otbrError AddIp6PrefixSet(const std::string &aTable, const std::string &aSet) override;

    otbrError AddSetElement(const std::string &aTable, const std::string &aSet, const Ip6Net &aPrefix) override;
    otbrError DelSetElement(const std::string &aTable, const std::string &aSet, const Ip6Net &aPrefix) override;
    otbrError FlushSet(const std::string &aTable, const std::string &aSet) override;

    otbrError AddRuleOifnameNeqReturn(const std::string &aTable,
                                      const std::string &aChain,
                                      const std::string &aOifname,
                                      uint64_t          *aHandleOut) override;
    otbrError AddRuleIifPkttypeVerdict(const std::string &aTable,
                                       const std::string &aChain,
                                       const std::string &aIifname,
                                       PktType            aPktType,
                                       Verdict            aVerdict,
                                       uint64_t          *aHandleOut) override;
    otbrError AddRulePkttypeVerdict(const std::string &aTable,
                                    const std::string &aChain,
                                    PktType            aPktType,
                                    Verdict            aVerdict,
                                    uint64_t          *aHandleOut) override;
    otbrError AddRuleSetLookupVerdict(const std::string &aTable,
                                      const std::string &aChain,
                                      const std::string &aSet,
                                      SetDirection       aDir,
                                      Verdict            aVerdict,
                                      uint64_t          *aHandleOut) override;
    otbrError AddRuleVerdict(const std::string &aTable,
                             const std::string &aChain,
                             Verdict            aVerdict,
                             uint64_t          *aHandleOut) override;
    otbrError AddRuleNdNsRedirect(const std::string &aTable,
                                  const std::string &aChain,
                                  const Ip6Net      &aDaddrPrefix,
                                  const std::string &aIifname,
                                  uint16_t           aQueueNum,
                                  uint64_t          *aHandleOut) override;

    otbrError DelRule(const std::string &aTable, const std::string &aChain, uint64_t aHandle) override;

private:
    static constexpr size_t kBatchPageSize = 8192;

    otbrError      SendStandalone(const void *aMessage, size_t aLength, bool aAllowEnoent);
    static int     HandleEchoReply(const struct nlmsghdr *aNlh, void *aContext);

    struct mnl_socket      *mSocket;
    struct mnl_nlmsg_batch *mBatch;
    void                   *mBatchBuffer;
    uint32_t                mPortId;
    uint32_t                mSeq;
    bool                    mInBatch;

    /// Maps message-sequence-number to the caller-provided handle out-pointer.
    /// CommitBatch() walks the kernel responses and writes handles back.
    std::map<uint32_t, uint64_t *> mPendingHandles;
};

} // namespace Firewall
} // namespace otbr

#endif // OTBR_ENABLE_NFTABLES

#endif // OTBR_FIREWALL_NFTABLES_IMPL_HPP_
