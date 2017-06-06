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
 *   This file implements starting an mDNS server, and publish border router service.
 */

#include "common/code_utils.hpp"

#include "mdns_publisher.hpp"

#define HOST_NAME "OPENTHREAD"
#define PERIODICAL_TIME 1000*7

namespace ot {
namespace Mdns {

enum
{
    kMdnsPublisher_OK = 0,
    kMdnsPublisher_FailedCreateServer,
    kMdnsPublisher_FailedCreatePoll,
    kMdnsPublisher_FailedFreePoll,
    kMdnsPublisher_FailedCreateGoup,
    kMdnsPublisher_FailedAddSevice,
    kMdnsPublisher_FailedRegisterSevice,
    kMdnsPublisher_FailedUpdateSevice
};

AvahiSEntryGroup *Publisher::mGroup = NULL;
AvahiSimplePoll  *Publisher::mSimplePoll = NULL;
AvahiServer      *Publisher::mServer = NULL;
char             *Publisher::mServiceName = NULL;
const char       *Publisher::mNetworkNameTxt = NULL;
const char       *Publisher::mExtPanIdTxt = NULL;
const char       *Publisher::mType = NULL;
bool              Publisher::isStart = false;
uint16_t          Publisher::mPort = 0;

void Publisher::Free(void)
{
    if (mServer)
        avahi_server_free(mServer);

    if (mSimplePoll)
        avahi_simple_poll_free(mSimplePoll);

    avahi_free(mServiceName);
}

int Publisher::CreateService(AvahiServer *aServer)
{
    int ret = kMdnsPublisher_OK;

    assert(aServer);

    if (!mGroup)
        VerifyOrExit((mGroup = avahi_s_entry_group_new(aServer, EntryGroupCallback, NULL)) != NULL,
                     ret = kMdnsPublisher_FailedCreateGoup);

    syslog(LOG_ERR, "Adding service '%s'", mServiceName);

    VerifyOrExit(avahi_server_add_service(aServer, mGroup, AVAHI_IF_UNSPEC,
                                          AVAHI_PROTO_UNSPEC, (AvahiPublishFlags) 0,
                                          mServiceName, mType, NULL, NULL, mPort,
                                          mNetworkNameTxt, mExtPanIdTxt, NULL) == 0,
                 ret = kMdnsPublisher_FailedAddSevice);

    syslog(LOG_INFO, " Service Name: %s Port: %d Network Name: %s Extended Pan ID: %s ",
           mServiceName, mPort, mNetworkNameTxt, mExtPanIdTxt);

    VerifyOrExit(avahi_s_entry_group_commit(mGroup) == 0,
                 ret = kMdnsPublisher_FailedRegisterSevice);
    return ret;

exit:
    if (ret != kMdnsPublisher_OK)
    {
        avahi_simple_poll_quit(mSimplePoll);
        isStart = false;
    }
    return ret;
}

void Publisher::ServerCallback(AvahiServer *aServer, AvahiServerState aState,
                               AVAHI_GCC_UNUSED void *aUserData)
{
    int ret = kMdnsPublisher_OK;

    assert(aServer);
    switch (aState)
    {

    case AVAHI_SERVER_RUNNING:
        if (!mGroup)
            CreateService(aServer);
        break;

    case AVAHI_SERVER_COLLISION:
        char *newHostName;

        newHostName = avahi_alternative_host_name(avahi_server_get_host_name(aServer));
        syslog(LOG_WARNING, "Host name collision, retrying with '%s'", newHostName);
        VerifyOrExit((ret = avahi_server_set_host_name(aServer, newHostName)) == 0);
        avahi_free(newHostName);

    case AVAHI_SERVER_REGISTERING:

        if (mGroup)
            avahi_s_entry_group_reset(mGroup);
        break;

    case AVAHI_SERVER_FAILURE:

        syslog(LOG_ERR, "Server failure: %s",
               avahi_strerror(avahi_server_errno(aServer)));
        avahi_simple_poll_quit(mSimplePoll);
        isStart = false;
        break;

    case AVAHI_SERVER_INVALID:
        ;
    }
exit:
    if (ret < 0)
    {
        avahi_simple_poll_quit(mSimplePoll);
    }
}

void Publisher::PeriodicallyPublish(AVAHI_GCC_UNUSED AvahiTimeout *aTimeout, void *aUserData)
{

    struct timeval tv;

    if (avahi_server_get_state(mServer) == AVAHI_SERVER_RUNNING)
    {

        if (mGroup)
            avahi_s_entry_group_reset(mGroup);

        CreateService(mServer);

        avahi_simple_poll_get(mSimplePoll)->timeout_new(
            avahi_simple_poll_get(mSimplePoll),
            avahi_elapse_time(&tv, PERIODICAL_TIME, 0),
            PeriodicallyPublish,
            mServer);
    }
}

int Publisher::StartServer(void)
{
    int               ret = kMdnsPublisher_OK;
    AvahiServerConfig config;
    int               error;
    struct timeval    tv;

    srand(time(NULL));

    VerifyOrExit((mSimplePoll = avahi_simple_poll_new()) != NULL,
                 ret = kMdnsPublisher_FailedCreatePoll);

    avahi_server_config_init(&config);

    config.host_name = avahi_strdup(HOST_NAME);
    config.publish_workstation = 0;
    config.publish_aaaa_on_ipv4 = 0;
    config.publish_a_on_ipv6 = 0;
    config.use_ipv4 = 1;
    config.use_ipv6 = 0;

    mServer = avahi_server_new(avahi_simple_poll_get(mSimplePoll), &config,
                               ServerCallback,
                               NULL, &error);
    avahi_server_config_free(&config);

    VerifyOrExit(mServer != NULL, ret = kMdnsPublisher_FailedCreateServer);

    avahi_simple_poll_get(mSimplePoll)->timeout_new(
        avahi_simple_poll_get(mSimplePoll),
        avahi_elapse_time(&tv, PERIODICAL_TIME, 0),
        PeriodicallyPublish,
        mServer);

    avahi_simple_poll_loop(mSimplePoll);
exit:
    Free();
    isStart = false;
    return ret;

}

void Publisher::SetServiceName(const char *aServiceName)
{
    mServiceName = avahi_strdup(aServiceName);
}

void Publisher::EntryGroupCallback(AvahiServer *aServer,
                                   AvahiSEntryGroup *aGroup, AvahiEntryGroupState aState,
                                   AVAHI_GCC_UNUSED void *aUserData)
{
    assert(aServer);
    assert(aGroup == mGroup);

    switch (aState)
    {

    case AVAHI_ENTRY_GROUP_ESTABLISHED:
        syslog(LOG_ERR, "Service '%s' successfully established.",
               mServiceName);
        isStart = true;
        break;

    case AVAHI_ENTRY_GROUP_COLLISION:
    {
        char *alternativeName;
        alternativeName = avahi_alternative_service_name(mServiceName);
        avahi_free(mServiceName);
        mServiceName = alternativeName;
        syslog(LOG_WARNING, "Service name collision, renaming service to '%s'",
               mServiceName);
        CreateService(aServer);
        break;
    }

    case AVAHI_ENTRY_GROUP_FAILURE:

        syslog(LOG_ERR, "Entry group failure: %s",
               avahi_strerror(avahi_server_errno(aServer)));
        avahi_simple_poll_quit(mSimplePoll);
        break;

    case AVAHI_ENTRY_GROUP_UNCOMMITED:
    case AVAHI_ENTRY_GROUP_REGISTERING:
        ;
    }
}

void Publisher::SetNetworkNameTxt(const char *aTxtData)
{
    mNetworkNameTxt = aTxtData;
}

void Publisher::SetExtPanIdTxt(const char *aTxtData)
{
    mExtPanIdTxt = aTxtData;
}

void Publisher::SetType(const char *aType)
{

    mType = aType;
}

void Publisher::SetPort(uint16_t aPort)
{
    mPort = aPort;
}

int Publisher::UpdateService(void)
{
    int ret = kMdnsPublisher_OK;

    ret = avahi_server_update_service_txt(mServer, mGroup, AVAHI_IF_UNSPEC,
                                          AVAHI_PROTO_UNSPEC, (AvahiPublishFlags) 0,
                                          mServiceName, mType, NULL, NULL, mPort,
                                          mNetworkNameTxt, mExtPanIdTxt, NULL);
    return ret;
}

} //namespace Mdns
} //namespace ot
