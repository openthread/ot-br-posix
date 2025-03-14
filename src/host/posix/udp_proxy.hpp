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

/**
 * @brief
 *   This module includes definition for Thread UDP Proxy.
 */

#ifndef OTBR_AGENT_POSIX_UDP_PROXY_HPP_
#define OTBR_AGENT_POSIX_UDP_PROXY_HPP_

#include <openthread/error.h>
#include <openthread/ip6.h>

#include "common/mainloop.hpp"
#include "common/types.hpp"

namespace otbr {

class UdpProxy : public MainloopProcessor
{
public:
    class Dependencies
    {
    public:
        virtual ~Dependencies(void) = default;

        virtual otbrError UdpForward(const uint8_t      *aUdpPayload,
                                     uint16_t            aLength,
                                     const otIp6Address &aRemoteAddr,
                                     uint16_t            aRemotePort,
                                     const UdpProxy     &aUdpProxy);
    };

    /**
     * The constructor to initialize the Thread Border Agent UDP Proxy.
     */
    explicit UdpProxy(Dependencies &aDeps);

    ~UdpProxy(void) = default;

    /**
     * Start the UDP Proxy for Thread UDP port @p aPort.
     *
     * The UDP Proxy will bind to an ephemeral port and set a mapping between the ephemeral port and @p aPort.
     *
     * @param[in] aPort  The UDP port to be proxied in Thread stack.
     */
    void Start(uint16_t aPort);

    /**
     * Stop the UDP Proxy if started.
     */
    void Stop(void);

    /**
     * Get the ephemeral UDP port bound on host.
     *
     * @returns The UDP port bound on the host. If the proxy isn't running, `0` will be returned.
     */
    uint16_t GetHostPort(void) const { return mHostPort; }

    /**
     * Get the UDP port on the Thread side.
     *
     * @returns The UDP port on the Thread side. If the proxy isn't running, `0` will be returned.
     */
    uint16_t GetThreadPort(void) const { return mThreadPort; }

    /**
     * Sends a UDP packet to the peer.
     *
     * @param[in] aUdpPlayload  The UDP payload.
     * @param[in] aLength       Then length of the UDP payload.
     * @param[in] aPeerAddr     The address of the peer.
     * @param[in] aPeerPort     The UDP of the peer.
     */
    void SendToPeer(const uint8_t *aUdpPayload, uint16_t aLength, const otIp6Address &aPeerAddr, uint16_t aPeerPort);

private:
    // MainloopProcessor methods
    void Process(const MainloopContext &aMainloop) override;
    void Update(MainloopContext &aMainloop) override;

    bool      IsStarted(void) const { return mHostPort != 0; }
    otbrError BindToEphemeralPort(void);
    otbrError ReceivePacket(uint8_t *aPayload, uint16_t &aLength, otIp6Address &aRemoteAddr, uint16_t &aRemotePort);

    int      mFd; ///< Used to proxy UDP packets in Thread network.
    uint16_t mHostPort;
    uint16_t mThreadPort;

    Dependencies &mDeps;
};

} // namespace otbr

#endif // OTBR_AGENT_POSIX_UDP_PROXY_HPP_
