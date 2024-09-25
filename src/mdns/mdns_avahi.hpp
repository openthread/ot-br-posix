/*
 *    Copyright (c) 2017, The OpenThread Authors.
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
 *   This file includes definition for mDNS publisher based on avahi.
 */

#ifndef OTBR_AGENT_MDNS_AVAHI_HPP_
#define OTBR_AGENT_MDNS_AVAHI_HPP_

#include "openthread-br/config.h"

#include <memory>
#include <set>
#include <vector>

#include <avahi-client/client.h>
#include <avahi-client/lookup.h>
#include <avahi-client/publish.h>
#include <avahi-common/domain.h>
#include <avahi-common/watch.h>

#include "mdns.hpp"
#include "common/code_utils.hpp"
#include "common/mainloop.hpp"
#include "common/time.hpp"

/**
 * @addtogroup border-router-mdns
 *
 * @brief
 *   This module includes definition for avahi-based mDNS publisher.
 *
 * @{
 */

namespace otbr {

namespace Mdns {

class AvahiPoller;

/**
 * This class implements mDNS publisher with avahi.
 */
class PublisherAvahi : public Publisher
{
public:
    PublisherAvahi(StateCallback aStateCallback);
    ~PublisherAvahi(void) override;

    void      UnpublishService(const std::string &aName, const std::string &aType, ResultCallback &&aCallback) override;
    void      UnpublishHost(const std::string &aName, ResultCallback &&aCallback) override;
    void      UnpublishKey(const std::string &aName, ResultCallback &&aCallback) override;
    void      SubscribeService(const std::string &aType, const std::string &aInstanceName) override;
    void      UnsubscribeService(const std::string &aType, const std::string &aInstanceName) override;
    void      SubscribeHost(const std::string &aHostName) override;
    void      UnsubscribeHost(const std::string &aHostName) override;
    otbrError Start(void) override;
    bool      IsStarted(void) const override;
    void      Stop(void) override;

protected:
    otbrError PublishServiceImpl(const std::string &aHostName,
                                 const std::string &aName,
                                 const std::string &aType,
                                 const SubTypeList &aSubTypeList,
                                 uint16_t           aPort,
                                 const TxtData     &aTxtData,
                                 ResultCallback   &&aCallback) override;
    otbrError PublishHostImpl(const std::string &aName,
                              const AddressList &aAddresses,
                              ResultCallback   &&aCallback) override;
    otbrError PublishKeyImpl(const std::string &aName, const KeyData &aKeyData, ResultCallback &&aCallback) override;
    void      OnServiceResolveFailedImpl(const std::string &aType,
                                         const std::string &aInstanceName,
                                         int32_t            aErrorCode) override;
    void      OnHostResolveFailedImpl(const std::string &aHostName, int32_t aErrorCode) override;
    otbrError DnsErrorToOtbrError(int32_t aErrorCode) override;

private:
    static constexpr size_t   kMaxSizeOfTxtRecord = 1024;
    static constexpr uint32_t kDefaultTtl         = 10; // In seconds.
    static constexpr uint16_t kDnsKeyRecordType   = 25;

    class AvahiServiceRegistration : public ServiceRegistration
    {
    public:
        AvahiServiceRegistration(const std::string &aHostName,
                                 const std::string &aName,
                                 const std::string &aType,
                                 const SubTypeList &aSubTypeList,
                                 uint16_t           aPort,
                                 const TxtData     &aTxtData,
                                 ResultCallback   &&aCallback,
                                 AvahiEntryGroup   *aEntryGroup,
                                 PublisherAvahi    *aPublisher)
            : ServiceRegistration(aHostName,
                                  aName,
                                  aType,
                                  aSubTypeList,
                                  aPort,
                                  aTxtData,
                                  std::move(aCallback),
                                  aPublisher)
            , mEntryGroup(aEntryGroup)
        {
        }

        ~AvahiServiceRegistration(void) override;
        const AvahiEntryGroup *GetEntryGroup(void) const { return mEntryGroup; }

    private:
        AvahiEntryGroup *mEntryGroup;
    };

    class AvahiHostRegistration : public HostRegistration
    {
    public:
        AvahiHostRegistration(const std::string &aName,
                              const AddressList &aAddresses,
                              ResultCallback   &&aCallback,
                              AvahiEntryGroup   *aEntryGroup,
                              PublisherAvahi    *aPublisher)
            : HostRegistration(aName, aAddresses, std::move(aCallback), aPublisher)
            , mEntryGroup(aEntryGroup)
        {
        }

        ~AvahiHostRegistration(void) override;
        const AvahiEntryGroup *GetEntryGroup(void) const { return mEntryGroup; }

    private:
        AvahiEntryGroup *mEntryGroup;
    };

    class AvahiKeyRegistration : public KeyRegistration
    {
    public:
        AvahiKeyRegistration(const std::string &aName,
                             const KeyData     &aKeyData,
                             ResultCallback   &&aCallback,
                             AvahiEntryGroup   *aEntryGroup,
                             PublisherAvahi    *aPublisher)
            : KeyRegistration(aName, aKeyData, std::move(aCallback), aPublisher)
            , mEntryGroup(aEntryGroup)
        {
        }

        ~AvahiKeyRegistration(void) override;
        const AvahiEntryGroup *GetEntryGroup(void) const { return mEntryGroup; }

    private:
        AvahiEntryGroup *mEntryGroup;
    };

    struct Subscription : private ::NonCopyable
    {
        PublisherAvahi *mPublisherAvahi;

        explicit Subscription(PublisherAvahi &aPublisherAvahi)
            : mPublisherAvahi(&aPublisherAvahi)
        {
        }
    };

    struct HostSubscription : public Subscription
    {
        explicit HostSubscription(PublisherAvahi &aAvahiPublisher, std::string aHostName)
            : Subscription(aAvahiPublisher)
            , mHostName(std::move(aHostName))
            , mRecordBrowser(nullptr)
        {
        }

        ~HostSubscription() { Release(); }

        void        Release(void);
        void        Resolve(void);
        static void HandleResolveResult(AvahiRecordBrowser    *aRecordBrowser,
                                        AvahiIfIndex           aInterfaceIndex,
                                        AvahiProtocol          aProtocol,
                                        AvahiBrowserEvent      aEvent,
                                        const char            *aName,
                                        uint16_t               aClazz,
                                        uint16_t               aType,
                                        const void            *aRdata,
                                        size_t                 aSize,
                                        AvahiLookupResultFlags aFlags,
                                        void                  *aContext);

        void HandleResolveResult(AvahiRecordBrowser    *aRecordBrowser,
                                 AvahiIfIndex           aInterfaceIndex,
                                 AvahiProtocol          aProtocol,
                                 AvahiBrowserEvent      aEvent,
                                 const char            *aName,
                                 uint16_t               aClazz,
                                 uint16_t               aType,
                                 const void            *aRdata,
                                 size_t                 aSize,
                                 AvahiLookupResultFlags aFlags);

        std::string         mHostName;
        DiscoveredHostInfo  mHostInfo;
        AvahiRecordBrowser *mRecordBrowser;
    };

    struct ServiceResolver
    {
        ~ServiceResolver()
        {
            if (mServiceResolver)
            {
                avahi_service_resolver_free(mServiceResolver);
            }
            if (mRecordBrowser)
            {
                avahi_record_browser_free(mRecordBrowser);
            }
        }

        static void HandleResolveServiceResult(AvahiServiceResolver  *aServiceResolver,
                                               AvahiIfIndex           aInterfaceIndex,
                                               AvahiProtocol          Protocol,
                                               AvahiResolverEvent     aEvent,
                                               const char            *aName,
                                               const char            *aType,
                                               const char            *aDomain,
                                               const char            *aHostName,
                                               const AvahiAddress    *aAddress,
                                               uint16_t               aPort,
                                               AvahiStringList       *aTxt,
                                               AvahiLookupResultFlags aFlags,
                                               void                  *aContext);

        void HandleResolveServiceResult(AvahiServiceResolver  *aServiceResolver,
                                        AvahiIfIndex           aInterfaceIndex,
                                        AvahiProtocol          Protocol,
                                        AvahiResolverEvent     aEvent,
                                        const char            *aName,
                                        const char            *aType,
                                        const char            *aDomain,
                                        const char            *aHostName,
                                        const AvahiAddress    *aAddress,
                                        uint16_t               aPort,
                                        AvahiStringList       *aTxt,
                                        AvahiLookupResultFlags aFlags);

        static void HandleResolveHostResult(AvahiRecordBrowser    *aRecordBrowser,
                                            AvahiIfIndex           aInterfaceIndex,
                                            AvahiProtocol          aProtocol,
                                            AvahiBrowserEvent      aEvent,
                                            const char            *aName,
                                            uint16_t               aClazz,
                                            uint16_t               aType,
                                            const void            *aRdata,
                                            size_t                 aSize,
                                            AvahiLookupResultFlags aFlags,
                                            void                  *aContext);

        void HandleResolveHostResult(AvahiRecordBrowser    *aRecordBrowser,
                                     AvahiIfIndex           aInterfaceIndex,
                                     AvahiProtocol          aProtocol,
                                     AvahiBrowserEvent      aEvent,
                                     const char            *aName,
                                     uint16_t               aClazz,
                                     uint16_t               aType,
                                     const void            *aRdata,
                                     size_t                 aSize,
                                     AvahiLookupResultFlags aFlags);

        std::string            mType;
        PublisherAvahi        *mPublisherAvahi;
        AvahiServiceResolver  *mServiceResolver = nullptr;
        AvahiRecordBrowser    *mRecordBrowser   = nullptr;
        DiscoveredInstanceInfo mInstanceInfo;
    };
    struct ServiceSubscription : public Subscription
    {
        explicit ServiceSubscription(PublisherAvahi &aPublisherAvahi, std::string aType, std::string aInstanceName)
            : Subscription(aPublisherAvahi)
            , mType(std::move(aType))
            , mInstanceName(std::move(aInstanceName))
            , mServiceBrowser(nullptr)
        {
        }

        ~ServiceSubscription() { Release(); }

        void Release(void);
        void Browse(void);
        void Resolve(uint32_t           aInterfaceIndex,
                     AvahiProtocol      aProtocol,
                     const std::string &aInstanceName,
                     const std::string &aType);
        void AddServiceResolver(const std::string &aInstanceName, ServiceResolver *aServiceResolver);
        void RemoveServiceResolver(const std::string &aInstanceName);

        static void HandleBrowseResult(AvahiServiceBrowser   *aServiceBrowser,
                                       AvahiIfIndex           aInterfaceIndex,
                                       AvahiProtocol          aProtocol,
                                       AvahiBrowserEvent      aEvent,
                                       const char            *aName,
                                       const char            *aType,
                                       const char            *aDomain,
                                       AvahiLookupResultFlags aFlags,
                                       void                  *aContext);

        void HandleBrowseResult(AvahiServiceBrowser   *aServiceBrowser,
                                AvahiIfIndex           aInterfaceIndex,
                                AvahiProtocol          aProtocol,
                                AvahiBrowserEvent      aEvent,
                                const char            *aName,
                                const char            *aType,
                                const char            *aDomain,
                                AvahiLookupResultFlags aFlags);

        std::string          mType;
        std::string          mInstanceName;
        AvahiServiceBrowser *mServiceBrowser;

        using ServiceResolversMap = std::map<std::string, std::set<ServiceResolver *>>;
        ServiceResolversMap mServiceResolvers;
    };

    typedef std::vector<std::unique_ptr<ServiceSubscription>> ServiceSubscriptionList;
    typedef std::vector<std::unique_ptr<HostSubscription>>    HostSubscriptionList;

    static void HandleClientState(AvahiClient *aClient, AvahiClientState aState, void *aContext);
    void        HandleClientState(AvahiClient *aClient, AvahiClientState aState);

    AvahiEntryGroup *CreateGroup(AvahiClient *aClient);
    static void      ReleaseGroup(AvahiEntryGroup *aGroup);

    static void HandleGroupState(AvahiEntryGroup *aGroup, AvahiEntryGroupState aState, void *aContext);
    void        HandleGroupState(AvahiEntryGroup *aGroup, AvahiEntryGroupState aState);
    void        CallHostOrServiceCallback(AvahiEntryGroup *aGroup, otbrError aError);

    static otbrError TxtDataToAvahiStringList(const TxtData    &aTxtData,
                                              AvahiStringList  *aBuffer,
                                              size_t            aBufferSize,
                                              AvahiStringList *&aHead);

    ServiceRegistration *FindServiceRegistration(const AvahiEntryGroup *aEntryGroup);
    HostRegistration    *FindHostRegistration(const AvahiEntryGroup *aEntryGroup);
    KeyRegistration     *FindKeyRegistration(const AvahiEntryGroup *aEntryGroup);

    AvahiClient                 *mClient;
    std::unique_ptr<AvahiPoller> mPoller;
    State                        mState;
    StateCallback                mStateCallback;

    ServiceSubscriptionList mSubscribedServices;
    HostSubscriptionList    mSubscribedHosts;
};

} // namespace Mdns

} // namespace otbr

/**
 * @}
 */
#endif // OTBR_AGENT_MDNS_AVAHI_HPP_
