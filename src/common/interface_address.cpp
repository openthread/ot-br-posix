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
#include "interface_address.hpp"

namespace ot {

namespace BorderRouter {

int InterfaceAddress::LookupAddresses(const char *aInterfaceName)
{
    struct ifaddrs *ifaHead, *ifaNext;
    int             index = 0, ret = kState_Ok;
    bool            gotIpv4 = false;
    bool            gotIpv6 = false;

    if (getifaddrs(&ifaHead) == -1)
    {
        ret = kState_NotFound;
        goto exit;
    }
    mInterfaceIndex = 0;
    ifaNext = ifaHead;
    while (ifaNext)
    {
        if (ifaNext->ifa_addr && ifaNext->ifa_addr->sa_family == AF_PACKET)
        {
            index++;
            if (strcmp(ifaNext->ifa_name, aInterfaceName) == 0)
            {
                mInterfaceIndex = index;
            }
        }

        if (strcmp(ifaNext->ifa_name, aInterfaceName) == 0)
        {
            if ((ifaNext->ifa_addr) && (ifaNext->ifa_addr->sa_family == AF_INET) && !gotIpv4)
            {
                struct sockaddr_in *in = (struct sockaddr_in *) ifaNext->ifa_addr;
                inet_ntop(AF_INET, &in->sin_addr, mIpv4Addr, sizeof(mIpv4Addr));
                gotIpv4 = true;
            }
            else if ((ifaNext->ifa_addr) && (ifaNext->ifa_addr->sa_family == AF_INET6) && !gotIpv6)
            {
                struct sockaddr_in6 *in6 = (struct sockaddr_in6 *) ifaNext->ifa_addr;
                inet_ntop(AF_INET6, &in6->sin6_addr, mIpv6Addr, sizeof(mIpv6Addr));
                gotIpv6 = true;
            }
        }

        ifaNext = ifaNext->ifa_next;
    }

    if (mInterfaceIndex == 0)
    {
        ret = kState_NotFound;
    }
exit:
    return ret;
}

const char *InterfaceAddress::GetIpv4Address() const
{
    return mIpv4Addr;
}

const char *InterfaceAddress::GetIpv6Address() const
{
    return mIpv6Addr;
}

int InterfaceAddress::GetInterfaceIndex() const
{
    return mInterfaceIndex;
}

} //namespace BorderRouter

} //namespace ot
