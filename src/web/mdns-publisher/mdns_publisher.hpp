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
 *   This file implements starting mDNS server, and publish border router service.
 */

#ifndef MDNS_SERVER_HPP
#define MDNS_SERVER_HPP

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <syslog.h>
#include <time.h>

#include <avahi-core/core.h>
#include <avahi-core/publish.h>
#include <avahi-common/simple-watch.h>
#include <avahi-common/malloc.h>
#include <avahi-common/alternative.h>
#include <avahi-common/error.h>

namespace ot {
namespace Mdns {

class Publisher
{
public:
    static int StartServer(void);
    static void SetServiceName(const char *aServiceName);
    static void SetNetworkNameTxT(const char *aTxtData);
    static void SetEPANIDTxT(const char *aTxtData);
    static void SetType(const char *aType);
    static void SetPort(uint16_t aPort);
    static int UpdateService(void);
    static bool IsServerStart(void) { return isStart; };

private:
    static int CreateService(AvahiServer *aServer);
    static void EntryGroupCallback(AvahiServer *aServer, AvahiSEntryGroup *aGroup,
                                   AvahiEntryGroupState aState, AVAHI_GCC_UNUSED void *aUserData);
    static void ServerCallback(AvahiServer *aServer, AvahiServerState aState,
                               AVAHI_GCC_UNUSED void *aUserData);
    static void PeriodicallyPublish(AVAHI_GCC_UNUSED AvahiTimeout *aTimeout, void *aUserData);
    static void Free(void);

    static AvahiSEntryGroup *mGroup;
    static AvahiSimplePoll  *mSimplePoll;
    static AvahiServer      *mServer;
    static const char       *mNetworkNameTxt;
    static const char       *mExtPanIdTxt;
    static const char       *mType;
    static uint16_t          mPort;
    static char             *mServiceName;
    static bool              isStart;

};

} //namespace Mdns
} //namespace ot
#endif  //MDNS_SERVER_HPP
