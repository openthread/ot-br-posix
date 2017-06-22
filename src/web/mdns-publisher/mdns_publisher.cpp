/*
 *  Copyright (c) 2017, The OpenThread Authors.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 *   This file implements starting an mDNS client, and publish border router service.
 */

#include "mdns_publisher.hpp"

#include "common/code_utils.hpp"
#include "common/logging.hpp"

#define OT_HOST_NAME "OPENTHREAD"
#define OT_PERIODICAL_TIME 1000 * 7

namespace ot {
namespace Mdns {

void Publisher::HandleServicePublish(AVAHI_GCC_UNUSED AvahiTimeout *aTimeout, AVAHI_GCC_UNUSED void *aUserData)
{
    Publisher *publisher = static_cast<Publisher *>(aUserData);

    publisher->HandleServicePublish(aTimeout);
}

void Publisher::HandleClientStart(AvahiClient *aClient, AvahiClientState aState,
                                  AVAHI_GCC_UNUSED void *aUserData)
{
    Publisher *publisher = static_cast<Publisher *>(aUserData);

    publisher->HandleClientStart(aClient, aState);
}

void Publisher::HandleEntryGroupStart(AvahiEntryGroup *aGroup, AvahiEntryGroupState aState,
                                      AVAHI_GCC_UNUSED void *aUserData)
{
    Publisher *publisher = static_cast<Publisher *>(aUserData);

    publisher->HandleEntryGroupStart(aGroup, aState);
}

void Publisher::Free(void)
{
    if (mClient != NULL)
    {
        avahi_client_free(mClient);
        mClient = NULL;
    }

    if (mSimplePoll != NULL)
    {
        avahi_simple_poll_free(mSimplePoll);
        mSimplePoll = NULL;
    }

    if (mServiceName != NULL)
    {
        avahi_free(mServiceName);
        mServiceName = NULL;
    }

    if (mClientGroup != NULL)
    {
        avahi_entry_group_free(mClientGroup);
        mClientGroup = NULL;
    }
}

Publisher::Publisher(void) :
    mClientGroup(NULL),
    mSimplePoll(NULL),
    mClient(NULL),
    mPort(0),
    mIsStarted(false),
    mServiceName(NULL),
    mNetworkNameTxt(NULL),
    mExtPanIdTxt(NULL),
    mType(NULL)
{
}

int Publisher::CreateService(AvahiClient *aClient)
{
    char *serviceName;
    int   ret = kMdnsPublisher_OK;

    assert(aClient);

    if (!mClientGroup)
    {
        mClientGroup = avahi_entry_group_new(aClient, HandleEntryGroupStart, this);
        VerifyOrExit(mClientGroup != NULL, ret = kMdnsPublisher_FailedCreateGoup);
    }

    if (avahi_entry_group_is_empty(mClientGroup))
    {
        otbrLog(OTBR_LOG_ERR, "Adding service '%s'", mServiceName);

        VerifyOrExit(avahi_entry_group_add_service(mClientGroup, AVAHI_IF_UNSPEC,
                                                   AVAHI_PROTO_UNSPEC, static_cast<AvahiPublishFlags>(0),
                                                   mServiceName, mType, NULL, NULL, mPort,
                                                   mNetworkNameTxt, mExtPanIdTxt, NULL) == 0,
                     ret = kMdnsPublisher_FailedAddSevice);

        otbrLog(OTBR_LOG_INFO, " Service Name: %s \n Port: %d \n Network Name: %s \n Extended Pan ID: %s",
                mServiceName, mPort, mNetworkNameTxt, mExtPanIdTxt);

        VerifyOrExit(avahi_entry_group_commit(mClientGroup) == 0,
                     ret = kMdnsPublisher_FailedRegisterSevice);
    }

exit:

    switch (ret)
    {
    case kMdnsPublisher_FailedAddSevice:
        serviceName = avahi_alternative_service_name(mServiceName);
        avahi_free(mServiceName);
        mServiceName = serviceName;
        otbrLog(OTBR_LOG_INFO, "Service name collision, renaming service to '%s'", mServiceName);
        avahi_entry_group_reset(mClientGroup);
        ret = CreateService(aClient);
        break;

    case kMdnsPublisher_FailedRegisterSevice:
        avahi_simple_poll_quit(mSimplePoll);
        break;

    default:
        break;
    }

    return ret;
}

void Publisher::HandleClientStart(AvahiClient *aClient, AvahiClientState aState)
{
    int ret = kMdnsPublisher_OK;

    assert(aClient);

    switch (aState)
    {
    case AVAHI_CLIENT_S_RUNNING:
        VerifyOrExit((ret = CreateService(aClient)) == kMdnsPublisher_OK);
        mIsStarted = true;
        break;

    case AVAHI_CLIENT_FAILURE:
        otbrLog(OTBR_LOG_ERR, "Client failure: %s", avahi_strerror(avahi_client_errno(aClient)));
        avahi_simple_poll_quit(mSimplePoll);
        break;

    case AVAHI_CLIENT_S_COLLISION:
    case AVAHI_CLIENT_S_REGISTERING:
        if (mClientGroup)
        {
            avahi_entry_group_reset(mClientGroup);
        }

        break;

    case AVAHI_CLIENT_CONNECTING:
        break;

    default:
        break;
    }

exit:

    if (ret != kMdnsPublisher_OK)
    {
        mIsStarted = false;
        otbrLog(OTBR_LOG_ERR, "started client failure: %d", ret);
    }
}

void Publisher::HandleServicePublish(AVAHI_GCC_UNUSED AvahiTimeout *aTimeout)
{
    struct timeval tv;
    int            ret = kMdnsPublisher_OK;

    if ((mClient != NULL) && (avahi_client_get_state(mClient) == AVAHI_CLIENT_S_RUNNING))
    {
        if (mClientGroup != NULL)
        {
            avahi_entry_group_reset(mClientGroup);
        }

        VerifyOrExit((ret = CreateService(mClient)) == kMdnsPublisher_OK);

        avahi_simple_poll_get(mSimplePoll)->timeout_new(
            avahi_simple_poll_get(mSimplePoll),
            avahi_elapse_time(&tv, OT_PERIODICAL_TIME, 0),
            HandleServicePublish,
            this);
    }

exit:

    if (ret != kMdnsPublisher_OK)
    {
        mIsStarted = false;
        otbrLog(OTBR_LOG_ERR, "published service failure: %d", ret);
    }
}

int Publisher::StartClient(void)
{
    int            error;
    int            ret = kMdnsPublisher_OK;
    struct timeval tv;

    VerifyOrExit((mSimplePoll = avahi_simple_poll_new()) != NULL,
                 ret = kMdnsPublisher_FailedCreatePoll);
    mClient = avahi_client_new(avahi_simple_poll_get(mSimplePoll), (AvahiClientFlags) 0,
                               HandleClientStart, this, &error);
    VerifyOrExit(mClient, ret = kMdnsPublisher_FailedCreateClient);

    avahi_simple_poll_get(mSimplePoll)->timeout_new(
        avahi_simple_poll_get(mSimplePoll),
        avahi_elapse_time(&tv, OT_PERIODICAL_TIME, 0),
        HandleServicePublish,
        this);
    avahi_simple_poll_loop(mSimplePoll);

exit:

    Free();
    return ret;
}

void Publisher::SetServiceName(const char *aServiceName)
{
    avahi_free(mServiceName);
    mServiceName = avahi_strdup(aServiceName);
}

void Publisher::HandleEntryGroupStart(AvahiEntryGroup *aGroup, AvahiEntryGroupState aState)
{
    int ret = kMdnsPublisher_OK;

    assert(aGroup == mClientGroup || mClientGroup == NULL);
    mClientGroup = aGroup;

    switch (aState)
    {

    case AVAHI_ENTRY_GROUP_ESTABLISHED:
        otbrLog(OTBR_LOG_INFO, "Service '%s' successfully established.", mServiceName);
        break;

    case AVAHI_ENTRY_GROUP_COLLISION:
        char *serviceName;

        serviceName = avahi_alternative_service_name(mServiceName);
        avahi_free(mServiceName);
        mServiceName = serviceName;
        otbrLog(OTBR_LOG_ERR, "Service name collision, renaming service to '%s'", mServiceName);
        VerifyOrExit((ret = CreateService(avahi_entry_group_get_client(aGroup))) == kMdnsPublisher_OK);
        break;

    case AVAHI_ENTRY_GROUP_FAILURE:

        otbrLog(OTBR_LOG_ERR, "Entry group failure: %s",
                avahi_strerror(avahi_client_errno(avahi_entry_group_get_client(aGroup))));

        avahi_simple_poll_quit(mSimplePoll);
        break;

    case AVAHI_ENTRY_GROUP_UNCOMMITED:
    case AVAHI_ENTRY_GROUP_REGISTERING:
        break;

    default:
        break;
    }

exit:

    if (ret != kMdnsPublisher_OK)
    {
        mIsStarted = false;
        otbrLog(OTBR_LOG_ERR, "Entry group failure: %d", ret);
    }
}

void Publisher::SetNetworkNameTxt(const char *aNetworkNameTxt)
{
    avahi_free(mNetworkNameTxt);
    mNetworkNameTxt = avahi_strdup(aNetworkNameTxt);
}

void Publisher::SetExtPanIdTxt(const char *aExtPanIdTxt)
{
    avahi_free(mExtPanIdTxt);
    mExtPanIdTxt = avahi_strdup(aExtPanIdTxt);
}

void Publisher::SetType(const char *aType)
{
    avahi_free(mType);
    mType = avahi_strdup(aType);
}

void Publisher::SetPort(uint16_t aPort)
{
    mPort = aPort;
}

int Publisher::UpdateService(void)
{
    int ret = kMdnsPublisher_OK;

    if (mClientGroup != NULL)
    {
        ret = avahi_entry_group_update_service_txt(mClientGroup, AVAHI_IF_UNSPEC,
                                                   AVAHI_PROTO_UNSPEC, static_cast<AvahiPublishFlags>(0),
                                                   mServiceName, mType, NULL, NULL, mPort,
                                                   mNetworkNameTxt, mExtPanIdTxt, NULL);
    }
    VerifyOrExit(ret == kMdnsPublisher_OK, ret = kMdnsPublisher_FailedUpdateSevice);
exit:
    return ret;
}

} //namespace Mdns
} //namespace ot
