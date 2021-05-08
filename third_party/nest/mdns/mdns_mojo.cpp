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
 *   This file includes implementation for MDNS service based on mojo.
 */

#include <arpa/inet.h>
#include <unistd.h>

#include <base/at_exit.h>
#include <base/bind.h>
#include <base/bind_helpers.h>
#include <base/command_line.h>
#include <base/logging.h>
#include <mojo/core/embedder/embedder.h>
#include <mojo/core/embedder/scoped_ipc_support.h>

#include "mdns_mojo.hpp"
#include "common/code_utils.hpp"
#include "common/logging.hpp"

namespace otbr {
namespace Mdns {

static otbrError ConvertMdnsResultToOtbrError(chromecast::mojom::MdnsResult aResult)
{
    otbrError error;

    switch (aResult)
    {
    case chromecast::mojom::MdnsResult::SUCCESS:
        error = OTBR_ERROR_NONE;
        break;
    case chromecast::mojom::MdnsResult::NOT_FOUND:
        error = OTBR_ERROR_NOT_FOUND;
        break;
    case chromecast::mojom::MdnsResult::DUPLICATE_SERVICE:
    case chromecast::mojom::MdnsResult::DUPLICATE_HOST:
        error = OTBR_ERROR_DUPLICATED;
        break;
    case chromecast::mojom::MdnsResult::CANNOT_CREATE_RECORDS:
        error = OTBR_ERROR_MDNS;
        break;
    case chromecast::mojom::MdnsResult::INVALID_TEXT:
    case chromecast::mojom::MdnsResult::INVALID_PARAMS:
        error = OTBR_ERROR_INVALID_ARGS;
        break;
    case chromecast::mojom::MdnsResult::NOT_IMPLEMENTED:
        error = OTBR_ERROR_NOT_IMPLEMENTED;
        break;
    default:
        error = OTBR_ERROR_MDNS;
        break;
    }

    return error;
}

void MdnsMojoPublisher::LaunchMojoThreads(void)
{
    otbrLog(OTBR_LOG_INFO, "chromeTask");
    base::CommandLine::Init(0, NULL);
    base::AtExitManager exitManager;

    base::MessageLoopForIO mainLoop;
    base::RunLoop          runLoop;

    mojo::core::Init();
    mojo::core::ScopedIPCSupport ipcSupport(mainLoop.task_runner(),
                                            mojo::core::ScopedIPCSupport::ShutdownPolicy::CLEAN);

    mMojoTaskRunner = mainLoop.task_runner();

    if (!VerifyFileAccess(chromecast::external_mojo::GetBrokerPath().c_str()))
    {
        otbrLog(OTBR_LOG_WARNING, "Cannot access %s, will wait until file is ready",
                chromecast::external_mojo::GetBrokerPath().c_str());
    }

    mMojoTaskRunner->PostTask(FROM_HERE, base::BindOnce(&MdnsMojoPublisher::ConnectToMojo, base::Unretained(this)));

    mMojoCoreThreadQuitClosure = runLoop.QuitClosure();
    runLoop.Run();
}

void MdnsMojoPublisher::TearDownMojoThreads(void)
{
    mConnector      = nullptr;
    mMojoTaskRunner = nullptr;

    mResponder = nullptr;

    mMojoCoreThreadQuitClosure.Run();
}

MdnsMojoPublisher::MdnsMojoPublisher(StateHandler aHandler, void *aContext)
    : mConnector(nullptr)
    , mStateHandler(aHandler)
    , mContext(aContext)
    , mStarted(false)
{
}

void MdnsMojoPublisher::ConnectToMojo(void)
{
    otbrLog(OTBR_LOG_INFO, "Connecting to Mojo");

    if (!VerifyFileAccess(chromecast::external_mojo::GetBrokerPath().c_str()))
    {
        mMojoConnectCb(nullptr);
    }
    else
    {
        MOJO_CONNECTOR_NS::ExternalConnector::Connect(
            chromecast::external_mojo::GetBrokerPath(),
            base::BindOnce(&MdnsMojoPublisher::mMojoConnectCb, base::Unretained(this)));
    }
}

bool MdnsMojoPublisher::VerifyFileAccess(const char *aFile)
{
    return (access(aFile, R_OK) == 0) && (access(aFile, W_OK) == 0);
}

void MdnsMojoPublisher::mMojoConnectCb(std::unique_ptr<MOJO_CONNECTOR_NS::ExternalConnector> aConnector)
{
    if (aConnector)
    {
        otbrLog(OTBR_LOG_INFO, "Mojo connected");
        aConnector->set_connection_error_callback(
            base::BindOnce(&MdnsMojoPublisher::mMojoDisconnectedCb, base::Unretained(this)));
        aConnector->BindInterface("chromecast", &mResponder);
        mConnector = std::move(aConnector);
        mStateHandler(mContext, State::kReady);
    }
    else
    {
        mMojoTaskRunner->PostDelayedTask(FROM_HERE,
                                         base::BindOnce(&MdnsMojoPublisher::ConnectToMojo, base::Unretained(this)),
                                         base::TimeDelta::FromSeconds(kMojoConnectRetrySeconds));
    }
}

void MdnsMojoPublisher::mMojoDisconnectedCb(void)
{
    mConnector = nullptr;
}

otbrError MdnsMojoPublisher::Start(void)
{
    mStarted = true;
    if (mResponder)
    {
        mStateHandler(mContext, State::kReady);
    }
    else if (!mMojoCoreThread)
    {
        mMojoCoreThread = std::make_unique<std::thread>(&MdnsMojoPublisher::LaunchMojoThreads, this);
    }

    return OTBR_ERROR_NONE;
}

bool MdnsMojoPublisher::IsStarted(void) const
{
    return mStarted;
}

void MdnsMojoPublisher::Stop()
{
    if (mResponder)
    {
        mMojoTaskRunner->PostTask(FROM_HERE,
                                  base::BindOnce(&MdnsMojoPublisher::StopPublishTask, base::Unretained(this)));
    }
    mStarted = false;
}

void MdnsMojoPublisher::StopPublishTask(void)
{
    for (const auto &serviceInstancePair : mPublishedServices)
    {
        mResponder->UnregisterServiceInstance(serviceInstancePair.first, serviceInstancePair.second, base::DoNothing());
    }
    mPublishedServices.clear();

    for (const auto &host : mPublishedHosts)
    {
        mResponder->UnregisterHost(host, base::DoNothing());
    }
    mPublishedHosts.clear();
}

otbrError MdnsMojoPublisher::PublishService(const char *   aHostName,
                                            uint16_t       aPort,
                                            const char *   aName,
                                            const char *   aType,
                                            const TxtList &aTxtList)
{
    otbrError                error = OTBR_ERROR_NONE;
    std::vector<std::string> text;
    std::string              name;
    std::string              transport;
    std::string              hostName;
    std::string              instanceName = aName;

    if (aHostName != nullptr)
    {
        hostName = aHostName;
    }

    std::tie(name, transport) = SplitServiceType(aType);

    VerifyOrExit(mConnector != nullptr, error = OTBR_ERROR_MDNS);
    for (const auto &txtEntry : aTxtList)
    {
        const char *name        = txtEntry.mName;
        size_t      nameLength  = txtEntry.mNameLength;
        const char *value       = reinterpret_cast<const char *>(txtEntry.mValue);
        size_t      valueLength = txtEntry.mValueLength;

        text.emplace_back(std::string(name, nameLength) + "=" + std::string(value, value + valueLength));
    }
    mMojoTaskRunner->PostTask(FROM_HERE, base::BindOnce(&MdnsMojoPublisher::PublishServiceTask, base::Unretained(this),
                                                        hostName, name, transport, instanceName, aPort, text));

exit:
    return error;
}

void MdnsMojoPublisher::PublishServiceTask(const std::string &             aHostInstanceName,
                                           const std::string &             aName,
                                           const std::string &             aTransport,
                                           const std::string &             aInstanceName,
                                           uint16_t                        aPort,
                                           const std::vector<std::string> &aText)
{
    mResponder->UnregisterServiceInstance(aName, aInstanceName, base::DoNothing());

    otbrLog(OTBR_LOG_INFO, "register service: instance %s, name %s, protocol %s", aInstanceName.c_str(), aName.c_str(),
            aTransport.c_str());
    mResponder->RegisterServiceInstance(
        aHostInstanceName, aName, aTransport, aInstanceName, aPort, aText,
        base::BindOnce(
            [](MdnsMojoPublisher *aThis, const std::string &aName, const std::string &aTransport,
               const std::string &aInstanceName, chromecast::mojom::MdnsResult aResult) {
                otbrError error = ConvertMdnsResultToOtbrError(aResult);

                otbrLog(OTBR_LOG_INFO, "register service result: %d", static_cast<int32_t>(aResult));

                // TODO: Actually, we should call the handles after mDNS probing and announcing.
                // But this is not easy with current mojo mDNS APIs.
                aThis->mMainloopTaskRunner.Post([=]() {
                    if (error == OTBR_ERROR_NONE)
                    {
                        aThis->mPublishedServices.emplace_back(std::make_pair(aName, aInstanceName));
                    }

                    if (aThis->mHostHandler != nullptr)
                    {
                        aThis->mServiceHandler(aInstanceName.c_str(), (aName + "." + aTransport).c_str(), error,
                                               aThis->mHostHandlerContext);
                    }
                });
            },
            base::Unretained(this), aName, aTransport, aInstanceName));
}

otbrError MdnsMojoPublisher::UnpublishService(const char *aInstanceName, const char *aType)
{
    otbrError   error = OTBR_ERROR_NONE;
    std::string name  = SplitServiceType(aType).first;

    VerifyOrExit(mConnector != nullptr, error = OTBR_ERROR_MDNS);

    mMojoTaskRunner->PostTask(FROM_HERE, base::BindOnce(&MdnsMojoPublisher::UnpublishServiceTask,
                                                        base::Unretained(this), name, aInstanceName));

exit:
    return error;
}

void MdnsMojoPublisher::UnpublishServiceTask(const std::string &aName, const std::string &aInstanceName)
{
    otbrLog(OTBR_LOG_INFO, "unregister service name %s, instance %s", aName.c_str(), aInstanceName.c_str());

    mResponder->UnregisterServiceInstance(aName, aInstanceName, base::BindOnce([](chromecast::mojom::MdnsResult r) {
                                              otbrLog(OTBR_LOG_INFO, "unregister service result %d",
                                                      static_cast<int32_t>(r));
                                          }));

    for (auto it = mPublishedServices.begin(); it != mPublishedServices.end(); ++it)
    {
        if (it->first == aName && it->second == aInstanceName)
        {
            mPublishedServices.erase(it);
            break;
        }
    }
}

otbrError MdnsMojoPublisher::PublishHost(const char *aName, const uint8_t *aAddress, uint8_t aAddressLength)
{
    otbrError error = OTBR_ERROR_NONE;
    char      ipv6Address[INET6_ADDRSTRLEN];

    // Currently supports only IPv6 addresses for custom host.
    VerifyOrExit(aAddressLength == OTBR_IP6_ADDRESS_SIZE, error = OTBR_ERROR_INVALID_ARGS);
    VerifyOrExit(inet_ntop(AF_INET6, aAddress, ipv6Address, sizeof(ipv6Address)) != nullptr,
                 error = OTBR_ERROR_INVALID_ARGS);

    VerifyOrExit(mConnector != nullptr, error = OTBR_ERROR_MDNS);

    mMojoTaskRunner->PostTask(FROM_HERE, base::BindOnce(&MdnsMojoPublisher::PublishHostTask, base::Unretained(this),
                                                        std::string(aName), std::string(ipv6Address)));

exit:
    return error;
}

void MdnsMojoPublisher::PublishHostTask(const std::string &aInstanceName, const std::string &aIpv6Address)
{
    otbrLog(OTBR_LOG_INFO, "register host: name = %s, address = %s", aInstanceName.c_str(), aIpv6Address.c_str());

    mResponder->UnregisterHost(aInstanceName, base::BindOnce([=](chromecast::mojom::MdnsResult r) {
                                   otbrLog(OTBR_LOG_INFO, "unregister host result: %d", static_cast<int32_t>(r));
                               }));
    mResponder->RegisterHost(
        aInstanceName, {aIpv6Address},
        base::BindOnce(
            [](MdnsMojoPublisher *aThis, const std::string &aInstanceName, chromecast::mojom::MdnsResult aResult) {
                otbrError error = ConvertMdnsResultToOtbrError(aResult);

                otbrLog(OTBR_LOG_INFO, "register host result: %d", static_cast<int32_t>(aResult));

                // TODO: Actually, we should call the handles after mDNS probing and announcing.
                // But this is not easy with current mojo mDNS APIs.
                aThis->mMainloopTaskRunner.Post([=]() {
                    if (error == OTBR_ERROR_NONE)
                    {
                        aThis->mPublishedHosts.emplace_back(aInstanceName);
                    }

                    if (aThis->mHostHandler != nullptr)
                    {
                        aThis->mHostHandler(aInstanceName.c_str(), error, aThis->mHostHandlerContext);
                    }
                });
            },
            base::Unretained(this), aInstanceName));
}

otbrError MdnsMojoPublisher::UnpublishHost(const char *aName)
{
    otbrError error = OTBR_ERROR_NONE;

    VerifyOrExit(mConnector != nullptr, error = OTBR_ERROR_MDNS);

    mMojoTaskRunner->PostTask(FROM_HERE,
                              base::BindOnce(&MdnsMojoPublisher::UnpublishHostTask, base::Unretained(this), aName));

exit:
    return error;
}

void MdnsMojoPublisher::UnpublishHostTask(const std::string &aInstanceName)
{
    otbrLog(OTBR_LOG_INFO, "unregister host: name = %s", aInstanceName.c_str());

    mResponder->UnregisterHost(aInstanceName, base::BindOnce([=](chromecast::mojom::MdnsResult r) {
                                   otbrLog(OTBR_LOG_INFO, "unregister host result: %d", static_cast<int32_t>(r));
                               }));

    for (auto it = mPublishedHosts.begin(); it != mPublishedHosts.end(); ++it)
    {
        if (*it == aInstanceName)
        {
            mPublishedHosts.erase(it);
            break;
        }
    }
}

void MdnsMojoPublisher::SubscribeService(const std::string &aType, const std::string &aInstanceName)
{
    otbrLog(OTBR_LOG_WARNING, "SubscribeService is not implemented");
}

void MdnsMojoPublisher::UnsubscribeService(const std::string &aType, const std::string &aInstanceName)
{
    otbrLog(OTBR_LOG_WARNING, "UnsubscribeService is not implemented");
}

void MdnsMojoPublisher::SubscribeHost(const std::string &aHostName)
{
    otbrLog(OTBR_LOG_WARNING, "SubscribeHost is not implemented");
}

void MdnsMojoPublisher::UnsubscribeHost(const std::string &aHostName)
{
    otbrLog(OTBR_LOG_WARNING, "UnsubscribeHost is not implemented");
}

void MdnsMojoPublisher::Update(MainloopContext &aMainloop)
{
    mMainloopTaskRunner.Update(aMainloop);
}

void MdnsMojoPublisher::Process(const MainloopContext &aMainloop)
{
    mMainloopTaskRunner.Process(aMainloop);
}

MdnsMojoPublisher::~MdnsMojoPublisher()
{
    if (mMojoCoreThread)
    {
        mMojoTaskRunner->PostTask(FROM_HERE,
                                  base::BindOnce(&MdnsMojoPublisher::TearDownMojoThreads, base::Unretained(this)));
        mMojoCoreThread->join();
    }
}

std::pair<std::string, std::string> MdnsMojoPublisher::SplitServiceType(const std::string &aType)
{
    std::string            serviceName;
    std::string            serviceProtocol;
    std::string::size_type split = aType.rfind('.');

    // Remove the last trailing dot since the cast mdns will add one
    if (split + 1 == aType.length())
    {
        split = aType.rfind('.', split - 1);
    }

    VerifyOrExit(split != std::string::npos);

    serviceName     = aType.substr(0, split);
    serviceProtocol = aType.substr(split + 1, std::string::npos);

    // Remove the last trailing dot since the cast mdns will add one
    if (serviceProtocol.back() == '.')
    {
        serviceProtocol.erase(serviceProtocol.size() - 1);
    }

exit:
    return {serviceName, serviceProtocol};
}

Publisher *Publisher::Create(int aFamily, const char *aDomain, StateHandler aHandler, void *aContext)
{
    (void)aFamily;
    (void)aDomain;

    return new MdnsMojoPublisher(aHandler, aContext);
}

void Publisher::Destroy(Publisher *aPublisher)
{
    delete aPublisher;
}

} // namespace Mdns
} // namespace otbr
