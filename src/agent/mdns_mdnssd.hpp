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

#ifndef MDNS_MDNSSD_HPP_
#define MDNS_MDNSSD_HPP_

#include <dns_sd.h>
#include <vector>

#include "mdns.hpp"
#include "common/types.hpp"

namespace ot {

namespace BorderRouter {

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
     * @param[in]   aHost               The name of host residing the services to be published.
                                        NULL to use default.
     * @param[in]   aDomain             The domain of the host. NULL to use default.
     * @param[in]   aHandler            The function to be called when state changes.
     * @param[in]   aContext            A pointer to application-specific context.
     *
     */
    PublisherMDnsSd(int aProtocol, const char *aHost, const char *aDomain, StateHandler aHandler, void *aContext);

    ~PublisherMDnsSd(void);

    /**
     * This method publishes or updates a service.
     *
     * @note only text record can be updated.
     *
     * @param[in]   aName               The name of this service.
     * @param[in]   aType               The type of this service.
     * @param[in]   aPort               The port number of this service.
     * @param[in]   ...                 Pointers to null-terminated string of key and value for text record.
     *                                  The last argument must be NULL.
     *
     * @retval  OTBR_ERROR_NONE     Successfully published or updated the service.
     * @retval  OTBR_ERROR_ERRNO    Failed to publish or update the service.
     *
     */
    otbrError PublishService(uint16_t aPort, const char *aName, const char *aType, ...);

    /**
     * This method starts the MDNS service.
     *
     * @retval OTBR_ERROR_NONE  Successfully started MDNS service;
     * @retval OTBR_ERROR_MDNS  Failed to start MDNS service.
     *
     */
    otbrError Start(void);

    /**
     * This method checks if publisher has been started.
     *
     * @retval true     Already started.
     * @retval false    Not started.
     *
     */
    bool IsStarted(void) const;

    /**
     * This method stops the MDNS service.
     *
     */
    void Stop(void);

    /**
     * This method performs avahi poll processing.
     *
     * @param[in]   aReadFdSet          A reference to read file descriptors.
     * @param[in]   aWriteFdSet         A reference to write file descriptors.
     * @param[in]   aErrorFdSet         A reference to error file descriptors.
     *
     */
    void Process(const fd_set &aReadFdSet, const fd_set &aWriteFdSet, const fd_set &aErrorFdSet);

    /**
     * This method updates the fd_set and timeout for mainloop.
     *
     * @param[inout]    aReadFdSet      A reference to fd_set for polling read.
     * @param[inout]    aWriteFdSet     A reference to fd_set for polling write.
     * @param[inout]    aErrorFdSet     A reference to fd_set for polling error.
     * @param[inout]    aMaxFd          A reference to the max file descriptor.
     * @param[inout]    aTimeout        A reference to the timeout.
     *
     */
    void UpdateFdSet(fd_set &aReadFdSet, fd_set &aWriteFdSet, fd_set &aErrorFdSet, int &aMaxFd, timeval &aTimeout);

private:
    void DiscardService(const char *aName, const char *aType, DNSServiceRef aServiceRef);
    void RecordService(const char *aName, const char *aType, DNSServiceRef aServiceRef);

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

    enum
    {
        kMaxSizeOfTxtRecord   = 128,
        kMaxSizeOfServiceName = kDNSServiceMaxServiceName,
        kMaxSizeOfHost        = 128,
        kMaxSizeOfDomain      = kDNSServiceMaxDomainName,
        kMaxSizeOfServiceType = 64,
        kMaxTextRecordSize    = 255,
    };

    struct Service
    {
        char          mName[kMaxSizeOfServiceName];
        char          mType[kMaxSizeOfServiceType];
        DNSServiceRef mService;
    };

    typedef std::vector<Service> Services;

    Services     mServices;
    const char * mHost;
    const char * mDomain;
    State        mState;
    StateHandler mStateHandler;
    void *       mContext;
};

/**
 * @}
 */

} // namespace Mdns

} // namespace BorderRouter

} // namespace ot

#endif // MDNS_MDNSSD_HPP_
