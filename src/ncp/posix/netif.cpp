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

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
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

otbrError Netif::Dependencies::Ip6Send(const uint8_t *aData, uint16_t aLength)
{
    OTBR_UNUSED_VARIABLE(aData);
    OTBR_UNUSED_VARIABLE(aLength);

    return OTBR_ERROR_NONE;
}

otbrError Netif::Dependencies::Ip6MulAddrUpdateSubscription(const otIp6Address &aAddress, bool aIsAdd)
{
    OTBR_UNUSED_VARIABLE(aAddress);
    OTBR_UNUSED_VARIABLE(aIsAdd);

    return OTBR_ERROR_NONE;
}

OT_TOOL_PACKED_BEGIN
struct Mldv2Header
{
    uint8_t  mType;
    uint8_t  _rsv0;
    uint16_t mChecksum;
    uint16_t _rsv1;
    uint16_t mNumRecords;
} OT_TOOL_PACKED_END;

OT_TOOL_PACKED_BEGIN
struct Mldv2Record
{
    uint8_t         mRecordType;
    uint8_t         mAuxDataLen;
    uint16_t        mNumSources;
    struct in6_addr mMulticastAddress;
} OT_TOOL_PACKED_END;

enum
{
    kIcmpv6Mldv2Type                      = 143,
    kIcmpv6Mldv2ModeIsIncludeType         = 1,
    kIcmpv6Mldv2ModeIsExcludeType         = 2,
    kIcmpv6Mldv2RecordChangeToIncludeType = 3,
    kIcmpv6Mldv2RecordChangeToExcludeType = 4,
};

Netif::Netif(Dependencies &aDependencies)
    : mTunFd(-1)
    , mIpFd(-1)
    , mNetlinkFd(-1)
    , mMldFd(-1)
    , mNetlinkSequence(0)
    , mNetifIndex(0)
    , mDeps(aDependencies)
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

    SuccessOrExit(error = InitMldListener());

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

    if (FD_ISSET(mMldFd, &aContext->mErrorFdSet))
    {
        close(mMldFd);
        DieNow("Error on MLD Fd!");
    }

    if (FD_ISSET(mTunFd, &aContext->mReadFdSet))
    {
        ProcessIp6Send();
    }

    if (FD_ISSET(mMldFd, &aContext->mReadFdSet))
    {
        ProcessMldEvent();
    }
}

void Netif::UpdateFdSet(MainloopContext *aContext)
{
    assert(aContext != nullptr);
    assert(mTunFd >= 0);
    assert(mIpFd >= 0);
    assert(mMldFd >= 0);

    aContext->AddFdToSet(mTunFd, MainloopContext::kErrorFdSet | MainloopContext::kReadFdSet);
    aContext->AddFdToSet(mMldFd, MainloopContext::kErrorFdSet | MainloopContext::kReadFdSet);
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

    error = mDeps.Ip6Send(packet, rval);
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

    if (mMldFd != -1)
    {
        close(mMldFd);
        mMldFd = -1;
    }

    mNetifIndex = 0;
    mIp6UnicastAddresses.clear();
    mIp6MulticastAddresses.clear();
}

static const otIp6Address kMldv2MulticastAddress = {
    {{0xff, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x16}}};
static const otIp6Address kAllRouterLocalMulticastAddress = {
    {{0xff, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02}}};

static bool IsMulAddrFiltered(const otIp6Address &aAddr)
{
    return Ip6Address(aAddr) == Ip6Address(kMldv2MulticastAddress) ||
           Ip6Address(aAddr) == Ip6Address(kAllRouterLocalMulticastAddress);
}

otbrError Netif::InitMldListener(void)
{
    otbrError        error = OTBR_ERROR_NONE;
    struct ipv6_mreq mreq6;

    mMldFd = SocketWithCloseExec(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6, kSocketNonBlock);
    VerifyOrExit(mMldFd != -1, error = OTBR_ERROR_ERRNO);

    mreq6.ipv6mr_interface = mNetifIndex;
    memcpy(&mreq6.ipv6mr_multiaddr, kMldv2MulticastAddress.mFields.m8, sizeof(kMldv2MulticastAddress.mFields.m8));

    VerifyOrExit(setsockopt(mMldFd, IPPROTO_IPV6, IPV6_JOIN_GROUP, &mreq6, sizeof(mreq6)) == 0,
                 error = OTBR_ERROR_ERRNO);
#ifdef __linux__
    VerifyOrExit(setsockopt(mMldFd, SOL_SOCKET, SO_BINDTODEVICE, mNetifName.c_str(),
                            static_cast<socklen_t>(mNetifName.length())) == 0,
                 error = OTBR_ERROR_ERRNO);
#endif

exit:
    return error;
}

void Netif::ProcessMldEvent(void)
{
    const size_t        kMaxMldEvent = 8192;
    uint8_t             buffer[kMaxMldEvent];
    ssize_t             bufferLen = -1;
    struct sockaddr_in6 srcAddr;
    socklen_t           addrLen  = sizeof(srcAddr);
    bool                fromSelf = false;
    Mldv2Header        *hdr      = reinterpret_cast<Mldv2Header *>(buffer);
    size_t              offset;
    uint8_t             type;
    struct ifaddrs     *ifAddrs = nullptr;
    char                addressString[INET6_ADDRSTRLEN + 1];

    bufferLen = recvfrom(mMldFd, buffer, sizeof(buffer), 0, reinterpret_cast<sockaddr *>(&srcAddr), &addrLen);
    VerifyOrExit(bufferLen > 0);

    type = buffer[0];
    VerifyOrExit(type == kIcmpv6Mldv2Type && bufferLen >= static_cast<ssize_t>(sizeof(Mldv2Header)));

    // Check whether it is sent by self
    VerifyOrExit(getifaddrs(&ifAddrs) == 0);
    for (struct ifaddrs *ifAddr = ifAddrs; ifAddr != nullptr; ifAddr = ifAddr->ifa_next)
    {
        if (ifAddr->ifa_addr != nullptr && ifAddr->ifa_addr->sa_family == AF_INET6 &&
            strncmp(mNetifName.c_str(), ifAddr->ifa_name, IFNAMSIZ) == 0)
        {
            struct sockaddr_in6 *addr6 = reinterpret_cast<struct sockaddr_in6 *>(ifAddr->ifa_addr);

            if (memcmp(&addr6->sin6_addr, &srcAddr.sin6_addr, sizeof(in6_addr)) == 0)
            {
                fromSelf = true;
                break;
            }
        }
    }
    VerifyOrExit(fromSelf);

    hdr    = reinterpret_cast<Mldv2Header *>(buffer);
    offset = sizeof(Mldv2Header);

    for (size_t i = 0; i < ntohs(hdr->mNumRecords) && offset < static_cast<size_t>(bufferLen); i++)
    {
        if (static_cast<size_t>(bufferLen) >= (sizeof(Mldv2Record) + offset))
        {
            Mldv2Record *record = reinterpret_cast<Mldv2Record *>(&buffer[offset]);

            otbrError    error = OTBR_ERROR_DROPPED;
            otIp6Address address;

            memcpy(&address, &record->mMulticastAddress, sizeof(address));
            if (IsMulAddrFiltered(address))
            {
                continue;
            }

            inet_ntop(AF_INET6, &record->mMulticastAddress, addressString, sizeof(addressString));

            switch (record->mRecordType)
            {
            case kIcmpv6Mldv2ModeIsIncludeType:
            case kIcmpv6Mldv2ModeIsExcludeType:
                error = OTBR_ERROR_NONE;
                break;
            ///< Only update subscription on NCP when the target multicast address is not in `mIp6MulticastAddresses`.
            ///< This indicates that this is the first time the multicast address subscription needs to be updated.
            case kIcmpv6Mldv2RecordChangeToIncludeType:
                if (record->mNumSources == 0)
                {
                    if (std::find(mIp6MulticastAddresses.begin(), mIp6MulticastAddresses.end(), Ip6Address(address)) !=
                        mIp6MulticastAddresses.end())
                    {
                        error = mDeps.Ip6MulAddrUpdateSubscription(address, /* isAdd */ false);
                    }
                    else
                    {
                        error = OTBR_ERROR_NONE;
                    }
                }
                break;
            case kIcmpv6Mldv2RecordChangeToExcludeType:
                if (std::find(mIp6MulticastAddresses.begin(), mIp6MulticastAddresses.end(), Ip6Address(address)) ==
                    mIp6MulticastAddresses.end())
                {
                    error = mDeps.Ip6MulAddrUpdateSubscription(address, /* isAdd */ true);
                }
                else
                {
                    error = OTBR_ERROR_NONE;
                }
                break;
            }

            offset += sizeof(Mldv2Record) + sizeof(in6_addr) * ntohs(record->mNumSources);

            if (error != OTBR_ERROR_NONE)
            {
                otbrLogWarning("Failed to Update multicast subscription: %s", otbrErrorString(error));
            }
        }
    }

exit:
    if (ifAddrs)
    {
        freeifaddrs(ifAddrs);
    }
}

} // namespace otbr
