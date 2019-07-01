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
 *   This file implements MDNS service based on avahi.
 */

#include "mdns_avahi.hpp"

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
        mTimeout = ot::BorderRouter::GetNow() + ot::BorderRouter::GetTimestamp(*aTimeout);
    }
    else
    {
        mTimeout = 0;
    }
}

namespace ot {

namespace BorderRouter {

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
    if (aTimeout == NULL)
    {
        aTimer->mTimeout = 0;
    }
    else
    {
        aTimer->mTimeout = ot::BorderRouter::GetNow() + ot::BorderRouter::GetTimestamp(*aTimeout);
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

void Poller::UpdateFdSet(fd_set &aReadFdSet, fd_set &aWriteFdSet, fd_set &aErrorFdSet, int &aMaxFd, timeval &aTimeout)
{
    for (Watches::iterator it = mWatches.begin(); it != mWatches.end(); ++it)
    {
        int             fd     = (*it)->mFd;
        AvahiWatchEvent events = (*it)->mEvents;

        if (AVAHI_WATCH_IN & events)
        {
            FD_SET(fd, &aReadFdSet);
        }

        if (AVAHI_WATCH_OUT & events)
        {
            FD_SET(fd, &aWriteFdSet);
        }

        if (AVAHI_WATCH_ERR & events)
        {
            FD_SET(fd, &aErrorFdSet);
        }

        if (AVAHI_WATCH_HUP & events)
        {
            // TODO what do with this event type?
        }

        if (aMaxFd < fd)
        {
            aMaxFd = fd;
        }

        (*it)->mHappened = 0;
    }

    unsigned long now = GetNow();

    for (Timers::iterator it = mTimers.begin(); it != mTimers.end(); ++it)
    {
        unsigned long timeout = (*it)->mTimeout;

        if (timeout == 0)
        {
            continue;
        }

        if (static_cast<long>(timeout - now) <= 0)
        {
            aTimeout.tv_usec = 0;
            aTimeout.tv_sec  = 0;
            break;
        }
        else
        {
            time_t      sec;
            suseconds_t usec;

            timeout -= now;
            sec  = static_cast<time_t>(timeout / 1000);
            usec = static_cast<suseconds_t>((timeout % 1000) * 1000);

            if (sec < aTimeout.tv_sec)
            {
                aTimeout.tv_sec = sec;
            }
            else if (sec == aTimeout.tv_sec)
            {
                if (usec < aTimeout.tv_usec)
                {
                    aTimeout.tv_usec = usec;
                }
            }
        }
    }
}

void Poller::Process(const fd_set &aReadFdSet, const fd_set &aWriteFdSet, const fd_set &aErrorFdSet)
{
    unsigned long now = GetNow();

    for (Watches::iterator it = mWatches.begin(); it != mWatches.end(); ++it)
    {
        int             fd     = (*it)->mFd;
        AvahiWatchEvent events = (*it)->mEvents;

        (*it)->mHappened = 0;

        if ((AVAHI_WATCH_IN & events) && FD_ISSET(fd, &aReadFdSet))
        {
            (*it)->mHappened |= AVAHI_WATCH_IN;
        }

        if ((AVAHI_WATCH_OUT & events) && FD_ISSET(fd, &aWriteFdSet))
        {
            (*it)->mHappened |= AVAHI_WATCH_OUT;
        }

        if ((AVAHI_WATCH_ERR & events) && FD_ISSET(fd, &aErrorFdSet))
        {
            (*it)->mHappened |= AVAHI_WATCH_ERR;
        }

        // TODO hup events
        if ((*it)->mHappened)
        {
            (*it)->mCallback(*it, (*it)->mFd, static_cast<AvahiWatchEvent>((*it)->mHappened), (*it)->mContext);
        }
    }

    std::vector<AvahiTimeout *> expired;

    for (Timers::iterator it = mTimers.begin(); it != mTimers.end(); ++it)
    {
        if ((*it)->mTimeout == 0)
        {
            continue;
        }

        if (static_cast<long>((*it)->mTimeout - now) <= 0)
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

PublisherAvahi::PublisherAvahi(int          aProtocol,
                               const char * aHost,
                               const char * aDomain,
                               StateHandler aHandler,
                               void *       aContext)
    : mClient(NULL)
    , mGroup(NULL)
    , mProtocol(aProtocol == AF_INET6 ? AVAHI_PROTO_INET6
                                      : aProtocol == AF_INET ? AVAHI_PROTO_INET : AVAHI_PROTO_UNSPEC)
    , mHost(aHost)
    , mDomain(aDomain)
    , mState(kStateIdle)
    , mStateHandler(aHandler)
    , mContext(aContext)
{
}

PublisherAvahi::~PublisherAvahi(void)
{
}

otbrError PublisherAvahi::Start(void)
{
    otbrError ret   = OTBR_ERROR_NONE;
    int       error = 0;

    mClient = avahi_client_new(mPoller.GetAvahiPoll(), AVAHI_CLIENT_NO_FAIL, HandleClientState, this, &error);

    if (error)
    {
        otbrLog(OTBR_LOG_ERR, "Failed to create avahi client: %s!", avahi_strerror(error));
        ret = OTBR_ERROR_MDNS;
    }

    return ret;
}

bool PublisherAvahi::IsStarted(void) const
{
    return mClient != NULL;
}

void PublisherAvahi::Stop(void)
{
    mServices.clear();

    if (mGroup)
    {
        int error = avahi_entry_group_reset(mGroup);

        if (error)
        {
            otbrLog(OTBR_LOG_ERR, "Failed to reset entry group: %s!", avahi_strerror(error));
        }
    }

    if (mClient)
    {
        avahi_client_free(mClient);
        mClient = NULL;
        mGroup  = NULL;
        mState  = kStateIdle;
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
    assert(mGroup == aGroup || mGroup == NULL);

    otbrLog(OTBR_LOG_INFO, "Avahi group change to state %d.", aState);
    mGroup = aGroup;

    /* Called whenever the entry group state changes */
    switch (aState)
    {
    case AVAHI_ENTRY_GROUP_ESTABLISHED:
        /* The entry group has been established successfully */
        otbrLog(OTBR_LOG_INFO, "Group established.");
        break;

    case AVAHI_ENTRY_GROUP_COLLISION:
        otbrLog(OTBR_LOG_ERR, "Name collision!");
        break;

    case AVAHI_ENTRY_GROUP_FAILURE:
        otbrLog(OTBR_LOG_ERR, "Group failed: %s!",
                avahi_strerror(avahi_client_errno(avahi_entry_group_get_client(aGroup))));
        /* Some kind of failure happened while we were registering our services */
        break;

    case AVAHI_ENTRY_GROUP_UNCOMMITED:
    case AVAHI_ENTRY_GROUP_REGISTERING:
        otbrLog(OTBR_LOG_ERR, "Group ready.");
        break;

    default:
        assert(false);
        break;
    }
}

void PublisherAvahi::CreateGroup(AvahiClient *aClient)
{
    VerifyOrExit(mGroup == NULL);

    VerifyOrExit((mGroup = avahi_entry_group_new(aClient, HandleGroupState, this)) != NULL);

exit:
    if (mGroup == NULL)
    {
        otbrLog(OTBR_LOG_ERR, "avahi_entry_group_new() failed: %s", avahi_strerror(avahi_client_errno(aClient)));
    }
}

void PublisherAvahi::HandleClientState(AvahiClient *aClient, AvahiClientState aState)
{
    otbrLog(OTBR_LOG_INFO, "Avahi client state changed to %d.", aState);
    switch (aState)
    {
    case AVAHI_CLIENT_S_RUNNING:
        /* The server has startup successfully and registered its host
         * name on the network, so it's time to create our services */
        otbrLog(OTBR_LOG_INFO, "Avahi client ready.");
        mState = kStateReady;
        CreateGroup(aClient);
        mStateHandler(mContext, mState);
        avahi_entry_group_commit(mGroup);
        break;

    case AVAHI_CLIENT_FAILURE:
        otbrLog(OTBR_LOG_ERR, "Client failure: %s", avahi_strerror(avahi_client_errno(aClient)));
        mState = kStateIdle;
        mStateHandler(mContext, mState);
        break;

    case AVAHI_CLIENT_S_COLLISION:
        /* Let's drop our registered services. When the server is back
         * in AVAHI_SERVER_RUNNING state we will register them
         * again with the new host name. */
        otbrLog(OTBR_LOG_ERR, "Client collision: %s", avahi_strerror(avahi_client_errno(aClient)));

        // fall through

    case AVAHI_CLIENT_S_REGISTERING:
        /* The server records are now being established. This
         * might be caused by a host name change. We need to wait
         * for our own records to register until the host name is
         * properly esatblished. */
        if (mGroup)
        {
            avahi_entry_group_reset(mGroup);
        }
        break;

    case AVAHI_CLIENT_CONNECTING:
        otbrLog(OTBR_LOG_DEBUG, "Connecting to avahi server");
        break;

    default:
        assert(false);
        break;
    }
}

void PublisherAvahi::UpdateFdSet(fd_set & aReadFdSet,
                                 fd_set & aWriteFdSet,
                                 fd_set & aErrorFdSet,
                                 int &    aMaxFd,
                                 timeval &aTimeout)
{
    mPoller.UpdateFdSet(aReadFdSet, aWriteFdSet, aErrorFdSet, aMaxFd, aTimeout);
}

void PublisherAvahi::Process(const fd_set &aReadFdSet, const fd_set &aWriteFdSet, const fd_set &aErrorFdSet)
{
    mPoller.Process(aReadFdSet, aWriteFdSet, aErrorFdSet);
}

otbrError PublisherAvahi::PublishService(uint16_t aPort, const char *aName, const char *aType, ...)
{
    otbrError ret   = OTBR_ERROR_ERRNO;
    int       error = 0;
    // aligned with AvahiStringList
    AvahiStringList  buffer[kMaxSizeOfTxtRecord / sizeof(AvahiStringList)];
    AvahiStringList *last = NULL;
    AvahiStringList *curr = buffer;
    va_list          args;
    size_t           used = 0;

    va_start(args, aType);

    VerifyOrExit(mState == kStateReady, errno = EAGAIN);

    for (const char *name = va_arg(args, const char *); name; name = va_arg(args, const char *))
    {
        int         rval;
        const char *value  = va_arg(args, const char *);
        size_t      needed = sizeof(AvahiStringList) + strlen(name) + strlen(value);

        VerifyOrExit(used + needed < sizeof(buffer), errno = EMSGSIZE);
        curr->next = last;
        last       = curr;
        rval       = sprintf(reinterpret_cast<char *>(curr->text), "%s=%s", name, value);
        assert(rval > 0);
        curr->size = static_cast<size_t>(rval);
        {
            const uint8_t *next = curr->text + curr->size;
            curr                = OTBR_ALIGNED(next, AvahiStringList *);
        }
        used = static_cast<size_t>(reinterpret_cast<uint8_t *>(curr) - reinterpret_cast<uint8_t *>(buffer));
    }

    for (Services::iterator it = mServices.begin(); it != mServices.end(); ++it)
    {
        if (!strncmp(it->mName, aName, sizeof(it->mName)) && !strncmp(it->mType, aType, sizeof(it->mType)) &&
            it->mPort == aPort)
        {
            otbrLog(OTBR_LOG_INFO, "MDNS update service %s", aName);
            error = avahi_entry_group_update_service_txt_strlst(
                mGroup, AVAHI_IF_UNSPEC, mProtocol, static_cast<AvahiPublishFlags>(0), aName, aType, mDomain, last);
            SuccessOrExit(error);
            ret = OTBR_ERROR_NONE;
            ExitNow();
        }
    }

    otbrLog(OTBR_LOG_INFO, "MDNS create service %s", aName);
    error = avahi_entry_group_add_service_strlst(mGroup, AVAHI_IF_UNSPEC, mProtocol, static_cast<AvahiPublishFlags>(0),
                                                 aName, aType, mDomain, mHost, aPort, last);
    SuccessOrExit(error);

    {
        Service service;
        strcpy_safe(service.mName, sizeof(service.mName), aName);
        strcpy_safe(service.mType, sizeof(service.mType), aType);
        service.mPort = aPort;
        mServices.push_back(service);
    }

    ret = OTBR_ERROR_NONE;

exit:
    va_end(args);

    if (error)
    {
        ret = OTBR_ERROR_MDNS;
        otbrLog(OTBR_LOG_ERR, "Failed to publish service for avahi error: %s!", avahi_strerror(error));
    }

    if (ret == OTBR_ERROR_ERRNO)
    {
        otbrLog(OTBR_LOG_ERR, "Failed to publish service: %s!", strerror(errno));
    }

    return ret;
}

Publisher *Publisher::Create(int aFamily, const char *aHost, const char *aDomain, StateHandler aHandler, void *aContext)
{
    return new PublisherAvahi(aFamily, aHost, aDomain, aHandler, aContext);
}

void Publisher::Destroy(Publisher *aPublisher)
{
    delete static_cast<PublisherAvahi *>(aPublisher);
}

} // namespace Mdns

} // namespace BorderRouter

} // namespace ot
