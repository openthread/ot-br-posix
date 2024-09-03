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

otbrError Netif::Init(const std::string &aInterfaceName, const Ip6SendFunc &aIp6SendFunc)
{
    otbrError error = OTBR_ERROR_NONE;

    VerifyOrExit(aIp6SendFunc, error = OTBR_ERROR_INVALID_ARGS);
    mIp6SendFunc = aIp6SendFunc;

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

void Netif::Process(const MainloopContext *aContext)
{
    if (FD_ISSET(mTunFd, &aContext->mErrorFdSet))
    {
        close(mTunFd);
        DieNow("Error on Tun Fd!");
    }

    if (FD_ISSET(mTunFd, &aContext->mReadFdSet))
    {
        ProcessIp6Send();
    }
}

void Netif::UpdateFdSet(MainloopContext *aContext)
{
    assert(aContext != nullptr);
    assert(mTunFd >= 0);
    assert(mIpFd >= 0);

    aContext->AddFdToSet(mTunFd, MainloopContext::kErrorFdSet | MainloopContext::kReadFdSet);
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

otbrError Netif::UpdateIp6MulticastAddresses(const std::vector<Ip6Address> &aAddrs)
{
    otbrError error = OTBR_ERROR_NONE;

    // Remove stale addresses
    for (const Ip6Address &address : mIp6MulticastAddresses)
    {
        if (std::find(aAddrs.begin(), aAddrs.end(), address) == aAddrs.end())
        {
            otbrLogInfo("Remove address: %s", Ip6Address(address).ToString().c_str());
            SuccessOrExit(error = ProcessMulticastAddressChange(address, /* aIsAdded */ false));
        }
    }

    // Add new addresses
    for (const Ip6Address &address : aAddrs)
    {
        if (std::find(mIp6MulticastAddresses.begin(), mIp6MulticastAddresses.end(), address) ==
            mIp6MulticastAddresses.end())
        {
            otbrLogInfo("Add address: %s", Ip6Address(address).ToString().c_str());
            SuccessOrExit(error = ProcessMulticastAddressChange(address, /* aIsAdded */ true));
        }
    }

    mIp6MulticastAddresses.assign(aAddrs.begin(), aAddrs.end());

exit:
    if (error != OTBR_ERROR_NONE)
    {
        mIp6MulticastAddresses.clear();
    }
    return error;
}

otbrError Netif::ProcessMulticastAddressChange(const Ip6Address &aAddress, bool aIsAdded)
{
    struct ipv6_mreq mreq;
    otbrError        error = OTBR_ERROR_NONE;
    int              err;

    VerifyOrExit(mIpFd >= 0, error = OTBR_ERROR_INVALID_STATE);
    memcpy(&mreq.ipv6mr_multiaddr, &aAddress, sizeof(mreq.ipv6mr_multiaddr));
    mreq.ipv6mr_interface = mNetifIndex;

    err = setsockopt(mIpFd, IPPROTO_IPV6, (aIsAdded ? IPV6_JOIN_GROUP : IPV6_LEAVE_GROUP), &mreq, sizeof(mreq));

    if (err != 0)
    {
        otbrLogWarning("%s failure (%d)", aIsAdded ? "IPV6_JOIN_GROUP" : "IPV6_LEAVE_GROUP", errno);
        ExitNow(error = OTBR_ERROR_ERRNO);
    }

    otbrLogInfo("%s multicast address %s", aIsAdded ? "Added" : "Removed", Ip6Address(aAddress).ToString().c_str());

exit:
    return error;
}

void Netif::SetNetifState(bool aState)
{
    otbrError    error = OTBR_ERROR_NONE;
    struct ifreq ifr;
    bool         ifState = false;

    VerifyOrExit(mIpFd >= 0);
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, mNetifName.c_str(), IFNAMSIZ - 1);
    VerifyOrExit(ioctl(mIpFd, SIOCGIFFLAGS, &ifr) == 0, error = OTBR_ERROR_ERRNO);

    ifState = ((ifr.ifr_flags & IFF_UP) == IFF_UP) ? true : false;

    otbrLogInfo("Changing interface state to %s%s.", aState ? "up" : "down",
                (ifState == aState) ? " (already done, ignoring)" : "");

    if (ifState != aState)
    {
        ifr.ifr_flags = aState ? (ifr.ifr_flags | IFF_UP) : (ifr.ifr_flags & ~IFF_UP);
        VerifyOrExit(ioctl(mIpFd, SIOCSIFFLAGS, &ifr) == 0, error = OTBR_ERROR_ERRNO);
    }

exit:
    if (error != OTBR_ERROR_NONE)
    {
        otbrLogWarning("Failed to update state %s", otbrErrorString(error));
    }
}

void Netif::Ip6Receive(const uint8_t *aBuf, uint16_t aLen)
{
    otbrError error = OTBR_ERROR_NONE;

    VerifyOrExit(aLen <= kIp6Mtu, error = OTBR_ERROR_DROPPED);
    VerifyOrExit(mTunFd > 0, error = OTBR_ERROR_INVALID_STATE);

    otbrLogInfo("Packet from NCP (%u bytes)", aLen);
    VerifyOrExit(write(mTunFd, aBuf, aLen) == aLen, error = OTBR_ERROR_ERRNO);

exit:
    if (error != OTBR_ERROR_NONE)
    {
        otbrLogWarning("Failed to receive, error:%s", otbrErrorString(error));
    }
}

void Netif::ProcessIp6Send(void)
{
    ssize_t   rval;
    uint8_t   packet[kIp6Mtu];
    otbrError error = OTBR_ERROR_NONE;

    rval = read(mTunFd, packet, sizeof(packet));
    VerifyOrExit(rval > 0, error = OTBR_ERROR_ERRNO);

    otbrLogInfo("Send packet (%hu bytes)", static_cast<uint16_t>(rval));

    if (mIp6SendFunc != nullptr)
    {
        error = mIp6SendFunc(packet, rval);
    }
exit:
    if (error == OTBR_ERROR_ERRNO)
    {
        otbrLogInfo("Error reading from Tun Fd: %s", strerror(errno));
    }
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
    mIp6UnicastAddresses.clear();
    mIp6MulticastAddresses.clear();
    mIp6SendFunc = nullptr;
}

} // namespace otbr
