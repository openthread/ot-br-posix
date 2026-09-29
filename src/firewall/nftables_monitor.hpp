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
 *   Watches nf_tables for the deletion of the agent's table.
 */

#ifndef OTBR_FIREWALL_NFTABLES_MONITOR_HPP_
#define OTBR_FIREWALL_NFTABLES_MONITOR_HPP_

#include "openthread-br/config.h"

#include <stdint.h>

#include <functional>
#include <memory>
#include <string>

#include "common/code_utils.hpp"
#include "common/mainloop.hpp"
#include "common/types.hpp"
#include "firewall/netlink_socket.hpp"

struct nlmsghdr;

namespace otbr {
namespace Firewall {

/**
 * Reports the deletion of one nf_tables table.
 *
 * `nft flush ruleset` -- OpenWrt's `fw4 flush` -- deletes every table on the
 * host, the agent's included, after which the agent's firewall updates fail
 * until its table is reinstalled. The kernel announces each deletion to the
 * nftables event group; this listens for the agent's table and hands the
 * event to a handler. Deletions the agent requested itself carry the port id
 * of its own socket and are ignored, so a reinstall does not report itself.
 */
class NftablesMonitor : public MainloopProcessor, private NonCopyable
{
public:
    using DeletedHandler = std::function<void(void)>;

    /**
     * Creates a monitor over its own netlink socket.
     */
    NftablesMonitor(std::string aTableName, DeletedHandler aHandler);

    /**
     * Creates a monitor over the given socket, which must outlive it.
     */
    NftablesMonitor(INetlinkSocket &aSocket, std::string aTableName, DeletedHandler aHandler);

    ~NftablesMonitor(void) override;

    /**
     * Opens the socket and subscribes to nftables events.
     *
     * @param[in] aOwnPortId  Port id of the socket the agent changes tables
     *                        through; events it caused are not reported.
     */
    otbrError Init(uint32_t aOwnPortId);

    void Deinit(void);

    void Update(MainloopContext &aMainloop) override;
    void Process(const MainloopContext &aMainloop) override;

    /**
     * Reads every event queued on the socket and calls the handler once if
     * the table was deleted. Process() calls this when the socket is
     * readable; tests call it directly.
     */
    void ProcessEvents(void);

private:
    static int HandleEvent(const struct nlmsghdr *aNlh, void *aContext);

    std::unique_ptr<INetlinkSocket> mOwnedSocket;
    INetlinkSocket                 &mSocket;
    std::string                     mTableName;
    DeletedHandler                  mHandler;
    uint32_t                        mOwnPortId;
    bool                            mTableDeleted;
};

} // namespace Firewall
} // namespace otbr

#endif // OTBR_FIREWALL_NFTABLES_MONITOR_HPP_
