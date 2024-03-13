/*
 *    Copyright (c) 2023, The OpenThread Authors.
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

#ifndef OTBR_AGENT_DNSSD_PLAT_HPP_
#define OTBR_AGENT_DNSSD_PLAT_HPP_

#include "openthread-br/config.h"

#include <algorithm>
#include <functional>
#include <map>

#include <openthread/instance.h>
#include <openthread/platform/dnssd.h>

#include "common/dns_utils.hpp"
#include "mdns/mdns.hpp"
#include "ncp/ncp_openthread.hpp"
#include "utils/string_utils.hpp"

namespace otbr {

/**
 * This class implements the DNS-SD platform.
 *
 */
class DnssdPlatform : private NonCopyable
{
public:
    /**
     * Initializes the `DnssdPlatform` instance
     *
     * @param[in] aNcp         The OT controller.
     * @param[in] aPublisher   A reference to `Mdns::Publisher` to use.
     *
     */
    DnssdPlatform(Ncp::ControllerOpenThread &aNcp, Mdns::Publisher &aPublisher);

    /**
     * Starts the `DnssdPlatform` module.
     *
     */
    void Start(void);

    /**
     * Stops the `DnssdPlatform` module
     *
     */
    void Stop(void);

    /**
     * Gets the singleton `DnssdPlatform` instance.
     *
     * @retval A reference to the `DnssdPlatform` instance.
     *
     */
    static DnssdPlatform &Get(void) { return *sDnssdPlatform; }

    //-----------------------------------------------------------------------------------------------------------------
    // `otPlatDnssd` APIs (see `openthread/include/openthread/platform/dnssd.h` for detailed documentation).

    typedef otPlatDnssdState            State;
    typedef otPlatDnssdServiceInstance  ServiceInstance;
    typedef otPlatDnssdService          Service;
    typedef otPlatDnssdHost             Host;
    typedef otPlatDnssdKey              Key;
    typedef otPlatDnssdRequestId        RequestId;
    typedef otPlatDnssdRegisterCallback RegisterCallback;
    typedef otPlatDnssdEvent            Event;

    State GetState(void) const { return mState; }
    void  RegisterService(const Service &aService, RequestId aRequestId, RegisterCallback aCallback);
    void  UnregisterService(const Service &aService, RequestId aRequestId, RegisterCallback aCallback);
    void  RegisterHost(const Host &aHost, RequestId aRequestId, RegisterCallback aCallback);
    void  UnregisterHost(const Host &aHost, RequestId aRequestId, RegisterCallback aCallback);
    void  RegisterKey(const Key &aKey, RequestId aRequestId, RegisterCallback aCallback);
    void  UnregisterKey(const Key &aKey, RequestId aRequestId, RegisterCallback aCallback);
    void  StartServiceBrowser(const char *aServiceType, uint32_t aInfraIfIndex);
    void  StopServiceBrowser(const char *aServiceType, uint32_t aInfraIfIndex);
    void  StartServiceResolver(const ServiceInstance &aInfo);
    void  StopServiceResolver(const ServiceInstance &aInfo);
    void  StartIp6AddressResolver(const char *aHostName, uint32_t aInfraIfIndex);
    void  StopIp6AddressResolver(const char *aHostName, uint32_t aInfraIfIndex);

    // Callback from BR agent
    void HandleMdnsPublisherStateChange(Mdns::Publisher::State aState);

private:
    static constexpr State    kStateReady        = OT_PLAT_DNSSD_READY;
    static constexpr State    kStateStopped      = OT_PLAT_DNSSD_STOPPED;
    static constexpr Event    kEventEntryAdded   = OT_PLAT_DNSSD_EVENT_ENTRY_ADDED;
    static constexpr Event    kEventEntryRemoved = OT_PLAT_DNSSD_EVENT_ENTRY_REMOVED;
    static constexpr uint32_t kAnyNetifIndex     = 0;

    class DnsName
    {
    public:
        DnsName(std::string aName)
            : mName(aName)
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
            : mInstance(aInstance)
            , mType(aType)
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

    class NetifIndexList
    {
    public:
        bool Matches(uint32_t aInterfaceIndex) const;
        bool IsEmpty(void) const { return mList.empty(); }
        bool Contains(uint32_t aInterfaceIndex) const;
        void Add(uint32_t aInterfaceIndex);
        void Remove(uint32_t aInterfaceIndex);

    private:
        std::vector<uint32_t> mList;
    };

    void                            UpdateState(void);
    Mdns::Publisher::ResultCallback MakePublisherCallback(RequestId aRequestId, RegisterCallback aCallback);

    static otError     ResultToError(otbrError aOtbrError);
    static std::string KeyNameFor(const Key &aKey);

    static void HandleDiscoveredService(const std::string &aType, const Mdns::Publisher::DiscoveredInstanceInfo &aInfo);
    static void HandleDiscoveredHost(const std::string &aHostName, const Mdns::Publisher::DiscoveredHostInfo &aInfo);

    void ProcessServiceBrowsers(const std::string &aType, const Mdns::Publisher::DiscoveredInstanceInfo &aInfo);
    void ProcessServiceResolvers(const std::string &aType, const Mdns::Publisher::DiscoveredInstanceInfo &aInfo);
    void ProcessIp6AddrResolvers(const std::string &aHostName, const Mdns::Publisher::DiscoveredHostInfo &aInfo);

    static DnssdPlatform *sDnssdPlatform;

    Ncp::ControllerOpenThread               &mNcp;
    Mdns::Publisher                         &mPublisher;
    State                                    mState;
    bool                                     mRunning;
    Mdns::Publisher::State                   mPublisherState;
    uint64_t                                 mSubscriberId;
    std::map<DnsName, NetifIndexList>        mServiceBrowsers;
    std::map<DnsServiceName, NetifIndexList> mServiceResolvers;
    std::map<DnsName, NetifIndexList>        mIp6AddrResolvers;
};

} // namespace otbr

#endif // OTBR_AGENT_DNSSD_PLAT_HPP_
