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
 *   This file includes definition for OTBR Agent.
 */

#ifndef OTBR_AGENT_APPLICATION_HPP_
#define OTBR_AGENT_APPLICATION_HPP_

#include "openthread-br/config.h"

#include <atomic>
#include <signal.h>
#include <stdint.h>
#include <vector>

#if OTBR_ENABLE_BORDER_AGENT
#include "border_agent/border_agent.hpp"
#endif
#include "ncp/rcp_host.hpp"
#if OTBR_ENABLE_BACKBONE_ROUTER
#include "backbone_router/backbone_agent.hpp"
#endif
#if OTBR_ENABLE_REST_SERVER
#include "rest/rest_web_server.hpp"
#endif
#if OTBR_ENABLE_DBUS_SERVER
#include "dbus/server/dbus_agent.hpp"
#endif
#if OTBR_ENABLE_OPENWRT
#include "openwrt/ubus/otubus.hpp"
#endif
#if OTBR_ENABLE_VENDOR_SERVER
#include "agent/vendor.hpp"
#endif
#include "utils/infra_link_selector.hpp"

namespace otbr {

#if OTBR_ENABLE_VENDOR_SERVER
namespace vendor {

class VendorServer;

}
#endif

/**
 * @addtogroup border-router-agent
 *
 * @brief
 *   This module includes definition for OTBR application.
 *
 * @{
 */

/**
 * This class implements OTBR application management.
 */
class Application : private NonCopyable
{
public:
    typedef std::function<otbrError(void)> ErrorCondition;

    /**
     * This constructor initializes the Application instance.
     *
     * @param[in] aHost                  A reference to the ThreadHost object.
     * @param[in] aInterfaceName         Name of the Thread network interface.
     * @param[in] aBackboneInterfaceName Name of the backbone network interface.
     * @param[in] aRestListenAddress     Network address to listen on.
     * @param[in] aRestListenPort        Network port to listen on.
     */
    explicit Application(Ncp::ThreadHost   &aHost,
                         const std::string &aInterfaceName,
                         const std::string &aBackboneInterfaceName,
                         const std::string &aRestListenAddress,
                         int                aRestListenPort);

    /**
     * This method initializes the Application instance.
     */
    void Init(void);

    /**
     * This method de-initializes the Application instance.
     */
    void Deinit(void);

    /**
     * This method sets an error condition for the application.
     *
     * If the error condition returns an error other than 'OTBR_ERROR_NONE', the application will
     * exit the loop in `Run`.
     *
     * @param[in] aErrorCondition  The error condition.
     */
    void SetErrorCondition(ErrorCondition aErrorCondition) { mErrorCondition = aErrorCondition; }

    /**
     * This method runs the application until exit.
     *
     * @retval OTBR_ERROR_NONE  The application exited without any error.
     * @retval OTBR_ERROR_ERRNO The application exited with some system error.
     */
    otbrError Run(void);

    /**
     * Get the OpenThread controller object the application is using.
     *
     * @returns The OpenThread controller object.
     */
    Ncp::ThreadHost &GetHost(void) { return mHost; }

#if OTBR_ENABLE_MDNS
    /**
     * Get the Publisher object the application is using.
     *
     * @returns The Publisher object.
     */
    Mdns::Publisher &GetPublisher(void)
    {
        return *mPublisher;
    }
#endif

#if OTBR_ENABLE_BORDER_AGENT
    /**
     * Get the border agent the application is using.
     *
     * @returns The border agent.
     */
    BorderAgent &GetBorderAgent(void)
    {
        return *mBorderAgent;
    }
#endif

#if OTBR_ENABLE_BACKBONE_ROUTER
    /**
     * Get the backbone agent the application is using.
     *
     * @returns The backbone agent.
     */
    BackboneRouter::BackboneAgent &GetBackboneAgent(void)
    {
        return *mBackboneAgent;
    }
#endif

#if OTBR_ENABLE_SRP_ADVERTISING_PROXY
    /**
     * Get the advertising proxy the application is using.
     *
     * @returns The advertising proxy.
     */
    AdvertisingProxy &GetAdvertisingProxy(void)
    {
        return *mAdvertisingProxy;
    }
#endif

#if OTBR_ENABLE_DNSSD_DISCOVERY_PROXY
    /**
     * Get the discovery proxy the application is using.
     *
     * @returns The discovery proxy.
     */
    Dnssd::DiscoveryProxy &GetDiscoveryProxy(void)
    {
        return *mDiscoveryProxy;
    }
#endif

#if OTBR_ENABLE_TREL
    /**
     * Get the TrelDnssd object the application is using.
     *
     * @returns The TrelDnssd.
     */
    TrelDnssd::TrelDnssd &GetTrelDnssd(void)
    {
        return *mTrelDnssd;
    }
#endif

#if OTBR_ENABLE_OPENWRT
    /**
     * Get the UBus agent the application is using.
     *
     * @returns The UBus agent.
     */
    ubus::UBusAgent &GetUBusAgent(void)
    {
        return *mUbusAgent;
    }
#endif

#if OTBR_ENABLE_REST_SERVER
    /**
     * Get the rest web server the application is using.
     *
     * @returns The rest web server.
     */
    rest::RestWebServer &GetRestWebServer(void)
    {
        return *mRestWebServer;
    }
#endif

#if OTBR_ENABLE_DBUS_SERVER
    /**
     * Get the DBus agent the application is using.
     *
     * @returns The DBus agent.
     */
    DBus::DBusAgent &GetDBusAgent(void)
    {
        return *mDBusAgent;
    }
#endif

private:
    // Default poll timeout.
    static const struct timeval kPollTimeout;

    static void HandleSignal(int aSignal);

    void CreateRcpMode(const std::string &aRestListenAddress, int aRestListenPort);
    void InitRcpMode(void);
    void DeinitRcpMode(void);

    void InitNcpMode(void);
    void DeinitNcpMode(void);

    std::string      mInterfaceName;
    const char      *mBackboneInterfaceName;
    Ncp::ThreadHost &mHost;
#if OTBR_ENABLE_MDNS
    Mdns::StateSubject               mMdnsStateSubject;
    std::unique_ptr<Mdns::Publisher> mPublisher;
#endif
#if OTBR_ENABLE_BORDER_AGENT
    std::unique_ptr<BorderAgent> mBorderAgent;
#endif
#if OTBR_ENABLE_BACKBONE_ROUTER
    std::unique_ptr<BackboneRouter::BackboneAgent> mBackboneAgent;
#endif
#if OTBR_ENABLE_SRP_ADVERTISING_PROXY
    std::unique_ptr<AdvertisingProxy> mAdvertisingProxy;
#endif
#if OTBR_ENABLE_DNSSD_DISCOVERY_PROXY
    std::unique_ptr<Dnssd::DiscoveryProxy> mDiscoveryProxy;
#endif
#if OTBR_ENABLE_TREL
    std::unique_ptr<TrelDnssd::TrelDnssd> mTrelDnssd;
#endif
#if OTBR_ENABLE_OPENWRT
    std::unique_ptr<ubus::UBusAgent> mUbusAgent;
#endif
#if OTBR_ENABLE_REST_SERVER
    std::unique_ptr<rest::RestWebServer> mRestWebServer;
#endif
#if OTBR_ENABLE_DBUS_SERVER
    std::unique_ptr<DBus::DBusAgent> mDBusAgent;
#endif
#if OTBR_ENABLE_VENDOR_SERVER
    std::shared_ptr<vendor::VendorServer> mVendorServer;
#endif

    static std::atomic_bool sShouldTerminate;
    ErrorCondition          mErrorCondition;
};

/**
 * @}
 */

} // namespace otbr

#endif // OTBR_AGENT_APPLICATION_HPP_
