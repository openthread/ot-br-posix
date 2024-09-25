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
 *   This file implements mDNS publisher based on avahi.
 */

#define OTBR_LOG_TAG "MDNS"

#include "mdns/mdns_avahi.hpp"

#include <algorithm>

#include <avahi-client/client.h>
#include <avahi-common/alternative.h>
#include <avahi-common/error.h>
#include <avahi-common/malloc.h>
#include <avahi-common/timeval.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include "common/code_utils.hpp"
#include "common/logging.hpp"
#include "common/time.hpp"

namespace otbr {
namespace Mdns {

class AvahiPoller;

} // namespace Mdns
} // namespace otbr

struct AvahiWatch
{
    typedef otbr::Mdns::AvahiPoller AvahiPoller;

    int                mFd;           ///< The file descriptor to watch.
    AvahiWatchEvent    mEvents;       ///< The interested events.
    int                mHappened;     ///< The events happened.
    AvahiWatchCallback mCallback;     ///< The function to be called to report events happened on `mFd`.
    void              *mContext;      ///< A pointer to application-specific context to use with `mCallback`.
    bool               mShouldReport; ///< Whether or not we need to report events (invoking callback).
    AvahiPoller       &mPoller;       ///< The poller owning this watch.

    /**
     * The constructor to initialize an Avahi watch.
     *
     * @param[in] aFd        The file descriptor to watch.
     * @param[in] aEvents    The events to watch.
     * @param[in] aCallback  The function to be called when events happened on this file descriptor.
     * @param[in] aContext   A pointer to application-specific context.
     * @param[in] aPoller    The AvahiPoller this watcher belongs to.
     */
    AvahiWatch(int aFd, AvahiWatchEvent aEvents, AvahiWatchCallback aCallback, void *aContext, AvahiPoller &aPoller)
        : mFd(aFd)
        , mEvents(aEvents)
        , mCallback(aCallback)
        , mContext(aContext)
        , mShouldReport(false)
        , mPoller(aPoller)
    {
    }
};

/**
 * This structure implements the AvahiTimeout.
 */
struct AvahiTimeout
{
    typedef otbr::Mdns::AvahiPoller AvahiPoller;

    otbr::Timepoint      mTimeout;      ///< Absolute time when this timer timeout.
    AvahiTimeoutCallback mCallback;     ///< The function to be called when timeout.
    void                *mContext;      ///< The pointer to application-specific context.
    bool                 mShouldReport; ///< Whether or not timeout occurred and need to reported (invoking callback).
    AvahiPoller         &mPoller;       ///< The poller created this timer.

    /**
     * The constructor to initialize an AvahiTimeout.
     *
     * @param[in] aTimeout   A pointer to the time after which the callback should be called.
     * @param[in] aCallback  The function to be called after timeout.
     * @param[in] aContext   A pointer to application-specific context.
     * @param[in] aPoller    The AvahiPoller this timeout belongs to.
     */
    AvahiTimeout(const struct timeval *aTimeout, AvahiTimeoutCallback aCallback, void *aContext, AvahiPoller &aPoller)
        : mCallback(aCallback)
        , mContext(aContext)
        , mShouldReport(false)
        , mPoller(aPoller)
    {
        if (aTimeout)
        {
            mTimeout = otbr::Clock::now() + otbr::FromTimeval<otbr::Microseconds>(*aTimeout);
        }
        else
        {
            mTimeout = otbr::Timepoint::min();
        }
    }
};

namespace otbr {

namespace Mdns {

static otbrError DnsErrorToOtbrError(int aAvahiError)
{
    otbrError error;

    switch (aAvahiError)
    {
    case AVAHI_OK:
    case AVAHI_ERR_INVALID_ADDRESS:
        error = OTBR_ERROR_NONE;
        break;

    case AVAHI_ERR_NOT_FOUND:
        error = OTBR_ERROR_NOT_FOUND;
        break;

    case AVAHI_ERR_INVALID_ARGUMENT:
        error = OTBR_ERROR_INVALID_ARGS;
        break;

    case AVAHI_ERR_COLLISION:
        error = OTBR_ERROR_DUPLICATED;
        break;

    case AVAHI_ERR_DNS_NOTIMP:
    case AVAHI_ERR_NOT_SUPPORTED:
        error = OTBR_ERROR_NOT_IMPLEMENTED;
        break;

    default:
        error = OTBR_ERROR_MDNS;
        break;
    }

    return error;
}

class AvahiPoller : public MainloopProcessor
{
public:
    AvahiPoller(void);

    // Implementation of MainloopProcessor.

    void Update(MainloopContext &aMainloop) override;
    void Process(const MainloopContext &aMainloop) override;

    const AvahiPoll *GetAvahiPoll(void) const { return &mAvahiPoll; }

private:
    typedef std::vector<AvahiWatch *>   Watches;
    typedef std::vector<AvahiTimeout *> Timers;

    static AvahiWatch     *WatchNew(const struct AvahiPoll *aPoll,
                                    int                     aFd,
                                    AvahiWatchEvent         aEvent,
                                    AvahiWatchCallback      aCallback,
                                    void                   *aContext);
    AvahiWatch            *WatchNew(int aFd, AvahiWatchEvent aEvent, AvahiWatchCallback aCallback, void *aContext);
    static void            WatchUpdate(AvahiWatch *aWatch, AvahiWatchEvent aEvent);
    static AvahiWatchEvent WatchGetEvents(AvahiWatch *aWatch);
    static void            WatchFree(AvahiWatch *aWatch);
    void                   WatchFree(AvahiWatch &aWatch);
    static AvahiTimeout   *TimeoutNew(const AvahiPoll      *aPoll,
                                      const struct timeval *aTimeout,
                                      AvahiTimeoutCallback  aCallback,
                                      void                 *aContext);
    AvahiTimeout          *TimeoutNew(const struct timeval *aTimeout, AvahiTimeoutCallback aCallback, void *aContext);
    static void            TimeoutUpdate(AvahiTimeout *aTimer, const struct timeval *aTimeout);
    static void            TimeoutFree(AvahiTimeout *aTimer);
    void                   TimeoutFree(AvahiTimeout &aTimer);

    Watches   mWatches;
    Timers    mTimers;
    AvahiPoll mAvahiPoll;
};

AvahiPoller::AvahiPoller(void)
{
    mAvahiPoll.userdata         = this;
    mAvahiPoll.watch_new        = WatchNew;
    mAvahiPoll.watch_update     = WatchUpdate;
    mAvahiPoll.watch_get_events = WatchGetEvents;
    mAvahiPoll.watch_free       = WatchFree;

    mAvahiPoll.timeout_new    = TimeoutNew;
    mAvahiPoll.timeout_update = TimeoutUpdate;
    mAvahiPoll.timeout_free   = TimeoutFree;
}

AvahiWatch *AvahiPoller::WatchNew(const struct AvahiPoll *aPoll,
                                  int                     aFd,
                                  AvahiWatchEvent         aEvent,
                                  AvahiWatchCallback      aCallback,
                                  void                   *aContext)
{
    return reinterpret_cast<AvahiPoller *>(aPoll->userdata)->WatchNew(aFd, aEvent, aCallback, aContext);
}

AvahiWatch *AvahiPoller::WatchNew(int aFd, AvahiWatchEvent aEvent, AvahiWatchCallback aCallback, void *aContext)
{
    assert(aEvent && aCallback && aFd >= 0);

    mWatches.push_back(new AvahiWatch(aFd, aEvent, aCallback, aContext, *this));

    return mWatches.back();
}

void AvahiPoller::WatchUpdate(AvahiWatch *aWatch, AvahiWatchEvent aEvent)
{
    aWatch->mEvents = aEvent;
}

AvahiWatchEvent AvahiPoller::WatchGetEvents(AvahiWatch *aWatch)
{
    return static_cast<AvahiWatchEvent>(aWatch->mHappened);
}

void AvahiPoller::WatchFree(AvahiWatch *aWatch)
{
    aWatch->mPoller.WatchFree(*aWatch);
}

void AvahiPoller::WatchFree(AvahiWatch &aWatch)
{
    for (Watches::iterator it = mWatches.begin(); it != mWatches.end(); ++it)
    {
        if (*it == &aWatch)
        {
            mWatches.erase(it);
            delete &aWatch;
            break;
        }
    }
}

AvahiTimeout *AvahiPoller::TimeoutNew(const AvahiPoll      *aPoll,
                                      const struct timeval *aTimeout,
                                      AvahiTimeoutCallback  aCallback,
                                      void                 *aContext)
{
    assert(aPoll && aCallback);
    return static_cast<AvahiPoller *>(aPoll->userdata)->TimeoutNew(aTimeout, aCallback, aContext);
}

AvahiTimeout *AvahiPoller::TimeoutNew(const struct timeval *aTimeout, AvahiTimeoutCallback aCallback, void *aContext)
{
    mTimers.push_back(new AvahiTimeout(aTimeout, aCallback, aContext, *this));
    return mTimers.back();
}

void AvahiPoller::TimeoutUpdate(AvahiTimeout *aTimer, const struct timeval *aTimeout)
{
    if (aTimeout == nullptr)
    {
        aTimer->mTimeout = Timepoint::min();
    }
    else
    {
        aTimer->mTimeout = Clock::now() + FromTimeval<Microseconds>(*aTimeout);
    }
}

void AvahiPoller::TimeoutFree(AvahiTimeout *aTimer)
{
    aTimer->mPoller.TimeoutFree(*aTimer);
}

void AvahiPoller::TimeoutFree(AvahiTimeout &aTimer)
{
    for (Timers::iterator it = mTimers.begin(); it != mTimers.end(); ++it)
    {
        if (*it == &aTimer)
        {
            mTimers.erase(it);
            delete &aTimer;
            break;
        }
    }
}

void AvahiPoller::Update(MainloopContext &aMainloop)
{
    Timepoint now = Clock::now();

    for (AvahiWatch *watch : mWatches)
    {
        int             fd     = watch->mFd;
        AvahiWatchEvent events = watch->mEvents;

        if (AVAHI_WATCH_IN & events)
        {
            FD_SET(fd, &aMainloop.mReadFdSet);
        }

        if (AVAHI_WATCH_OUT & events)
        {
            FD_SET(fd, &aMainloop.mWriteFdSet);
        }

        if (AVAHI_WATCH_ERR & events)
        {
            FD_SET(fd, &aMainloop.mErrorFdSet);
        }

        if (AVAHI_WATCH_HUP & events)
        {
            // TODO what do with this event type?
        }

        aMainloop.mMaxFd = std::max(aMainloop.mMaxFd, fd);

        watch->mHappened = 0;
    }

    for (AvahiTimeout *timer : mTimers)
    {
        Timepoint timeout = timer->mTimeout;

        if (timeout == Timepoint::min())
        {
            continue;
        }

        if (timeout <= now)
        {
            aMainloop.mTimeout = ToTimeval(Microseconds::zero());
            break;
        }
        else
        {
            auto delay = std::chrono::duration_cast<Microseconds>(timeout - now);

            if (delay < FromTimeval<Microseconds>(aMainloop.mTimeout))
            {
                aMainloop.mTimeout = ToTimeval(delay);
            }
        }
    }
}

void AvahiPoller::Process(const MainloopContext &aMainloop)
{
    Timepoint now          = Clock::now();
    bool      shouldReport = false;

    for (AvahiWatch *watch : mWatches)
    {
        int             fd     = watch->mFd;
        AvahiWatchEvent events = watch->mEvents;

        watch->mHappened = 0;

        if ((AVAHI_WATCH_IN & events) && FD_ISSET(fd, &aMainloop.mReadFdSet))
        {
            watch->mHappened |= AVAHI_WATCH_IN;
        }

        if ((AVAHI_WATCH_OUT & events) && FD_ISSET(fd, &aMainloop.mWriteFdSet))
        {
            watch->mHappened |= AVAHI_WATCH_OUT;
        }

        if ((AVAHI_WATCH_ERR & events) && FD_ISSET(fd, &aMainloop.mErrorFdSet))
        {
            watch->mHappened |= AVAHI_WATCH_ERR;
        }

        if (watch->mHappened != 0)
        {
            watch->mShouldReport = true;
            shouldReport         = true;
        }
    }

    // When we invoke the callback for an `AvahiWatch` or `AvahiTimeout`,
    // the Avahi module can call any of `mAvahiPoll` APIs we provided to
    // it. For example, it can update or free any of `AvahiWatch/Timeout`
    // entries, which in turn, modifies our `mWatches` or `mTimers` list.
    // So, before invoking the callback, we update the entry's state and
    // then restart the iteration over the `mWacthes` list to find the
    // next entry to report, as the list may have changed.

    while (shouldReport)
    {
        shouldReport = false;

        for (AvahiWatch *watch : mWatches)
        {
            if (watch->mShouldReport)
            {
                shouldReport         = true;
                watch->mShouldReport = false;
                watch->mCallback(watch, watch->mFd, WatchGetEvents(watch), watch->mContext);

                break;
            }
        }
    }

    for (AvahiTimeout *timer : mTimers)
    {
        if (timer->mTimeout == Timepoint::min())
        {
            continue;
        }

        if (timer->mTimeout <= now)
        {
            timer->mShouldReport = true;
            shouldReport         = true;
        }
    }

    while (shouldReport)
    {
        shouldReport = false;

        for (AvahiTimeout *timer : mTimers)
        {
            if (timer->mShouldReport)
            {
                shouldReport         = true;
                timer->mShouldReport = false;
                timer->mCallback(timer, timer->mContext);

                break;
            }
        }
    }
}

PublisherAvahi::PublisherAvahi(StateCallback aStateCallback)
    : mClient(nullptr)
    , mPoller(MakeUnique<AvahiPoller>())
    , mState(State::kIdle)
    , mStateCallback(std::move(aStateCallback))
{
}

PublisherAvahi::~PublisherAvahi(void)
{
    Stop();
}

PublisherAvahi::AvahiServiceRegistration::~AvahiServiceRegistration(void)
{
    ReleaseGroup(mEntryGroup);
}

PublisherAvahi::AvahiHostRegistration::~AvahiHostRegistration(void)
{
    ReleaseGroup(mEntryGroup);
}

PublisherAvahi::AvahiKeyRegistration::~AvahiKeyRegistration(void)
{
    ReleaseGroup(mEntryGroup);
}

otbrError PublisherAvahi::Start(void)
{
    otbrError error      = OTBR_ERROR_NONE;
    int       avahiError = AVAHI_OK;

    assert(mClient == nullptr);

    mClient = avahi_client_new(mPoller->GetAvahiPoll(), AVAHI_CLIENT_NO_FAIL, HandleClientState, this, &avahiError);

    if (avahiError != AVAHI_OK)
    {
        otbrLogErr("Failed to create avahi client: %s!", avahi_strerror(avahiError));
        error = OTBR_ERROR_MDNS;
    }

    return error;
}

bool PublisherAvahi::IsStarted(void) const
{
    return mClient != nullptr;
}

void PublisherAvahi::Stop(void)
{
    mServiceRegistrations.clear();
    mHostRegistrations.clear();

    mSubscribedServices.clear();
    mSubscribedHosts.clear();

    if (mClient)
    {
        avahi_client_free(mClient);
        mClient = nullptr;
    }

    mState = Mdns::Publisher::State::kIdle;
}

void PublisherAvahi::HandleClientState(AvahiClient *aClient, AvahiClientState aState, void *aContext)
{
    static_cast<PublisherAvahi *>(aContext)->HandleClientState(aClient, aState);
}

void PublisherAvahi::HandleGroupState(AvahiEntryGroup *aGroup, AvahiEntryGroupState aState, void *aContext)
{
    static_cast<PublisherAvahi *>(aContext)->HandleGroupState(aGroup, aState);
}

void PublisherAvahi::HandleGroupState(AvahiEntryGroup *aGroup, AvahiEntryGroupState aState)
{
    switch (aState)
    {
    case AVAHI_ENTRY_GROUP_ESTABLISHED:
        otbrLogInfo("Avahi group (@%p) is established", aGroup);
        CallHostOrServiceCallback(aGroup, OTBR_ERROR_NONE);
        break;

    case AVAHI_ENTRY_GROUP_COLLISION:
        otbrLogInfo("Avahi group (@%p) name conflicted", aGroup);
        CallHostOrServiceCallback(aGroup, OTBR_ERROR_DUPLICATED);
        break;

    case AVAHI_ENTRY_GROUP_FAILURE:
        otbrLogErr("Avahi group (@%p) failed: %s!", aGroup,
                   avahi_strerror(avahi_client_errno(avahi_entry_group_get_client(aGroup))));
        CallHostOrServiceCallback(aGroup, OTBR_ERROR_MDNS);
        break;

    case AVAHI_ENTRY_GROUP_UNCOMMITED:
    case AVAHI_ENTRY_GROUP_REGISTERING:
        break;
    }
}

void PublisherAvahi::CallHostOrServiceCallback(AvahiEntryGroup *aGroup, otbrError aError)
{
    ServiceRegistration *serviceReg;
    HostRegistration    *hostReg;
    KeyRegistration     *keyReg;

    if ((serviceReg = FindServiceRegistration(aGroup)) != nullptr)
    {
        if (aError == OTBR_ERROR_NONE)
        {
            serviceReg->Complete(aError);
        }
        else
        {
            RemoveServiceRegistration(serviceReg->mName, serviceReg->mType, aError);
        }
    }
    else if ((hostReg = FindHostRegistration(aGroup)) != nullptr)
    {
        if (aError == OTBR_ERROR_NONE)
        {
            hostReg->Complete(aError);
        }
        else
        {
            RemoveHostRegistration(hostReg->mName, aError);
        }
    }
    else if ((keyReg = FindKeyRegistration(aGroup)) != nullptr)
    {
        if (aError == OTBR_ERROR_NONE)
        {
            keyReg->Complete(aError);
        }
        else
        {
            RemoveKeyRegistration(keyReg->mName, aError);
        }
    }
    else
    {
        otbrLogWarning("No registered service or host matches avahi group @%p", aGroup);
    }
}

AvahiEntryGroup *PublisherAvahi::CreateGroup(AvahiClient *aClient)
{
    AvahiEntryGroup *group = avahi_entry_group_new(aClient, HandleGroupState, this);

    if (group == nullptr)
    {
        otbrLogErr("Failed to create entry avahi group: %s", avahi_strerror(avahi_client_errno(aClient)));
    }

    return group;
}

void PublisherAvahi::ReleaseGroup(AvahiEntryGroup *aGroup)
{
    int error;

    otbrLogInfo("Releasing avahi entry group @%p", aGroup);

    error = avahi_entry_group_reset(aGroup);

    if (error != 0)
    {
        otbrLogErr("Failed to reset entry group for avahi error: %s", avahi_strerror(error));
    }

    error = avahi_entry_group_free(aGroup);
    if (error != 0)
    {
        otbrLogErr("Failed to free entry group for avahi error: %s", avahi_strerror(error));
    }
}

void PublisherAvahi::HandleClientState(AvahiClient *aClient, AvahiClientState aState)
{
    otbrLogInfo("Avahi client state changed to %d", aState);

    switch (aState)
    {
    case AVAHI_CLIENT_S_RUNNING:
        // The server has startup successfully and registered its host
        // name on the network, so it's time to create our services.
        otbrLogInfo("Avahi client is ready");
        mClient = aClient;
        mState  = State::kReady;
        mStateCallback(mState);
        break;

    case AVAHI_CLIENT_FAILURE:
        otbrLogErr("Avahi client failed to start: %s", avahi_strerror(avahi_client_errno(aClient)));
        mState = State::kIdle;
        mStateCallback(mState);
        Stop();
        Start();
        break;

    case AVAHI_CLIENT_S_COLLISION:
        // Let's drop our registered services. When the server is back
        // in AVAHI_SERVER_RUNNING state we will register them again
        // with the new host name.
        otbrLogErr("Avahi client collision detected: %s", avahi_strerror(avahi_client_errno(aClient)));

        // fall through

    case AVAHI_CLIENT_S_REGISTERING:
        // The server records are now being established. This might be
        // caused by a host name change. We need to wait for our own
        // records to register until the host name is properly established.
        mServiceRegistrations.clear();
        mHostRegistrations.clear();
        break;

    case AVAHI_CLIENT_CONNECTING:
        otbrLogInfo("Avahi client is connecting to the server");
        break;
    }
}

otbrError PublisherAvahi::PublishServiceImpl(const std::string &aHostName,
                                             const std::string &aName,
                                             const std::string &aType,
                                             const SubTypeList &aSubTypeList,
                                             uint16_t           aPort,
                                             const TxtData     &aTxtData,
                                             ResultCallback   &&aCallback)
{
    otbrError         error             = OTBR_ERROR_NONE;
    int               avahiError        = AVAHI_OK;
    SubTypeList       sortedSubTypeList = SortSubTypeList(aSubTypeList);
    const std::string logHostName       = !aHostName.empty() ? aHostName : "localhost";
    std::string       fullHostName;
    std::string       serviceName = aName;
    AvahiEntryGroup  *group       = nullptr;

    // Aligned with AvahiStringList
    AvahiStringList  txtBuffer[(kMaxSizeOfTxtRecord - 1) / sizeof(AvahiStringList) + 1];
    AvahiStringList *txtHead = nullptr;

    VerifyOrExit(mState == State::kReady, error = OTBR_ERROR_INVALID_STATE);
    VerifyOrExit(mClient != nullptr, error = OTBR_ERROR_INVALID_STATE);

    if (!aHostName.empty())
    {
        fullHostName = MakeFullHostName(aHostName);
    }
    if (serviceName.empty())
    {
        serviceName = avahi_client_get_host_name(mClient);
    }

    aCallback = HandleDuplicateServiceRegistration(aHostName, serviceName, aType, sortedSubTypeList, aPort, aTxtData,
                                                   std::move(aCallback));
    VerifyOrExit(!aCallback.IsNull());

    SuccessOrExit(error = TxtDataToAvahiStringList(aTxtData, txtBuffer, sizeof(txtBuffer), txtHead));
    VerifyOrExit((group = CreateGroup(mClient)) != nullptr, error = OTBR_ERROR_MDNS);
    avahiError = avahi_entry_group_add_service_strlst(group, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, AvahiPublishFlags{},
                                                      serviceName.c_str(), aType.c_str(),
                                                      /* domain */ nullptr, fullHostName.c_str(), aPort, txtHead);
    VerifyOrExit(avahiError == AVAHI_OK);

    for (const std::string &subType : aSubTypeList)
    {
        otbrLogInfo("Add subtype %s for service %s.%s", subType.c_str(), serviceName.c_str(), aType.c_str());
        std::string fullSubType = subType + "._sub." + aType;
        avahiError              = avahi_entry_group_add_service_subtype(group, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC,
                                                                        AvahiPublishFlags{}, serviceName.c_str(), aType.c_str(),
                                                                        /* domain */ nullptr, fullSubType.c_str());
        VerifyOrExit(avahiError == AVAHI_OK);
    }

    otbrLogInfo("Commit avahi service %s.%s", serviceName.c_str(), aType.c_str());
    avahiError = avahi_entry_group_commit(group);
    VerifyOrExit(avahiError == AVAHI_OK);

    AddServiceRegistration(std::unique_ptr<AvahiServiceRegistration>(new AvahiServiceRegistration(
        aHostName, serviceName, aType, sortedSubTypeList, aPort, aTxtData, std::move(aCallback), group, this)));

exit:
    if (avahiError != AVAHI_OK || error != OTBR_ERROR_NONE)
    {
        if (avahiError != AVAHI_OK)
        {
            error = OTBR_ERROR_MDNS;
            otbrLogErr("Failed to publish service for avahi error: %s!", avahi_strerror(avahiError));
        }

        if (group != nullptr)
        {
            ReleaseGroup(group);
        }
        std::move(aCallback)(error);
    }
    return error;
}

void PublisherAvahi::UnpublishService(const std::string &aName, const std::string &aType, ResultCallback &&aCallback)
{
    otbrError error = OTBR_ERROR_NONE;

    VerifyOrExit(mState == Publisher::State::kReady, error = OTBR_ERROR_INVALID_STATE);
    RemoveServiceRegistration(aName, aType, OTBR_ERROR_ABORTED);

exit:
    std::move(aCallback)(error);
}

otbrError PublisherAvahi::PublishHostImpl(const std::string &aName,
                                          const AddressList &aAddresses,
                                          ResultCallback   &&aCallback)
{
    otbrError        error      = OTBR_ERROR_NONE;
    int              avahiError = AVAHI_OK;
    std::string      fullHostName;
    AvahiEntryGroup *group = nullptr;

    VerifyOrExit(mState == State::kReady, error = OTBR_ERROR_INVALID_STATE);
    VerifyOrExit(mClient != nullptr, error = OTBR_ERROR_INVALID_STATE);

    aCallback = HandleDuplicateHostRegistration(aName, aAddresses, std::move(aCallback));
    VerifyOrExit(!aCallback.IsNull());
    VerifyOrExit(!aAddresses.empty(), std::move(aCallback)(OTBR_ERROR_NONE));

    VerifyOrExit((group = CreateGroup(mClient)) != nullptr, error = OTBR_ERROR_MDNS);

    fullHostName = MakeFullHostName(aName);
    for (const auto &address : aAddresses)
    {
        AvahiAddress avahiAddress;

        avahiAddress.proto = AVAHI_PROTO_INET6;
        memcpy(avahiAddress.data.ipv6.address, address.m8, sizeof(address.m8));
        avahiError = avahi_entry_group_add_address(group, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, AVAHI_PUBLISH_NO_REVERSE,
                                                   fullHostName.c_str(), &avahiAddress);
        VerifyOrExit(avahiError == AVAHI_OK);
    }

    otbrLogInfo("Commit avahi host %s", aName.c_str());
    avahiError = avahi_entry_group_commit(group);
    VerifyOrExit(avahiError == AVAHI_OK);

    AddHostRegistration(std::unique_ptr<AvahiHostRegistration>(
        new AvahiHostRegistration(aName, aAddresses, std::move(aCallback), group, this)));

exit:
    if (avahiError != AVAHI_OK || error != OTBR_ERROR_NONE)
    {
        if (avahiError != AVAHI_OK)
        {
            error = OTBR_ERROR_MDNS;
            otbrLogErr("Failed to publish host for avahi error: %s!", avahi_strerror(avahiError));
        }

        if (group != nullptr)
        {
            ReleaseGroup(group);
        }
        std::move(aCallback)(error);
    }
    return error;
}

void PublisherAvahi::UnpublishHost(const std::string &aName, ResultCallback &&aCallback)
{
    otbrError error = OTBR_ERROR_NONE;

    VerifyOrExit(mState == Publisher::State::kReady, error = OTBR_ERROR_INVALID_STATE);
    RemoveHostRegistration(aName, OTBR_ERROR_ABORTED);

exit:
    std::move(aCallback)(error);
}

otbrError PublisherAvahi::PublishKeyImpl(const std::string &aName, const KeyData &aKeyData, ResultCallback &&aCallback)
{
    otbrError        error      = OTBR_ERROR_NONE;
    int              avahiError = AVAHI_OK;
    std::string      fullKeyName;
    AvahiEntryGroup *group = nullptr;

    VerifyOrExit(mState == State::kReady, error = OTBR_ERROR_INVALID_STATE);
    VerifyOrExit(mClient != nullptr, error = OTBR_ERROR_INVALID_STATE);

    aCallback = HandleDuplicateKeyRegistration(aName, aKeyData, std::move(aCallback));
    VerifyOrExit(!aCallback.IsNull());

    VerifyOrExit((group = CreateGroup(mClient)) != nullptr, error = OTBR_ERROR_MDNS);

    fullKeyName = MakeFullKeyName(aName);

    avahiError = avahi_entry_group_add_record(group, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, AVAHI_PUBLISH_UNIQUE,
                                              fullKeyName.c_str(), AVAHI_DNS_CLASS_IN, kDnsKeyRecordType, kDefaultTtl,
                                              aKeyData.data(), aKeyData.size());
    VerifyOrExit(avahiError == AVAHI_OK);

    otbrLogInfo("Commit avahi key record for %s", aName.c_str());
    avahiError = avahi_entry_group_commit(group);
    VerifyOrExit(avahiError == AVAHI_OK);

    AddKeyRegistration(std::unique_ptr<AvahiKeyRegistration>(
        new AvahiKeyRegistration(aName, aKeyData, std::move(aCallback), group, this)));

exit:
    if (avahiError != AVAHI_OK || error != OTBR_ERROR_NONE)
    {
        if (avahiError != AVAHI_OK)
        {
            error = OTBR_ERROR_MDNS;
            otbrLogErr("Failed to publish key record - avahi error: %s!", avahi_strerror(avahiError));
        }

        if (group != nullptr)
        {
            ReleaseGroup(group);
        }
        std::move(aCallback)(error);
    }
    return error;
}

void PublisherAvahi::UnpublishKey(const std::string &aName, ResultCallback &&aCallback)
{
    otbrError error = OTBR_ERROR_NONE;

    VerifyOrExit(mState == Publisher::State::kReady, error = OTBR_ERROR_INVALID_STATE);
    RemoveKeyRegistration(aName, OTBR_ERROR_ABORTED);

exit:
    std::move(aCallback)(error);
}

otbrError PublisherAvahi::TxtDataToAvahiStringList(const TxtData    &aTxtData,
                                                   AvahiStringList  *aBuffer,
                                                   size_t            aBufferSize,
                                                   AvahiStringList *&aHead)
{
    otbrError        error = OTBR_ERROR_NONE;
    size_t           used  = 0;
    AvahiStringList *last  = nullptr;
    AvahiStringList *curr  = aBuffer;
    const uint8_t   *next;
    const uint8_t   *data    = aTxtData.data();
    const uint8_t   *dataEnd = aTxtData.data() + aTxtData.size();

    aHead = nullptr;

    while (data < dataEnd)
    {
        uint8_t entryLength = *data++;
        size_t  needed      = sizeof(AvahiStringList) - sizeof(AvahiStringList::text) + entryLength;

        if (entryLength == 0)
        {
            continue;
        }

        VerifyOrExit(data + entryLength <= dataEnd, error = OTBR_ERROR_PARSE);

        VerifyOrExit(used + needed <= aBufferSize, error = OTBR_ERROR_INVALID_ARGS);
        curr->next = last;
        last       = curr;

        memcpy(curr->text, data, entryLength);
        curr->size = entryLength;

        data += entryLength;

        next = curr->text + curr->size;
        curr = OTBR_ALIGNED(next, AvahiStringList *);
        used = static_cast<size_t>(reinterpret_cast<uint8_t *>(curr) - reinterpret_cast<uint8_t *>(aBuffer));
    }

    aHead = last;

exit:
    return error;
}

Publisher::ServiceRegistration *PublisherAvahi::FindServiceRegistration(const AvahiEntryGroup *aEntryGroup)
{
    ServiceRegistration *result = nullptr;

    for (const auto &kv : mServiceRegistrations)
    {
        const auto &serviceReg = static_cast<const AvahiServiceRegistration &>(*kv.second);
        if (serviceReg.GetEntryGroup() == aEntryGroup)
        {
            result = kv.second.get();
            break;
        }
    }

    return result;
}

Publisher::HostRegistration *PublisherAvahi::FindHostRegistration(const AvahiEntryGroup *aEntryGroup)
{
    HostRegistration *result = nullptr;

    for (const auto &kv : mHostRegistrations)
    {
        const auto &hostReg = static_cast<const AvahiHostRegistration &>(*kv.second);
        if (hostReg.GetEntryGroup() == aEntryGroup)
        {
            result = kv.second.get();
            break;
        }
    }

    return result;
}

Publisher::KeyRegistration *PublisherAvahi::FindKeyRegistration(const AvahiEntryGroup *aEntryGroup)
{
    KeyRegistration *result = nullptr;

    for (const auto &entry : mKeyRegistrations)
    {
        const auto &keyReg = static_cast<const AvahiKeyRegistration &>(*entry.second);
        if (keyReg.GetEntryGroup() == aEntryGroup)
        {
            result = entry.second.get();
            break;
        }
    }

    return result;
}

void PublisherAvahi::SubscribeService(const std::string &aType, const std::string &aInstanceName)
{
    auto service = MakeUnique<ServiceSubscription>(*this, aType, aInstanceName);

    VerifyOrExit(mState == Publisher::State::kReady);
    mSubscribedServices.push_back(std::move(service));

    otbrLogInfo("Subscribe service %s.%s (total %zu)", aInstanceName.c_str(), aType.c_str(),
                mSubscribedServices.size());

    if (aInstanceName.empty())
    {
        mSubscribedServices.back()->Browse();
    }
    else
    {
        mSubscribedServices.back()->Resolve(AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, aInstanceName, aType);
    }

exit:
    return;
}

void PublisherAvahi::UnsubscribeService(const std::string &aType, const std::string &aInstanceName)
{
    ServiceSubscriptionList::iterator it;

    VerifyOrExit(mState == Publisher::State::kReady);
    it = std::find_if(mSubscribedServices.begin(), mSubscribedServices.end(),
                      [&aType, &aInstanceName](const std::unique_ptr<ServiceSubscription> &aService) {
                          return aService->mType == aType && aService->mInstanceName == aInstanceName;
                      });

    VerifyOrExit(it != mSubscribedServices.end());

    {
        std::unique_ptr<ServiceSubscription> service = std::move(*it);

        mSubscribedServices.erase(it);
        service->Release();
    }

    otbrLogInfo("Unsubscribe service %s.%s (left %zu)", aInstanceName.c_str(), aType.c_str(),
                mSubscribedServices.size());

exit:
    return;
}

void PublisherAvahi::OnServiceResolveFailedImpl(const std::string &aType,
                                                const std::string &aInstanceName,
                                                int32_t            aErrorCode)
{
    otbrLogWarning("Resolve service %s.%s failed: %s", aInstanceName.c_str(), aType.c_str(),
                   avahi_strerror(aErrorCode));
}

void PublisherAvahi::OnHostResolveFailedImpl(const std::string &aHostName, int32_t aErrorCode)
{
    otbrLogWarning("Resolve host %s failed: %s", aHostName.c_str(), avahi_strerror(aErrorCode));
}

otbrError PublisherAvahi::DnsErrorToOtbrError(int32_t aErrorCode)
{
    return otbr::Mdns::DnsErrorToOtbrError(aErrorCode);
}

void PublisherAvahi::SubscribeHost(const std::string &aHostName)
{
    auto host = MakeUnique<HostSubscription>(*this, aHostName);

    VerifyOrExit(mState == Publisher::State::kReady);

    mSubscribedHosts.push_back(std::move(host));

    otbrLogInfo("Subscribe host %s (total %zu)", aHostName.c_str(), mSubscribedHosts.size());

    mSubscribedHosts.back()->Resolve();

exit:
    return;
}

void PublisherAvahi::UnsubscribeHost(const std::string &aHostName)
{
    HostSubscriptionList::iterator it;

    VerifyOrExit(mState == Publisher::State::kReady);
    it = std::find_if(
        mSubscribedHosts.begin(), mSubscribedHosts.end(),
        [&aHostName](const std::unique_ptr<HostSubscription> &aHost) { return aHost->mHostName == aHostName; });

    VerifyOrExit(it != mSubscribedHosts.end());

    {
        std::unique_ptr<HostSubscription> host = std::move(*it);

        mSubscribedHosts.erase(it);
        host->Release();
    }

    otbrLogInfo("Unsubscribe host %s (remaining %zu)", aHostName.c_str(), mSubscribedHosts.size());

exit:
    return;
}

Publisher *Publisher::Create(StateCallback aStateCallback)
{
    return new PublisherAvahi(std::move(aStateCallback));
}

void Publisher::Destroy(Publisher *aPublisher)
{
    delete static_cast<PublisherAvahi *>(aPublisher);
}

void PublisherAvahi::ServiceSubscription::Browse(void)
{
    assert(mPublisherAvahi->mClient != nullptr);

    otbrLogInfo("Browse service %s", mType.c_str());
    mServiceBrowser =
        avahi_service_browser_new(mPublisherAvahi->mClient, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, mType.c_str(),
                                  /* domain */ nullptr, static_cast<AvahiLookupFlags>(0), HandleBrowseResult, this);
    if (!mServiceBrowser)
    {
        otbrLogWarning("Failed to browse service %s: %s", mType.c_str(),
                       avahi_strerror(avahi_client_errno(mPublisherAvahi->mClient)));
    }
}

void PublisherAvahi::ServiceSubscription::Release(void)
{
    std::vector<std::string> instanceNames;

    for (const auto &resolvers : mServiceResolvers)
    {
        instanceNames.push_back(resolvers.first);
    }
    for (const auto &name : instanceNames)
    {
        RemoveServiceResolver(name);
    }

    if (mServiceBrowser != nullptr)
    {
        avahi_service_browser_free(mServiceBrowser);
        mServiceBrowser = nullptr;
    }
}

void PublisherAvahi::ServiceSubscription::HandleBrowseResult(AvahiServiceBrowser   *aServiceBrowser,
                                                             AvahiIfIndex           aInterfaceIndex,
                                                             AvahiProtocol          aProtocol,
                                                             AvahiBrowserEvent      aEvent,
                                                             const char            *aName,
                                                             const char            *aType,
                                                             const char            *aDomain,
                                                             AvahiLookupResultFlags aFlags,
                                                             void                  *aContext)
{
    static_cast<PublisherAvahi::ServiceSubscription *>(aContext)->HandleBrowseResult(
        aServiceBrowser, aInterfaceIndex, aProtocol, aEvent, aName, aType, aDomain, aFlags);
}

void PublisherAvahi::ServiceSubscription::HandleBrowseResult(AvahiServiceBrowser   *aServiceBrowser,
                                                             AvahiIfIndex           aInterfaceIndex,
                                                             AvahiProtocol          aProtocol,
                                                             AvahiBrowserEvent      aEvent,
                                                             const char            *aName,
                                                             const char            *aType,
                                                             const char            *aDomain,
                                                             AvahiLookupResultFlags aFlags)
{
    OTBR_UNUSED_VARIABLE(aServiceBrowser);
    OTBR_UNUSED_VARIABLE(aProtocol);
    OTBR_UNUSED_VARIABLE(aDomain);

    assert(mServiceBrowser == aServiceBrowser);

    otbrLogInfo("Browse service reply: %s.%s proto %d inf %u event %d flags %d", aName, aType, aProtocol,
                aInterfaceIndex, static_cast<int>(aEvent), static_cast<int>(aFlags));

    switch (aEvent)
    {
    case AVAHI_BROWSER_NEW:
        Resolve(aInterfaceIndex, aProtocol, aName, aType);
        break;
    case AVAHI_BROWSER_REMOVE:
        mPublisherAvahi->OnServiceRemoved(static_cast<uint32_t>(aInterfaceIndex), aType, aName);
        RemoveServiceResolver(aName);
        break;
    case AVAHI_BROWSER_CACHE_EXHAUSTED:
    case AVAHI_BROWSER_ALL_FOR_NOW:
        // do nothing
        break;
    case AVAHI_BROWSER_FAILURE:
        mPublisherAvahi->OnServiceResolveFailed(aType, aName, avahi_client_errno(mPublisherAvahi->mClient));
        break;
    }
}

void PublisherAvahi::ServiceSubscription::Resolve(uint32_t           aInterfaceIndex,
                                                  AvahiProtocol      aProtocol,
                                                  const std::string &aInstanceName,
                                                  const std::string &aType)
{
    auto serviceResolver = MakeUnique<ServiceResolver>();

    mPublisherAvahi->mServiceInstanceResolutionBeginTime[std::make_pair(aInstanceName, aType)] = Clock::now();

    otbrLogInfo("Resolve service %s.%s inf %" PRIu32, aInstanceName.c_str(), aType.c_str(), aInterfaceIndex);

    serviceResolver->mType            = aType;
    serviceResolver->mPublisherAvahi  = this->mPublisherAvahi;
    serviceResolver->mServiceResolver = avahi_service_resolver_new(
        mPublisherAvahi->mClient, aInterfaceIndex, aProtocol, aInstanceName.c_str(), aType.c_str(),
        /* domain */ nullptr, AVAHI_PROTO_UNSPEC, static_cast<AvahiLookupFlags>(AVAHI_LOOKUP_NO_ADDRESS),
        &ServiceResolver::HandleResolveServiceResult, serviceResolver.get());

    if (serviceResolver->mServiceResolver != nullptr)
    {
        AddServiceResolver(aInstanceName, serviceResolver.release());
    }
    else
    {
        otbrLogErr("Failed to resolve serivce %s: %s", mType.c_str(),
                   avahi_strerror(avahi_client_errno(mPublisherAvahi->mClient)));
    }
}

void PublisherAvahi::ServiceResolver::HandleResolveServiceResult(AvahiServiceResolver  *aServiceResolver,
                                                                 AvahiIfIndex           aInterfaceIndex,
                                                                 AvahiProtocol          aProtocol,
                                                                 AvahiResolverEvent     aEvent,
                                                                 const char            *aName,
                                                                 const char            *aType,
                                                                 const char            *aDomain,
                                                                 const char            *aHostName,
                                                                 const AvahiAddress    *aAddress,
                                                                 uint16_t               aPort,
                                                                 AvahiStringList       *aTxt,
                                                                 AvahiLookupResultFlags aFlags,
                                                                 void                  *aContext)
{
    static_cast<PublisherAvahi::ServiceResolver *>(aContext)->HandleResolveServiceResult(
        aServiceResolver, aInterfaceIndex, aProtocol, aEvent, aName, aType, aDomain, aHostName, aAddress, aPort, aTxt,
        aFlags);
}

void PublisherAvahi::ServiceResolver::HandleResolveServiceResult(AvahiServiceResolver  *aServiceResolver,
                                                                 AvahiIfIndex           aInterfaceIndex,
                                                                 AvahiProtocol          aProtocol,
                                                                 AvahiResolverEvent     aEvent,
                                                                 const char            *aName,
                                                                 const char            *aType,
                                                                 const char            *aDomain,
                                                                 const char            *aHostName,
                                                                 const AvahiAddress    *aAddress,
                                                                 uint16_t               aPort,
                                                                 AvahiStringList       *aTxt,
                                                                 AvahiLookupResultFlags aFlags)
{
    OT_UNUSED_VARIABLE(aServiceResolver);
    OT_UNUSED_VARIABLE(aInterfaceIndex);
    OT_UNUSED_VARIABLE(aProtocol);
    OT_UNUSED_VARIABLE(aType);
    OT_UNUSED_VARIABLE(aDomain);
    OT_UNUSED_VARIABLE(aAddress);

    size_t totalTxtSize = 0;
    bool   resolved     = false;
    int    avahiError   = AVAHI_OK;

    otbrLog(aEvent == AVAHI_RESOLVER_FOUND ? OTBR_LOG_INFO : OTBR_LOG_WARNING, OTBR_LOG_TAG,
            "Resolve service reply: protocol %d %s.%s.%s = host %s port %" PRIu16 " flags %d event %d", aProtocol,
            aName, aType, aDomain, aHostName, aPort, static_cast<int>(aFlags), static_cast<int>(aEvent));

    VerifyOrExit(aEvent == AVAHI_RESOLVER_FOUND, avahiError = avahi_client_errno(mPublisherAvahi->mClient));
    VerifyOrExit(aHostName != nullptr, avahiError = AVAHI_ERR_INVALID_HOST_NAME);

    mInstanceInfo.mNetifIndex = static_cast<uint32_t>(aInterfaceIndex);
    mInstanceInfo.mName       = aName;
    mInstanceInfo.mHostName   = std::string(aHostName) + ".";
    mInstanceInfo.mPort       = aPort;

    otbrLogInfo("Resolve service reply: flags=%u, host=%s", aFlags, aHostName);

    // TODO priority
    // TODO weight
    // TODO use a more proper TTL
    mInstanceInfo.mTtl = kDefaultTtl;
    for (auto p = aTxt; p; p = avahi_string_list_get_next(p))
    {
        totalTxtSize += avahi_string_list_get_size(p) + 1;
    }
    mInstanceInfo.mTxtData.resize(totalTxtSize);
    avahi_string_list_serialize(aTxt, mInstanceInfo.mTxtData.data(), totalTxtSize);

    // NOTE: Avahi only returns one of the host's addresses in the service resolution callback. However, the address may
    // be link-local so it may not be preferred from Thread's perspective. We want to go through the complete list of
    // addresses associated with the host and choose a routable address. Therefore, as below we will resolve the host
    // and go through all its addresses.

    resolved = true;

exit:
    if (resolved)
    {
        // In case the callback is triggered when a service instance is updated, there may already be a record browser.
        // We should free it before switching to the new record browser.
        if (mRecordBrowser)
        {
            avahi_record_browser_free(mRecordBrowser);
            mRecordBrowser = nullptr;
            mInstanceInfo.mAddresses.clear();
        }
        // NOTE: This `ServiceResolver` object may be freed in `OnServiceResolved`.
        mRecordBrowser = avahi_record_browser_new(mPublisherAvahi->mClient, aInterfaceIndex, AVAHI_PROTO_UNSPEC,
                                                  aHostName, AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_AAAA,
                                                  static_cast<AvahiLookupFlags>(0), HandleResolveHostResult, this);
        if (!mRecordBrowser)
        {
            resolved   = false;
            avahiError = avahi_client_errno(mPublisherAvahi->mClient);
        }
    }
    if (!resolved && avahiError != AVAHI_OK)
    {
        mPublisherAvahi->OnServiceResolveFailed(aType, aName, avahiError);
    }
}

void PublisherAvahi::ServiceResolver::HandleResolveHostResult(AvahiRecordBrowser    *aRecordBrowser,
                                                              AvahiIfIndex           aInterfaceIndex,
                                                              AvahiProtocol          aProtocol,
                                                              AvahiBrowserEvent      aEvent,
                                                              const char            *aName,
                                                              uint16_t               aClazz,
                                                              uint16_t               aType,
                                                              const void            *aRdata,
                                                              size_t                 aSize,
                                                              AvahiLookupResultFlags aFlags,
                                                              void                  *aContext)
{
    static_cast<PublisherAvahi::ServiceResolver *>(aContext)->HandleResolveHostResult(
        aRecordBrowser, aInterfaceIndex, aProtocol, aEvent, aName, aClazz, aType, aRdata, aSize, aFlags);
}

void PublisherAvahi::ServiceResolver::HandleResolveHostResult(AvahiRecordBrowser    *aRecordBrowser,
                                                              AvahiIfIndex           aInterfaceIndex,
                                                              AvahiProtocol          aProtocol,
                                                              AvahiBrowserEvent      aEvent,
                                                              const char            *aName,
                                                              uint16_t               aClazz,
                                                              uint16_t               aType,
                                                              const void            *aRdata,
                                                              size_t                 aSize,
                                                              AvahiLookupResultFlags aFlags)
{
    OTBR_UNUSED_VARIABLE(aRecordBrowser);
    OTBR_UNUSED_VARIABLE(aInterfaceIndex);
    OTBR_UNUSED_VARIABLE(aProtocol);
    OTBR_UNUSED_VARIABLE(aEvent);
    OTBR_UNUSED_VARIABLE(aClazz);
    OTBR_UNUSED_VARIABLE(aType);
    OTBR_UNUSED_VARIABLE(aFlags);

    Ip6Address address;
    bool       resolved   = false;
    int        avahiError = AVAHI_OK;

    otbrLog(aEvent != AVAHI_BROWSER_FAILURE ? OTBR_LOG_INFO : OTBR_LOG_WARNING, OTBR_LOG_TAG,
            "Resolve host reply: %s inf %d protocol %d class %" PRIu16 " type %" PRIu16 " size %zu flags %d event %d",
            aName, aInterfaceIndex, aProtocol, aClazz, aType, aSize, static_cast<int>(aFlags),
            static_cast<int>(aEvent));

    VerifyOrExit(aEvent == AVAHI_BROWSER_NEW || aEvent == AVAHI_BROWSER_REMOVE);
    VerifyOrExit(aSize == OTBR_IP6_ADDRESS_SIZE || aSize == OTBR_IP4_ADDRESS_SIZE,
                 otbrLogErr("Unexpected address data length: %zu", aSize), avahiError = AVAHI_ERR_INVALID_ADDRESS);
    VerifyOrExit(aSize == OTBR_IP6_ADDRESS_SIZE, otbrLogInfo("IPv4 address ignored"),
                 avahiError = AVAHI_ERR_INVALID_ADDRESS);
    address = Ip6Address(*static_cast<const uint8_t(*)[OTBR_IP6_ADDRESS_SIZE]>(aRdata));

    VerifyOrExit(!address.IsLinkLocal() && !address.IsMulticast() && !address.IsLoopback() && !address.IsUnspecified(),
                 avahiError = AVAHI_ERR_INVALID_ADDRESS);
    otbrLogInfo("Resolved host address: %s %s", aEvent == AVAHI_BROWSER_NEW ? "add" : "remove",
                address.ToString().c_str());
    if (aEvent == AVAHI_BROWSER_NEW)
    {
        mInstanceInfo.AddAddress(address);
    }
    else
    {
        mInstanceInfo.RemoveAddress(address);
    }
    resolved = true;

exit:
    if (resolved)
    {
        // NOTE: This `HostSubscrption` object may be freed in `OnHostResolved`.
        mPublisherAvahi->OnServiceResolved(mType, mInstanceInfo);
    }
    else if (avahiError != AVAHI_OK)
    {
        mPublisherAvahi->OnServiceResolveFailed(mType, mInstanceInfo.mName, avahiError);
    }
}

void PublisherAvahi::ServiceSubscription::AddServiceResolver(const std::string &aInstanceName,
                                                             ServiceResolver   *aServiceResolver)
{
    assert(aServiceResolver != nullptr);
    mServiceResolvers[aInstanceName].insert(aServiceResolver);

    otbrLogDebug("Added service resolver for instance %s", aInstanceName.c_str());
}

void PublisherAvahi::ServiceSubscription::RemoveServiceResolver(const std::string &aInstanceName)
{
    int numResolvers = 0;

    VerifyOrExit(mServiceResolvers.find(aInstanceName) != mServiceResolvers.end());

    numResolvers = mServiceResolvers[aInstanceName].size();

    for (auto resolver : mServiceResolvers[aInstanceName])
    {
        delete resolver;
    }

    mServiceResolvers.erase(aInstanceName);

exit:
    otbrLogDebug("Removed %d service resolver for instance %s", numResolvers, aInstanceName.c_str());
    return;
}

void PublisherAvahi::HostSubscription::Release(void)
{
    if (mRecordBrowser != nullptr)
    {
        avahi_record_browser_free(mRecordBrowser);
        mRecordBrowser = nullptr;
    }
}

void PublisherAvahi::HostSubscription::Resolve(void)
{
    std::string fullHostName = MakeFullHostName(mHostName);

    mPublisherAvahi->mHostResolutionBeginTime[mHostName] = Clock::now();

    otbrLogInfo("Resolve host %s inf %d", fullHostName.c_str(), static_cast<int>(AVAHI_IF_UNSPEC));
    mRecordBrowser = avahi_record_browser_new(mPublisherAvahi->mClient, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC,
                                              fullHostName.c_str(), AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_AAAA,
                                              static_cast<AvahiLookupFlags>(0), HandleResolveResult, this);
    if (!mRecordBrowser)
    {
        otbrLogErr("Failed to resolve host %s: %s", fullHostName.c_str(),
                   avahi_strerror(avahi_client_errno(mPublisherAvahi->mClient)));
    }
}

void PublisherAvahi::HostSubscription::HandleResolveResult(AvahiRecordBrowser    *aRecordBrowser,
                                                           AvahiIfIndex           aInterfaceIndex,
                                                           AvahiProtocol          aProtocol,
                                                           AvahiBrowserEvent      aEvent,
                                                           const char            *aName,
                                                           uint16_t               aClazz,
                                                           uint16_t               aType,
                                                           const void            *aRdata,
                                                           size_t                 aSize,
                                                           AvahiLookupResultFlags aFlags,
                                                           void                  *aContext)
{
    static_cast<PublisherAvahi::HostSubscription *>(aContext)->HandleResolveResult(
        aRecordBrowser, aInterfaceIndex, aProtocol, aEvent, aName, aClazz, aType, aRdata, aSize, aFlags);
}

void PublisherAvahi::HostSubscription::HandleResolveResult(AvahiRecordBrowser    *aRecordBrowser,
                                                           AvahiIfIndex           aInterfaceIndex,
                                                           AvahiProtocol          aProtocol,
                                                           AvahiBrowserEvent      aEvent,
                                                           const char            *aName,
                                                           uint16_t               aClazz,
                                                           uint16_t               aType,
                                                           const void            *aRdata,
                                                           size_t                 aSize,
                                                           AvahiLookupResultFlags aFlags)
{
    OTBR_UNUSED_VARIABLE(aRecordBrowser);
    OTBR_UNUSED_VARIABLE(aProtocol);
    OTBR_UNUSED_VARIABLE(aEvent);
    OTBR_UNUSED_VARIABLE(aClazz);
    OTBR_UNUSED_VARIABLE(aType);
    OTBR_UNUSED_VARIABLE(aFlags);

    Ip6Address address;
    bool       resolved   = false;
    int        avahiError = AVAHI_OK;

    otbrLog(aEvent != AVAHI_BROWSER_FAILURE ? OTBR_LOG_INFO : OTBR_LOG_WARNING, OTBR_LOG_TAG,
            "Resolve host reply: %s inf %d protocol %d class %" PRIu16 " type %" PRIu16 " size %zu flags %d event %d",
            aName, aInterfaceIndex, aProtocol, aClazz, aType, aSize, static_cast<int>(aFlags),
            static_cast<int>(aEvent));

    VerifyOrExit(aEvent == AVAHI_BROWSER_NEW || aEvent == AVAHI_BROWSER_REMOVE);
    VerifyOrExit(aSize == OTBR_IP6_ADDRESS_SIZE || aSize == OTBR_IP4_ADDRESS_SIZE,
                 otbrLogErr("Unexpected address data length: %zu", aSize), avahiError = AVAHI_ERR_INVALID_ADDRESS);
    VerifyOrExit(aSize == OTBR_IP6_ADDRESS_SIZE, otbrLogInfo("IPv4 address ignored"),
                 avahiError = AVAHI_ERR_INVALID_ADDRESS);
    address = Ip6Address(*static_cast<const uint8_t(*)[OTBR_IP6_ADDRESS_SIZE]>(aRdata));

    VerifyOrExit(!address.IsLinkLocal() && !address.IsMulticast() && !address.IsLoopback() && !address.IsUnspecified(),
                 avahiError = AVAHI_ERR_INVALID_ADDRESS);
    otbrLogInfo("Resolved host address: %s %s", aEvent == AVAHI_BROWSER_NEW ? "add" : "remove",
                address.ToString().c_str());

    mHostInfo.mHostName = std::string(aName) + ".";
    if (aEvent == AVAHI_BROWSER_NEW)
    {
        mHostInfo.AddAddress(address);
    }
    else
    {
        mHostInfo.RemoveAddress(address);
    }
    mHostInfo.mNetifIndex = static_cast<uint32_t>(aInterfaceIndex);
    // TODO: Use a more proper TTL
    mHostInfo.mTtl = kDefaultTtl;
    resolved       = true;

exit:
    if (resolved)
    {
        // NOTE: This `HostSubscrption` object may be freed in `OnHostResolved`.
        mPublisherAvahi->OnHostResolved(mHostName, mHostInfo);
    }
    else if (avahiError != AVAHI_OK)
    {
        mPublisherAvahi->OnHostResolveFailed(mHostName, avahiError);
    }
}

} // namespace Mdns

} // namespace otbr
