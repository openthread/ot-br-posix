/*
 *    Copyright (c) 2023, The OpenThread Authors.
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
 *   This file includes definitions for implementing OpenThread DNS-SD platform APIs.
 */

#define OTBR_LOG_TAG "DnssdPlat"

#include "mdns/dnssd_plat.hpp"

#include <openthread/platform/toolchain.h>

#include "common/code_utils.hpp"
#include "utils/dns_utils.hpp"

#if OTBR_ENABLE_DNSSD_PLAT

static otbr::DnssdPlatform *sDnssdPlatform = nullptr;

//----------------------------------------------------------------------------------------------------------------------
// otPlatDnssd APIs

extern "C" otPlatDnssdState otPlatDnssdGetState(otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);

    return sDnssdPlatform->GetState();
}

extern "C" void otPlatDnssdRegisterService(otInstance                 *aInstance,
                                           const otPlatDnssdService   *aService,
                                           otPlatDnssdRequestId        aRequestId,
                                           otPlatDnssdRegisterCallback aCallback)
{
    OT_UNUSED_VARIABLE(aInstance);
    sDnssdPlatform->RegisterService(*aService, aRequestId, aCallback);
}

extern "C" void otPlatDnssdUnregisterService(otInstance                 *aInstance,
                                             const otPlatDnssdService   *aService,
                                             otPlatDnssdRequestId        aRequestId,
                                             otPlatDnssdRegisterCallback aCallback)
{
    OT_UNUSED_VARIABLE(aInstance);
    sDnssdPlatform->UnregisterService(*aService, aRequestId, aCallback);
}

extern "C" void otPlatDnssdRegisterHost(otInstance                 *aInstance,
                                        const otPlatDnssdHost      *aHost,
                                        otPlatDnssdRequestId        aRequestId,
                                        otPlatDnssdRegisterCallback aCallback)
{
    OT_UNUSED_VARIABLE(aInstance);
    sDnssdPlatform->RegisterHost(*aHost, aRequestId, aCallback);
}

extern "C" void otPlatDnssdUnregisterHost(otInstance                 *aInstance,
                                          const otPlatDnssdHost      *aHost,
                                          otPlatDnssdRequestId        aRequestId,
                                          otPlatDnssdRegisterCallback aCallback)
{
    OT_UNUSED_VARIABLE(aInstance);
    sDnssdPlatform->UnregisterHost(*aHost, aRequestId, aCallback);
}

extern "C" void otPlatDnssdRegisterKey(otInstance                 *aInstance,
                                       const otPlatDnssdKey       *aKey,
                                       otPlatDnssdRequestId        aRequestId,
                                       otPlatDnssdRegisterCallback aCallback)
{
    OT_UNUSED_VARIABLE(aInstance);
    sDnssdPlatform->RegisterKey(*aKey, aRequestId, aCallback);
}

extern "C" void otPlatDnssdUnregisterKey(otInstance                 *aInstance,
                                         const otPlatDnssdKey       *aKey,
                                         otPlatDnssdRequestId        aRequestId,
                                         otPlatDnssdRegisterCallback aCallback)
{
    OT_UNUSED_VARIABLE(aInstance);
    sDnssdPlatform->UnregisterKey(*aKey, aRequestId, aCallback);
}

// This is a temporary config to allow us to build/test (till the PRs in OT which define these APIs are merged).
#define OTBR_DNSSD_ADD_BROWSER_RESOLVER_APIS 1

#if OTBR_DNSSD_ADD_BROWSER_RESOLVER_APIS

extern "C" void otPlatDnssdStartServiceBrowser(otInstance *aInstance, const char *aServiceType, uint32_t aInfraIfIndex)
{
    OT_UNUSED_VARIABLE(aInstance);
    OT_UNUSED_VARIABLE(aServiceType);
    OT_UNUSED_VARIABLE(aInfraIfIndex);
}

extern "C" void otPlatDnssdStopServiceBrowser(otInstance *aInstance, const char *aServiceType, uint32_t aInfraIfIndex)
{
    OT_UNUSED_VARIABLE(aInstance);
    OT_UNUSED_VARIABLE(aServiceType);
    OT_UNUSED_VARIABLE(aInfraIfIndex);
}

extern "C" void otPlatDnssdStartServiceResolver(otInstance                       *aInstance,
                                                const otPlatDnssdServiceInstance *aServiceInstance)
{
    OT_UNUSED_VARIABLE(aInstance);
    OT_UNUSED_VARIABLE(aServiceInstance);
}

extern "C" void otPlatDnssdStopServiceResolver(otInstance                       *aInstance,
                                               const otPlatDnssdServiceInstance *aServiceInstance)
{
    OT_UNUSED_VARIABLE(aInstance);
    OT_UNUSED_VARIABLE(aServiceInstance);
}

extern "C" void otPlatDnssdStartIp6AddressResolver(otInstance *aInstance, const char *aHostName, uint32_t aInfraIfIndex)
{
    OT_UNUSED_VARIABLE(aInstance);
    OT_UNUSED_VARIABLE(aHostName);
    OT_UNUSED_VARIABLE(aInfraIfIndex);
}

extern "C" void otPlatDnssdStopIp6AddressResolver(otInstance *aInstance, const char *aHostName, uint32_t aInfraIfIndex)
{
    OT_UNUSED_VARIABLE(aInstance);
    OT_UNUSED_VARIABLE(aHostName);
    OT_UNUSED_VARIABLE(aInfraIfIndex);
}

extern "C" void otPlatDnssdStartIp4AddressResolver(otInstance *aInstance, const char *aHostName, uint32_t aInfraIfIndex)
{
    OT_UNUSED_VARIABLE(aInstance);
    OT_UNUSED_VARIABLE(aHostName);
    OT_UNUSED_VARIABLE(aInfraIfIndex);
}

extern "C" void otPlatDnssdStopIp4AddressResolver(otInstance *aInstance, const char *aHostName, uint32_t aInfraIfIndex)
{
    OT_UNUSED_VARIABLE(aInstance);
    OT_UNUSED_VARIABLE(aHostName);
    OT_UNUSED_VARIABLE(aInfraIfIndex);
}

#endif // OTBR_DNSSD_ADD_BROWSER_RESOLVER_APIS

//----------------------------------------------------------------------------------------------------------------------

namespace otbr {

DnssdPlatform::DnssdPlatform(Ncp::ControllerOpenThread &aNcp, Mdns::Publisher &aPublisher)
    : mNcp(aNcp)
    , mPublisher(aPublisher)
    , mState(OT_PLAT_DNSSD_STOPPED)
{
    sDnssdPlatform = this;
}

void DnssdPlatform::Start(void)
{
}

void DnssdPlatform::Stop(void)
{
    HandleMdnsPublisherStateChange(Mdns::Publisher::State::kIdle);
}

otError DnssdPlatform::ResultToError(otbrError aOtbrError)
{
    otError error = OT_ERROR_FAILED;

    switch (aOtbrError)
    {
    case OTBR_ERROR_NONE:
        error = OT_ERROR_NONE;
        break;
    case OTBR_ERROR_DUPLICATED:
        error = OT_ERROR_DUPLICATED;
        break;
    case OTBR_ERROR_INVALID_ARGS:
        error = OT_ERROR_INVALID_ARGS;
        break;
    case OTBR_ERROR_ABORTED:
        error = OT_ERROR_ABORT;
        break;
    case OTBR_ERROR_INVALID_STATE:
        error = OT_ERROR_INVALID_STATE;
        break;
    case OTBR_ERROR_NOT_IMPLEMENTED:
        error = OT_ERROR_NOT_IMPLEMENTED;
        break;
    case OTBR_ERROR_NOT_FOUND:
        error = OT_ERROR_NOT_FOUND;
        break;
    case OTBR_ERROR_PARSE:
        error = OT_ERROR_PARSE;
        break;
    default:
        break;
    }

    return error;
}

void DnssdPlatform::HandleMdnsPublisherStateChange(Mdns::Publisher::State aState)
{
    State oldState = mState;

    switch (aState)
    {
    case Mdns::Publisher::State::kIdle:
        mState = OT_PLAT_DNSSD_STOPPED;
        break;

    case Mdns::Publisher::State::kReady:
        mState = OT_PLAT_DNSSD_READY;
        break;
    }

    VerifyOrExit(oldState != mState);
    otPlatDnssdStateHandleStateChange(mNcp.GetInstance());

exit:
    return;
}

Mdns::Publisher::ResultCallback DnssdPlatform::MakePublisherCallback(RequestId aRequestId, RegisterCallback aCallback)
{
    otInstance *instance = mNcp.GetInstance();

    return [instance, aRequestId, aCallback](otbrError aError) {
        if (aCallback != nullptr)
        {
            aCallback(instance, aRequestId, DnssdPlatform::ResultToError(aError));
        }
    };
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

    for (uint16_t index = 0; index < aHost.mNumAddresses; index++)
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

} // namespace otbr

#endif // OTBR_ENABLE_DNSSD_PLAT
