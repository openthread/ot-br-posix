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
 *   This file includes definitions for vendor MDNS service.
 */

#ifndef OTBR_AGENT_VENDOR_MDNSSD_HPP_
#define OTBR_AGENT_VENDOR_MDNSSD_HPP_

/**
 * This enumeration defines the DNS error codes.
 *
 */
enum
{
    kDNSServiceErr_NoError                   = 0,
    kDNSServiceErr_Unknown                   = -65537, /* 0xFFFE FFFF */
    kDNSServiceErr_NoSuchName                = -65538,
    kDNSServiceErr_NoMemory                  = -65539,
    kDNSServiceErr_BadParam                  = -65540,
    kDNSServiceErr_BadReference              = -65541,
    kDNSServiceErr_BadState                  = -65542,
    kDNSServiceErr_BadFlags                  = -65543,
    kDNSServiceErr_Unsupported               = -65544,
    kDNSServiceErr_NotInitialized            = -65545,
    kDNSServiceErr_AlreadyRegistered         = -65547,
    kDNSServiceErr_NameConflict              = -65548,
    kDNSServiceErr_Invalid                   = -65549,
    kDNSServiceErr_Firewall                  = -65550,
    kDNSServiceErr_Incompatible              = -65551,
    kDNSServiceErr_BadInterfaceIndex         = -65552,
    kDNSServiceErr_Refused                   = -65553,
    kDNSServiceErr_NoSuchRecord              = -65554,
    kDNSServiceErr_NoAuth                    = -65555,
    kDNSServiceErr_NoSuchKey                 = -65556,
    kDNSServiceErr_NATTraversal              = -65557,
    kDNSServiceErr_DoubleNAT                 = -65558,
    kDNSServiceErr_BadTime                   = -65559,
    kDNSServiceErr_BadSig                    = -65560,
    kDNSServiceErr_BadKey                    = -65561,
    kDNSServiceErr_Transient                 = -65562,
    kDNSServiceErr_ServiceNotRunning         = -65563,
    kDNSServiceErr_NATPortMappingUnsupported = -65564,
    kDNSServiceErr_NATPortMappingDisabled    = -65565,
    kDNSServiceErr_NoRouter                  = -65566,
    kDNSServiceErr_PollingMode               = -65567,
    kDNSServiceErr_Timeout                   = -65568
};

/**
 * This type represents the DNS service error codes.
 *
 */
typedef int32_t DNSServiceErrorType;

/**
 * This type represents an identifier for the DNS service.
 *
 * The value is an opaque unsigned integer maintained by the vendor DNS service.
 *
 */
typedef int32_t DNSServiceRef;

/**
 * This type represents an identifier for the DNS record.
 *
 * The value is an opaque unsigned identifier maintained by the vendor DNS service.
 *
 */
typedef int32_t DNSRecordRef;

/**
 * This type represents the DNS service flags.
 *
 */
typedef uint32_t DNSServiceFlags;

/**
 * This enumeration defines the DNS related constants.
 *
 */
enum
{
    kDNSServiceMaxServiceName    = 64,   ///< The max length of the service name.
    kDNSServiceMaxDomainName     = 1009, ///< The max length of the domain name.
    kDNSServiceInterfaceIndexAny = 0,    ///< This interface index indicates that if the service name is in an mDNS
                                         ///< local multicast domain then multicast on all applicable interfaces,
                                         ///< otherwise send via unicast to the appropriate DNS server.
    kDNSServiceFlagsAdd    = 0x02,       ///< This flag indicates that the domain has been successfully registered.
    kDNSServiceFlagsUnique = 0x20,       ///< This flag indicates that the record's name is to be unique on the
                                         ///< network (e.g. SRV records).
    kDNSServiceClass_IN  = 1,            ///< The DNS service class Internet.
    kDNSServiceType_AAAA = 28,           ///< The DNS service type AAAA.
};

/**
 * This enumeration defines the invalid DNSServiceRef and DNSRecordRef values.
 *
 */
enum
{
    kDNSInvalidServiceRef = -1, ///< Invalid DNSServiceRef value.
    kDNSInvalidRecordRef  = -1, ///< Invalid DNSRecordRef value.
};

/**
 * This enumeration defines the DNS service state.
 *
 */
typedef enum
{
    kDNSServiceStateIdle    = 0, ///< The DNS service is unavailable.
    kDNSServiceStateIsReady = 1, ///< The DNS service is available.
} DnsServiceState;

/**
 * This function pointer is called when the DNS service state is updated.
 *
 * @param[in] aState    The DNS service state.
 * @param[in] aContext  The context pointer that was passed to the callout.
 *
 */
typedef void (*otbrVendorMdnsStateUpdatedCallback)(DnsServiceState aState, void *aContext);

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
DNSServiceErrorType otbrVendorMdnsInit(otbrVendorMdnsStateUpdatedCallback aCallback, void *aContext);

/**
 * This method indicates whether the DNS service is ready to use.
 *
 * @retval TRUE   The DNS service is ready.
 * @retval FALSE  The DNS service is not ready.
 *
 */
bool otbrVendorMdnsIsReady(void);

/**
 * This method create a connection to the daemon allowing efficient registration of multiple individual records.
 *
 * @[param[out]] aServiceRef  A pointer to an uninitialized DNSServiceRef.
 *
 * @retval kDNSServiceErr_NoError  Successfully created a connection.
 * @retval ...                     Other DNS error codes.
 *
 */
DNSServiceErrorType DNSServiceCreateConnection(DNSServiceRef *aServiceRef);

/**
 * This function pointer is called when the service specified by DNSServiceRegister() is registered successfully or
 * failed.
 *
 * @param[in] aServiceRef   The DNSServiceRef initialized by DNSServiceRegister().
 * @param[in] aFlags        When a name is successfully registered, the callback will be invoked with the
 *                          kDNSServiceFlagsAdd flag set.
 * @param[in] aError        The error code. It will be set to kDNSServiceErr_NoError on success, otherwise will
 *                          indicate the failure that occurred.
 * @param[in] aServiceName  The service name registered (if the application did not specify a name in
 *                          DNSServiceRegister(), this indicates what name was automatically chosen).
 * @param[in] aServiceType  The type of service registered, as it was passed to the callout.
 * @param[in] aDomain       The domain on which the service was registered (if the application did not specify a domain
 *                          in DNSServiceRegister(), this indicates the default domain on which the service was
 *                          registered).
 * @param[in] aContext      The context pointer that was passed to the callout.
 *
 */
typedef void (*DNSServiceRegisterReply)(DNSServiceRef       aServiceRef,
                                        DNSServiceFlags     aFlags,
                                        DNSServiceErrorType aError,
                                        const char *        aServiceName,
                                        const char *        aServiceType,
                                        const char *        aDomain,
                                        void *              aContext);

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
DNSServiceErrorType DNSServiceRegister(DNSServiceRef *         aServiceRef,
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
 * This function pointer is called when the resource record specified by DNSServiceRegisterRecord() is registered
 * successfully or failed.
 *
 * @param[in] aServiceRef   The connected DNSServiceRef initialized by DNSServiceCreateConnection().
 * @param[in] aRecordRef    The DNSRecordRef initialized by DNSServiceRegisterRecord().
 * @param[in] aFlags        Currently unused, reserved for future use.
 * @param[in] aError        The error code. It will be set to kDNSServiceErr_NoError on success, otherwise will
 *                          indicate the failure that occurred.
 * @param[in] aContext      The context pointer that was passed to the callout.
 *
 */
typedef void (*DNSServiceRegisterRecordReply)(DNSServiceRef       aServiceRef,
                                              DNSRecordRef        aRecordRef,
                                              DNSServiceFlags     aFlags,
                                              DNSServiceErrorType aError,
                                              void *              aContext);

/**
 * This methods registers an individual resource record on a connected DNSServiceRef.
 *
 * Note that name conflicts occurring for records registered via this call must be handled
 * by the client in the callback.
 *
 * @param[in]  aServiceRef           A DNSServiceRef initialized by DNSServiceCreateConnection().
 * @param[out] aRecordRef            A pointer to an uninitialized DNSRecordRef. Upon succesfull completion of this
 *                                   call, this ref may be passed to DNSServiceUpdateRecord() or
 *                                   DNSServiceRemoveRecord().
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
DNSServiceErrorType DNSServiceRegisterRecord(DNSServiceRef                 aServiceRef,
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
 *   - The primary txt record of a service registered via DNSServiceRegister()
 *   - An individual record registered by DNSServiceRegisterRecord()
 *
 * @param[in] aServiceRef          A DNSServiceRef that was initialized by DNSServiceRegister() or
 *                                 DNSServiceCreateConnection().
 * @param[in] aRecordRef           A DNSRecordRef initialized by DNSServiceRegisterRecord(), or NULL to update the
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
DNSServiceErrorType DNSServiceUpdateRecord(DNSServiceRef   aServiceRef,
                                           DNSRecordRef    aRecordRef,
                                           DNSServiceFlags aFlags,
                                           uint16_t        aResourceDataLength,
                                           const void *    aResourceData,
                                           uint32_t        aTimeToLive);

/**
 * This methods removes a record previously added to a service record set via DNSServiceAddRecord(), or deregister
 * an record registered individually via DNSServiceRegisterRecord().
 *
 * @param[in] aServiceRef  A DNSServiceRef initialized by DNSServiceRegister() or by DNSServiceCreateConnection() (if
 *                         the record being removed was registered via DNSServiceRegisterRecord()).
 * @param[in] aRecordRef   A DNSRecordRef initialized by a successful call to DNSServiceRegisterRecord().
 * @param[in] aFlags       Currently ignored, reserved for future use.
 *
 * @retval kDNSServiceErr_NoError  Successfully removed the resource record.
 * @retval ...                     Other DNS error codes.
 *
 */
DNSServiceErrorType DNSServiceRemoveRecord(DNSServiceRef aServiceRef, DNSRecordRef aRecordRef, DNSServiceFlags aFlags);

/**
 * This method terminates a connection with the daemon and free memory associated with the DNSServiceRef.
 * Any services or records registered with this DNSServiceRef will be deregistered.
 *
 * @param[in] aServiceRef  A DNSServiceRef initialized by any of the DNS Service calls.
 *
 */
void DNSServiceRefDeallocate(DNSServiceRef aServiceRef);
#endif // OTBR_AGENT_VENDOR_MDNSSD_HPP_
