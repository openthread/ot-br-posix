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
 *   This file implements mDNS publisher based on mDNSResponder.
 */

#define OTBR_LOG_TAG "MDNS"

#include "mdns/mdns_mdnssd.hpp"

#include <algorithm>

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/code_utils.hpp"
#include "common/dns_utils.hpp"
#include "common/logging.hpp"
#include "common/time.hpp"

namespace otbr {

namespace Mdns {

static const char kDomain[] = "local.";

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

    case kDNSServiceErr_NameConflict:
        error = OTBR_ERROR_DUPLICATED;
        break;

    case kDNSServiceErr_Unsupported:
        error = OTBR_ERROR_NOT_IMPLEMENTED;
        break;

    case kDNSServiceErr_ServiceNotRunning:
        error = OTBR_ERROR_INVALID_STATE;
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

PublisherMDnsSd::PublisherMDnsSd(StateCallback aCallback)
    : mHostsRef(nullptr)
    , mState(State::kIdle)
    , mStateCallback(std::move(aCallback))
{
}

PublisherMDnsSd::~PublisherMDnsSd(void)
{
    Stop(kNormalStop);
}

otbrError PublisherMDnsSd::Start(void)
{
    mState = State::kReady;
    mStateCallback(State::kReady);
    return OTBR_ERROR_NONE;
}

bool PublisherMDnsSd::IsStarted(void) const
{
    return mState == State::kReady;
}

void PublisherMDnsSd::Stop(StopMode aStopMode)
{
    VerifyOrExit(mState == State::kReady);

    // If we get a `kDNSServiceErr_ServiceNotRunning` and need to
    // restart the `Publisher`, we should immediately de-allocate
    // all `ServiceRef`. Otherwise, we first clear the `Registrations`
    // list so that `DnssdHostRegisteration` destructor gets the chance
    // to update registered records if needed.

    switch (aStopMode)
    {
    case kNormalStop:
        break;

    case kStopOnServiceNotRunningError:
        DeallocateHostsRef();
        break;
    }

    mServiceRegistrations.clear();
    mHostRegistrations.clear();
    mKeyRegistrations.clear();
    DeallocateHostsRef();

    mSubscribedServices.clear();
    mSubscribedHosts.clear();

    mState = State::kIdle;

exit:
    return;
}

DNSServiceErrorType PublisherMDnsSd::CreateSharedHostsRef(void)
{
    DNSServiceErrorType dnsError = kDNSServiceErr_NoError;

    VerifyOrExit(mHostsRef == nullptr);

    dnsError = DNSServiceCreateConnection(&mHostsRef);
    otbrLogDebug("Created new shared DNSServiceRef: %p", mHostsRef);

exit:
    return dnsError;
}

void PublisherMDnsSd::DeallocateHostsRef(void)
{
    VerifyOrExit(mHostsRef != nullptr);

    HandleServiceRefDeallocating(mHostsRef);
    DNSServiceRefDeallocate(mHostsRef);
    otbrLogDebug("Deallocated DNSServiceRef for hosts: %p", mHostsRef);
    mHostsRef = nullptr;

exit:
    return;
}

void PublisherMDnsSd::Update(MainloopContext &aMainloop)
{
    for (auto &kv : mServiceRegistrations)
    {
        auto &serviceReg = static_cast<DnssdServiceRegistration &>(*kv.second);

        serviceReg.Update(aMainloop);
    }

    if (mHostsRef != nullptr)
    {
        int fd = DNSServiceRefSockFD(mHostsRef);

        assert(fd != -1);

        aMainloop.AddFdToReadSet(fd);
    }

    for (const auto &service : mSubscribedServices)
    {
        service->UpdateAll(aMainloop);
    }

    for (const auto &host : mSubscribedHosts)
    {
        host->Update(aMainloop);
    }
}

void PublisherMDnsSd::Process(const MainloopContext &aMainloop)
{
    mServiceRefsToProcess.clear();

    for (auto &kv : mServiceRegistrations)
    {
        auto &serviceReg = static_cast<DnssdServiceRegistration &>(*kv.second);

        serviceReg.Process(aMainloop, mServiceRefsToProcess);
    }

    if (mHostsRef != nullptr)
    {
        int fd = DNSServiceRefSockFD(mHostsRef);

        if (FD_ISSET(fd, &aMainloop.mReadFdSet))
        {
            mServiceRefsToProcess.push_back(mHostsRef);
        }
    }

    for (const auto &service : mSubscribedServices)
    {
        service->ProcessAll(aMainloop, mServiceRefsToProcess);
    }

    for (const auto &host : mSubscribedHosts)
    {
        host->Process(aMainloop, mServiceRefsToProcess);
    }

    for (DNSServiceRef serviceRef : mServiceRefsToProcess)
    {
        DNSServiceErrorType error;

        // As we go through the list of `mServiceRefsToProcess` the call
        // to `DNSServiceProcessResult()` can itself invoke callbacks
        // into `PublisherMDnsSd` and OT, which in turn, may change the
        // state of `Publisher` and potentially trigger a previously
        // valid `ServiceRef` in the list to be deallocated. We use
        // `HandleServiceRefDeallocating()` which is called whenever a
        // `ServiceRef` is being deallocated and from this we update
        // the entry in `mServiceRefsToProcess` list to `nullptr` so to
        // avoid calling `DNSServiceProcessResult()` on an already
        // freed `ServiceRef`.

        if (serviceRef == nullptr)
        {
            continue;
        }

        error = DNSServiceProcessResult(serviceRef);

        if (error != kDNSServiceErr_NoError)
        {
            otbrLogLevel logLevel = (error == kDNSServiceErr_BadReference) ? OTBR_LOG_INFO : OTBR_LOG_WARNING;
            otbrLog(logLevel, OTBR_LOG_TAG, "DNSServiceProcessResult failed: %s (serviceRef = %p)",
                    DNSErrorToString(error), serviceRef);
        }
        if (error == kDNSServiceErr_ServiceNotRunning)
        {
            otbrLogWarning("Need to reconnect to mdnsd");
            Stop(kStopOnServiceNotRunningError);
            Start();
            ExitNow();
        }
    }
exit:
    return;
}

void PublisherMDnsSd::HandleServiceRefDeallocating(const DNSServiceRef &aServiceRef)
{
    for (DNSServiceRef &entry : mServiceRefsToProcess)
    {
        if (entry == aServiceRef)
        {
            entry = nullptr;
        }
    }
}

void PublisherMDnsSd::DnssdServiceRegistration::Update(MainloopContext &aMainloop) const
{
    int fd;

    VerifyOrExit(mServiceRef != nullptr);

    fd = DNSServiceRefSockFD(mServiceRef);
    VerifyOrExit(fd != -1);

    aMainloop.AddFdToReadSet(fd);

exit:
    return;
}

void PublisherMDnsSd::DnssdServiceRegistration::Process(const MainloopContext      &aMainloop,
                                                        std::vector<DNSServiceRef> &aReadyServices) const
{
    int fd;

    VerifyOrExit(mServiceRef != nullptr);

    fd = DNSServiceRefSockFD(mServiceRef);
    VerifyOrExit(fd != -1);

    VerifyOrExit(FD_ISSET(fd, &aMainloop.mReadFdSet));
    aReadyServices.push_back(mServiceRef);

exit:
    return;
}

otbrError PublisherMDnsSd::DnssdServiceRegistration::Register(void)
{
    std::string           fullHostName;
    std::string           regType            = MakeRegType(mType, mSubTypeList);
    const char           *hostNameCString    = nullptr;
    const char           *serviceNameCString = nullptr;
    DnssdKeyRegistration *keyReg;
    DNSServiceErrorType   dnsError;

    if (!mHostName.empty())
    {
        fullHostName    = MakeFullHostName(mHostName);
        hostNameCString = fullHostName.c_str();
    }

    if (!mName.empty())
    {
        serviceNameCString = mName.c_str();
    }

    keyReg = static_cast<DnssdKeyRegistration *>(GetPublisher().FindKeyRegistration(mName, mType));

    if (keyReg != nullptr)
    {
        keyReg->Unregister();
    }

    otbrLogInfo("Registering service %s.%s", mName.c_str(), regType.c_str());

    dnsError = DNSServiceRegister(&mServiceRef, kDNSServiceFlagsNoAutoRename, kDNSServiceInterfaceIndexAny,
                                  serviceNameCString, regType.c_str(),
                                  /* domain */ nullptr, hostNameCString, htons(mPort), mTxtData.size(), mTxtData.data(),
                                  HandleRegisterResult, this);

    if (dnsError != kDNSServiceErr_NoError)
    {
        HandleRegisterResult(/* aFlags */ 0, dnsError);
    }

    if (keyReg != nullptr)
    {
        keyReg->Register();
    }

    return GetPublisher().DnsErrorToOtbrError(dnsError);
}

void PublisherMDnsSd::DnssdServiceRegistration::Unregister(void)
{
    DnssdKeyRegistration *keyReg = mRelatedKeyReg;

    VerifyOrExit(mServiceRef != nullptr);

    // If we have a related key registration associated with this
    // service registration, we first unregister it and after we
    // deallocated the `mServiceRef` try to register it again
    // (which will add it as an individual record not tied to a
    // service registration). Note that the `keyReg->Unregister()`
    // will clear the `mRelatedKeyReg` field, so we need to keep
    // a local copy to it in `keyReg`.

    if (keyReg != nullptr)
    {
        keyReg->Unregister();
    }

    GetPublisher().HandleServiceRefDeallocating(mServiceRef);
    DNSServiceRefDeallocate(mServiceRef);
    mServiceRef = nullptr;

    if (keyReg != nullptr)
    {
        keyReg->Register();
    }

exit:
    return;
}

void PublisherMDnsSd::DnssdServiceRegistration::HandleRegisterResult(DNSServiceRef       aServiceRef,
                                                                     DNSServiceFlags     aFlags,
                                                                     DNSServiceErrorType aError,
                                                                     const char         *aName,
                                                                     const char         *aType,
                                                                     const char         *aDomain,
                                                                     void               *aContext)
{
    OTBR_UNUSED_VARIABLE(aServiceRef);
    OTBR_UNUSED_VARIABLE(aName);
    OTBR_UNUSED_VARIABLE(aType);
    OTBR_UNUSED_VARIABLE(aDomain);

    static_cast<DnssdServiceRegistration *>(aContext)->HandleRegisterResult(aFlags, aError);
}

void PublisherMDnsSd::DnssdServiceRegistration::HandleRegisterResult(DNSServiceFlags aFlags, DNSServiceErrorType aError)
{
    if (mRelatedKeyReg != nullptr)
    {
        mRelatedKeyReg->HandleRegisterResult(aError);
    }

    if ((aError == kDNSServiceErr_NoError) && (aFlags & kDNSServiceFlagsAdd))
    {
        otbrLogInfo("Successfully registered service %s.%s", mName.c_str(), mType.c_str());
        Complete(OTBR_ERROR_NONE);
    }
    else
    {
        otbrLogErr("Failed to register service %s.%s: %s", mName.c_str(), mType.c_str(), DNSErrorToString(aError));
        GetPublisher().RemoveServiceRegistration(mName, mType, DNSErrorToOtbrError(aError));
    }
}

otbrError PublisherMDnsSd::DnssdHostRegistration::Register(void)
{
    DNSServiceErrorType dnsError = kDNSServiceErr_NoError;

    otbrLogInfo("Registering new host %s", mName.c_str());

    for (const Ip6Address &address : mAddresses)
    {
        DNSRecordRef recordRef = nullptr;

        dnsError = GetPublisher().CreateSharedHostsRef();
        VerifyOrExit(dnsError == kDNSServiceErr_NoError);

        dnsError = DNSServiceRegisterRecord(GetPublisher().mHostsRef, &recordRef, kDNSServiceFlagsShared,
                                            kDNSServiceInterfaceIndexAny, MakeFullHostName(mName).c_str(),
                                            kDNSServiceType_AAAA, kDNSServiceClass_IN, sizeof(address.m8), address.m8,
                                            /* ttl */ 0, HandleRegisterResult, this);
        VerifyOrExit(dnsError == kDNSServiceErr_NoError);

        mAddrRecordRefs.push_back(recordRef);
        mAddrRegistered.push_back(false);
    }

exit:
    if ((dnsError != kDNSServiceErr_NoError) || mAddresses.empty())
    {
        HandleRegisterResult(/* aRecordRef */ nullptr, dnsError);
    }

    return GetPublisher().DnsErrorToOtbrError(dnsError);
}

void PublisherMDnsSd::DnssdHostRegistration::Unregister(void)
{
    DNSServiceErrorType dnsError;

    VerifyOrExit(GetPublisher().mHostsRef != nullptr);

    for (size_t index = 0; index < mAddrRecordRefs.size(); index++)
    {
        const Ip6Address &address = mAddresses[index];

        if (mAddrRegistered[index])
        {
            // The Bonjour mDNSResponder somehow doesn't send goodbye message for the AAAA record when it is
            // removed by `DNSServiceRemoveRecord`. Per RFC 6762, a goodbye message of a record sets its TTL
            // to zero but the receiver should record the TTL of 1 and flushes the cache 1 second later. Here
            // we remove the AAAA record after updating its TTL to 1 second. This has the same effect as
            // sending a goodbye message.
            // TODO: resolve the goodbye issue with Bonjour mDNSResponder.
            dnsError = DNSServiceUpdateRecord(GetPublisher().mHostsRef, mAddrRecordRefs[index], kDNSServiceFlagsUnique,
                                              sizeof(address.m8), address.m8, /* ttl */ 1);
            otbrLogResult(DNSErrorToOtbrError(dnsError), "Send goodbye message for host %s address %s: %s",
                          MakeFullHostName(mName).c_str(), address.ToString().c_str(), DNSErrorToString(dnsError));
        }

        dnsError = DNSServiceRemoveRecord(GetPublisher().mHostsRef, mAddrRecordRefs[index], /* flags */ 0);

        otbrLogResult(DNSErrorToOtbrError(dnsError), "Remove record for host %s address %s: %s",
                      MakeFullHostName(mName).c_str(), address.ToString().c_str(), DNSErrorToString(dnsError));
    }

exit:
    mAddrRegistered.clear();
    mAddrRecordRefs.clear();
}

void PublisherMDnsSd::DnssdHostRegistration::HandleRegisterResult(DNSServiceRef       aServiceRef,
                                                                  DNSRecordRef        aRecordRef,
                                                                  DNSServiceFlags     aFlags,
                                                                  DNSServiceErrorType aError,
                                                                  void               *aContext)
{
    OTBR_UNUSED_VARIABLE(aServiceRef);
    OTBR_UNUSED_VARIABLE(aFlags);

    static_cast<DnssdHostRegistration *>(aContext)->HandleRegisterResult(aRecordRef, aError);
}

void PublisherMDnsSd::DnssdHostRegistration::HandleRegisterResult(DNSRecordRef aRecordRef, DNSServiceErrorType aError)
{
    if (aError != kDNSServiceErr_NoError)
    {
        otbrLogErr("Failed to register host %s: %s", mName.c_str(), DNSErrorToString(aError));
        GetPublisher().RemoveHostRegistration(mName, DNSErrorToOtbrError(aError));
    }
    else
    {
        bool shouldComplete = !IsCompleted();

        for (size_t index = 0; index < mAddrRecordRefs.size(); index++)
        {
            if ((mAddrRecordRefs[index] == aRecordRef) && !mAddrRegistered[index])
            {
                mAddrRegistered[index] = true;
                otbrLogInfo("Successfully registered host %s address %s", mName.c_str(),
                            mAddresses[index].ToString().c_str());
            }

            if (!mAddrRegistered[index])
            {
                shouldComplete = false;
            }
        }

        if (shouldComplete)
        {
            otbrLogInfo("Successfully registered all host %s addresses", mName.c_str());
            Complete(OTBR_ERROR_NONE);
        }
    }
}

otbrError PublisherMDnsSd::DnssdKeyRegistration::Register(void)
{
    DNSServiceErrorType       dnsError = kDNSServiceErr_NoError;
    DnssdServiceRegistration *serviceReg;

    otbrLogInfo("Registering new key %s", mName.c_str());

    serviceReg = static_cast<DnssdServiceRegistration *>(GetPublisher().FindServiceRegistration(mName));

    if ((serviceReg != nullptr) && (serviceReg->mServiceRef != nullptr))
    {
        otbrLogInfo("Key %s is being registered as a record of an existing service registration", mName.c_str());

        dnsError = DNSServiceAddRecord(serviceReg->mServiceRef, &mRecordRef, kDNSServiceFlagsUnique,
                                       kDNSServiceType_KEY, mKeyData.size(), mKeyData.data(), /* ttl */ 0);

        VerifyOrExit(dnsError == kDNSServiceErr_NoError);

        mRelatedServiceReg         = serviceReg;
        serviceReg->mRelatedKeyReg = this;

        if (mRelatedServiceReg->IsCompleted())
        {
            HandleRegisterResult(kDNSServiceErr_NoError);
        }

        // If related service registration is not yet finished,
        // we wait for service registration completion to signal
        // key record registration as well.
    }
    else
    {
        otbrLogInfo("Key %s is being registered individually", mName.c_str());

        dnsError = GetPublisher().CreateSharedHostsRef();
        VerifyOrExit(dnsError == kDNSServiceErr_NoError);

        dnsError = DNSServiceRegisterRecord(GetPublisher().mHostsRef, &mRecordRef, kDNSServiceFlagsUnique,
                                            kDNSServiceInterfaceIndexAny, MakeFullKeyName(mName).c_str(),
                                            kDNSServiceType_KEY, kDNSServiceClass_IN, mKeyData.size(), mKeyData.data(),
                                            /* ttl */ 0, HandleRegisterResult, this);
        VerifyOrExit(dnsError == kDNSServiceErr_NoError);
    }

exit:
    if (dnsError != kDNSServiceErr_NoError)
    {
        HandleRegisterResult(dnsError);
    }

    return GetPublisher().DnsErrorToOtbrError(dnsError);
}

void PublisherMDnsSd::DnssdKeyRegistration::Unregister(void)
{
    DNSServiceErrorType dnsError;
    DNSServiceRef       serviceRef;

    VerifyOrExit(mRecordRef != nullptr);

    if (mRelatedServiceReg != nullptr)
    {
        serviceRef = mRelatedServiceReg->mServiceRef;

        mRelatedServiceReg->mRelatedKeyReg = nullptr;
        mRelatedServiceReg                 = nullptr;

        otbrLogInfo("Unregistering key %s (was registered as a record of a service)", mName.c_str());
    }
    else
    {
        serviceRef = GetPublisher().mHostsRef;

        otbrLogInfo("Unregistering key %s (was registered individually)", mName.c_str());
    }

    VerifyOrExit(serviceRef != nullptr);

    dnsError = DNSServiceRemoveRecord(serviceRef, mRecordRef, /* flags */ 0);

    otbrLogInfo("Unregistered key %s: error:%s", mName.c_str(), DNSErrorToString(dnsError));

exit:
    return;
}

void PublisherMDnsSd::DnssdKeyRegistration::HandleRegisterResult(DNSServiceRef       aServiceRef,
                                                                 DNSRecordRef        aRecordRef,
                                                                 DNSServiceFlags     aFlags,
                                                                 DNSServiceErrorType aError,
                                                                 void               *aContext)
{
    OTBR_UNUSED_VARIABLE(aServiceRef);
    OTBR_UNUSED_VARIABLE(aRecordRef);
    OTBR_UNUSED_VARIABLE(aFlags);

    static_cast<DnssdKeyRegistration *>(aContext)->HandleRegisterResult(aError);
}

void PublisherMDnsSd::DnssdKeyRegistration::HandleRegisterResult(DNSServiceErrorType aError)
{
    if (aError != kDNSServiceErr_NoError)
    {
        otbrLogErr("Failed to register key %s: %s", mName.c_str(), DNSErrorToString(aError));
        GetPublisher().RemoveKeyRegistration(mName, DNSErrorToOtbrError(aError));
    }
    else
    {
        otbrLogInfo("Successfully registered key %s", mName.c_str());
        Complete(OTBR_ERROR_NONE);
    }
}

otbrError PublisherMDnsSd::PublishServiceImpl(const std::string &aHostName,
                                              const std::string &aName,
                                              const std::string &aType,
                                              const SubTypeList &aSubTypeList,
                                              uint16_t           aPort,
                                              const TxtData     &aTxtData,
                                              ResultCallback   &&aCallback)
{
    otbrError                 error             = OTBR_ERROR_NONE;
    SubTypeList               sortedSubTypeList = SortSubTypeList(aSubTypeList);
    std::string               regType           = MakeRegType(aType, sortedSubTypeList);
    DnssdServiceRegistration *serviceReg;

    if (mState != State::kReady)
    {
        error = OTBR_ERROR_INVALID_STATE;
        std::move(aCallback)(error);
        ExitNow();
    }

    aCallback = HandleDuplicateServiceRegistration(aHostName, aName, aType, sortedSubTypeList, aPort, aTxtData,
                                                   std::move(aCallback));
    VerifyOrExit(!aCallback.IsNull());

    serviceReg = new DnssdServiceRegistration(aHostName, aName, aType, sortedSubTypeList, aPort, aTxtData,
                                              std::move(aCallback), this);
    AddServiceRegistration(std::unique_ptr<DnssdServiceRegistration>(serviceReg));

    error = serviceReg->Register();

exit:
    return error;
}

void PublisherMDnsSd::UnpublishService(const std::string &aName, const std::string &aType, ResultCallback &&aCallback)
{
    otbrError error = OTBR_ERROR_NONE;

    VerifyOrExit(mState == Publisher::State::kReady, error = OTBR_ERROR_INVALID_STATE);
    RemoveServiceRegistration(aName, aType, OTBR_ERROR_ABORTED);

exit:
    std::move(aCallback)(error);
}

otbrError PublisherMDnsSd::PublishHostImpl(const std::string &aName,
                                           const AddressList &aAddresses,
                                           ResultCallback   &&aCallback)
{
    otbrError              error = OTBR_ERROR_NONE;
    DnssdHostRegistration *hostReg;

    if (mState != State::kReady)
    {
        error = OTBR_ERROR_INVALID_STATE;
        std::move(aCallback)(error);
        ExitNow();
    }

    aCallback = HandleDuplicateHostRegistration(aName, aAddresses, std::move(aCallback));
    VerifyOrExit(!aCallback.IsNull());

    hostReg = new DnssdHostRegistration(aName, aAddresses, std::move(aCallback), this);
    AddHostRegistration(std::unique_ptr<DnssdHostRegistration>(hostReg));

    error = hostReg->Register();

exit:
    return error;
}

void PublisherMDnsSd::UnpublishHost(const std::string &aName, ResultCallback &&aCallback)
{
    otbrError error = OTBR_ERROR_NONE;

    VerifyOrExit(mState == Publisher::State::kReady, error = OTBR_ERROR_INVALID_STATE);
    RemoveHostRegistration(aName, OTBR_ERROR_ABORTED);

exit:
    // We may failed to unregister the host from underlying mDNS publishers, but
    // it usually means that the mDNS publisher is already not functioning. So it's
    // okay to return success directly since the service is not advertised anyway.
    std::move(aCallback)(error);
}

otbrError PublisherMDnsSd::PublishKeyImpl(const std::string &aName, const KeyData &aKeyData, ResultCallback &&aCallback)
{
    otbrError             error = OTBR_ERROR_NONE;
    DnssdKeyRegistration *keyReg;

    if (mState != State::kReady)
    {
        error = OTBR_ERROR_INVALID_STATE;
        std::move(aCallback)(error);
        ExitNow();
    }

    aCallback = HandleDuplicateKeyRegistration(aName, aKeyData, std::move(aCallback));
    VerifyOrExit(!aCallback.IsNull());

    keyReg = new DnssdKeyRegistration(aName, aKeyData, std::move(aCallback), this);
    AddKeyRegistration(std::unique_ptr<DnssdKeyRegistration>(keyReg));

    error = keyReg->Register();

exit:
    return error;
}

void PublisherMDnsSd::UnpublishKey(const std::string &aName, ResultCallback &&aCallback)
{
    otbrError error = OTBR_ERROR_NONE;

    VerifyOrExit(mState == Publisher::State::kReady, error = OTBR_ERROR_INVALID_STATE);
    RemoveKeyRegistration(aName, OTBR_ERROR_ABORTED);

exit:
    std::move(aCallback)(error);
}

// See `regtype` parameter of the DNSServiceRegister() function for more information.
std::string PublisherMDnsSd::MakeRegType(const std::string &aType, SubTypeList aSubTypeList)
{
    std::string regType = aType;

    std::sort(aSubTypeList.begin(), aSubTypeList.end());

    for (const auto &subType : aSubTypeList)
    {
        regType += "," + subType;
    }

    return regType;
}

void PublisherMDnsSd::SubscribeService(const std::string &aType, const std::string &aInstanceName)
{
    VerifyOrExit(mState == Publisher::State::kReady);
    mSubscribedServices.push_back(MakeUnique<ServiceSubscription>(*this, aType, aInstanceName));

    otbrLogInfo("Subscribe service %s.%s (total %zu)", aInstanceName.c_str(), aType.c_str(),
                mSubscribedServices.size());

    if (aInstanceName.empty())
    {
        mSubscribedServices.back()->Browse();
    }
    else
    {
        mSubscribedServices.back()->Resolve(kDNSServiceInterfaceIndexAny, aInstanceName, aType, kDomain);
    }

exit:
    return;
}

void PublisherMDnsSd::UnsubscribeService(const std::string &aType, const std::string &aInstanceName)
{
    ServiceSubscriptionList::iterator it;

    VerifyOrExit(mState == Publisher::State::kReady);
    it = std::find_if(mSubscribedServices.begin(), mSubscribedServices.end(),
                      [&aType, &aInstanceName](const std::unique_ptr<ServiceSubscription> &aService) {
                          return aService->mType == aType && aService->mInstanceName == aInstanceName;
                      });
    VerifyOrExit(it != mSubscribedServices.end());

    mSubscribedServices.erase(it);

    otbrLogInfo("Unsubscribe service %s.%s (left %zu)", aInstanceName.c_str(), aType.c_str(),
                mSubscribedServices.size());

exit:
    return;
}

void PublisherMDnsSd::OnServiceResolveFailedImpl(const std::string &aType,
                                                 const std::string &aInstanceName,
                                                 int32_t            aErrorCode)
{
    otbrLogWarning("Resolve service %s.%s failed: code=%" PRId32, aInstanceName.c_str(), aType.c_str(), aErrorCode);
}

void PublisherMDnsSd::OnHostResolveFailedImpl(const std::string &aHostName, int32_t aErrorCode)
{
    otbrLogWarning("Resolve host %s failed: code=%" PRId32, aHostName.c_str(), aErrorCode);
}

otbrError PublisherMDnsSd::DnsErrorToOtbrError(int32_t aErrorCode)
{
    return otbr::Mdns::DNSErrorToOtbrError(aErrorCode);
}

void PublisherMDnsSd::SubscribeHost(const std::string &aHostName)
{
    VerifyOrExit(mState == State::kReady);
    mSubscribedHosts.push_back(MakeUnique<HostSubscription>(*this, aHostName));

    otbrLogInfo("Subscribe host %s (total %zu)", aHostName.c_str(), mSubscribedHosts.size());

    mSubscribedHosts.back()->Resolve();

exit:
    return;
}

void PublisherMDnsSd::UnsubscribeHost(const std::string &aHostName)
{
    HostSubscriptionList ::iterator it;

    VerifyOrExit(mState == Publisher::State::kReady);
    it = std::find_if(
        mSubscribedHosts.begin(), mSubscribedHosts.end(),
        [&aHostName](const std::unique_ptr<HostSubscription> &aHost) { return aHost->mHostName == aHostName; });

    VerifyOrExit(it != mSubscribedHosts.end());

    mSubscribedHosts.erase(it);

    otbrLogInfo("Unsubscribe host %s (remaining %d)", aHostName.c_str(), mSubscribedHosts.size());

exit:
    return;
}

Publisher *Publisher::Create(StateCallback aCallback)
{
    return new PublisherMDnsSd(aCallback);
}

void Publisher::Destroy(Publisher *aPublisher)
{
    delete static_cast<PublisherMDnsSd *>(aPublisher);
}

void PublisherMDnsSd::ServiceRef::Release(void)
{
    DeallocateServiceRef();
}

void PublisherMDnsSd::ServiceRef::DeallocateServiceRef(void)
{
    if (mServiceRef != nullptr)
    {
        mPublisher.HandleServiceRefDeallocating(mServiceRef);
        DNSServiceRefDeallocate(mServiceRef);
        mServiceRef = nullptr;
    }
}

void PublisherMDnsSd::ServiceRef::Update(MainloopContext &aMainloop) const
{
    int fd;

    VerifyOrExit(mServiceRef != nullptr);

    fd = DNSServiceRefSockFD(mServiceRef);
    assert(fd != -1);
    aMainloop.AddFdToReadSet(fd);
exit:
    return;
}

void PublisherMDnsSd::ServiceRef::Process(const MainloopContext      &aMainloop,
                                          std::vector<DNSServiceRef> &aReadyServices) const
{
    int fd;

    VerifyOrExit(mServiceRef != nullptr);

    fd = DNSServiceRefSockFD(mServiceRef);
    assert(fd != -1);
    if (FD_ISSET(fd, &aMainloop.mReadFdSet))
    {
        aReadyServices.push_back(mServiceRef);
    }
exit:
    return;
}

void PublisherMDnsSd::ServiceSubscription::Browse(void)
{
    assert(mServiceRef == nullptr);

    otbrLogInfo("DNSServiceBrowse %s", mType.c_str());
    DNSServiceBrowse(&mServiceRef, /* flags */ 0, kDNSServiceInterfaceIndexAny, mType.c_str(),
                     /* domain */ nullptr, HandleBrowseResult, this);
}

void PublisherMDnsSd::ServiceSubscription::HandleBrowseResult(DNSServiceRef       aServiceRef,
                                                              DNSServiceFlags     aFlags,
                                                              uint32_t            aInterfaceIndex,
                                                              DNSServiceErrorType aErrorCode,
                                                              const char         *aInstanceName,
                                                              const char         *aType,
                                                              const char         *aDomain,
                                                              void               *aContext)
{
    static_cast<ServiceSubscription *>(aContext)->HandleBrowseResult(aServiceRef, aFlags, aInterfaceIndex, aErrorCode,
                                                                     aInstanceName, aType, aDomain);
}

void PublisherMDnsSd::ServiceSubscription::HandleBrowseResult(DNSServiceRef       aServiceRef,
                                                              DNSServiceFlags     aFlags,
                                                              uint32_t            aInterfaceIndex,
                                                              DNSServiceErrorType aErrorCode,
                                                              const char         *aInstanceName,
                                                              const char         *aType,
                                                              const char         *aDomain)
{
    OTBR_UNUSED_VARIABLE(aServiceRef);
    OTBR_UNUSED_VARIABLE(aDomain);

    otbrLogInfo("DNSServiceBrowse reply: %s %s.%s inf %" PRIu32 ", flags=%" PRIu32 ", error=%" PRId32,
                aFlags & kDNSServiceFlagsAdd ? "add" : "remove", aInstanceName, aType, aInterfaceIndex, aFlags,
                aErrorCode);

    VerifyOrExit(aErrorCode == kDNSServiceErr_NoError);

    if (aFlags & kDNSServiceFlagsAdd)
    {
        Resolve(aInterfaceIndex, aInstanceName, aType, aDomain);
    }
    else
    {
        mPublisher.OnServiceRemoved(aInterfaceIndex, mType, aInstanceName);
    }

exit:
    if (aErrorCode != kDNSServiceErr_NoError)
    {
        mPublisher.OnServiceResolveFailed(mType, mInstanceName, aErrorCode);
        Release();
    }
}

void PublisherMDnsSd::ServiceSubscription::Resolve(uint32_t           aInterfaceIndex,
                                                   const std::string &aInstanceName,
                                                   const std::string &aType,
                                                   const std::string &aDomain)
{
    mResolvingInstances.push_back(
        MakeUnique<ServiceInstanceResolution>(*this, aInstanceName, aType, aDomain, aInterfaceIndex));
    mResolvingInstances.back()->Resolve();
}

void PublisherMDnsSd::ServiceSubscription::UpdateAll(MainloopContext &aMainloop) const
{
    Update(aMainloop);

    for (const auto &instance : mResolvingInstances)
    {
        instance->Update(aMainloop);
    }
}

void PublisherMDnsSd::ServiceSubscription::ProcessAll(const MainloopContext      &aMainloop,
                                                      std::vector<DNSServiceRef> &aReadyServices) const
{
    Process(aMainloop, aReadyServices);

    for (const auto &instance : mResolvingInstances)
    {
        instance->Process(aMainloop, aReadyServices);
    }
}

void PublisherMDnsSd::ServiceInstanceResolution::Resolve(void)
{
    assert(mServiceRef == nullptr);

    mSubscription->mPublisher.mServiceInstanceResolutionBeginTime[std::make_pair(mInstanceName, mType)] = Clock::now();

    otbrLogInfo("DNSServiceResolve %s %s inf %u", mInstanceName.c_str(), mType.c_str(), mNetifIndex);
    DNSServiceResolve(&mServiceRef, /* flags */ kDNSServiceFlagsTimeout, mNetifIndex, mInstanceName.c_str(),
                      mType.c_str(), mDomain.c_str(), HandleResolveResult, this);
}

void PublisherMDnsSd::ServiceInstanceResolution::HandleResolveResult(DNSServiceRef        aServiceRef,
                                                                     DNSServiceFlags      aFlags,
                                                                     uint32_t             aInterfaceIndex,
                                                                     DNSServiceErrorType  aErrorCode,
                                                                     const char          *aFullName,
                                                                     const char          *aHostTarget,
                                                                     uint16_t             aPort,
                                                                     uint16_t             aTxtLen,
                                                                     const unsigned char *aTxtRecord,
                                                                     void                *aContext)
{
    static_cast<ServiceInstanceResolution *>(aContext)->HandleResolveResult(
        aServiceRef, aFlags, aInterfaceIndex, aErrorCode, aFullName, aHostTarget, aPort, aTxtLen, aTxtRecord);
}

void PublisherMDnsSd::ServiceInstanceResolution::HandleResolveResult(DNSServiceRef        aServiceRef,
                                                                     DNSServiceFlags      aFlags,
                                                                     uint32_t             aInterfaceIndex,
                                                                     DNSServiceErrorType  aErrorCode,
                                                                     const char          *aFullName,
                                                                     const char          *aHostTarget,
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

    mInstanceInfo.mNetifIndex = aInterfaceIndex;
    mInstanceInfo.mName       = instanceName;
    mInstanceInfo.mHostName   = aHostTarget;
    mInstanceInfo.mPort       = ntohs(aPort);
    mInstanceInfo.mTxtData.assign(aTxtRecord, aTxtRecord + aTxtLen);
    // priority and weight are not given in the reply
    mInstanceInfo.mPriority = 0;
    mInstanceInfo.mWeight   = 0;

    DeallocateServiceRef();
    error = GetAddrInfo(aInterfaceIndex);

exit:
    if (error != OTBR_ERROR_NONE)
    {
        otbrLogWarning("Failed to resolve service instance %s", aFullName);
    }

    if (aErrorCode != kDNSServiceErr_NoError || error != OTBR_ERROR_NONE)
    {
        mSubscription->mPublisher.OnServiceResolveFailed(mSubscription->mType, mInstanceName, aErrorCode);
        FinishResolution();
    }
}

otbrError PublisherMDnsSd::ServiceInstanceResolution::GetAddrInfo(uint32_t aInterfaceIndex)
{
    DNSServiceErrorType dnsError;

    assert(mServiceRef == nullptr);

    otbrLogInfo("DNSServiceGetAddrInfo %s inf %d", mInstanceInfo.mHostName.c_str(), aInterfaceIndex);

    dnsError = DNSServiceGetAddrInfo(&mServiceRef, /* flags */ 0, aInterfaceIndex,
                                     kDNSServiceProtocol_IPv6 | kDNSServiceProtocol_IPv4,
                                     mInstanceInfo.mHostName.c_str(), HandleGetAddrInfoResult, this);

    if (dnsError != kDNSServiceErr_NoError)
    {
        otbrLogWarning("DNSServiceGetAddrInfo failed: %s", DNSErrorToString(dnsError));
    }

    return dnsError == kDNSServiceErr_NoError ? OTBR_ERROR_NONE : OTBR_ERROR_MDNS;
}

void PublisherMDnsSd::ServiceInstanceResolution::HandleGetAddrInfoResult(DNSServiceRef          aServiceRef,
                                                                         DNSServiceFlags        aFlags,
                                                                         uint32_t               aInterfaceIndex,
                                                                         DNSServiceErrorType    aErrorCode,
                                                                         const char            *aHostName,
                                                                         const struct sockaddr *aAddress,
                                                                         uint32_t               aTtl,
                                                                         void                  *aContext)
{
    static_cast<ServiceInstanceResolution *>(aContext)->HandleGetAddrInfoResult(aServiceRef, aFlags, aInterfaceIndex,
                                                                                aErrorCode, aHostName, aAddress, aTtl);
}

void PublisherMDnsSd::ServiceInstanceResolution::HandleGetAddrInfoResult(DNSServiceRef          aServiceRef,
                                                                         DNSServiceFlags        aFlags,
                                                                         uint32_t               aInterfaceIndex,
                                                                         DNSServiceErrorType    aErrorCode,
                                                                         const char            *aHostName,
                                                                         const struct sockaddr *aAddress,
                                                                         uint32_t               aTtl)
{
    OTBR_UNUSED_VARIABLE(aServiceRef);
    OTBR_UNUSED_VARIABLE(aInterfaceIndex);

    Ip6Address address;
    bool       isAdd = (aFlags & kDNSServiceFlagsAdd) != 0;

    otbrLog(aErrorCode == kDNSServiceErr_NoError ? OTBR_LOG_INFO : OTBR_LOG_WARNING, OTBR_LOG_TAG,
            "DNSServiceGetAddrInfo reply: flags=%" PRIu32 ", host=%s, sa_family=%u, error=%" PRId32, aFlags, aHostName,
            static_cast<unsigned int>(aAddress->sa_family), aErrorCode);

    VerifyOrExit(aErrorCode == kDNSServiceErr_NoError);
    VerifyOrExit(aAddress->sa_family == AF_INET6);

    address.CopyFrom(*reinterpret_cast<const struct sockaddr_in6 *>(aAddress));
    VerifyOrExit(!address.IsUnspecified() && !address.IsLinkLocal() && !address.IsMulticast() && !address.IsLoopback(),
                 otbrLogDebug("DNSServiceGetAddrInfo ignores address %s", address.ToString().c_str()));

    otbrLogInfo("DNSServiceGetAddrInfo reply: %s address=%s, ttl=%" PRIu32, isAdd ? "add" : "remove",
                address.ToString().c_str(), aTtl);

    if (isAdd)
    {
        mInstanceInfo.AddAddress(address);
    }
    else
    {
        mInstanceInfo.RemoveAddress(address);
    }
    mInstanceInfo.mTtl = aTtl;

exit:
    if (!mInstanceInfo.mAddresses.empty() || aErrorCode != kDNSServiceErr_NoError)
    {
        FinishResolution();
    }
}

void PublisherMDnsSd::ServiceInstanceResolution::FinishResolution(void)
{
    ServiceSubscription   *subscription = mSubscription;
    std::string            serviceName  = mSubscription->mType;
    DiscoveredInstanceInfo instanceInfo = mInstanceInfo;

    // NOTE: The `ServiceSubscription` object may be freed in `OnServiceResolved`.
    subscription->mPublisher.OnServiceResolved(serviceName, instanceInfo);
}

void PublisherMDnsSd::HostSubscription::Resolve(void)
{
    std::string fullHostName = MakeFullHostName(mHostName);

    assert(mServiceRef == nullptr);

    mPublisher.mHostResolutionBeginTime[mHostName] = Clock::now();

    otbrLogInfo("DNSServiceGetAddrInfo %s inf %d", fullHostName.c_str(), kDNSServiceInterfaceIndexAny);

    DNSServiceGetAddrInfo(&mServiceRef, /* flags */ 0, kDNSServiceInterfaceIndexAny,
                          kDNSServiceProtocol_IPv6 | kDNSServiceProtocol_IPv4, fullHostName.c_str(),
                          HandleResolveResult, this);
}

void PublisherMDnsSd::HostSubscription::HandleResolveResult(DNSServiceRef          aServiceRef,
                                                            DNSServiceFlags        aFlags,
                                                            uint32_t               aInterfaceIndex,
                                                            DNSServiceErrorType    aErrorCode,
                                                            const char            *aHostName,
                                                            const struct sockaddr *aAddress,
                                                            uint32_t               aTtl,
                                                            void                  *aContext)
{
    static_cast<HostSubscription *>(aContext)->HandleResolveResult(aServiceRef, aFlags, aInterfaceIndex, aErrorCode,
                                                                   aHostName, aAddress, aTtl);
}

void PublisherMDnsSd::HostSubscription::HandleResolveResult(DNSServiceRef          aServiceRef,
                                                            DNSServiceFlags        aFlags,
                                                            uint32_t               aInterfaceIndex,
                                                            DNSServiceErrorType    aErrorCode,
                                                            const char            *aHostName,
                                                            const struct sockaddr *aAddress,
                                                            uint32_t               aTtl)
{
    OTBR_UNUSED_VARIABLE(aServiceRef);

    Ip6Address address;
    bool       isAdd = (aFlags & kDNSServiceFlagsAdd) != 0;

    otbrLog(aErrorCode == kDNSServiceErr_NoError ? OTBR_LOG_INFO : OTBR_LOG_WARNING, OTBR_LOG_TAG,
            "DNSServiceGetAddrInfo reply: flags=%" PRIu32 ", host=%s, sa_family=%u, error=%" PRId32, aFlags, aHostName,
            static_cast<unsigned int>(aAddress->sa_family), aErrorCode);

    VerifyOrExit(aErrorCode == kDNSServiceErr_NoError);
    VerifyOrExit(aAddress->sa_family == AF_INET6);

    address.CopyFrom(*reinterpret_cast<const struct sockaddr_in6 *>(aAddress));
    VerifyOrExit(!address.IsLinkLocal(),
                 otbrLogDebug("DNSServiceGetAddrInfo ignore link-local address %s", address.ToString().c_str()));

    otbrLogInfo("DNSServiceGetAddrInfo reply: %s address=%s, ttl=%" PRIu32, isAdd ? "add" : "remove",
                address.ToString().c_str(), aTtl);

    if (isAdd)
    {
        mHostInfo.AddAddress(address);
    }
    else
    {
        mHostInfo.RemoveAddress(address);
    }
    mHostInfo.mHostName   = aHostName;
    mHostInfo.mNetifIndex = aInterfaceIndex;
    mHostInfo.mTtl        = aTtl;

    // NOTE: This `HostSubscription` object may be freed in `OnHostResolved`.
    mPublisher.OnHostResolved(mHostName, mHostInfo);

exit:
    if (aErrorCode != kDNSServiceErr_NoError)
    {
        mPublisher.OnHostResolveFailed(aHostName, aErrorCode);
    }
}

} // namespace Mdns

} // namespace otbr
