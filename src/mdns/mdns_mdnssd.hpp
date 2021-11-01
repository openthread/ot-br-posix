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
 *   This file includes definition for mDNS service.
 */

#ifndef OTBR_AGENT_MDNS_MDNSSD_HPP_
#define OTBR_AGENT_MDNS_MDNSSD_HPP_

#include <array>
#include <map>
#include <utility>
#include <vector>

#include <assert.h>
#include <dns_sd.h>

#include "common/code_utils.hpp"
#include "common/mainloop.hpp"
#include "common/types.hpp"
#include "mdns/mdns.hpp"

namespace otbr {

namespace Mdns {

/**
 * This class implements mDNS service with mDNSResponder.
 *
 */
class PublisherMDnsSd : public MainloopProcessor, public Publisher
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

    // Implementation of Mdns::Publisher.

    otbrError PublishService(const char *       aHostName,
                             uint16_t           aPort,
                             const char *       aName,
                             const char *       aType,
                             const SubTypeList &aSubTypeList,
                             const TxtList &    aTxtList) override;
    otbrError UnpublishService(const char *aName, const char *aType) override;
    otbrError PublishHost(const char *aName, const uint8_t *aAddress, uint8_t aAddressLength) override;
    otbrError UnpublishHost(const char *aName) override;
    void      SubscribeService(const std::string &aType, const std::string &aInstanceName) override;
    void      UnsubscribeService(const std::string &aType, const std::string &aInstanceName) override;
    void      SubscribeHost(const std::string &aHostName) override;
    void      UnsubscribeHost(const std::string &aHostName) override;
    otbrError Start(void) override;
    bool      IsStarted(void) const override;
    void      Stop(void) override;

    // Implementation of MainloopProcessor.

    void Update(MainloopContext &aMainloop) override;
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
        std::string   mRegType; ///< Service type with optional subtypes separated by commas
        DNSServiceRef mService;
        uint16_t      mPort = 0;
    };

    struct Host
    {
        char                                       mName[kMaxSizeOfServiceName];
        std::array<uint8_t, OTBR_IP6_ADDRESS_SIZE> mAddress;
        DNSRecordRef                               mRecord;
    };

    struct Subscription
    {
        PublisherMDnsSd *mMDnsSd;
        DNSServiceRef    mServiceRef;

        explicit Subscription(PublisherMDnsSd &aMDnsSd)
            : mMDnsSd(&aMDnsSd)
            , mServiceRef(nullptr)
        {
        }

        void Release(void);
        void DeallocateServiceRef(void);
    };

    struct ServiceSubscription : public Subscription
    {
        explicit ServiceSubscription(PublisherMDnsSd &aMDnsSd, std::string aType, std::string aInstanceName)
            : Subscription(aMDnsSd)
            , mType(std::move(aType))
            , mInstanceName(std::move(aInstanceName))
        {
        }

        void Browse(void);
        void Resolve(uint32_t aInterfaceIndex, const char *aInstanceName, const char *aType, const char *aDomain);
        void GetAddrInfo(uint32_t aInterfaceIndex);

        static void HandleBrowseResult(DNSServiceRef       aServiceRef,
                                       DNSServiceFlags     aFlags,
                                       uint32_t            aInterfaceIndex,
                                       DNSServiceErrorType aErrorCode,
                                       const char *        aInstanceName,
                                       const char *        aType,
                                       const char *        aDomain,
                                       void *              aContext);
        void        HandleBrowseResult(DNSServiceRef       aServiceRef,
                                       DNSServiceFlags     aFlags,
                                       uint32_t            aInterfaceIndex,
                                       DNSServiceErrorType aErrorCode,
                                       const char *        aInstanceName,
                                       const char *        aType,
                                       const char *        aDomain);
        static void HandleResolveResult(DNSServiceRef        aServiceRef,
                                        DNSServiceFlags      aFlags,
                                        uint32_t             aInterfaceIndex,
                                        DNSServiceErrorType  aErrorCode,
                                        const char *         aFullName,
                                        const char *         aHostTarget,
                                        uint16_t             aPort, // In network byte order.
                                        uint16_t             aTxtLen,
                                        const unsigned char *aTxtRecord,
                                        void *               aContext);
        void        HandleResolveResult(DNSServiceRef        aServiceRef,
                                        DNSServiceFlags      aFlags,
                                        uint32_t             aInterfaceIndex,
                                        DNSServiceErrorType  aErrorCode,
                                        const char *         aFullName,
                                        const char *         aHostTarget,
                                        uint16_t             aPort, // In network byte order.
                                        uint16_t             aTxtLen,
                                        const unsigned char *aTxtRecord);
        static void HandleGetAddrInfoResult(DNSServiceRef          aServiceRef,
                                            DNSServiceFlags        aFlags,
                                            uint32_t               aInterfaceIndex,
                                            DNSServiceErrorType    aErrorCode,
                                            const char *           aHostName,
                                            const struct sockaddr *aAddress,
                                            uint32_t               aTtl,
                                            void *                 aContext);
        void        HandleGetAddrInfoResult(DNSServiceRef          aServiceRef,
                                            DNSServiceFlags        aFlags,
                                            uint32_t               aInterfaceIndex,
                                            DNSServiceErrorType    aErrorCode,
                                            const char *           aHostName,
                                            const struct sockaddr *aAddress,
                                            uint32_t               aTtl);

        std::string            mType;
        std::string            mInstanceName;
        DiscoveredInstanceInfo mInstanceInfo;
    };

    struct HostSubscription : public Subscription
    {
        explicit HostSubscription(PublisherMDnsSd &aMDnsSd, std::string aHostName)
            : Subscription(aMDnsSd)
            , mHostName(std::move(aHostName))
        {
        }

        void        Resolve(void);
        static void HandleResolveResult(DNSServiceRef          aServiceRef,
                                        DNSServiceFlags        aFlags,
                                        uint32_t               aInterfaceIndex,
                                        DNSServiceErrorType    aErrorCode,
                                        const char *           aHostName,
                                        const struct sockaddr *aAddress,
                                        uint32_t               aTtl,
                                        void *                 aContext);
        void        HandleResolveResult(DNSServiceRef          aServiceRef,
                                        DNSServiceFlags        aFlags,
                                        uint32_t               aInterfaceIndex,
                                        DNSServiceErrorType    aErrorCode,
                                        const char *           aHostName,
                                        const struct sockaddr *aAddress,
                                        uint32_t               aTtl);

        std::string        mHostName;
        DiscoveredHostInfo mHostInfo;
    };

    typedef std::vector<Service>             Services;
    typedef std::vector<Host>                Hosts;
    typedef std::vector<Service>::iterator   ServiceIterator;
    typedef std::vector<Host>::iterator      HostIterator;
    typedef std::vector<ServiceSubscription> ServiceSubscriptionList;
    typedef std::vector<HostSubscription>    HostSubscriptionList;

    void        DiscardService(const char *aName, const char *aType, DNSServiceRef aServiceRef = nullptr);
    void        RecordService(const char *  aName,
                              const char *  aType,
                              const char *  aRegType,
                              uint16_t      aPort,
                              DNSServiceRef aServiceRef);
    static bool IsServiceOutdated(const Service &service, const std::string &aNewRegType, int aNewPort);

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

    otbrError          MakeFullName(char *aFullName, size_t aFullNameLength, const char *aName);
    static std::string MakeRegType(const char *aType, const SubTypeList &aSubTypeList);

    ServiceIterator FindPublishedService(const char *aName, const char *aType);
    ServiceIterator FindPublishedService(const DNSServiceRef &aServiceRef);
    HostIterator    FindPublishedHost(const DNSRecordRef &aRecordRef);
    HostIterator    FindPublishedHost(const char *aHostName);

    void        OnServiceResolved(ServiceSubscription &aService);
    static void OnServiceResolveFailed(const ServiceSubscription &aService, DNSServiceErrorType aErrorCode);
    void        OnHostResolved(HostSubscription &aHost);
    void        OnHostResolveFailed(const HostSubscription &aHost, DNSServiceErrorType aErrorCode);

    Services      mServices;
    Hosts         mHosts;
    DNSServiceRef mHostsRef;
    const char *  mDomain;
    State         mState;
    StateHandler  mStateHandler;
    void *        mContext;

    ServiceSubscriptionList mSubscribedServices;
    HostSubscriptionList    mSubscribedHosts;
};

/**
 * @}
 */

} // namespace Mdns

} // namespace otbr

#endif // OTBR_AGENT_MDNS_MDNSSD_HPP_
