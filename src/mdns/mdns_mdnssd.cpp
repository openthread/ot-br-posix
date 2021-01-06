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
 *   This file implements MDNS service based on avahi.
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
    : mHostsConnection(nullptr)
    , mDomain(aDomain)
    , mState(State::kIdle)
    , mStateHandler(aHandler)
    , mContext(aContext)
{
    (void)aProtocol;
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

    for (Services::iterator it = mServices.begin(); it != mServices.end(); ++it)
    {
        otbrLog(OTBR_LOG_INFO, "MDNS remove service %s", it->mName);
        DNSServiceRefDeallocate(it->mService);
    }

    mServices.clear();

    if (mHostsConnection != nullptr)
    {
        otbrLog(OTBR_LOG_INFO, "MDNS remove all hosts");

        // This effectively removes all hosts registered on this connection.
        DNSServiceRefDeallocate(mHostsConnection);
        mHostsConnection = nullptr;
        mHosts.clear();
    }

exit:
    return;
}

void PublisherMDnsSd::UpdateFdSet(fd_set & aReadFdSet,
                                  fd_set & aWriteFdSet,
                                  fd_set & aErrorFdSet,
                                  int &    aMaxFd,
                                  timeval &aTimeout)
{
    (void)aWriteFdSet;
    (void)aErrorFdSet;
    (void)aTimeout;

    for (Services::iterator it = mServices.begin(); it != mServices.end(); ++it)
    {
        int fd = DNSServiceRefSockFD(it->mService);

        assert(fd != -1);

        FD_SET(fd, &aReadFdSet);

        if (fd > aMaxFd)
        {
            aMaxFd = fd;
        }
    }

    if (mHostsConnection != nullptr)
    {
        int fd = DNSServiceRefSockFD(mHostsConnection);

        assert(fd != -1);

        FD_SET(fd, &aReadFdSet);

        if (fd > aMaxFd)
        {
            aMaxFd = fd;
        }
    }
}

void PublisherMDnsSd::Process(const fd_set &aReadFdSet, const fd_set &aWriteFdSet, const fd_set &aErrorFdSet)
{
    std::vector<DNSServiceRef> readyServices;

    (void)aWriteFdSet;
    (void)aErrorFdSet;

    for (Services::iterator it = mServices.begin(); it != mServices.end(); ++it)
    {
        int fd = DNSServiceRefSockFD(it->mService);

        if (FD_ISSET(fd, &aReadFdSet))
        {
            readyServices.push_back(it->mService);
        }
    }

    if (mHostsConnection != nullptr)
    {
        int fd = DNSServiceRefSockFD(mHostsConnection);

        if (FD_ISSET(fd, &aReadFdSet))
        {
            readyServices.push_back(mHostsConnection);
        }
    }

    for (std::vector<DNSServiceRef>::iterator it = readyServices.begin(); it != readyServices.end(); ++it)
    {
        DNSServiceErrorType error = DNSServiceProcessResult(*it);

        if (error != kDNSServiceErr_NoError)
        {
            otbrLog(OTBR_LOG_WARNING, "DNSServiceProcessResult failed: %s", DNSErrorToString(error));
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
    otbrLog(OTBR_LOG_INFO, "Got a reply for service %s.%s%s", aName, aType, aDomain);

    if (aError == kDNSServiceErr_NoError)
    {
        if (aFlags & kDNSServiceFlagsAdd)
        {
            otbrLog(OTBR_LOG_INFO, "MDNS added service %s", aName);
            RecordService(aName, aType, aServiceRef);
        }
        else
        {
            otbrLog(OTBR_LOG_INFO, "MDNS remove service %s", aName);
            DiscardService(aName, aType, aServiceRef);
        }
    }
    else
    {
        otbrLog(OTBR_LOG_ERR, "Failed to register service %s: %s", aName, DNSErrorToString(aError));
        DiscardService(aName, aType, aServiceRef);
    }
}

void PublisherMDnsSd::DiscardService(const char *aName, const char *aType, DNSServiceRef aServiceRef)
{
    for (Services::iterator it = mServices.begin(); it != mServices.end(); ++it)
    {
        if (!strncmp(it->mName, aName, sizeof(it->mName)) && !strncmp(it->mType, aType, sizeof(it->mType)))
        {
            assert(aServiceRef == nullptr || aServiceRef == it->mService);
            mServices.erase(it);
            DNSServiceRefDeallocate(aServiceRef);
            aServiceRef = nullptr;
            break;
        }
    }

    assert(aServiceRef == nullptr);
}

void PublisherMDnsSd::RecordService(const char *aName, const char *aType, DNSServiceRef aServiceRef)
{
    for (Services::iterator it = mServices.begin(); it != mServices.end(); ++it)
    {
        if (!strncmp(it->mName, aName, sizeof(it->mName)) && !strncmp(it->mType, aType, sizeof(it->mType)))
        {
            assert(aServiceRef == it->mService);
            ExitNow();
        }
    }

    {
        Service service;

        strcpy_safe(service.mName, sizeof(service.mName), aName);
        strcpy_safe(service.mType, sizeof(service.mType), aType);
        service.mService = aServiceRef;
        mServices.push_back(service);
    }

exit:
    return;
}

otbrError PublisherMDnsSd::PublishService(const char *   aHostName,
                                          uint16_t       aPort,
                                          const char *   aName,
                                          const char *   aType,
                                          const TxtList &aTxtList)
{
    otbrError     ret   = OTBR_ERROR_NONE;
    int           error = 0;
    uint8_t       txt[kMaxSizeOfTxtRecord];
    uint8_t *     cur        = txt;
    DNSServiceRef serviceRef = nullptr;
    char          fullHostName[kMaxSizeOfDomain];

    if (aHostName != nullptr)
    {
        auto host = std::find_if(mHosts.begin(), mHosts.end(),
                                 [&](const Host &aHost) { return strcmp(aHost.mName, aHostName) == 0; });

        // Make sure that the host has been published.
        VerifyOrExit(host != mHosts.end(), ret = OTBR_ERROR_INVALID_ARGS);
        SuccessOrExit(error = MakeFullName(fullHostName, sizeof(fullHostName), aHostName));
    }

    for (const auto &txtEntry : aTxtList)
    {
        const char *   name         = txtEntry.mName;
        const size_t   nameLength   = strlen(txtEntry.mName);
        const uint8_t *value        = txtEntry.mValue;
        const size_t   valueLength  = txtEntry.mValueLength;
        size_t         recordLength = nameLength + 1 + valueLength;

        assert(nameLength > 0 && valueLength > 0 && recordLength < kMaxTextRecordSize);

        if (cur + recordLength >= txt + sizeof(txt))
        {
            otbrLog(OTBR_LOG_WARNING, "Skip text record too much long: name=%s, entry-length=%zu", name, recordLength);
            continue;
        }

        assert(recordLength <= 255);
        cur[0] = static_cast<uint8_t>(recordLength);
        cur += 1;

        memcpy(cur, name, nameLength);
        cur += nameLength;

        cur[0] = '=';
        cur += 1;

        memcpy(cur, value, valueLength);
        cur += valueLength;
    }

    for (Services::iterator it = mServices.begin(); it != mServices.end(); ++it)
    {
        if (!strncmp(it->mName, aName, sizeof(it->mName)) && !strncmp(it->mType, aType, sizeof(it->mType)))
        {
            otbrLog(OTBR_LOG_INFO, "MDNS remove current service %s", aName);
            DNSServiceUpdateRecord(it->mService, nullptr, 0, static_cast<uint16_t>(cur - txt), txt, 0);
            ExitNow();
        }
    }

    SuccessOrExit(error = DNSServiceRegister(&serviceRef, 0, kDNSServiceInterfaceIndexAny, aName, aType, mDomain,
                                             (aHostName != nullptr) ? fullHostName : nullptr, htons(aPort),
                                             static_cast<uint16_t>(cur - txt), txt, HandleServiceRegisterResult, this));
    if (serviceRef != nullptr)
    {
        RecordService(aName, aType, serviceRef);
    }

exit:

    if (error != kDNSServiceErr_NoError)
    {
        ret = OTBR_ERROR_MDNS;
        otbrLog(OTBR_LOG_ERR, "Failed to publish service for mdnssd error: %s!", DNSErrorToString(error));
    }

    return ret;
}

otbrError PublisherMDnsSd::UnpublishService(const char *aName, const char *aType)
{
    DiscardService(aName, aType);
    return OTBR_ERROR_NONE;
}

otbrError PublisherMDnsSd::PublishHost(const char *aName, const uint8_t *aAddress, uint8_t aAddressLength)
{
    otbrError    ret   = OTBR_ERROR_NONE;
    int          error = 0;
    char         fullName[kMaxSizeOfDomain];
    DNSRecordRef hostRecord;
    Host         newHost;

    // Supports only IPv6 for now, may support IPv4 in the future.
    VerifyOrExit(aAddressLength == OTBR_IP6_ADDRESS_SIZE, error = OTBR_ERROR_INVALID_ARGS);

    SuccessOrExit(ret = MakeFullName(fullName, sizeof(fullName), aName));

    if (mHostsConnection == nullptr)
    {
        SuccessOrExit(error = DNSServiceCreateConnection(&mHostsConnection));
    }

    for (const auto &host : mHosts)
    {
        if (strcmp(aName, host.mName) == 0)
        {
            otbrLog(OTBR_LOG_INFO, "mDNS update host %s", aName);
            SuccessOrExit(error = DNSServiceUpdateRecord(mHostsConnection, host.mRecord, /* flags */ 0, aAddressLength,
                                                         aAddress, /* ttl */ 0));
            ExitNow();
        }
    }

    SuccessOrExit(error = DNSServiceRegisterRecord(mHostsConnection, &hostRecord, kDNSServiceFlagsUnique,
                                                   kDNSServiceInterfaceIndexAny, fullName, kDNSServiceType_AAAA,
                                                   kDNSServiceClass_IN, aAddressLength, aAddress, /* ttl */ 0,
                                                   HandleRegisterHostResult, this));
    strcpy(newHost.mName, aName);
    newHost.mRecord = hostRecord;
    mHosts.push_back(newHost);

exit:
    if (error != kDNSServiceErr_NoError)
    {
        ret = OTBR_ERROR_MDNS;
        otbrLog(OTBR_LOG_ERR, "Failed to publish/update host %s for mdnssd error: %s!", aName, DNSErrorToString(error));
    }
    return ret;
}

otbrError PublisherMDnsSd::UnpublishHost(const char *aName)
{
    otbrError ret   = OTBR_ERROR_NONE;
    int       error = 0;

    auto host =
        std::find_if(mHosts.begin(), mHosts.end(), [&](const Host &aHost) { return strcmp(aHost.mName, aName) == 0; });
    if (host == mHosts.end())
    {
        ExitNow();
    }

    SuccessOrExit(error = DNSServiceRemoveRecord(mHostsConnection, host->mRecord, /* flags */ 0));
    mHosts.erase(host);

exit:
    if (error != kDNSServiceErr_NoError)
    {
        ret = OTBR_ERROR_MDNS;
        otbrLog(OTBR_LOG_ERR, "Failed to un-publish host %s for mdnssd error: %s!", aName, DNSErrorToString(error));
    }
    return ret;
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

    auto host =
        std::find_if(mHosts.begin(), mHosts.end(), [&](const Host &aHost) { return aHost.mRecord == aHostRecord; });
    assert(host != mHosts.end());

    if (aErrorCode == kDNSServiceErr_NoError)
    {
        otbrLog(OTBR_LOG_INFO, "Successfully registered host %s", host->mName);
    }
    else
    {
        otbrLog(OTBR_LOG_WARNING, "Failed to register host %s for mdnssd error: %s", host->mName,
                DNSErrorToString(aErrorCode));
    }
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
