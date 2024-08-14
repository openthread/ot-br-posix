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

/**
 * @file
 *   This file includes definitions of the posix Netif of otbr-agent.
 */

#ifndef OTBR_AGENT_POSIX_NETIF_HPP_
#define OTBR_AGENT_POSIX_NETIF_HPP_

#include <net/if.h>

#include <vector>

#include <openthread/ip6.h>

#include "common/types.hpp"

namespace otbr {

class Netif
{
public:
    Netif(void);

    otbrError Init(const std::string &aInterfaceName);
    void      Deinit(void);

    void UpdateIp6UnicastAddresses(const std::vector<Ip6AddressInfo> &aAddrInfos);

private:
    // TODO: Retrieve the Maximum Ip6 size from the coprocessor.
    static constexpr size_t kIp6Mtu = 1280;

    void Clear(void);

    otbrError CreateTunDevice(const std::string &aInterfaceName);
    otbrError InitNetlink(void);

    void PlatformSpecificInit(void);
    void SetAddrGenModeToNone(void);
    void ProcessUnicastAddressChange(const Ip6AddressInfo &aAddressInfo, bool aIsAdded);

    int      mTunFd;           ///< Used to exchange IPv6 packets.
    int      mIpFd;            ///< Used to manage IPv6 stack on the network interface.
    int      mNetlinkFd;       ///< Used to receive netlink events.
    uint32_t mNetlinkSequence; ///< Netlink message sequence.

    unsigned int mNetifIndex;
    std::string  mNetifName;

    std::vector<Ip6AddressInfo> mIp6UnicastAddresses;
};

} // namespace otbr

#endif // OTBR_AGENT_POSIX_NETIF_HPP_
