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
 *   This file implements starting mDNS client, and publish border router service.
 */

#ifndef MDNS_SERVER_HPP
#define MDNS_SERVER_HPP

#include <string>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include <avahi-client/client.h>
#include <avahi-client/publish.h>
#include <avahi-common/timeval.h>
#include <avahi-common/simple-watch.h>
#include <avahi-common/malloc.h>
#include <avahi-common/alternative.h>
#include <avahi-common/error.h>

namespace ot {
namespace Mdns {

/**
 *  Status of mDNS client
 *
 */
enum
{
    kMdnsPublisher_OK                   = 0, ///< successfully start mDNS service
    kMdnsPublisher_FailedCreatePoll     = 1, ///< failed to create poll
    kMdnsPublisher_FailedFreePoll       = 2, ///< failed to free poll
    kMdnsPublisher_FailedCreateGoup     = 3, ///< failed to create group
    kMdnsPublisher_FailedAddSevice      = 4, ///< failed to add service
    kMdnsPublisher_FailedRegisterSevice = 5, ///< failed to register service
    kMdnsPublisher_FailedUpdateSevice   = 6, ///< failed to update mDNS service
    kMdnsPublisher_FailedCreateClient   = 7, ///< failed to create mDNS client
};

/**
 * This class implements mDNS clinet.
 *
 */
class Publisher
{
public:

    /**
     * This method sets the service name.
     *
     * @param[in]  aServiceName  A pointer to the service name.
     *
     */
    void SetServiceName(const char *aServiceName);

    /**
     * This method sets the network name in TxT record.
     *
     * @param[in]  aNetworkNameTxt  A pointer to the network name.
     *
     */
    void SetNetworkNameTxt(const char *aNetworkNameTxt);

    /**
     * This method sets the extended PAN ID in TxT record.
     *
     * @param[in]  aExtPanIdTxt  A pointer to the extended PAN ID.
     *
     */
    void SetExtPanIdTxt(const char *aExtPanIdTxt);

    /**
     * This method sets the service type.
     *
     * @param[in]  aType  A pointer to the service type.
     *
     */
    void SetType(const char *aType);

    /**
     * This method sets the service port.
     *
     * @param[in]  aPort  service port.
     *
     */
    void SetPort(uint16_t aPort);

    /**
     * This method sets the protocol type.
     *
     * @param[in]  aProtoType  protocol type.
     *
     */
    void SetProtoType(int aProtoType);

    /**
     * This method sets the interface index.
     *
     * @param[in]  aInterfaceIndex  Index of interface.
     *
     */
    void SetInterfaceIndex(int aInterfaceIndex);

    /**
     * This method sets the publish IP address.
     *
     * @param[in]  aIpAddress  The pointer of IP address.
     * @param[in]  aLen        The Length of IP address.
     *
     */
    void SetIpAddress(const char *aIpAddress, uint8_t aLen);

    /**
     * This method starts the mDNS client.
     *
     * @retval kMdnsPublisher_OK                  Successfully started the mDNS client.
     * @retval kMdnsPublisher_FailedCreatePoll    Failed to created the simple poll.
     * @retval kMdnsPublisher_FailedCreateServer  Failed to created the mDNS client.
     *
     */
    int StartClient(void);

    /**
     * This method updates the publishing service.
     *
     * @retval kMdnsPublisher_OK                  Successfully updated the publishing service.
     * @retval kMdnsPublisher_FailedUpdateSevice  Failed to updated the publishing service.
     *
     */
    int UpdateService(void);

    /**
     * This method returns the reference to the mDNS publisher instance.
     *
     * @returns The reference to the mDNS publisher instance.
     *
     */
    static Publisher &GetInstance(void)
    {
        static Publisher publisherInstance;

        return publisherInstance;
    }

    /**
     * This method indicates whether or not the mDNS service is running.
     *
     * @retval true   Successfully started the mdns service.
     * @retval false  Failed to start the mdns service.
     *
     */
    bool IsRunning(void) const { return mIsStarted; }

private:
    Publisher(void);

    static void HandleEntryGroupStart(AvahiEntryGroup *aGroup, AvahiEntryGroupState aState,
                                      AVAHI_GCC_UNUSED void *aUserData);
    void HandleEntryGroupStart(AvahiEntryGroup *aGroup, AvahiEntryGroupState aState);
    static void HandleClientStart(AvahiClient *aClient, AvahiClientState aState,
                                  AVAHI_GCC_UNUSED void *aUserData);
    void HandleClientStart(AvahiClient *aClient, AvahiClientState aState);
    static void HandleServicePublish(AVAHI_GCC_UNUSED AvahiTimeout *aTimeout, AVAHI_GCC_UNUSED void *aUserData);
    void HandleServicePublish(AVAHI_GCC_UNUSED AvahiTimeout *aTimeout);
    int CreateService(AvahiClient *aClient);
    void Free(void);

    AvahiEntryGroup *mClientGroup;
    AvahiSimplePoll *mSimplePoll;
    AvahiClient     *mClient;
    uint16_t         mPort;
    bool             mIsStarted;
    char            *mServiceName;
    char            *mNetworkNameTxt;
    char            *mExtPanIdTxt;
    char            *mType;
    int              mProtoType;
    int              mInterfaceIndex;
    char            *mHostName;
    char            *mIpAddress;
};

} //namespace Mdns
} //namespace ot
#endif  //MDNS_SERVER_HPP
