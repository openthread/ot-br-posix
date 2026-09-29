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

#define OTBR_LOG_TAG "FIREWALL"

#include "firewall/nftables_monitor.hpp"

#include <errno.h>
#include <string.h>

#include <linux/netfilter.h>
#include <linux/netfilter/nf_tables.h>
#include <linux/netfilter/nfnetlink.h>
#include <linux/netlink.h>

#include <libmnl/libmnl.h>
#include <libnftnl/table.h>

#include "common/logging.hpp"

namespace otbr {
namespace Firewall {

NftablesMonitor::NftablesMonitor(std::string aTableName, DeletedHandler aHandler)
    : mOwnedSocket(MakeUnique<MnlNetlinkSocket>())
    , mSocket(*mOwnedSocket)
    , mTableName(std::move(aTableName))
    , mHandler(std::move(aHandler))
    , mOwnPortId(0)
    , mTableDeleted(false)
{
}

NftablesMonitor::NftablesMonitor(INetlinkSocket &aSocket, std::string aTableName, DeletedHandler aHandler)
    : mSocket(aSocket)
    , mTableName(std::move(aTableName))
    , mHandler(std::move(aHandler))
    , mOwnPortId(0)
    , mTableDeleted(false)
{
}

NftablesMonitor::~NftablesMonitor(void)
{
    Deinit();
}

otbrError NftablesMonitor::Init(uint32_t aOwnPortId)
{
    otbrError error = OTBR_ERROR_NONE;

    VerifyOrExit(!mSocket.IsOpen(), error = OTBR_ERROR_INVALID_STATE);

    SuccessOrExit(error = mSocket.Open());
    SuccessOrExit(error = mSocket.Subscribe(NFNLGRP_NFTABLES));
    mOwnPortId = aOwnPortId;

exit:
    if (error != OTBR_ERROR_NONE)
    {
        mSocket.Close();
    }
    otbrLogResult(error, "NftablesMonitor: %s", __FUNCTION__);
    return error;
}

void NftablesMonitor::Deinit(void)
{
    mSocket.Close();
}

void NftablesMonitor::Update(MainloopContext &aMainloop)
{
    int fd = mSocket.GetFd();

    if (fd >= 0)
    {
        aMainloop.AddFdToReadSet(fd);
    }
}

void NftablesMonitor::Process(const MainloopContext &aMainloop)
{
    int fd = mSocket.GetFd();

    if (fd >= 0 && FD_ISSET(fd, &aMainloop.mReadFdSet))
    {
        ProcessEvents();
    }
}

void NftablesMonitor::ProcessEvents(void)
{
    alignas(struct nlmsghdr) char buf[MNL_SOCKET_BUFFER_SIZE];
    bool                          eventsLost = false;

    mTableDeleted = false;

    while (true)
    {
        ssize_t len = mSocket.RecvNoWait(buf, sizeof(buf));

        if (len < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            // The kernel dropped events the socket was too slow to take.
            // Whether the table's deletion was among them cannot be known, so
            // it is taken to have been: a reinstall over an intact table is
            // one atomic transaction and changes nothing. Kept apart from
            // mTableDeleted: the error is reported ahead of the events still
            // queued, which are older than the lost ones, so a recreation of
            // our own among them must not cancel it.
            if (errno == ENOBUFS)
            {
                otbrLogWarning("NftablesMonitor: events were lost, assuming table %s is gone", mTableName.c_str());
                eventsLost = true;
                continue;
            }
            break;
        }
        if (len == 0)
        {
            break;
        }

        // Events carry the requester's sequence number and port id, not ours;
        // zero disables both checks.
        mnl_cb_run(buf, static_cast<size_t>(len), 0, 0, &NftablesMonitor::HandleEvent, this);
    }

    if (mTableDeleted || eventsLost)
    {
        mHandler();
    }
}

int NftablesMonitor::HandleEvent(const struct nlmsghdr *aNlh, void *aContext)
{
    NftablesMonitor    *monitor = static_cast<NftablesMonitor *>(aContext);
    struct nftnl_table *table   = nullptr;
    uint16_t            type    = NFNL_MSG_TYPE(aNlh->nlmsg_type);
    bool                ours    = (aNlh->nlmsg_pid == monitor->mOwnPortId);
    const char         *name;

    VerifyOrExit(NFNL_SUBSYS_ID(aNlh->nlmsg_type) == NFNL_SUBSYS_NFTABLES);
    VerifyOrExit(type == NFT_MSG_DELTABLE || type == NFT_MSG_NEWTABLE);

    table = nftnl_table_alloc();
    VerifyOrExit(table != nullptr);
    VerifyOrExit(nftnl_table_nlmsg_parse(aNlh, table) >= 0);
    VerifyOrExit(nftnl_table_get_u32(table, NFTNL_TABLE_FAMILY) == NFPROTO_INET);
    name = nftnl_table_get_str(table, NFTNL_TABLE_NAME);
    VerifyOrExit(name != nullptr && monitor->mTableName == name);

    if (type == NFT_MSG_DELTABLE && !ours)
    {
        // Init(), Deinit() and a reinstall delete the table too; those carry
        // the agent's own port id.
        if (!monitor->mTableDeleted)
        {
            otbrLogWarning("NftablesMonitor: table %s was deleted outside otbr-agent", monitor->mTableName.c_str());
        }
        monitor->mTableDeleted = true;
    }
    else if (type == NFT_MSG_NEWTABLE && ours)
    {
        // The agent put the table back before these events were read: an
        // update ran into ENOENT and reinstalled. Nothing left to do.
        monitor->mTableDeleted = false;
    }

exit:
    if (table != nullptr)
    {
        nftnl_table_free(table);
    }
    return MNL_CB_OK;
}

} // namespace Firewall
} // namespace otbr
