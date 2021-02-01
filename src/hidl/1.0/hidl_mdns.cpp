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
 *   This file implements HIDL MDNS interface.
 */

#include "hidl/1.0/hidl_mdns.hpp"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <hidl/HidlTransportSupport.h>

#include "common/code_utils.hpp"
#include "common/logging.hpp"

#if OTBR_ENABLE_MDNS_MDNSSD_HIDL
#if !OTBR_ENABLE_HIDL_SERVER
#error "OTBR_ENABLE_HIDL_SERVER is required for OTBR_ENABLE_MDNS_MDNSSD_HIDL."
#endif

namespace otbr {
namespace Hidl {
using android::hardware::setupTransportPolling;

HidlMdns::HidlMdns(void)
    : mMdnsCallback(nullptr)
    , mServiceRegisterCallback(nullptr)
    , mServiceRegisterContext(nullptr)
    , mServiceRegisterRecordCallback(nullptr)
    , mServiceRegisterRecordContext(nullptr)
    , mStateUpdateCallback(nullptr)
    , mStateUpdateCallbackContext(nullptr)
{
    VerifyOrDie((setupTransportPolling()) >= 0, "Setup HIDL transport for use with (e)poll failed");
}

void HidlMdns::Init(void)
{
    otbrLog(OTBR_LOG_INFO, "Register HIDL MDNS service");
    VerifyOrDie(registerAsService() == android::NO_ERROR, "Register HIDL MDNS service failed");

    mDeathRecipient = new ClientDeathRecipient(sClientDeathCallback, this);
    VerifyOrDie(mDeathRecipient != nullptr, "Create client death reciptient failed");
}

Return<void> HidlMdns::initialize(const sp<IThreadMdnsCallback> &aCallback)
{
    VerifyOrExit(aCallback != NULL);

    mMdnsCallback = aCallback;

    mDeathRecipient->SetClientHasDied(false);
    mMdnsCallback->linkToDeath(mDeathRecipient, 3);

    if (mStateUpdateCallback != nullptr)
    {
        mStateUpdateCallback(kDNSServiceStateIsReady, mStateUpdateCallbackContext);
    }

    otbrLog(OTBR_LOG_INFO, "HIDL MDNS interface initialized");

exit:
    return Void();
}

Return<void> HidlMdns::deinitialize(void)
{
    if ((!mDeathRecipient->GetClientHasDied()) && (mMdnsCallback != nullptr))
    {
        mMdnsCallback->unlinkToDeath(mDeathRecipient);
        mDeathRecipient->SetClientHasDied(true);
    }

    mMdnsCallback = nullptr;

    if (mStateUpdateCallback != nullptr)
    {
        mStateUpdateCallback(kDNSServiceStateIdle, mStateUpdateCallbackContext);
    }

    otbrLog(OTBR_LOG_INFO, "HIDL MDNS interface deinitialized");

    return Void();
}

void HidlMdns::ServiceInit(MdnsStateUpdatedCallback aCallback, void *aContext)
{
    mStateUpdateCallback        = aCallback;
    mStateUpdateCallbackContext = aContext;
}

bool HidlMdns::IsReady(void)
{
    return (mMdnsCallback != nullptr);
}

DNSServiceErrorType HidlMdns::ServiceCreateConnection(DNSServiceRef *aServiceRef)
{
    DNSServiceErrorType error;

    VerifyOrExit(mMdnsCallback != nullptr, error = kDNSServiceErr_BadState);

    mMdnsCallback->onServiceCreateConnection([&](int aError, uint32_t aServiceRefId) {
        if ((error = aError) == kDNSServiceErr_NoError)
        {
            *aServiceRef = aServiceRefId;
        }
    });

exit:
    return error;
}

Return<void> HidlMdns::setServiceRegisterReply(uint32_t           aServiceRef,
                                               uint32_t           aFlags,
                                               int32_t            aError,
                                               const hidl_string &aName,
                                               const hidl_string &aType,
                                               const hidl_string &aDomain)
{
    if (mServiceRegisterCallback != nullptr)
    {
        mServiceRegisterCallback(aServiceRef, aFlags, aError, aName.c_str(), aType.c_str(), aDomain.c_str(),
                                 mServiceRegisterContext);
        mServiceRegisterCallback = nullptr;
    }

    return Void();
}

DNSServiceErrorType HidlMdns::ServiceRegister(DNSServiceRef *         aServiceRef,
                                              DNSServiceFlags         aFlags,
                                              uint32_t                aInterfaceIndex,
                                              const char *            aServiceName,
                                              const char *            aServiceType,
                                              const char *            aDomain,
                                              const char *            aHost,
                                              uint16_t                aPort,
                                              uint16_t                aTxtLength,
                                              const void *            aTxtRecord,
                                              DNSServiceRegisterReply aCallback,
                                              void *                  aContext)
{
    DNSServiceErrorType error;

    VerifyOrExit(mMdnsCallback != nullptr, error = kDNSServiceErr_BadState);

    mServiceRegisterCallback = aCallback;
    mServiceRegisterContext  = aContext;

    mMdnsCallback->onServiceRegister(aFlags, aInterfaceIndex, ToHidlString(aServiceName), ToHidlString(aServiceType),
                                     ToHidlString(aDomain), ToHidlString(aHost), aPort,
                                     hidl_vec<uint8_t>(reinterpret_cast<const uint8_t *>(aTxtRecord),
                                                       reinterpret_cast<const uint8_t *>(aTxtRecord) + aTxtLength),
                                     [&](int32_t aError, uint32_t aServiceRefId) {
                                         if ((error = aError) == kDNSServiceErr_NoError)
                                         {
                                             *aServiceRef = aServiceRefId;
                                         }
                                     });

exit:
    return error;
}

Return<void> HidlMdns::setServiceRegisterRecordReply(uint32_t aServiceRef,
                                                     uint32_t aRecordRef,
                                                     uint32_t aFlags,
                                                     int32_t  aError)
{
    if (mServiceRegisterRecordCallback != nullptr)
    {
        mServiceRegisterRecordCallback(aServiceRef, aRecordRef, aFlags, aError, mServiceRegisterRecordContext);
        mServiceRegisterRecordCallback = nullptr;
    }

    return Void();
}

DNSServiceErrorType HidlMdns::ServiceRegisterRecord(DNSServiceRef                 aServiceRef,
                                                    DNSRecordRef *                aRecordRef,
                                                    DNSServiceFlags               aFlags,
                                                    uint32_t                      aInterfaceIndex,
                                                    const char *                  aFullName,
                                                    uint16_t                      aResourceRecordType,
                                                    uint16_t                      aResourceRecordClass,
                                                    uint16_t                      aResourceDataLength,
                                                    const void *                  aResourceData,
                                                    uint32_t                      aTimeToLive,
                                                    DNSServiceRegisterRecordReply aCallback,
                                                    void *                        aContext)
{
    DNSServiceErrorType error;

    VerifyOrExit(mMdnsCallback != nullptr, error = kDNSServiceErr_BadState);

    mServiceRegisterRecordCallback = aCallback;
    mServiceRegisterRecordContext  = aContext;

    mMdnsCallback->onServiceRegisterRecord(
        aServiceRef, aFlags, aInterfaceIndex, aFullName, aResourceRecordType, aResourceRecordClass,
        hidl_vec<uint8_t>(reinterpret_cast<const uint8_t *>(aResourceData),
                          reinterpret_cast<const uint8_t *>(aResourceData) + aResourceDataLength),
        aTimeToLive, [&](int32_t aError, uint32_t aRecordRefId) {
            if ((error = aError) == kDNSServiceErr_NoError)
            {
                *aRecordRef = aRecordRefId;
            }
        });

exit:
    return error;
}

DNSServiceErrorType HidlMdns::ServiceUpdateRecord(DNSServiceRef   aServiceRef,
                                                  DNSRecordRef    aRecordRef,
                                                  DNSServiceFlags aFlags,
                                                  uint16_t        aResourceDataLength,
                                                  const void *    aResourceData,
                                                  uint32_t        aTimeToLive)
{
    DNSServiceErrorType error;

    VerifyOrExit(mMdnsCallback != nullptr, error = kDNSServiceErr_BadState);
    error = mMdnsCallback->onServiceUpdateRecord(
        aServiceRef, aRecordRef, aFlags,
        hidl_vec<uint8_t>(reinterpret_cast<const uint8_t *>(aResourceData),
                          reinterpret_cast<const uint8_t *>(aResourceData) + aResourceDataLength),
        aTimeToLive);

exit:
    return error;
}

DNSServiceErrorType HidlMdns::ServiceRemoveRecord(DNSServiceRef   aServiceRef,
                                                  DNSRecordRef    aRecordRef,
                                                  DNSServiceFlags aFlags)
{
    DNSServiceErrorType error;

    VerifyOrExit(mMdnsCallback != nullptr, error = kDNSServiceErr_BadState);
    error = mMdnsCallback->onServiceRemoveRecord(aServiceRef, aRecordRef, aFlags);

exit:
    return error;
}

void HidlMdns::ServiceRefDeallocate(DNSServiceRef aServiceRef)
{
    VerifyOrExit(mMdnsCallback != nullptr);
    mMdnsCallback->onServiceRefDeallocate(aServiceRef);

exit:
    return;
}

} // namespace Hidl

} // namespace otbr

#include "hidl/1.0/hidl_agent.hpp"

extern std::unique_ptr<otbr::Hidl::HidlAgent> gHidlAgent;

DNSServiceErrorType HidlMdnsInit(MdnsStateUpdatedCallback aCallback, void *aContext)
{
    DNSServiceErrorType error = kDNSServiceErr_NoError;

    VerifyOrExit(gHidlAgent != nullptr, error = kDNSServiceErr_BadState);
    gHidlAgent->GetMdns().ServiceInit(aCallback, aContext);

exit:
    return error;
}

bool HidlMdnsIsReady(void)
{
    bool isReady;

    VerifyOrExit(gHidlAgent != nullptr, isReady = false);
    isReady = gHidlAgent->GetMdns().IsReady();

exit:
    return isReady;
}

DNSServiceErrorType DNSServiceCreateConnection(DNSServiceRef *aServiceRef)
{
    DNSServiceErrorType error;

    VerifyOrExit(HidlMdnsIsReady(), error = kDNSServiceErr_BadState);
    error = gHidlAgent->GetMdns().ServiceCreateConnection(aServiceRef);

exit:
    return error;
}

DNSServiceErrorType DNSServiceRegister(DNSServiceRef *         aServiceRef,
                                       DNSServiceFlags         aFlags,
                                       uint32_t                aInterfaceIndex,
                                       const char *            aName,
                                       const char *            aType,
                                       const char *            aDomain,
                                       const char *            aHost,
                                       uint16_t                aPort,
                                       uint16_t                aTxtLength,
                                       const void *            aTxtRecord,
                                       DNSServiceRegisterReply aCallback,
                                       void *                  aContext)
{
    DNSServiceErrorType error;

    VerifyOrExit(HidlMdnsIsReady(), error = kDNSServiceErr_BadState);
    error = gHidlAgent->GetMdns().ServiceRegister(aServiceRef, aFlags, aInterfaceIndex, aName, aType, aDomain, aHost,
                                                  aPort, aTxtLength, aTxtRecord, aCallback, aContext);

exit:
    return error;
}

DNSServiceErrorType DNSServiceRegisterRecord(DNSServiceRef                 aServiceRef,
                                             DNSRecordRef *                aRecordRef,
                                             DNSServiceFlags               aFlags,
                                             uint32_t                      aInterfaceIndex,
                                             const char *                  aFullName,
                                             uint16_t                      aRRType,
                                             uint16_t                      aRRclass,
                                             uint16_t                      aResourceDataLength,
                                             const void *                  aResourceData,
                                             uint32_t                      aTimeToLive,
                                             DNSServiceRegisterRecordReply aCallback,
                                             void *                        aContext)
{
    DNSServiceErrorType error;

    VerifyOrExit(HidlMdnsIsReady(), error = kDNSServiceErr_BadState);
    error = gHidlAgent->GetMdns().ServiceRegisterRecord(aServiceRef, aRecordRef, aFlags, aInterfaceIndex, aFullName,
                                                        aRRType, aRRclass, aResourceDataLength, aResourceData,
                                                        aTimeToLive, aCallback, aContext);

exit:
    return error;
}

DNSServiceErrorType DNSServiceUpdateRecord(DNSServiceRef   aServiceRef,
                                           DNSRecordRef    aRecordRef,
                                           DNSServiceFlags aFlags,
                                           uint16_t        aResourceDataLength,
                                           const void *    aResourceData,
                                           uint32_t        aTimeToLive)
{
    DNSServiceErrorType error;

    VerifyOrExit(HidlMdnsIsReady(), error = kDNSServiceErr_BadState);
    error = gHidlAgent->GetMdns().ServiceUpdateRecord(aServiceRef, aRecordRef, aFlags, aResourceDataLength,
                                                      aResourceData, aTimeToLive);

exit:
    return error;
}

DNSServiceErrorType DNSServiceRemoveRecord(DNSServiceRef aServiceRef, DNSRecordRef aRecordRef, DNSServiceFlags aFlags)
{
    DNSServiceErrorType error;

    VerifyOrExit(HidlMdnsIsReady(), error = kDNSServiceErr_BadState);
    error = gHidlAgent->GetMdns().ServiceRemoveRecord(aServiceRef, aRecordRef, aFlags);

exit:
    return error;
}

void DNSServiceRefDeallocate(DNSServiceRef aServiceRef)
{
    VerifyOrExit(HidlMdnsIsReady());
    gHidlAgent->GetMdns().ServiceRefDeallocate(aServiceRef);

exit:
    return;
}
#endif // OTBR_ENABLE_MDNS_MDNSSD_HIDL
