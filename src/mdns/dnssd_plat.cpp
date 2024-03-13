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

//----------------------------------------------------------------------------------------------------------------------
// otPlatDnssd APIs

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
    otbr::DnssdPlatform::Get().RegisterService(*aService, aRequestId, aCallback);
}

extern "C" void otPlatDnssdUnregisterService(otInstance                 *aInstance,
                                             const otPlatDnssdService   *aService,
                                             otPlatDnssdRequestId        aRequestId,
                                             otPlatDnssdRegisterCallback aCallback)
{
    OTBR_UNUSED_VARIABLE(aInstance);
    otbr::DnssdPlatform::Get().UnregisterService(*aService, aRequestId, aCallback);
}

extern "C" void otPlatDnssdRegisterHost(otInstance                 *aInstance,
                                        const otPlatDnssdHost      *aHost,
                                        otPlatDnssdRequestId        aRequestId,
                                        otPlatDnssdRegisterCallback aCallback)
{
    OTBR_UNUSED_VARIABLE(aInstance);
    otbr::DnssdPlatform::Get().RegisterHost(*aHost, aRequestId, aCallback);
}

extern "C" void otPlatDnssdUnregisterHost(otInstance                 *aInstance,
                                          const otPlatDnssdHost      *aHost,
                                          otPlatDnssdRequestId        aRequestId,
                                          otPlatDnssdRegisterCallback aCallback)
{
    OTBR_UNUSED_VARIABLE(aInstance);
    otbr::DnssdPlatform::Get().UnregisterHost(*aHost, aRequestId, aCallback);
}

extern "C" void otPlatDnssdRegisterKey(otInstance                 *aInstance,
                                       const otPlatDnssdKey       *aKey,
                                       otPlatDnssdRequestId        aRequestId,
                                       otPlatDnssdRegisterCallback aCallback)
{
    OTBR_UNUSED_VARIABLE(aInstance);
    otbr::DnssdPlatform::Get().RegisterKey(*aKey, aRequestId, aCallback);
}

extern "C" void otPlatDnssdUnregisterKey(otInstance                 *aInstance,
                                         const otPlatDnssdKey       *aKey,
                                         otPlatDnssdRequestId        aRequestId,
                                         otPlatDnssdRegisterCallback aCallback)
{
    OTBR_UNUSED_VARIABLE(aInstance);
    otbr::DnssdPlatform::Get().UnregisterKey(*aKey, aRequestId, aCallback);
}

// This is a temporary config to allow us to build/test (till the PRs in OT which define these APIs are merged).
#define OTBR_DNSSD_ADD_BROWSER_RESOLVER_APIS 1

#if OTBR_DNSSD_ADD_BROWSER_RESOLVER_APIS

extern "C" void otPlatDnssdStartServiceBrowser(otInstance *aInstance, const char *aServiceType, uint32_t aInfraIfIndex)
{
    OTBR_UNUSED_VARIABLE(aInstance);
    otbr::DnssdPlatform::Get().StartServiceBrowser(aServiceType, aInfraIfIndex);
}

extern "C" void otPlatDnssdStopServiceBrowser(otInstance *aInstance, const char *aServiceType, uint32_t aInfraIfIndex)
{
    OTBR_UNUSED_VARIABLE(aInstance);
    otbr::DnssdPlatform::Get().StopServiceBrowser(aServiceType, aInfraIfIndex);
}

extern "C" void otPlatDnssdStartServiceResolver(otInstance                       *aInstance,
                                                const otPlatDnssdServiceInstance *aServiceInstance)
{
    OTBR_UNUSED_VARIABLE(aInstance);
    otbr::DnssdPlatform::Get().StartServiceResolver(*aServiceInstance);
}

extern "C" void otPlatDnssdStopServiceResolver(otInstance                       *aInstance,
                                               const otPlatDnssdServiceInstance *aServiceInstance)
{
    OTBR_UNUSED_VARIABLE(aInstance);
    otbr::DnssdPlatform::Get().StopServiceResolver(*aServiceInstance);
}

extern "C" void otPlatDnssdStartIp6AddressResolver(otInstance *aInstance, const char *aHostName, uint32_t aInfraIfIndex)
{
    OTBR_UNUSED_VARIABLE(aInstance);
    otbr::DnssdPlatform::Get().StartIp6AddressResolver(aHostName, aInfraIfIndex);
}

extern "C" void otPlatDnssdStopIp6AddressResolver(otInstance *aInstance, const char *aHostName, uint32_t aInfraIfIndex)
{
    OTBR_UNUSED_VARIABLE(aInstance);
    otbr::DnssdPlatform::Get().StopIp6AddressResolver(aHostName, aInfraIfIndex);
}

extern "C" void otPlatDnssdStartIp4AddressResolver(otInstance *aInstance, const char *aHostName, uint32_t aInfraIfIndex)
{
    OTBR_UNUSED_VARIABLE(aInstance);
    OTBR_UNUSED_VARIABLE(aHostName);
    OTBR_UNUSED_VARIABLE(aInfraIfIndex);
}

extern "C" void otPlatDnssdStopIp4AddressResolver(otInstance *aInstance, const char *aHostName, uint32_t aInfraIfIndex)
{
    OTBR_UNUSED_VARIABLE(aInstance);
    OTBR_UNUSED_VARIABLE(aHostName);
    OTBR_UNUSED_VARIABLE(aInfraIfIndex);
}

#endif // OTBR_DNSSD_ADD_BROWSER_RESOLVER_APIS

//----------------------------------------------------------------------------------------------------------------------

namespace otbr {

DnssdPlatform *DnssdPlatform::sDnssdPlatform = nullptr;

DnssdPlatform::DnssdPlatform(Ncp::ControllerOpenThread &aNcp, Mdns::Publisher &aPublisher)
    : mNcp(aNcp)
    , mPublisher(aPublisher)
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

void DnssdPlatform::HandleMdnsPublisherStateChange(Mdns::Publisher::State aState)
{
    if (mPublisherState != aState)
    {
        mPublisherState = aState;
        UpdateState();
    }
}

void DnssdPlatform::UpdateState(void)
{
    if (mRunning && (mPublisherState == Mdns::Publisher::State::kReady))
    {
        VerifyOrExit(mState != kStateReady);

        mState        = kStateReady;
        mSubscriberId = mPublisher.AddSubscriptionCallbacks(HandleDiscoveredService, HandleDiscoveredHost);
    }
    else
    {
        VerifyOrExit(mState != kStateStopped);

        mServiceBrowsers.clear();
        mServiceResolvers.clear();
        mIp6AddrResolvers.clear();
        mState = kStateStopped;
        mPublisher.RemoveSubscriptionCallbacks(mSubscriberId);
    }

    otPlatDnssdStateHandleStateChange(mNcp.GetInstance());

exit:
    return;
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

void DnssdPlatform::StartServiceBrowser(const char *aServiceType, uint32_t aInfraIfIndex)
{
    NetifIndexList &list     = mServiceBrowsers[DnsName(aServiceType)];
    bool            wasEmpty = list.IsEmpty();

    list.Add(aInfraIfIndex);

    if (wasEmpty)
    {
        mPublisher.SubscribeService(aServiceType, "");
    }
}

void DnssdPlatform::StopServiceBrowser(const char *aServiceType, uint32_t aInfraIfIndex)
{
    auto it = mServiceBrowsers.find(DnsName(aServiceType));

    if (it != mServiceBrowsers.end())
    {
        NetifIndexList &list = it->second;

        list.Remove(aInfraIfIndex);

        if (list.IsEmpty())
        {
            mServiceBrowsers.erase(it);
            mPublisher.UnsubscribeService(aServiceType, "");
        }
    }
}

void DnssdPlatform::StartServiceResolver(const ServiceInstance &aInfo)
{
    NetifIndexList &list     = mServiceResolvers[DnsServiceName(aInfo.mServiceInstance, aInfo.mServiceType)];
    bool            wasEmpty = list.IsEmpty();

    list.Add(aInfo.mInfraIfIndex);

    if (wasEmpty)
    {
        mPublisher.SubscribeService(aInfo.mServiceType, aInfo.mServiceInstance);
    }
}

void DnssdPlatform::StopServiceResolver(const ServiceInstance &aInfo)
{
    auto it = mServiceResolvers.find(DnsServiceName(aInfo.mServiceInstance, aInfo.mServiceType));

    if (it != mServiceResolvers.end())
    {
        NetifIndexList &list = it->second;

        list.Remove(aInfo.mInfraIfIndex);

        if (list.IsEmpty())
        {
            mServiceResolvers.erase(it);
            mPublisher.UnsubscribeService(aInfo.mServiceType, aInfo.mServiceInstance);
        }
    }
}

void DnssdPlatform::StartIp6AddressResolver(const char *aHostName, uint32_t aInfraIfIndex)
{
    NetifIndexList &list     = mIp6AddrResolvers[DnsName(aHostName)];
    bool            wasEmpty = list.IsEmpty();

    list.Add(aInfraIfIndex);

    if (wasEmpty)
    {
        mPublisher.SubscribeHost(aHostName);
    }
}

void DnssdPlatform::StopIp6AddressResolver(const char *aHostName, uint32_t aInfraIfIndex)
{
    auto it = mIp6AddrResolvers.find(DnsName(aHostName));

    if (it != mIp6AddrResolvers.end())
    {
        NetifIndexList &list = it->second;

        list.Remove(aInfraIfIndex);

        if (list.IsEmpty())
        {
            mIp6AddrResolvers.erase(it);
            mPublisher.UnsubscribeHost(aHostName);
        }
    }
}

void DnssdPlatform::HandleDiscoveredService(const std::string                             &aType,
                                            const Mdns::Publisher::DiscoveredInstanceInfo &aInfo)
{
    sDnssdPlatform->ProcessServiceBrowsers(aType, aInfo);
    sDnssdPlatform->ProcessServiceResolvers(aType, aInfo);
}

void DnssdPlatform::HandleDiscoveredHost(const std::string &aHostName, const Mdns::Publisher::DiscoveredHostInfo &aInfo)
{
    sDnssdPlatform->ProcessIp6AddrResolvers(aHostName, aInfo);
}

void DnssdPlatform::ProcessServiceBrowsers(const std::string                             &aType,
                                           const Mdns::Publisher::DiscoveredInstanceInfo &aInfo)
{
    std::string     instanceName;
    ServiceInstance service;
    Event           event;

    VerifyOrExit(mState == kStateReady);

    VerifyOrExit(mServiceBrowsers.count(aType) != 0);
    VerifyOrExit(mServiceBrowsers[aType].Matches(aInfo.mNetifIndex));

    instanceName = DnsUtils::UnescapeInstanceName(aInfo.mName);

    memset(&service, 0, sizeof(service));
    service.mServiceType     = aType.c_str();
    service.mServiceInstance = instanceName.c_str();
    service.mTtl             = aInfo.mTtl;
    service.mInfraIfIndex    = aInfo.mNetifIndex;
    event                    = aInfo.mRemoved ? kEventEntryRemoved : kEventEntryAdded;

    otPlatDnssdHandleServiceBrowseResult(mNcp.GetInstance(), event, &service);

exit:
    return;
}

void DnssdPlatform::ProcessServiceResolvers(const std::string                             &aType,
                                            const Mdns::Publisher::DiscoveredInstanceInfo &aInfo)
{
    std::string    instanceName = DnsUtils::UnescapeInstanceName(aInfo.mName);
    DnsServiceName serviceName(instanceName, aType);
    std::string    hostName;
    std::string    domain;
    Service        service;

    VerifyOrExit(mState == kStateReady);

    VerifyOrExit(mServiceResolvers.count(serviceName) != 0);
    VerifyOrExit(mServiceResolvers[serviceName].Matches(aInfo.mNetifIndex));

    SuccessOrExit(SplitFullHostName(aInfo.mHostName, hostName, domain));

    memset(&service, 0, sizeof(service));
    service.mServiceType     = aType.c_str();
    service.mServiceInstance = instanceName.c_str();
    service.mHostName        = hostName.c_str();
    service.mTxtData         = aInfo.mTxtData.data();
    service.mTxtDataLength   = aInfo.mTxtData.size();
    service.mPort            = aInfo.mPort;
    service.mPriority        = aInfo.mPriority;
    service.mWeight          = aInfo.mWeight;
    service.mTtl             = aInfo.mTtl;
    service.mInfraIfIndex    = aInfo.mNetifIndex;

    otPlatDnssdHandleServiceResolveResult(mNcp.GetInstance(), &service);

exit:
    return;
}

void DnssdPlatform::ProcessIp6AddrResolvers(const std::string                         &aHostName,
                                            const Mdns::Publisher::DiscoveredHostInfo &aInfo)
{
    Host host;

    VerifyOrExit(mState == kStateReady);

    VerifyOrExit(mIp6AddrResolvers.count(aHostName) != 0);
    VerifyOrExit(mIp6AddrResolvers[aHostName].Matches(aInfo.mNetifIndex));

    memset(&host, 0, sizeof(host));
    host.mHostName     = aHostName.c_str();
    host.mAddresses    = reinterpret_cast<const otIp6Address *>(&aInfo.mAddresses[0]);
    host.mNumAddresses = aInfo.mAddresses.size();
    host.mTtl          = aInfo.mTtl;
    host.mInfraIfIndex = aInfo.mNetifIndex;

    otPlatDnssdHandleIp6AddressResolveResult(mNcp.GetInstance(), kEventEntryAdded, &host);

exit:
    return;
}

//---------------------------------------------------------------------------------------------------------------------
// `DnssdPlatform::NetifIndexList`

bool DnssdPlatform::NetifIndexList::Matches(uint32_t aInterfaceIndex) const
{
    bool matches = false;

    if (aInterfaceIndex == kAnyNetifIndex)
    {
        ExitNow(matches = true);
    }

    for (uint32_t netifIndex : mList)
    {
        if ((netifIndex == kAnyNetifIndex) || (netifIndex == aInterfaceIndex))
        {
            ExitNow(matches = true);
        }
    }

exit:
    return matches;
}

bool DnssdPlatform::NetifIndexList::Contains(uint32_t aInterfaceIndex) const
{
    bool contains = false;

    for (uint32_t netifIndex : mList)
    {
        if (netifIndex == aInterfaceIndex)
        {
            contains = true;
            break;
        }
    }

    return contains;
}

void DnssdPlatform::NetifIndexList::Add(uint32_t aInterfaceIndex)
{
    if (!Contains(aInterfaceIndex))
    {
        mList.push_back(aInterfaceIndex);
    }
}

void DnssdPlatform::NetifIndexList::Remove(uint32_t aInterfaceIndex)
{
    auto it = std::find(mList.begin(), mList.end(), aInterfaceIndex);

    if (it != mList.end())
    {
        mList.erase(it);
    }
}

} // namespace otbr

#endif // OTBR_ENABLE_DNSSD_PLAT
