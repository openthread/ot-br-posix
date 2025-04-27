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

#ifndef BACKBONE_ROUTER_MULTICAST_ROUTING_MANAGER_HPP_
#define BACKBONE_ROUTER_MULTICAST_ROUTING_MANAGER_HPP_

#include "openthread-br/config.h"

#include <set>

#include <openthread/backbone_router_ftd.h>
#include <openthread/dataset.h>

#include "common/code_utils.hpp"
#include "common/mainloop.hpp"
#include "common/time.hpp"
#include "common/types.hpp"
#include "host/posix/infra_if.hpp"
#include "host/posix/netif.hpp"
#include "host/thread_host.hpp"

namespace otbr {

class MulticastRoutingManager : public MainloopProcessor, private NonCopyable
{
public:
    explicit MulticastRoutingManager(const Netif                   &aNetif,
                                     const InfraIf                 &aInfraIf,
                                     const Host::NetworkProperties &aNetworkProperties);

    void Deinit(void) { FinalizeMulticastRouterSock(); }
    bool IsEnabled(void) const { return mMulticastRouterSock >= 0; }
    void HandleStateChange(otBackboneRouterState aState);
    void HandleBackboneMulticastListenerEvent(otBackboneRouterMulticastListenerEvent aEvent,
                                              const Ip6Address                      &aAddress);

private:
    static constexpr uint32_t kUsPerSecond = 1000000; //< Microseconds per second.
    static constexpr uint32_t kMulticastForwardingCacheExpireTimeout =
        300; //< Expire timeout of Multicast Forwarding Cache (in seconds)
    static constexpr uint32_t kMulticastForwardingCacheExpiringInterval =
        60; //< Expire interval of Multicast Forwarding Cache (in seconds)
    static constexpr uint32_t kMulticastMaxListeners = 75; //< The max number of Multicast listeners
    static constexpr uint32_t kMulticastForwardingCacheTableSize =
        kMulticastMaxListeners * 10; //< The max size of MFC table.

    enum MifIndex : uint8_t
    {
        kMifIndexNone     = 0xff,
        kMifIndexThread   = 0,
        kMifIndexBackbone = 1,
    };

    class MulticastForwardingCache
    {
        friend class MulticastRoutingManager;

    private:
        MulticastForwardingCache()
            : mIif(kMifIndexNone)
        {
        }

        bool IsValid() const { return mIif != kMifIndexNone; }
        void Set(MifIndex aIif, MifIndex aOif);
        void Set(const Ip6Address &aSrcAddr, const Ip6Address &aGroupAddr, MifIndex aIif, MifIndex aOif);
        void Erase(void) { mIif = kMifIndexNone; }
        void SetValidPktCnt(unsigned long aValidPktCnt);

        Ip6Address    mSrcAddr;
        Ip6Address    mGroupAddr;
        Timepoint     mLastUseTime;
        unsigned long mValidPktCnt;
        MifIndex      mIif;
        MifIndex      mOif;
    };

    void Update(MainloopContext &aContext) override;
    void Process(const MainloopContext &aContext) override;

    void      Enable(void);
    void      Disable(void);
    void      Add(const Ip6Address &aAddress);
    void      Remove(const Ip6Address &aAddress);
    void      UpdateMldReport(const Ip6Address &aAddress, bool isAdd);
    bool      HasMulticastListener(const Ip6Address &aAddress) const;
    void      InitMulticastRouterSock(void);
    void      FinalizeMulticastRouterSock(void);
    void      ProcessMulticastRouterMessages(void);
    otbrError AddMulticastForwardingCache(const Ip6Address &aSrcAddr, const Ip6Address &aGroupAddr, MifIndex aIif);
    void      SaveMulticastForwardingCache(const Ip6Address &aSrcAddr,
                                           const Ip6Address &aGroupAddr,
                                           MifIndex          aIif,
                                           MifIndex          aOif);
    void      UnblockInboundMulticastForwardingCache(const Ip6Address &aGroupAddr);
    void      RemoveInboundMulticastForwardingCache(const Ip6Address &aGroupAddr);
    void      ExpireMulticastForwardingCache(void);
    bool      UpdateMulticastRouteInfo(MulticastForwardingCache &aMfc) const;
    void      RemoveMulticastForwardingCache(MulticastForwardingCache &aMfc) const;
    static const char *MifIndexToString(MifIndex aMif);
    void               DumpMulticastForwardingCache(void) const;

    static bool MatchesMeshLocalPrefix(const Ip6Address &aAddress, const otMeshLocalPrefix &aMeshLocalPrefix);

    const Netif                   &mNetif;
    const InfraIf                 &mInfraIf;
    const Host::NetworkProperties &mNetworkProperties;
    MulticastForwardingCache       mMulticastForwardingCacheTable[kMulticastForwardingCacheTableSize];
    otbr::Timepoint                mLastExpireTime;
    int                            mMulticastRouterSock;
    std::set<Ip6Address>           mMulticastListeners;
};

} // namespace otbr

#endif // BACKBONE_ROUTER_MULTICAST_ROUTING_MANAGER_HPP_
