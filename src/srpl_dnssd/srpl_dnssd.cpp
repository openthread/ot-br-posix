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
 *   This file includes implementation of SRPL DNS-SD over mDNS.
 */

#if OTBR_ENABLE_SRP_REPLICATION

#if !OTBR_ENABLE_DNS_DSO
#error "SRP Replication requires OTBR_DNS_DSO to be enabled."
#endif

#define OTBR_LOG_TAG "SrplDns"

#include "srpl_dnssd/srpl_dnssd.hpp"

#include "openthread/platform/srp_replication.h"

#include "mdns/mdns.hpp"
#include "openthread/ip6.h"
#include "utils/string_utils.hpp"

static otbr::SrplDnssd::SrplDnssd *sSrplDnssd = nullptr;

extern "C" void otPlatSrplDnssdBrowse(otInstance *aInstance, bool aEnable)
{
    OTBR_UNUSED_VARIABLE(aInstance);

    if (aEnable)
    {
        sSrplDnssd->StartBrowse();
    }
    else
    {
        sSrplDnssd->StopBrowse();
    }
}

extern "C" void otPlatSrplRegisterDnssdService(otInstance *aInstance, const uint8_t *aTxtData, uint16_t aTxtLength)
{
    OTBR_UNUSED_VARIABLE(aInstance);

    sSrplDnssd->RegisterService(aTxtData, aTxtLength);
}

extern "C" void otPlatSrplUnregisterDnssdService(otInstance *aInstance)
{
    OTBR_UNUSED_VARIABLE(aInstance);

    sSrplDnssd->UnregisterService();
}

namespace otbr {

namespace SrplDnssd {

SrplDnssd::SrplDnssd(Ncp::ControllerOpenThread &aNcp, Mdns::Publisher &aPublisher)
    : mNcp(aNcp)
    , mPublisher(aPublisher)
{
    sSrplDnssd = this;
}

void SrplDnssd::StartBrowse(void)
{
    VerifyOrExit(!IsBrowsing());

    otbrLogDebug("Start browsing SRPL services ...");

    mSubscriberId = mPublisher.AddSubscriptionCallbacks(
        [this](const std::string &aType, const DiscoveredInstanceInfo &aInstanceInfo) {
            OnServiceInstanceResolved(aType, aInstanceInfo);
        },
        nullptr);
    mPublisher.SubscribeService(kServiceType, /* aInstanceName */ "");

exit:
    return;
}

void SrplDnssd::StopBrowse(void)
{
    VerifyOrExit(IsBrowsing());

    otbrLogDebug("Stop browsing SRPL services.");

    mPublisher.UnsubscribeService(kServiceType, /* aInstanceName */ "");
    mPublisher.RemoveSubscriptionCallbacks(mSubscriberId);
    mSubscriberId = 0;
    mDiscoveredInstances.clear();

exit:
    return;
}

// TODO: handle the case when mDNS publisher is not ready
void SrplDnssd::RegisterService(const uint8_t *aTxtData, uint8_t aTxtLength)
{
    otbr::Mdns::Publisher::TxtList txtList;

    SuccessOrExit(otbr::Mdns::Publisher::DecodeTxtData(txtList, aTxtData, aTxtLength));

    otbrLogInfo("Publishing SRPL service");
    mPublisher.PublishService(/* aHostName */ "", /* aName */ "", kServiceType, {}, kPort, txtList,
                              [this](otbrError aError) {
                                  otbrLogResult(aError, "Result of publishing SRPL service");
                                  if (aError == OTBR_ERROR_NONE)
                                  {
                                      auto serviceRegistration = mPublisher.FindServiceRegistrationByType(kServiceType);
                                      if (serviceRegistration)
                                      {
                                          mServiceInstanceName = serviceRegistration->mName;
                                          otbrLogInfo("SRPL service instance name is %s", mServiceInstanceName.c_str());
                                      }
                                  }
                              });

exit:
    return;
}

void SrplDnssd::UnregisterService()
{
    otbrLogInfo("Unpublishing SRPL service: %s", mServiceInstanceName.c_str());
    mPublisher.UnpublishService(mServiceInstanceName, kServiceType, [this](otbrError aError) {
        if (aError == OTBR_ERROR_NONE)
        {
            mServiceInstanceName.clear();
        }
    });
}

void SrplDnssd::OnServiceInstanceResolved(const std::string &aType, const DiscoveredInstanceInfo &aInstanceInfo)
{
    otPlatSrplPartnerInfo partnerInfo;
    std::string           lowercaseInstanceName = StringUtils::ToLowercase(aInstanceInfo.mName);

    VerifyOrExit(IsBrowsing());
    VerifyOrExit(StringUtils::EqualCaseInsensitive(aType, kServiceType));
    VerifyOrExit(!StringUtils::EqualCaseInsensitive(aInstanceInfo.mName, mServiceInstanceName));

    // TODO: Also need to check by addresses to mark as 'me'.

    partnerInfo.mRemoved = aInstanceInfo.mRemoved;
    otbrLogInfo("Discovered SRPL peer: %s %s", aInstanceInfo.mName.c_str(), partnerInfo.mRemoved ? "Removed" : "Added");

    if (!partnerInfo.mRemoved)
    {
        auto it = mDiscoveredInstances.find(lowercaseInstanceName);

        VerifyOrExit(!aInstanceInfo.mAddresses.empty());

        // TODO: Currently the mDNS publisher only returns 1 address in every callback, ideally we should collect all
        // resolved addresses for the same service and decide which address to use.
        otbrLogInfo("peer address: %s", aInstanceInfo.mAddresses.front().ToString().c_str());
        SuccessOrExit(otIp6AddressFromString(aInstanceInfo.mAddresses.front().ToString().c_str(),
                                             &partnerInfo.mSockAddr.mAddress));

        partnerInfo.mTxtData        = aInstanceInfo.mTxtData.data();
        partnerInfo.mTxtLength      = aInstanceInfo.mTxtData.size();
        partnerInfo.mSockAddr.mPort = aInstanceInfo.mPort;

        if (it != mDiscoveredInstances.end() && it->second.mAddresses != aInstanceInfo.mAddresses)
        {
            // The service's addresses have changed. Need to signal removing the old service first.
            otPlatSrplPartnerInfo removeOldPartnerInfo{};

            SuccessOrExit(otIp6AddressFromString(it->second.mAddresses.front().ToString().c_str(),
                                                 &partnerInfo.mSockAddr.mAddress));
            removeOldPartnerInfo.mSockAddr.mPort = it->second.mPort;
            removeOldPartnerInfo.mRemoved        = true;

            otPlatSrplHandleDnssdBrowseResult(mNcp.GetInstance(), &removeOldPartnerInfo);
        }
        mDiscoveredInstances[lowercaseInstanceName] = aInstanceInfo;
    }
    else
    {
        mDiscoveredInstances.erase(lowercaseInstanceName);
    }
    otPlatSrplHandleDnssdBrowseResult(mNcp.GetInstance(), &partnerInfo);

exit:
    return;
}

} // namespace SrplDnssd
} // namespace otbr

#endif
