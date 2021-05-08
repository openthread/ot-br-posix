/*
 *    Copyright (c) 2019, The OpenThread Authors.
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
 *   This file includes definition for MDNS service based on mojo.
 */

#ifndef OTBR_AGENT_MDNS_MOJO_HPP__
#define OTBR_AGENT_MDNS_MOJO_HPP__

#include <base/macros.h>
#include <base/message_loop/message_loop.h>
#include <base/message_loop/message_pump_for_io.h>
#include <chromecast/external_mojo/public/cpp/common.h>

#include <base/task_scheduler/post_task.h>
#include <chromecast/external_mojo/public/cpp/external_connector.h>

#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "chromecast/internal/receiver/mdns/public/mojom/mdns.mojom.h"
#include "common/task_runner.hpp"
#include "mdns/mdns.hpp"

#define MOJO_CONNECTOR_NS chromecast::external_mojo

namespace otbr {
namespace Mdns {

class MdnsMojoPublisher : public Publisher
{
public:
    /**
     * The constructor to MdnsMojoPublisher
     *
     * @param[in]   aHandler    The callback function for mojo connect state
     * change
     * @param[in]   aContext    The context for callback function
     *
     */
    MdnsMojoPublisher(StateHandler aHandler, void *aContext);

    /**
     * This method starts the MDNS service.
     *
     * @retval OTBR_ERROR_NONE  Successfully started MDNS service;
     * @retval OTBR_ERROR_MDNS  Failed to start MDNS service.
     *
     */
    otbrError Start(void) override;

    /**
     * This method stops the MDNS service.
     *
     */
    void Stop(void) override;

    /**
     * This method checks if publisher has been started.
     *
     * @retval true     Already started.
     * @retval false    Not started.
     *
     */
    bool IsStarted(void) const override;

    /**
     * This method publishes or updates a service.
     *
     * @param[in]   aHostName           The name of the host which this service resides on. If NULL is provided,
     *                                  this service resides on local host and it is the implementation to provide
     *                                  specific host name. Otherwise, the caller MUST publish the host with method
     *                                  PublishHost.
     * @param[in]   aName               The name of this service.
     * @param[in]   aType               The type of this service.
     * @param[in]   aPort               The port number of this service.
     * @param[in]   aTxtList            A list of TXT name/value pairs.
     *
     * @retval  OTBR_ERROR_NONE     Successfully published or updated the service.
     * @retval  OTBR_ERROR_ERRNO    Failed to publish or update the service.
     *
     */
    otbrError PublishService(const char *   aHostName,
                             uint16_t       aPort,
                             const char *   aName,
                             const char *   aType,
                             const TxtList &aTxtList) override;

    /**
     * This method un-publishes a service.
     *
     * @param[in]   aName               The name of this service.
     * @param[in]   aType               The type of this service.
     *
     * @retval  OTBR_ERROR_NONE     Successfully un-published the service.
     * @retval  OTBR_ERROR_ERRNO    Failed to un-publish the service.
     *
     */
    otbrError UnpublishService(const char *aName, const char *aType) override;

    /**
     * This method publishes or updates a host.
     *
     * Publishing a host is advertising an A/AAAA RR for the host name. This method should be called
     * before a service with non-null host name is published.
     *
     * @param[in]  aName           The name of the host.
     * @param[in]  aAddress        The address of the host.
     * @param[in]  aAddressLength  The length of @p aAddress.
     *
     * @retval  OTBR_ERROR_NONE          Successfully published or updated the host.
     * @retval  OTBR_ERROR_INVALID_ARGS  The arguments are not valid.
     * @retval  OTBR_ERROR_ERRNO         Failed to publish or update the host.
     *
     */
    otbrError PublishHost(const char *aName, const uint8_t *aAddress, uint8_t aAddressLength) override;

    /**
     * This method un-publishes a host.
     *
     * @param[in]  aName  A host name.
     *
     * @retval  OTBR_ERROR_NONE     Successfully un-published the host.
     * @retval  OTBR_ERROR_ERRNO    Failed to un-publish the host.
     *
     */
    otbrError UnpublishHost(const char *aName) override;

    /**
     * This method subscribes a given service or service instance. If @p aInstanceName is not empty, this method
     * subscribes the service instance. Otherwise, this method subscribes the service.
     *
     * mDNS implementations should use the `DiscoveredServiceInstanceCallback` function to notify discovered service
     * instances.
     *
     * @note Discovery Proxy implementation guarantees no duplicate subscriptions for the same service or service
     * instance.
     *
     * @param[in]  aType          The service type.
     * @param[in]  aInstanceName  The service instance to subscribe, or empty to subscribe the service.
     *
     */
    void SubscribeService(const std::string &aType, const std::string &aInstanceName) override;

    /**
     * This method unsubscribes a given service or service instance. If @p aInstanceName is not empty, this method
     * unsubscribes the service instance. Otherwise, this method unsubscribes the service.
     *
     * @note Discovery Proxy implementation guarantees no redundant unsubscription for a service or service instance.
     *
     * @param[in]  aType          The service type.
     * @param[in]  aInstanceName  The service instance to unsubscribe, or empty to unsubscribe the service.
     *
     */
    void UnsubscribeService(const std::string &aType, const std::string &aInstanceName) override;

    /**
     * This method subscribes a given host.
     *
     * mDNS implementations should use the `DiscoveredHostCallback` function to notify discovered hosts.
     *
     * @note Discovery Proxy implementation guarantees no duplicate subscriptions for the same host.
     *
     * @param[in]  aHostName    The host name (without domain).
     *
     */
    void SubscribeHost(const std::string &aHostName) override;

    /**
     * This method unsubscribes a given host.
     *
     * @note Discovery Proxy implementation guarantees no redundant unsubscription for a host.
     *
     * @param[in]  aHostName    The host name (without domain).
     *
     */
    void UnsubscribeHost(const std::string &aHostName) override;

    /**
     * This method updates the mainloop context.
     *
     * @param[inout]  aMainloop  A reference to the mainloop to be updated.
     *
     */
    void Update(MainloopContext &aMainloop) override;

    /**
     * This method processes mainloop events.
     *
     * @param[in]  aMainloop  A reference to the mainloop context.
     *
     */
    void Process(const MainloopContext &aMainloop) override;

    ~MdnsMojoPublisher(void) override;

private:
    static const int kMojoConnectRetrySeconds = 10;

    static std::pair<std::string, std::string> SplitServiceType(const std::string &aType);

    void PublishServiceTask(const std::string &             aHostInstanceName,
                            const std::string &             aName,
                            const std::string &             aTransport,
                            const std::string &             aInstanceName,
                            uint16_t                        aPort,
                            const std::vector<std::string> &aText);
    void UnpublishServiceTask(const std::string &aName, const std::string &aInstanceName);
    void PublishHostTask(const std::string &aInstanceName, const std::string &aIpv6Address);
    void UnpublishHostTask(const std::string &aInstanceName);

    bool VerifyFileAccess(const char *aFile);

    void StopPublishTask(void);

    void LaunchMojoThreads(void);
    void TearDownMojoThreads(void);
    void ConnectToMojo(void);
    void mMojoConnectCb(std::unique_ptr<chromecast::external_mojo::ExternalConnector> aConnector);
    void mMojoDisconnectedCb(void);
    void mRegisterServiceCb(chromecast::mojom::MdnsResult aResult);

    scoped_refptr<base::SingleThreadTaskRunner>                   mMojoTaskRunner;
    std::unique_ptr<std::thread>                                  mMojoCoreThread;
    base::Closure                                                 mMojoCoreThreadQuitClosure;
    std::unique_ptr<chromecast::external_mojo::ExternalConnector> mConnector;

    chromecast::mojom::MdnsResponderPtr mResponder;

    TaskRunner mMainloopTaskRunner;

    std::vector<std::pair<std::string, std::string>> mPublishedServices;
    std::vector<std::string>                         mPublishedHosts;

    StateHandler mStateHandler;
    void *       mContext;
    bool         mStarted;

    MdnsMojoPublisher(const MdnsMojoPublisher &) = delete;
    MdnsMojoPublisher &operator=(const MdnsMojoPublisher &) = delete;
};

} // namespace Mdns
} // namespace otbr

#endif // OTBR_AGENT_MDNS_MOJO_HPP__
