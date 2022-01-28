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

#include "mdns/mdns.hpp"

#include <assert.h>

#include "common/code_utils.hpp"
#include "utils/dns_utils.hpp"

namespace otbr {

namespace Mdns {

bool Publisher::IsServiceTypeEqual(std::string aFirstType, std::string aSecondType)
{
    if (!aFirstType.empty() && aFirstType.back() == '.')
    {
        aFirstType.pop_back();
    }

    if (!aSecondType.empty() && aSecondType.back() == '.')
    {
        aSecondType.pop_back();
    }

    return aFirstType == aSecondType;
}

otbrError Publisher::EncodeTxtData(const TxtList &aTxtList, std::vector<uint8_t> &aTxtData)
{
    otbrError error = OTBR_ERROR_NONE;

    for (const auto &txtEntry : aTxtList)
    {
        const auto & name        = txtEntry.mName;
        const auto & value       = txtEntry.mValue;
        const size_t entryLength = name.length() + 1 + value.size();

        VerifyOrExit(entryLength <= kMaxTextEntrySize, error = OTBR_ERROR_INVALID_ARGS);

        aTxtData.push_back(static_cast<uint8_t>(entryLength));
        aTxtData.insert(aTxtData.end(), name.begin(), name.end());
        aTxtData.push_back('=');
        aTxtData.insert(aTxtData.end(), value.begin(), value.end());
    }

exit:
    return error;
}

otbrError Publisher::DecodeTxtData(Publisher::TxtList &aTxtList, const uint8_t *aTxtData, uint16_t aTxtLength)
{
    otbrError error = OTBR_ERROR_NONE;

    for (uint16_t r = 0; r < aTxtLength;)
    {
        uint16_t entrySize = aTxtData[r];
        uint16_t keyStart  = r + 1;
        uint16_t entryEnd  = keyStart + entrySize;
        uint16_t keyEnd    = keyStart;
        uint16_t valStart;

        while (keyEnd < entryEnd && aTxtData[keyEnd] != '=')
        {
            keyEnd++;
        }

        valStart = keyEnd;
        if (valStart < entryEnd && aTxtData[valStart] == '=')
        {
            valStart++;
        }

        aTxtList.emplace_back(reinterpret_cast<const char *>(&aTxtData[keyStart]), keyEnd - keyStart,
                              &aTxtData[valStart], entryEnd - valStart);

        r += entrySize + 1;
        VerifyOrExit(r <= aTxtLength, error = OTBR_ERROR_PARSE);
    }

exit:
    return error;
}

void Publisher::RemoveSubscriptionCallbacks(uint64_t aSubscriberId)
{
    size_t erased;

    OTBR_UNUSED_VARIABLE(erased);

    assert(aSubscriberId > 0);

    erased = mDiscoveredCallbacks.erase(aSubscriberId);

    assert(erased == 1);
}

uint64_t Publisher::AddSubscriptionCallbacks(Publisher::DiscoveredServiceInstanceCallback aInstanceCallback,
                                             Publisher::DiscoveredHostCallback            aHostCallback)
{
    uint64_t subscriberId = mNextSubscriberId++;

    assert(subscriberId > 0);

    mDiscoveredCallbacks.emplace(subscriberId, std::make_pair(std::move(aInstanceCallback), std::move(aHostCallback)));
    return subscriberId;
}

void Publisher::OnServiceResolved(const std::string &aType, const DiscoveredInstanceInfo &aInstanceInfo)
{
    otbrLogInfo("Service %s is resolved successfully: %s %s host %s addresses %zu", aType.c_str(),
                aInstanceInfo.mRemoved ? "remove" : "add", aInstanceInfo.mName.c_str(), aInstanceInfo.mHostName.c_str(),
                aInstanceInfo.mAddresses.size());

    DnsUtils::CheckServiceNameSanity(aType);

    assert(aInstanceInfo.mNetifIndex > 0);

    if (!aInstanceInfo.mRemoved)
    {
        DnsUtils::CheckHostnameSanity(aInstanceInfo.mHostName);
    }

    for (const auto &subCallback : mDiscoveredCallbacks)
    {
        if (subCallback.second.first != nullptr)
        {
            subCallback.second.first(aType, aInstanceInfo);
        }
    }
}

void Publisher::OnServiceRemoved(uint32_t aNetifIndex, const std::string &aType, const std::string &aInstanceName)
{
    DiscoveredInstanceInfo instanceInfo;

    otbrLogInfo("Service %s.%s is removed from netif %u.", aInstanceName.c_str(), aType.c_str(), aNetifIndex);

    instanceInfo.mRemoved    = true;
    instanceInfo.mNetifIndex = aNetifIndex;
    instanceInfo.mName       = aInstanceName;

    OnServiceResolved(aType, instanceInfo);
}

void Publisher::OnHostResolved(const std::string &aHostName, const Publisher::DiscoveredHostInfo &aHostInfo)
{
    otbrLogInfo("Host %s is resolved successfully: host %s addresses %zu ttl %u", aHostName.c_str(),
                aHostInfo.mHostName.c_str(), aHostInfo.mAddresses.size(), aHostInfo.mTtl);

    if (!aHostInfo.mHostName.empty())
    {
        DnsUtils::CheckHostnameSanity(aHostInfo.mHostName);
    }

    for (const auto &subCallback : mDiscoveredCallbacks)
    {
        if (subCallback.second.second != nullptr)
        {
            subCallback.second.second(aHostName, aHostInfo);
        }
    }
}

} // namespace Mdns

} // namespace otbr
