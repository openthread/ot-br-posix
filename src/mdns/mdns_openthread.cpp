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
 *   This file implements mDNS publisher based on OpenThread.
 */

#define OTBR_LOG_TAG "MDNS"

#include "mdns/mdns_openthread.hpp"

#include "common/code_utils.hpp"

namespace otbr {
namespace Mdns {

PublisherOpenThread::PublisherOpenThread(void)
{
}

PublisherOpenThread::~PublisherOpenThread(void)
{
}

otbrError PublisherOpenThread::Start(void)
{
    return OTBR_ERROR_NONE;
}

bool PublisherOpenThread::IsStarted(void) const
{
    return true;
}

void PublisherOpenThread::Stop(void)
{
    return;
}

void PublisherOpenThread::UnpublishService(const std::string &aName,
                                           const std::string &aType,
                                           ResultCallback   &&aCallback)
{
    OTBR_UNUSED_VARIABLE(aName);
    OTBR_UNUSED_VARIABLE(aType);
    OTBR_UNUSED_VARIABLE(aCallback);
    DieNow("UnpublishService is not implemented.");
}

void PublisherOpenThread::UnpublishHost(const std::string &aName, ResultCallback &&aCallback)
{
    OTBR_UNUSED_VARIABLE(aName);
    OTBR_UNUSED_VARIABLE(aCallback);
    DieNow("UnpublishHost is not implemented.");
}

void PublisherOpenThread::UnpublishKey(const std::string &aName, ResultCallback &&aCallback)
{
    OTBR_UNUSED_VARIABLE(aName);
    OTBR_UNUSED_VARIABLE(aCallback);
    DieNow("UnpublishKey is not implemented.");
}

void PublisherOpenThread::SubscribeService(const std::string &aType, const std::string &aInstanceName)
{
    OTBR_UNUSED_VARIABLE(aType);
    OTBR_UNUSED_VARIABLE(aInstanceName);
    DieNow("SubscribeService is not implemented.");
}

void PublisherOpenThread::UnsubscribeService(const std::string &aType, const std::string &aInstanceName)
{
    OTBR_UNUSED_VARIABLE(aType);
    OTBR_UNUSED_VARIABLE(aInstanceName);
    DieNow("UnsubscribeService is not implemented.");
}

void PublisherOpenThread::SubscribeHost(const std::string &aHostName)
{
    OTBR_UNUSED_VARIABLE(aHostName);
    DieNow("SubscribeHost is not implemented.");
}

void PublisherOpenThread::UnsubscribeHost(const std::string &aHostName)
{
    OTBR_UNUSED_VARIABLE(aHostName);
    DieNow("UnsubscribeHost is not implemented.");
}

otbrError PublisherOpenThread::PublishServiceImpl(const std::string &aHostName,
                                                  const std::string &aName,
                                                  const std::string &aType,
                                                  const SubTypeList &aSubTypeList,
                                                  uint16_t           aPort,
                                                  const TxtData     &aTxtData,
                                                  ResultCallback   &&aCallback)
{
    OTBR_UNUSED_VARIABLE(aHostName);
    OTBR_UNUSED_VARIABLE(aName);
    OTBR_UNUSED_VARIABLE(aType);
    OTBR_UNUSED_VARIABLE(aSubTypeList);
    OTBR_UNUSED_VARIABLE(aPort);
    OTBR_UNUSED_VARIABLE(aTxtData);
    OTBR_UNUSED_VARIABLE(aCallback);
    DieNow("PublishServiceImpl is not implemented.");
}

otbrError PublisherOpenThread::PublishHostImpl(const std::string &aName,
                                               const AddressList &aAddresses,
                                               ResultCallback   &&aCallback)
{
    OTBR_UNUSED_VARIABLE(aName);
    OTBR_UNUSED_VARIABLE(aAddresses);
    OTBR_UNUSED_VARIABLE(aCallback);
    DieNow("PublishHostImpl is not implemented.");
}

otbrError PublisherOpenThread::PublishKeyImpl(const std::string &aName,
                                              const KeyData     &aKeyData,
                                              ResultCallback   &&aCallback)
{
    OTBR_UNUSED_VARIABLE(aName);
    OTBR_UNUSED_VARIABLE(aKeyData);
    OTBR_UNUSED_VARIABLE(aCallback);
    DieNow("PublishKeyImpl is not implemented.");
}

void PublisherOpenThread::OnServiceResolveFailedImpl(const std::string &aType,
                                                     const std::string &aInstanceName,
                                                     int32_t            aErrorCode)
{
    OTBR_UNUSED_VARIABLE(aType);
    OTBR_UNUSED_VARIABLE(aInstanceName);
    OTBR_UNUSED_VARIABLE(aErrorCode);
    DieNow("OnServiceResolveFailedImpl is not implemented.");
}

void PublisherOpenThread::OnHostResolveFailedImpl(const std::string &aHostName, int32_t aErrorCode)
{
    OTBR_UNUSED_VARIABLE(aHostName);
    OTBR_UNUSED_VARIABLE(aErrorCode);
    DieNow("OnHostResolveFailedImpl is not implemented.");
}

otbrError PublisherOpenThread::DnsErrorToOtbrError(int32_t aErrorCode)
{
    OTBR_UNUSED_VARIABLE(aErrorCode);
    DieNow("DnsErrorToOtbrError is not implemented.");
}

Publisher *Publisher::Create(StateCallback aCallback)
{
    OTBR_UNUSED_VARIABLE(aCallback);

    return new PublisherOpenThread();
}

void Publisher::Destroy(Publisher *aPublisher)
{
    delete static_cast<PublisherOpenThread *>(aPublisher);
}

} // namespace Mdns
} // namespace otbr
