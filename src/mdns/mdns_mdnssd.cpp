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

#include "mdns/mdns_mdnssd.hpp"

#include <algorithm>

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/code_utils.hpp"
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
        otbrLog(OTBR_LOG_INFO, "[mdns] remove service %s.%s", service.mName, service.mType);
        DNSServiceRefDeallocate(service.mService);
    }
    mServices.clear();

    otbrLog(OTBR_LOG_INFO, "[mdns] remove all hosts");
    DNSServiceRefDeallocate(mHostsRef);
    mHostsRef = nullptr;
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

    for (DNSServiceRef serviceRef : readyServices)
    {
        DNSServiceErrorType error = DNSServiceProcessResult(serviceRef);

        if (error != kDNSServiceErr_NoError)
        {
            otbrLog(OTBR_LOG_WARNING, "[mdns] DNSServiceProcessResult failed: %s", DNSErrorToString(error));
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

    otbrLog(OTBR_LOG_INFO, "[mdns] received reply for service %s.%s", originalInstanceName.c_str(), aType);

    if (originalInstanceName != aName)
    {
        otbrLog(OTBR_LOG_INFO, "[mdns] service %s.%s renamed to %s.%s", originalInstanceName.c_str(), aType, aName,
                aType);
    }

    if (aError == kDNSServiceErr_NoError)
    {
        otbrLog(OTBR_LOG_INFO, "[mdns] successfully registered service %s.%s", originalInstanceName.c_str(), aType);
        if (aFlags & kDNSServiceFlagsAdd)
        {
            RecordService(originalInstanceName.c_str(), aType, aServiceRef);
        }
        else
        {
            DiscardService(originalInstanceName.c_str(), aType, aServiceRef);
        }
    }
    else
    {
        otbrLog(OTBR_LOG_ERR, "[mdns] failed to register service %s.%s: %s", originalInstanceName.c_str(), aType,
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
    ServiceIterator service = FindPublishedService(aName, aType);

    if (service != mServices.end())
    {
        assert(aServiceRef == nullptr || aServiceRef == service->mService);

        otbrLog(OTBR_LOG_INFO, "[mdns] remove service ref %p", service->mService);

        DNSServiceRefDeallocate(service->mService);
        mServices.erase(service);
    }
}

void PublisherMDnsSd::RecordService(const char *aName, const char *aType, DNSServiceRef aServiceRef)
{
    ServiceIterator service = FindPublishedService(aName, aType);

    if (service == mServices.end())
    {
        Service newService;

        otbrLog(OTBR_LOG_INFO, "[mdns] add service: %s.%s (ref: %p)", aName, aType, aServiceRef);

        strcpy(newService.mName, aName);
        strcpy(newService.mType, aType);
        newService.mService = aServiceRef;
        mServices.push_back(newService);
    }
    else
    {
        assert(service->mService == aServiceRef);
    }
}

otbrError PublisherMDnsSd::PublishService(const char *   aHostName,
                                          uint16_t       aPort,
                                          const char *   aName,
                                          const char *   aType,
                                          const TxtList &aTxtList)
{
    otbrError       ret   = OTBR_ERROR_NONE;
    int             error = 0;
    uint8_t         txt[kMaxSizeOfTxtRecord];
    uint16_t        txtLength  = sizeof(txt);
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

    if (service != mServices.end())
    {
        otbrLog(OTBR_LOG_INFO, "[mdns] update service %s.%s", aName, aType);

        // Setting TTL to 0 to use default value.
        SuccessOrExit(error = DNSServiceUpdateRecord(service->mService, nullptr, 0, txtLength, txt, /* ttl */ 0));

        if (mServiceHandler != nullptr)
        {
            mServiceHandler(aName, aType, DNSErrorToOtbrError(error), mServiceHandlerContext);
        }
    }
    else
    {
        SuccessOrExit(error = DNSServiceRegister(&serviceRef, /* flags */ 0, kDNSServiceInterfaceIndexAny, aName, aType,
                                                 mDomain, (aHostName != nullptr) ? fullHostName : nullptr, htons(aPort),
                                                 txtLength, txt, HandleServiceRegisterResult, this));
        RecordService(aName, aType, serviceRef);
    }

exit:
    if (error != kDNSServiceErr_NoError)
    {
        ret = OTBR_ERROR_MDNS;
        otbrLog(OTBR_LOG_ERR, "[mdns] failed to publish service for mdnssd error: %s!", DNSErrorToString(error));
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

    otbrLog(OTBR_LOG_INFO, "[mdns] remove host: %s (record ref: %p)", host->mName, host->mRecord);

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
        otbrLog(OTBR_LOG_ERR, "[mdns] failed to remove host %s for mdnssd error: %s!", aName, DNSErrorToString(error));
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

        otbrLog(OTBR_LOG_INFO, "[mdns] add new host %s", aName);

        strcpy(newHost.mName, aName);
        std::copy(aAddress, aAddress + aAddressLength, newHost.mAddress.begin());
        newHost.mRecord = aRecordRef;
        mHosts.push_back(newHost);
    }
    else
    {
        otbrLog(OTBR_LOG_INFO, "[mdns] update existing host %s", host->mName);

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
        otbrLog(OTBR_LOG_INFO, "[mdns] update existing host %s", aName);
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

        otbrLog(OTBR_LOG_INFO, "[mdns] publish new host %s", aName);
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
        otbrLog(OTBR_LOG_ERR, "[mdns] failed to publish/update host %s for mdnssd error: %s!", aName,
                DNSErrorToString(error));
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

    otbrLog(OTBR_LOG_INFO, "[mdns] received reply for host %s", hostName.c_str());

    if (aErrorCode == kDNSServiceErr_NoError)
    {
        otbrLog(OTBR_LOG_INFO, "[mdns] successfully registered host %s", hostName.c_str());
    }
    else
    {
        otbrLog(OTBR_LOG_WARNING, "[mdns] failed to register host %s for mdnssd error: %s", hostName.c_str(),
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

    strcpy(aFullName, aName);
    strcpy(aFullName + nameLength, ".");
    strcpy(aFullName + nameLength + 1, domain);

exit:
    return error;
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

Publisher *Publisher::Create(int aFamily, const char *aDomain, StateHandler aHandler, void *aContext)
{
    return new PublisherMDnsSd(aFamily, aDomain, aHandler, aContext);
}

void Publisher::Destroy(Publisher *aPublisher)
{
    delete static_cast<PublisherMDnsSd *>(aPublisher);
}

} // namespace Mdns

} // namespace otbr
