/*
 *    Copyright (c) 2020, The OpenThread Authors.
 *    All rights reserved.
 *
 *    Redistribution and use in source and binary forms, with or without
 *    modification, are permitted provided that the following conditions are met:
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *    3. Neither the name of the copyright holder nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 *    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *    ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *    LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *    CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *    ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *    POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 *   This file includes definitions for Advertising Proxy.
 */

#ifndef OTBR_SRP_ADVERTISING_PROXY_HPP_
#define OTBR_SRP_ADVERTISING_PROXY_HPP_

#include "openthread-br/config.h"

#if OTBR_ENABLE_SRP_ADVERTISING_PROXY

#include <stdint.h>

#include <openthread/instance.h>
#include <openthread/srp_server.h>

#include "common/code_utils.hpp"
#include "mdns/mdns.hpp"
#include "ncp/rcp_host.hpp"

namespace otbr {

/**
 * This class implements the Advertising Proxy.
 */
class AdvertisingProxy : private NonCopyable
{
public:
    /**
     * This constructor initializes the Advertising Proxy object.
     *
     * @param[in] aHost       A reference to the NCP controller.
     * @param[in] aPublisher  A reference to the mDNS publisher.
     */
    explicit AdvertisingProxy(Ncp::RcpHost &aHost, Mdns::Publisher &aPublisher);

    /**
     * This method enables/disables the Advertising Proxy.
     *
     * @param[in] aIsEnabled  Whether to enable the Advertising Proxy.
     */
    void SetEnabled(bool aIsEnabled);

    /**
     * This method publishes all registered hosts and services.
     */
    void PublishAllHostsAndServices(void);

    /**
     * This method handles mDNS publisher's state changes.
     *
     * @param[in] aState  The state of mDNS publisher.
     */
    void HandleMdnsState(Mdns::Publisher::State aState);

private:
    struct OutstandingUpdate
    {
        otSrpServerServiceUpdateId mId;                // The ID of the SRP service update transaction.
        std::string                mHostName;          // The host name.
        uint32_t                   mCallbackCount = 0; // The number of callbacks which we are waiting for.
    };

    static void AdvertisingHandler(otSrpServerServiceUpdateId aId,
                                   const otSrpServerHost     *aHost,
                                   uint32_t                   aTimeout,
                                   void                      *aContext);
    void        AdvertisingHandler(otSrpServerServiceUpdateId aId, const otSrpServerHost *aHost, uint32_t aTimeout);

    static Mdns::Publisher::TxtData     MakeTxtData(const otSrpServerService *aSrpService);
    static Mdns::Publisher::SubTypeList MakeSubTypeList(const otSrpServerService *aSrpService);
    void                                OnMdnsPublishResult(otSrpServerServiceUpdateId aUpdateId, otbrError aError);

    std::vector<Ip6Address> GetEligibleAddresses(const otIp6Address *aHostAddresses, uint8_t aHostAddressNum);

    void Start(void);
    void Stop(void);
    bool IsEnabled(void) const { return mIsEnabled; }

    /**
     * This method publishes a specified host and its services.
     *
     * It also makes a OutstandingUpdate object when needed.
     *
     * @param[in]  aHost         A pointer to the host.
     * @param[in]  aUpdate       A pointer to the output OutstandingUpdate object. When it's not null, the method will
     *                           fill its fields, otherwise it's ignored.
     *
     * @retval  OTBR_ERROR_NONE  Successfully published the host and its services.
     * @retval  ...              Failed to publish the host and/or its services.
     */
    otbrError PublishHostAndItsServices(const otSrpServerHost *aHost, OutstandingUpdate *aUpdate);

    otInstance *GetInstance(void) { return mHost.GetInstance(); }

    // A reference to the NCP controller, has no ownership.
    Ncp::RcpHost &mHost;

    // A reference to the mDNS publisher, has no ownership.
    Mdns::Publisher &mPublisher;

    bool mIsEnabled;

    // A vector that tracks outstanding updates.
    std::vector<OutstandingUpdate> mOutstandingUpdates;
};

} // namespace otbr

#endif // OTBR_ENABLE_SRP_ADVERTISING_PROXY

#endif // OTBR_SRP_ADVERTISING_PROXY_HPP_
