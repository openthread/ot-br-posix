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

#ifndef BORDERROUTER_IP_ADDRESS_HPP
#define BORDERROUTER_IP_ADDRESS_HPP

#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>


namespace ot {

namespace BorderRouter {

/**
 * This class implements the get IP address according to the interface name.
 *
 */
class InterfaceAddress
{
public:
    /**
     * This method returns the IPv4 address.
     *
     * @returns the pointer of IPv4 address.
     *
     */
    const char *GetIpv4Address() const;

    /**
     * This method returns the IPv6 address.
     *
     * @returns the pointer of IPv6 address.
     *
     */
    const char *GetIpv6Address() const;

    /**
     * This method returns the index of the interface.
     *
     * @returns the index of the interface.
     *
     */
    int GetInterfaceIndex() const;

    /**
     * This method lookups addresses and interface index according to interface name.
     *
     * @retval kState_Ok       Successfully lookup the interface info
     * @retval kState_NotFound Failed to find the addresses or index to specified interface
     *
     *
     */
    int LookupAddresses(const char *aInterfaceName);

private:
    enum
    {
        kState_Ok       = 0,
        kState_NotFound = -1,
    };

    char mIpv4Addr[20];
    char mIpv6Addr[50];
    int  mInterfaceIndex;
};

} //namespace BorderRouter

} //namespace ot
#endif  //BORDERROUTER_IP_ADDRESS_HPP
