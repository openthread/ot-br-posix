/*
 *    Copyright (c) 2025, The OpenThread Authors.
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
 *   This file includes definitions for implementing OpenThread DNS-SD platform APIs.
 */

#ifndef OTBR_AGENT_POSIX_DNSSD_HPP_
#define OTBR_AGENT_POSIX_DNSSD_HPP_

#include "openthread-br/config.h"

#if OTBR_ENABLE_DNSSD_PLAT

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <string>

#include <openthread/instance.h>
#include <openthread/platform/dnssd.h>

#include "common/code_utils.hpp"
#include "mdns/mdns.hpp"
#include "utils/dns_utils.hpp"
#include "utils/string_utils.hpp"

namespace otbr {

/**
 * This class implements the DNS-SD platform.
 *
 */
class DnssdPlatform : public Mdns::StateObserver, private NonCopyable
{
public:
    /**
     * Initializes the `DnssdPlatform` instance
     *
     * @param[in] aPublisher   A reference to `Mdns::Publisher` to use.
     */
    DnssdPlatform(Mdns::Publisher &aPublisher);

    /**
     * Starts the `DnssdPlatform` module.
     */
    void Start(void);

    /**
     * Stops the `DnssdPlatform` module
     */
    void Stop(void);

    /**
     * Gets the singleton `DnssdPlatform` instance.
     *
     * @returns  A reference to the `DnssdPlatform` instance.
     */
    static DnssdPlatform &Get(void) { return *sDnssdPlatform; }

    typedef std::function<void(otPlatDnssdState)> DnssdStateChangeCallback;

    /**
     * Sets a Dnssd State changed callback.
     *
     * The main usage of this method is to call `otPlatDnssdStateHandleStateChange` to notify OT core about the change
     * when the dnssd state changes. We shouldn't directly call `otPlatDnssdStateHandleStateChange` in this module'
     * because it only fits the RCP case.
     *
     * @param[in] aCallback  The callback to be invoked when the dnssd state changes.
     */
    void SetDnssdStateChangedCallback(DnssdStateChangeCallback aCallback);

    //-----------------------------------------------------------------------------------------------------------------
    // `otPlatDnssd` APIs (see `openthread/include/openthread/platform/dnssd.h` for detailed documentation).

    typedef otPlatDnssdState           State;
    typedef otPlatDnssdService         Service;
    typedef otPlatDnssdHost            Host;
    typedef otPlatDnssdKey             Key;
    typedef otPlatDnssdRequestId       RequestId;
    typedef otPlatDnssdBrowser         Browser;
    typedef otPlatDnssdBrowseResult    BrowseResult;
    typedef otPlatDnssdSrvResolver     SrvResolver;
    typedef otPlatDnssdSrvResult       SrvResult;
    typedef otPlatDnssdTxtResolver     TxtResolver;
    typedef otPlatDnssdTxtResult       TxtResult;
    typedef otPlatDnssdAddressResolver AddressResolver;
    typedef otPlatDnssdAddressResult   AddressResult;
    typedef otPlatDnssdAddressAndTtl   AddressAndTtl;

    typedef std::function<void(otPlatDnssdRequestId, otError)> RegisterCallback;

    typedef enum
    {
        kOtCallback,
        kStdFunc,
    } CallbackType;

    template <typename DnssdResultType> class DnssdCallback
    {
    public:
        using ResultType = DnssdResultType;

        virtual void InvokeCallback(const DnssdResultType &aDnssdResult) = 0;

        virtual ~DnssdCallback() = default;

        bool operator==(const DnssdCallback &aOther) const
        {
            return (GetType() == aOther.GetType()) && IsEqualWhenSameType(aOther);
        }

    private:
        virtual CallbackType GetType(void) const                                                     = 0;
        virtual bool         IsEqualWhenSameType(const DnssdCallback<DnssdResultType> &aOther) const = 0;
    };

    typedef DnssdCallback<BrowseResult>  BrowseCallback;
    typedef DnssdCallback<SrvResult>     SrvCallback;
    typedef DnssdCallback<TxtResult>     TxtCallback;
    typedef DnssdCallback<AddressResult> AddressCallback;

    template <typename OtDnssdCallbackType, typename DnssdResultType>
    class OtDnssdCallback : public DnssdCallback<DnssdResultType>
    {
    public:
        OtDnssdCallback(otInstance *aInstance, OtDnssdCallbackType aCallback)
            : mInstance(aInstance)
            , mCallback(aCallback)
        {
        }

        void InvokeCallback(const DnssdResultType &aResult) override { mCallback(mInstance, &aResult); }

    private:
        CallbackType GetType(void) const override { return kOtCallback; }
        bool         IsEqualWhenSameType(const DnssdCallback<DnssdResultType> &aOther) const override
        {
            const OtDnssdCallback &other = static_cast<const OtDnssdCallback &>(aOther);
            return mCallback == other.mCallback;
        }

        otInstance         *mInstance;
        OtDnssdCallbackType mCallback;
    };

    typedef OtDnssdCallback<otPlatDnssdBrowseCallback, BrowseResult>   OtBrowseCallback;
    typedef OtDnssdCallback<otPlatDnssdSrvCallback, SrvResult>         OtSrvCallback;
    typedef OtDnssdCallback<otPlatDnssdTxtCallback, TxtResult>         OtTxtCallback;
    typedef OtDnssdCallback<otPlatDnssdAddressCallback, AddressResult> OtAddressCallback;

    template <typename StdDnssdCallbackType, typename DnssdResultType>
    class StdDnssdCallback : public DnssdCallback<DnssdResultType>
    {
    public:
        StdDnssdCallback(StdDnssdCallbackType aCallback, uint64_t aId)
            : mCallback(aCallback)
            , mId(aId)
        {
        }

        void InvokeCallback(const DnssdResultType &aResult) override { mCallback(aResult); }

    private:
        CallbackType GetType(void) const override { return kStdFunc; }
        bool         IsEqualWhenSameType(const DnssdCallback<DnssdResultType> &aOther) const override
        {
            const StdDnssdCallback &other = static_cast<const StdDnssdCallback &>(aOther);
            return this->mId == other.mId;
        }

        StdDnssdCallbackType mCallback;
        uint64_t             mId;
    };

    typedef StdDnssdCallback<std::function<void(const BrowseResult &)>, BrowseResult>   StdBrowseCallback;
    typedef StdDnssdCallback<std::function<void(const SrvResult &)>, SrvResult>         StdSrvCallback;
    typedef StdDnssdCallback<std::function<void(const TxtResult &)>, TxtResult>         StdTxtCallback;
    typedef StdDnssdCallback<std::function<void(const AddressResult &)>, AddressResult> StdAddressCallback;

    typedef std::unique_ptr<BrowseCallback>         BrowseCallbackPtr;
    typedef std::unique_ptr<SrvCallback>            SrvCallbackPtr;
    typedef std::unique_ptr<TxtCallback>            TxtCallbackPtr;
    typedef std::unique_ptr<AddressCallback>        AddressCallbackPtr;
    typedef std::pair<uint64_t, BrowseCallbackPtr>  BrowseEntry;
    typedef std::pair<uint64_t, SrvCallbackPtr>     SrvEntry;
    typedef std::pair<uint64_t, TxtCallbackPtr>     TxtEntry;
    typedef std::pair<uint64_t, AddressCallbackPtr> AddressEntry;

    State GetState(void) const { return mState; }
    void  RegisterService(const Service &aService, RequestId aRequestId, RegisterCallback aCallback);
    void  UnregisterService(const Service &aService, RequestId aRequestId, RegisterCallback aCallback);
    void  RegisterHost(const Host &aHost, RequestId aRequestId, RegisterCallback aCallback);
    void  UnregisterHost(const Host &aHost, RequestId aRequestId, RegisterCallback aCallback);
    void  RegisterKey(const Key &aKey, RequestId aRequestId, RegisterCallback aCallback);
    void  UnregisterKey(const Key &aKey, RequestId aRequestId, RegisterCallback aCallback);
    void  StartServiceBrowser(const Browser &aBrowser, BrowseCallbackPtr aCallbackPtr);
    void  StopServiceBrowser(const Browser &aBrowser, const BrowseCallback &aCallback);
    void  StartServiceResolver(const SrvResolver &aSrvResolver, SrvCallbackPtr aCallbackPtr);
    void  StopServiceResolver(const SrvResolver &aSrvResolver, const SrvCallback &aCallback);
    void  StartTxtResolver(const TxtResolver &aTxtResolver, TxtCallbackPtr aCallbackPtr);
    void  StopTxtResolver(const TxtResolver &aTxtResolver, const TxtCallback &aCallback);
    void  StartIp6AddressResolver(const AddressResolver &aAddressResolver, AddressCallbackPtr aCallbackPtr);
    void  StopIp6AddressResolver(const AddressResolver &aAddressResolver, const AddressCallback &aCallback);
    void  StartIp4AddressResolver(const AddressResolver &aAddressResolver, AddressCallbackPtr aCallbackPtr);
    void  StopIp4AddressResolver(const AddressResolver &aAddressResolver, const AddressCallback &aCallback);

    void SetInvokingCallbacks(bool aInvokingCallbacks) { mInvokingCallbacks = aInvokingCallbacks; }

private:
    static constexpr State kStateReady   = OT_PLAT_DNSSD_READY;
    static constexpr State kStateStopped = OT_PLAT_DNSSD_STOPPED;

    class DnsName
    {
    public:
        DnsName(std::string aName)
            : mName(std::move(aName))
        {
        }

        bool operator==(const DnsName &aOther) const
        {
            return StringUtils::ToLowercase(mName) == StringUtils::ToLowercase(aOther.mName);
        }

        bool operator<(const DnsName &aOther) const
        {
            return StringUtils::ToLowercase(mName) < StringUtils::ToLowercase(aOther.mName);
        }

    private:
        std::string mName;
    };

    class DnsServiceType
    {
    public:
        DnsServiceType(const char *aType, const char *aSubType)
            : mType(aType ? aType : "")
            , mSubType(aSubType ? aSubType : "")
        {
        }

        bool operator==(const DnsServiceType &aOther) const
        {
            return StringUtils::ToLowercase(ToString()) == StringUtils::ToLowercase(aOther.ToString());
        }

        bool operator<(const DnsServiceType &aOther) const
        {
            return StringUtils::ToLowercase(ToString()) < StringUtils::ToLowercase(aOther.ToString());
        }

        const std::string ToString(void) const;

    private:
        std::string mType;
        std::string mSubType;
    };

    class DnsServiceName
    {
    public:
        DnsServiceName(std::string aInstance, std::string aType)
            : mInstance(std::move(aInstance))
            , mType(std::move(aType))
        {
        }

        bool operator==(const DnsServiceName &aOther) const
        {
            return (mInstance == aOther.mInstance) && (mType == aOther.mType);
        }

        bool operator<(const DnsServiceName &aOther) const
        {
            return (mInstance == aOther.mInstance) ? (mType < aOther.mType) : (mInstance < aOther.mInstance);
        }

    private:
        DnsName mInstance;
        DnsName mType;
    };

    template <typename T> struct FirstFunctionArg;

    template <typename R, typename Arg, typename... Rest> struct FirstFunctionArg<std::function<R(Arg, Rest...)>>
    {
        using Type = Arg;
    };

    // RequestType MUST be a std::pair<uint64_t, std::unique_ptr<CallbackType>>
    template <typename RequestType> class EntryList
    {
    public:
        using CallbackType       = typename RequestType::second_type::element_type;
        using CallbackPtrType    = std::unique_ptr<CallbackType>;
        using CallbackResultType = typename CallbackType::ResultType;

        bool HasSubscribedOnInfraIf(uint64_t aInfraIfIndex)
        {
            return std::find_if(mEntries.begin(), mEntries.end(), [aInfraIfIndex](const RequestType &entry) {
                       return entry.first == aInfraIfIndex;
                   }) != mEntries.end();
        }

        void AddIfAbsent(uint64_t aInfraIfIndex, CallbackPtrType &&aCallbackPtr)
        {
            auto iter = FindEntry(aInfraIfIndex, *aCallbackPtr);
            if (iter == mEntries.end())
            {
                mEntries.emplace_back(aInfraIfIndex, std::move(aCallbackPtr));
            }
        }

        void MarkAsDeleted(uint64_t aInfraIfIndex, const CallbackType &aCallback)
        {
            auto iter = FindEntry(aInfraIfIndex, aCallback);
            if (iter != mEntries.end())
            {
                iter->second = nullptr;
            }
        }

        bool IsEmpty(void) { return mEntries.empty(); }

        bool HasAnyValidCallbacks(void)
        {
            auto iter = std::find_if(mEntries.begin(), mEntries.end(),
                                     [](const RequestType &entry) { return entry.second != nullptr; });
            return iter != mEntries.end();
        }

        void CleanUpDeletedEntries(void)
        {
            for (auto iter = mEntries.begin(); iter != mEntries.end();)
            {
                if (!iter->second)
                {
                    iter = mEntries.erase(iter);
                }
                else
                {
                    ++iter;
                }
            }
        }

        void InvokeAllCallbacks(uint64_t aInfraIfIndex, CallbackResultType &aResult)
        {
            for (auto &entry : mEntries)
            {
                if (entry.first == aInfraIfIndex && entry.second)
                {
                    entry.second->InvokeCallback(aResult);
                }
            }
        }

    private:
        using IteratorType = typename std::vector<RequestType>::iterator;

        IteratorType FindEntry(uint64_t aInfraIfIndex, const CallbackType &aCallback)
        {
            return std::find_if(mEntries.begin(), mEntries.end(),
                                [aInfraIfIndex, &aCallback](const RequestType &entry) {
                                    return entry.first == aInfraIfIndex && *entry.second == aCallback;
                                });
        }
        std::vector<RequestType> mEntries;
    };

    void HandleMdnsState(Mdns::Publisher::State aState) override;

    void                            UpdateState(void);
    Mdns::Publisher::ResultCallback MakePublisherCallback(RequestId aRequestId, RegisterCallback aCallback);

    static std::string KeyNameFor(const Key &aKey);

    static void HandleDiscoveredService(const std::string &aType, const Mdns::Publisher::DiscoveredInstanceInfo &aInfo);
    static void HandleDiscoveredHost(const std::string &aHostName, const Mdns::Publisher::DiscoveredHostInfo &aInfo);

    void ProcessServiceBrowsers(const std::string &aType, const Mdns::Publisher::DiscoveredInstanceInfo &aInfo);
    void ProcessServiceResolvers(const std::string &aType, const Mdns::Publisher::DiscoveredInstanceInfo &aInfo);
    void ProcessTxtResolvers(const std::string &aType, const Mdns::Publisher::DiscoveredInstanceInfo &aInfo);
    void ProcessAddrResolvers(const std::string                          &aHostName,
                              const Mdns::Publisher::DiscoveredHostInfo  &aInfo,
                              std::map<DnsName, EntryList<AddressEntry>> &aResolversMap);
    void ProcessIp6AddrResolvers(const std::string &aHostName, const Mdns::Publisher::DiscoveredHostInfo &aInfo);
    void ProcessIp4AddrResolvers(const std::string &aHostName, const Mdns::Publisher::DiscoveredHostInfo &aInfo);

    void StartAddressResolver(const AddressResolver                      &aAddressResolver,
                              AddressCallbackPtr                          aCallbackPtr,
                              std::map<DnsName, EntryList<AddressEntry>> &aResolversMap);
    void StopAddressResolver(const AddressResolver                      &aAddressResolver,
                             const AddressCallback                      &aCallback,
                             std::map<DnsName, EntryList<AddressEntry>> &aResolversMap);

    static DnssdPlatform *sDnssdPlatform;

    Mdns::Publisher                                 &mPublisher;
    State                                            mState;
    bool                                             mRunning;
    bool                                             mInvokingCallbacks;
    Mdns::Publisher::State                           mPublisherState;
    DnssdStateChangeCallback                         mStateChangeCallback;
    uint64_t                                         mSubscriberId;
    std::map<DnsServiceType, EntryList<BrowseEntry>> mServiceBrowsersMap;
    std::map<DnsServiceName, EntryList<SrvEntry>>    mServiceResolversMap;
    std::map<DnsServiceName, EntryList<TxtEntry>>    mTxtResolversMap;
    std::map<DnsName, EntryList<AddressEntry>>       mIp6AddrResolversMap;
    std::map<DnsName, EntryList<AddressEntry>>       mIp4AddrResolversMap;
};

} // namespace otbr

#endif // OTBR_ENABLE_DNSSD_PLAT

#endif // OTBR_AGENT_POSIX_DNSSD_HPP_
