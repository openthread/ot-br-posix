/*
 *    Copyright (c) 2026, The OpenThread Authors.
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
 *   A mock of the mDNS publisher for the tests of its callers.
 */

#ifndef OTBR_TEST_MOCK_MDNS_PUBLISHER_HPP_
#define OTBR_TEST_MOCK_MDNS_PUBLISHER_HPP_

#include <gmock/gmock.h>

#include "common/code_utils.hpp"
#include "mdns/mdns.hpp"

class MockMdnsPublisher : public otbr::Mdns::Publisher
{
public:
    MockMdnsPublisher(void)           = default;
    ~MockMdnsPublisher(void) override = default;

    MOCK_METHOD(otbrError,
                PublishServiceImpl,
                (const std::string &aHostName,
                 const std::string &aName,
                 const std::string &aType,
                 const SubTypeList &aSubTypeList,
                 uint16_t           aPort,
                 const TxtData     &aTxtData,
                 ResultCallback   &&aCallback),
                (override));
    MOCK_METHOD(void,
                UnpublishService,
                (const std::string &aName, const std::string &aType, ResultCallback &&aCallback),
                (override));
    MOCK_METHOD(otbrError,
                PublishHostImpl,
                (const std::string &aName, const AddressList &aAddresses, ResultCallback &&aCallback),
                (override));
    MOCK_METHOD(void, UnpublishHost, (const std::string &aName, ResultCallback &&aCallback), (override));
    MOCK_METHOD(otbrError,
                PublishKeyImpl,
                (const std::string &aName, const KeyData &aKey, ResultCallback &&aCallback),
                (override));
    MOCK_METHOD(void, UnpublishKey, (const std::string &aName, ResultCallback &&aCallback), (override));
    MOCK_METHOD(void,
                SubscribeService,
                (const std::string &aType, const std::string &aInstanceName, uint32_t aNetifIndex),
                (override));
    MOCK_METHOD(void,
                UnsubscribeService,
                (const std::string &aType, const std::string &aInstanceName, uint32_t aNetifIndex),
                (override));
    MOCK_METHOD(void, SubscribeHost, (const std::string &aHostName, uint32_t aNetifIndex), (override));
    MOCK_METHOD(void, UnsubscribeHost, (const std::string &aHostName, uint32_t aNetifIndex), (override));

    otbrError Start(void) override { return OTBR_ERROR_NONE; }
    void      Stop(void) override {}
    bool      IsStarted(void) const override { return true; }

    void OnServiceResolveFailedImpl(const std::string &aType,
                                    const std::string &aInstanceName,
                                    int32_t            aErrorCode) override
    {
        OTBR_UNUSED_VARIABLE(aType);
        OTBR_UNUSED_VARIABLE(aInstanceName);
        OTBR_UNUSED_VARIABLE(aErrorCode);
    }

    void OnHostResolveFailedImpl(const std::string &aHostName, int32_t aErrorCode) override
    {
        OTBR_UNUSED_VARIABLE(aHostName);
        OTBR_UNUSED_VARIABLE(aErrorCode);
    }

    otbrError DnsErrorToOtbrError(int32_t aError) override
    {
        OTBR_UNUSED_VARIABLE(aError);
        return OTBR_ERROR_NONE;
    }

    void TestOnServiceResolved(std::string aType, otbr::Mdns::Publisher::DiscoveredInstanceInfo aInstanceInfo)
    {
        OnServiceResolved(std::move(aType), std::move(aInstanceInfo));
    }

    void TestOnHostResolved(std::string aHostName, otbr::Mdns::Publisher::DiscoveredHostInfo aHostInfo)
    {
        OnHostResolved(std::move(aHostName), std::move(aHostInfo));
    }
};

#endif // OTBR_TEST_MOCK_MDNS_PUBLISHER_HPP_
