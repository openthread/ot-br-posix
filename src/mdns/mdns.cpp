/*
 *    Copyright (c) 2021, The OpenThread Authors.
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
 *   This file includes implementation of mDNS publisher.
 */

#define OTBR_LOG_TAG "MDNS"

#include "mdns/mdns.hpp"

#if OTBR_ENABLE_MDNS

#include <assert.h>

#include <algorithm>
#include <functional>

#include "common/code_utils.hpp"
#include "utils/dns_utils.hpp"

namespace otbr {

namespace Mdns {

void Publisher::PublishService(const std::string &aHostName,
                               const std::string &aName,
                               const std::string &aType,
                               const SubTypeList &aSubTypeList,
                               uint16_t           aPort,
                               const TxtData     &aTxtData,
                               ResultCallback   &&aCallback)
{
    otbrError error;

    mServiceRegistrationBeginTime[std::make_pair(aName, aType)] = Clock::now();

    error = PublishServiceImpl(aHostName, aName, aType, aSubTypeList, aPort, aTxtData, std::move(aCallback));
    if (error != OTBR_ERROR_NONE)
    {
        UpdateMdnsResponseCounters(mTelemetryInfo.mServiceRegistrations, error);
    }
}

void Publisher::PublishHost(const std::string &aName, const AddressList &aAddresses, ResultCallback &&aCallback)
{
    otbrError error;

    mHostRegistrationBeginTime[aName] = Clock::now();

    error = PublishHostImpl(aName, aAddresses, std::move(aCallback));
    if (error != OTBR_ERROR_NONE)
    {
        UpdateMdnsResponseCounters(mTelemetryInfo.mHostRegistrations, error);
    }
}

void Publisher::PublishKey(const std::string &aName, const KeyData &aKeyData, ResultCallback &&aCallback)
{
    otbrError error;

    mKeyRegistrationBeginTime[aName] = Clock::now();

    error = PublishKeyImpl(aName, aKeyData, std::move(aCallback));
    if (error != OTBR_ERROR_NONE)
    {
        UpdateMdnsResponseCounters(mTelemetryInfo.mKeyRegistrations, error);
    }
}

void Publisher::OnServiceResolveFailed(std::string aType, std::string aInstanceName, int32_t aErrorCode)
{
    UpdateMdnsResponseCounters(mTelemetryInfo.mServiceResolutions, DnsErrorToOtbrError(aErrorCode));
    UpdateServiceInstanceResolutionEmaLatency(aInstanceName, aType, DnsErrorToOtbrError(aErrorCode));
    OnServiceResolveFailedImpl(aType, aInstanceName, aErrorCode);
}

void Publisher::OnHostResolveFailed(std::string aHostName, int32_t aErrorCode)
{
    UpdateMdnsResponseCounters(mTelemetryInfo.mHostResolutions, DnsErrorToOtbrError(aErrorCode));
    UpdateHostResolutionEmaLatency(aHostName, DnsErrorToOtbrError(aErrorCode));
    OnHostResolveFailedImpl(aHostName, aErrorCode);
}

otbrError Publisher::EncodeTxtData(const TxtList &aTxtList, std::vector<uint8_t> &aTxtData)
{
    otbrError error = OTBR_ERROR_NONE;

    aTxtData.clear();

    for (const TxtEntry &txtEntry : aTxtList)
    {
        size_t entryLength = txtEntry.mKey.length();

        if (!txtEntry.mIsBooleanAttribute)
        {
            entryLength += txtEntry.mValue.size() + sizeof(uint8_t); // for `=` char.
        }

        VerifyOrExit(entryLength <= kMaxTextEntrySize, error = OTBR_ERROR_INVALID_ARGS);

        aTxtData.push_back(static_cast<uint8_t>(entryLength));
        aTxtData.insert(aTxtData.end(), txtEntry.mKey.begin(), txtEntry.mKey.end());

        if (!txtEntry.mIsBooleanAttribute)
        {
            aTxtData.push_back('=');
            aTxtData.insert(aTxtData.end(), txtEntry.mValue.begin(), txtEntry.mValue.end());
        }
    }

    if (aTxtData.empty())
    {
        aTxtData.push_back(0);
    }

exit:
    return error;
}

otbrError Publisher::DecodeTxtData(Publisher::TxtList &aTxtList, const uint8_t *aTxtData, uint16_t aTxtLength)
{
    otbrError error = OTBR_ERROR_NONE;

    aTxtList.clear();

    for (uint16_t r = 0; r < aTxtLength;)
    {
        uint16_t entrySize = aTxtData[r];
        uint16_t keyStart  = r + 1;
        uint16_t entryEnd  = keyStart + entrySize;
        uint16_t keyEnd    = keyStart;

        VerifyOrExit(entryEnd <= aTxtLength, error = OTBR_ERROR_PARSE);

        while (keyEnd < entryEnd && aTxtData[keyEnd] != '=')
        {
            keyEnd++;
        }

        if (keyEnd == entryEnd)
        {
            if (keyEnd > keyStart)
            {
                // No `=`, treat as a boolean attribute.
                aTxtList.emplace_back(reinterpret_cast<const char *>(&aTxtData[keyStart]), keyEnd - keyStart);
            }
        }
        else
        {
            uint16_t valStart = keyEnd + 1; // To skip over `=`

            aTxtList.emplace_back(reinterpret_cast<const char *>(&aTxtData[keyStart]), keyEnd - keyStart,
                                  &aTxtData[valStart], entryEnd - valStart);
        }

        r += entrySize + 1;
    }

exit:
    return error;
}

void Publisher::RemoveSubscriptionCallbacks(uint64_t aSubscriberId)
{
    mDiscoverCallbacks.remove_if(
        [aSubscriberId](const DiscoverCallback &aCallback) { return (aCallback.mId == aSubscriberId); });
}

uint64_t Publisher::AddSubscriptionCallbacks(Publisher::DiscoveredServiceInstanceCallback aInstanceCallback,
                                             Publisher::DiscoveredHostCallback            aHostCallback)
{
    uint64_t id = mNextSubscriberId++;

    assert(id > 0);
    mDiscoverCallbacks.emplace_back(id, aInstanceCallback, aHostCallback);

    return id;
}

void Publisher::OnServiceResolved(std::string aType, DiscoveredInstanceInfo aInstanceInfo)
{
    bool checkToInvoke = false;

    otbrLogInfo("Service %s is resolved successfully: %s %s host %s addresses %zu", aType.c_str(),
                aInstanceInfo.mRemoved ? "remove" : "add", aInstanceInfo.mName.c_str(), aInstanceInfo.mHostName.c_str(),
                aInstanceInfo.mAddresses.size());

    if (!aInstanceInfo.mRemoved)
    {
        std::string addressesString;

        for (const auto &address : aInstanceInfo.mAddresses)
        {
            addressesString += address.ToString() + ",";
        }
        if (addressesString.size())
        {
            addressesString.pop_back();
        }
        otbrLogInfo("addresses: [ %s ]", addressesString.c_str());
    }

    DnsUtils::CheckServiceNameSanity(aType);

    assert(aInstanceInfo.mNetifIndex > 0);

    if (!aInstanceInfo.mRemoved)
    {
        DnsUtils::CheckHostnameSanity(aInstanceInfo.mHostName);
    }

    UpdateMdnsResponseCounters(mTelemetryInfo.mServiceResolutions, OTBR_ERROR_NONE);
    UpdateServiceInstanceResolutionEmaLatency(aInstanceInfo.mName, aType, OTBR_ERROR_NONE);

    // The `mDiscoverCallbacks` list can get updated as the callbacks
    // are invoked. We first mark `mShouldInvoke` on all non-null
    // service callbacks. We clear it before invoking the callback
    // and restart the iteration over the `mDiscoverCallbacks` list
    // to find the next one to signal, since the list may have changed.

    for (DiscoverCallback &callback : mDiscoverCallbacks)
    {
        if (callback.mServiceCallback != nullptr)
        {
            callback.mShouldInvoke = true;
            checkToInvoke          = true;
        }
    }

    while (checkToInvoke)
    {
        checkToInvoke = false;

        for (DiscoverCallback &callback : mDiscoverCallbacks)
        {
            if (callback.mShouldInvoke)
            {
                callback.mShouldInvoke = false;
                checkToInvoke          = true;
                callback.mServiceCallback(aType, aInstanceInfo);
                break;
            }
        }
    }
}

void Publisher::OnServiceRemoved(uint32_t aNetifIndex, std::string aType, std::string aInstanceName)
{
    DiscoveredInstanceInfo instanceInfo;

    otbrLogInfo("Service %s.%s is removed from netif %u.", aInstanceName.c_str(), aType.c_str(), aNetifIndex);

    instanceInfo.mRemoved    = true;
    instanceInfo.mNetifIndex = aNetifIndex;
    instanceInfo.mName       = aInstanceName;

    OnServiceResolved(aType, instanceInfo);
}

void Publisher::OnHostResolved(std::string aHostName, Publisher::DiscoveredHostInfo aHostInfo)
{
    bool checkToInvoke = false;

    otbrLogInfo("Host %s is resolved successfully: host %s addresses %zu ttl %u", aHostName.c_str(),
                aHostInfo.mHostName.c_str(), aHostInfo.mAddresses.size(), aHostInfo.mTtl);

    if (!aHostInfo.mHostName.empty())
    {
        DnsUtils::CheckHostnameSanity(aHostInfo.mHostName);
    }

    UpdateMdnsResponseCounters(mTelemetryInfo.mHostResolutions, OTBR_ERROR_NONE);
    UpdateHostResolutionEmaLatency(aHostName, OTBR_ERROR_NONE);

    // The `mDiscoverCallbacks` list can get updated as the callbacks
    // are invoked. We first mark `mShouldInvoke` on all non-null
    // host callbacks. We clear it before invoking the callback
    // and restart the iteration over the `mDiscoverCallbacks` list
    // to find the next one to signal, since the list may have changed.

    for (DiscoverCallback &callback : mDiscoverCallbacks)
    {
        if (callback.mHostCallback != nullptr)
        {
            callback.mShouldInvoke = true;
            checkToInvoke          = true;
        }
    }

    while (checkToInvoke)
    {
        checkToInvoke = false;

        for (DiscoverCallback &callback : mDiscoverCallbacks)
        {
            if (callback.mShouldInvoke)
            {
                callback.mShouldInvoke = false;
                checkToInvoke          = true;
                callback.mHostCallback(aHostName, aHostInfo);
                break;
            }
        }
    }
}

Publisher::SubTypeList Publisher::SortSubTypeList(SubTypeList aSubTypeList)
{
    std::sort(aSubTypeList.begin(), aSubTypeList.end());
    return aSubTypeList;
}

Publisher::AddressList Publisher::SortAddressList(AddressList aAddressList)
{
    std::sort(aAddressList.begin(), aAddressList.end());
    return aAddressList;
}

std::string Publisher::MakeFullServiceName(const std::string &aName, const std::string &aType)
{
    return aName + "." + aType + ".local";
}

std::string Publisher::MakeFullName(const std::string &aName)
{
    return aName + ".local";
}

void Publisher::AddServiceRegistration(ServiceRegistrationPtr &&aServiceReg)
{
    mServiceRegistrations.emplace(MakeFullServiceName(aServiceReg->mName, aServiceReg->mType), std::move(aServiceReg));
}

void Publisher::RemoveServiceRegistration(const std::string &aName, const std::string &aType, otbrError aError)
{
    auto                   it = mServiceRegistrations.find(MakeFullServiceName(aName, aType));
    ServiceRegistrationPtr serviceReg;

    otbrLogInfo("Removing service %s.%s", aName.c_str(), aType.c_str());
    VerifyOrExit(it != mServiceRegistrations.end());

    // Keep the ServiceRegistration around before calling `Complete`
    // to invoke the callback. This is for avoiding invalid access
    // to the ServiceRegistration when it's freed from the callback.
    serviceReg = std::move(it->second);
    mServiceRegistrations.erase(it);
    serviceReg->Complete(aError);

exit:
    return;
}

Publisher::ServiceRegistration *Publisher::FindServiceRegistration(const std::string &aName, const std::string &aType)
{
    auto it = mServiceRegistrations.find(MakeFullServiceName(aName, aType));

    return it != mServiceRegistrations.end() ? it->second.get() : nullptr;
}

Publisher::ServiceRegistration *Publisher::FindServiceRegistration(const std::string &aNameAndType)
{
    auto it = mServiceRegistrations.find(MakeFullName(aNameAndType));

    return it != mServiceRegistrations.end() ? it->second.get() : nullptr;
}

Publisher::ResultCallback Publisher::HandleDuplicateServiceRegistration(const std::string &aHostName,
                                                                        const std::string &aName,
                                                                        const std::string &aType,
                                                                        const SubTypeList &aSubTypeList,
                                                                        uint16_t           aPort,
                                                                        const TxtData     &aTxtData,
                                                                        ResultCallback   &&aCallback)
{
    ServiceRegistration *serviceReg = FindServiceRegistration(aName, aType);

    VerifyOrExit(serviceReg != nullptr);

    if (serviceReg->IsOutdated(aHostName, aName, aType, aSubTypeList, aPort, aTxtData))
    {
        otbrLogInfo("Removing existing service %s.%s: outdated", aName.c_str(), aType.c_str());
        RemoveServiceRegistration(aName, aType, OTBR_ERROR_ABORTED);
    }
    else if (serviceReg->IsCompleted())
    {
        // Returns success if the same service has already been
        // registered with exactly the same parameters.
        std::move(aCallback)(OTBR_ERROR_NONE);
    }
    else
    {
        // If the same service is being registered with the same parameters,
        // let's join the waiting queue for the result.
        serviceReg->mCallback = std::bind(
            [](std::shared_ptr<ResultCallback> aExistingCallback, std::shared_ptr<ResultCallback> aNewCallback,
               otbrError aError) {
                std::move (*aExistingCallback)(aError);
                std::move (*aNewCallback)(aError);
            },
            std::make_shared<ResultCallback>(std::move(serviceReg->mCallback)),
            std::make_shared<ResultCallback>(std::move(aCallback)), std::placeholders::_1);
    }

exit:
    return std::move(aCallback);
}

Publisher::ResultCallback Publisher::HandleDuplicateHostRegistration(const std::string &aName,
                                                                     const AddressList &aAddresses,
                                                                     ResultCallback   &&aCallback)
{
    HostRegistration *hostReg = FindHostRegistration(aName);

    VerifyOrExit(hostReg != nullptr);

    if (hostReg->IsOutdated(aName, aAddresses))
    {
        otbrLogInfo("Removing existing host %s: outdated", aName.c_str());
        RemoveHostRegistration(hostReg->mName, OTBR_ERROR_ABORTED);
    }
    else if (hostReg->IsCompleted())
    {
        // Returns success if the same service has already been
        // registered with exactly the same parameters.
        std::move(aCallback)(OTBR_ERROR_NONE);
    }
    else
    {
        // If the same service is being registered with the same parameters,
        // let's join the waiting queue for the result.
        hostReg->mCallback = std::bind(
            [](std::shared_ptr<ResultCallback> aExistingCallback, std::shared_ptr<ResultCallback> aNewCallback,
               otbrError aError) {
                std::move (*aExistingCallback)(aError);
                std::move (*aNewCallback)(aError);
            },
            std::make_shared<ResultCallback>(std::move(hostReg->mCallback)),
            std::make_shared<ResultCallback>(std::move(aCallback)), std::placeholders::_1);
    }

exit:
    return std::move(aCallback);
}

void Publisher::AddHostRegistration(HostRegistrationPtr &&aHostReg)
{
    mHostRegistrations.emplace(MakeFullHostName(aHostReg->mName), std::move(aHostReg));
}

void Publisher::RemoveHostRegistration(const std::string &aName, otbrError aError)
{
    auto                it = mHostRegistrations.find(MakeFullHostName(aName));
    HostRegistrationPtr hostReg;

    otbrLogInfo("Removing host %s", aName.c_str());
    VerifyOrExit(it != mHostRegistrations.end());

    // Keep the HostRegistration around before calling `Complete`
    // to invoke the callback. This is for avoiding invalid access
    // to the HostRegistration when it's freed from the callback.
    hostReg = std::move(it->second);
    mHostRegistrations.erase(it);
    hostReg->Complete(aError);
    otbrLogInfo("Removed host %s", aName.c_str());

exit:
    return;
}

Publisher::HostRegistration *Publisher::FindHostRegistration(const std::string &aName)
{
    auto it = mHostRegistrations.find(MakeFullHostName(aName));

    return it != mHostRegistrations.end() ? it->second.get() : nullptr;
}

Publisher::ResultCallback Publisher::HandleDuplicateKeyRegistration(const std::string &aName,
                                                                    const KeyData     &aKeyData,
                                                                    ResultCallback   &&aCallback)
{
    KeyRegistration *keyReg = FindKeyRegistration(aName);

    VerifyOrExit(keyReg != nullptr);

    if (keyReg->IsOutdated(aName, aKeyData))
    {
        otbrLogInfo("Removing existing key %s: outdated", aName.c_str());
        RemoveKeyRegistration(keyReg->mName, OTBR_ERROR_ABORTED);
    }
    else if (keyReg->IsCompleted())
    {
        // Returns success if the same key has already been
        // registered with exactly the same parameters.
        std::move(aCallback)(OTBR_ERROR_NONE);
    }
    else
    {
        // If the same key is being registered with the same parameters,
        // let's join the waiting queue for the result.
        keyReg->mCallback = std::bind(
            [](std::shared_ptr<ResultCallback> aExistingCallback, std::shared_ptr<ResultCallback> aNewCallback,
               otbrError aError) {
                std::move (*aExistingCallback)(aError);
                std::move (*aNewCallback)(aError);
            },
            std::make_shared<ResultCallback>(std::move(keyReg->mCallback)),
            std::make_shared<ResultCallback>(std::move(aCallback)), std::placeholders::_1);
    }

exit:
    return std::move(aCallback);
}

void Publisher::AddKeyRegistration(KeyRegistrationPtr &&aKeyReg)
{
    mKeyRegistrations.emplace(MakeFullKeyName(aKeyReg->mName), std::move(aKeyReg));
}

void Publisher::RemoveKeyRegistration(const std::string &aName, otbrError aError)
{
    auto               it = mKeyRegistrations.find(MakeFullKeyName(aName));
    KeyRegistrationPtr keyReg;

    otbrLogInfo("Removing key %s", aName.c_str());
    VerifyOrExit(it != mKeyRegistrations.end());

    // Keep the KeyRegistration around before calling `Complete`
    // to invoke the callback. This is for avoiding invalid access
    // to the KeyRegistration when it's freed from the callback.
    keyReg = std::move(it->second);
    mKeyRegistrations.erase(it);
    keyReg->Complete(aError);
    otbrLogInfo("Removed key %s", aName.c_str());

exit:
    return;
}

Publisher::KeyRegistration *Publisher::FindKeyRegistration(const std::string &aName)
{
    auto it = mKeyRegistrations.find(MakeFullKeyName(aName));

    return it != mKeyRegistrations.end() ? it->second.get() : nullptr;
}

Publisher::KeyRegistration *Publisher::FindKeyRegistration(const std::string &aName, const std::string &aType)
{
    auto it = mKeyRegistrations.find(MakeFullServiceName(aName, aType));

    return it != mKeyRegistrations.end() ? it->second.get() : nullptr;
}

Publisher::Registration::~Registration(void)
{
    TriggerCompleteCallback(OTBR_ERROR_ABORTED);
}

bool Publisher::ServiceRegistration::IsOutdated(const std::string &aHostName,
                                                const std::string &aName,
                                                const std::string &aType,
                                                const SubTypeList &aSubTypeList,
                                                uint16_t           aPort,
                                                const TxtData     &aTxtData) const
{
    return !(mHostName == aHostName && mName == aName && mType == aType && mSubTypeList == aSubTypeList &&
             mPort == aPort && mTxtData == aTxtData);
}

void Publisher::ServiceRegistration::Complete(otbrError aError)
{
    OnComplete(aError);
    Registration::TriggerCompleteCallback(aError);
}

void Publisher::ServiceRegistration::OnComplete(otbrError aError)
{
    if (!IsCompleted())
    {
        mPublisher->UpdateMdnsResponseCounters(mPublisher->mTelemetryInfo.mServiceRegistrations, aError);
        mPublisher->UpdateServiceRegistrationEmaLatency(mName, mType, aError);
    }
}

bool Publisher::HostRegistration::IsOutdated(const std::string &aName, const AddressList &aAddresses) const
{
    return !(mName == aName && mAddresses == aAddresses);
}

void Publisher::HostRegistration::Complete(otbrError aError)
{
    OnComplete(aError);
    Registration::TriggerCompleteCallback(aError);
}

void Publisher::HostRegistration::OnComplete(otbrError aError)
{
    if (!IsCompleted())
    {
        mPublisher->UpdateMdnsResponseCounters(mPublisher->mTelemetryInfo.mHostRegistrations, aError);
        mPublisher->UpdateHostRegistrationEmaLatency(mName, aError);
    }
}

bool Publisher::KeyRegistration::IsOutdated(const std::string &aName, const KeyData &aKeyData) const
{
    return !(mName == aName && mKeyData == aKeyData);
}

void Publisher::KeyRegistration::Complete(otbrError aError)
{
    OnComplete(aError);
    Registration::TriggerCompleteCallback(aError);
}

void Publisher::KeyRegistration::OnComplete(otbrError aError)
{
    if (!IsCompleted())
    {
        mPublisher->UpdateMdnsResponseCounters(mPublisher->mTelemetryInfo.mKeyRegistrations, aError);
        mPublisher->UpdateKeyRegistrationEmaLatency(mName, aError);
    }
}

void Publisher::UpdateMdnsResponseCounters(otbr::MdnsResponseCounters &aCounters, otbrError aError)
{
    switch (aError)
    {
    case OTBR_ERROR_NONE:
        ++aCounters.mSuccess;
        break;
    case OTBR_ERROR_NOT_FOUND:
        ++aCounters.mNotFound;
        break;
    case OTBR_ERROR_INVALID_ARGS:
        ++aCounters.mInvalidArgs;
        break;
    case OTBR_ERROR_DUPLICATED:
        ++aCounters.mDuplicated;
        break;
    case OTBR_ERROR_NOT_IMPLEMENTED:
        ++aCounters.mNotImplemented;
        break;
    case OTBR_ERROR_ABORTED:
        ++aCounters.mAborted;
        break;
    case OTBR_ERROR_INVALID_STATE:
        ++aCounters.mInvalidState;
        break;
    case OTBR_ERROR_MDNS:
    default:
        ++aCounters.mUnknownError;
        break;
    }
}

void Publisher::UpdateEmaLatency(uint32_t &aEmaLatency, uint32_t aLatency, otbrError aError)
{
    VerifyOrExit(aError != OTBR_ERROR_ABORTED);

    if (!aEmaLatency)
    {
        aEmaLatency = aLatency;
    }
    else
    {
        aEmaLatency =
            (aLatency * MdnsTelemetryInfo::kEmaFactorNumerator +
             aEmaLatency * (MdnsTelemetryInfo::kEmaFactorDenominator - MdnsTelemetryInfo::kEmaFactorNumerator)) /
            MdnsTelemetryInfo::kEmaFactorDenominator;
    }

exit:
    return;
}

void Publisher::UpdateServiceRegistrationEmaLatency(const std::string &aInstanceName,
                                                    const std::string &aType,
                                                    otbrError          aError)
{
    auto it = mServiceRegistrationBeginTime.find(std::make_pair(aInstanceName, aType));

    if (it != mServiceRegistrationBeginTime.end())
    {
        uint32_t latency = std::chrono::duration_cast<Milliseconds>(Clock::now() - it->second).count();
        UpdateEmaLatency(mTelemetryInfo.mServiceRegistrationEmaLatency, latency, aError);
        mServiceRegistrationBeginTime.erase(it);
    }
}

void Publisher::UpdateHostRegistrationEmaLatency(const std::string &aHostName, otbrError aError)
{
    auto it = mHostRegistrationBeginTime.find(aHostName);

    if (it != mHostRegistrationBeginTime.end())
    {
        uint32_t latency = std::chrono::duration_cast<Milliseconds>(Clock::now() - it->second).count();
        UpdateEmaLatency(mTelemetryInfo.mHostRegistrationEmaLatency, latency, aError);
        mHostRegistrationBeginTime.erase(it);
    }
}

void Publisher::UpdateKeyRegistrationEmaLatency(const std::string &aKeyName, otbrError aError)
{
    auto it = mKeyRegistrationBeginTime.find(aKeyName);

    if (it != mKeyRegistrationBeginTime.end())
    {
        uint32_t latency = std::chrono::duration_cast<Milliseconds>(Clock::now() - it->second).count();
        UpdateEmaLatency(mTelemetryInfo.mKeyRegistrationEmaLatency, latency, aError);
        mKeyRegistrationBeginTime.erase(it);
    }
}

void Publisher::UpdateServiceInstanceResolutionEmaLatency(const std::string &aInstanceName,
                                                          const std::string &aType,
                                                          otbrError          aError)
{
    auto it = mServiceInstanceResolutionBeginTime.find(std::make_pair(aInstanceName, aType));

    if (it != mServiceInstanceResolutionBeginTime.end())
    {
        uint32_t latency = std::chrono::duration_cast<Milliseconds>(Clock::now() - it->second).count();
        UpdateEmaLatency(mTelemetryInfo.mServiceResolutionEmaLatency, latency, aError);
        mServiceInstanceResolutionBeginTime.erase(it);
    }
}

void Publisher::UpdateHostResolutionEmaLatency(const std::string &aHostName, otbrError aError)
{
    auto it = mHostResolutionBeginTime.find(aHostName);

    if (it != mHostResolutionBeginTime.end())
    {
        uint32_t latency = std::chrono::duration_cast<Milliseconds>(Clock::now() - it->second).count();
        UpdateEmaLatency(mTelemetryInfo.mHostResolutionEmaLatency, latency, aError);
        mHostResolutionBeginTime.erase(it);
    }
}

void Publisher::AddAddress(AddressList &aAddressList, const Ip6Address &aAddress)
{
    aAddressList.push_back(aAddress);
}

void Publisher::RemoveAddress(AddressList &aAddressList, const Ip6Address &aAddress)
{
    auto it = std::find(aAddressList.begin(), aAddressList.end(), aAddress);

    if (it != aAddressList.end())
    {
        aAddressList.erase(it);
    }
}

void StateSubject::AddObserver(StateObserver &aObserver)
{
    mObservers.push_back(&aObserver);
}

void StateSubject::UpdateState(Publisher::State aState)
{
    for (StateObserver *observer : mObservers)
    {
        observer->HandleMdnsState(aState);
    }
}

void StateSubject::Clear(void)
{
    mObservers.clear();
}

} // namespace Mdns
} // namespace otbr

#endif // OTBR_ENABLE_MDNS
