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
 *   The file implements the DNS-SD Discovery Proxy.
 */

#if OTBR_ENABLE_DNSSD_DISCOVERY_PROXY

#include "agent/discovery_proxy.hpp"

#include <algorithm>
#include <string>

#include <assert.h>

#include <openthread/dnssd_server.h>

#include "common/code_utils.hpp"
#include "common/dns_utils.hpp"
#include "common/logging.hpp"

namespace otbr {
namespace Dnssd {

DiscoveryProxy::DiscoveryProxy(Ncp::ControllerOpenThread &aNcp, Mdns::Publisher &aPublisher)
    : mNcp(aNcp)
    , mMdnsPublisher(aPublisher)
{
}

void DiscoveryProxy::Start(void)
{
    otDnssdQuerySetCallbacks(mNcp.GetInstance(), &DiscoveryProxy::OnDiscoveryProxySubscribe,
                             &DiscoveryProxy::OnDiscoveryProxyUnsubscribe, this);

    mMdnsPublisher.SetSubscriptionCallbacks(
        [this](const std::string &aType, const Mdns::Publisher::DiscoveredInstanceInfo &aInstanceInfo) {
            OnServiceDiscovered(aType, aInstanceInfo);
        },

        [this](const std::string &aHostName, const Mdns::Publisher::DiscoveredHostInfo &aHostInfo) {
            OnHostDiscovered(aHostName, aHostInfo);
        });

    otbrLog(OTBR_LOG_INFO, "[discproxy] started");
}

void DiscoveryProxy::Stop(void)
{
    otDnssdQuerySetCallbacks(mNcp.GetInstance(), nullptr, nullptr, nullptr);
    mMdnsPublisher.SetSubscriptionCallbacks(nullptr, nullptr);

    otbrLog(OTBR_LOG_INFO, "[discproxy] stopped");
}

void DiscoveryProxy::OnDiscoveryProxySubscribe(void *aContext, const char *aFullName)
{
    reinterpret_cast<DiscoveryProxy *>(aContext)->OnDiscoveryProxySubscribe(aFullName);
}

void DiscoveryProxy::OnDiscoveryProxySubscribe(const char *aFullName)
{
    std::string                    fullName(aFullName);
    std::string                    instanceName, serviceName, hostName, domain;
    otbrError                      error = OTBR_ERROR_NONE;
    MdnsSubscriptionList::iterator it;
    DnsNameType                    nameType = GetDnsNameType(fullName);

    otbrLog(OTBR_LOG_INFO, "[discproxy] subscribe: %s", fullName.c_str());

    switch (nameType)
    {
    case kDnsNameTypeService:
        SuccessOrExit(error = SplitFullServiceName(fullName, serviceName, domain));
        break;
    case kDnsNameTypeInstance:
        SuccessOrExit(error = SplitFullServiceInstanceName(fullName, instanceName, serviceName, domain));
        break;
    case kDnsNameTypeHost:
        SuccessOrExit(error = SplitFullHostName(fullName, hostName, domain));
        break;
    default:
        ExitNow(error = OTBR_ERROR_NOT_IMPLEMENTED);
    }

    it = std::find_if(mSubscriptions.begin(), mSubscriptions.end(), [&](const MdnsSubscription &aSubscription) {
        return aSubscription.Matches(instanceName, serviceName, hostName, domain);
    });

    VerifyOrExit(it == mSubscriptions.end(), it->mSubscriptionCount++);

    mSubscriptions.emplace_back(instanceName, serviceName, hostName, domain);

    {
        MdnsSubscription &subscription = mSubscriptions.back();

        otbrLog(OTBR_LOG_DEBUG, "[discproxy] subscriptions: %sx%d", subscription.ToString().c_str(),
                subscription.mSubscriptionCount);

        if (GetServiceSubscriptionCount(instanceName, serviceName) == 1)
        {
            if (subscription.mHostName.empty())
            {
                mMdnsPublisher.SubscribeService(serviceName, instanceName);
            }
            else
            {
                mMdnsPublisher.SubscribeHost(hostName);
            }
        }
    }

exit:
    if (error != OTBR_ERROR_NONE)
    {
        otbrLog(OTBR_LOG_WARNING, "[discproxy] failed to subscribe %s: %s", fullName.c_str(), otbrErrorString(error));
    }
}

void DiscoveryProxy::OnDiscoveryProxyUnsubscribe(void *aContext, const char *aFullName)
{
    reinterpret_cast<DiscoveryProxy *>(aContext)->OnDiscoveryProxyUnsubscribe(aFullName);
}

void DiscoveryProxy::OnDiscoveryProxyUnsubscribe(const char *aFullName)
{
    std::string                    fullName(aFullName);
    std::string                    instanceName, serviceName, hostName, domain;
    otbrError                      error = OTBR_ERROR_NONE;
    MdnsSubscriptionList::iterator it;
    DnsNameType                    nameType = GetDnsNameType(fullName);

    otbrLog(OTBR_LOG_INFO, "[discproxy] unsubscribe: %s", fullName.c_str());

    switch (nameType)
    {
    case kDnsNameTypeService:
        SuccessOrExit(error = SplitFullServiceName(fullName, serviceName, domain));
        break;
    case kDnsNameTypeInstance:
        SuccessOrExit(error = SplitFullServiceInstanceName(fullName, instanceName, serviceName, domain));
        break;
    case kDnsNameTypeHost:
        SuccessOrExit(error = SplitFullHostName(fullName, hostName, domain));
        break;
    default:
        ExitNow(error = OTBR_ERROR_NOT_IMPLEMENTED);
    }

    it = std::find_if(mSubscriptions.begin(), mSubscriptions.end(), [&](const MdnsSubscription &aSubscription) {
        return aSubscription.Matches(instanceName, serviceName, hostName, domain);
    });

    VerifyOrExit(it != mSubscriptions.end(), error = OTBR_ERROR_NOT_FOUND);

    {
        MdnsSubscription &subscription = *it;

        subscription.mSubscriptionCount--;
        assert(subscription.mSubscriptionCount >= 0);

        if (subscription.mSubscriptionCount == 0)
        {
            mSubscriptions.erase(it);
        }

        otbrLog(OTBR_LOG_DEBUG, "[discproxy] service subscriptions: %sx%d", it->ToString().c_str(),
                it->mSubscriptionCount);

        if (GetServiceSubscriptionCount(instanceName, serviceName) == 0)
        {
            if (subscription.mHostName.empty())
            {
                mMdnsPublisher.UnsubscribeService(serviceName, instanceName);
            }
            else
            {
                mMdnsPublisher.UnsubscribeHost(hostName);
            }
        }
    }
exit:
    if (error != OTBR_ERROR_NONE)
    {
        otbrLog(OTBR_LOG_WARNING, "[discproxy] failed to unsubscribe %s: %s", fullName.c_str(), otbrErrorString(error));
    }
}

void DiscoveryProxy::OnServiceDiscovered(const std::string &                            aType,
                                         const Mdns::Publisher::DiscoveredInstanceInfo &aInstanceInfo)
{
    otDnssdServiceInstanceInfo instanceInfo;

    otbrLog(OTBR_LOG_INFO,
            "[discproxy] service discovered: %s, instance %s hostname %s addresses %zu port %d priority %d "
            "weight %d",
            aType.c_str(), aInstanceInfo.mName.c_str(), aInstanceInfo.mHostName.c_str(),
            aInstanceInfo.mAddresses.size(), aInstanceInfo.mPort, aInstanceInfo.mPriority, aInstanceInfo.mWeight);

    CheckServiceNameSanity(aType);
    CheckHostnameSanity(aInstanceInfo.mHostName);

    instanceInfo.mAddressNum = aInstanceInfo.mAddresses.size();

    if (!aInstanceInfo.mAddresses.empty())
    {
        instanceInfo.mAddresses = reinterpret_cast<const otIp6Address *>(&aInstanceInfo.mAddresses[0]);
    }
    else
    {
        instanceInfo.mAddresses = nullptr;
    }

    instanceInfo.mPort      = aInstanceInfo.mPort;
    instanceInfo.mPriority  = aInstanceInfo.mPriority;
    instanceInfo.mWeight    = aInstanceInfo.mWeight;
    instanceInfo.mTxtLength = static_cast<uint16_t>(aInstanceInfo.mTxtData.size());
    instanceInfo.mTxtData   = aInstanceInfo.mTxtData.data();
    instanceInfo.mTtl       = CapTtl(aInstanceInfo.mTtl);

    std::for_each(mSubscriptions.begin(), mSubscriptions.end(), [&](const MdnsSubscription &aSubscription) {
        if (aSubscription.MatchesServiceInstance(aType, aInstanceInfo.mName))
        {
            std::string serviceFullName  = aType + "." + aSubscription.mDomain;
            std::string hostName         = TranslateDomain(aInstanceInfo.mHostName, aSubscription.mDomain);
            std::string instanceFullName = aInstanceInfo.mName + "." + serviceFullName;

            instanceInfo.mFullName = instanceFullName.c_str();
            instanceInfo.mHostName = hostName.c_str();

            otDnssdQueryHandleDiscoveredServiceInstance(mNcp.GetInstance(), serviceFullName.c_str(), &instanceInfo);
        }
    });
}

void DiscoveryProxy::OnHostDiscovered(const std::string &                        aHostName,
                                      const Mdns::Publisher::DiscoveredHostInfo &aHostInfo)
{
    otDnssdHostInfo hostInfo;

    otbrLog(OTBR_LOG_INFO, "[discproxy] host discovered: %s hostname %s addresses %zu", aHostName.c_str(),
            aHostInfo.mHostName.c_str(), aHostInfo.mAddresses.size());

    CheckHostnameSanity(aHostInfo.mHostName);

    hostInfo.mAddressNum = aHostInfo.mAddresses.size();
    if (!aHostInfo.mAddresses.empty())
    {
        hostInfo.mAddresses = reinterpret_cast<const otIp6Address *>(&aHostInfo.mAddresses[0]);
    }
    else
    {
        hostInfo.mAddresses = nullptr;
    }

    hostInfo.mTtl = CapTtl(aHostInfo.mTtl);

    std::for_each(mSubscriptions.begin(), mSubscriptions.end(), [&](const MdnsSubscription &aSubscription) {
        if (aSubscription.MatchesHost(aHostName))
        {
            std::string hostFullName = TranslateDomain(aHostInfo.mHostName, aSubscription.mDomain);

            otDnssdQueryHandleDiscoveredHost(mNcp.GetInstance(), hostFullName.c_str(), &hostInfo);
        }
    });
}

std::string DiscoveryProxy::TranslateDomain(const std::string &aName, const std::string &aTargetDomain)
{
    std::string targetName;
    std::string hostName, domain;

    VerifyOrExit(OTBR_ERROR_NONE == SplitFullHostName(aName, hostName, domain), targetName = aName);
    VerifyOrExit(domain == "local.", targetName = aName);

    targetName = hostName + "." + aTargetDomain;

exit:
    otbrLog(OTBR_LOG_DEBUG, "[discproxy] translate domain: %s => %s", aName.c_str(), targetName.c_str());
    return targetName;
}

int DiscoveryProxy::GetServiceSubscriptionCount(const std::string &aInstanceName, const std::string &aServiceName)
{
    return std::accumulate(
        mSubscriptions.begin(), mSubscriptions.end(), 0, [&](int aAccum, const MdnsSubscription &aSubscription) {
            return aAccum + (aSubscription.mInstanceName == aInstanceName && aSubscription.mServiceName == aServiceName
                                 ? aSubscription.mSubscriptionCount
                                 : 0);
        });
}

void DiscoveryProxy::CheckServiceNameSanity(const std::string &aType)
{
    size_t dotpos;

    OTBR_UNUSED_VARIABLE(aType);
    OTBR_UNUSED_VARIABLE(dotpos);

    assert(aType.length() > 0);
    assert(aType[aType.length() - 1] != '.');
    dotpos = aType.find_first_of('.');
    assert(dotpos != std::string::npos);
    assert(dotpos == aType.find_last_of('.'));
}

void DiscoveryProxy::CheckHostnameSanity(const std::string &aHostName)
{
    OTBR_UNUSED_VARIABLE(aHostName);

    assert(aHostName.length() > 0);
    assert(aHostName[aHostName.length() - 1] == '.');
}

uint32_t DiscoveryProxy::CapTtl(uint32_t aTtl)
{
    return std::min(aTtl, static_cast<uint32_t>(kServiceTtlCapLimit));
}

std::string DiscoveryProxy::MdnsSubscription::ToString(void) const
{
    std::string str;

    if (!mHostName.empty())
    {
        str = mHostName + "." + mDomain;
    }
    else if (!mInstanceName.empty())
    {
        str = mInstanceName + "." + mServiceName + "." + mDomain;
    }
    else
    {
        str = mServiceName + "." + mDomain;
    }

    return str;
}

bool DiscoveryProxy::MdnsSubscription::Matches(const std::string &aInstanceName,
                                               const std::string &aServiceName,
                                               const std::string &aHostName,
                                               const std::string &aDomain) const
{
    return mInstanceName == aInstanceName && mServiceName == aServiceName && mHostName == aHostName &&
           mDomain == aDomain;
}

bool DiscoveryProxy::MdnsSubscription::MatchesServiceInstance(const std::string &aType,
                                                              const std::string &aInstanceName) const
{
    return mServiceName == aType && (mInstanceName.empty() || mInstanceName == aInstanceName);
}

bool DiscoveryProxy::MdnsSubscription::MatchesHost(const std::string &aHostName) const
{
    return mHostName == aHostName;
}

} // namespace Dnssd
} // namespace otbr

#endif // OTBR_ENABLE_DNSSD_DISCOVERY_PROXY
