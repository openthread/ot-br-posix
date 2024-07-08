/*
 *    Copyright (c) 2018, The OpenThread Authors.
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

#include <gtest/gtest.h>

#include "mdns/mdns_mdnssd.cpp"

TEST(MdnsSd, TestDNSErrorToString)
{
    EXPECT_NE(otbr::Mdns::DNSErrorToString(kDNSServiceErr_NoError), nullptr);
    EXPECT_NE(otbr::Mdns::DNSErrorToString(kDNSServiceErr_Unknown), nullptr);
    EXPECT_NE(otbr::Mdns::DNSErrorToString(kDNSServiceErr_NoSuchName), nullptr);
    EXPECT_NE(otbr::Mdns::DNSErrorToString(kDNSServiceErr_NoMemory), nullptr);
    EXPECT_NE(otbr::Mdns::DNSErrorToString(kDNSServiceErr_BadParam), nullptr);
    EXPECT_NE(otbr::Mdns::DNSErrorToString(kDNSServiceErr_BadReference), nullptr);
    EXPECT_NE(otbr::Mdns::DNSErrorToString(kDNSServiceErr_BadState), nullptr);
    EXPECT_NE(otbr::Mdns::DNSErrorToString(kDNSServiceErr_BadFlags), nullptr);
    EXPECT_NE(otbr::Mdns::DNSErrorToString(kDNSServiceErr_Unsupported), nullptr);
    EXPECT_NE(otbr::Mdns::DNSErrorToString(kDNSServiceErr_NotInitialized), nullptr);
    EXPECT_NE(otbr::Mdns::DNSErrorToString(kDNSServiceErr_AlreadyRegistered), nullptr);
    EXPECT_NE(otbr::Mdns::DNSErrorToString(kDNSServiceErr_NameConflict), nullptr);
    EXPECT_NE(otbr::Mdns::DNSErrorToString(kDNSServiceErr_Invalid), nullptr);
    EXPECT_NE(otbr::Mdns::DNSErrorToString(kDNSServiceErr_Firewall), nullptr);
    EXPECT_NE(otbr::Mdns::DNSErrorToString(kDNSServiceErr_Incompatible), nullptr);
    EXPECT_NE(otbr::Mdns::DNSErrorToString(kDNSServiceErr_BadInterfaceIndex), nullptr);
    EXPECT_NE(otbr::Mdns::DNSErrorToString(kDNSServiceErr_Refused), nullptr);
    EXPECT_NE(otbr::Mdns::DNSErrorToString(kDNSServiceErr_NoSuchRecord), nullptr);
    EXPECT_NE(otbr::Mdns::DNSErrorToString(kDNSServiceErr_NoAuth), nullptr);
    EXPECT_NE(otbr::Mdns::DNSErrorToString(kDNSServiceErr_NoSuchKey), nullptr);
    EXPECT_NE(otbr::Mdns::DNSErrorToString(kDNSServiceErr_NATTraversal), nullptr);
    EXPECT_NE(otbr::Mdns::DNSErrorToString(kDNSServiceErr_DoubleNAT), nullptr);
    EXPECT_NE(otbr::Mdns::DNSErrorToString(kDNSServiceErr_BadTime), nullptr);
    EXPECT_NE(otbr::Mdns::DNSErrorToString(kDNSServiceErr_BadSig), nullptr);
    EXPECT_NE(otbr::Mdns::DNSErrorToString(kDNSServiceErr_BadKey), nullptr);
    EXPECT_NE(otbr::Mdns::DNSErrorToString(kDNSServiceErr_Transient), nullptr);
    EXPECT_NE(otbr::Mdns::DNSErrorToString(kDNSServiceErr_ServiceNotRunning), nullptr);
    EXPECT_NE(otbr::Mdns::DNSErrorToString(kDNSServiceErr_NATPortMappingUnsupported), nullptr);
    EXPECT_NE(otbr::Mdns::DNSErrorToString(kDNSServiceErr_NATPortMappingDisabled), nullptr);
    EXPECT_NE(otbr::Mdns::DNSErrorToString(kDNSServiceErr_NoRouter), nullptr);
    EXPECT_NE(otbr::Mdns::DNSErrorToString(kDNSServiceErr_PollingMode), nullptr);
    EXPECT_NE(otbr::Mdns::DNSErrorToString(kDNSServiceErr_Timeout), nullptr);
}
