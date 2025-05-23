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
 *   This file includes definitions of the Infrastructure network interface of otbr-agent.
 */

#ifndef OTBR_AGENT_POSIX_INFRA_IF_HPP_
#define OTBR_AGENT_POSIX_INFRA_IF_HPP_

#include <net/if.h>

#include <vector>

#include <openthread/ip6.h>

#include "common/code_utils.hpp"
#include "common/mainloop.hpp"
#include "common/types.hpp"

namespace otbr {

/**
 * Host infrastructure network interface module.
 *
 * The infrastructure network interface MUST be explicitly set by `SetInfraIf` before the InfraIf module can work.
 *
 */
class InfraIf : public MainloopProcessor, private NonCopyable
{
public:
    class Dependencies
    {
    public:
        virtual ~Dependencies(void) = default;

        virtual otbrError SetInfraIf(unsigned int                   aInfraIfIndex,
                                     bool                           aIsRunning,
                                     const std::vector<Ip6Address> &aIp6Addresses);
        virtual otbrError HandleIcmp6Nd(uint32_t          aInfraIfIndex,
                                        const Ip6Address &aSrcAddress,
                                        const uint8_t    *aData,
                                        uint16_t          aDataLen);
    };

    InfraIf(Dependencies &aDependencies);

    void      Init(void);
    void      Deinit(void);
    otbrError SetInfraIf(std::string aInfraIfName);
    otbrError SendIcmp6Nd(uint32_t            aInfraIfIndex,
                          const otIp6Address &aDestAddress,
                          const uint8_t      *aBuffer,
                          uint16_t            aBufferLength);

    unsigned int GetIfIndex(void) const { return mInfraIfIndex; }

private:
    static int              CreateIcmp6Socket(const char *aInfraIfName);
    bool                    IsRunning(const std::vector<Ip6Address> &aAddrs) const;
    short                   GetFlags(void) const;
    std::vector<Ip6Address> GetAddresses(void);
    static bool             HasLinkLocalAddress(const std::vector<Ip6Address> &aAddrs);
    void                    ReceiveIcmp6Message(void);
#ifdef __linux__
    void ReceiveNetlinkMessage(void);
#endif

    void Process(const MainloopContext &aContext) override;
    void Update(MainloopContext &aContext) override;

    Dependencies &mDeps;
    std::string   mInfraIfName;
    unsigned int  mInfraIfIndex;
#ifdef __linux__
    int mNetlinkSocket;
#endif
    int mInfraIfIcmp6Socket;
};

} // namespace otbr

#endif // OTBR_AGENT_POSIX_INFRA_IF_HPP_
