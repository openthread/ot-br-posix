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
 *   This file implements mDNS service based on avahi.
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include "common/code_utils.hpp"
#include "common/logging.hpp"
#include "common/time.hpp"
#include "utils/strcpy_utils.hpp"

AvahiTimeout::AvahiTimeout(const struct timeval *aTimeout,
                           AvahiTimeoutCallback  aCallback,
                           void *                aContext,
                           void *                aPoller)
    : mCallback(aCallback)
    , mContext(aContext)
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

namespace otbr {

namespace Mdns {

Poller::Poller(void)
{
    mAvahiPoller.userdata         = this;
    mAvahiPoller.watch_new        = WatchNew;
    mAvahiPoller.watch_update     = WatchUpdate;
    mAvahiPoller.watch_get_events = WatchGetEvents;
    mAvahiPoller.watch_free       = WatchFree;

    mAvahiPoller.timeout_new    = TimeoutNew;
    mAvahiPoller.timeout_update = TimeoutUpdate;
    mAvahiPoller.timeout_free   = TimeoutFree;
}

AvahiWatch *Poller::WatchNew(const struct AvahiPoll *aPoller,
                             int                     aFd,
                             AvahiWatchEvent         aEvent,
                             AvahiWatchCallback      aCallback,
                             void *                  aContext)
{
    return reinterpret_cast<Poller *>(aPoller->userdata)->WatchNew(aFd, aEvent, aCallback, aContext);
}

AvahiWatch *Poller::WatchNew(int aFd, AvahiWatchEvent aEvent, AvahiWatchCallback aCallback, void *aContext)
{
    assert(aEvent && aCallback && aFd >= 0);

    mWatches.push_back(new AvahiWatch(aFd, aEvent, aCallback, aContext, this));

    return mWatches.back();
}

void Poller::WatchUpdate(AvahiWatch *aWatch, AvahiWatchEvent aEvent)
{
    aWatch->mEvents = aEvent;
}

AvahiWatchEvent Poller::WatchGetEvents(AvahiWatch *aWatch)
{
    return static_cast<AvahiWatchEvent>(aWatch->mHappened);
}

void Poller::WatchFree(AvahiWatch *aWatch)
{
    reinterpret_cast<Poller *>(aWatch->mPoller)->WatchFree(*aWatch);
}

void Poller::WatchFree(AvahiWatch &aWatch)
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

AvahiTimeout *Poller::TimeoutNew(const AvahiPoll *     aPoller,
                                 const struct timeval *aTimeout,
                                 AvahiTimeoutCallback  aCallback,
                                 void *                aContext)
{
    assert(aPoller && aCallback);
    return static_cast<Poller *>(aPoller->userdata)->TimeoutNew(aTimeout, aCallback, aContext);
}

AvahiTimeout *Poller::TimeoutNew(const struct timeval *aTimeout, AvahiTimeoutCallback aCallback, void *aContext)
{
    mTimers.push_back(new AvahiTimeout(aTimeout, aCallback, aContext, this));
    return mTimers.back();
}

void Poller::TimeoutUpdate(AvahiTimeout *aTimer, const struct timeval *aTimeout)
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

void Poller::TimeoutFree(AvahiTimeout *aTimer)
{
    static_cast<Poller *>(aTimer->mPoller)->TimeoutFree(*aTimer);
}

void Poller::TimeoutFree(AvahiTimeout &aTimer)
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

void Poller::Update(MainloopContext &aMainloop)
{
    Timepoint now = Clock::now();

    for (Watches::iterator it = mWatches.begin(); it != mWatches.end(); ++it)
    {
        int             fd     = (*it)->mFd;
        AvahiWatchEvent events = (*it)->mEvents;

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

        (*it)->mHappened = 0;
    }

    for (Timers::iterator it = mTimers.begin(); it != mTimers.end(); ++it)
    {
        Timepoint timeout = (*it)->mTimeout;

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

void Poller::Process(const MainloopContext &aMainloop)
{
    Timepoint                   now = Clock::now();
    std::vector<AvahiTimeout *> expired;

    for (Watches::iterator it = mWatches.begin(); it != mWatches.end(); ++it)
    {
        int             fd     = (*it)->mFd;
        AvahiWatchEvent events = (*it)->mEvents;

        (*it)->mHappened = 0;

        if ((AVAHI_WATCH_IN & events) && FD_ISSET(fd, &aMainloop.mReadFdSet))
        {
            (*it)->mHappened |= AVAHI_WATCH_IN;
        }

        if ((AVAHI_WATCH_OUT & events) && FD_ISSET(fd, &aMainloop.mWriteFdSet))
        {
            (*it)->mHappened |= AVAHI_WATCH_OUT;
        }

        if ((AVAHI_WATCH_ERR & events) && FD_ISSET(fd, &aMainloop.mErrorFdSet))
        {
            (*it)->mHappened |= AVAHI_WATCH_ERR;
        }

        // TODO hup events
        if ((*it)->mHappened)
        {
            (*it)->mCallback(*it, (*it)->mFd, static_cast<AvahiWatchEvent>((*it)->mHappened), (*it)->mContext);
        }
    }

    for (Timers::iterator it = mTimers.begin(); it != mTimers.end(); ++it)
    {
        if ((*it)->mTimeout == Timepoint::min())
        {
            continue;
        }

        if ((*it)->mTimeout <= now)
        {
            expired.push_back(*it);
        }
    }

    for (std::vector<AvahiTimeout *>::iterator it = expired.begin(); it != expired.end(); ++it)
    {
        AvahiTimeout *avahiTimeout = *it;

        avahiTimeout->mCallback(avahiTimeout, avahiTimeout->mContext);
    }
}

PublisherAvahi::PublisherAvahi(int aProtocol, const char *aDomain, StateHandler aHandler, void *aContext)
    : mClient(nullptr)
    , mProtocol(aProtocol == AF_INET6 ? AVAHI_PROTO_INET6
                                      : aProtocol == AF_INET ? AVAHI_PROTO_INET : AVAHI_PROTO_UNSPEC)
    , mDomain(aDomain)
    , mState(State::kIdle)
    , mStateHandler(aHandler)
    , mContext(aContext)
{
}

PublisherAvahi::~PublisherAvahi(void)
{
}

otbrError PublisherAvahi::Start(void)
{
    otbrError error      = OTBR_ERROR_NONE;
    int       avahiError = 0;

    mClient = avahi_client_new(mPoller.GetAvahiPoll(), AVAHI_CLIENT_NO_FAIL, HandleClientState, this, &avahiError);

    if (avahiError)
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
    FreeAllGroups();

    if (mClient)
    {
        avahi_client_free(mClient);
        mClient = nullptr;
        mState  = State::kIdle;
        mStateHandler(mContext, mState);
    }
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
    otbrLogInfo("Avahi group change to state %d.", aState);

    /* Called whenever the entry group state changes */
    switch (aState)
    {
    case AVAHI_ENTRY_GROUP_ESTABLISHED:
        /* The entry group has been established successfully */
        otbrLogInfo("Group established.");
        CallHostOrServiceCallback(aGroup, OTBR_ERROR_NONE);
        break;

    case AVAHI_ENTRY_GROUP_COLLISION:
    {
        otbrLogErr("Name collision!");
        Services::iterator serviceIt = FindService(aGroup);
        if (serviceIt != mServices.end())
        {
            char *alternativeName = avahi_alternative_service_name(serviceIt->mName.c_str());
            assert(alternativeName != nullptr);
            otbrLogInfo("Re-publishing service %s using an alternative name: %s", serviceIt->mName.c_str(),
                        alternativeName);
            serviceIt->mName = alternativeName;
            avahi_free(alternativeName);
            RetryPublishService(serviceIt);
        }
        break;
    }

    case AVAHI_ENTRY_GROUP_FAILURE:
        /* Some kind of failure happened while we were registering our services */
        otbrLogErr("Group failed: %s!", avahi_strerror(avahi_client_errno(avahi_entry_group_get_client(aGroup))));
        CallHostOrServiceCallback(aGroup, OTBR_ERROR_MDNS);
        break;

    case AVAHI_ENTRY_GROUP_UNCOMMITED:
    case AVAHI_ENTRY_GROUP_REGISTERING:
        otbrLogErr("Group ready.");
        break;

    default:
        assert(false);
        break;
    }
}

void PublisherAvahi::CallHostOrServiceCallback(AvahiEntryGroup *aGroup, otbrError aError) const
{
    if (mHostHandler != nullptr)
    {
        const auto hostIt =
            std::find_if(mHosts.begin(), mHosts.end(), [aGroup](const Host &aHost) { return aHost.mGroup == aGroup; });

        if (hostIt != mHosts.end())
        {
            mHostHandler(hostIt->mHostName.c_str(), aError, mHostHandlerContext);
        }
    }

    if (mServiceHandler != nullptr)
    {
        const auto serviceIt = std::find_if(mServices.begin(), mServices.end(),
                                            [aGroup](const Service &aService) { return aService.mGroup == aGroup; });

        if (serviceIt != mServices.end())
        {
            mServiceHandler(serviceIt->mName.c_str(), serviceIt->mType.c_str(), aError, mServiceHandlerContext);
        }
    }
}

otbrError PublisherAvahi::RetryPublishService(const Services::iterator &aServiceIt)
{
    otbrError error      = OTBR_ERROR_NONE;
    int       avahiError = 0;
    Service & service    = *aServiceIt;
    // aligned with AvahiStringList
    AvahiStringList  buffer[(kMaxSizeOfTxtRecord - 1) / sizeof(AvahiStringList) + 1];
    AvahiStringList *head = nullptr;

    SuccessOrExit(error = TxtListToAvahiStringList(service.mTxtList, buffer, sizeof(buffer), head));
    avahiError = avahi_entry_group_add_service_strlst(service.mGroup, AVAHI_IF_UNSPEC, mProtocol, AvahiPublishFlags{},
                                                      service.mName.c_str(), service.mType.c_str(), mDomain,
                                                      service.mHostName.c_str(), service.mPort, head);
    SuccessOrExit(avahiError);

    for (const std::string &subType : service.mSubTypeList)
    {
        otbrLogInfo("Add subtype %s for service %s.%s", subType.c_str(), service.mName.c_str(), service.mType.c_str());
        std::string fullSubType = subType + "._sub." + service.mType;
        avahiError              = avahi_entry_group_add_service_subtype(service.mGroup, AVAHI_IF_UNSPEC, mProtocol,
                                                           AvahiPublishFlags{}, service.mName.c_str(),
                                                           service.mType.c_str(), mDomain, fullSubType.c_str());
        SuccessOrExit(avahiError);
    }

    otbrLogInfo("Commit service %s.%s", service.mName.c_str(), service.mType.c_str());
    avahiError = avahi_entry_group_commit(service.mGroup);
    SuccessOrExit(avahiError);

exit:
    if (avahiError)
    {
        error = OTBR_ERROR_MDNS;
        otbrLogErr("Failed to retry publishing service for avahi error: %s!", avahi_strerror(avahiError));
    }
    else if (error != OTBR_ERROR_NONE)
    {
        otbrLogErr("Failed to retry publishing service: %s!", otbrErrorString(error));
        FreeGroup(service.mGroup);
        mServices.erase(aServiceIt);
    }

    return error;
}

PublisherAvahi::Hosts::iterator PublisherAvahi::FindHost(const char *aHostName)
{
    assert(aHostName != nullptr);

    return std::find_if(mHosts.begin(), mHosts.end(),
                        [aHostName](const Host &aHost) { return aHost.mHostName == aHostName; });
}

otbrError PublisherAvahi::CreateHost(AvahiClient &aClient, const char *aHostName, Hosts::iterator &aOutHostIt)
{
    assert(aHostName != nullptr);

    otbrError error = OTBR_ERROR_NONE;
    Host      newHost;

    newHost.mHostName = aHostName;
    SuccessOrExit(error = CreateGroup(aClient, newHost.mGroup));

    mHosts.push_back(newHost);
    aOutHostIt = mHosts.end() - 1;

exit:
    return error;
}

PublisherAvahi::Services::iterator PublisherAvahi::FindService(const char *aName, const char *aType)
{
    assert(aName != nullptr);
    assert(aType != nullptr);

    return std::find_if(mServices.begin(), mServices.end(), [aName, aType](const Service &aService) {
        return aService.mName == aName && aService.mType == aType;
    });
}

PublisherAvahi::Services::iterator PublisherAvahi::FindService(AvahiEntryGroup *aGroup)
{
    return std::find_if(mServices.begin(), mServices.end(),
                        [aGroup](const Service &aService) { return aService.mGroup == aGroup; });
}

otbrError PublisherAvahi::CreateService(AvahiClient &       aClient,
                                        const char *        aName,
                                        const char *        aType,
                                        Services::iterator &aOutServiceIt)
{
    assert(aName != nullptr);
    assert(aType != nullptr);

    otbrError error = OTBR_ERROR_NONE;
    Service   newService;

    newService.mName = aName;
    newService.mType = aType;
    SuccessOrExit(error = CreateGroup(aClient, newService.mGroup));

    mServices.push_back(newService);
    aOutServiceIt = mServices.end() - 1;

exit:
    return error;
}

bool PublisherAvahi::IsServiceOutdated(const Service &    aService,
                                       const char *       aNewHostName,
                                       uint16_t           aNewPort,
                                       const SubTypeList &aNewSubTypeList)
{
    return aService.mHostName != aNewHostName || aService.mPort != aNewPort || aService.mSubTypeList != aNewSubTypeList;
}

otbrError PublisherAvahi::CreateGroup(AvahiClient &aClient, AvahiEntryGroup *&aOutGroup)
{
    otbrError error = OTBR_ERROR_NONE;

    assert(aOutGroup == nullptr);

    aOutGroup = avahi_entry_group_new(&aClient, HandleGroupState, this);
    VerifyOrExit(aOutGroup != nullptr, error = OTBR_ERROR_MDNS);

exit:
    if (error == OTBR_ERROR_MDNS)
    {
        otbrLogErr("Failed to create entry group for avahi error: %s", avahi_strerror(avahi_client_errno(&aClient)));
    }

    return error;
}

otbrError PublisherAvahi::ResetGroup(AvahiEntryGroup *aGroup)
{
    assert(aGroup != nullptr);

    otbrError error      = OTBR_ERROR_NONE;
    int       avahiError = avahi_entry_group_reset(aGroup);

    if (avahiError)
    {
        error = OTBR_ERROR_MDNS;
        otbrLogErr("Failed to reset entry group for avahi error: %s", avahi_strerror(avahiError));
    }

    return error;
}

otbrError PublisherAvahi::FreeGroup(AvahiEntryGroup *aGroup)
{
    assert(aGroup != nullptr);

    otbrError error      = OTBR_ERROR_NONE;
    int       avahiError = avahi_entry_group_free(aGroup);

    if (avahiError)
    {
        error = OTBR_ERROR_MDNS;
        otbrLogErr("Failed to free entry group for avahi error: %s", avahi_strerror(avahiError));
    }

    return error;
}

void PublisherAvahi::FreeAllGroups(void)
{
    for (Service &service : mServices)
    {
        FreeGroup(service.mGroup);
    }

    mServices.clear();

    for (Host &host : mHosts)
    {
        FreeGroup(host.mGroup);
    }

    mHosts.clear();
}

void PublisherAvahi::HandleClientState(AvahiClient *aClient, AvahiClientState aState)
{
    otbrLogInfo("Avahi client state changed to %d.", aState);
    switch (aState)
    {
    case AVAHI_CLIENT_S_RUNNING:
        /* The server has startup successfully and registered its host
         * name on the network, so it's time to create our services */
        otbrLogInfo("Avahi client ready.");
        mState  = State::kReady;
        mClient = aClient;
        mStateHandler(mContext, mState);
        break;

    case AVAHI_CLIENT_FAILURE:
        otbrLogErr("Client failure: %s", avahi_strerror(avahi_client_errno(aClient)));
        mState = State::kIdle;
        mStateHandler(mContext, mState);
        break;

    case AVAHI_CLIENT_S_COLLISION:
        /* Let's drop our registered services. When the server is back
         * in AVAHI_SERVER_RUNNING state we will register them
         * again with the new host name. */
        otbrLogErr("Client collision: %s", avahi_strerror(avahi_client_errno(aClient)));

        // fall through

    case AVAHI_CLIENT_S_REGISTERING:
        /* The server records are now being established. This
         * might be caused by a host name change. We need to wait
         * for our own records to register until the host name is
         * properly esatblished. */
        FreeAllGroups();
        break;

    case AVAHI_CLIENT_CONNECTING:
        otbrLogDebug("Connecting to avahi server");
        break;

    default:
        assert(false);
        break;
    }
}

void PublisherAvahi::Update(MainloopContext &aMainloop)
{
    mPoller.Update(aMainloop);
}

void PublisherAvahi::Process(const MainloopContext &aMainloop)
{
    mPoller.Process(aMainloop);
}

otbrError PublisherAvahi::PublishService(const char *       aHostName,
                                         uint16_t           aPort,
                                         const char *       aName,
                                         const char *       aType,
                                         const SubTypeList &aSubTypeList,
                                         const TxtList &    aTxtList)
{
    otbrError          error        = OTBR_ERROR_NONE;
    int                avahiError   = 0;
    Services::iterator serviceIt    = mServices.end();
    const char *       safeHostName = (aHostName != nullptr) ? aHostName : "";
    const char *       logHostName  = (aHostName != nullptr) ? aHostName : "localhost";
    std::string        fullHostName;
    // aligned with AvahiStringList
    AvahiStringList  buffer[(kMaxSizeOfTxtRecord - 1) / sizeof(AvahiStringList) + 1];
    AvahiStringList *head = nullptr;

    SuccessOrExit(error = TxtListToAvahiStringList(aTxtList, buffer, sizeof(buffer), head));

    VerifyOrExit(mState == State::kReady, errno = EAGAIN, error = OTBR_ERROR_ERRNO);
    VerifyOrExit(mClient != nullptr, errno = EAGAIN, error = OTBR_ERROR_ERRNO);
    VerifyOrExit(aName != nullptr, error = OTBR_ERROR_INVALID_ARGS);
    VerifyOrExit(aType != nullptr, error = OTBR_ERROR_INVALID_ARGS);

    if (aHostName != nullptr)
    {
        fullHostName = MakeFullName(aHostName);
        aHostName    = fullHostName.c_str();
    }

    serviceIt = FindService(aName, aType);

    if (serviceIt == mServices.end())
    {
        SuccessOrExit(error = CreateService(*mClient, aName, aType, serviceIt));
    }
    else if (IsServiceOutdated(*serviceIt, safeHostName, aPort, aSubTypeList))
    {
        SuccessOrExit(error = ResetGroup(serviceIt->mGroup));
    }
    else
    {
        otbrLogInfo("Update service %s.%s for host %s", aName, aType, logHostName);
        avahiError = avahi_entry_group_update_service_txt_strlst(serviceIt->mGroup, AVAHI_IF_UNSPEC, mProtocol,
                                                                 AvahiPublishFlags{}, aName, aType, mDomain, head);
        if (avahiError == 0 && mServiceHandler != nullptr)
        {
            // The handler should be called even if the request can be processed synchronously
            mServiceHandler(aName, aType, OTBR_ERROR_NONE, mServiceHandlerContext);
        }
        serviceIt->mTxtList = aTxtList;
        ExitNow();
    }

    otbrLogInfo("Create service %s.%s for host %s", aName, aType, logHostName);
    avahiError =
        avahi_entry_group_add_service_strlst(serviceIt->mGroup, AVAHI_IF_UNSPEC, mProtocol, AvahiPublishFlags{}, aName,
                                             aType, mDomain, aHostName, aPort, head);
    SuccessOrExit(avahiError);

    for (const std::string &subType : aSubTypeList)
    {
        otbrLogInfo("Add subtype %s for service %s.%s", subType.c_str(), aName, aType);
        std::string fullSubType = subType + "._sub." + aType;
        avahiError =
            avahi_entry_group_add_service_subtype(serviceIt->mGroup, AVAHI_IF_UNSPEC, mProtocol, AvahiPublishFlags{},
                                                  aName, aType, mDomain, fullSubType.c_str());
        SuccessOrExit(avahiError);
    }

    otbrLogInfo("Commit service %s.%s", aName, aType);
    avahiError = avahi_entry_group_commit(serviceIt->mGroup);
    SuccessOrExit(avahiError);

    serviceIt->mSubTypeList = aSubTypeList;
    serviceIt->mHostName    = safeHostName;
    serviceIt->mPort        = aPort;
    serviceIt->mTxtList     = aTxtList;

exit:

    if (avahiError)
    {
        error = OTBR_ERROR_MDNS;
        otbrLogErr("Failed to publish service for avahi error: %s!", avahi_strerror(avahiError));
    }
    else if (error != OTBR_ERROR_NONE)
    {
        otbrLogErr("Failed to publish service: %s!", otbrErrorString(error));
    }

    if (error != OTBR_ERROR_NONE && serviceIt != mServices.end())
    {
        FreeGroup(serviceIt->mGroup);
        mServices.erase(serviceIt);
    }

    return error;
}

otbrError PublisherAvahi::UnpublishService(const char *aName, const char *aType)
{
    otbrError          error = OTBR_ERROR_NONE;
    Services::iterator serviceIt;

    VerifyOrExit(aName != nullptr, error = OTBR_ERROR_INVALID_ARGS);
    VerifyOrExit(aType != nullptr, error = OTBR_ERROR_INVALID_ARGS);

    serviceIt = FindService(aName, aType);
    VerifyOrExit(serviceIt != mServices.end());

    otbrLogInfo("Unpublish service %s.%s", aName, aType);
    error = FreeGroup(serviceIt->mGroup);
    mServices.erase(serviceIt);

exit:
    return error;
}

otbrError PublisherAvahi::PublishHost(const char *aName, const uint8_t *aAddress, uint8_t aAddressLength)
{
    otbrError       error      = OTBR_ERROR_NONE;
    int             avahiError = 0;
    Hosts::iterator hostIt     = mHosts.end();
    std::string     fullHostName;
    AvahiAddress    address;

    VerifyOrExit(mState == State::kReady, errno = EAGAIN, error = OTBR_ERROR_ERRNO);
    VerifyOrExit(mClient != nullptr, errno = EAGAIN, error = OTBR_ERROR_ERRNO);
    VerifyOrExit(aName != nullptr, error = OTBR_ERROR_INVALID_ARGS);
    VerifyOrExit(aAddress != nullptr, error = OTBR_ERROR_INVALID_ARGS);
    VerifyOrExit(aAddressLength == sizeof(address.data.ipv6.address), error = OTBR_ERROR_INVALID_ARGS);

    fullHostName = MakeFullName(aName);
    hostIt       = FindHost(aName);

    if (hostIt == mHosts.end())
    {
        SuccessOrExit(error = CreateHost(*mClient, aName, hostIt));
    }
    else if (memcmp(hostIt->mAddress.data.ipv6.address, aAddress, aAddressLength))
    {
        SuccessOrExit(error = ResetGroup(hostIt->mGroup));
    }
    else
    {
        if (mHostHandler != nullptr)
        {
            // The handler should be called even if the request can be processed synchronously
            mHostHandler(aName, OTBR_ERROR_NONE, mHostHandlerContext);
        }
        ExitNow();
    }

    address.proto = AVAHI_PROTO_INET6;
    memcpy(&address.data.ipv6.address[0], aAddress, aAddressLength);

    otbrLogInfo("Create host %s", aName);
    avahiError = avahi_entry_group_add_address(hostIt->mGroup, AVAHI_IF_UNSPEC, mProtocol, AVAHI_PUBLISH_NO_REVERSE,
                                               fullHostName.c_str(), &address);
    SuccessOrExit(avahiError);

    otbrLogInfo("Commit host %s", aName);
    avahiError = avahi_entry_group_commit(hostIt->mGroup);
    SuccessOrExit(avahiError);

    hostIt->mAddress = address;

exit:

    if (avahiError)
    {
        error = OTBR_ERROR_MDNS;
        otbrLogErr("Failed to publish host for avahi error: %s!", avahi_strerror(avahiError));
    }
    else if (error != OTBR_ERROR_NONE)
    {
        otbrLogErr("Failed to publish host: %s!", otbrErrorString(error));
    }

    if (error != OTBR_ERROR_NONE && hostIt != mHosts.end())
    {
        FreeGroup(hostIt->mGroup);
        mHosts.erase(hostIt);
    }

    return error;
}

otbrError PublisherAvahi::UnpublishHost(const char *aName)
{
    otbrError       error = OTBR_ERROR_NONE;
    Hosts::iterator hostIt;

    VerifyOrExit(aName != nullptr, error = OTBR_ERROR_INVALID_ARGS);

    hostIt = FindHost(aName);
    VerifyOrExit(hostIt != mHosts.end());

    otbrLogInfo("Delete host %s", aName);
    error = FreeGroup(hostIt->mGroup);
    mHosts.erase(hostIt);

exit:
    return error;
}

otbrError PublisherAvahi::TxtListToAvahiStringList(const TxtList &   aTxtList,
                                                   AvahiStringList * aBuffer,
                                                   size_t            aBufferSize,
                                                   AvahiStringList *&aHead)
{
    otbrError        error = OTBR_ERROR_NONE;
    size_t           used  = 0;
    AvahiStringList *last  = nullptr;
    AvahiStringList *curr  = aBuffer;

    aHead = nullptr;
    for (const auto &txtEntry : aTxtList)
    {
        const char *   name        = txtEntry.mName.c_str();
        size_t         nameLength  = txtEntry.mName.length();
        const uint8_t *value       = txtEntry.mValue.data();
        size_t         valueLength = txtEntry.mValue.size();
        // +1 for the size of "=", avahi doesn't need '\0' at the end of the entry
        size_t needed = sizeof(AvahiStringList) - sizeof(AvahiStringList::text) + nameLength + valueLength + 1;

        VerifyOrExit(used + needed <= aBufferSize, errno = EMSGSIZE, error = OTBR_ERROR_ERRNO);
        curr->next = last;
        last       = curr;
        memcpy(curr->text, name, nameLength);
        curr->text[nameLength] = '=';
        memcpy(curr->text + nameLength + 1, value, valueLength);
        curr->size = nameLength + valueLength + 1;
        {
            const uint8_t *next = curr->text + curr->size;
            curr                = OTBR_ALIGNED(next, AvahiStringList *);
        }
        used = static_cast<size_t>(reinterpret_cast<uint8_t *>(curr) - reinterpret_cast<uint8_t *>(aBuffer));
    }
    SuccessOrExit(error);
    aHead = last;
exit:
    return error;
}

std::string PublisherAvahi::MakeFullName(const char *aName)
{
    assert(aName != nullptr);

    std::string fullHostName(aName);

    fullHostName += '.';
    fullHostName += (mDomain == nullptr ? "local." : mDomain);

    return fullHostName;
}

void PublisherAvahi::SubscribeService(const std::string &aType, const std::string &aInstanceName)
{
    mSubscribedServices.emplace_back(*this, aType, aInstanceName);

    otbrLogInfo("subscribe service %s.%s (total %zu)", aInstanceName.c_str(), aType.c_str(),
                mSubscribedServices.size());

    if (aInstanceName.empty())
    {
        mSubscribedServices.back().Browse();
    }
    else
    {
        mSubscribedServices.back().Resolve(AVAHI_IF_UNSPEC, mProtocol, aInstanceName.c_str(), aType.c_str(), "local.");
    }
}

void PublisherAvahi::UnsubscribeService(const std::string &aType, const std::string &aInstanceName)
{
    ServiceSubscriptionList::iterator it =
        std::find_if(mSubscribedServices.begin(), mSubscribedServices.end(),
                     [&aType, &aInstanceName](const ServiceSubscription &aService) {
                         return aService.mType == aType && aService.mInstanceName == aInstanceName;
                     });

    assert(it != mSubscribedServices.end());

    it->Release();
    mSubscribedServices.erase(it);

    otbrLogInfo("unsubscribe service %s.%s (left %zu)", aInstanceName.c_str(), aType.c_str(),
                mSubscribedServices.size());
}

void PublisherAvahi::OnServiceResolved(ServiceSubscription &aService)
{
    otbrLogInfo("Service %s is resolved successfully: %s host %s addresses %zu", aService.mType.c_str(),
                aService.mInstanceInfo.mName.c_str(), aService.mInstanceInfo.mHostName.c_str(),
                aService.mInstanceInfo.mAddresses.size());
    if (mDiscoveredServiceInstanceCallback != nullptr)
    {
        mDiscoveredServiceInstanceCallback(aService.mType, aService.mInstanceInfo);
    }
}

void PublisherAvahi::OnServiceResolveFailed(const ServiceSubscription &aService, int aErrorCode)
{
    otbrLogWarning("Service %s resolving failed: code=%d", aService.mType.c_str(), aErrorCode);
}

void PublisherAvahi::OnHostResolved(HostSubscription &aHost)
{
    otbrLogInfo("Host %s is resolved successfully: host %s addresses %zu ttl %u", aHost.mHostName.c_str(),
                aHost.mHostInfo.mHostName.c_str(), aHost.mHostInfo.mAddresses.size(), aHost.mHostInfo.mTtl);
    if (mDiscoveredHostCallback != nullptr)
    {
        mDiscoveredHostCallback(aHost.mHostName, aHost.mHostInfo);
    }
}

void PublisherAvahi::OnHostResolveFailed(const HostSubscription &aHost, int aErrorCode)
{
    otbrLogWarning("Host %s resolving failed: code=%d", aHost.mHostName.c_str(), aErrorCode);
}

void PublisherAvahi::SubscribeHost(const std::string &aHostName)
{
    mSubscribedHosts.emplace_back(*this, aHostName);

    otbrLogInfo("subscribe host %s (total %zu)", aHostName.c_str(), mSubscribedHosts.size());

    mSubscribedHosts.back().Resolve();
}

void PublisherAvahi::UnsubscribeHost(const std::string &aHostName)
{
    HostSubscriptionList::iterator it =
        std::find_if(mSubscribedHosts.begin(), mSubscribedHosts.end(),
                     [&aHostName](const HostSubscription &aHost) { return aHost.mHostName == aHostName; });

    assert(it != mSubscribedHosts.end());

    it->Release();
    mSubscribedHosts.erase(it);

    otbrLogInfo("unsubscribe host %s (remaining %d)", aHostName.c_str(), mSubscribedHosts.size());
}

Publisher *Publisher::Create(int aFamily, const char *aDomain, StateHandler aHandler, void *aContext)
{
    return new PublisherAvahi(aFamily, aDomain, aHandler, aContext);
}

void Publisher::Destroy(Publisher *aPublisher)
{
    delete static_cast<PublisherAvahi *>(aPublisher);
}

void PublisherAvahi::ServiceSubscription::Browse(void)
{
    assert(mPublisherAvahi->mClient != nullptr);

    otbrLogInfo("browse service %s", mType.c_str());
    mServiceBrowser =
        avahi_service_browser_new(mPublisherAvahi->mClient, AVAHI_IF_UNSPEC, mPublisherAvahi->mProtocol, mType.c_str(),
                                  mPublisherAvahi->mDomain, static_cast<AvahiLookupFlags>(0), HandleBrowseResult, this);
    if (!mServiceBrowser)
    {
        otbrLogWarning("failed to browse service %s: %s", mType.c_str(),
                       avahi_strerror(avahi_client_errno(mPublisherAvahi->mClient)));
    }
}

void PublisherAvahi::ServiceSubscription::Release(void)
{
    if (mServiceBrowser != nullptr)
    {
        avahi_service_browser_free(mServiceBrowser);
        mServiceBrowser = nullptr;
    }
    if (mServiceResolver != nullptr)
    {
        avahi_service_resolver_free(mServiceResolver);
        mServiceResolver = nullptr;
    }
}

void PublisherAvahi::ServiceSubscription::HandleBrowseResult(AvahiServiceBrowser *  aServiceBrowser,
                                                             AvahiIfIndex           aInterfaceIndex,
                                                             AvahiProtocol          aProtocol,
                                                             AvahiBrowserEvent      aEvent,
                                                             const char *           aName,
                                                             const char *           aType,
                                                             const char *           aDomain,
                                                             AvahiLookupResultFlags aFlags,
                                                             void *                 aContext)
{
    static_cast<PublisherAvahi::ServiceSubscription *>(aContext)->HandleBrowseResult(
        aServiceBrowser, aInterfaceIndex, aProtocol, aEvent, aName, aType, aDomain, aFlags);
}

void PublisherAvahi::ServiceSubscription::HandleBrowseResult(AvahiServiceBrowser *  aServiceBrowser,
                                                             AvahiIfIndex           aInterfaceIndex,
                                                             AvahiProtocol          aProtocol,
                                                             AvahiBrowserEvent      aEvent,
                                                             const char *           aName,
                                                             const char *           aType,
                                                             const char *           aDomain,
                                                             AvahiLookupResultFlags aFlags)
{
    OTBR_UNUSED_VARIABLE(aServiceBrowser);
    OTBR_UNUSED_VARIABLE(aProtocol);

    assert(mServiceBrowser == aServiceBrowser);

    otbrLogInfo("browse service reply: %s.%s.%s inf %u, flags=%u", aName, aType, aDomain, aInterfaceIndex, aFlags);

    if (aEvent == AVAHI_BROWSER_FAILURE)
    {
        mPublisherAvahi->OnServiceResolveFailed(*this, avahi_client_errno(mPublisherAvahi->mClient));
    }
    else
    {
        Resolve(aInterfaceIndex, aProtocol, aName, aType, aDomain);
    }
    if (mServiceBrowser != nullptr)
    {
        avahi_service_browser_free(mServiceBrowser);
        mServiceBrowser = nullptr;
    }
}

void PublisherAvahi::ServiceSubscription::Resolve(uint32_t      aInterfaceIndex,
                                                  AvahiProtocol aProtocol,
                                                  const char *  aInstanceName,
                                                  const char *  aType,
                                                  const char *  aDomain)
{
    otbrLogInfo("resolve service %s %s %s inf %d", aInstanceName, aType, aDomain, aInterfaceIndex);
    mServiceResolver =
        avahi_service_resolver_new(mPublisherAvahi->mClient, aInterfaceIndex, aProtocol, aInstanceName, aType, aDomain,
                                   AVAHI_PROTO_INET6, static_cast<AvahiLookupFlags>(0), HandleResolveResult, this);
    if (!mServiceResolver)
    {
        otbrLogErr("failed to resolve serivce %s: %s", mType.c_str(),
                   avahi_strerror(avahi_client_errno(mPublisherAvahi->mClient)));
    }
}

void PublisherAvahi::ServiceSubscription::HandleResolveResult(AvahiServiceResolver * aServiceResolver,
                                                              AvahiIfIndex           aInterfaceIndex,
                                                              AvahiProtocol          aProtocol,
                                                              AvahiResolverEvent     aEvent,
                                                              const char *           aName,
                                                              const char *           aType,
                                                              const char *           aDomain,
                                                              const char *           aHostName,
                                                              const AvahiAddress *   aAddress,
                                                              uint16_t               aPort,
                                                              AvahiStringList *      aTxt,
                                                              AvahiLookupResultFlags aFlags,
                                                              void *                 aContext)
{
    static_cast<PublisherAvahi::ServiceSubscription *>(aContext)->HandleResolveResult(
        aServiceResolver, aInterfaceIndex, aProtocol, aEvent, aName, aType, aDomain, aHostName, aAddress, aPort, aTxt,
        aFlags);
}

void PublisherAvahi::ServiceSubscription::HandleResolveResult(AvahiServiceResolver * aServiceResolver,
                                                              AvahiIfIndex           aInterfaceIndex,
                                                              AvahiProtocol          aProtocol,
                                                              AvahiResolverEvent     aEvent,
                                                              const char *           aName,
                                                              const char *           aType,
                                                              const char *           aDomain,
                                                              const char *           aHostName,
                                                              const AvahiAddress *   aAddress,
                                                              uint16_t               aPort,
                                                              AvahiStringList *      aTxt,
                                                              AvahiLookupResultFlags aFlags)
{
    OT_UNUSED_VARIABLE(aServiceResolver);
    OT_UNUSED_VARIABLE(aInterfaceIndex);
    OT_UNUSED_VARIABLE(aProtocol);
    OT_UNUSED_VARIABLE(aType);
    OT_UNUSED_VARIABLE(aDomain);

    char       buf[AVAHI_ADDRESS_STR_MAX];
    Ip6Address address;
    size_t     totalTxtSize = 0;

    assert(mServiceResolver == aServiceResolver);
    VerifyOrExit(
        aEvent == AVAHI_RESOLVER_FOUND,
        otbrLogErr("failed to resolve service: %s", avahi_strerror(avahi_client_errno(mPublisherAvahi->mClient))));
    VerifyOrExit(aHostName != nullptr, otbrLogErr("host name is null"));

    mInstanceInfo.mName     = aName;
    mInstanceInfo.mHostName = std::string(aHostName) + ".";
    mInstanceInfo.mPort     = aPort;
    avahi_address_snprint(buf, sizeof(buf), aAddress);
    VerifyOrExit(otbrError::OTBR_ERROR_NONE == Ip6Address::FromString(buf, address),
                 otbrLogErr("failed to parse the IP address: %s", buf));

    otbrLogDebug("resolve service reply: flags=%u, host=%s", aFlags, aHostName);

    VerifyOrExit(!address.IsLinkLocal() && !address.IsMulticast() && !address.IsLoopback() && !address.IsUnspecified(),
                 otbrLogDebug("ignoring address %s", address.ToString().c_str()));

    mInstanceInfo.mAddresses.push_back(address);

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

    otbrLogDebug("resolve service reply: address=%s, ttl=%u", address.ToString().c_str(), mInstanceInfo.mTtl);

    mPublisherAvahi->OnServiceResolved(*this);

exit:
    if (avahi_client_errno(mPublisherAvahi->mClient) != AVAHI_OK)
    {
        mPublisherAvahi->OnServiceResolveFailed(*this, avahi_client_errno(mPublisherAvahi->mClient));
    }
    if (mServiceBrowser != nullptr)
    {
        avahi_service_resolver_free(mServiceResolver);
        mServiceResolver = nullptr;
    }
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
    std::string fullHostName = mHostName + ".local.";

    otbrLogDebug("resolve host %s inf %d", fullHostName.c_str(), AVAHI_IF_UNSPEC);
    mRecordBrowser = avahi_record_browser_new(mPublisherAvahi->mClient, AVAHI_IF_UNSPEC, mPublisherAvahi->mProtocol,
                                              fullHostName.c_str(), AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_AAAA,
                                              static_cast<AvahiLookupFlags>(0), HandleResolveResult, this);
    if (!mRecordBrowser)
    {
        otbrLogErr("failed to resolve host %s: %s", fullHostName.c_str(),
                   avahi_strerror(avahi_client_errno(mPublisherAvahi->mClient)));
    }
}

void PublisherAvahi::HostSubscription::HandleResolveResult(AvahiRecordBrowser *   aRecordBrowser,
                                                           AvahiIfIndex           aInterfaceIndex,
                                                           AvahiProtocol          aProtocol,
                                                           AvahiBrowserEvent      aEvent,
                                                           const char *           aName,
                                                           uint16_t               aClazz,
                                                           uint16_t               aType,
                                                           const void *           aRdata,
                                                           size_t                 aSize,
                                                           AvahiLookupResultFlags aFlags,
                                                           void *                 aContext)
{
    static_cast<PublisherAvahi::HostSubscription *>(aContext)->HandleResolveResult(
        aRecordBrowser, aInterfaceIndex, aProtocol, aEvent, aName, aClazz, aType, aRdata, aSize, aFlags);
}

void PublisherAvahi::HostSubscription::HandleResolveResult(AvahiRecordBrowser *   aRecordBrowser,
                                                           AvahiIfIndex           aInterfaceIndex,
                                                           AvahiProtocol          aProtocol,
                                                           AvahiBrowserEvent      aEvent,
                                                           const char *           aName,
                                                           uint16_t               aClazz,
                                                           uint16_t               aType,
                                                           const void *           aRdata,
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

    Ip6Address address = *static_cast<const uint8_t(*)[16]>(aRdata);
    assert(mRecordBrowser == aRecordBrowser);
    VerifyOrExit(!address.IsLinkLocal() && !address.IsMulticast() && !address.IsLoopback() && !address.IsUnspecified());
    VerifyOrExit(aSize == 16, otbrLogErr("unexpected address data length: %u", aSize));
    otbrLogInfo("resolved host address: %s", address.ToString().c_str());

    mHostInfo.mHostName = std::string(aName) + ".";
    mHostInfo.mAddresses.push_back(std::move(address));
    // TODO: Use a more proper TTL
    mHostInfo.mTtl = kDefaultTtl;
    mPublisherAvahi->OnHostResolved(*this);

exit:
    if (avahi_client_errno(mPublisherAvahi->mClient) != AVAHI_OK)
    {
        mPublisherAvahi->OnHostResolveFailed(*this, avahi_client_errno(mPublisherAvahi->mClient));
    }
    if (mRecordBrowser != nullptr)
    {
        avahi_record_browser_free(mRecordBrowser);
        mRecordBrowser = nullptr;
    }
}

} // namespace Mdns

} // namespace otbr
