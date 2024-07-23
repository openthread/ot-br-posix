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

#include <list>
#include <string.h>
#include <string>
#include <vector>

#include <openthread/ip6.h>

#include <openthread-br/otbr-posix-config.h>

#include "common/mainloop.hpp"
#include "ncp/ncp_spinel.hpp"

namespace otbr {
namespace Posix {

class Netif
{
public:
    /**
     * Constructor.
     *
     * @param[in]  aInterfaceName  A string of the NCP interface name.
     *
     */
    Netif(const char *aInterfaceName);

    /**
     * This method initializes the posix platform network interface.
     *
     */
    void Init(void);

    /**
     * This method de-initializes the posix platform network interface.
     *
     */
    void Deinit(void);

    /**
     * This method processes the posix platform network interface tasks.
     *
     * @param[in] aContext  A pointer to the mainloop context.
     *
     */
    void Process(const MainloopContext *aContext);

    /**
     * This method updates the FD set of the mainloop with the FDs used in this module.
     *
     * @param[in] aContext  A pointer to the mainloop context.
     *
     */
    void UpdateFdSet(MainloopContext *aContext);

    /**
     * This method updates the Ip6 addresses of the network interface.
     *
     * @param[in] aAddresses  The latest Ip6 addresses.
     *
     */
    void UpdateIp6Addresses(const std::vector<otIp6AddressInfo> &aAddresses);

    /**
     * This method updates the Ip6 multicast addresses of the network interface.
     *
     * @param[in] aAddresses  The latest Ip6 multicast addresses.
     *
     */
    void UpdateIp6MulticastAddresses(const std::vector<otIp6Address> &aAddresses);

private:
    static constexpr size_t kMaxIp6Size = 1280;

    enum SocketBlockOption
    {
        kSocketBlock,
        kSocketNonBlock,
    };

    struct Ip6AddressInfo
    {
        Ip6AddressInfo(const otIp6AddressInfo &aInfo)
            : mPrefixLength(aInfo.mPrefixLength)
            , mScope(aInfo.mScope)
            , mPreferred(aInfo.mPreferred)
            , mMeshLocal(aInfo.mMeshLocal)
        {
            memcpy(&mAddress, aInfo.mAddress, sizeof(otIp6Address));
        }

        otIp6Address mAddress;       ///< A pointer to the IPv6 address.
        uint8_t      mPrefixLength;  ///< The prefix length of mAddress if it is a unicast address.
        uint8_t      mScope : 4;     ///< The scope of this address.
        bool         mPreferred : 1; ///< Whether this is a preferred address.
        bool         mMeshLocal : 1; ///< Whether this is a mesh-local unicast/anycast address.

        bool operator==(const Ip6AddressInfo &aOther) const
        {
            return memcmp(this, &aOther, sizeof(Ip6AddressInfo)) == 0;
        }
    };

    static bool CompareIp6Address(const otIp6Address &aObj1, const otIp6Address &aObj2);

    static int SocketWithCloseExec(int aDomain, int aType, int aProtocol, SocketBlockOption aBlockOption);

    void PlatInit(void);
    void PlatConfigureNetLink(void);
    void PlatConfigureTunDevice(void);
    void PlatUpdateUnicast(const Ip6AddressInfo &aAddressInfo, bool aIsAdded);
    void PlatProcessNetlinkEvent(void);

    void SetAddrGenModeToNone(void);
    void ProcessAddressChange(const Ip6AddressInfo &aAddressInfo, bool aIsAdded);
    void UpdateMulticast(const otIp6Address &aAddress, bool aIsAdded);

    const char *mInterfaceName;

    int      mTunFd;           ///< Used to exchange IPv6 packets.
    int      mIpFd;            ///< Used to manage IPv6 stack on Thread interface.
    int      mNetlinkFd;       ///< Used to receive netlink events.
    uint32_t mNetlinkSequence; ///< Netlink message sequence.

    unsigned int mNetifIndex;
    char         mNetifName[IFNAMSIZ];

    std::list<Ip6AddressInfo> mIp6Addresses;
    std::list<Ip6AddressInfo> mIp6AddressesUpdate;
    std::vector<otIp6Address> mIp6MulticastAddresses;
};

} // namespace Posix
} // namespace otbr

#endif // OTBR_AGENT_POSIX_NETIF_HPP_
