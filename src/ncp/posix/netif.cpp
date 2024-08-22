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

#define OTBR_LOG_TAG "NETIF"

#include "netif.hpp"

#include <errno.h>
#include <fcntl.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>

#include "common/code_utils.hpp"
#include "common/logging.hpp"
#include "common/types.hpp"
#include "utils/socket_utils.hpp"

namespace otbr {

Netif::Netif(void)
    : mTunFd(-1)
    , mIpFd(-1)
    , mNetlinkFd(-1)
    , mNetlinkSequence(0)
    , mNetifIndex(0)
{
}

otbrError Netif::Init(const std::string &aInterfaceName)
{
    otbrError error = OTBR_ERROR_NONE;

    mIpFd = SocketWithCloseExec(AF_INET6, SOCK_DGRAM, IPPROTO_IP, kSocketNonBlock);
    VerifyOrExit(mIpFd >= 0, error = OTBR_ERROR_ERRNO);

    SuccessOrExit(error = CreateTunDevice(aInterfaceName));
    SuccessOrExit(error = InitNetlink());

    mNetifIndex = if_nametoindex(mNetifName.c_str());
    VerifyOrExit(mNetifIndex > 0, error = OTBR_ERROR_INVALID_STATE);

    PlatformSpecificInit();

exit:
    if (error != OTBR_ERROR_NONE)
    {
        Clear();
    }
    return error;
}

void Netif::Deinit(void)
{
    Clear();
}

void Netif::UpdateIp6UnicastAddresses(const std::vector<Ip6AddressInfo> &aAddrInfos)
{
    // Remove stale addresses
    for (const Ip6AddressInfo &addrInfo : mIp6UnicastAddresses)
    {
        if (std::find(aAddrInfos.begin(), aAddrInfos.end(), addrInfo) == aAddrInfos.end())
        {
            otbrLogInfo("Remove address: %s", Ip6Address(addrInfo.mAddress).ToString().c_str());
            // TODO: Verify success of the addition or deletion in Netlink response.
            ProcessUnicastAddressChange(addrInfo, false);
        }
    }

    // Add new addresses
    for (const Ip6AddressInfo &addrInfo : aAddrInfos)
    {
        if (std::find(mIp6UnicastAddresses.begin(), mIp6UnicastAddresses.end(), addrInfo) == mIp6UnicastAddresses.end())
        {
            otbrLogInfo("Add address: %s", Ip6Address(addrInfo.mAddress).ToString().c_str());
            // TODO: Verify success of the addition or deletion in Netlink response.
            ProcessUnicastAddressChange(addrInfo, true);
        }
    }

    mIp6UnicastAddresses.assign(aAddrInfos.begin(), aAddrInfos.end());
}

void Netif::Clear(void)
{
    if (mTunFd != -1)
    {
        close(mTunFd);
        mTunFd = -1;
    }

    if (mIpFd != -1)
    {
        close(mIpFd);
        mIpFd = -1;
    }

    if (mNetlinkFd != -1)
    {
        close(mNetlinkFd);
        mNetlinkFd = -1;
    }

    mNetifIndex = 0;
}

} // namespace otbr
