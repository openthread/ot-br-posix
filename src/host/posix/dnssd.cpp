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

template <typename T> otbr::DnssdPlatform::DiscoveryCallbackIdentifier ToDiscoveryCallbackId(T *aCallback)
{
    return reinterpret_cast<uint64_t>(aCallback);
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
    otPlatDnssdBrowseCallback callback = aBrowser->mCallback;

    otbr::DnssdPlatform::Get().StartServiceBrowser(
        *aBrowser, ToDiscoveryCallbackId(callback),
        [aInstance, callback](const otbr::DnssdPlatform::BrowseResult &aResult) { callback(aInstance, &aResult); });
}

extern "C" void otPlatDnssdStopBrowser(otInstance *aInstance, const otPlatDnssdBrowser *aBrowser)
{
    OTBR_UNUSED_VARIABLE(aInstance);

    otbr::DnssdPlatform::Get().StopServiceBrowser(*aBrowser, ToDiscoveryCallbackId(aBrowser->mCallback));
}

extern "C" void otPlatDnssdStartSrvResolver(otInstance *aInstance, const otPlatDnssdSrvResolver *aResolver)
{
    otPlatDnssdSrvCallback callback = aResolver->mCallback;

    otbr::DnssdPlatform::Get().StartServiceResolver(
        *aResolver, ToDiscoveryCallbackId(callback),
        [aInstance, callback](otbr::DnssdPlatform::SrvResult &aResult) { callback(aInstance, &aResult); });
}

extern "C" void otPlatDnssdStopSrvResolver(otInstance *aInstance, const otPlatDnssdSrvResolver *aResolver)
{
    OTBR_UNUSED_VARIABLE(aInstance);

    otbr::DnssdPlatform::Get().StopServiceResolver(*aResolver, ToDiscoveryCallbackId(aResolver->mCallback));
}

extern "C" void otPlatDnssdStartTxtResolver(otInstance *aInstance, const otPlatDnssdTxtResolver *aResolver)
{
    otPlatDnssdTxtCallback callback = aResolver->mCallback;

    otbr::DnssdPlatform::Get().StartTxtResolver(
        *aResolver, ToDiscoveryCallbackId(callback),
        [aInstance, callback](otbr::DnssdPlatform::TxtResult &aResult) { callback(aInstance, &aResult); });
}

extern "C" void otPlatDnssdStopTxtResolver(otInstance *aInstance, const otPlatDnssdTxtResolver *aResolver)
{
    OTBR_UNUSED_VARIABLE(aInstance);

    otbr::DnssdPlatform::Get().StopTxtResolver(*aResolver, ToDiscoveryCallbackId(aResolver->mCallback));
}

extern "C" void otPlatDnssdStartIp6AddressResolver(otInstance *aInstance, const otPlatDnssdAddressResolver *aResolver)
{
    otPlatDnssdAddressCallback callback = aResolver->mCallback;

    otbr::DnssdPlatform::Get().StartIp6AddressResolver(
        *aResolver, ToDiscoveryCallbackId(callback),
        [aInstance, callback](otbr::DnssdPlatform::AddressResult &aResult) { callback(aInstance, &aResult); });
}

extern "C" void otPlatDnssdStopIp6AddressResolver(otInstance *aInstance, const otPlatDnssdAddressResolver *aResolver)
{
    OTBR_UNUSED_VARIABLE(aInstance);

    otbr::DnssdPlatform::Get().StopIp6AddressResolver(*aResolver, ToDiscoveryCallbackId(aResolver->mCallback));
}

void otPlatDnssdStartIp4AddressResolver(otInstance *aInstance, const otPlatDnssdAddressResolver *aResolver)
{
    otPlatDnssdAddressCallback callback = aResolver->mCallback;

    otbr::DnssdPlatform::Get().StartIp4AddressResolver(
        *aResolver, ToDiscoveryCallbackId(callback),
        [aInstance, callback](otbr::DnssdPlatform::AddressResult &aResult) { callback(aInstance, &aResult); });
}

void otPlatDnssdStopIp4AddressResolver(otInstance *aInstance, const otPlatDnssdAddressResolver *aResolver)
{
    OTBR_UNUSED_VARIABLE(aInstance);

    otbr::DnssdPlatform::Get().StopIp4AddressResolver(*aResolver, ToDiscoveryCallbackId(aResolver->mCallback));
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

void DnssdPlatform::StartServiceBrowser(const Browser                     &aBrowser,
                                        const DiscoveryCallbackIdentifier &aId,
                                        const BrowseCallback              &aCallback)
{
    DnsServiceType fullServiceType(aBrowser.mServiceType, aBrowser.mSubTypeLabel);
    auto          &callbackTable     = mServiceBrowsersTable[fullServiceType];
    bool           needsSubscription = !callbackTable.HasAnyValidCallbacks(aBrowser.mInfraIfIndex);

    VerifyOrExit(!mInvokingCallbacks);

    callbackTable.Add(aBrowser.mInfraIfIndex, aId, aCallback);

    if (needsSubscription)
    {
        mPublisher.SubscribeService(fullServiceType.ToString(), "");
    }

exit:
    return;
}

void DnssdPlatform::StopServiceBrowser(const Browser &aBrowser, const DiscoveryCallbackIdentifier &aId)
{
    DnsServiceType fullServiceType(aBrowser.mServiceType, aBrowser.mSubTypeLabel);
    auto           it = mServiceBrowsersTable.find(fullServiceType);

    if (it != mServiceBrowsersTable.end())
    {
        auto &infraIfCallbackTable = it->second;

        infraIfCallbackTable.Remove(aBrowser.mInfraIfIndex, aId);

        if (!infraIfCallbackTable.HasAnyValidCallbacks(aBrowser.mInfraIfIndex))
        {
            mPublisher.UnsubscribeService(fullServiceType.ToString(), "");
        }

        if (!mInvokingCallbacks)
        {
            infraIfCallbackTable.CleanUpRemovedCallbacks();
            if (infraIfCallbackTable.IsEmpty())
            {
                mServiceBrowsersTable.erase(it);
            }
        }
    }
}

void DnssdPlatform::StartServiceResolver(const SrvResolver                 &aSrvResolver,
                                         const DiscoveryCallbackIdentifier &aId,
                                         const SrvCallback                 &aCallback)
{
    auto &callbackTable =
        mServiceResolversTable[DnsServiceName(aSrvResolver.mServiceInstance, aSrvResolver.mServiceType)];
    bool needsSubscription = !callbackTable.HasAnyValidCallbacks(aSrvResolver.mInfraIfIndex);

    VerifyOrExit(!mInvokingCallbacks);

    callbackTable.Add(aSrvResolver.mInfraIfIndex, aId, aCallback);

    if (needsSubscription)
    {
        mPublisher.SubscribeService(aSrvResolver.mServiceType, aSrvResolver.mServiceInstance);
    }

exit:
    return;
}

void DnssdPlatform::StopServiceResolver(const SrvResolver &aSrvResolver, const DiscoveryCallbackIdentifier &aId)
{
    auto it = mServiceResolversTable.find(DnsServiceName(aSrvResolver.mServiceInstance, aSrvResolver.mServiceType));

    if (it != mServiceResolversTable.end())
    {
        auto &infraIfCallbackTable = it->second;

        infraIfCallbackTable.Remove(aSrvResolver.mInfraIfIndex, aId);

        if (!infraIfCallbackTable.HasAnyValidCallbacks(aSrvResolver.mInfraIfIndex))
        {
            mPublisher.UnsubscribeService(aSrvResolver.mServiceType, aSrvResolver.mServiceInstance);
        }

        if (!mInvokingCallbacks)
        {
            infraIfCallbackTable.CleanUpRemovedCallbacks();
            if (infraIfCallbackTable.IsEmpty())
            {
                mServiceResolversTable.erase(it);
            }
        }
    }
}

void DnssdPlatform::StartTxtResolver(const TxtResolver                 &aTxtResolver,
                                     const DiscoveryCallbackIdentifier &aId,
                                     const TxtCallback                 &aCallback)
{
    auto &callbackTable = mTxtResolversTable[DnsServiceName(aTxtResolver.mServiceInstance, aTxtResolver.mServiceType)];
    bool  needsSubscription = !callbackTable.HasAnyValidCallbacks(aTxtResolver.mInfraIfIndex);

    VerifyOrExit(!mInvokingCallbacks);

    callbackTable.Add(aTxtResolver.mInfraIfIndex, aId, aCallback);

    if (needsSubscription)
    {
        mPublisher.SubscribeService(aTxtResolver.mServiceType, aTxtResolver.mServiceInstance);
    }

exit:
    return;
}

void DnssdPlatform::StopTxtResolver(const TxtResolver &aTxtResolver, const DiscoveryCallbackIdentifier &aId)
{
    auto it = mTxtResolversTable.find(DnsServiceName(aTxtResolver.mServiceInstance, aTxtResolver.mServiceType));

    if (it != mTxtResolversTable.end())
    {
        auto &infraIfCallbackTable = it->second;

        infraIfCallbackTable.Remove(aTxtResolver.mInfraIfIndex, aId);

        if (!infraIfCallbackTable.HasAnyValidCallbacks(aTxtResolver.mInfraIfIndex))
        {
            mPublisher.UnsubscribeService(aTxtResolver.mServiceType, aTxtResolver.mServiceInstance);
        }

        if (!mInvokingCallbacks)
        {
            infraIfCallbackTable.CleanUpRemovedCallbacks();
            if (infraIfCallbackTable.IsEmpty())
            {
                mTxtResolversTable.erase(it);
            }
        }
    }
}

void DnssdPlatform::StartIp6AddressResolver(const AddressResolver             &aAddressResolver,
                                            const DiscoveryCallbackIdentifier &aId,
                                            const AddressCallback             &aCallback)
{
    StartAddressResolver(aAddressResolver, aCallback, aId, mIp6AddrResolversTable);
}

void DnssdPlatform::StopIp6AddressResolver(const AddressResolver             &aAddressResolver,
                                           const DiscoveryCallbackIdentifier &aId)
{
    StopAddressResolver(aAddressResolver, aId, mIp6AddrResolversTable);
}

void DnssdPlatform::StartIp4AddressResolver(const AddressResolver             &aAddressResolver,
                                            const DiscoveryCallbackIdentifier &aId,
                                            const AddressCallback             &aCallback)
{
    StartAddressResolver(aAddressResolver, aCallback, aId, mIp4AddrResolversTable);
}

void DnssdPlatform::StopIp4AddressResolver(const AddressResolver             &aAddressResolver,
                                           const DiscoveryCallbackIdentifier &aId)
{
    StopAddressResolver(aAddressResolver, aId, mIp4AddrResolversTable);
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
    auto         it = mServiceBrowsersTable.find(DnsServiceType(aType.c_str(), nullptr));

    VerifyOrExit(mState == kStateReady);

    VerifyOrExit(it != mServiceBrowsersTable.end());

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
    auto           it = mServiceResolversTable.find(serviceName);

    VerifyOrExit(mState == kStateReady);
    VerifyOrExit(it != mServiceResolversTable.end());

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
    auto           it = mTxtResolversTable.find(serviceName);

    VerifyOrExit(mState == kStateReady);

    VerifyOrExit(it != mTxtResolversTable.end());

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

void DnssdPlatform::ProcessAddrResolvers(const std::string                                        &aHostName,
                                         const Mdns::Publisher::DiscoveredHostInfo                &aInfo,
                                         std::map<DnsName, InfraIfCallbackTable<AddressCallback>> &aResolversTable)
{
    std::string                           instanceName;
    AddressResult                         result;
    std::vector<otPlatDnssdAddressAndTtl> addressAndTtls;
    auto                                  it = aResolversTable.find(aHostName);

    VerifyOrExit(mState == kStateReady);

    VerifyOrExit(it != aResolversTable.end());

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
    ProcessAddrResolvers(aHostName, aInfo, mIp6AddrResolversTable);
}

void DnssdPlatform::ProcessIp4AddrResolvers(const std::string                         &aHostName,
                                            const Mdns::Publisher::DiscoveredHostInfo &aInfo)
{
    ProcessAddrResolvers(aHostName, aInfo, mIp4AddrResolversTable);
}

void DnssdPlatform::StartAddressResolver(const AddressResolver                                    &aAddressResolver,
                                         const AddressCallback                                    &aCallback,
                                         const DiscoveryCallbackIdentifier                        &aId,
                                         std::map<DnsName, InfraIfCallbackTable<AddressCallback>> &aResolversTable)
{
    auto &callbackTable     = aResolversTable[DnsName(aAddressResolver.mHostName)];
    bool  needsSubscription = !callbackTable.HasAnyValidCallbacks(aAddressResolver.mInfraIfIndex);

    VerifyOrExit(!mInvokingCallbacks);

    callbackTable.Add(aAddressResolver.mInfraIfIndex, aId, aCallback);

    if (needsSubscription)
    {
        mPublisher.SubscribeHost(aAddressResolver.mHostName);
    }

exit:
    return;
}

void DnssdPlatform::StopAddressResolver(const AddressResolver                                    &aAddressResolver,
                                        const DiscoveryCallbackIdentifier                        &aId,
                                        std::map<DnsName, InfraIfCallbackTable<AddressCallback>> &aResolversTable)
{
    auto it = aResolversTable.find(DnsName(aAddressResolver.mHostName));

    if (it != aResolversTable.end())
    {
        auto &infraIfCallbackTable = it->second;

        infraIfCallbackTable.Remove(aAddressResolver.mInfraIfIndex, aId);

        if (!infraIfCallbackTable.HasAnyValidCallbacks(aAddressResolver.mInfraIfIndex))
        {
            mPublisher.UnsubscribeHost(aAddressResolver.mHostName);

            if (!mInvokingCallbacks)
            {
                infraIfCallbackTable.CleanUpRemovedCallbacks();
                if (infraIfCallbackTable.IsEmpty())
                {
                    aResolversTable.erase(it);
                }
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
