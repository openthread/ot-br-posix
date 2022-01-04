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
 * This class implements mDNS publisher with mDNSResponder.
 *
 */
class PublisherMDnsSd : public MainloopProcessor, public Publisher
{
public:
    /**
     * The constructor to initialize a Publisher.
     *
     * @param[in] aHandler  The function to be called when state changes.
     * @param[in] aContext  A pointer to application-specific context.
     *
     */
    PublisherMDnsSd(StateHandler aHandler, void *aContext);

    ~PublisherMDnsSd(void) override;

    // Implementation of Mdns::Publisher.

    otbrError PublishService(const std::string &aHostName,
                             uint16_t           aPort,
                             const std::string &aName,
                             const std::string &aType,
                             const SubTypeList &aSubTypeList,
                             const TxtList &    aTxtList) override;
    otbrError UnpublishService(const std::string &aName, const std::string &aType) override;
    otbrError PublishHost(const std::string &aName, const std::vector<uint8_t> &aAddress) override;
    otbrError UnpublishHost(const std::string &aName) override;
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
    static constexpr uint32_t kDefaultTtl = 10;

    struct Service
    {
        std::string   mName;
        std::string   mType;
        std::string   mRegType; ///< Service type with optional subtypes separated by commas
        DNSServiceRef mService;
        uint16_t      mPort;
    };

    struct Host
    {
        std::string          mName;
        std::vector<uint8_t> mAddress;
        DNSRecordRef         mRecord;
    };

    struct ServiceRef : private ::NonCopyable
    {
        DNSServiceRef mServiceRef;

        explicit ServiceRef(void)
            : mServiceRef(nullptr)
        {
        }

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
            : ServiceRef()
            , mSubscription(&aSubscription)
            , mInstanceName(std::move(aInstanceName))
            , mTypeEndWithDot(std::move(aType))
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

        ServiceSubscription *  mSubscription;
        std::string            mInstanceName;
        std::string            mTypeEndWithDot;
        std::string            mDomain;
        uint32_t               mNetifIndex;
        DiscoveredInstanceInfo mInstanceInfo;
    };

    struct ServiceSubscription : public ServiceRef
    {
        explicit ServiceSubscription(PublisherMDnsSd &aMDnsSd, std::string aType, std::string aInstanceName)
            : ServiceRef()
            , mMDnsSd(&aMDnsSd)
            , mType(std::move(aType))
            , mInstanceName(std::move(aInstanceName))
        {
        }

        void Browse(void);
        void Resolve(uint32_t           aNetifIndex,
                     const std::string &aInstanceName,
                     const std::string &aType,
                     const std::string &aDomain);
        void RemoveInstanceResolution(ServiceInstanceResolution &aInstanceResolution);
        void UpdateAll(MainloopContext &aMainloop) const;
        void ProcessAll(const MainloopContext &aMainloop, std::vector<DNSServiceRef> &aReadyServices) const;

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

        PublisherMDnsSd *mMDnsSd;
        std::string      mType;
        std::string      mInstanceName;

        std::vector<std::unique_ptr<ServiceInstanceResolution>> mResolvingInstances;
    };

    struct HostSubscription : public ServiceRef
    {
        explicit HostSubscription(PublisherMDnsSd &aMDnsSd, std::string aHostName)
            : ServiceRef()
            , mMDnsSd(&aMDnsSd)
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

        PublisherMDnsSd *  mMDnsSd;
        std::string        mHostName;
        DiscoveredHostInfo mHostInfo;
    };

    typedef std::vector<Service>                              Services;
    typedef std::vector<Host>                                 Hosts;
    typedef std::vector<Service>::iterator                    ServiceIterator;
    typedef std::vector<Host>::iterator                       HostIterator;
    typedef std::vector<std::unique_ptr<ServiceSubscription>> ServiceSubscriptionList;
    typedef std::vector<std::unique_ptr<HostSubscription>>    HostSubscriptionList;

    void        DiscardService(const std::string &aName, const std::string &aType, DNSServiceRef aServiceRef = nullptr);
    void        RecordService(const std::string &aName,
                              const std::string &aType,
                              const std::string &aRegType,
                              uint16_t           aPort,
                              DNSServiceRef      aServiceRef);
    static bool IsServiceOutdated(const Service &service, const std::string &aNewRegType, int aNewPort);

    otbrError DiscardHost(const std::string &aName, bool aSendGoodbye = true);
    void      RecordHost(const std::string &aName, const std::vector<uint8_t> &aAddress, DNSRecordRef aRecordRef);

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

    static std::string MakeFullName(const std::string &aName);
    static std::string MakeRegType(const std::string &aType, const SubTypeList &aSubTypeList);

    ServiceIterator FindPublishedService(const std::string &aName, const std::string &aType);
    ServiceIterator FindPublishedService(const DNSServiceRef &aServiceRef);
    HostIterator    FindPublishedHost(const DNSRecordRef &aRecordRef);
    HostIterator    FindPublishedHost(const std::string &aHostName);

    static void OnServiceResolveFailed(const std::string & aType,
                                       const std::string & aInstanceName,
                                       DNSServiceErrorType aErrorCode);
    void        OnHostResolveFailed(const HostSubscription &aHost, DNSServiceErrorType aErrorCode);

    Services      mServices;
    Hosts         mHosts;
    DNSServiceRef mHostsRef;
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
