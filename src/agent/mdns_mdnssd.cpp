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

#include "mdns_mdnssd.hpp"

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

#include "dns_sd.h"

namespace ot {

namespace BorderRouter {

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
        return NULL;
    }
}

PublisherMDnsSd::PublisherMDnsSd(int          aProtocol,
                                 const char * aHost,
                                 const char * aDomain,
                                 StateHandler aHandler,
                                 void *       aContext)
    : mHost(aHost)
    , mDomain(aDomain)
    , mState(kStateIdle)
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
    mState = kStateReady;
    mStateHandler(mContext, kStateReady);
    return OTBR_ERROR_NONE;
}

bool PublisherMDnsSd::IsStarted(void) const
{
    return mState == kStateReady;
}

void PublisherMDnsSd::Stop(void)
{
    VerifyOrExit(mState == kStateReady);

    for (Services::iterator it = mServices.begin(); it != mServices.end(); ++it)
    {
        otbrLog(OTBR_LOG_INFO, "MDNS remove service %s", it->mName);
        DNSServiceRefDeallocate(it->mService);
    }

    mServices.clear();

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
            assert(aServiceRef == it->mService);
            mServices.erase(it);
            DNSServiceRefDeallocate(aServiceRef);
            aServiceRef = NULL;
            break;
        }
    }

    assert(aServiceRef == NULL);
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

otbrError PublisherMDnsSd::PublishService(uint16_t aPort, const char *aName, const char *aType, ...)
{
    otbrError     ret   = OTBR_ERROR_NONE;
    int           error = 0;
    va_list       args;
    uint8_t       txt[kMaxSizeOfTxtRecord];
    uint8_t *     cur        = txt;
    DNSServiceRef serviceRef = NULL;

    va_start(args, aType);

    for (const char *name = va_arg(args, const char *); name; name = va_arg(args, const char *))
    {
        const char * value        = va_arg(args, const char *);
        const size_t nameLength   = strlen(name);
        const size_t valueLength  = strlen(value);
        size_t       recordLength = nameLength + 1 + valueLength;

        assert(nameLength > 0 && valueLength > 0 && recordLength < kMaxTextRecordSize);

        if (cur + recordLength >= txt + sizeof(txt))
        {
            otbrLog(OTBR_LOG_WARNING, "Skip text record too much long: %s=%s", name, value);
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

    va_end(args);

    for (Services::iterator it = mServices.begin(); it != mServices.end(); ++it)
    {
        if (!strncmp(it->mName, aName, sizeof(it->mName)) && !strncmp(it->mType, aType, sizeof(it->mType)))
        {
            otbrLog(OTBR_LOG_INFO, "MDNS remove current service %s", aName);
            DNSServiceUpdateRecord(it->mService, NULL, 0, static_cast<uint16_t>(cur - txt), txt, 0);
            ExitNow();
        }
    }

    SuccessOrExit(error = DNSServiceRegister(&serviceRef, 0, kDNSServiceInterfaceIndexAny, aName, aType, mDomain, mHost,
                                             htons(aPort), static_cast<uint16_t>(cur - txt), txt,
                                             HandleServiceRegisterResult, this));
    if (serviceRef != NULL)
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

Publisher *Publisher::Create(int aFamily, const char *aHost, const char *aDomain, StateHandler aHandler, void *aContext)
{
    return new PublisherMDnsSd(aFamily, aHost, aDomain, aHandler, aContext);
}

void Publisher::Destroy(Publisher *aPublisher)
{
    delete static_cast<PublisherMDnsSd *>(aPublisher);
}

} // namespace Mdns

} // namespace BorderRouter

} // namespace ot
