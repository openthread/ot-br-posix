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
        *aBrowser, MakeUnique<otbr::DnssdPlatform::OtBrowseCallback>(aInstance, aBrowser->mCallback));
}

extern "C" void otPlatDnssdStopBrowser(otInstance *aInstance, const otPlatDnssdBrowser *aBrowser)
{
    otbr::DnssdPlatform::Get().StopServiceBrowser(
        *aBrowser, otbr::DnssdPlatform::OtBrowseCallback(aInstance, aBrowser->mCallback));
}

extern "C" void otPlatDnssdStartSrvResolver(otInstance *aInstance, const otPlatDnssdSrvResolver *aResolver)
{
    otbr::DnssdPlatform::Get().StartServiceResolver(
        *aResolver, MakeUnique<otbr::DnssdPlatform::OtSrvCallback>(aInstance, aResolver->mCallback));
}

extern "C" void otPlatDnssdStopSrvResolver(otInstance *aInstance, const otPlatDnssdSrvResolver *aResolver)
{
    otbr::DnssdPlatform::Get().StopServiceResolver(*aResolver,
                                                   otbr::DnssdPlatform::OtSrvCallback(aInstance, aResolver->mCallback));
}

extern "C" void otPlatDnssdStartTxtResolver(otInstance *aInstance, const otPlatDnssdTxtResolver *aResolver)
{
    otbr::DnssdPlatform::Get().StartTxtResolver(
        *aResolver, MakeUnique<otbr::DnssdPlatform::OtTxtCallback>(aInstance, aResolver->mCallback));
}

extern "C" void otPlatDnssdStopTxtResolver(otInstance *aInstance, const otPlatDnssdTxtResolver *aResolver)
{
    otbr::DnssdPlatform::Get().StopTxtResolver(*aResolver,
                                               otbr::DnssdPlatform::OtTxtCallback(aInstance, aResolver->mCallback));
}

extern "C" void otPlatDnssdStartIp6AddressResolver(otInstance *aInstance, const otPlatDnssdAddressResolver *aResolver)
{
    otbr::DnssdPlatform::Get().StartIp6AddressResolver(
        *aResolver, MakeUnique<otbr::DnssdPlatform::OtAddressCallback>(aInstance, aResolver->mCallback));
}

extern "C" void otPlatDnssdStopIp6AddressResolver(otInstance *aInstance, const otPlatDnssdAddressResolver *aResolver)
{
    otbr::DnssdPlatform::Get().StopIp6AddressResolver(
        *aResolver, otbr::DnssdPlatform::OtAddressCallback(aInstance, aResolver->mCallback));
}

void otPlatDnssdStartIp4AddressResolver(otInstance *aInstance, const otPlatDnssdAddressResolver *aResolver)
{
    otbr::DnssdPlatform::Get().StartIp4AddressResolver(
        *aResolver, MakeUnique<otbr::DnssdPlatform::OtAddressCallback>(aInstance, aResolver->mCallback));
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
    , mInvokingCallbacks(false)
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
    auto          &entryList         = mServiceBrowsersMap[fullServiceType];
    bool           needsSubscription = !entryList.HasSubscribedOnInfraIf(aBrowser.mInfraIfIndex);

    VerifyOrExit(!mInvokingCallbacks);

    entryList.AddIfAbsent(aBrowser.mInfraIfIndex, std::move(aCallbackPtr));

    if (needsSubscription)
    {
        mPublisher.SubscribeService(fullServiceType.ToString(), "");
    }

exit:
    return;
}

void DnssdPlatform::StopServiceBrowser(const Browser &aBrowser, const BrowseCallback &aCallback)
{
    DnsServiceType fullServiceType(aBrowser.mServiceType, aBrowser.mSubTypeLabel);
    auto           iter = mServiceBrowsersMap.find(fullServiceType);

    if (iter != mServiceBrowsersMap.end())
    {
        auto &entryList = iter->second;

        entryList.MarkAsDeleted(aBrowser.mInfraIfIndex, aCallback);

        if (!entryList.HasAnyValidCallbacks())
        {
            mPublisher.UnsubscribeService(fullServiceType.ToString(), "");
        }

        if (!mInvokingCallbacks)
        {
            entryList.CleanUpDeletedEntries();
            if (entryList.IsEmpty())
            {
                mServiceBrowsersMap.erase(iter);
            }
        }
    }
}

void DnssdPlatform::StartServiceResolver(const SrvResolver &aSrvResolver, SrvCallbackPtr aCallbackPtr)
{
    auto &entryList = mServiceResolversMap[DnsServiceName(aSrvResolver.mServiceInstance, aSrvResolver.mServiceType)];
    bool  needsSubscription = !entryList.HasSubscribedOnInfraIf(aSrvResolver.mInfraIfIndex);

    VerifyOrExit(!mInvokingCallbacks);

    entryList.AddIfAbsent(aSrvResolver.mInfraIfIndex, std::move(aCallbackPtr));

    if (needsSubscription)
    {
        mPublisher.SubscribeService(aSrvResolver.mServiceType, aSrvResolver.mServiceInstance);
    }

exit:
    return;
}

void DnssdPlatform::StopServiceResolver(const SrvResolver &aSrvResolver, const SrvCallback &aCallback)
{
    auto iter = mServiceResolversMap.find(DnsServiceName(aSrvResolver.mServiceInstance, aSrvResolver.mServiceType));

    if (iter != mServiceResolversMap.end())
    {
        auto &entryList = iter->second;

        entryList.MarkAsDeleted(aSrvResolver.mInfraIfIndex, aCallback);

        if (!entryList.HasAnyValidCallbacks())
        {
            mPublisher.UnsubscribeService(aSrvResolver.mServiceType, aSrvResolver.mServiceInstance);
        }

        if (!mInvokingCallbacks)
        {
            entryList.CleanUpDeletedEntries();
            if (entryList.IsEmpty())
            {
                mServiceResolversMap.erase(iter);
            }
        }
    }
}

void DnssdPlatform::StartTxtResolver(const TxtResolver &aTxtResolver, TxtCallbackPtr aCallbackPtr)
{
    auto &entryList = mTxtResolversMap[DnsServiceName(aTxtResolver.mServiceInstance, aTxtResolver.mServiceType)];
    bool  needsSubscription = !entryList.HasSubscribedOnInfraIf(aTxtResolver.mInfraIfIndex);

    VerifyOrExit(!mInvokingCallbacks);

    entryList.AddIfAbsent(aTxtResolver.mInfraIfIndex, std::move(aCallbackPtr));

    if (needsSubscription)
    {
        mPublisher.SubscribeService(aTxtResolver.mServiceType, aTxtResolver.mServiceInstance);
    }

exit:
    return;
}

void DnssdPlatform::StopTxtResolver(const TxtResolver &aTxtResolver, const TxtCallback &aCallback)
{
    auto iter = mTxtResolversMap.find(DnsServiceName(aTxtResolver.mServiceInstance, aTxtResolver.mServiceType));

    if (iter != mTxtResolversMap.end())
    {
        auto &entryList = iter->second;

        entryList.MarkAsDeleted(aTxtResolver.mInfraIfIndex, aCallback);

        if (!entryList.HasAnyValidCallbacks())
        {
            mPublisher.UnsubscribeService(aTxtResolver.mServiceType, aTxtResolver.mServiceInstance);
        }

        if (!mInvokingCallbacks)
        {
            entryList.CleanUpDeletedEntries();
            if (entryList.IsEmpty())
            {
                mTxtResolversMap.erase(iter);
            }
        }
    }
}

void DnssdPlatform::StartIp6AddressResolver(const AddressResolver &aAddressResolver, AddressCallbackPtr aCallbackPtr)
{
    StartAddressResolver(aAddressResolver, std::move(aCallbackPtr), mIp6AddrResolversMap);
}

void DnssdPlatform::StopIp6AddressResolver(const AddressResolver &aAddressResolver, const AddressCallback &aCallback)
{
    StopAddressResolver(aAddressResolver, aCallback, mIp6AddrResolversMap);
}

void DnssdPlatform::StartIp4AddressResolver(const AddressResolver &aAddressResolver, AddressCallbackPtr aCallbackPtr)
{
    StartAddressResolver(aAddressResolver, std::move(aCallbackPtr), mIp4AddrResolversMap);
}

void DnssdPlatform::StopIp4AddressResolver(const AddressResolver &aAddressResolver, const AddressCallback &aCallback)
{
    StopAddressResolver(aAddressResolver, aCallback, mIp4AddrResolversMap);
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
    otbr::DnssdPlatform::Get().SetInvokingCallbacks(true);

    otbr::DnssdPlatform::Get().ProcessServiceBrowsers(aType, aInfo);
    otbr::DnssdPlatform::Get().ProcessServiceResolvers(aType, aInfo);
    otbr::DnssdPlatform::Get().ProcessTxtResolvers(aType, aInfo);

    otbr::DnssdPlatform::Get().SetInvokingCallbacks(false);
}

void DnssdPlatform::HandleDiscoveredHost(const std::string &aHostName, const Mdns::Publisher::DiscoveredHostInfo &aInfo)
{
    otbr::DnssdPlatform::Get().SetInvokingCallbacks(true);

    otbr::DnssdPlatform::Get().ProcessIp6AddrResolvers(aHostName, aInfo);
    otbr::DnssdPlatform::Get().ProcessIp4AddrResolvers(aHostName, aInfo);

    otbr::DnssdPlatform::Get().SetInvokingCallbacks(false);
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

void DnssdPlatform::ProcessAddrResolvers(const std::string                          &aHostName,
                                         const Mdns::Publisher::DiscoveredHostInfo  &aInfo,
                                         std::map<DnsName, EntryList<AddressEntry>> &aResolversMap)
{
    std::string                           instanceName;
    AddressResult                         result;
    std::vector<otPlatDnssdAddressAndTtl> addressAndTtls;
    auto                                  it = aResolversMap.find(aHostName);

    VerifyOrExit(mState == kStateReady);

    VerifyOrExit(it != aResolversMap.end());

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

void DnssdPlatform::ProcessIp6AddrResolvers(const std::string                         &aHostName,
                                            const Mdns::Publisher::DiscoveredHostInfo &aInfo)
{
    ProcessAddrResolvers(aHostName, aInfo, mIp6AddrResolversMap);
}

void DnssdPlatform::ProcessIp4AddrResolvers(const std::string                         &aHostName,
                                            const Mdns::Publisher::DiscoveredHostInfo &aInfo)
{
    ProcessAddrResolvers(aHostName, aInfo, mIp4AddrResolversMap);
}

void DnssdPlatform::StartAddressResolver(const AddressResolver                      &aAddressResolver,
                                         AddressCallbackPtr                          aCallbackPtr,
                                         std::map<DnsName, EntryList<AddressEntry>> &aResolversMap)
{
    auto &entryList         = aResolversMap[DnsName(aAddressResolver.mHostName)];
    bool  needsSubscription = !entryList.HasSubscribedOnInfraIf(aAddressResolver.mInfraIfIndex);

    VerifyOrExit(!mInvokingCallbacks);

    entryList.AddIfAbsent(aAddressResolver.mInfraIfIndex, std::move(aCallbackPtr));

    if (needsSubscription)
    {
        mPublisher.SubscribeHost(aAddressResolver.mHostName);
    }

exit:
    return;
}

void DnssdPlatform::StopAddressResolver(const AddressResolver                      &aAddressResolver,
                                        const AddressCallback                      &aCallback,
                                        std::map<DnsName, EntryList<AddressEntry>> &aResolversMap)
{
    auto iter = aResolversMap.find(DnsName(aAddressResolver.mHostName));

    if (iter != aResolversMap.end())
    {
        auto &entryList = iter->second;

        entryList.MarkAsDeleted(aAddressResolver.mInfraIfIndex, aCallback);

        if (!entryList.HasAnyValidCallbacks())
        {
            mPublisher.UnsubscribeHost(aAddressResolver.mHostName);
        }

        if (!mInvokingCallbacks)
        {
            entryList.CleanUpDeletedEntries();
            if (entryList.IsEmpty())
            {
                aResolversMap.erase(iter);
            }
        }
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
