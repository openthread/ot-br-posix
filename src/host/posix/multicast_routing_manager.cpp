/*
 *  Copyright (c) 2025, The OpenThread Authors.
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

#include "multicast_routing_manager.hpp"

#include <assert.h>
#include <net/if.h>
#include <netinet/icmp6.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#ifdef __linux__
#include <linux/mroute6.h>
#endif

#include <openthread/ip6.h>

#include "common/code_utils.hpp"
#include "common/logging.hpp"
#include "common/types.hpp"
#include "utils/socket_utils.hpp"

#ifdef __linux__
#if OTBR_ENABLE_BACKBONE_ROUTER

namespace otbr {

MulticastRoutingManager::MulticastRoutingManager(const Netif                   &aNetif,
                                                 const InfraIf                 &aInfraIf,
                                                 const Host::NetworkProperties &aNetworkProperties)
    : mNetif(aNetif)
    , mInfraIf(aInfraIf)
    , mNetworkProperties(aNetworkProperties)
    , mLastExpireTime(otbr::Timepoint::min())
    , mMulticastRouterSock(-1)
{
}

void MulticastRoutingManager::HandleStateChange(otBackboneRouterState aState)
{
    otbrLogInfo("State Change:%u", aState);

    switch (aState)
    {
    case OT_BACKBONE_ROUTER_STATE_DISABLED:
    case OT_BACKBONE_ROUTER_STATE_SECONDARY:
        Disable();
        break;
    case OT_BACKBONE_ROUTER_STATE_PRIMARY:
        Enable();
        break;
    }
}

void MulticastRoutingManager::HandleBackboneMulticastListenerEvent(otBackboneRouterMulticastListenerEvent aEvent,
                                                                   const Ip6Address                      &aAddress)
{
    switch (aEvent)
    {
    case OT_BACKBONE_ROUTER_MULTICAST_LISTENER_ADDED:
        mMulticastListeners.insert(aAddress);
        Add(aAddress);
        break;
    case OT_BACKBONE_ROUTER_MULTICAST_LISTENER_REMOVED:
        mMulticastListeners.erase(aAddress);
        Remove(aAddress);
        break;
    }
}

void MulticastRoutingManager::Update(MainloopContext &aContext)
{
    VerifyOrExit(IsEnabled());

    aContext.AddFdToReadSet(mMulticastRouterSock);

exit:
    return;
}

void MulticastRoutingManager::Process(const MainloopContext &aContext)
{
    VerifyOrExit(IsEnabled());

    ExpireMulticastForwardingCache();

    if (FD_ISSET(mMulticastRouterSock, &aContext.mReadFdSet))
    {
        ProcessMulticastRouterMessages();
    }

exit:
    return;
}

void MulticastRoutingManager::Enable(void)
{
    VerifyOrExit(!IsEnabled());
    VerifyOrExit(mInfraIf.GetIfIndex() != 0); // Only enable the MulticastRoutingManager when the Infra If has been set.

    InitMulticastRouterSock();

    otbrLogResult(OTBR_ERROR_NONE, "%s", __FUNCTION__);

exit:
    return;
}

void MulticastRoutingManager::Disable(void)
{
    FinalizeMulticastRouterSock();

    otbrLogResult(OTBR_ERROR_NONE, "%s", __FUNCTION__);
}

void MulticastRoutingManager::Add(const Ip6Address &aAddress)
{
    VerifyOrExit(IsEnabled());

    UnblockInboundMulticastForwardingCache(aAddress);
    UpdateMldReport(aAddress, true);

    otbrLogResult(OTBR_ERROR_NONE, "%s: %s", __FUNCTION__, aAddress.ToString().c_str());

exit:
    return;
}

void MulticastRoutingManager::Remove(const Ip6Address &aAddress)
{
    VerifyOrExit(IsEnabled());

    RemoveInboundMulticastForwardingCache(aAddress);
    UpdateMldReport(aAddress, false);

    otbrLogResult(OTBR_ERROR_NONE, "%s: %s", __FUNCTION__, aAddress.ToString().c_str());

exit:
    return;
}

void MulticastRoutingManager::UpdateMldReport(const Ip6Address &aAddress, bool isAdd)
{
    struct ipv6_mreq ipv6mr;
    otbrError        error = OTBR_ERROR_NONE;

    ipv6mr.ipv6mr_interface = mInfraIf.GetIfIndex();
    aAddress.CopyTo(ipv6mr.ipv6mr_multiaddr);
    if (setsockopt(mMulticastRouterSock, IPPROTO_IPV6, (isAdd ? IPV6_JOIN_GROUP : IPV6_LEAVE_GROUP), (void *)&ipv6mr,
                   sizeof(ipv6mr)) != 0)
    {
        error = OTBR_ERROR_ERRNO;
    }

    otbrLogResult(error, "%s: address %s %s", __FUNCTION__, aAddress.ToString().c_str(), (isAdd ? "Added" : "Removed"));
}

bool MulticastRoutingManager::HasMulticastListener(const Ip6Address &aAddress) const
{
    return mMulticastListeners.find(aAddress) != mMulticastListeners.end();
}

void MulticastRoutingManager::InitMulticastRouterSock(void)
{
    int                 one = 1;
    struct icmp6_filter filter;
    struct mif6ctl      mif6ctl;

    // Create a Multicast Routing socket
    mMulticastRouterSock = SocketWithCloseExec(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6, kSocketBlock);
    VerifyOrDie(mMulticastRouterSock != -1, "Failed to create socket");

    // Enable Multicast Forwarding in Kernel
    VerifyOrDie(0 == setsockopt(mMulticastRouterSock, IPPROTO_IPV6, MRT6_INIT, &one, sizeof(one)),
                "Failed to enable multicast forwarding");

    // Filter all ICMPv6 messages
    ICMP6_FILTER_SETBLOCKALL(&filter);
    VerifyOrDie(0 == setsockopt(mMulticastRouterSock, IPPROTO_ICMPV6, ICMP6_FILTER, (void *)&filter, sizeof(filter)),
                "Failed to set filter");

    memset(&mif6ctl, 0, sizeof(mif6ctl));
    mif6ctl.mif6c_flags     = 0;
    mif6ctl.vifc_threshold  = 1;
    mif6ctl.vifc_rate_limit = 0;

    // Add Thread network interface to MIF
    mif6ctl.mif6c_mifi = kMifIndexThread;
    mif6ctl.mif6c_pifi = mNetif.GetIfIndex();
    VerifyOrDie(mif6ctl.mif6c_pifi > 0, "Thread interface index is invalid");
    VerifyOrDie(0 == setsockopt(mMulticastRouterSock, IPPROTO_IPV6, MRT6_ADD_MIF, &mif6ctl, sizeof(mif6ctl)),
                "Failed to add Thread network interface to MIF");

    // Add Backbone network interface to MIF
    mif6ctl.mif6c_mifi = kMifIndexBackbone;
    mif6ctl.mif6c_pifi = mInfraIf.GetIfIndex();
    VerifyOrDie(mif6ctl.mif6c_pifi > 0, "Backbone interface index is invalid");
    VerifyOrDie(0 == setsockopt(mMulticastRouterSock, IPPROTO_IPV6, MRT6_ADD_MIF, &mif6ctl, sizeof(mif6ctl)),
                "Failed to add Backbone interface to MIF");
}

void MulticastRoutingManager::FinalizeMulticastRouterSock(void)
{
    VerifyOrExit(IsEnabled());

    close(mMulticastRouterSock);
    mMulticastRouterSock = -1;

exit:
    return;
}

void MulticastRoutingManager::ProcessMulticastRouterMessages(void)
{
    otbrError       error = OTBR_ERROR_NONE;
    char            buf[sizeof(struct mrt6msg)];
    int             nr;
    struct mrt6msg *mrt6msg;
    Ip6Address      src, dst;

    nr = read(mMulticastRouterSock, buf, sizeof(buf));

    VerifyOrExit(nr >= static_cast<int>(sizeof(struct mrt6msg)), error = OTBR_ERROR_ERRNO);

    mrt6msg = reinterpret_cast<struct mrt6msg *>(buf);

    VerifyOrExit(mrt6msg->im6_mbz == 0);
    VerifyOrExit(mrt6msg->im6_msgtype == MRT6MSG_NOCACHE);

    src.CopyFrom(mrt6msg->im6_src);
    dst.CopyFrom(mrt6msg->im6_dst);

    error = AddMulticastForwardingCache(src, dst, static_cast<MifIndex>(mrt6msg->im6_mif));

exit:
    otbrLogResult(error, "%s", __FUNCTION__);
}

otbrError MulticastRoutingManager::AddMulticastForwardingCache(const Ip6Address &aSrcAddr,
                                                               const Ip6Address &aGroupAddr,
                                                               MifIndex          aIif)
{
    otbrError      error = OTBR_ERROR_NONE;
    struct mf6cctl mf6cctl;
    MifIndex       forwardMif = kMifIndexNone;

    VerifyOrExit(aIif == kMifIndexThread || aIif == kMifIndexBackbone, error = OTBR_ERROR_INVALID_ARGS);

    ExpireMulticastForwardingCache();

    if (aIif == kMifIndexBackbone)
    {
        // Forward multicast traffic from Backbone to Thread if the group address is subscribed by any Thread device via
        // MLR.
        if (HasMulticastListener(aGroupAddr))
        {
            forwardMif = kMifIndexThread;
        }
    }
    else
    {
        VerifyOrExit(!aSrcAddr.IsLinkLocal(), error = OTBR_ERROR_NONE);
        VerifyOrExit(!MatchesMeshLocalPrefix(aSrcAddr, *mNetworkProperties.GetMeshLocalPrefix()),
                     error = OTBR_ERROR_NONE);

        // Forward multicast traffic from Thread to Backbone if multicast scope > kRealmLocalScope
        // TODO: (MLR) allow scope configuration of outbound multicast routing
        if (aGroupAddr.GetScope() > Ip6Address::kRealmLocalScope)
        {
            forwardMif = kMifIndexBackbone;
        }
    }

    memset(&mf6cctl, 0, sizeof(mf6cctl));

    aSrcAddr.CopyTo(mf6cctl.mf6cc_origin.sin6_addr);
    aGroupAddr.CopyTo(mf6cctl.mf6cc_mcastgrp.sin6_addr);
    mf6cctl.mf6cc_parent = aIif;

    if (forwardMif != kMifIndexNone)
    {
        IF_SET(forwardMif, &mf6cctl.mf6cc_ifset);
    }

    // Note that kernel reports repetitive `MRT6MSG_NOCACHE` upcalls with a rate limit (e.g. once per 10s for Linux).
    // Because of it, we need to add a "blocking" MFC even if there is no forwarding for this group address.
    // When a  Multicast Listener is later added, the "blocking" MFC will be altered to be a "forwarding" MFC so that
    // corresponding multicast traffic can be forwarded instantly.
    VerifyOrExit(0 == setsockopt(mMulticastRouterSock, IPPROTO_IPV6, MRT6_ADD_MFC, &mf6cctl, sizeof(mf6cctl)),
                 error = OTBR_ERROR_ERRNO);

    SaveMulticastForwardingCache(aSrcAddr, aGroupAddr, aIif, forwardMif);
exit:
    otbrLogResult(error, "%s: add dynamic route: %s %s => %s %s", __FUNCTION__, MifIndexToString(aIif),
                  aSrcAddr.ToString().c_str(), aGroupAddr.ToString().c_str(), MifIndexToString(forwardMif));

    return error;
}

void MulticastRoutingManager::UnblockInboundMulticastForwardingCache(const Ip6Address &aGroupAddr)
{
    struct mf6cctl mf6cctl;

    memset(&mf6cctl, 0, sizeof(mf6cctl));
    aGroupAddr.CopyTo(mf6cctl.mf6cc_mcastgrp.sin6_addr);
    mf6cctl.mf6cc_parent = kMifIndexBackbone;
    IF_SET(kMifIndexThread, &mf6cctl.mf6cc_ifset);

    for (MulticastForwardingCache &mfc : mMulticastForwardingCacheTable)
    {
        otbrError error;

        if (!mfc.IsValid() || mfc.mIif != kMifIndexBackbone || mfc.mOif == kMifIndexThread ||
            mfc.mGroupAddr != aGroupAddr)
        {
            continue;
        }

        // Unblock this inbound route
        mfc.mSrcAddr.CopyTo(mf6cctl.mf6cc_origin.sin6_addr);

        error = (0 == setsockopt(mMulticastRouterSock, IPPROTO_IPV6, MRT6_ADD_MFC, &mf6cctl, sizeof(mf6cctl)))
                    ? OTBR_ERROR_NONE
                    : OTBR_ERROR_ERRNO;

        mfc.Set(kMifIndexBackbone, kMifIndexThread);

        otbrLogResult(error, "%s: %s %s => %s %s", __FUNCTION__, MifIndexToString(mfc.mIif),
                      mfc.mSrcAddr.ToString().c_str(), mfc.mGroupAddr.ToString().c_str(),
                      MifIndexToString(kMifIndexThread));
    }
}

void MulticastRoutingManager::RemoveInboundMulticastForwardingCache(const Ip6Address &aGroupAddr)
{
    for (MulticastForwardingCache &mfc : mMulticastForwardingCacheTable)
    {
        if (mfc.IsValid() && mfc.mIif == kMifIndexBackbone && mfc.mGroupAddr == aGroupAddr)
        {
            RemoveMulticastForwardingCache(mfc);
        }
    }
}

void MulticastRoutingManager::ExpireMulticastForwardingCache(void)
{
    struct sioc_sg_req6 sioc_sg_req6;
    Timepoint           now = Clock::now();
    struct mf6cctl      mf6cctl;

    VerifyOrExit(now >= mLastExpireTime + Microseconds(kMulticastForwardingCacheExpiringInterval * kUsPerSecond));

    mLastExpireTime = now;

    memset(&mf6cctl, 0, sizeof(mf6cctl));
    memset(&sioc_sg_req6, 0, sizeof(sioc_sg_req6));

    for (MulticastForwardingCache &mfc : mMulticastForwardingCacheTable)
    {
        if (mfc.IsValid() &&
            mfc.mLastUseTime + Microseconds(kMulticastForwardingCacheExpireTimeout * kUsPerSecond) < now)
        {
            if (!UpdateMulticastRouteInfo(mfc))
            {
                // The multicast route is expired
                RemoveMulticastForwardingCache(mfc);
            }
        }
    }

    DumpMulticastForwardingCache();

exit:
    return;
}

bool MulticastRoutingManager::UpdateMulticastRouteInfo(MulticastForwardingCache &aMfc) const
{
    bool                updated = false;
    struct sioc_sg_req6 sioc_sg_req6;

    memset(&sioc_sg_req6, 0, sizeof(sioc_sg_req6));

    aMfc.mSrcAddr.CopyTo(sioc_sg_req6.src.sin6_addr);
    aMfc.mGroupAddr.CopyTo(sioc_sg_req6.grp.sin6_addr);

    if (ioctl(mMulticastRouterSock, SIOCGETSGCNT_IN6, &sioc_sg_req6) != -1)
    {
        unsigned long validPktCnt;

        otbrLogDebug("%s: SIOCGETSGCNT_IN6 %s => %s: bytecnt=%lu, pktcnt=%lu, wrong_if=%lu", __FUNCTION__,
                     aMfc.mSrcAddr.ToString().c_str(), aMfc.mGroupAddr.ToString().c_str(), sioc_sg_req6.bytecnt,
                     sioc_sg_req6.pktcnt, sioc_sg_req6.wrong_if);

        validPktCnt = sioc_sg_req6.pktcnt - sioc_sg_req6.wrong_if;
        if (validPktCnt != aMfc.mValidPktCnt)
        {
            aMfc.SetValidPktCnt(validPktCnt);

            updated = true;
        }
    }
    else
    {
        otbrLogDebug("%s: SIOCGETSGCNT_IN6 %s => %s failed: %s", __FUNCTION__, aMfc.mSrcAddr.ToString().c_str(),
                     aMfc.mGroupAddr.ToString().c_str(), strerror(errno));
    }

    return updated;
}

const char *MulticastRoutingManager::MifIndexToString(MifIndex aMif)
{
    const char *string = "Unknown";

    switch (aMif)
    {
    case kMifIndexNone:
        string = "None";
        break;
    case kMifIndexThread:
        string = "Thread";
        break;
    case kMifIndexBackbone:
        string = "Backbone";
        break;
    }

    return string;
}

void MulticastRoutingManager::DumpMulticastForwardingCache(void) const
{
    VerifyOrExit(otbrLogGetLevel() == OTBR_LOG_DEBUG);

    otbrLogDebug("==================== MFC ENTRIES ====================");

    for (const MulticastForwardingCache &mfc : mMulticastForwardingCacheTable)
    {
        if (mfc.IsValid())
        {
            otbrLogDebug("%s %s => %s %s", MifIndexToString(mfc.mIif), mfc.mSrcAddr.ToString().c_str(),
                         mfc.mGroupAddr.ToString().c_str(), MifIndexToString(mfc.mOif));
        }
    }

    otbrLogDebug("=====================================================");

exit:
    return;
}

void MulticastRoutingManager::MulticastForwardingCache::Set(MulticastRoutingManager::MifIndex aIif,
                                                            MulticastRoutingManager::MifIndex aOif)
{
    mIif         = aIif;
    mOif         = aOif;
    mValidPktCnt = 0;
    mLastUseTime = Clock::now();
}

void MulticastRoutingManager::MulticastForwardingCache::Set(const Ip6Address &aSrcAddr,
                                                            const Ip6Address &aGroupAddr,
                                                            MifIndex          aIif,
                                                            MifIndex          aOif)
{
    mSrcAddr   = aSrcAddr;
    mGroupAddr = aGroupAddr;
    Set(aIif, aOif);
}

void MulticastRoutingManager::MulticastForwardingCache::SetValidPktCnt(unsigned long aValidPktCnt)
{
    mValidPktCnt = aValidPktCnt;
    mLastUseTime = Clock::now();
}

void MulticastRoutingManager::SaveMulticastForwardingCache(const Ip6Address                 &aSrcAddr,
                                                           const Ip6Address                 &aGroupAddr,
                                                           MulticastRoutingManager::MifIndex aIif,
                                                           MulticastRoutingManager::MifIndex aOif)
{
    MulticastForwardingCache *invalid = nullptr;
    MulticastForwardingCache *oldest  = nullptr;

    for (MulticastForwardingCache &mfc : mMulticastForwardingCacheTable)
    {
        if (mfc.IsValid())
        {
            if (mfc.mSrcAddr == aSrcAddr && mfc.mGroupAddr == aGroupAddr)
            {
                mfc.Set(aIif, aOif);
                ExitNow();
            }

            if (oldest == nullptr || mfc.mLastUseTime < oldest->mLastUseTime)
            {
                oldest = &mfc;
            }
        }
        else if (invalid == nullptr)
        {
            invalid = &mfc;
        }
    }

    if (invalid != nullptr)
    {
        invalid->Set(aSrcAddr, aGroupAddr, aIif, aOif);
    }
    else
    {
        RemoveMulticastForwardingCache(*oldest);
        oldest->Set(aSrcAddr, aGroupAddr, aIif, aOif);
    }

exit:
    return;
}

void MulticastRoutingManager::RemoveMulticastForwardingCache(
    MulticastRoutingManager::MulticastForwardingCache &aMfc) const
{
    otbrError      error;
    struct mf6cctl mf6cctl;

    memset(&mf6cctl, 0, sizeof(mf6cctl));

    aMfc.mSrcAddr.CopyTo(mf6cctl.mf6cc_origin.sin6_addr);
    aMfc.mGroupAddr.CopyTo(mf6cctl.mf6cc_mcastgrp.sin6_addr);

    mf6cctl.mf6cc_parent = aMfc.mIif;

    error = (0 == setsockopt(mMulticastRouterSock, IPPROTO_IPV6, MRT6_DEL_MFC, &mf6cctl, sizeof(mf6cctl)))
                ? OTBR_ERROR_NONE
                : OTBR_ERROR_ERRNO;

    otbrLogResult(error, "%s: %s %s => %s %s", __FUNCTION__, MifIndexToString(aMfc.mIif),
                  aMfc.mSrcAddr.ToString().c_str(), aMfc.mGroupAddr.ToString().c_str(), MifIndexToString(aMfc.mOif));

    aMfc.Erase();
}

bool MulticastRoutingManager::MatchesMeshLocalPrefix(const Ip6Address        &aAddress,
                                                     const otMeshLocalPrefix &aMeshLocalPrefix)
{
    otIp6Address matcher{};
    memcpy(matcher.mFields.m8, aMeshLocalPrefix.m8, sizeof(otMeshLocalPrefix));

    return otIp6PrefixMatch(reinterpret_cast<const otIp6Address *>(aAddress.m8), &matcher) >= OT_IP6_PREFIX_BITSIZE;
}

} // namespace otbr

#endif // OTBR_ENABLE_BACKBONE_ROUTER
#endif // __linux__
