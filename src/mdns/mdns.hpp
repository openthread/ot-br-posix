/*
 *    Copyright (c) 2017, The OpenThread Authors.
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
 *   This file includes definitions for mDNS publisher.
 */

#ifndef OTBR_AGENT_MDNS_HPP_
#define OTBR_AGENT_MDNS_HPP_

#include "openthread-br/config.h"

#ifndef OTBR_ENABLE_MDNS
#define OTBR_ENABLE_MDNS (OTBR_ENABLE_MDNS_AVAHI || OTBR_ENABLE_MDNS_MDNSSD)
#endif

#include <functional>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <sys/select.h>

#include "common/callback.hpp"
#include "common/code_utils.hpp"
#include "common/time.hpp"
#include "common/types.hpp"

namespace otbr {

namespace Mdns {

/**
 * @addtogroup border-router-mdns
 *
 * @brief
 *   This module includes definition for mDNS publisher.
 *
 * @{
 */

/**
 * This interface defines the functionality of mDNS publisher.
 */
class Publisher : private NonCopyable
{
public:
    /**
     * This structure represents a key/value pair of the TXT record.
     */
    struct TxtEntry
    {
        std::string          mKey;                ///< The key of the TXT entry.
        std::vector<uint8_t> mValue;              ///< The value of the TXT entry. Can be empty.
        bool                 mIsBooleanAttribute; ///< This entry is boolean attribute (encoded as `key` without `=`).

        TxtEntry(const char *aKey, const char *aValue)
            : TxtEntry(aKey, reinterpret_cast<const uint8_t *>(aValue), strlen(aValue))
        {
        }

        TxtEntry(const char *aKey, const uint8_t *aValue, size_t aValueLength)
            : TxtEntry(aKey, strlen(aKey), aValue, aValueLength)
        {
        }

        TxtEntry(const char *aKey, size_t aKeyLength, const uint8_t *aValue, size_t aValueLength)
            : mKey(aKey, aKeyLength)
            , mValue(aValue, aValue + aValueLength)
            , mIsBooleanAttribute(false)
        {
        }

        TxtEntry(const char *aKey)
            : TxtEntry(aKey, strlen(aKey))
        {
        }

        TxtEntry(const char *aKey, size_t aKeyLength)
            : mKey(aKey, aKeyLength)
            , mIsBooleanAttribute(true)
        {
        }

        bool operator==(const TxtEntry &aOther) const
        {
            return (mKey == aOther.mKey) && (mValue == aOther.mValue) &&
                   (mIsBooleanAttribute == aOther.mIsBooleanAttribute);
        }
    };

    typedef std::vector<uint8_t>     TxtData;
    typedef std::vector<TxtEntry>    TxtList;
    typedef std::vector<std::string> SubTypeList;
    typedef std::vector<Ip6Address>  AddressList;
    typedef std::vector<uint8_t>     KeyData;

    /**
     * This structure represents information of a discovered service instance.
     */
    struct DiscoveredInstanceInfo
    {
        bool        mRemoved    = false; ///< The Service Instance is removed.
        uint32_t    mNetifIndex = 0;     ///< Network interface.
        std::string mName;               ///< Instance name.
        std::string mHostName;           ///< Full host name.
        AddressList mAddresses;          ///< IPv6 addresses.
        uint16_t    mPort     = 0;       ///< Port.
        uint16_t    mPriority = 0;       ///< Service priority.
        uint16_t    mWeight   = 0;       ///< Service weight.
        TxtData     mTxtData;            ///< TXT RDATA bytes.
        uint32_t    mTtl = 0;            ///< Service TTL.

        void AddAddress(const Ip6Address &aAddress) { Publisher::AddAddress(mAddresses, aAddress); }
        void RemoveAddress(const Ip6Address &aAddress) { Publisher::RemoveAddress(mAddresses, aAddress); }
    };

    /**
     * This structure represents information of a discovered host.
     */
    struct DiscoveredHostInfo
    {
        std::string mHostName;       ///< Full host name.
        AddressList mAddresses;      ///< IP6 addresses.
        uint32_t    mNetifIndex = 0; ///< Network interface.
        uint32_t    mTtl        = 0; ///< Host TTL.

        void AddAddress(const Ip6Address &aAddress) { Publisher::AddAddress(mAddresses, aAddress); }
        void RemoveAddress(const Ip6Address &aAddress) { Publisher::RemoveAddress(mAddresses, aAddress); }
    };

    /**
     * This function is called to notify a discovered service instance.
     */
    using DiscoveredServiceInstanceCallback =
        std::function<void(const std::string &aType, const DiscoveredInstanceInfo &aInstanceInfo)>;

    /**
     * This function is called to notify a discovered host.
     */
    using DiscoveredHostCallback =
        std::function<void(const std::string &aHostName, const DiscoveredHostInfo &aHostInfo)>;

    /**
     * mDNS state values.
     */
    enum class State
    {
        kIdle,  ///< Unable to publish service.
        kReady, ///< Ready to publish service.
    };

    /** The callback for receiving mDNS publisher state changes. */
    using StateCallback = std::function<void(State aNewState)>;

    /** The callback for receiving the result of a operation. */
    using ResultCallback = OnceCallback<void(otbrError aError)>;

    /**
     * This method starts the mDNS publisher.
     *
     * @retval OTBR_ERROR_NONE  Successfully started mDNS publisher;
     * @retval OTBR_ERROR_MDNS  Failed to start mDNS publisher.
     */
    virtual otbrError Start(void) = 0;

    /**
     * This method stops the mDNS publisher.
     */
    virtual void Stop(void) = 0;

    /**
     * This method checks if publisher has been started.
     *
     * @retval true   Already started.
     * @retval false  Not started.
     */
    virtual bool IsStarted(void) const = 0;

    /**
     * This method publishes or updates a service.
     *
     * @param[in] aHostName     The name of the host which this service resides on. If an empty string is
     *                          provided, this service resides on local host and it is the implementation
     *                          to provide specific host name. Otherwise, the caller MUST publish the host
     *                          with method PublishHost.
     * @param[in] aName         The name of this service. If an empty string is provided, the service's name will be the
     *                          same as the platform's hostname.
     * @param[in] aType         The type of this service, e.g., "_srv._udp" (MUST NOT end with dot).
     * @param[in] aSubTypeList  A list of service subtypes.
     * @param[in] aPort         The port number of this service.
     * @param[in] aTxtData      The encoded TXT data for this service.
     * @param[in] aCallback     The callback for receiving the publishing result. `OTBR_ERROR_NONE` will be
     *                          returned if the operation is successful and all other values indicate a
     *                          failure. Specifically, `OTBR_ERROR_DUPLICATED` indicates that the name has
     *                          already been published and the caller can re-publish with a new name if an
     *                          alternative name is available/acceptable.
     */
    void PublishService(const std::string &aHostName,
                        const std::string &aName,
                        const std::string &aType,
                        const SubTypeList &aSubTypeList,
                        uint16_t           aPort,
                        const TxtData     &aTxtData,
                        ResultCallback   &&aCallback);

    /**
     * This method un-publishes a service.
     *
     * @param[in] aName      The name of this service.
     * @param[in] aType      The type of this service, e.g., "_srv._udp" (MUST NOT end with dot).
     * @param[in] aCallback  The callback for receiving the publishing result.
     */
    virtual void UnpublishService(const std::string &aName, const std::string &aType, ResultCallback &&aCallback) = 0;

    /**
     * This method publishes or updates a host.
     *
     * Publishing a host is advertising an AAAA RR for the host name. This method should be called
     * before a service with non-empty host name is published.
     *
     * @param[in] aName       The name of the host.
     * @param[in] aAddresses  The addresses of the host.
     * @param[in] aCallback   The callback for receiving the publishing result.`OTBR_ERROR_NONE` will be
     *                        returned if the operation is successful and all other values indicate a
     *                        failure. Specifically, `OTBR_ERROR_DUPLICATED` indicates that the name has
     *                        already been published and the caller can re-publish with a new name if an
     *                        alternative name is available/acceptable.
     */
    void PublishHost(const std::string &aName, const AddressList &aAddresses, ResultCallback &&aCallback);

    /**
     * This method un-publishes a host.
     *
     * @param[in] aName      A host name (MUST not end with dot).
     * @param[in] aCallback  The callback for receiving the publishing result.
     */
    virtual void UnpublishHost(const std::string &aName, ResultCallback &&aCallback) = 0;

    /**
     * This method publishes or updates a key record for a name.
     *
     * @param[in] aName       The name associated with key record (can be host name or service instance name).
     * @param[in] aKeyData    The key data to publish.
     * @param[in] aCallback   The callback for receiving the publishing result.`OTBR_ERROR_NONE` will be
     *                        returned if the operation is successful and all other values indicate a
     *                        failure. Specifically, `OTBR_ERROR_DUPLICATED` indicates that the name has
     *                        already been published and the caller can re-publish with a new name if an
     *                        alternative name is available/acceptable.
     */
    void PublishKey(const std::string &aName, const KeyData &aKeyData, ResultCallback &&aCallback);

    /**
     * This method un-publishes a key record
     *
     * @param[in] aName      The name associated with key record.
     * @param[in] aCallback  The callback for receiving the publishing result.
     */
    virtual void UnpublishKey(const std::string &aName, ResultCallback &&aCallback) = 0;

    /**
     * This method subscribes a given service or service instance.
     *
     * If @p aInstanceName is not empty, this method subscribes the service instance. Otherwise, this method subscribes
     * the service. mDNS implementations should use the `DiscoveredServiceInstanceCallback` function to notify
     * discovered service instances.
     *
     * @note Discovery Proxy implementation guarantees no duplicate subscriptions for the same service or service
     * instance.
     *
     * @param[in] aType          The service type, e.g., "_srv._udp" (MUST NOT end with dot).
     * @param[in] aInstanceName  The service instance to subscribe, or empty to subscribe the service.
     */
    virtual void SubscribeService(const std::string &aType, const std::string &aInstanceName) = 0;

    /**
     * This method unsubscribes a given service or service instance.
     *
     * If @p aInstanceName is not empty, this method unsubscribes the service instance. Otherwise, this method
     * unsubscribes the service.
     *
     * @note Discovery Proxy implementation guarantees no redundant unsubscription for a service or service instance.
     *
     * @param[in] aType          The service type, e.g., "_srv._udp" (MUST NOT end with dot).
     * @param[in] aInstanceName  The service instance to unsubscribe, or empty to unsubscribe the service.
     */
    virtual void UnsubscribeService(const std::string &aType, const std::string &aInstanceName) = 0;

    /**
     * This method subscribes a given host.
     *
     * mDNS implementations should use the `DiscoveredHostCallback` function to notify discovered hosts.
     *
     * @note Discovery Proxy implementation guarantees no duplicate subscriptions for the same host.
     *
     * @param[in] aHostName  The host name (without domain).
     */
    virtual void SubscribeHost(const std::string &aHostName) = 0;

    /**
     * This method unsubscribes a given host.
     *
     * @note Discovery Proxy implementation guarantees no redundant unsubscription for a host.
     *
     * @param[in] aHostName  The host name (without domain).
     */
    virtual void UnsubscribeHost(const std::string &aHostName) = 0;

    /**
     * This method sets the callbacks for subscriptions.
     *
     * @param[in] aInstanceCallback  The callback function to receive discovered service instances.
     * @param[in] aHostCallback      The callback function to receive discovered hosts.
     *
     * @returns  The Subscriber ID for the callbacks.
     */
    uint64_t AddSubscriptionCallbacks(DiscoveredServiceInstanceCallback aInstanceCallback,
                                      DiscoveredHostCallback            aHostCallback);

    /**
     * This method cancels callbacks for subscriptions.
     *
     * @param[in] aSubscriberId  The Subscriber ID previously returned by `AddSubscriptionCallbacks`.
     */
    void RemoveSubscriptionCallbacks(uint64_t aSubscriberId);

    /**
     * This method returns the mDNS statistics information of the publisher.
     *
     * @returns  The MdnsTelemetryInfo of the publisher.
     */
    const MdnsTelemetryInfo &GetMdnsTelemetryInfo(void) const { return mTelemetryInfo; }

    virtual ~Publisher(void) = default;

    /**
     * This function creates a mDNS publisher.
     *
     * @param[in] aCallback  The callback for receiving mDNS publisher state changes.
     *
     * @returns A pointer to the newly created mDNS publisher.
     */
    static Publisher *Create(StateCallback aCallback);

    /**
     * This function destroys the mDNS publisher.
     *
     * @param[in] aPublisher  A pointer to the publisher.
     */
    static void Destroy(Publisher *aPublisher);

    /**
     * This function writes the TXT entry list to a TXT data buffer. The TXT entries
     * will be sorted by their keys.
     *
     * The output data is in standard DNS-SD TXT data format.
     * See RFC 6763 for details: https://tools.ietf.org/html/rfc6763#section-6.
     *
     * @param[in]  aTxtList  A TXT entry list.
     * @param[out] aTxtData  A TXT data buffer. Will be cleared.
     *
     * @retval OTBR_ERROR_NONE          Successfully write the TXT entry list.
     * @retval OTBR_ERROR_INVALID_ARGS  The @p aTxtList includes invalid TXT entry.
     *
     * @sa DecodeTxtData
     */
    static otbrError EncodeTxtData(const TxtList &aTxtList, TxtData &aTxtData);

    /**
     * This function decodes a TXT entry list from a TXT data buffer.
     *
     * The input data should be in standard DNS-SD TXT data format.
     * See RFC 6763 for details: https://tools.ietf.org/html/rfc6763#section-6.
     *
     * @param[out]  aTxtList    A TXT entry list.
     * @param[in]   aTxtData    A pointer to TXT data.
     * @param[in]   aTxtLength  The TXT data length.
     *
     * @retval OTBR_ERROR_NONE          Successfully decoded the TXT data.
     * @retval OTBR_ERROR_INVALID_ARGS  The @p aTxtdata has invalid TXT format.
     *
     * @sa EncodeTxtData
     */
    static otbrError DecodeTxtData(TxtList &aTxtList, const uint8_t *aTxtData, uint16_t aTxtLength);

protected:
    static constexpr uint8_t kMaxTextEntrySize = 255;

    class Registration
    {
    public:
        ResultCallback mCallback;
        Publisher     *mPublisher;

        Registration(ResultCallback &&aCallback, Publisher *aPublisher)
            : mCallback(std::move(aCallback))
            , mPublisher(aPublisher)
        {
        }
        virtual ~Registration(void);

        // Tells whether the service registration has been completed (typically by calling
        // `ServiceRegistration::Complete`).
        bool IsCompleted() const { return mCallback.IsNull(); }

    protected:
        // Completes the service registration with given result/error.
        void TriggerCompleteCallback(otbrError aError)
        {
            if (!IsCompleted())
            {
                std::move(mCallback)(aError);
            }
        }
    };

    // TODO: We may need a registration ID to fetch the information of a registration.
    class ServiceRegistration : public Registration
    {
    public:
        std::string mHostName;
        std::string mName;
        std::string mType;
        SubTypeList mSubTypeList;
        uint16_t    mPort;
        TxtData     mTxtData;

        ServiceRegistration(std::string      aHostName,
                            std::string      aName,
                            std::string      aType,
                            SubTypeList      aSubTypeList,
                            uint16_t         aPort,
                            TxtData          aTxtData,
                            ResultCallback &&aCallback,
                            Publisher       *aPublisher)
            : Registration(std::move(aCallback), aPublisher)
            , mHostName(std::move(aHostName))
            , mName(std::move(aName))
            , mType(std::move(aType))
            , mSubTypeList(SortSubTypeList(std::move(aSubTypeList)))
            , mPort(aPort)
            , mTxtData(std::move(aTxtData))
        {
        }
        ~ServiceRegistration(void) override { OnComplete(OTBR_ERROR_ABORTED); }

        void Complete(otbrError aError);

        // Tells whether this `ServiceRegistration` object is outdated comparing to the given parameters.
        bool IsOutdated(const std::string &aHostName,
                        const std::string &aName,
                        const std::string &aType,
                        const SubTypeList &aSubTypeList,
                        uint16_t           aPort,
                        const TxtData     &aTxtData) const;

    private:
        void OnComplete(otbrError aError);
    };

    class HostRegistration : public Registration
    {
    public:
        std::string mName;
        AddressList mAddresses;

        HostRegistration(std::string aName, AddressList aAddresses, ResultCallback &&aCallback, Publisher *aPublisher)
            : Registration(std::move(aCallback), aPublisher)
            , mName(std::move(aName))
            , mAddresses(SortAddressList(std::move(aAddresses)))
        {
        }

        ~HostRegistration(void) override { OnComplete(OTBR_ERROR_ABORTED); }

        void Complete(otbrError aError);

        // Tells whether this `HostRegistration` object is outdated comparing to the given parameters.
        bool IsOutdated(const std::string &aName, const AddressList &aAddresses) const;

    private:
        void OnComplete(otbrError aError);
    };

    class KeyRegistration : public Registration
    {
    public:
        std::string mName;
        KeyData     mKeyData;

        KeyRegistration(std::string aName, KeyData aKeyData, ResultCallback &&aCallback, Publisher *aPublisher)
            : Registration(std::move(aCallback), aPublisher)
            , mName(std::move(aName))
            , mKeyData(std::move(aKeyData))
        {
        }

        ~KeyRegistration(void) { OnComplete(OTBR_ERROR_ABORTED); }

        void Complete(otbrError aError);

        // Tells whether this `KeyRegistration` object is outdated comparing to the given parameters.
        bool IsOutdated(const std::string &aName, const KeyData &aKeyData) const;

    private:
        void OnComplete(otbrError aError);
    };

    using ServiceRegistrationPtr = std::unique_ptr<ServiceRegistration>;
    using ServiceRegistrationMap = std::map<std::string, ServiceRegistrationPtr>;
    using HostRegistrationPtr    = std::unique_ptr<HostRegistration>;
    using HostRegistrationMap    = std::map<std::string, HostRegistrationPtr>;
    using KeyRegistrationPtr     = std::unique_ptr<KeyRegistration>;
    using KeyRegistrationMap     = std::map<std::string, KeyRegistrationPtr>;

    static SubTypeList SortSubTypeList(SubTypeList aSubTypeList);
    static AddressList SortAddressList(AddressList aAddressList);
    static std::string MakeFullName(const std::string &aName);
    static std::string MakeFullServiceName(const std::string &aName, const std::string &aType);
    static std::string MakeFullHostName(const std::string &aName) { return MakeFullName(aName); }
    static std::string MakeFullKeyName(const std::string &aName) { return MakeFullName(aName); }

    virtual otbrError PublishServiceImpl(const std::string &aHostName,
                                         const std::string &aName,
                                         const std::string &aType,
                                         const SubTypeList &aSubTypeList,
                                         uint16_t           aPort,
                                         const TxtData     &aTxtData,
                                         ResultCallback   &&aCallback) = 0;

    virtual otbrError PublishHostImpl(const std::string &aName,
                                      const AddressList &aAddresses,
                                      ResultCallback   &&aCallback) = 0;

    virtual otbrError PublishKeyImpl(const std::string &aName, const KeyData &aKeyData, ResultCallback &&aCallback) = 0;

    virtual void OnServiceResolveFailedImpl(const std::string &aType,
                                            const std::string &aInstanceName,
                                            int32_t            aErrorCode) = 0;

    virtual void OnHostResolveFailedImpl(const std::string &aHostName, int32_t aErrorCode) = 0;

    virtual otbrError DnsErrorToOtbrError(int32_t aError) = 0;

    void AddServiceRegistration(ServiceRegistrationPtr &&aServiceReg);
    void RemoveServiceRegistration(const std::string &aName, const std::string &aType, otbrError aError);
    ServiceRegistration *FindServiceRegistration(const std::string &aName, const std::string &aType);
    ServiceRegistration *FindServiceRegistration(const std::string &aNameAndType);

    void OnServiceResolved(std::string aType, DiscoveredInstanceInfo aInstanceInfo);
    void OnServiceResolveFailed(std::string aType, std::string aInstanceName, int32_t aErrorCode);
    void OnServiceRemoved(uint32_t aNetifIndex, std::string aType, std::string aInstanceName);
    void OnHostResolved(std::string aHostName, DiscoveredHostInfo aHostInfo);
    void OnHostResolveFailed(std::string aHostName, int32_t aErrorCode);

    // Handles the cases that there is already a registration for the same service.
    // If the returned callback is completed, current registration should be considered
    // success and no further action should be performed.
    ResultCallback HandleDuplicateServiceRegistration(const std::string &aHostName,
                                                      const std::string &aName,
                                                      const std::string &aType,
                                                      const SubTypeList &aSubTypeList,
                                                      uint16_t           aPort,
                                                      const TxtData     &aTxtData,
                                                      ResultCallback   &&aCallback);

    ResultCallback HandleDuplicateHostRegistration(const std::string &aName,
                                                   const AddressList &aAddresses,
                                                   ResultCallback   &&aCallback);

    ResultCallback HandleDuplicateKeyRegistration(const std::string &aName,
                                                  const KeyData     &aKeyData,
                                                  ResultCallback   &&aCallback);

    void              AddHostRegistration(HostRegistrationPtr &&aHostReg);
    void              RemoveHostRegistration(const std::string &aName, otbrError aError);
    HostRegistration *FindHostRegistration(const std::string &aName);

    void             AddKeyRegistration(KeyRegistrationPtr &&aKeyReg);
    void             RemoveKeyRegistration(const std::string &aName, otbrError aError);
    KeyRegistration *FindKeyRegistration(const std::string &aName);
    KeyRegistration *FindKeyRegistration(const std::string &aName, const std::string &aType);

    static void UpdateMdnsResponseCounters(MdnsResponseCounters &aCounters, otbrError aError);
    static void UpdateEmaLatency(uint32_t &aEmaLatency, uint32_t aLatency, otbrError aError);

    void UpdateServiceRegistrationEmaLatency(const std::string &aInstanceName,
                                             const std::string &aType,
                                             otbrError          aError);
    void UpdateHostRegistrationEmaLatency(const std::string &aHostName, otbrError aError);
    void UpdateKeyRegistrationEmaLatency(const std::string &aKeyName, otbrError aError);
    void UpdateServiceInstanceResolutionEmaLatency(const std::string &aInstanceName,
                                                   const std::string &aType,
                                                   otbrError          aError);
    void UpdateHostResolutionEmaLatency(const std::string &aHostName, otbrError aError);

    static void AddAddress(AddressList &aAddressList, const Ip6Address &aAddress);
    static void RemoveAddress(AddressList &aAddressList, const Ip6Address &aAddress);

    ServiceRegistrationMap mServiceRegistrations;
    HostRegistrationMap    mHostRegistrations;
    KeyRegistrationMap     mKeyRegistrations;

    struct DiscoverCallback
    {
        DiscoverCallback(uint64_t                          aId,
                         DiscoveredServiceInstanceCallback aServiceCallback,
                         DiscoveredHostCallback            aHostCallback)
            : mId(aId)
            , mServiceCallback(std::move(aServiceCallback))
            , mHostCallback(std::move(aHostCallback))
            , mShouldInvoke(false)
        {
        }

        uint64_t                          mId;
        DiscoveredServiceInstanceCallback mServiceCallback;
        DiscoveredHostCallback            mHostCallback;
        bool                              mShouldInvoke;
    };

    uint64_t mNextSubscriberId = 1;

    std::list<DiscoverCallback> mDiscoverCallbacks;

    // {instance name, service type} -> the timepoint to begin service registration
    std::map<std::pair<std::string, std::string>, Timepoint> mServiceRegistrationBeginTime;
    // host name -> the timepoint to begin host registration
    std::map<std::string, Timepoint> mHostRegistrationBeginTime;
    // key name -> the timepoint to begin key registration
    std::map<std::string, Timepoint> mKeyRegistrationBeginTime;
    // {instance name, service type} -> the timepoint to begin service resolution
    std::map<std::pair<std::string, std::string>, Timepoint> mServiceInstanceResolutionBeginTime;
    // host name -> the timepoint to begin host resolution
    std::map<std::string, Timepoint> mHostResolutionBeginTime;

    MdnsTelemetryInfo mTelemetryInfo{};
};

/**
 * This interface is a mDNS State Observer.
 */
class StateObserver
{
public:
    /**
     * This method notifies the mDNS state to the observer.
     *
     * @param[in] aState  The mDNS State.
     */
    virtual void HandleMdnsState(Publisher::State aState) = 0;

    /**
     * The destructor.
     */
    virtual ~StateObserver(void) = default;
};

/**
 * This class defines a mDNS State Subject.
 */
class StateSubject
{
public:
    /**
     * Constructor.
     */
    StateSubject(void) = default;

    /**
     * Destructor.
     */
    ~StateSubject(void) = default;

    /**
     * This method adds an mDNS State Observer to this subject.
     *
     * @param[in] aObserver  A reference to the observer. If it's nullptr, it won't be added.
     */
    void AddObserver(StateObserver &aObserver);

    /**
     * This method updates the mDNS State.
     *
     * @param[in] aState  The mDNS State.
     */
    void UpdateState(Publisher::State aState);

    /**
     * This method removes all the observers.
     */
    void Clear(void);

private:
    std::vector<StateObserver *> mObservers;
};

/**
 * @}
 */

} // namespace Mdns

} // namespace otbr

#endif // OTBR_AGENT_MDNS_HPP_
