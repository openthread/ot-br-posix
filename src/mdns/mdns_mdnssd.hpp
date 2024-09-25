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
 *   This file includes definition for mDNS publisher.
 */

#ifndef OTBR_AGENT_MDNS_MDNSSD_HPP_
#define OTBR_AGENT_MDNS_MDNSSD_HPP_

#include "openthread-br/config.h"

#include <array>
#include <map>
#include <memory>
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
 * This class implements mDNS publisher with mDNSResponder.
 */
class PublisherMDnsSd : public MainloopProcessor, public Publisher
{
public:
    explicit PublisherMDnsSd(StateCallback aCallback);

    ~PublisherMDnsSd(void) override;

    // Implementation of Mdns::Publisher.

    void UnpublishService(const std::string &aName, const std::string &aType, ResultCallback &&aCallback) override;

    void      UnpublishHost(const std::string &aName, ResultCallback &&aCallback) override;
    void      UnpublishKey(const std::string &aName, ResultCallback &&aCallback) override;
    void      SubscribeService(const std::string &aType, const std::string &aInstanceName) override;
    void      UnsubscribeService(const std::string &aType, const std::string &aInstanceName) override;
    void      SubscribeHost(const std::string &aHostName) override;
    void      UnsubscribeHost(const std::string &aHostName) override;
    otbrError Start(void) override;
    bool      IsStarted(void) const override;
    void      Stop(void) override { Stop(kNormalStop); }

    // Implementation of MainloopProcessor.

    void Update(MainloopContext &aMainloop) override;
    void Process(const MainloopContext &aMainloop) override;

protected:
    otbrError PublishServiceImpl(const std::string &aHostName,
                                 const std::string &aName,
                                 const std::string &aType,
                                 const SubTypeList &aSubTypeList,
                                 uint16_t           aPort,
                                 const TxtData     &aTxtData,
                                 ResultCallback   &&aCallback) override;
    otbrError PublishHostImpl(const std::string &aName,
                              const AddressList &aAddress,
                              ResultCallback   &&aCallback) override;
    otbrError PublishKeyImpl(const std::string &aName, const KeyData &aKeyData, ResultCallback &&aCallback) override;
    void      OnServiceResolveFailedImpl(const std::string &aType,
                                         const std::string &aInstanceName,
                                         int32_t            aErrorCode) override;
    void      OnHostResolveFailedImpl(const std::string &aHostName, int32_t aErrorCode) override;
    otbrError DnsErrorToOtbrError(int32_t aErrorCode) override;

private:
    static constexpr uint32_t kDefaultTtl = 10;

    enum StopMode : uint8_t
    {
        kNormalStop,
        kStopOnServiceNotRunningError,
    };

    class DnssdKeyRegistration;

    class DnssdServiceRegistration : public ServiceRegistration
    {
        friend class DnssdKeyRegistration;

    public:
        using ServiceRegistration::ServiceRegistration; // Inherit base constructor

        ~DnssdServiceRegistration(void) override { Unregister(); }

        void      Update(MainloopContext &aMainloop) const;
        void      Process(const MainloopContext &aMainloop, std::vector<DNSServiceRef> &aReadyServices) const;
        otbrError Register(void);

    private:
        void             Unregister(void);
        PublisherMDnsSd &GetPublisher(void) { return *static_cast<PublisherMDnsSd *>(mPublisher); }
        void             HandleRegisterResult(DNSServiceFlags aFlags, DNSServiceErrorType aError);
        static void      HandleRegisterResult(DNSServiceRef       aServiceRef,
                                              DNSServiceFlags     aFlags,
                                              DNSServiceErrorType aError,
                                              const char         *aName,
                                              const char         *aType,
                                              const char         *aDomain,
                                              void               *aContext);

        DNSServiceRef         mServiceRef    = nullptr;
        DnssdKeyRegistration *mRelatedKeyReg = nullptr;
    };

    class DnssdHostRegistration : public HostRegistration
    {
    public:
        using HostRegistration::HostRegistration; // Inherit base class constructor

        ~DnssdHostRegistration(void) override { Unregister(); }

        otbrError Register(void);

    private:
        void             Unregister(void);
        PublisherMDnsSd &GetPublisher(void) { return *static_cast<PublisherMDnsSd *>(mPublisher); }
        void             HandleRegisterResult(DNSRecordRef aRecordRef, DNSServiceErrorType aError);
        static void      HandleRegisterResult(DNSServiceRef       aServiceRef,
                                              DNSRecordRef        aRecordRef,
                                              DNSServiceFlags     aFlags,
                                              DNSServiceErrorType aErrorCode,
                                              void               *aContext);

        std::vector<DNSRecordRef> mAddrRecordRefs;
        std::vector<bool>         mAddrRegistered;
    };

    class DnssdKeyRegistration : public KeyRegistration
    {
        friend class DnssdServiceRegistration;

    public:
        using KeyRegistration::KeyRegistration; // Inherit base class constructor

        ~DnssdKeyRegistration(void) override { Unregister(); }

        otbrError Register(void);

    private:
        void             Unregister(void);
        PublisherMDnsSd &GetPublisher(void) { return *static_cast<PublisherMDnsSd *>(mPublisher); }
        void             HandleRegisterResult(DNSServiceErrorType aError);
        static void      HandleRegisterResult(DNSServiceRef       aServiceRef,
                                              DNSRecordRef        aRecordRef,
                                              DNSServiceFlags     aFlags,
                                              DNSServiceErrorType aErrorCode,
                                              void               *aContext);

        DNSRecordRef              mRecordRef         = nullptr;
        DnssdServiceRegistration *mRelatedServiceReg = nullptr;
    };

    struct ServiceRef : private ::NonCopyable
    {
        DNSServiceRef    mServiceRef;
        PublisherMDnsSd &mPublisher;

        explicit ServiceRef(PublisherMDnsSd &aPublisher)
            : mServiceRef(nullptr)
            , mPublisher(aPublisher)
        {
        }

        ~ServiceRef() { Release(); }

        void Update(MainloopContext &aMainloop) const;
        void Process(const MainloopContext &aMainloop, std::vector<DNSServiceRef> &aReadyServices) const;
        void Release(void);
        void DeallocateServiceRef(void);
    };

    struct ServiceSubscription;

    struct ServiceInstanceResolution : public ServiceRef
    {
        explicit ServiceInstanceResolution(ServiceSubscription &aSubscription,
                                           std::string          aInstanceName,
                                           std::string          aType,
                                           std::string          aDomain,
                                           uint32_t             aNetifIndex)
            : ServiceRef(aSubscription.mPublisher)
            , mSubscription(&aSubscription)
            , mInstanceName(std::move(aInstanceName))
            , mType(std::move(aType))
            , mDomain(std::move(aDomain))
            , mNetifIndex(aNetifIndex)
        {
        }

        void      Resolve(void);
        otbrError GetAddrInfo(uint32_t aInterfaceIndex);
        void      FinishResolution(void);

        static void HandleResolveResult(DNSServiceRef        aServiceRef,
                                        DNSServiceFlags      aFlags,
                                        uint32_t             aInterfaceIndex,
                                        DNSServiceErrorType  aErrorCode,
                                        const char          *aFullName,
                                        const char          *aHostTarget,
                                        uint16_t             aPort, // In network byte order.
                                        uint16_t             aTxtLen,
                                        const unsigned char *aTxtRecord,
                                        void                *aContext);
        void        HandleResolveResult(DNSServiceRef        aServiceRef,
                                        DNSServiceFlags      aFlags,
                                        uint32_t             aInterfaceIndex,
                                        DNSServiceErrorType  aErrorCode,
                                        const char          *aFullName,
                                        const char          *aHostTarget,
                                        uint16_t             aPort, // In network byte order.
                                        uint16_t             aTxtLen,
                                        const unsigned char *aTxtRecord);
        static void HandleGetAddrInfoResult(DNSServiceRef          aServiceRef,
                                            DNSServiceFlags        aFlags,
                                            uint32_t               aInterfaceIndex,
                                            DNSServiceErrorType    aErrorCode,
                                            const char            *aHostName,
                                            const struct sockaddr *aAddress,
                                            uint32_t               aTtl,
                                            void                  *aContext);
        void        HandleGetAddrInfoResult(DNSServiceRef          aServiceRef,
                                            DNSServiceFlags        aFlags,
                                            uint32_t               aInterfaceIndex,
                                            DNSServiceErrorType    aErrorCode,
                                            const char            *aHostName,
                                            const struct sockaddr *aAddress,
                                            uint32_t               aTtl);

        ServiceSubscription   *mSubscription;
        std::string            mInstanceName;
        std::string            mType;
        std::string            mDomain;
        uint32_t               mNetifIndex;
        DiscoveredInstanceInfo mInstanceInfo;
    };

    struct ServiceSubscription : public ServiceRef
    {
        explicit ServiceSubscription(PublisherMDnsSd &aPublisher, std::string aType, std::string aInstanceName)
            : ServiceRef(aPublisher)
            , mType(std::move(aType))
            , mInstanceName(std::move(aInstanceName))
        {
        }

        void Browse(void);
        void Resolve(uint32_t           aNetifIndex,
                     const std::string &aInstanceName,
                     const std::string &aType,
                     const std::string &aDomain);
        void UpdateAll(MainloopContext &aMainloop) const;
        void ProcessAll(const MainloopContext &aMainloop, std::vector<DNSServiceRef> &aReadyServices) const;

        static void HandleBrowseResult(DNSServiceRef       aServiceRef,
                                       DNSServiceFlags     aFlags,
                                       uint32_t            aInterfaceIndex,
                                       DNSServiceErrorType aErrorCode,
                                       const char         *aInstanceName,
                                       const char         *aType,
                                       const char         *aDomain,
                                       void               *aContext);
        void        HandleBrowseResult(DNSServiceRef       aServiceRef,
                                       DNSServiceFlags     aFlags,
                                       uint32_t            aInterfaceIndex,
                                       DNSServiceErrorType aErrorCode,
                                       const char         *aInstanceName,
                                       const char         *aType,
                                       const char         *aDomain);

        std::string mType;
        std::string mInstanceName;

        std::vector<std::unique_ptr<ServiceInstanceResolution>> mResolvingInstances;
    };

    struct HostSubscription : public ServiceRef
    {
        explicit HostSubscription(PublisherMDnsSd &aPublisher, std::string aHostName)
            : ServiceRef(aPublisher)
            , mHostName(std::move(aHostName))
        {
        }

        void        Resolve(void);
        static void HandleResolveResult(DNSServiceRef          aServiceRef,
                                        DNSServiceFlags        aFlags,
                                        uint32_t               aInterfaceIndex,
                                        DNSServiceErrorType    aErrorCode,
                                        const char            *aHostName,
                                        const struct sockaddr *aAddress,
                                        uint32_t               aTtl,
                                        void                  *aContext);
        void        HandleResolveResult(DNSServiceRef          aServiceRef,
                                        DNSServiceFlags        aFlags,
                                        uint32_t               aInterfaceIndex,
                                        DNSServiceErrorType    aErrorCode,
                                        const char            *aHostName,
                                        const struct sockaddr *aAddress,
                                        uint32_t               aTtl);

        std::string        mHostName;
        DiscoveredHostInfo mHostInfo;
    };

    using ServiceSubscriptionList = std::vector<std::unique_ptr<ServiceSubscription>>;
    using HostSubscriptionList    = std::vector<std::unique_ptr<HostSubscription>>;

    static std::string MakeRegType(const std::string &aType, SubTypeList aSubTypeList);

    void                Stop(StopMode aStopMode);
    DNSServiceErrorType CreateSharedHostsRef(void);
    void                DeallocateHostsRef(void);
    void                HandleServiceRefDeallocating(const DNSServiceRef &aServiceRef);

    DNSServiceRef mHostsRef;
    State         mState;
    StateCallback mStateCallback;

    ServiceSubscriptionList mSubscribedServices;
    HostSubscriptionList    mSubscribedHosts;

    std::vector<DNSServiceRef> mServiceRefsToProcess;
};

/**
 * @}
 */

} // namespace Mdns

} // namespace otbr

#endif // OTBR_AGENT_MDNS_MDNSSD_HPP_
