/*
 *    Copyright (c) 2018, The OpenThread Authors.
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
 *   This file implements mDNS service based on mDNSResponder.
 */

#define OTBR_LOG_TAG "MDNS"

#include "mdns/mdns_mdnssd.hpp"

#include <algorithm>

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/code_utils.hpp"
#include "common/dns_utils.hpp"
#include "common/logging.hpp"
#include "common/time.hpp"
#include "utils/strcpy_utils.hpp"

namespace otbr {

namespace Mdns {

static otbrError DNSErrorToOtbrError(DNSServiceErrorType aError)
{
    otbrError error;

    switch (aError)
    {
    case kDNSServiceErr_NoError:
        error = OTBR_ERROR_NONE;
        break;

    case kDNSServiceErr_NoSuchKey:
    case kDNSServiceErr_NoSuchName:
    case kDNSServiceErr_NoSuchRecord:
        error = OTBR_ERROR_NOT_FOUND;
        break;

    case kDNSServiceErr_Invalid:
    case kDNSServiceErr_BadParam:
    case kDNSServiceErr_BadFlags:
    case kDNSServiceErr_BadInterfaceIndex:
        error = OTBR_ERROR_INVALID_ARGS;
        break;

    case kDNSServiceErr_AlreadyRegistered:
    case kDNSServiceErr_NameConflict:
        error = OTBR_ERROR_DUPLICATED;
        break;

    case kDNSServiceErr_Unsupported:
        error = OTBR_ERROR_NOT_IMPLEMENTED;
        break;

    default:
        error = OTBR_ERROR_MDNS;
        break;
    }

    return error;
}

static const char *DNSErrorToString(DNSServiceErrorType aError)
{
    switch (aError)
    {
    case kDNSServiceErr_NoError:
        return "OK";

    case kDNSServiceErr_Unknown:
        // 0xFFFE FFFF
        return "Unknown";

    case kDNSServiceErr_NoSuchName:
        return "No Such Name";

    case kDNSServiceErr_NoMemory:
        return "No Memory";

    case kDNSServiceErr_BadParam:
        return "Bad Param";

    case kDNSServiceErr_BadReference:
        return "Bad Reference";

    case kDNSServiceErr_BadState:
        return "Bad State";

    case kDNSServiceErr_BadFlags:
        return "Bad Flags";

    case kDNSServiceErr_Unsupported:
        return "Unsupported";

    case kDNSServiceErr_NotInitialized:
        return "Not Initialized";

    case kDNSServiceErr_AlreadyRegistered:
        return "Already Registered";

    case kDNSServiceErr_NameConflict:
        return "Name Conflict";

    case kDNSServiceErr_Invalid:
        return "Invalid";

    case kDNSServiceErr_Firewall:
        return "Firewall";

    case kDNSServiceErr_Incompatible:
        // client library incompatible with daemon
        return "Incompatible";

    case kDNSServiceErr_BadInterfaceIndex:
        return "Bad Interface Index";

    case kDNSServiceErr_Refused:
        return "Refused";

    case kDNSServiceErr_NoSuchRecord:
        return "No Such Record";

    case kDNSServiceErr_NoAuth:
        return "No Auth";

    case kDNSServiceErr_NoSuchKey:
        return "No Such Key";

    case kDNSServiceErr_NATTraversal:
        return "NAT Traversal";

    case kDNSServiceErr_DoubleNAT:
        return "Double NAT";

    case kDNSServiceErr_BadTime:
        // Codes up to here existed in Tiger
        return "Bad Time";

    case kDNSServiceErr_BadSig:
        return "Bad Sig";

    case kDNSServiceErr_BadKey:
        return "Bad Key";

    case kDNSServiceErr_Transient:
        return "Transient";

    case kDNSServiceErr_ServiceNotRunning:
        // Background daemon not running
        return "Service Not Running";

    case kDNSServiceErr_NATPortMappingUnsupported:
        // NAT doesn't support NAT-PMP or UPnP
        return "NAT Port Mapping Unsupported";

    case kDNSServiceErr_NATPortMappingDisabled:
        // NAT supports NAT-PMP or UPnP but it's disabled by the administrator
        return "NAT Port Mapping Disabled";

    case kDNSServiceErr_NoRouter:
        // No router currently configured (probably no network connectivity)
        return "No Router";

    case kDNSServiceErr_PollingMode:
        return "Polling Mode";

    case kDNSServiceErr_Timeout:
        return "Timeout";

    default:
        assert(false);
        return nullptr;
    }
}

PublisherMDnsSd::PublisherMDnsSd(int aProtocol, const char *aDomain, StateHandler aHandler, void *aContext)
    : mHostsRef(nullptr)
    , mDomain(aDomain)
    , mState(State::kIdle)
    , mStateHandler(aHandler)
    , mContext(aContext)
{
    OTBR_UNUSED_VARIABLE(aProtocol);
}

PublisherMDnsSd::~PublisherMDnsSd(void)
{
    Stop();
}

otbrError PublisherMDnsSd::Start(void)
{
    mState = State::kReady;
    mStateHandler(mContext, State::kReady);
    return OTBR_ERROR_NONE;
}

bool PublisherMDnsSd::IsStarted(void) const
{
    return mState == State::kReady;
}

void PublisherMDnsSd::Stop(void)
{
    VerifyOrExit(mState == State::kReady);

    for (Service &service : mServices)
    {
        otbrLogInfo("Remove service %s.%s", service.mName, service.mType);
        DNSServiceRefDeallocate(service.mService);
    }
    mServices.clear();

    otbrLogInfo("Remove all hosts");
    if (mHostsRef != nullptr)
    {
        DNSServiceRefDeallocate(mHostsRef);
        mHostsRef = nullptr;
    }

    mHosts.clear();

exit:
    return;
}

void PublisherMDnsSd::Update(MainloopContext &aMainloop)
{
    for (Service &service : mServices)
    {
        assert(service.mService != nullptr);

        int fd = DNSServiceRefSockFD(service.mService);

        assert(fd != -1);

        FD_SET(fd, &aMainloop.mReadFdSet);

        aMainloop.mMaxFd = std::max(aMainloop.mMaxFd, fd);
    }

    if (mHostsRef != nullptr)
    {
        int fd = DNSServiceRefSockFD(mHostsRef);

        assert(fd != -1);

        FD_SET(fd, &aMainloop.mReadFdSet);

        aMainloop.mMaxFd = std::max(aMainloop.mMaxFd, fd);
    }

    for (Subscription &subscription : mSubscribedServices)
    {
        if (subscription.mServiceRef != nullptr)
        {
            int fd = DNSServiceRefSockFD(subscription.mServiceRef);
            assert(fd != -1);

            FD_SET(fd, &aMainloop.mReadFdSet);
            aMainloop.mMaxFd = std::max(aMainloop.mMaxFd, fd);
        }
    }

    for (Subscription &subscription : mSubscribedHosts)
    {
        if (subscription.mServiceRef != nullptr)
        {
            int fd = DNSServiceRefSockFD(subscription.mServiceRef);
            assert(fd != -1);

            FD_SET(fd, &aMainloop.mReadFdSet);
            aMainloop.mMaxFd = std::max(aMainloop.mMaxFd, fd);
        }
    }
}

void PublisherMDnsSd::Process(const MainloopContext &aMainloop)
{
    std::vector<DNSServiceRef> readyServices;

    for (Service &service : mServices)
    {
        int fd = DNSServiceRefSockFD(service.mService);

        if (FD_ISSET(fd, &aMainloop.mReadFdSet))
        {
            readyServices.push_back(service.mService);
        }
    }

    if (mHostsRef != nullptr)
    {
        int fd = DNSServiceRefSockFD(mHostsRef);

        if (FD_ISSET(fd, &aMainloop.mReadFdSet))
        {
            readyServices.push_back(mHostsRef);
        }
    }

    for (Subscription &subscription : mSubscribedServices)
    {
        if (subscription.mServiceRef != nullptr)
        {
            int fd = DNSServiceRefSockFD(subscription.mServiceRef);
            assert(fd != -1);

            if (FD_ISSET(fd, &aMainloop.mReadFdSet))
            {
                readyServices.push_back(subscription.mServiceRef);
            }
        }
    }

    for (Subscription &service : mSubscribedHosts)
    {
        if (service.mServiceRef != nullptr)
        {
            int fd = DNSServiceRefSockFD(service.mServiceRef);
            assert(fd != -1);

            if (FD_ISSET(fd, &aMainloop.mReadFdSet))
            {
                readyServices.push_back(service.mServiceRef);
            }
        }
    }

    for (DNSServiceRef serviceRef : readyServices)
    {
        DNSServiceErrorType error = DNSServiceProcessResult(serviceRef);

        if (error != kDNSServiceErr_NoError)
        {
            otbrLogWarning("DNSServiceProcessResult failed: %s", DNSErrorToString(error));
        }
    }
}

void PublisherMDnsSd::HandleServiceRegisterResult(DNSServiceRef         aService,
                                                  const DNSServiceFlags aFlags,
                                                  DNSServiceErrorType   aError,
                                                  const char *          aName,
                                                  const char *          aType,
                                                  const char *          aDomain,
                                                  void *                aContext)
{
    static_cast<PublisherMDnsSd *>(aContext)->HandleServiceRegisterResult(aService, aFlags, aError, aName, aType,
                                                                          aDomain);
}

void PublisherMDnsSd::HandleServiceRegisterResult(DNSServiceRef         aServiceRef,
                                                  const DNSServiceFlags aFlags,
                                                  DNSServiceErrorType   aError,
                                                  const char *          aName,
                                                  const char *          aType,
                                                  const char *          aDomain)
{
    OTBR_UNUSED_VARIABLE(aDomain);

    otbrError       error = DNSErrorToOtbrError(aError);
    std::string     originalInstanceName;
    ServiceIterator service = FindPublishedService(aServiceRef);

    VerifyOrExit(service != mServices.end());

    // mDNSResponder could auto-rename the service instance name when name conflict
    // is detected. In this case, `aName` may not match `service->mName` and we
    // should use the original `service->mName` to find associated SRP service.
    originalInstanceName = service->mName;

    otbrLogInfo("Received reply for service %s.%s", originalInstanceName.c_str(), aType);

    if (originalInstanceName != aName)
    {
        otbrLogInfo("Service %s.%s renamed to %s.%s", originalInstanceName.c_str(), aType, aName, aType);
    }

    if (aError == kDNSServiceErr_NoError && (aFlags & kDNSServiceFlagsAdd))
    {
        otbrLogInfo("Successfully registered service %s.%s", originalInstanceName.c_str(), aType);
    }
    else
    {
        otbrLogErr("Failed to register service %s.%s: %s", originalInstanceName.c_str(), aType,
                   DNSErrorToString(aError));
        DiscardService(originalInstanceName.c_str(), aType, aServiceRef);
    }

    if (mServiceHandler != nullptr)
    {
        // TODO: pass the renewed service instance name back to SRP server handler.
        mServiceHandler(originalInstanceName.c_str(), aType, error, mServiceHandlerContext);
    }

exit:
    return;
}

void PublisherMDnsSd::DiscardService(const char *aName, const char *aType, DNSServiceRef aServiceRef)
{
    OTBR_UNUSED_VARIABLE(aServiceRef);

    ServiceIterator service = FindPublishedService(aName, aType);

    if (service != mServices.end())
    {
        assert(aServiceRef == nullptr || aServiceRef == service->mService);

        otbrLogInfo("Remove service ref %p", service->mService);

        DNSServiceRefDeallocate(service->mService);
        mServices.erase(service);
    }
}

void PublisherMDnsSd::RecordService(const char *  aName,
                                    const char *  aType,
                                    const char *  aRegType,
                                    DNSServiceRef aServiceRef)
{
    ServiceIterator service = FindPublishedService(aName, aType);

    if (service == mServices.end())
    {
        Service newService;

        otbrLogInfo("Add service: %s.%s (ref: %p)", aName, aType, aServiceRef);

        strcpy(newService.mName, aName);
        strcpy(newService.mType, aType);
        newService.mRegType = aRegType;
        newService.mService = aServiceRef;
        mServices.push_back(newService);
    }
    else
    {
        assert(service->mRegType == aRegType);
        assert(service->mService == aServiceRef);
    }
}

bool PublisherMDnsSd::IsServiceOutdated(const Service &service, const std::string &aNewRegType)
{
    return service.mRegType != aNewRegType;
}

otbrError PublisherMDnsSd::PublishService(const char *       aHostName,
                                          uint16_t           aPort,
                                          const char *       aName,
                                          const char *       aType,
                                          const SubTypeList &aSubTypeList,
                                          const TxtList &    aTxtList)
{
    otbrError       ret   = OTBR_ERROR_NONE;
    int             error = 0;
    uint8_t         txt[kMaxSizeOfTxtRecord];
    uint16_t        txtLength  = sizeof(txt);
    std::string     regType    = MakeRegType(aType, aSubTypeList);
    ServiceIterator service    = FindPublishedService(aName, aType);
    DNSServiceRef   serviceRef = nullptr;
    char            fullHostName[kMaxSizeOfDomain];

    if (aHostName != nullptr)
    {
        HostIterator host = FindPublishedHost(aHostName);

        // Make sure that the host has been published.
        VerifyOrExit(host != mHosts.end(), ret = OTBR_ERROR_INVALID_ARGS);
        SuccessOrExit(error = MakeFullName(fullHostName, sizeof(fullHostName), aHostName));
    }

    SuccessOrExit(ret = EncodeTxtData(aTxtList, txt, txtLength));

    if (service != mServices.end() && !IsServiceOutdated(*service, regType))
    {
        otbrLogInfo("Update service %s.%s", aName, aType);

        // Setting TTL to 0 to use default value.
        SuccessOrExit(error = DNSServiceUpdateRecord(service->mService, nullptr, 0, txtLength, txt, /* ttl */ 0));

        if (mServiceHandler != nullptr)
        {
            mServiceHandler(aName, aType, DNSErrorToOtbrError(error), mServiceHandlerContext);
        }

        ExitNow();
    }

    if (service != mServices.end())
    {
        // Service is outdated and needs to be recreated.
        DiscardService(aName, aType, serviceRef);
    }

    SuccessOrExit(error = DNSServiceRegister(&serviceRef, /* flags */ 0, kDNSServiceInterfaceIndexAny, aName,
                                             regType.c_str(), mDomain, (aHostName != nullptr) ? fullHostName : nullptr,
                                             htons(aPort), txtLength, txt, HandleServiceRegisterResult, this));
    RecordService(aName, aType, regType.c_str(), serviceRef);

exit:
    if (error != kDNSServiceErr_NoError)
    {
        ret = OTBR_ERROR_MDNS;
        otbrLogErr("Failed to publish service for mdnssd error: %s!", DNSErrorToString(error));
    }
    return ret;
}

otbrError PublisherMDnsSd::UnpublishService(const char *aName, const char *aType)
{
    DiscardService(aName, aType);
    return OTBR_ERROR_NONE;
}

otbrError PublisherMDnsSd::DiscardHost(const char *aName, bool aSendGoodbye)
{
    otbrError    ret   = OTBR_ERROR_NONE;
    int          error = 0;
    HostIterator host  = FindPublishedHost(aName);

    VerifyOrExit(mHostsRef != nullptr && host != mHosts.end());

    otbrLogInfo("Remove host: %s (record ref: %p)", host->mName, host->mRecord);

    if (aSendGoodbye)
    {
        // The Bonjour mDNSResponder somehow doesn't send goodbye message for the AAAA record when it is
        // removed by `DNSServiceRemoveRecord`. Per RFC 6762, a goodbye message of a record sets its TTL
        // to zero but the receiver should record the TTL of 1 and flushes the cache 1 second later. Here
        // we remove the AAAA record after updating its TTL to 1 second. This has the same effect as
        // sending a goodbye message.
        // TODO: resolve the goodbye issue with Bonjour mDNSResponder.
        error = DNSServiceUpdateRecord(mHostsRef, host->mRecord, kDNSServiceFlagsUnique, host->mAddress.size(),
                                       &host->mAddress.front(), /* ttl */ 1);
        // Do not SuccessOrExit so that we always erase the host entry.

        DNSServiceRemoveRecord(mHostsRef, host->mRecord, /* flags */ 0);
    }
    mHosts.erase(host);

exit:
    if (error != kDNSServiceErr_NoError)
    {
        ret = OTBR_ERROR_MDNS;
        otbrLogErr("Failed to remove host %s for mdnssd error: %s!", aName, DNSErrorToString(error));
    }
    return ret;
}

void PublisherMDnsSd::RecordHost(const char *   aName,
                                 const uint8_t *aAddress,
                                 uint8_t        aAddressLength,
                                 DNSRecordRef   aRecordRef)
{
    HostIterator host = FindPublishedHost(aName);

    if (host == mHosts.end())
    {
        Host newHost;

        otbrLogInfo("Add new host %s", aName);

        strcpy(newHost.mName, aName);
        std::copy(aAddress, aAddress + aAddressLength, newHost.mAddress.begin());
        newHost.mRecord = aRecordRef;
        mHosts.push_back(newHost);
    }
    else
    {
        otbrLogInfo("Update existing host %s", host->mName);

        // The address of the host may be updated.
        std::copy(aAddress, aAddress + aAddressLength, host->mAddress.begin());
        assert(host->mRecord == aRecordRef);
    }
}

otbrError PublisherMDnsSd::PublishHost(const char *aName, const uint8_t *aAddress, uint8_t aAddressLength)
{
    otbrError    ret   = OTBR_ERROR_NONE;
    int          error = 0;
    char         fullName[kMaxSizeOfDomain];
    HostIterator host = FindPublishedHost(aName);

    // Supports only IPv6 for now, may support IPv4 in the future.
    VerifyOrExit(aAddressLength == OTBR_IP6_ADDRESS_SIZE, error = OTBR_ERROR_INVALID_ARGS);

    SuccessOrExit(ret = MakeFullName(fullName, sizeof(fullName), aName));

    if (mHostsRef == nullptr)
    {
        SuccessOrExit(error = DNSServiceCreateConnection(&mHostsRef));
    }

    if (host != mHosts.end())
    {
        otbrLogInfo("Update existing host %s", aName);
        SuccessOrExit(error = DNSServiceUpdateRecord(mHostsRef, host->mRecord, kDNSServiceFlagsUnique, aAddressLength,
                                                     aAddress, /* ttl */ 0));

        RecordHost(aName, aAddress, aAddressLength, host->mRecord);
        if (mHostHandler != nullptr)
        {
            mHostHandler(aName, DNSErrorToOtbrError(error), mHostHandlerContext);
        }
    }
    else
    {
        DNSRecordRef record;

        otbrLogInfo("Publish new host %s", aName);
        SuccessOrExit(error = DNSServiceRegisterRecord(mHostsRef, &record, kDNSServiceFlagsUnique,
                                                       kDNSServiceInterfaceIndexAny, fullName, kDNSServiceType_AAAA,
                                                       kDNSServiceClass_IN, aAddressLength, aAddress, /* ttl */ 0,
                                                       HandleRegisterHostResult, this));
        RecordHost(aName, aAddress, aAddressLength, record);
    }

exit:
    if (error != kDNSServiceErr_NoError)
    {
        if (mHostsRef != nullptr)
        {
            DNSServiceRefDeallocate(mHostsRef);
            mHostsRef = nullptr;
        }

        ret = OTBR_ERROR_MDNS;
        otbrLogErr("Failed to publish/update host %s for mdnssd error: %s!", aName, DNSErrorToString(error));
    }
    return ret;
}

otbrError PublisherMDnsSd::UnpublishHost(const char *aName)
{
    return DiscardHost(aName);
}

void PublisherMDnsSd::HandleRegisterHostResult(DNSServiceRef       aHostsConnection,
                                               DNSRecordRef        aHostRecord,
                                               DNSServiceFlags     aFlags,
                                               DNSServiceErrorType aErrorCode,
                                               void *              aContext)
{
    static_cast<PublisherMDnsSd *>(aContext)->HandleRegisterHostResult(aHostsConnection, aHostRecord, aFlags,
                                                                       aErrorCode);
}

void PublisherMDnsSd::HandleRegisterHostResult(DNSServiceRef       aHostsConnection,
                                               DNSRecordRef        aHostRecord,
                                               DNSServiceFlags     aFlags,
                                               DNSServiceErrorType aErrorCode)
{
    OTBR_UNUSED_VARIABLE(aHostsConnection);
    OTBR_UNUSED_VARIABLE(aFlags);

    HostIterator host = FindPublishedHost(aHostRecord);
    std::string  hostName;

    VerifyOrExit(host != mHosts.end());
    hostName = host->mName;

    otbrLogInfo("Received reply for host %s", hostName.c_str());

    if (aErrorCode == kDNSServiceErr_NoError)
    {
        otbrLogInfo("Successfully registered host %s", hostName.c_str());
    }
    else
    {
        otbrLogWarning("failed to register host %s for mdnssd error: %s", hostName.c_str(),
                       DNSErrorToString(aErrorCode));

        DiscardHost(hostName.c_str(), /* aSendGoodbye */ false);
    }

    if (mHostHandler != nullptr)
    {
        mHostHandler(hostName.c_str(), DNSErrorToOtbrError(aErrorCode), mHostHandlerContext);
    }

exit:
    return;
}

otbrError PublisherMDnsSd::MakeFullName(char *aFullName, size_t aFullNameLength, const char *aName)
{
    otbrError   error      = OTBR_ERROR_NONE;
    size_t      nameLength = strlen(aName);
    const char *domain     = (mDomain == nullptr) ? "local." : mDomain;

    VerifyOrExit(nameLength <= kMaxSizeOfHost, error = OTBR_ERROR_INVALID_ARGS);

    assert(aFullNameLength >= nameLength + sizeof(".") + strlen(domain));
    OTBR_UNUSED_VARIABLE(aFullNameLength);

    strcpy(aFullName, aName);
    strcpy(aFullName + nameLength, ".");
    strcpy(aFullName + nameLength + 1, domain);

exit:
    return error;
}

std::string PublisherMDnsSd::MakeRegType(const char *aType, const SubTypeList &aSubTypeList)
{
    // See `regtype` parameter of the DNSServiceRegister() function for more information.
    std::string fullType(aType);

    for (const std::string &subType : aSubTypeList)
    {
        fullType += ',';
        fullType += subType;
    }

    return fullType;
}

PublisherMDnsSd::ServiceIterator PublisherMDnsSd::FindPublishedService(const char *aName, const char *aType)
{
    return std::find_if(mServices.begin(), mServices.end(), [&aName, aType](const Service &service) {
        return strcmp(aName, service.mName) == 0 && IsServiceTypeEqual(aType, service.mType);
    });
}

PublisherMDnsSd::ServiceIterator PublisherMDnsSd::FindPublishedService(const DNSServiceRef &aServiceRef)
{
    return std::find_if(mServices.begin(), mServices.end(),
                        [&aServiceRef](const Service &service) { return service.mService == aServiceRef; });
}

PublisherMDnsSd::HostIterator PublisherMDnsSd::FindPublishedHost(const DNSRecordRef &aRecordRef)
{
    return std::find_if(mHosts.begin(), mHosts.end(),
                        [&aRecordRef](const Host &host) { return host.mRecord == aRecordRef; });
}

PublisherMDnsSd::HostIterator PublisherMDnsSd::FindPublishedHost(const char *aHostName)
{
    return std::find_if(mHosts.begin(), mHosts.end(),
                        [&aHostName](const Host &host) { return strcmp(host.mName, aHostName) == 0; });
}

void PublisherMDnsSd::SubscribeService(const std::string &aType, const std::string &aInstanceName)
{
    mSubscribedServices.emplace_back(*this, aType, aInstanceName);

    otbrLogInfo("subscribe service %s.%s (total %zu)", aInstanceName.c_str(), aType.c_str(),
                mSubscribedServices.size());

    if (aInstanceName.empty())
    {
        mSubscribedServices.back().Browse();
    }
    else
    {
        mSubscribedServices.back().Resolve(kDNSServiceInterfaceIndexAny, aInstanceName.c_str(), aType.c_str(),
                                           "local.");
    }
}

void PublisherMDnsSd::UnsubscribeService(const std::string &aType, const std::string &aInstanceName)
{
    ServiceSubscriptionList::iterator it =
        std::find_if(mSubscribedServices.begin(), mSubscribedServices.end(),
                     [&aType, &aInstanceName](const ServiceSubscription &aService) {
                         return aService.mType == aType && aService.mInstanceName == aInstanceName;
                     });

    assert(it != mSubscribedServices.end());

    it->Release();
    mSubscribedServices.erase(it);

    otbrLogInfo("unsubscribe service %s.%s (left %zu)", aInstanceName.c_str(), aType.c_str(),
                mSubscribedServices.size());
}

void PublisherMDnsSd::OnServiceResolved(PublisherMDnsSd::ServiceSubscription &aService)
{
    otbrLogInfo("Service %s is resolved successfully: %s host %s addresses %zu", aService.mType.c_str(),
                aService.mInstanceInfo.mName.c_str(), aService.mInstanceInfo.mHostName.c_str(),
                aService.mInstanceInfo.mAddresses.size());

    if (mDiscoveredServiceInstanceCallback != nullptr)
    {
        mDiscoveredServiceInstanceCallback(aService.mType, aService.mInstanceInfo);
    }
}

void PublisherMDnsSd::OnServiceResolveFailed(const ServiceSubscription &aService, DNSServiceErrorType aErrorCode)
{
    otbrLogWarning("Service %s resolving failed: code=%d", aService.mType.c_str(), aErrorCode);
}

void PublisherMDnsSd::OnHostResolved(PublisherMDnsSd::HostSubscription &aHost)
{
    otbrLogInfo("Host %s is resolved successfully: host %s addresses %zu ttl %u", aHost.mHostName.c_str(),
                aHost.mHostInfo.mHostName.c_str(), aHost.mHostInfo.mAddresses.size(), aHost.mHostInfo.mTtl);

    if (mDiscoveredHostCallback != nullptr)
    {
        mDiscoveredHostCallback(aHost.mHostName, aHost.mHostInfo);
    }
}

void PublisherMDnsSd::OnHostResolveFailed(const PublisherMDnsSd::HostSubscription &aHost,
                                          DNSServiceErrorType                      aErrorCode)
{
    otbrLogWarning("Host %s resolving failed: code=%d", aHost.mHostName.c_str(), aErrorCode);
}

void PublisherMDnsSd::SubscribeHost(const std::string &aHostName)
{
    mSubscribedHosts.emplace_back(*this, aHostName);

    otbrLogInfo("subscribe host %s (total %zu)", aHostName.c_str(), mSubscribedHosts.size());

    mSubscribedHosts.back().Resolve();
}

void PublisherMDnsSd::UnsubscribeHost(const std::string &aHostName)
{
    HostSubscriptionList ::iterator it =
        std::find_if(mSubscribedHosts.begin(), mSubscribedHosts.end(),
                     [&aHostName](const HostSubscription &aHost) { return aHost.mHostName == aHostName; });

    assert(it != mSubscribedHosts.end());

    it->Release();
    mSubscribedHosts.erase(it);

    otbrLogInfo("unsubscribe host %s (remaining %d)", aHostName.c_str(), mSubscribedHosts.size());
}

Publisher *Publisher::Create(int aFamily, const char *aDomain, StateHandler aHandler, void *aContext)
{
    return new PublisherMDnsSd(aFamily, aDomain, aHandler, aContext);
}

void Publisher::Destroy(Publisher *aPublisher)
{
    delete static_cast<PublisherMDnsSd *>(aPublisher);
}

void PublisherMDnsSd::Subscription::Release(void)
{
    DeallocateServiceRef();
}

void PublisherMDnsSd::Subscription::DeallocateServiceRef(void)
{
    if (mServiceRef != nullptr)
    {
        DNSServiceRefDeallocate(mServiceRef);
        mServiceRef = nullptr;
    }
}

void PublisherMDnsSd::ServiceSubscription::Browse(void)
{
    assert(mServiceRef == nullptr);

    otbrLogInfo("DNSServiceBrowse %s", mType.c_str());
    DNSServiceBrowse(&mServiceRef, /* flags */ kDNSServiceFlagsTimeout, kDNSServiceInterfaceIndexAny, mType.c_str(),
                     /* domain */ nullptr, HandleBrowseResult, this);
}

void PublisherMDnsSd::ServiceSubscription::HandleBrowseResult(DNSServiceRef       aServiceRef,
                                                              DNSServiceFlags     aFlags,
                                                              uint32_t            aInterfaceIndex,
                                                              DNSServiceErrorType aErrorCode,
                                                              const char *        aInstanceName,
                                                              const char *        aType,
                                                              const char *        aDomain,
                                                              void *              aContext)
{
    static_cast<ServiceSubscription *>(aContext)->HandleBrowseResult(aServiceRef, aFlags, aInterfaceIndex, aErrorCode,
                                                                     aInstanceName, aType, aDomain);
}

void PublisherMDnsSd::ServiceSubscription::HandleBrowseResult(DNSServiceRef       aServiceRef,
                                                              DNSServiceFlags     aFlags,
                                                              uint32_t            aInterfaceIndex,
                                                              DNSServiceErrorType aErrorCode,
                                                              const char *        aInstanceName,
                                                              const char *        aType,
                                                              const char *        aDomain)
{
    OTBR_UNUSED_VARIABLE(aServiceRef);

    otbrLogInfo("DNSServiceBrowse reply: %s.%s%s inf %u, flags=%u, error=%d", aInstanceName, aType, aDomain,
                aInterfaceIndex, aFlags, aErrorCode);

    VerifyOrExit(aErrorCode == kDNSServiceErr_NoError);
    VerifyOrExit(aFlags & kDNSServiceFlagsAdd);

    DeallocateServiceRef();
    Resolve(aInterfaceIndex, aInstanceName, aType, aDomain);

exit:
    if (aErrorCode != kDNSServiceErr_NoError)
    {
        mMDnsSd->OnServiceResolveFailed(*this, aErrorCode);
    }
    else if (!(aFlags & (kDNSServiceFlagsAdd | kDNSServiceFlagsMoreComing)))
    {
        mMDnsSd->OnServiceResolveFailed(*this, kDNSServiceErr_NoSuchName);
    }
}

void PublisherMDnsSd::ServiceSubscription::Resolve(uint32_t    aInterfaceIndex,
                                                   const char *aInstanceName,
                                                   const char *aType,
                                                   const char *aDomain)
{
    assert(mServiceRef == nullptr);

    otbrLogInfo("DNSServiceResolve %s %s %s inf %d", aInstanceName, aType, aDomain, aInterfaceIndex);
    DNSServiceResolve(&mServiceRef, /* flags */ 0, aInterfaceIndex, aInstanceName, aType, aDomain, HandleResolveResult,
                      this);
}

void PublisherMDnsSd::ServiceSubscription::HandleResolveResult(DNSServiceRef        aServiceRef,
                                                               DNSServiceFlags      aFlags,
                                                               uint32_t             aInterfaceIndex,
                                                               DNSServiceErrorType  aErrorCode,
                                                               const char *         aFullName,
                                                               const char *         aHostTarget,
                                                               uint16_t             aPort,
                                                               uint16_t             aTxtLen,
                                                               const unsigned char *aTxtRecord,
                                                               void *               aContext)
{
    static_cast<ServiceSubscription *>(aContext)->HandleResolveResult(
        aServiceRef, aFlags, aInterfaceIndex, aErrorCode, aFullName, aHostTarget, aPort, aTxtLen, aTxtRecord);
}

void PublisherMDnsSd::ServiceSubscription::HandleResolveResult(DNSServiceRef        aServiceRef,
                                                               DNSServiceFlags      aFlags,
                                                               uint32_t             aInterfaceIndex,
                                                               DNSServiceErrorType  aErrorCode,
                                                               const char *         aFullName,
                                                               const char *         aHostTarget,
                                                               uint16_t             aPort,
                                                               uint16_t             aTxtLen,
                                                               const unsigned char *aTxtRecord)
{
    OTBR_UNUSED_VARIABLE(aServiceRef);

    std::string instanceName, type, domain;
    otbrError   error = OTBR_ERROR_NONE;

    otbrLogInfo("DNSServiceResolve reply: %s host %s:%d, TXT=%dB inf %u, flags=%u", aFullName, aHostTarget, aPort,
                aTxtLen, aInterfaceIndex, aFlags);

    VerifyOrExit(aErrorCode == kDNSServiceErr_NoError);

    SuccessOrExit(error = SplitFullServiceInstanceName(aFullName, instanceName, type, domain));

    mInstanceInfo.mName     = instanceName;
    mInstanceInfo.mHostName = aHostTarget;
    mInstanceInfo.mPort     = ntohs(aPort);
    mInstanceInfo.mTxtData.assign(aTxtRecord, aTxtRecord + aTxtLen);
    // priority and weight are not given in the reply
    mInstanceInfo.mPriority = 0;
    mInstanceInfo.mWeight   = 0;

    DeallocateServiceRef();
    GetAddrInfo(aInterfaceIndex);

exit:
    if (aErrorCode != kDNSServiceErr_NoError || error != OTBR_ERROR_NONE)
    {
        mMDnsSd->OnServiceResolveFailed(*this, aErrorCode);
    }

    if (error != OTBR_ERROR_NONE)
    {
        otbrLogWarning("failed to resolve service instance %s", aFullName);
    }
}

void PublisherMDnsSd::ServiceSubscription::GetAddrInfo(uint32_t aInterfaceIndex)
{
    assert(mServiceRef == nullptr);

    otbrLogInfo("DNSServiceGetAddrInfo %s inf %d", mInstanceInfo.mHostName.c_str(), aInterfaceIndex);

    DNSServiceGetAddrInfo(&mServiceRef, /* flags */ 0, aInterfaceIndex,
                          kDNSServiceProtocol_IPv6 | kDNSServiceProtocol_IPv4, mInstanceInfo.mHostName.c_str(),
                          HandleGetAddrInfoResult, this);
}

void PublisherMDnsSd::ServiceSubscription::HandleGetAddrInfoResult(DNSServiceRef          aServiceRef,
                                                                   DNSServiceFlags        aFlags,
                                                                   uint32_t               aInterfaceIndex,
                                                                   DNSServiceErrorType    aErrorCode,
                                                                   const char *           aHostName,
                                                                   const struct sockaddr *aAddress,
                                                                   uint32_t               aTtl,
                                                                   void *                 aContext)
{
    static_cast<ServiceSubscription *>(aContext)->HandleGetAddrInfoResult(aServiceRef, aFlags, aInterfaceIndex,
                                                                          aErrorCode, aHostName, aAddress, aTtl);
}

void PublisherMDnsSd::ServiceSubscription::HandleGetAddrInfoResult(DNSServiceRef          aServiceRef,
                                                                   DNSServiceFlags        aFlags,
                                                                   uint32_t               aInterfaceIndex,
                                                                   DNSServiceErrorType    aErrorCode,
                                                                   const char *           aHostName,
                                                                   const struct sockaddr *aAddress,
                                                                   uint32_t               aTtl)
{
    OTBR_UNUSED_VARIABLE(aServiceRef);
    OTBR_UNUSED_VARIABLE(aInterfaceIndex);

    Ip6Address address;

    otbrLogDebug("DNSServiceGetAddrInfo reply: %d, flags=%u, host=%s, sa_family=%d", aErrorCode, aFlags, aHostName,
                 aAddress->sa_family);

    VerifyOrExit(aErrorCode == kDNSServiceErr_NoError);
    VerifyOrExit((aFlags & kDNSServiceFlagsAdd) && aAddress->sa_family == AF_INET6);

    address.CopyFrom(*reinterpret_cast<const struct sockaddr_in6 *>(aAddress));
    VerifyOrExit(!address.IsUnspecified() && !address.IsLinkLocal() && !address.IsMulticast() && !address.IsLoopback(),
                 otbrLogDebug("DNSServiceGetAddrInfo ignores address %s", address.ToString().c_str()));

    mInstanceInfo.mAddresses.push_back(address);
    mInstanceInfo.mTtl = aTtl;

    otbrLogDebug("DNSServiceGetAddrInfo reply: address=%s, ttl=%u", address.ToString().c_str(), aTtl);

    mMDnsSd->OnServiceResolved(*this);

exit:
    if (aErrorCode != kDNSServiceErr_NoError)
    {
        otbrLogWarning("DNSServiceGetAddrInfo failed: %d", aErrorCode);

        mMDnsSd->OnServiceResolveFailed(*this, aErrorCode);
    }
    else if (mInstanceInfo.mAddresses.empty() && (aFlags & kDNSServiceFlagsMoreComing) == 0)
    {
        otbrLogDebug("DNSServiceGetAddrInfo reply: no IPv6 address found");
        mInstanceInfo.mTtl = aTtl;
        mMDnsSd->OnServiceResolved(*this);
    }
}

void PublisherMDnsSd::HostSubscription::Resolve(void)
{
    std::string fullHostName = mHostName + ".local.";

    assert(mServiceRef == nullptr);

    otbrLogDebug("DNSServiceGetAddrInfo %s inf %d", fullHostName.c_str(), kDNSServiceInterfaceIndexAny);

    DNSServiceGetAddrInfo(&mServiceRef, /* flags */ 0, kDNSServiceInterfaceIndexAny,
                          kDNSServiceProtocol_IPv6 | kDNSServiceProtocol_IPv4, fullHostName.c_str(),
                          HandleResolveResult, this);
}

void PublisherMDnsSd::HostSubscription::HandleResolveResult(DNSServiceRef          aServiceRef,
                                                            DNSServiceFlags        aFlags,
                                                            uint32_t               aInterfaceIndex,
                                                            DNSServiceErrorType    aErrorCode,
                                                            const char *           aHostName,
                                                            const struct sockaddr *aAddress,
                                                            uint32_t               aTtl,
                                                            void *                 aContext)
{
    static_cast<HostSubscription *>(aContext)->HandleResolveResult(aServiceRef, aFlags, aInterfaceIndex, aErrorCode,
                                                                   aHostName, aAddress, aTtl);
}

void PublisherMDnsSd::HostSubscription::HandleResolveResult(DNSServiceRef          aServiceRef,
                                                            DNSServiceFlags        aFlags,
                                                            uint32_t               aInterfaceIndex,
                                                            DNSServiceErrorType    aErrorCode,
                                                            const char *           aHostName,
                                                            const struct sockaddr *aAddress,
                                                            uint32_t               aTtl)
{
    OTBR_UNUSED_VARIABLE(aServiceRef);
    OTBR_UNUSED_VARIABLE(aInterfaceIndex);

    Ip6Address address;

    otbrLogDebug("DNSServiceGetAddrInfo reply: %d, flags=%u, host=%s, sa_family=%d", aErrorCode, aFlags, aHostName,
                 aAddress->sa_family);

    VerifyOrExit(aErrorCode == kDNSServiceErr_NoError);
    VerifyOrExit((aFlags & kDNSServiceFlagsAdd) && aAddress->sa_family == AF_INET6);

    address.CopyFrom(*reinterpret_cast<const struct sockaddr_in6 *>(aAddress));
    VerifyOrExit(!address.IsLinkLocal(),
                 otbrLogDebug("DNSServiceGetAddrInfo ignore link-local address %s", address.ToString().c_str()));

    mHostInfo.mHostName = aHostName;
    mHostInfo.mAddresses.push_back(address);
    mHostInfo.mTtl = aTtl;

    otbrLogDebug("DNSServiceGetAddrInfo reply: address=%s, ttl=%u", address.ToString().c_str(), aTtl);

    mMDnsSd->OnHostResolved(*this);

exit:
    if (aErrorCode != kDNSServiceErr_NoError)
    {
        otbrLogWarning("DNSServiceGetAddrInfo failed: %d", aErrorCode);

        mMDnsSd->OnHostResolveFailed(*this, aErrorCode);
    }
    else if (mHostInfo.mAddresses.empty() && (aFlags & kDNSServiceFlagsMoreComing) == 0)
    {
        otbrLogDebug("DNSServiceGetAddrInfo reply: no IPv6 address found");
        mHostInfo.mTtl = aTtl;
        mMDnsSd->OnHostResolved(*this);
    }
}

} // namespace Mdns

} // namespace otbr
