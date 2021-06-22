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

#define OTBR_LOG_TAG "DPROXY"

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

    otbrLogInfo("started");
}

void DiscoveryProxy::Stop(void)
{
    otDnssdQuerySetCallbacks(mNcp.GetInstance(), nullptr, nullptr, nullptr);
    mMdnsPublisher.SetSubscriptionCallbacks(nullptr, nullptr);

    otbrLogInfo("stopped");
}

void DiscoveryProxy::OnDiscoveryProxySubscribe(void *aContext, const char *aFullName)
{
    reinterpret_cast<DiscoveryProxy *>(aContext)->OnDiscoveryProxySubscribe(aFullName);
}

void DiscoveryProxy::OnDiscoveryProxySubscribe(const char *aFullName)
{
    std::string fullName(aFullName);
    otbrError   error    = OTBR_ERROR_NONE;
    DnsNameInfo nameInfo = SplitFullDnsName(fullName);

    otbrLogInfo("subscribe: %s", fullName.c_str());

    if (GetServiceSubscriptionCount(nameInfo) == 1)
    {
        if (nameInfo.mHostName.empty())
        {
            mMdnsPublisher.SubscribeService(nameInfo.mServiceName, nameInfo.mInstanceName);
        }
        else
        {
            mMdnsPublisher.SubscribeHost(nameInfo.mHostName);
        }
    }

    if (error != OTBR_ERROR_NONE)
    {
        otbrLogWarning("failed to subscribe %s: %s", fullName.c_str(), otbrErrorString(error));
    }
}

void DiscoveryProxy::OnDiscoveryProxyUnsubscribe(void *aContext, const char *aFullName)
{
    reinterpret_cast<DiscoveryProxy *>(aContext)->OnDiscoveryProxyUnsubscribe(aFullName);
}

void DiscoveryProxy::OnDiscoveryProxyUnsubscribe(const char *aFullName)
{
    std::string fullName(aFullName);
    otbrError   error    = OTBR_ERROR_NONE;
    DnsNameInfo nameInfo = SplitFullDnsName(fullName);

    otbrLogInfo("unsubscribe: %s", fullName.c_str());

    if (GetServiceSubscriptionCount(nameInfo) == 1)
    {
        if (nameInfo.mHostName.empty())
        {
            mMdnsPublisher.UnsubscribeService(nameInfo.mServiceName, nameInfo.mInstanceName);
        }
        else
        {
            mMdnsPublisher.UnsubscribeHost(nameInfo.mHostName);
        }
    }
    if (error != OTBR_ERROR_NONE)
    {
        otbrLogWarning("failed to unsubscribe %s: %s", fullName.c_str(), otbrErrorString(error));
    }
}

void DiscoveryProxy::OnServiceDiscovered(const std::string &                            aType,
                                         const Mdns::Publisher::DiscoveredInstanceInfo &aInstanceInfo)
{
    otDnssdServiceInstanceInfo instanceInfo;
    const otDnssdQuery *       query = nullptr;

    otbrLogInfo("service discovered: %s, instance %s hostname %s addresses %zu port %d priority %d "
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

    while ((query = otDnssdGetNextQuery(mNcp.GetInstance(), query)) != nullptr)
    {
        std::string      instanceName;
        std::string      serviceName;
        std::string      hostName;
        std::string      domain;
        char             queryName[OT_DNS_MAX_NAME_SIZE];
        otDnssdQueryType type = otDnssdGetQueryTypeAndName(query, &queryName);
        otbrError        splitError;

        switch (type)
        {
        case OT_DNSSD_QUERY_TYPE_BROWSE:
            splitError = SplitFullServiceName(queryName, serviceName, domain);
            assert(splitError == OTBR_ERROR_NONE);
            break;
        case OT_DNSSD_QUERY_TYPE_RESOLVE:
            splitError = SplitFullServiceInstanceName(queryName, instanceName, serviceName, domain);
            assert(splitError == OTBR_ERROR_NONE);
            break;
        default:
            splitError = OTBR_ERROR_NOT_FOUND;
            break;
        }
        if (splitError != OTBR_ERROR_NONE)
        {
            continue;
        }

        if (serviceName == aType && (instanceName.empty() || instanceName == aInstanceInfo.mName))
        {
            std::string serviceFullName  = aType + "." + domain;
            std::string hostName         = TranslateDomain(aInstanceInfo.mHostName, domain);
            std::string instanceFullName = aInstanceInfo.mName + "." + serviceFullName;

            instanceInfo.mFullName = instanceFullName.c_str();
            instanceInfo.mHostName = hostName.c_str();

            otDnssdQueryHandleDiscoveredServiceInstance(mNcp.GetInstance(), serviceFullName.c_str(), &instanceInfo);
        }
    }
}

void DiscoveryProxy::OnHostDiscovered(const std::string &                        aHostName,
                                      const Mdns::Publisher::DiscoveredHostInfo &aHostInfo)
{
    otDnssdHostInfo     hostInfo;
    const otDnssdQuery *query            = nullptr;
    std::string         resolvedHostName = aHostInfo.mHostName;

    otbrLogInfo("host discovered: %s hostname %s addresses %zu", aHostName.c_str(), aHostInfo.mHostName.c_str(),
                aHostInfo.mAddresses.size());

    if (resolvedHostName.empty())
    {
        resolvedHostName = aHostName + ".local.";
    }

    CheckHostnameSanity(resolvedHostName);

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

    while ((query = otDnssdGetNextQuery(mNcp.GetInstance(), query)) != nullptr)
    {
        std::string      hostName, domain;
        char             queryName[OT_DNS_MAX_NAME_SIZE];
        otDnssdQueryType type = otDnssdGetQueryTypeAndName(query, &queryName);
        otbrError        splitError;

        if (type != OT_DNSSD_QUERY_TYPE_RESOLVE_HOST)
        {
            continue;
        }
        splitError = SplitFullHostName(queryName, hostName, domain);
        assert(splitError == OTBR_ERROR_NONE);

        if (hostName == aHostName)
        {
            std::string hostFullName = TranslateDomain(resolvedHostName, domain);

            otDnssdQueryHandleDiscoveredHost(mNcp.GetInstance(), hostFullName.c_str(), &hostInfo);
        }
    }
}

std::string DiscoveryProxy::TranslateDomain(const std::string &aName, const std::string &aTargetDomain)
{
    std::string targetName;
    std::string hostName;
    std::string domain;

    VerifyOrExit(OTBR_ERROR_NONE == SplitFullHostName(aName, hostName, domain), targetName = aName);
    VerifyOrExit(domain == "local.", targetName = aName);

    targetName = hostName + "." + aTargetDomain;

exit:
    otbrLogDebug("translate domain: %s => %s", aName.c_str(), targetName.c_str());
    return targetName;
}

int DiscoveryProxy::GetServiceSubscriptionCount(const DnsNameInfo &aNameInfo) const
{
    const otDnssdQuery *query = nullptr;
    int                 count = 0;

    while ((query = otDnssdGetNextQuery(mNcp.GetInstance(), query)) != nullptr)
    {
        char        queryName[OT_DNS_MAX_NAME_SIZE];
        DnsNameInfo queryInfo;

        otDnssdGetQueryTypeAndName(query, &queryName);
        queryInfo = SplitFullDnsName(queryName);

        count += (aNameInfo.mInstanceName == queryInfo.mInstanceName &&
                  aNameInfo.mServiceName == queryInfo.mServiceName && aNameInfo.mHostName == queryInfo.mHostName);
    }

    return count;
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

} // namespace Dnssd
} // namespace otbr

#endif // OTBR_ENABLE_DNSSD_DISCOVERY_PROXY
