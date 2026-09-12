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

#include <chrono>
#include <cstring>
#include <functional>
#include <vector>

#if OTBR_ENABLE_DHCP6_PD && OTBR_ENABLE_BORDER_ROUTING
#include <openthread/border_routing.h>
#endif
#include <openthread/ip6.h>

#include "common/code_utils.hpp"
#include "common/mainloop.hpp"
#include "common/types.hpp"

namespace otbr {

class Netif : public MainloopProcessor, private NonCopyable
{
public:
    class Dependencies
    {
    public:
        virtual ~Dependencies(void) = default;

        virtual otbrError Ip6Send(const uint8_t *aData, uint16_t aLength);
        virtual otbrError Ip6MulAddrUpdateSubscription(const otIp6Address &aAddress, bool aIsAdded);
#if OTBR_ENABLE_DHCP6_PD && OTBR_ENABLE_BORDER_ROUTING
        virtual otbrError BorderRoutingProcessDhcp6PdPrefix(const otBorderRoutingPrefixTableEntry *aPrefixInfo);
#endif
    };

    Netif(const std::string &aInterfaceName, Dependencies &aDependencies);

    otbrError Init(void);
    void      Deinit(void);

    void      UpdateIp6UnicastAddresses(const std::vector<Ip6AddressInfo> &aAddrInfos);
    otbrError UpdateIp6MulticastAddresses(const std::vector<Ip6Address> &aAddrs);
    void      SetNetifState(bool aState);

    enum class UnicastAddressAction : uint8_t
    {
        kAdd,     ///< Add a new address.
        kRemove,  ///< Remove an existing address.
        kReplace, ///< Replace metadata of an existing address in-place.
    };

    struct UnicastAddressChange
    {
        Ip6AddressInfo       mAddressInfo;
        UnicastAddressAction mAction;
    };

    using UnicastAddressChangeHandler =
        std::function<std::vector<otbrError>(const std::vector<UnicastAddressChange> &)>;

    void Ip6Receive(const uint8_t *aBuf, uint16_t aLen);

    unsigned int GetIfIndex(void) const { return mNetifIndex; }

private:
    friend class NetifTestPeer;

    // TODO: Retrieve the Maximum Ip6 size from the coprocessor.
    static constexpr size_t kIp6Mtu = 1280;

    struct PendingNetlinkTxQueue
    {
        static constexpr size_t kMaxBufferSize = 512;
        static constexpr size_t kMaxEntries    = 16;

        struct Entry
        {
            uint16_t mLength;
            alignas(uint32_t) uint8_t mBuffer[kMaxBufferSize];
        };

        bool IsEmpty(void) const { return mCount == 0; }
        bool IsFull(void) const { return mCount == kMaxEntries; }

        void Clear(void)
        {
            mHead  = 0;
            mCount = 0;
        }

        const Entry &Front(void) const { return mEntries[mHead]; }

        void PopFront(void)
        {
            mHead = (mHead + 1) % kMaxEntries;
            mCount--;
        }

        void PushBack(const void *aBuffer, size_t aLength)
        {
            Entry &entry = mEntries[(mHead + mCount) % kMaxEntries];

            memcpy(entry.mBuffer, aBuffer, aLength);
            entry.mLength = static_cast<uint16_t>(aLength);
            mCount++;
        }

        Entry  mEntries[kMaxEntries];
        size_t mHead  = 0;
        size_t mCount = 0;
    };

    struct PendingNetlinkRequest
    {
        static constexpr size_t kMaxPayloadLen = 512;
        enum : uint32_t
        {
            kTimeoutMs = 50,
        };

        uint32_t                              mSequence;
        UnicastAddressAction                  mAction;
        uint8_t                               mRetryCount;
        std::chrono::steady_clock::time_point mExpireTime;
        Ip6AddressInfo                        mAddressInfo;
        uint16_t                              mPayloadLen;
        alignas(uint32_t) uint8_t mPayload[kMaxPayloadLen];
    };

    void Clear(void);

    otbrError CreateTunDevice(const std::string &aInterfaceName);
    otbrError InitNetlink(void);
    otbrError InitMldListener(void);

    void PlatformSpecificInit(void);
    void SetAddrGenModeToNone(void);

    otbrError                   SendNetlinkMessage(const void *aBuffer, size_t aLength);
    void                        ProcessPendingNetlinkTx(void);
    void                        ProcessNetlinkEvent(void);
    void                        PruneExpiredNetlinkRequests(void);
    void                        RecordPendingNetlinkRequest(uint32_t              aSeq,
                                                            UnicastAddressAction  aAction,
                                                            const Ip6AddressInfo &aAddressInfo,
                                                            const void           *aPayload,
                                                            size_t                aPayloadLen);
    static void                 ApplyUnicastAddressChange(std::vector<Ip6AddressInfo> &aAddresses,
                                                          UnicastAddressAction         aAction,
                                                          const Ip6AddressInfo        &aAddressInfo);
    void                        CommitUnicastAddressChange(const PendingNetlinkRequest &aRequest);
    std::vector<Ip6AddressInfo> GetEffectiveUnicastAddresses(void) const;
    void                        HandleNetlinkAck(uint32_t aSeq, int aKernelErr);

    static std::vector<Ip6AddressInfo> ReconcileIp6UnicastAddresses(
        const std::vector<Ip6AddressInfo> &aCachedAddrInfos,
        const std::vector<Ip6AddressInfo> &aDesiredAddrInfos,
        const UnicastAddressChangeHandler &aChangeHandler);
    std::vector<otbrError> ProcessUnicastAddressChanges(const std::vector<UnicastAddressChange> &aChanges);
    otbrError              ProcessMulticastAddressChange(const Ip6Address &aAddress, bool aIsAdded);
    void                   ProcessIp6Send(void);
    void                   ProcessMldEvent(void);
#if OTBR_ENABLE_DHCP6_PD && OTBR_ENABLE_BORDER_ROUTING
    otbrError TryProcessIcmp6RaMessage(const uint8_t *aData, uint16_t aLength);
#endif

    void Update(MainloopContext &aContext) override;
    void Process(const MainloopContext &aContext) override;

    int      mTunFd;           ///< Used to exchange IPv6 packets.
    int      mIpFd;            ///< Used to manage IPv6 stack on the network interface.
    int      mNetlinkFd;       ///< Used to receive netlink events.
    int      mMldFd;           ///< Used to receive MLD events.
    uint32_t mNetlinkSequence; ///< Netlink message sequence.

    unsigned int mNetifIndex;
    std::string  mNetifName;

    PendingNetlinkTxQueue              mPendingNetlinkTxQueue;
    std::vector<PendingNetlinkRequest> mPendingNetlinkRequests;
    std::vector<Ip6AddressInfo>        mIp6UnicastAddresses;
    std::vector<Ip6Address>            mIp6MulticastAddresses;
    Dependencies                      &mDeps;
};

} // namespace otbr

#endif // OTBR_AGENT_POSIX_NETIF_HPP_
