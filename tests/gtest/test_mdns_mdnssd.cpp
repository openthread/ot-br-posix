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
    EXPECT_TRUE(nullptr != otbr::Mdns::DNSErrorToString(kDNSServiceErr_NoError));
    EXPECT_TRUE(nullptr != otbr::Mdns::DNSErrorToString(kDNSServiceErr_Unknown));
    EXPECT_TRUE(nullptr != otbr::Mdns::DNSErrorToString(kDNSServiceErr_NoSuchName));
    EXPECT_TRUE(nullptr != otbr::Mdns::DNSErrorToString(kDNSServiceErr_NoMemory));
    EXPECT_TRUE(nullptr != otbr::Mdns::DNSErrorToString(kDNSServiceErr_BadParam));
    EXPECT_TRUE(nullptr != otbr::Mdns::DNSErrorToString(kDNSServiceErr_BadReference));
    EXPECT_TRUE(nullptr != otbr::Mdns::DNSErrorToString(kDNSServiceErr_BadState));
    EXPECT_TRUE(nullptr != otbr::Mdns::DNSErrorToString(kDNSServiceErr_BadFlags));
    EXPECT_TRUE(nullptr != otbr::Mdns::DNSErrorToString(kDNSServiceErr_Unsupported));
    EXPECT_TRUE(nullptr != otbr::Mdns::DNSErrorToString(kDNSServiceErr_NotInitialized));
    EXPECT_TRUE(nullptr != otbr::Mdns::DNSErrorToString(kDNSServiceErr_AlreadyRegistered));
    EXPECT_TRUE(nullptr != otbr::Mdns::DNSErrorToString(kDNSServiceErr_NameConflict));
    EXPECT_TRUE(nullptr != otbr::Mdns::DNSErrorToString(kDNSServiceErr_Invalid));
    EXPECT_TRUE(nullptr != otbr::Mdns::DNSErrorToString(kDNSServiceErr_Firewall));
    EXPECT_TRUE(nullptr != otbr::Mdns::DNSErrorToString(kDNSServiceErr_Incompatible));
    EXPECT_TRUE(nullptr != otbr::Mdns::DNSErrorToString(kDNSServiceErr_BadInterfaceIndex));
    EXPECT_TRUE(nullptr != otbr::Mdns::DNSErrorToString(kDNSServiceErr_Refused));
    EXPECT_TRUE(nullptr != otbr::Mdns::DNSErrorToString(kDNSServiceErr_NoSuchRecord));
    EXPECT_TRUE(nullptr != otbr::Mdns::DNSErrorToString(kDNSServiceErr_NoAuth));
    EXPECT_TRUE(nullptr != otbr::Mdns::DNSErrorToString(kDNSServiceErr_NoSuchKey));
    EXPECT_TRUE(nullptr != otbr::Mdns::DNSErrorToString(kDNSServiceErr_NATTraversal));
    EXPECT_TRUE(nullptr != otbr::Mdns::DNSErrorToString(kDNSServiceErr_DoubleNAT));
    EXPECT_TRUE(nullptr != otbr::Mdns::DNSErrorToString(kDNSServiceErr_BadTime));
    EXPECT_TRUE(nullptr != otbr::Mdns::DNSErrorToString(kDNSServiceErr_BadSig));
    EXPECT_TRUE(nullptr != otbr::Mdns::DNSErrorToString(kDNSServiceErr_BadKey));
    EXPECT_TRUE(nullptr != otbr::Mdns::DNSErrorToString(kDNSServiceErr_Transient));
    EXPECT_TRUE(nullptr != otbr::Mdns::DNSErrorToString(kDNSServiceErr_ServiceNotRunning));
    EXPECT_TRUE(nullptr != otbr::Mdns::DNSErrorToString(kDNSServiceErr_NATPortMappingUnsupported));
    EXPECT_TRUE(nullptr != otbr::Mdns::DNSErrorToString(kDNSServiceErr_NATPortMappingDisabled));
    EXPECT_TRUE(nullptr != otbr::Mdns::DNSErrorToString(kDNSServiceErr_NoRouter));
    EXPECT_TRUE(nullptr != otbr::Mdns::DNSErrorToString(kDNSServiceErr_PollingMode));
    EXPECT_TRUE(nullptr != otbr::Mdns::DNSErrorToString(kDNSServiceErr_Timeout));
}
