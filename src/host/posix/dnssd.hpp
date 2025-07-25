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

#include <functional>
#include <map>
#include <string>

#include <openthread/instance.h>
#include <openthread/platform/dnssd.h>

#include "common/code_utils.hpp"
#include "common/dns_utils.hpp"
#include "host/thread_host.hpp"
#include "mdns/mdns.hpp"
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
    typedef std::function<void(BrowseResult &)>                BrowseCallback;
    typedef std::function<void(SrvResult &)>                   SrvCallback;
    typedef std::function<void(TxtResult &)>                   TxtCallback;
    typedef std::function<void(AddressResult &)>               AddressCallback;

    State GetState(void) const { return mState; }
    void  RegisterService(const Service &aService, RequestId aRequestId, RegisterCallback aCallback);
    void  UnregisterService(const Service &aService, RequestId aRequestId, RegisterCallback aCallback);
    void  RegisterHost(const Host &aHost, RequestId aRequestId, RegisterCallback aCallback);
    void  UnregisterHost(const Host &aHost, RequestId aRequestId, RegisterCallback aCallback);
    void  RegisterKey(const Key &aKey, RequestId aRequestId, RegisterCallback aCallback);
    void  UnregisterKey(const Key &aKey, RequestId aRequestId, RegisterCallback aCallback);
    void  StartServiceBrowser(const Browser &aBrowser, const BrowseCallback &aCallback);
    void  StopServiceBrowser(const Browser &aBrowser);
    void  StartServiceResolver(const SrvResolver &aSrvResolver, const SrvCallback &aCallback);
    void  StopServiceResolver(const SrvResolver &aSrvResolver);
    void  StartTxtResolver(const TxtResolver &aTxtResolver, const TxtCallback &aCallback);
    void  StopTxtResolver(const TxtResolver &aTxtResolver);
    void  StartIp6AddressResolver(const AddressResolver &aAddressResolver, const AddressCallback &aCallback);
    void  StopIp6AddressResolver(const AddressResolver &aAddressResolver);
    void  StartIp4AddressResolver(const AddressResolver &aAddressResolver, const AddressCallback &aCallback);
    void  StopIp4AddressResolver(const AddressResolver &aAddressResolver);

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

    template <typename CallbackType> class CallbackTable
    {
    public:
        void Add(uint64_t aInfraIfIndex, CallbackType aCallback)
        {
            mCallbackTable[aInfraIfIndex].push_back(std::move(aCallback));
        }

        void Remove(uint64_t aInfraIfIndex) { mCallbackTable.erase(aInfraIfIndex); }

        bool Matches(uint64_t aInfraIfIndex) { return mCallbackTable.find(aInfraIfIndex) != mCallbackTable.end(); }

        bool IsEmpty(void) { return mCallbackTable.empty(); }

        const std::vector<CallbackType> &GetCallbacks(uint64_t aInfraIfIndex)
        {
            static std::vector<CallbackType> sEmptyResult;

            auto *ptr  = &sEmptyResult;
            auto  iter = mCallbackTable.find(aInfraIfIndex);

            if (iter != mCallbackTable.end())
            {
                ptr = &iter->second;
            }

            return *ptr;
        }

    private:
        std::map<uint64_t, std::vector<CallbackType>> mCallbackTable;
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
    void ProcessAddrResolvers(const std::string                                 &aHostName,
                              const Mdns::Publisher::DiscoveredHostInfo         &aInfo,
                              std::map<DnsName, CallbackTable<AddressCallback>> &aResolversTable);
    void ProcessIp6AddrResolvers(const std::string &aHostName, const Mdns::Publisher::DiscoveredHostInfo &aInfo);
    void ProcessIp4AddrResolvers(const std::string &aHostName, const Mdns::Publisher::DiscoveredHostInfo &aInfo);

    void StartAddressResolver(const AddressResolver                             &aAddressResolver,
                              const AddressCallback                             &aCallback,
                              std::map<DnsName, CallbackTable<AddressCallback>> &aResolversTable);
    void StopAddressResolver(const AddressResolver                             &aAddressResolver,
                             std::map<DnsName, CallbackTable<AddressCallback>> &aResolversTable);

    static DnssdPlatform *sDnssdPlatform;

    Mdns::Publisher                                     &mPublisher;
    State                                                mState;
    bool                                                 mRunning;
    Mdns::Publisher::State                               mPublisherState;
    DnssdStateChangeCallback                             mStateChangeCallback;
    uint64_t                                             mSubscriberId;
    std::map<DnsName, CallbackTable<BrowseCallback>>     mServiceBrowsersTable;
    std::map<DnsServiceName, CallbackTable<SrvCallback>> mServiceResolversTable;
    std::map<DnsServiceName, CallbackTable<TxtCallback>> mTxtResolversTable;
    std::map<DnsName, CallbackTable<AddressCallback>>    mIp6AddrResolversTable;
    std::map<DnsName, CallbackTable<AddressCallback>>    mIp4AddrResolversTable;
};

} // namespace otbr

#endif // OTBR_ENABLE_DNSSD_PLAT

#endif // OTBR_AGENT_POSIX_DNSSD_HPP_
