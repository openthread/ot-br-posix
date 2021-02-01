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
 *   This file includes definition for MDNS HIDL interface.
 */

#pragma once

#include <openthread-br/config.h>

#include <vector>

#if OTBR_ENABLE_MDNS_MDNSSD_HIDL
#include <android/hardware/thread/1.0/IThreadMdns.h>
#include <android/hardware/thread/1.0/IThreadMdnsCallback.h>
#endif

#include "common/types.hpp"
#include "hidl/1.0/hidl_death_recipient.hpp"
#include "hidl/1.0/hidl_mdns_api.hpp"
#include "mdns/mdns.hpp"

#if OTBR_ENABLE_MDNS_MDNSSD_HIDL
namespace otbr {
namespace Hidl {
using ::android::sp;
using ::android::hardware::hidl_string;
using ::android::hardware::hidl_vec;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::hardware::thread::V1_0::IThreadMdns;
using ::android::hardware::thread::V1_0::IThreadMdnsCallback;

/**
 * This class implements HIDL MDNS interface.
 *
 */
class HidlMdns : public IThreadMdns
{
public:
    /**
     * The constructor to initialize a HIDL MDNS interface.
     *
     */
    HidlMdns(void);

    ~HidlMdns(void){};

    /**
     * This method performs initialization for the HIDL MDNS service.
     *
     */
    void Init(void);

    /**
     * This method is called by the HIDL client to initalizes the HIDL MDNS callback object.
     *
     */
    Return<void> initialize(const sp<IThreadMdnsCallback> &aCallback) override;

    /**
     * This method is called by the HIDL client to deinitalizes the HIDL MDNS callback object.
     *
     */
    Return<void> deinitialize(void) override;

    /**
     * This methid is called by the HIDL client when the service specified by ServiceRegister() is registered
     * successfully or failed.
     *
     * @param[in] aServiceRef   The DNS service reference identifier initialized by onServiceRegister().
     * @param[in] aFlags        When a name is successfully registered, the callback will be invoked with the
     *                          kDNSServiceFlagsAdd flag set.
     * @param[in] aError        The error code. It will be set to kDNSServiceErr_NoError on success, otherwise will
     *                          indicate the failure that occurred.
     * @param[in] aServiceName  The service name registered (if the application did not specify a name in
     *                          DNSServiceRegister(), this indicates what name was automatically chosen).
     * @param[in] aServiceType  The type of service registered, as it was passed to the callout.
     * @param[in] aDomain       The domain on which the service was registered (if the application did not specify a
     *                          domain in ServiceRegister(), this indicates the default domain on which the service
     *                          was registered).
     *
     */
    Return<void> setServiceRegisterReply(uint32_t           aServiceRefId,
                                         uint32_t           aFlags,
                                         int32_t            aError,
                                         const hidl_string &aServiceName,
                                         const hidl_string &aServiceType,
                                         const hidl_string &aDomain) override;

    /**
     * This method is called by the HIDL client when the resource record specified by ServiceRegisterRecord() is
     * registered successfully or failed.
     *
     * @param[in] aServiceRef   The connected DNS service reference identifier initialized by
     *                          onServiceCreateConnection().
     * @param[in] aRecordRef    The DNS record reference identifier initialized by on ServiceRegisterRecord().
     * @param[in] aFlags        Currently unused, reserved for future use.
     * @param[in] aError        The error code. It will be set to kDNSServiceErr_NoError on success, otherwise will
     *                          indicate the failure that occurred.
     *
     */
    Return<void> setServiceRegisterRecordReply(uint32_t aServiceRefId,
                                               uint32_t aRecordRef,
                                               uint32_t aFlags,
                                               int32_t  aError) override;

    /**
     * This method registers a handles to monitor the DNS service state.
     *
     * @param[in] aCallback        The function to be called when the DNS service state is updated.
     * @param[in] aContext         An application context pointer which is passed to the callback function.
     *
     * @retval kDNSServiceErr_NoError  Successfully registerd the service (any subsequent, asynchronous, errors are
     *                                 delivered to the callback).
     * @retval ...                     Other DNS error codes.
     *
     */
    void ServiceInit(MdnsStateUpdatedCallback aCallback, void *aContext);

    /**
     * This method indicates whether the DNS service is ready to use.
     *
     * @retval TRUE   The DNS service is ready.
     * @retval FALSE  The DNS service is not ready.
     *
     */
    bool IsReady(void);

    /**
     * This method creates a connection to the daemon allowing efficient registration of multiple individual records.
     *
     * @[param[out]] aServiceRef  A pointer to an uninitialized DNSServiceRef.
     *
     * @retval kDNSServiceErr_NoError  Successfully created a connection.
     * @retval ...                     Other DNS error codes.
     *
     */
    DNSServiceErrorType ServiceCreateConnection(DNSServiceRef *aServiceRef);

    /**
     * This method registers a DNS service.
     *
     * @param[out] aServiceRef     A pointer to an uninitialized DNSServiceRef. If the call succeeds then it
     *                             initializes the DNSServiceRef.
     * @param[in] aFlags           Indicates the renaming behavior on name conflict.
     * @param[in] aInterfaceIndex  The interface index. If non-zero, specifies the interface on which to register the
     *                             service. If zero, registers on all avaliable interfaces.
     * @param[in] aServiceName     A pointer to the service name to be registered or NULL to use the computer name as
     *                             the service name.
     * @param[in] aServiceType     A pointer to the service type followed by the protocol, separated by a dot.
     *                             (e.g. "_ftp._tcp").
     * @param[in] aDomain          A pointer to the domain on which to advertise the service or NULL to use the default
     *                             domain(s).
     * @param[in] aHost            A pointer to the SRV target host name or NULL to use the machines's default host
     *                             name.
     * @param[in] aPort            The port, in network byte order, on which the service accepts connections.
     * @param[in] aTxtLength       The length of the @p aTxtRecord, in bytes. Must be zero if the @p aTxtRecord is NULL.
     * @param[in] aTxtRecord       A pointer TXT record rdata.
     * @param[in] aCallback        The function to be called when the registration completes or asynchronously fails.
     * @param[in] aContext         An application context pointer which is passed to the callback function.
     *
     * @retval kDNSServiceErr_NoError  Successfully registerd the service (any subsequent, asynchronous, errors are
     *                                 delivered to the callback).
     * @retval ...                     Other DNS error codes.
     *
     */
    DNSServiceErrorType ServiceRegister(DNSServiceRef *         aServiceRef,
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
                                        void *                  aContext);

    /**
     * This methods registers an individual resource record on a connected DNSServiceRef.
     *
     * Note that name conflicts occurring for records registered via this call must be handled
     * by the client in the callback.
     *
     * @param[in]  aServiceRef           A DNSServiceRef initialized by ServiceCreateConnection().
     * @param[out] aRecordRef            A pointer to an uninitialized DNSRecordRef. Upon succesfull completion of this
     *                                   call, this ref may be passed to ServiceUpdateRecord() or ServiceRemoveRecord().
     * @param[in]  aFlags                Possible values are kDNSServiceFlagsShared or kDNSServiceFlagsUnique.
     * @param[in]  aInterfaceIndex       If non-zero, specifies the interface on which to register the record
     *                                   Passing 0 causes the record to be registered on all interfaces.
     * @param[in]  aFullName             A poniter to the full domain name of the resource record.
     * @param[in]  aResourceRecordType   The type of the resource record.
     * @param[in]  aResourceRecordClass  The class of the resource record.
     * @param[in]  aResourceDataLength   The length of the @p aResourceData, in bytes.
     * @param[in]  aResourceData         A pointer to the raw rdata, as it is to appear in the DNS record.
     * @param[in]  aTimeToLive           The time to live of the resource record, in seconds.
     * @param[in]  aCallback             The function to be called when a result is found, or if the call
     *                                   asynchronously fails (e.g. because of a name conflict.)
     * @param[in]  aContext              An application context pointer which is passed to the callback function.
     *
     * @retval kDNSServiceErr_NoError  Successfully registerd the service (any subsequent, asynchronous, errors are
     *                                 delivered to the callback).
     * @retval ...                     Other DNS error codes.
     *
     */
    DNSServiceErrorType ServiceRegisterRecord(DNSServiceRef                 aServiceRef,
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
                                              void *                        aContext);
    /**
     * This methods updates a registered resource record. The record must either be:
     *   - The primary txt record of a service registered via ServiceRegister()
     *   - An individual record registered by ServiceRegisterRecord()
     *
     * @param[in] aServiceRef          A DNSServiceRef that was initialized by ServiceRegister() or
     *                                 ServiceCreateConnection().
     * @param[in] aRecordRef           A DNSRecordRef initialized by ServiceRegisterRecord(), or NULL to update the
     *                                 service's primary txt record.
     * @param[in] aFlags               Currently ignored, reserved for future use.
     * @param[in] aResourceDataLength  The length of the @p aResourceData, in bytes.
     * @param[in] aResourceData        A pointer to resource data to be contained in the updated resource record.
     * @param[in] aTimeToLive          The time to live of the updated resource record, in seconds.
     *
     * @retval kDNSServiceErr_NoError  Successfully updated the resource record.
     * @retval ...                     Other DNS error codes.
     *
     */
    DNSServiceErrorType ServiceUpdateRecord(DNSServiceRef   aServiceRef,
                                            DNSRecordRef    aRecordRef,
                                            DNSServiceFlags aFlags,
                                            uint16_t        aResourceDataLength,
                                            const void *    aResourceData,
                                            uint32_t        aTimeToLive);

    /**
     * This methods removes a record previously added to a service record set via ServiceAddRecord(), or deregister
     * an record registered individually via ServiceRegisterRecord().
     *
     * @param[in] aServiceRef  A DNSServiceRef initialized by ServiceRegister() or by ServiceCreateConnection() (if the
     *                         record  being removed was registered via ServiceRegisterRecord()).
     * @param[in] aRecordRef   A DNSRecordRef initialized by a successful call to ServiceRegisterRecord().
     * @param[in] aFlags       Currently ignored, reserved for future use.
     *
     * @retval kDNSServiceErr_NoError  Successfully removed the resource record.
     * @retval ...                     Other DNS error codes.
     *
     */
    DNSServiceErrorType ServiceRemoveRecord(DNSServiceRef aServiceRef, DNSRecordRef aRecordRef, DNSServiceFlags aFlags);

    /**
     * This method terminates a connection with the daemon and free memory associated with the DNSServiceRef.
     * Any services or records registered with this DNSServiceRef will be deregistered.
     *
     * @param[in] aServiceRef  A DNSServiceRef initialized by any of the DNS Service calls.
     *
     */
    void ServiceRefDeallocate(DNSServiceRef aServiceRef);

private:
    hidl_string ToHidlString(const char *aString)
    {
        return ((aString == nullptr) || (strlen(aString) == 0)) ? hidl_string() : hidl_string(aString);
    }

    static void sClientDeathCallback(void *aContext)
    {
        HidlMdns *hidlMdns = static_cast<HidlMdns *>(aContext);
        hidlMdns->deinitialize();
    }

    sp<IThreadMdnsCallback>       mMdnsCallback;
    DNSServiceRegisterReply       mServiceRegisterCallback;
    void *                        mServiceRegisterContext;
    DNSServiceRegisterRecordReply mServiceRegisterRecordCallback;
    void *                        mServiceRegisterRecordContext;
    MdnsStateUpdatedCallback      mStateUpdateCallback;
    void *                        mStateUpdateCallbackContext;
    sp<ClientDeathRecipient>      mDeathRecipient;
};

} // namespace Hidl
} // namespace otbr
#endif // OTBR_ENABLE_MDNS_MDNSSD_HIDL
