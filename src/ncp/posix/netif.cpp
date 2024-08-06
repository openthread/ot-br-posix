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
#include <utility>

#include "common/code_utils.hpp"
#include "common/logging.hpp"

namespace otbr {
namespace Posix {

Netif::Netif(const char *aInterfaceName)
    : mInterfaceName(aInterfaceName)
    , mTunFd(-1)
    , mIpFd(-1)
    , mNetlinkFd(-1)
    , mNetlinkSequence(0)
{
    memset(mNetifName, 0, sizeof(mNetifName));
}

void Netif::Init(void)
{
    mIpFd = SocketWithCloseExec(AF_INET6, SOCK_DGRAM, IPPROTO_IP, kSocketNonBlock);
    VerifyOrDie(mIpFd >= 0, strerror(errno));

    PlatConfigureNetLink();
    PlatConfigureTunDevice();

    mNetifIndex = if_nametoindex(mNetifName);
    VerifyOrDie(mNetifIndex > 0, OTBR_ERROR_INVALID_STATE);

    PlatInit();
}

void Netif::Deinit(void)
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

void Netif::Process(const MainloopContext *aContext)
{
    VerifyOrExit(mNetifIndex > 0);

    if (FD_ISSET(mTunFd, &aContext->mErrorFdSet))
    {
        close(mTunFd);
        DieNow("Error on Tun Fd!");
    }

    if (FD_ISSET(mNetlinkFd, &aContext->mErrorFdSet))
    {
        close(mNetlinkFd);
        DieNow("Error on Netlink Fd!");
    }

    if (FD_ISSET(mNetlinkFd, &aContext->mReadFdSet))
    {
        PlatProcessNetlinkEvent();
    }

exit:
    return;
}

void Netif::UpdateFdSet(MainloopContext *aContext)
{
    VerifyOrExit(mNetifIndex > 0);

    assert(aContext != nullptr);
    assert(mTunFd >= 0);
    assert(mNetlinkFd >= 0);
    assert(mIpFd >= 0);

    FD_SET(mTunFd, &aContext->mReadFdSet);
    FD_SET(mTunFd, &aContext->mErrorFdSet);
    FD_SET(mNetlinkFd, &aContext->mReadFdSet);
    FD_SET(mNetlinkFd, &aContext->mErrorFdSet);

    if (mTunFd > aContext->mMaxFd)
    {
        aContext->mMaxFd = mTunFd;
    }

    if (mNetlinkFd > aContext->mMaxFd)
    {
        aContext->mMaxFd = mNetlinkFd;
    }

exit:
    return;
}

void Netif::UpdateIp6Addresses(const std::vector<otIp6AddressInfo> &aAddresses)
{
    mIp6AddressesUpdate.clear();
    for (const otIp6AddressInfo &address : aAddresses)
    {
        mIp6AddressesUpdate.emplace_back(address);
    }

    // Remove stale addresses
    for (const Ip6AddressInfo &address : mIp6Addresses)
    {
        if (std::find(mIp6AddressesUpdate.begin(), mIp6AddressesUpdate.end(), address) == mIp6AddressesUpdate.end())
        {
            otbrLogInfo("Remove address: %s", Ip6Address(address.mAddress).ToString().c_str());
            ProcessAddressChange(address, false);
        }
    }

    // Add new addresses
    for (const Ip6AddressInfo &address : mIp6AddressesUpdate)
    {
        if (std::find(mIp6Addresses.begin(), mIp6Addresses.end(), address) == mIp6Addresses.end())
        {
            otbrLogInfo("Add address: %s", Ip6Address(address.mAddress).ToString().c_str());
            ProcessAddressChange(address, true);
        }
    }

    std::swap(mIp6Addresses, mIp6AddressesUpdate);
}

void Netif::UpdateIp6MulticastAddresses(const std::vector<otIp6Address> &aAddresses)
{
    // Remove stale addresses
    for (const otIp6Address &address : mIp6MulticastAddresses)
    {
        auto condition = [&address](const otIp6Address &aAddr) {
            return memcmp(&address, &aAddr, sizeof(otIp6Address)) == 0;
        };

        if (std::find_if(aAddresses.begin(), aAddresses.end(), condition) == aAddresses.end())
        {
            otbrLogInfo("Remove address: %s", Ip6Address(address).ToString().c_str());
            UpdateMulticast(address, false);
        }
    }

    // Add new addresses
    for (const otIp6Address &address : aAddresses)
    {
        auto condition = [&address](const otIp6Address &aAddr) {
            return memcmp(&address, &aAddr, sizeof(otIp6Address)) == 0;
        };

        if (std::find_if(mIp6MulticastAddresses.begin(), mIp6MulticastAddresses.end(), condition) ==
            mIp6MulticastAddresses.end())
        {
            otbrLogInfo("Remove address: %s", Ip6Address(address).ToString().c_str());
            UpdateMulticast(address, true);
        }
    }

    mIp6MulticastAddresses.assign(aAddresses.begin(), aAddresses.end());
}

int Netif::SocketWithCloseExec(int aDomain, int aType, int aProtocol, SocketBlockOption aBlockOption)
{
    int rval = 0;
    int fd   = -1;

#ifdef __APPLE__
    VerifyOrExit((fd = socket(aDomain, aType, aProtocol)) != -1, perror("socket(SOCK_CLOEXEC)"));

    VerifyOrExit((rval = fcntl(fd, F_GETFD, 0)) != -1, perror("fcntl(F_GETFD)"));
    rval |= aBlockOption == kSocketNonBlock ? O_NONBLOCK | FD_CLOEXEC : FD_CLOEXEC;
    VerifyOrExit((rval = fcntl(fd, F_SETFD, rval)) != -1, perror("fcntl(F_SETFD)"));
#else
    aType |= aBlockOption == kSocketNonBlock ? SOCK_CLOEXEC | SOCK_NONBLOCK : SOCK_CLOEXEC;
    VerifyOrExit((fd = socket(aDomain, aType, aProtocol)) != -1, perror("socket(SOCK_CLOEXEC)"));
#endif

exit:
    if (rval == -1)
    {
        VerifyOrDie(close(fd) == 0, strerror(errno));
        fd = -1;
    }

    return fd;
}

void Netif::UpdateMulticast(const otIp6Address &aAddress, bool aIsAdded)
{
    struct ipv6_mreq mreq;
    otbrError        error = OTBR_ERROR_NONE;
    int              err;

    VerifyOrExit(mIpFd >= 0);
    memcpy(&mreq.ipv6mr_multiaddr, &aAddress, sizeof(mreq.ipv6mr_multiaddr));
    mreq.ipv6mr_interface = mNetifIndex;

    err = setsockopt(mIpFd, IPPROTO_IPV6, (aIsAdded ? IPV6_JOIN_GROUP : IPV6_LEAVE_GROUP), &mreq, sizeof(mreq));

    if (err != 0)
    {
        otbrLogWarning("%s failure (%d)", aIsAdded ? "IPV6_JOIN_GROUP" : "IPV6_LEAVE_GROUP", errno);
        error = OTBR_ERROR_ERRNO;
        ExitNow();
    }

    otbrLogInfo("%s multicast address %s", aIsAdded ? "Added" : "Removed", Ip6Address(aAddress).ToString().c_str());

exit:
    SuccessOrDie(error, strerror(errno));
}

void Netif::ProcessAddressChange(const Ip6AddressInfo &aAddressInfo, bool aIsAdded)
{
    if (aAddressInfo.mAddress.mFields.m8[0] == 0xff)
    {
        UpdateMulticast(aAddressInfo.mAddress, aIsAdded);
    }
    else
    {
        PlatUpdateUnicast(aAddressInfo, aIsAdded);
    }
}

bool Netif::CompareIp6Address(const otIp6Address &aObj1, const otIp6Address &aObj2)
{
    return memcmp(&aObj1, &aObj2, sizeof(otIp6Address)) == 0;
}

} // namespace Posix
} // namespace otbr
