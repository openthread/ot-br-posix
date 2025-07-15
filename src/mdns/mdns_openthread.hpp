/*
 *    Copyright (c) 2025, The OpenThread Authors.
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
 *   This file includes definition for mDNS publisher.
 */

#ifndef OTBR_AGENT_MDNS_OPENTHREAD_HPP_
#define OTBR_AGENT_MDNS_OPENTHREAD_HPP_

#include "openthread-br/config.h"

#include "common/types.hpp"
#include "mdns/mdns.hpp"

namespace otbr {
namespace Mdns {

/**
 * This class implements mDNS publisher with OpenThread.
 */
class PublisherOpenThread : public Publisher
{
public:
    PublisherOpenThread(void);
    ~PublisherOpenThread(void) override;

    void      UnpublishService(const std::string &aName, const std::string &aType, ResultCallback &&aCallback) override;
    void      UnpublishHost(const std::string &aName, ResultCallback &&aCallback) override;
    void      UnpublishKey(const std::string &aName, ResultCallback &&aCallback) override;
    void      SubscribeService(const std::string &aType, const std::string &aInstanceName) override;
    void      UnsubscribeService(const std::string &aType, const std::string &aInstanceName) override;
    void      SubscribeHost(const std::string &aHostName) override;
    void      UnsubscribeHost(const std::string &aHostName) override;
    otbrError Start(void) override;
    bool      IsStarted(void) const override;
    void      Stop(void) override;

protected:
    otbrError PublishServiceImpl(const std::string &aHostName,
                                 const std::string &aName,
                                 const std::string &aType,
                                 const SubTypeList &aSubTypeList,
                                 uint16_t           aPort,
                                 const TxtData     &aTxtData,
                                 ResultCallback   &&aCallback) override;
    otbrError PublishHostImpl(const std::string &aName,
                              const AddressList &aAddresses,
                              ResultCallback   &&aCallback) override;
    otbrError PublishKeyImpl(const std::string &aName, const KeyData &aKeyData, ResultCallback &&aCallback) override;
    void      OnServiceResolveFailedImpl(const std::string &aType,
                                         const std::string &aInstanceName,
                                         int32_t            aErrorCode) override;
    void      OnHostResolveFailedImpl(const std::string &aHostName, int32_t aErrorCode) override;
    otbrError DnsErrorToOtbrError(int32_t aErrorCode) override;
};

} // namespace Mdns
} // namespace otbr

#endif // OTBR_AGENT_MDNS_OPENTHREAD_HPP_
