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
 *   This file includes definition for mDNS service based on avahi.
 */

#ifndef OTBR_AGENT_MDNS_AVAHI_HPP_
#define OTBR_AGENT_MDNS_AVAHI_HPP_

#include <vector>

#include <avahi-client/client.h>
#include <avahi-client/lookup.h>
#include <avahi-client/publish.h>
#include <avahi-common/domain.h>
#include <avahi-common/watch.h>

#include "mdns.hpp"
#include "common/mainloop.hpp"
#include "common/time.hpp"

/**
 * @addtogroup border-router-mdns
 *
 * @brief
 *   This module includes definition for avahi-based mDNS service.
 *
 * @{
 */

/**
 * This structure implements AvahiWatch.
 *
 */
struct AvahiWatch
{
    int                mFd;       ///< The file descriptor to watch.
    AvahiWatchEvent    mEvents;   ///< The interested events.
    int                mHappened; ///< The events happened.
    AvahiWatchCallback mCallback; ///< The function to be called when interested events happened on mFd.
    void *             mContext;  ///< A pointer to application-specific context.
    void *             mPoller;   ///< The poller created this watch.

    /**
     * The constructor to initialize an Avahi watch.
     *
     * @param[in]   aFd         The file descriptor to watch.
     * @param[in]   aEvents     The events to watch.
     * @param[in]   aCallback   The function to be called when events happend on this file descriptor.
     * @param[in]   aContext    A pointer to application-specific context.
     * @param[in]   aPoller     The Poller this watcher belongs to.
     *
     */
    AvahiWatch(int aFd, AvahiWatchEvent aEvents, AvahiWatchCallback aCallback, void *aContext, void *aPoller)
        : mFd(aFd)
        , mEvents(aEvents)
        , mCallback(aCallback)
        , mContext(aContext)
        , mPoller(aPoller)
    {
    }
};

/**
 * This structure implements the AvahiTimeout.
 *
 */
struct AvahiTimeout
{
    otbr::Timepoint      mTimeout;  ///< Absolute time when this timer timeout.
    AvahiTimeoutCallback mCallback; ///< The function to be called when timeout.
    void *               mContext;  ///< The pointer to application-specific context.
    void *               mPoller;   ///< The poller created this timer.

    /**
     * The constructor to initialize an AvahiTimeout.
     *
     * @param[in]   aTimeout    A pointer to the time after which the callback should be called.
     * @param[in]   aCallback   The function to be called after timeout.
     * @param[in]   aContext    A pointer to application-specific context.
     * @param[in]   aPoller     The Poller this timeout belongs to.
     *
     */
    AvahiTimeout(const struct timeval *aTimeout, AvahiTimeoutCallback aCallback, void *aContext, void *aPoller);
};

namespace otbr {

namespace Mdns {

/**
 * This class implements the AvahiPoll.
 *
 */
class Poller : public MainloopProcessor
{
public:
    /**
     * The constructor to initialize a Poller.
     *
     */
    Poller(void);

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

    /**
     * This method returns the AvahiPoll.
     *
     * @returns A pointer to the AvahiPoll.
     *
     */
    const AvahiPoll *GetAvahiPoll(void) const { return &mAvahiPoller; }

private:
    typedef std::vector<AvahiWatch *>   Watches;
    typedef std::vector<AvahiTimeout *> Timers;

    static AvahiWatch *    WatchNew(const struct AvahiPoll *aPoller,
                                    int                     aFd,
                                    AvahiWatchEvent         aEvent,
                                    AvahiWatchCallback      aCallback,
                                    void *                  aContext);
    AvahiWatch *           WatchNew(int aFd, AvahiWatchEvent aEvent, AvahiWatchCallback aCallback, void *aContext);
    static void            WatchUpdate(AvahiWatch *aWatch, AvahiWatchEvent aEvent);
    static AvahiWatchEvent WatchGetEvents(AvahiWatch *aWatch);
    static void            WatchFree(AvahiWatch *aWatch);
    void                   WatchFree(AvahiWatch &aWatch);
    static AvahiTimeout *  TimeoutNew(const AvahiPoll *     aPoller,
                                      const struct timeval *aTimeout,
                                      AvahiTimeoutCallback  aCallback,
                                      void *                aContext);
    AvahiTimeout *         TimeoutNew(const struct timeval *aTimeout, AvahiTimeoutCallback aCallback, void *aContext);
    static void            TimeoutUpdate(AvahiTimeout *aTimer, const struct timeval *aTimeout);
    static void            TimeoutFree(AvahiTimeout *aTimer);
    void                   TimeoutFree(AvahiTimeout &aTimer);

    Watches   mWatches;
    Timers    mTimers;
    AvahiPoll mAvahiPoller;
};

/**
 * This class implements mDNS service with avahi.
 *
 */
class PublisherAvahi : public Publisher
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
    PublisherAvahi(int aProtocol, const char *aDomain, StateHandler aHandler, void *aContext);

    ~PublisherAvahi(void) override;

    /**
     * This method publishes or updates a service.
     *
     * @param[in]   aHostName           The name of the host which this service resides on. If NULL is provided,
     *                                  this service resides on local host and it is the implementation to provide
     *                                  specific host name. Otherwise, the caller MUST publish the host with method
     *                                  PublishHost.
     * @param[in]   aPort               The port number of this service.
     * @param[in]   aName               The name of this service.
     * @param[in]   aType               The type of this service.
     * @param[in]   aSubTypeList        A list of service subtypes.
     * @param[in]   aTxtList            A list of TXT name/value pairs.
     *
     * @retval  OTBR_ERROR_NONE     Successfully published or updated the service.
     * @retval  OTBR_ERROR_ERRNO    Failed to publish or update the service.
     *
     */
    otbrError PublishService(const char *       aHostName,
                             uint16_t           aPort,
                             const char *       aName,
                             const char *       aType,
                             const SubTypeList &aSubTypeList,
                             const TxtList &    aTxtList) override;

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
     * This method subscribes a given service or service instance. If @p aInstanceName is not empty, this method
     * subscribes the service instance. Otherwise, this method subscribes the service.
     *
     * mDNS implementations should use the `DiscoveredServiceInstanceCallback` function to notify discovered service
     * instances.
     *
     * @note Discovery Proxy implementation guarantees no duplicate subscriptions for the same service or service
     * instance.
     *
     * @param[in]  aType          The service type.
     * @param[in]  aInstanceName  The service instance to subscribe, or empty to subscribe the service.
     *
     */
    void SubscribeService(const std::string &aType, const std::string &aInstanceName) override;

    /**
     * This method unsubscribes a given service or service instance. If @p aInstanceName is not empty, this method
     * unsubscribes the service instance. Otherwise, this method unsubscribes the service.
     *
     * @note Discovery Proxy implementation guarantees no redundant unsubscription for a service or service instance.
     *
     * @param[in]  aType          The service type.
     * @param[in]  aInstanceName  The service instance to unsubscribe, or empty to unsubscribe the service.
     *
     */
    void UnsubscribeService(const std::string &aType, const std::string &aInstanceName) override;

    /**
     * This method subscribes a given host.
     *
     * mDNS implementations should use the `DiscoveredHostCallback` function to notify discovered hosts.
     *
     * @note Discovery Proxy implementation guarantees no duplicate subscriptions for the same host.
     *
     * @param[in]  aHostName    The host name (without domain).
     *
     */
    void SubscribeHost(const std::string &aHostName) override;

    /**
     * This method unsubscribes a given host.
     *
     * @note Discovery Proxy implementation guarantees no redundant unsubscription for a host.
     *
     * @param[in]  aHostName    The host name (without domain).
     *
     */
    void UnsubscribeHost(const std::string &aHostName) override;

    /**
     * This method starts the mDNS service.
     *
     * @retval OTBR_ERROR_NONE  Successfully started mDNS service;
     * @retval OTBR_ERROR_MDNS  Failed to start mDNS service.
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
     * This method stops the mDNS service.
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
        kMaxSizeOfTxtRecord   = 1024,
        kMaxSizeOfServiceName = AVAHI_LABEL_MAX,
        kMaxSizeOfHost        = AVAHI_LABEL_MAX,
        kMaxSizeOfDomain      = AVAHI_LABEL_MAX,
        kMaxSizeOfServiceType = AVAHI_LABEL_MAX,
        kDefaultTtl           = 10,
    };

    struct Service
    {
        std::string      mName;
        std::string      mType;
        SubTypeList      mSubTypeList;
        std::string      mHostName;
        uint16_t         mPort  = 0;
        AvahiEntryGroup *mGroup = nullptr;
        TxtList          mTxtList;
    };

    typedef std::vector<Service> Services;

    struct Host
    {
        std::string      mHostName;
        AvahiAddress     mAddress = {};
        AvahiEntryGroup *mGroup   = nullptr;
    };

    struct Subscription
    {
        PublisherAvahi *mPublisherAvahi;

        explicit Subscription(PublisherAvahi &aPublisherAvahi)
            : mPublisherAvahi(&aPublisherAvahi)
        {
        }
    };

    struct ServiceSubscription : public Subscription
    {
        explicit ServiceSubscription(PublisherAvahi &aPublisherAvahi, std::string aType, std::string aInstanceName)
            : Subscription(aPublisherAvahi)
            , mType(std::move(aType))
            , mInstanceName(std::move(aInstanceName))
            , mServiceBrowser(nullptr)
            , mServiceResolver(nullptr)
        {
        }

        void Release(void);
        void Browse(void);
        void Resolve(uint32_t      aInterfaceIndex,
                     AvahiProtocol aProtocol,
                     const char *  aInstanceName,
                     const char *  aType,
                     const char *  aDomain);
        void GetAddrInfo(uint32_t aInterfaceIndex);

        static void HandleBrowseResult(AvahiServiceBrowser *  aServiceBrowser,
                                       AvahiIfIndex           aInterfaceIndex,
                                       AvahiProtocol          aProtocol,
                                       AvahiBrowserEvent      aEvent,
                                       const char *           aName,
                                       const char *           aType,
                                       const char *           aDomain,
                                       AvahiLookupResultFlags aFlags,
                                       void *                 aContext);

        void HandleBrowseResult(AvahiServiceBrowser *  aServiceBrowser,
                                AvahiIfIndex           aInterfaceIndex,
                                AvahiProtocol          aProtocol,
                                AvahiBrowserEvent      aEvent,
                                const char *           aName,
                                const char *           aType,
                                const char *           aDomain,
                                AvahiLookupResultFlags aFlags);

        static void HandleResolveResult(AvahiServiceResolver * aServiceResolver,
                                        AvahiIfIndex           aInterfaceIndex,
                                        AvahiProtocol          Protocol,
                                        AvahiResolverEvent     aEvent,
                                        const char *           aName,
                                        const char *           aType,
                                        const char *           aDomain,
                                        const char *           aHostName,
                                        const AvahiAddress *   aAddress,
                                        uint16_t               aPort,
                                        AvahiStringList *      aTxt,
                                        AvahiLookupResultFlags aFlags,
                                        void *                 aContext);

        void HandleResolveResult(AvahiServiceResolver * aServiceResolver,
                                 AvahiIfIndex           aInterfaceIndex,
                                 AvahiProtocol          Protocol,
                                 AvahiResolverEvent     aEvent,
                                 const char *           aName,
                                 const char *           aType,
                                 const char *           aDomain,
                                 const char *           aHostName,
                                 const AvahiAddress *   aAddress,
                                 uint16_t               aPort,
                                 AvahiStringList *      aTxt,
                                 AvahiLookupResultFlags aFlags);

        std::string            mType;
        std::string            mInstanceName;
        DiscoveredInstanceInfo mInstanceInfo;
        AvahiServiceBrowser *  mServiceBrowser;
        AvahiServiceResolver * mServiceResolver;
    };

    struct HostSubscription : public Subscription
    {
        explicit HostSubscription(PublisherAvahi &aMDnsSd, std::string aHostName)
            : Subscription(aMDnsSd)
            , mHostName(std::move(aHostName))
            , mRecordBrowser(nullptr)
        {
        }

        void        Release(void);
        void        Resolve(void);
        static void HandleResolveResult(AvahiRecordBrowser *   aRecordBrowser,
                                        AvahiIfIndex           aInterfaceIndex,
                                        AvahiProtocol          aProtocol,
                                        AvahiBrowserEvent      aEvent,
                                        const char *           aName,
                                        uint16_t               aClazz,
                                        uint16_t               aType,
                                        const void *           aRdata,
                                        size_t                 aSize,
                                        AvahiLookupResultFlags aFlags,
                                        void *                 aContext);

        void HandleResolveResult(AvahiRecordBrowser *   aRecordBrowser,
                                 AvahiIfIndex           aInterfaceIndex,
                                 AvahiProtocol          aProtocol,
                                 AvahiBrowserEvent      aEvent,
                                 const char *           aName,
                                 uint16_t               aClazz,
                                 uint16_t               aType,
                                 const void *           aRdata,
                                 size_t                 aSize,
                                 AvahiLookupResultFlags aFlags);

        std::string         mHostName;
        DiscoveredHostInfo  mHostInfo;
        AvahiRecordBrowser *mRecordBrowser;
    };

    typedef std::vector<Host>                Hosts;
    typedef std::vector<ServiceSubscription> ServiceSubscriptionList;
    typedef std::vector<HostSubscription>    HostSubscriptionList;

    static void HandleClientState(AvahiClient *aClient, AvahiClientState aState, void *aContext);
    void        HandleClientState(AvahiClient *aClient, AvahiClientState aState);

    Hosts::iterator FindHost(const char *aHostName);
    otbrError       CreateHost(AvahiClient &aClient, const char *aHostName, Hosts::iterator &aOutHostIt);

    Services::iterator FindService(const char *aName, const char *aType);
    Services::iterator FindService(AvahiEntryGroup *aGroup);
    otbrError          CreateService(AvahiClient &       aClient,
                                     const char *        aName,
                                     const char *        aType,
                                     Services::iterator &aOutServiceIt);
    static bool        IsServiceOutdated(const Service &    aService,
                                         const char *       aNewHostName,
                                         uint16_t           aNewPort,
                                         const SubTypeList &aNewSubTypeList);

    otbrError        CreateGroup(AvahiClient &aClient, AvahiEntryGroup *&aOutGroup);
    static otbrError ResetGroup(AvahiEntryGroup *aGroup);
    static otbrError FreeGroup(AvahiEntryGroup *aGroup);
    void             FreeAllGroups(void);
    static void      HandleGroupState(AvahiEntryGroup *aGroup, AvahiEntryGroupState aState, void *aContext);
    void             HandleGroupState(AvahiEntryGroup *aGroup, AvahiEntryGroupState aState);
    void             CallHostOrServiceCallback(AvahiEntryGroup *aGroup, otbrError aError) const;
    otbrError        RetryPublishService(const Services::iterator &aServiceIt);

    static otbrError TxtListToAvahiStringList(const TxtList &   aTxtList,
                                              AvahiStringList * aBuffer,
                                              size_t            aBufferSize,
                                              AvahiStringList *&aHead);

    std::string MakeFullName(const char *aName);

    void        OnServiceResolved(ServiceSubscription &aService);
    static void OnServiceResolveFailed(const ServiceSubscription &aService, int aErrorCode);
    void        OnHostResolved(HostSubscription &aHost);
    void        OnHostResolveFailed(const HostSubscription &aHost, int aErrorCode);

    AvahiClient *mClient;
    Hosts        mHosts;
    Services     mServices;
    Poller       mPoller;
    int          mProtocol;
    const char * mDomain;
    State        mState;
    StateHandler mStateHandler;
    void *       mContext;

    ServiceSubscriptionList mSubscribedServices;
    HostSubscriptionList    mSubscribedHosts;
};

} // namespace Mdns

} // namespace otbr

/**
 * @}
 */
#endif // OTBR_AGENT_MDNS_AVAHI_HPP_
