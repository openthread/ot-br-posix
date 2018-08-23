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

#include <CppUTest/TestHarness.h>

#include "agent/mdns_mdnssd.cpp"

TEST_GROUP(MdnsSd){};

TEST(MdnsSd, TestDNSErrorToString)
{
    CHECK(NULL != ot::BorderRouter::Mdns::DNSErrorToString(kDNSServiceErr_NoError));
    CHECK(NULL != ot::BorderRouter::Mdns::DNSErrorToString(kDNSServiceErr_Unknown));
    CHECK(NULL != ot::BorderRouter::Mdns::DNSErrorToString(kDNSServiceErr_NoSuchName));
    CHECK(NULL != ot::BorderRouter::Mdns::DNSErrorToString(kDNSServiceErr_NoMemory));
    CHECK(NULL != ot::BorderRouter::Mdns::DNSErrorToString(kDNSServiceErr_BadParam));
    CHECK(NULL != ot::BorderRouter::Mdns::DNSErrorToString(kDNSServiceErr_BadReference));
    CHECK(NULL != ot::BorderRouter::Mdns::DNSErrorToString(kDNSServiceErr_BadState));
    CHECK(NULL != ot::BorderRouter::Mdns::DNSErrorToString(kDNSServiceErr_BadFlags));
    CHECK(NULL != ot::BorderRouter::Mdns::DNSErrorToString(kDNSServiceErr_Unsupported));
    CHECK(NULL != ot::BorderRouter::Mdns::DNSErrorToString(kDNSServiceErr_NotInitialized));
    CHECK(NULL != ot::BorderRouter::Mdns::DNSErrorToString(kDNSServiceErr_AlreadyRegistered));
    CHECK(NULL != ot::BorderRouter::Mdns::DNSErrorToString(kDNSServiceErr_NameConflict));
    CHECK(NULL != ot::BorderRouter::Mdns::DNSErrorToString(kDNSServiceErr_Invalid));
    CHECK(NULL != ot::BorderRouter::Mdns::DNSErrorToString(kDNSServiceErr_Firewall));
    CHECK(NULL != ot::BorderRouter::Mdns::DNSErrorToString(kDNSServiceErr_Incompatible));
    CHECK(NULL != ot::BorderRouter::Mdns::DNSErrorToString(kDNSServiceErr_BadInterfaceIndex));
    CHECK(NULL != ot::BorderRouter::Mdns::DNSErrorToString(kDNSServiceErr_Refused));
    CHECK(NULL != ot::BorderRouter::Mdns::DNSErrorToString(kDNSServiceErr_NoSuchRecord));
    CHECK(NULL != ot::BorderRouter::Mdns::DNSErrorToString(kDNSServiceErr_NoAuth));
    CHECK(NULL != ot::BorderRouter::Mdns::DNSErrorToString(kDNSServiceErr_NoSuchKey));
    CHECK(NULL != ot::BorderRouter::Mdns::DNSErrorToString(kDNSServiceErr_NATTraversal));
    CHECK(NULL != ot::BorderRouter::Mdns::DNSErrorToString(kDNSServiceErr_DoubleNAT));
    CHECK(NULL != ot::BorderRouter::Mdns::DNSErrorToString(kDNSServiceErr_BadTime));
    CHECK(NULL != ot::BorderRouter::Mdns::DNSErrorToString(kDNSServiceErr_BadSig));
    CHECK(NULL != ot::BorderRouter::Mdns::DNSErrorToString(kDNSServiceErr_BadKey));
    CHECK(NULL != ot::BorderRouter::Mdns::DNSErrorToString(kDNSServiceErr_Transient));
    CHECK(NULL != ot::BorderRouter::Mdns::DNSErrorToString(kDNSServiceErr_ServiceNotRunning));
    CHECK(NULL != ot::BorderRouter::Mdns::DNSErrorToString(kDNSServiceErr_NATPortMappingUnsupported));
    CHECK(NULL != ot::BorderRouter::Mdns::DNSErrorToString(kDNSServiceErr_NATPortMappingDisabled));
    CHECK(NULL != ot::BorderRouter::Mdns::DNSErrorToString(kDNSServiceErr_NoRouter));
    CHECK(NULL != ot::BorderRouter::Mdns::DNSErrorToString(kDNSServiceErr_PollingMode));
    CHECK(NULL != ot::BorderRouter::Mdns::DNSErrorToString(kDNSServiceErr_Timeout));
}
