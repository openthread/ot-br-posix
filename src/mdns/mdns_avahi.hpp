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
 *   This module includes definition for avahi-based mDNS publisher.
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
     * @param[in] aFd        The file descriptor to watch.
     * @param[in] aEvents    The events to watch.
     * @param[in] aCallback  The function to be called when events happend on this file descriptor.
     * @param[in] aContext   A pointer to application-specific context.
     * @param[in] aPoller    The Poller this watcher belongs to.
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
     * @param[in] aTimeout   A pointer to the time after which the callback should be called.
     * @param[in] aCallback  The function to be called after timeout.
     * @param[in] aContext   A pointer to application-specific context.
     * @param[in] aPoller    The Poller this timeout belongs to.
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

    // Implementation of MainloopProcessor.

    void Update(MainloopContext &aMainloop) override;
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
 * This class implements mDNS publisher with avahi.
 *
 */
class PublisherAvahi : public Publisher
{
public:
    /**
     * The constructor to initialize a Publisher.
     *
     * @param[in] aHandler  The function to be called when state changes.
     * @param[in] aContext  A pointer to application-specific context.
     *
     */
    PublisherAvahi(StateHandler aHandler, void *aContext);

    ~PublisherAvahi(void) override;

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

private:
    static constexpr size_t   kMaxSizeOfTxtRecord = 1024;
    static constexpr uint32_t kDefaultTtl         = 10; // In seconds.

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
        void Resolve(uint32_t           aInterfaceIndex,
                     AvahiProtocol      aProtocol,
                     const std::string &aInstanceName,
                     const std::string &aType);
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

    Hosts::iterator FindHost(const std::string &aHostName);
    otbrError       CreateHost(AvahiClient &aClient, const std::string &aHostName, Hosts::iterator &aOutHostIt);

    Services::iterator FindService(const std::string &aName, const std::string &aType);
    Services::iterator FindService(AvahiEntryGroup *aGroup);
    otbrError          CreateService(AvahiClient &       aClient,
                                     const std::string & aName,
                                     const std::string & aType,
                                     Services::iterator &aOutServiceIt);
    static bool        IsServiceOutdated(const Service &    aService,
                                         const std::string &aNewHostName,
                                         uint16_t           aNewPort,
                                         const SubTypeList &aNewSubTypeList);

    otbrError        CreateGroup(AvahiClient &aClient, AvahiEntryGroup *&aOutGroup);
    static otbrError ResetGroup(AvahiEntryGroup *aGroup);
    static otbrError FreeGroup(AvahiEntryGroup *aGroup);
    void             FreeAllGroups(void);
    static void      HandleGroupState(AvahiEntryGroup *aGroup, AvahiEntryGroupState aState, void *aContext);
    void             HandleGroupState(AvahiEntryGroup *aGroup, AvahiEntryGroupState aState);
    void             CallHostOrServiceCallback(AvahiEntryGroup *aGroup, otbrError aError) const;

    static otbrError TxtListToAvahiStringList(const TxtList &   aTxtList,
                                              AvahiStringList * aBuffer,
                                              size_t            aBufferSize,
                                              AvahiStringList *&aHead);

    static std::string MakeFullName(const std::string &aName);

    void        OnServiceResolved(ServiceSubscription &aService);
    static void OnServiceResolveFailed(const ServiceSubscription &aService, int aErrorCode);
    void        OnHostResolved(HostSubscription &aHost);
    void        OnHostResolveFailed(const HostSubscription &aHost, int aErrorCode);

    AvahiClient *mClient;
    Hosts        mHosts;
    Services     mServices;
    Poller       mPoller;
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
