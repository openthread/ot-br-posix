/*
 *    Copyright (c) 2018, The OpenThread Authors.
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
 *   This file includes definition for MDNS service.
 */

#ifndef OTBR_AGENT_MDNS_MDNSSD_HPP_
#define OTBR_AGENT_MDNS_MDNSSD_HPP_

#include <array>
#include <vector>

#include <dns_sd.h>

#include "common/types.hpp"
#include "mdns/mdns.hpp"

namespace otbr {

namespace Mdns {

/**
 * This class implements MDNS service with avahi.
 *
 */
class PublisherMDnsSd : public Publisher
{
public:
    /**
     * The constructor to initialize a Publisher.
     *
     * @param[in]   aProtocol           The protocol used for publishing. IPv4, IPv6 or both.
     * @param[in]   aDomain             The domain of the host. nullptr to use default.
     * @param[in]   aHandler            The function to be called when state changes.
     * @param[in]   aContext            A pointer to application-specific context.
     *
     */
    PublisherMDnsSd(int aProtocol, const char *aDomain, StateHandler aHandler, void *aContext);

    ~PublisherMDnsSd(void) override;

    /**
     * This method publishes or updates a service.
     *
     * @param[in]   aHostName           The name of the host which this service resides on. If NULL is provided,
     *                                  this service resides on local host and it is the implementation to provide
     *                                  specific host name. Otherwise, the caller MUST publish the host with method
     *                                  PublishHost.
     * @param[in]   aName               The name of this service.
     * @param[in]   aType               The type of this service.
     * @param[in]   aPort               The port number of this service.
     * @param[in]   aTxtList            A list of TXT name/value pairs.
     *
     * @retval  OTBR_ERROR_NONE     Successfully published or updated the service.
     * @retval  OTBR_ERROR_ERRNO    Failed to publish or update the service.
     *
     */
    otbrError PublishService(const char *   aHostName,
                             uint16_t       aPort,
                             const char *   aName,
                             const char *   aType,
                             const TxtList &aTxtList) override;

    /**
     * This method un-publishes a service.
     *
     * @param[in]   aName               The name of this service.
     * @param[in]   aType               The type of this service.
     *
     * @retval  OTBR_ERROR_NONE     Successfully un-published the service.
     * @retval  OTBR_ERROR_ERRNO    Failed to un-publish the service.
     *
     */
    otbrError UnpublishService(const char *aName, const char *aType) override;

    /**
     * This method publishes or updates a host.
     *
     * Publishing a host is advertising an AAAA RR for the host name. This method should be called
     * before a service with non-null host name is published.
     *
     * @param[in]  aName           The name of the host.
     * @param[in]  aAddress        The address of the host.
     * @param[in]  aAddressLength  The length of @p aAddress.
     *
     * @retval  OTBR_ERROR_NONE     Successfully published or updated the host.
     * @retval  OTBR_ERROR_ERRNO    Failed to publish or update the host.
     *
     */
    otbrError PublishHost(const char *aName, const uint8_t *aAddress, uint8_t aAddressLength) override;

    /**
     * This method un-publishes a host.
     *
     * @param[in]  aName  A host name.
     *
     * @retval  OTBR_ERROR_NONE     Successfully un-published the host.
     * @retval  OTBR_ERROR_ERRNO    Failed to un-publish the host.
     *
     * @note  All services reside on this host should be un-published by UnpublishService.
     *
     */
    otbrError UnpublishHost(const char *aName) override;

    /**
     * This method starts the MDNS service.
     *
     * @retval OTBR_ERROR_NONE  Successfully started MDNS service;
     * @retval OTBR_ERROR_MDNS  Failed to start MDNS service.
     *
     */
    otbrError Start(void) override;

    /**
     * This method checks if publisher has been started.
     *
     * @retval true     Already started.
     * @retval false    Not started.
     *
     */
    bool IsStarted(void) const override;

    /**
     * This method stops the MDNS service.
     *
     */
    void Stop(void) override;

    /**
     * This method updates the mainloop context.
     *
     * @param[inout]  aMainloop  A reference to the mainloop to be updated.
     *
     */
    void Update(MainloopContext &aMainloop) override;

    /**
     * This method processes mainloop events.
     *
     * @param[in]  aMainloop  A reference to the mainloop context.
     *
     */
    void Process(const MainloopContext &aMainloop) override;

private:
    enum
    {
        kMaxSizeOfTxtRecord   = 256,
        kMaxSizeOfServiceName = kDNSServiceMaxServiceName,
        kMaxSizeOfHost        = 128,
        kMaxSizeOfDomain      = kDNSServiceMaxDomainName,
        kMaxSizeOfServiceType = 69,
    };

    struct Service
    {
        char          mName[kMaxSizeOfServiceName];
        char          mType[kMaxSizeOfServiceType];
        DNSServiceRef mService;
    };

    struct Host
    {
        char                                       mName[kMaxSizeOfServiceName];
        std::array<uint8_t, OTBR_IP6_ADDRESS_SIZE> mAddress;
        DNSRecordRef                               mRecord;
    };

    typedef std::vector<Service>           Services;
    typedef std::vector<Host>              Hosts;
    typedef std::vector<Service>::iterator ServiceIterator;
    typedef std::vector<Host>::iterator    HostIterator;

    void DiscardService(const char *aName, const char *aType, DNSServiceRef aServiceRef = nullptr);
    void RecordService(const char *aName, const char *aType, DNSServiceRef aServiceRef);

    otbrError DiscardHost(const char *aName, bool aSendGoodbye = true);
    void      RecordHost(const char *aName, const uint8_t *aAddress, uint8_t aAddressLength, DNSRecordRef aRecordRef);

    static void HandleServiceRegisterResult(DNSServiceRef         aService,
                                            const DNSServiceFlags aFlags,
                                            DNSServiceErrorType   aError,
                                            const char *          aName,
                                            const char *          aType,
                                            const char *          aDomain,
                                            void *                aContext);
    void        HandleServiceRegisterResult(DNSServiceRef         aService,
                                            const DNSServiceFlags aFlags,
                                            DNSServiceErrorType   aError,
                                            const char *          aName,
                                            const char *          aType,
                                            const char *          aDomain);
    static void HandleRegisterHostResult(DNSServiceRef       aHostsConnection,
                                         DNSRecordRef        aHostRecord,
                                         DNSServiceFlags     aFlags,
                                         DNSServiceErrorType aErrorCode,
                                         void *              aContext);
    void        HandleRegisterHostResult(DNSServiceRef       aHostsConnection,
                                         DNSRecordRef        aHostRecord,
                                         DNSServiceFlags     aFlags,
                                         DNSServiceErrorType aErrorCode);

    otbrError MakeFullName(char *aFullName, size_t aFullNameLength, const char *aName);

    ServiceIterator FindPublishedService(const char *aName, const char *aType);
    ServiceIterator FindPublishedService(const DNSServiceRef &aServiceRef);
    HostIterator    FindPublishedHost(const DNSRecordRef &aRecordRef);
    HostIterator    FindPublishedHost(const char *aHostName);

    Services      mServices;
    Hosts         mHosts;
    DNSServiceRef mHostsRef;
    const char *  mDomain;
    State         mState;
    StateHandler  mStateHandler;
    void *        mContext;
};

/**
 * @}
 */

} // namespace Mdns

} // namespace otbr

#endif // OTBR_AGENT_MDNS_MDNSSD_HPP_
