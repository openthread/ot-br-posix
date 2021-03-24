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

#if OTBR_ENABLE_SRP_ADVERTISING_PROXY

#include <stdint.h>

#include <openthread/instance.h>
#include <openthread/srp_server.h>

#include "agent/ncp_openthread.hpp"
#include "mdns/mdns.hpp"

namespace otbr {

/**
 * This class implements the Advertising Proxy.
 *
 */
class AdvertisingProxy
{
public:
    /**
     * This constructor initializes the Advertising Proxy object.
     *
     * @param[in]  aNcp        A reference to the NCP controller.
     * @param[in]  aPublisher  A reference to the mDNS publisher.
     *
     */
    explicit AdvertisingProxy(Ncp::ControllerOpenThread &aNcp, Mdns::Publisher &aPublisher);

    /**
     * This method starts the Advertising Proxy.
     *
     * @retval  OTBR_ERROR_NONE  Successfully started the Advertising Proxy.
     * @retval  ...              Failed to start the Advertising Proxy.
     *
     */
    otbrError Start(void);

    /**
     * This method stops the Advertising Proxy.
     *
     */
    void Stop();

private:
    struct OutstandingUpdate
    {
        typedef std::vector<std::pair<std::string, std::string>> ServiceNameList;

        const otSrpServerHost *mHost = nullptr;    // The SRP host which being published.
        std::string            mHostName;          // The host name.
        ServiceNameList        mServiceNames;      // The list of service instance and name pair.
        uint32_t               mCallbackCount = 0; // The number of callbacks which we are waiting for.
    };

    static void AdvertisingHandler(const otSrpServerHost *aHost, uint32_t aTimeout, void *aContext);
    void        AdvertisingHandler(const otSrpServerHost *aHost, uint32_t aTimeout);

    static Mdns::Publisher::TxtList MakeTxtList(const otSrpServerService *aSrpService);

    static void PublishServiceHandler(const char *aName, const char *aType, otbrError aError, void *aContext);
    void        PublishServiceHandler(const char *aName, const char *aType, otbrError aError);
    static void PublishHostHandler(const char *aName, otbrError aError, void *aContext);
    void        PublishHostHandler(const char *aName, otbrError aError);

    otInstance *GetInstance(void) { return mNcp.GetInstance(); }

    // A reference to the NCP controller, has no ownership.
    Ncp::ControllerOpenThread &mNcp;

    // A reference to the mDNS publisher, has no ownership.
    Mdns::Publisher &mPublisher;

    // A vector that tracks outstanding updates.
    std::vector<OutstandingUpdate> mOutstandingUpdates;
};

} // namespace otbr

#endif // OTBR_ENABLE_SRP_ADVERTISING_PROXY

#endif // OTBR_SRP_ADVERTISING_PROXY_HPP_
