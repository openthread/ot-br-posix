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

#if OTBR_ENABLE_DNSSD_PLAT
#include <string>
#include <vector>

#include <openthread/platform/dnssd.h>

#include "common/code_utils.hpp"
#include "common/logging.hpp"
#include "common/types.hpp"
#include "utils/dns_utils.hpp"

otbr::DnssdPlatform::RegisterCallback MakeRegisterCallback(otInstance *aInstance, otPlatDnssdRegisterCallback aCallback)
{
    return [aInstance, aCallback](otPlatDnssdRequestId aRequestId, otError aError) {
        if (aCallback)
        {
            aCallback(aInstance, aRequestId, aError);
        }
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
    otbr::DnssdPlatform::Get().RegisterService(*aService, aRequestId, MakeRegisterCallback(aInstance, aCallback));
}

extern "C" void otPlatDnssdUnregisterService(otInstance                 *aInstance,
                                             const otPlatDnssdService   *aService,
                                             otPlatDnssdRequestId        aRequestId,
                                             otPlatDnssdRegisterCallback aCallback)
{
    otbr::DnssdPlatform::Get().UnregisterService(*aService, aRequestId, MakeRegisterCallback(aInstance, aCallback));
}

extern "C" void otPlatDnssdRegisterHost(otInstance                 *aInstance,
                                        const otPlatDnssdHost      *aHost,
                                        otPlatDnssdRequestId        aRequestId,
                                        otPlatDnssdRegisterCallback aCallback)
{
    otbr::DnssdPlatform::Get().RegisterHost(*aHost, aRequestId, MakeRegisterCallback(aInstance, aCallback));
}

extern "C" void otPlatDnssdUnregisterHost(otInstance                 *aInstance,
                                          const otPlatDnssdHost      *aHost,
                                          otPlatDnssdRequestId        aRequestId,
                                          otPlatDnssdRegisterCallback aCallback)
{
    otbr::DnssdPlatform::Get().UnregisterHost(*aHost, aRequestId, MakeRegisterCallback(aInstance, aCallback));
}

extern "C" void otPlatDnssdRegisterKey(otInstance                 *aInstance,
                                       const otPlatDnssdKey       *aKey,
                                       otPlatDnssdRequestId        aRequestId,
                                       otPlatDnssdRegisterCallback aCallback)
{
    otbr::DnssdPlatform::Get().RegisterKey(*aKey, aRequestId, MakeRegisterCallback(aInstance, aCallback));
}

extern "C" void otPlatDnssdUnregisterKey(otInstance                 *aInstance,
                                         const otPlatDnssdKey       *aKey,
                                         otPlatDnssdRequestId        aRequestId,
                                         otPlatDnssdRegisterCallback aCallback)
{
    otbr::DnssdPlatform::Get().UnregisterKey(*aKey, aRequestId, MakeRegisterCallback(aInstance, aCallback));
}

extern "C" void otPlatDnssdStartBrowser(otInstance *aInstance, const otPlatDnssdBrowser *aBrowser)
{
    otbr::DnssdPlatform::Get().StartServiceBrowser(
        *aBrowser, std::make_shared<otbr::DnssdPlatform::OtBrowseCallback>(aInstance, aBrowser->mCallback));
}

extern "C" void otPlatDnssdStopBrowser(otInstance *aInstance, const otPlatDnssdBrowser *aBrowser)
{
    otbr::DnssdPlatform::Get().StopServiceBrowser(
        *aBrowser, otbr::DnssdPlatform::OtBrowseCallback(aInstance, aBrowser->mCallback));
}

extern "C" void otPlatDnssdStartSrvResolver(otInstance *aInstance, const otPlatDnssdSrvResolver *aResolver)
{
    otbr::DnssdPlatform::Get().StartServiceResolver(
        *aResolver, std::make_shared<otbr::DnssdPlatform::OtSrvCallback>(aInstance, aResolver->mCallback));
}

extern "C" void otPlatDnssdStopSrvResolver(otInstance *aInstance, const otPlatDnssdSrvResolver *aResolver)
{
    otbr::DnssdPlatform::Get().StopServiceResolver(*aResolver,
                                                   otbr::DnssdPlatform::OtSrvCallback(aInstance, aResolver->mCallback));
}

extern "C" void otPlatDnssdStartTxtResolver(otInstance *aInstance, const otPlatDnssdTxtResolver *aResolver)
{
    otbr::DnssdPlatform::Get().StartTxtResolver(
        *aResolver, std::make_shared<otbr::DnssdPlatform::OtTxtCallback>(aInstance, aResolver->mCallback));
}

extern "C" void otPlatDnssdStopTxtResolver(otInstance *aInstance, const otPlatDnssdTxtResolver *aResolver)
{
    otbr::DnssdPlatform::Get().StopTxtResolver(*aResolver,
                                               otbr::DnssdPlatform::OtTxtCallback(aInstance, aResolver->mCallback));
}

extern "C" void otPlatDnssdStartIp6AddressResolver(otInstance *aInstance, const otPlatDnssdAddressResolver *aResolver)
{
    otbr::DnssdPlatform::Get().StartIp6AddressResolver(
        *aResolver, std::make_shared<otbr::DnssdPlatform::OtAddressCallback>(aInstance, aResolver->mCallback));
}

extern "C" void otPlatDnssdStopIp6AddressResolver(otInstance *aInstance, const otPlatDnssdAddressResolver *aResolver)
{
    otbr::DnssdPlatform::Get().StopIp6AddressResolver(
        *aResolver, otbr::DnssdPlatform::OtAddressCallback(aInstance, aResolver->mCallback));
}

void otPlatDnssdStartIp4AddressResolver(otInstance *aInstance, const otPlatDnssdAddressResolver *aResolver)
{
    otbr::DnssdPlatform::Get().StartIp4AddressResolver(
        *aResolver, std::make_shared<otbr::DnssdPlatform::OtAddressCallback>(aInstance, aResolver->mCallback));
}

void otPlatDnssdStopIp4AddressResolver(otInstance *aInstance, const otPlatDnssdAddressResolver *aResolver)
{
    otbr::DnssdPlatform::Get().StopIp4AddressResolver(
        *aResolver, otbr::DnssdPlatform::OtAddressCallback(aInstance, aResolver->mCallback));
}

void otPlatDnssdStartRecordQuerier(otInstance *aInstance, const otPlatDnssdRecordQuerier *aQuerier)
{
    OTBR_UNUSED_VARIABLE(aInstance);
    OTBR_UNUSED_VARIABLE(aQuerier);
}

void otPlatDnssdStopRecordQuerier(otInstance *aInstance, const otPlatDnssdRecordQuerier *aQuerier)
{
    OTBR_UNUSED_VARIABLE(aInstance);
    OTBR_UNUSED_VARIABLE(aQuerier);
}

//----------------------------------------------------------------------------------------------------------------------

namespace otbr {

DnssdPlatform            *DnssdPlatform::sDnssdPlatform = nullptr;
static constexpr uint64_t kInvalidSubscriberId          = 0;

DnssdPlatform::DnssdPlatform(Mdns::Publisher &aPublisher)
    : mPublisher(aPublisher)
    , mState(kStateStopped)
    , mRunning(false)
    , mServiceSubscriptionUpdateTaskPosted(false)
    , mHostSubscriptionUpdateTaskPosted(false)
    , mPublisherState(Mdns::Publisher::State::kIdle)
    , mSubscriberId(kInvalidSubscriberId)
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

        mState        = kStateReady;
        mSubscriberId = mPublisher.AddSubscriptionCallbacks(HandleDiscoveredService, HandleDiscoveredHost);
    }
    else
    {
        VerifyOrExit(mState != kStateStopped);

        mState = kStateStopped;
        mPublisher.RemoveSubscriptionCallbacks(mSubscriberId);
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
    const char                  *hostName;
    Mdns::Publisher::SubTypeList subTypeList;
    Mdns::Publisher::TxtData     txtData(aService.mTxtData, aService.mTxtData + aService.mTxtDataLength);

    for (uint16_t index = 0; index < aService.mSubTypeLabelsLength; index++)
    {
        subTypeList.push_back(aService.mSubTypeLabels[index]);
    }

    // When `aService.mHostName` is `nullptr`, the service is for
    // the local host. `Mdns::Publisher` expects an empty string
    // to indicate this.

    hostName = aService.mHostName;

    if (hostName == nullptr)
    {
        hostName = "";
    }

    mPublisher.PublishService(hostName, aService.mServiceInstance, aService.mServiceType, subTypeList, aService.mPort,
                              txtData, MakePublisherCallback(aRequestId, aCallback));
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

void DnssdPlatform::StartServiceBrowser(const Browser &aBrowser, BrowseCallbackPtr aCallbackPtr)
{
    DnsServiceType fullServiceType(aBrowser.mServiceType, aBrowser.mSubTypeLabel);
    auto          &entryList = mServiceBrowsersMap[fullServiceType];

    entryList.AddIfAbsent(aBrowser.mInfraIfIndex, std::move(aCallbackPtr));

    PostServiceSubscriptionUpdateTask();
}

void DnssdPlatform::StopServiceBrowser(const Browser &aBrowser, const BrowseCallback &aCallback)
{
    DnsServiceType fullServiceType(aBrowser.mServiceType, aBrowser.mSubTypeLabel);
    auto           iter = mServiceBrowsersMap.find(fullServiceType);

    if (iter != mServiceBrowsersMap.end())
    {
        auto &entryList = iter->second;

        entryList.Delete(aBrowser.mInfraIfIndex, aCallback);

        if (entryList.IsEmpty())
        {
            mServiceBrowsersMap.erase(iter);
        }

        PostServiceSubscriptionUpdateTask();
    }
}

void DnssdPlatform::StartServiceResolver(const SrvResolver &aSrvResolver, SrvCallbackPtr aCallbackPtr)
{
    auto &entryList = mServiceResolversMap[DnsServiceName(aSrvResolver.mServiceInstance, aSrvResolver.mServiceType)];

    entryList.AddIfAbsent(aSrvResolver.mInfraIfIndex, std::move(aCallbackPtr));

    PostServiceSubscriptionUpdateTask();
}

void DnssdPlatform::StopServiceResolver(const SrvResolver &aSrvResolver, const SrvCallback &aCallback)
{
    auto iter = mServiceResolversMap.find(DnsServiceName(aSrvResolver.mServiceInstance, aSrvResolver.mServiceType));

    if (iter != mServiceResolversMap.end())
    {
        auto &entryList = iter->second;

        entryList.Delete(aSrvResolver.mInfraIfIndex, aCallback);

        if (entryList.IsEmpty())
        {
            mServiceResolversMap.erase(iter);
        }

        PostServiceSubscriptionUpdateTask();
    }
}

void DnssdPlatform::StartTxtResolver(const TxtResolver &aTxtResolver, TxtCallbackPtr aCallbackPtr)
{
    auto &entryList = mTxtResolversMap[DnsServiceName(aTxtResolver.mServiceInstance, aTxtResolver.mServiceType)];

    entryList.AddIfAbsent(aTxtResolver.mInfraIfIndex, std::move(aCallbackPtr));

    PostServiceSubscriptionUpdateTask();
}

void DnssdPlatform::StopTxtResolver(const TxtResolver &aTxtResolver, const TxtCallback &aCallback)
{
    auto iter = mTxtResolversMap.find(DnsServiceName(aTxtResolver.mServiceInstance, aTxtResolver.mServiceType));

    if (iter != mTxtResolversMap.end())
    {
        auto &entryList = iter->second;

        entryList.Delete(aTxtResolver.mInfraIfIndex, aCallback);

        if (entryList.IsEmpty())
        {
            mTxtResolversMap.erase(iter);
        }

        PostServiceSubscriptionUpdateTask();
    }
}

void DnssdPlatform::StartIp6AddressResolver(const AddressResolver &aAddressResolver, AddressCallbackPtr aCallbackPtr)
{
    StartAddressResolver(aAddressResolver, std::move(aCallbackPtr));
}

void DnssdPlatform::StopIp6AddressResolver(const AddressResolver &aAddressResolver, const AddressCallback &aCallback)
{
    StopAddressResolver(aAddressResolver, aCallback);
}

void DnssdPlatform::StartIp4AddressResolver(const AddressResolver &aAddressResolver, AddressCallbackPtr aCallbackPtr)
{
    StartAddressResolver(aAddressResolver, std::move(aCallbackPtr));
}

void DnssdPlatform::StopIp4AddressResolver(const AddressResolver &aAddressResolver, const AddressCallback &aCallback)
{
    StopAddressResolver(aAddressResolver, aCallback);
}

void DnssdPlatform::HandleMdnsState(Mdns::Publisher::State aState)
{
    if (mPublisherState != aState)
    {
        mPublisherState = aState;
        UpdateState();
    }
}

void DnssdPlatform::HandleDiscoveredService(const std::string                             &aType,
                                            const Mdns::Publisher::DiscoveredInstanceInfo &aInfo)
{
    otbr::DnssdPlatform::Get().ProcessServiceBrowsers(aType, aInfo);
    otbr::DnssdPlatform::Get().ProcessServiceResolvers(aType, aInfo);
    otbr::DnssdPlatform::Get().ProcessTxtResolvers(aType, aInfo);
}

void DnssdPlatform::HandleDiscoveredHost(const std::string &aHostName, const Mdns::Publisher::DiscoveredHostInfo &aInfo)
{
    otbr::DnssdPlatform::Get().ProcessIpAddrResolvers(aHostName, aInfo);
}

void DnssdPlatform::ProcessServiceBrowsers(const std::string                             &aType,
                                           const Mdns::Publisher::DiscoveredInstanceInfo &aInfo)
{
    std::string  instanceName;
    BrowseResult result;
    auto         it = mServiceBrowsersMap.find(DnsServiceType(aType.c_str(), nullptr));

    VerifyOrExit(mState == kStateReady);

    VerifyOrExit(it != mServiceBrowsersMap.end());

    instanceName = DnsUtils::UnescapeInstanceName(aInfo.mName);

    result.mServiceType     = aType.c_str();
    result.mSubTypeLabel    = nullptr;
    result.mServiceInstance = instanceName.c_str();
    result.mTtl             = aInfo.mTtl;
    result.mInfraIfIndex    = aInfo.mNetifIndex;

    it->second.InvokeAllCallbacks(result.mInfraIfIndex, result);

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
    SrvResult      srvResult;
    auto           it = mServiceResolversMap.find(serviceName);

    VerifyOrExit(mState == kStateReady);
    VerifyOrExit(it != mServiceResolversMap.end());

    SuccessOrExit(DnsUtils::SplitFullHostName(aInfo.mHostName, hostName, domain));
    srvResult.mServiceInstance = instanceName.c_str();
    srvResult.mServiceType     = aType.c_str();
    srvResult.mHostName        = hostName.c_str();
    srvResult.mPort            = aInfo.mPort;
    srvResult.mPriority        = aInfo.mPriority;
    srvResult.mWeight          = aInfo.mWeight;
    srvResult.mTtl             = aInfo.mTtl;
    srvResult.mInfraIfIndex    = aInfo.mNetifIndex;

    it->second.InvokeAllCallbacks(srvResult.mInfraIfIndex, srvResult);

exit:
    return;
}

void DnssdPlatform::ProcessTxtResolvers(const std::string &aType, const Mdns::Publisher::DiscoveredInstanceInfo &aInfo)
{
    std::string    instanceName = DnsUtils::UnescapeInstanceName(aInfo.mName);
    DnsServiceName serviceName(instanceName, aType);
    TxtResult      txtResult;
    auto           it = mTxtResolversMap.find(serviceName);

    VerifyOrExit(mState == kStateReady);

    VerifyOrExit(it != mTxtResolversMap.end());

    txtResult.mServiceInstance = instanceName.c_str();
    txtResult.mServiceType     = aType.c_str();
    txtResult.mTxtData         = aInfo.mTxtData.data();
    txtResult.mTxtDataLength   = aInfo.mTxtData.size();
    txtResult.mTtl             = aInfo.mTtl;
    txtResult.mInfraIfIndex    = aInfo.mNetifIndex;

    it->second.InvokeAllCallbacks(txtResult.mInfraIfIndex, txtResult);

exit:
    return;
}

void DnssdPlatform::ProcessIpAddrResolvers(const std::string                         &aHostName,
                                           const Mdns::Publisher::DiscoveredHostInfo &aInfo)
{
    std::string                           instanceName;
    AddressResult                         result;
    std::vector<otPlatDnssdAddressAndTtl> addressAndTtls;
    auto                                  it = mIpAddrResolversMap.find(aHostName);

    VerifyOrExit(mState == kStateReady);

    VerifyOrExit(it != mIpAddrResolversMap.end());

    result.mHostName = aHostName.c_str();
    for (const auto &addr : aInfo.mAddresses)
    {
        otIp6Address ip6Addr;

        addr.CopyTo(ip6Addr);

        addressAndTtls.push_back({ip6Addr, aInfo.mTtl});
    }
    result.mAddresses       = addressAndTtls.data();
    result.mAddressesLength = addressAndTtls.size();
    result.mInfraIfIndex    = aInfo.mNetifIndex;

    it->second.InvokeAllCallbacks(result.mInfraIfIndex, result);

exit:
    return;
}

void DnssdPlatform::StartAddressResolver(const AddressResolver &aAddressResolver, AddressCallbackPtr aCallbackPtr)
{
    auto &entryList = mIpAddrResolversMap[DnsName(aAddressResolver.mHostName)];

    entryList.AddIfAbsent(aAddressResolver.mInfraIfIndex, std::move(aCallbackPtr));

    PostHostSubscriptionUpdateTask();
}

void DnssdPlatform::StopAddressResolver(const AddressResolver &aAddressResolver, const AddressCallback &aCallback)
{
    auto iter = mIpAddrResolversMap.find(DnsName(aAddressResolver.mHostName));

    if (iter != mIpAddrResolversMap.end())
    {
        auto &entryList = iter->second;

        entryList.Delete(aAddressResolver.mInfraIfIndex, aCallback);

        if (entryList.IsEmpty())
        {
            mIpAddrResolversMap.erase(iter);
        }

        PostHostSubscriptionUpdateTask();
    }
}

void DnssdPlatform::ExecuteServiceSubscriptionUpdate(void)
{
    mServiceSubscriptionUpdateTaskPosted = false;

    // Unsubscribe services (DnsServiceType) that are stale.
    for (auto iter = mServiceTypeSubscriptions.begin(); iter != mServiceTypeSubscriptions.end();)
    {
        if (mServiceBrowsersMap.find(*iter) == mServiceBrowsersMap.end())
        {
            mPublisher.UnsubscribeService(iter->ToString(), /* aInstanceName */ "");
            iter = mServiceTypeSubscriptions.erase(iter);
        }
        else
        {
            iter++;
        }
    }

    // Subscribe to services (DnsServiceType) that haven't been subscribed.
    for (const auto &entry : mServiceBrowsersMap)
    {
        const DnsServiceType &serviceType = entry.first;

        if (mServiceTypeSubscriptions.find(serviceType) == mServiceTypeSubscriptions.end())
        {
            mServiceTypeSubscriptions.insert(serviceType);
            mPublisher.SubscribeService(serviceType.ToString(), /* aInstanceName */ "");
        }
    }

    // Unsubscribe services (DnsServiceName) that are stale.
    for (auto iter = mServiceNameSubscriptions.begin(); iter != mServiceNameSubscriptions.end();)
    {
        if (mServiceResolversMap.find(*iter) == mServiceResolversMap.end() &&
            mTxtResolversMap.find(*iter) == mTxtResolversMap.end())
        {
            mPublisher.UnsubscribeService(iter->GetType(), iter->GetInstance());
            iter = mServiceNameSubscriptions.erase(iter);
        }
        else
        {
            iter++;
        }
    }

    // Subscribe to services (DnsServiceName) that haven't been subscribed.
    for (const auto &entry : mServiceResolversMap)
    {
        const DnsServiceName &serviceName = entry.first;

        if (mServiceNameSubscriptions.find(serviceName) == mServiceNameSubscriptions.end())
        {
            mServiceNameSubscriptions.insert(serviceName);
            mPublisher.SubscribeService(serviceName.GetType(), serviceName.GetInstance());
        }
    }
    for (const auto &entry : mTxtResolversMap)
    {
        const DnsServiceName &serviceName = entry.first;

        if (mServiceNameSubscriptions.find(serviceName) == mServiceNameSubscriptions.end())
        {
            mServiceNameSubscriptions.insert(serviceName);
            mPublisher.SubscribeService(serviceName.GetType(), serviceName.GetInstance());
        }
    }
}

void DnssdPlatform::PostServiceSubscriptionUpdateTask(void)
{
    if (!mServiceSubscriptionUpdateTaskPosted)
    {
        mTaskRunner.Post([this](void) { this->ExecuteServiceSubscriptionUpdate(); });
        mServiceSubscriptionUpdateTaskPosted = true;
    }
}

void DnssdPlatform::ExecuteHostSubscriptionUpdate(void)
{
    mHostSubscriptionUpdateTaskPosted = false;

    // Unsubscribe hosts (DnsName) that are stale.
    for (auto iter = mHostSubscriptions.begin(); iter != mHostSubscriptions.end();)
    {
        const DnsName &dnsName = *iter;

        if (mIpAddrResolversMap.find(dnsName) == mIpAddrResolversMap.end())
        {
            mPublisher.UnsubscribeHost(dnsName.GetName());
            iter = mHostSubscriptions.erase(iter);
        }
        else
        {
            iter++;
        }
    }

    // Subscribe to hosts (DnsName) that haven't been subscribed.
    for (auto &entry : mIpAddrResolversMap)
    {
        const DnsName &dnsName = entry.first;

        if (mHostSubscriptions.find(dnsName) == mHostSubscriptions.end())
        {
            mHostSubscriptions.insert(dnsName);
            mPublisher.SubscribeHost(dnsName.GetName());
        }
    }
}

void DnssdPlatform::PostHostSubscriptionUpdateTask(void)
{
    if (!mHostSubscriptionUpdateTaskPosted)
    {
        mTaskRunner.Post([this](void) { this->ExecuteHostSubscriptionUpdate(); });
        mHostSubscriptionUpdateTaskPosted = true;
    }
}

const std::string DnssdPlatform::DnsServiceType::ToString(void) const
{
    std::string serviceType(mType);

    if (!mSubType.empty())
    {
        serviceType = std::string(mSubType) + "._sub." + serviceType;
    }

    return serviceType;
}

} // namespace otbr

#endif // OTBR_ENABLE_DNSSD_PLAT
