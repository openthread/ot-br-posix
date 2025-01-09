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
 *   This file includes implementation of OpenThread DNS-SD platform APIs on the posix platform.
 */

#define OTBR_LOG_TAG "DNSSD"

#include "host/posix/dnssd.hpp"

#include <string>

#include <openthread/platform/dnssd.h>

#include "common/code_utils.hpp"
#include "common/logging.hpp"
#include "common/types.hpp"

static otbr::DnssdPlatform::RegisterCallback MakeRegisterCallback(otInstance                 *aInstance,
                                                                  otPlatDnssdRegisterCallback aCallback)
{
    return [aInstance, aCallback](otPlatDnssdRequestId aRequestId, otError aError) {
        aCallback(aInstance, aRequestId, aError);
    };
}

extern "C" otPlatDnssdState otPlatDnssdGetState(otInstance *aInstance)
{
    OTBR_UNUSED_VARIABLE(aInstance);

    return otbr::DnssdPlatform::Get().GetState();
}

extern "C" void otPlatDnssdRegisterService(otInstance                 *aInstance,
                                           const otPlatDnssdService   *aService,
                                           otPlatDnssdRequestId        aRequestId,
                                           otPlatDnssdRegisterCallback aCallback)
{
    OTBR_UNUSED_VARIABLE(aInstance);

    otbr::DnssdPlatform::Get().RegisterService(*aService, aRequestId, MakeRegisterCallback(aInstance, aCallback));
}

extern "C" void otPlatDnssdUnregisterService(otInstance                 *aInstance,
                                             const otPlatDnssdService   *aService,
                                             otPlatDnssdRequestId        aRequestId,
                                             otPlatDnssdRegisterCallback aCallback)
{
    OTBR_UNUSED_VARIABLE(aInstance);

    otbr::DnssdPlatform::Get().UnregisterService(*aService, aRequestId, MakeRegisterCallback(aInstance, aCallback));
}

extern "C" void otPlatDnssdRegisterHost(otInstance                 *aInstance,
                                        const otPlatDnssdHost      *aHost,
                                        otPlatDnssdRequestId        aRequestId,
                                        otPlatDnssdRegisterCallback aCallback)
{
    OTBR_UNUSED_VARIABLE(aInstance);

    otbr::DnssdPlatform::Get().RegisterHost(*aHost, aRequestId, MakeRegisterCallback(aInstance, aCallback));
}

extern "C" void otPlatDnssdUnregisterHost(otInstance                 *aInstance,
                                          const otPlatDnssdHost      *aHost,
                                          otPlatDnssdRequestId        aRequestId,
                                          otPlatDnssdRegisterCallback aCallback)
{
    OTBR_UNUSED_VARIABLE(aInstance);

    otbr::DnssdPlatform::Get().UnregisterHost(*aHost, aRequestId, MakeRegisterCallback(aInstance, aCallback));
}

extern "C" void otPlatDnssdRegisterKey(otInstance                 *aInstance,
                                       const otPlatDnssdKey       *aKey,
                                       otPlatDnssdRequestId        aRequestId,
                                       otPlatDnssdRegisterCallback aCallback)
{
    OTBR_UNUSED_VARIABLE(aInstance);

    otbr::DnssdPlatform::Get().RegisterKey(*aKey, aRequestId, MakeRegisterCallback(aInstance, aCallback));
}

extern "C" void otPlatDnssdUnregisterKey(otInstance                 *aInstance,
                                         const otPlatDnssdKey       *aKey,
                                         otPlatDnssdRequestId        aRequestId,
                                         otPlatDnssdRegisterCallback aCallback)
{
    OTBR_UNUSED_VARIABLE(aInstance);

    otbr::DnssdPlatform::Get().UnregisterKey(*aKey, aRequestId, MakeRegisterCallback(aInstance, aCallback));
}

extern "C" void otPlatDnssdStartBrowser(otInstance *aInstance, const otPlatDnssdBrowser *aBrowser)
{
    OTBR_UNUSED_VARIABLE(aInstance);
    OTBR_UNUSED_VARIABLE(aBrowser);
}

extern "C" void otPlatDnssdStopBrowser(otInstance *aInstance, const otPlatDnssdBrowser *aBrowser)
{
    OTBR_UNUSED_VARIABLE(aInstance);
    OTBR_UNUSED_VARIABLE(aBrowser);
}

extern "C" void otPlatDnssdStartSrvResolver(otInstance *aInstance, const otPlatDnssdSrvResolver *aResolver)
{
    OTBR_UNUSED_VARIABLE(aInstance);
    OTBR_UNUSED_VARIABLE(aResolver);
}

extern "C" void otPlatDnssdStopSrvResolver(otInstance *aInstance, const otPlatDnssdSrvResolver *aResolver)
{
    OTBR_UNUSED_VARIABLE(aInstance);
    OTBR_UNUSED_VARIABLE(aResolver);
}

extern "C" void otPlatDnssdStartTxtResolver(otInstance *aInstance, const otPlatDnssdTxtResolver *aResolver)
{
    OTBR_UNUSED_VARIABLE(aInstance);
    OTBR_UNUSED_VARIABLE(aResolver);
}

extern "C" void otPlatDnssdStopTxtResolver(otInstance *aInstance, const otPlatDnssdTxtResolver *aResolver)
{
    OTBR_UNUSED_VARIABLE(aInstance);
    OTBR_UNUSED_VARIABLE(aResolver);
}

extern "C" void otPlatDnssdStartIp6AddressResolver(otInstance *aInstance, const otPlatDnssdAddressResolver *aResolver)
{
    OTBR_UNUSED_VARIABLE(aInstance);
    OTBR_UNUSED_VARIABLE(aResolver);
}

extern "C" void otPlatDnssdStopIp6AddressResolver(otInstance *aInstance, const otPlatDnssdAddressResolver *aResolver)
{
    OTBR_UNUSED_VARIABLE(aInstance);
    OTBR_UNUSED_VARIABLE(aResolver);
}

void otPlatDnssdStartIp4AddressResolver(otInstance *aInstance, const otPlatDnssdAddressResolver *aResolver)
{
    OTBR_UNUSED_VARIABLE(aInstance);
    OTBR_UNUSED_VARIABLE(aResolver);
}

void otPlatDnssdStopIp4AddressResolver(otInstance *aInstance, const otPlatDnssdAddressResolver *aResolver)
{
    OTBR_UNUSED_VARIABLE(aInstance);
    OTBR_UNUSED_VARIABLE(aResolver);
}

//----------------------------------------------------------------------------------------------------------------------

namespace otbr {

DnssdPlatform *DnssdPlatform::sDnssdPlatform = nullptr;

DnssdPlatform::DnssdPlatform(Mdns::Publisher &aPublisher)
    : mPublisher(aPublisher)
    , mState(kStateStopped)
    , mRunning(false)
    , mPublisherState(Mdns::Publisher::State::kIdle)
{
    sDnssdPlatform = this;
}

void DnssdPlatform::Start(void)
{
    if (!mRunning)
    {
        mRunning = true;
        UpdateState();
    }
}

void DnssdPlatform::Stop(void)
{
    if (mRunning)
    {
        mRunning = false;
        UpdateState();
    }
}

void DnssdPlatform::UpdateState(void)
{
    if (mRunning && (mPublisherState == Mdns::Publisher::State::kReady))
    {
        VerifyOrExit(mState != kStateReady);

        mState = kStateReady;
    }
    else
    {
        VerifyOrExit(mState != kStateStopped);

        mState = kStateStopped;
    }

    if (mStateChangeCallback)
    {
        mStateChangeCallback(mState);
    }

exit:
    return;
}

Mdns::Publisher::ResultCallback DnssdPlatform::MakePublisherCallback(RequestId aRequestId, RegisterCallback aCallback)
{
    return [aRequestId, aCallback](otbrError aError) {
        if (aCallback != nullptr)
        {
            aCallback(aRequestId, OtbrErrorToOtError(aError));
        }
    };
}

void DnssdPlatform::SetDnssdStateChangedCallback(DnssdStateChangeCallback aCallback)
{
    mStateChangeCallback = aCallback;
}

void DnssdPlatform::RegisterService(const Service &aService, RequestId aRequestId, RegisterCallback aCallback)
{
    Mdns::Publisher::SubTypeList subTypeList;
    Mdns::Publisher::TxtData     txtData(aService.mTxtData, aService.mTxtData + aService.mTxtDataLength);

    for (uint16_t index = 0; index < aService.mSubTypeLabelsLength; index++)
    {
        subTypeList.push_back(aService.mSubTypeLabels[index]);
    }

    mPublisher.PublishService(aService.mHostName, aService.mServiceInstance, aService.mServiceType, subTypeList,
                              aService.mPort, txtData, MakePublisherCallback(aRequestId, aCallback));
}

void DnssdPlatform::UnregisterService(const Service &aService, RequestId aRequestId, RegisterCallback aCallback)
{
    mPublisher.UnpublishService(aService.mServiceInstance, aService.mServiceType,
                                MakePublisherCallback(aRequestId, aCallback));
}

void DnssdPlatform::RegisterHost(const Host &aHost, RequestId aRequestId, RegisterCallback aCallback)
{
    Mdns::Publisher::AddressList addressList;

    for (uint16_t index = 0; index < aHost.mAddressesLength; index++)
    {
        addressList.push_back(Ip6Address(aHost.mAddresses[index].mFields.m8));
    }

    mPublisher.PublishHost(aHost.mHostName, addressList, MakePublisherCallback(aRequestId, aCallback));
}

void DnssdPlatform::UnregisterHost(const Host &aHost, RequestId aRequestId, RegisterCallback aCallback)
{
    mPublisher.UnpublishHost(aHost.mHostName, MakePublisherCallback(aRequestId, aCallback));
}

std::string DnssdPlatform::KeyNameFor(const Key &aKey)
{
    std::string name(aKey.mName);

    if (aKey.mServiceType != nullptr)
    {
        // TODO: current code would not work with service instance labels that include a '.'
        name += ".";
        name += aKey.mServiceType;
    }

    return name;
}

void DnssdPlatform::RegisterKey(const Key &aKey, RequestId aRequestId, RegisterCallback aCallback)
{
    Mdns::Publisher::KeyData keyData(aKey.mKeyData, aKey.mKeyData + aKey.mKeyDataLength);

    mPublisher.PublishKey(KeyNameFor(aKey), keyData, MakePublisherCallback(aRequestId, aCallback));
}

void DnssdPlatform::UnregisterKey(const Key &aKey, RequestId aRequestId, RegisterCallback aCallback)
{
    mPublisher.UnpublishKey(KeyNameFor(aKey), MakePublisherCallback(aRequestId, aCallback));
}

void DnssdPlatform::HandleMdnsState(Mdns::Publisher::State aState)
{
    if (mPublisherState != aState)
    {
        mPublisherState = aState;
        UpdateState();
    }
}

} // namespace otbr
