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

#define OTBR_LOG_TAG "MDNS"

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
#include "common/types.hpp"

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
    otbrLogInfo("chromeTask");
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
        otbrLogWarning("Cannot access %s, will wait until file is ready",
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

    mServiceListener     = nullptr;
    mServiceListenerImpl = nullptr;
    mRecordListener      = nullptr;
    mRecordListenerImpl  = nullptr;

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
    otbrLogInfo("Connecting to Mojo");

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
        otbrLogInfo("Mojo connected");
        mPublishedServices.clear();
        mPublishedHosts.clear();
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
    otbrLogInfo("Disconnected, will reconnect.");
    mConnector = nullptr;
    mMojoTaskRunner->PostDelayedTask(FROM_HERE,
                                     base::BindOnce(&MdnsMojoPublisher::ConnectToMojo, base::Unretained(this)),
                                     base::TimeDelta::FromSeconds(kMojoConnectRetrySeconds));
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

otbrError MdnsMojoPublisher::PublishService(const char *       aHostName,
                                            uint16_t           aPort,
                                            const char *       aName,
                                            const char *       aType,
                                            const SubTypeList &aSubTypeList,
                                            const TxtList &    aTxtList)
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
        const char *value       = reinterpret_cast<const char *>(txtEntry.mValue.data());
        size_t      valueLength = txtEntry.mValue.size();

        text.emplace_back(txtEntry.mName + "=" + std::string(value, value + valueLength));
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

    otbrLogInfo("register service: instance %s, name %s, protocol %s", aInstanceName.c_str(), aName.c_str(),
                aTransport.c_str());
    mResponder->RegisterServiceInstance(
        aHostInstanceName, aName, aTransport, aInstanceName, aPort, aText,
        base::BindOnce(
            [](MdnsMojoPublisher *aThis, const std::string &aName, const std::string &aTransport,
               const std::string &aInstanceName, chromecast::mojom::MdnsResult aResult) {
                otbrError error = ConvertMdnsResultToOtbrError(aResult);

                otbrLogInfo("register service result: %d", static_cast<int32_t>(aResult));

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
                                                        base::Unretained(this), name, std::string(aInstanceName)));

exit:
    return error;
}

void MdnsMojoPublisher::UnpublishServiceTask(const std::string &aName, const std::string &aInstanceName)
{
    otbrLogInfo("unregister service name %s, instance %s", aName.c_str(), aInstanceName.c_str());

    mResponder->UnregisterServiceInstance(aName, aInstanceName, base::BindOnce([](chromecast::mojom::MdnsResult r) {
                                              otbrLogInfo("unregister service result %d", static_cast<int32_t>(r));
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
    otbrLogInfo("register host: name = %s, address = %s", aInstanceName.c_str(), aIpv6Address.c_str());

    mResponder->UnregisterHost(aInstanceName, base::BindOnce([=](chromecast::mojom::MdnsResult r) {
                                   otbrLogInfo("unregister host result: %d", static_cast<int32_t>(r));
                               }));
    mResponder->RegisterHost(
        aInstanceName, {aIpv6Address},
        base::BindOnce(
            [](MdnsMojoPublisher *aThis, const std::string &aInstanceName, chromecast::mojom::MdnsResult aResult) {
                otbrError error = ConvertMdnsResultToOtbrError(aResult);

                otbrLogInfo("register host result: %d", static_cast<int32_t>(aResult));

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
                              base::BindOnce(&MdnsMojoPublisher::UnpublishHostTask,
                                             base::Unretained(this), std::string(aName)));

exit:
    return error;
}

void MdnsMojoPublisher::UnpublishHostTask(const std::string &aInstanceName)
{
    otbrLogInfo("unregister host: name = %s", aInstanceName.c_str());

    mResponder->UnregisterHost(aInstanceName, base::BindOnce([=](chromecast::mojom::MdnsResult r) {
                                   otbrLogInfo("unregister host result: %d", static_cast<int32_t>(r));
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
    OTBR_UNUSED_VARIABLE(aInstanceName);

    std::string name, transport;

    std::tie(name, transport) = SplitServiceType(aType);
    mMojoTaskRunner->PostTask(
        FROM_HERE, base::BindOnce(&MdnsMojoPublisher::SubscribeServiceTask, base::Unretained(this), name, transport));
}

void MdnsMojoPublisher::SubscribeServiceTask(const std::string &aService, const std::string &aTransport)
{
    otbrLogInfo("[MdnsMojo] subscribe service %s.%s", aService.c_str(), aTransport.c_str());

    if (mServiceListenerImpl == nullptr)
    {
        auto serviceListenerRequest = mojo::MakeRequest(&mServiceListener);

        mResponder->AddListenerObserver(std::move(mServiceListener));
        mServiceListenerImpl =
            std::make_unique<MdnsDiscoveredServiceListenerImpl>(this, std::move(serviceListenerRequest));

        otbrLogInfo("[MdnsMojo] service listener observer added once");
    }

    mResponder->StartServiceListener(aService, aTransport);
}

void MdnsMojoPublisher::UnsubscribeService(const std::string &aType, const std::string &aInstanceName)
{
    OTBR_UNUSED_VARIABLE(aInstanceName);

    std::string name, transport;

    std::tie(name, transport) = SplitServiceType(aType);
    mMojoTaskRunner->PostTask(
        FROM_HERE, base::BindOnce(&MdnsMojoPublisher::UnsubscribeServiceTask, base::Unretained(this), name, transport));
}

void MdnsMojoPublisher::UnsubscribeServiceTask(const std::string &aService, const std::string &aTransport)
{
    otbrLogInfo("[MdnsMojo] unsubscribe service %s.%s", aService.c_str(), aTransport.c_str());

    mResponder->StopServiceListener(aService, aTransport);
}

void MdnsMojoPublisher::SubscribeHost(const std::string &aHostName)
{
    mMojoTaskRunner->PostTask(FROM_HERE,
                              base::BindOnce(&MdnsMojoPublisher::SubscribeHostTask, base::Unretained(this), aHostName));
}

void MdnsMojoPublisher::SubscribeHostTask(const std::string &aHostName)
{
    std::string fullHostName = aHostName + ".local";
    otbrLogInfo("[MdnsMojo] subscribe host %s", fullHostName.c_str());

    if (mRecordListenerImpl == nullptr)
    {
        auto recordListenerRequest = mojo::MakeRequest(&mRecordListener);

        mResponder->AddRecordListenerObserver(std::move(mRecordListener));
        mRecordListenerImpl =
            std::make_unique<MdnsDiscoveredRecordListenerImpl>(this, std::move(recordListenerRequest));

        otbrLogInfo("[MdnsMojo] record listener observer added once");
    }

    mResponder->StartRecordListener(fullHostName, kResourceRecordTypeAaaa);
    mResponder->StartRecordListener(fullHostName, kResourceRecordTypeA);
}

void MdnsMojoPublisher::UnsubscribeHost(const std::string &aHostName)
{
    mMojoTaskRunner->PostTask(
        FROM_HERE, base::BindOnce(&MdnsMojoPublisher::UnsubscribeHostTask, base::Unretained(this), aHostName));
}

void MdnsMojoPublisher::UnsubscribeHostTask(const std::string aHostName)
{
    std::string fullHostName = aHostName + ".local";
    otbrLogInfo("[MdnsMojo] unsubscribe host %s", fullHostName.c_str());

    mResponder->StopRecordListener(fullHostName, kResourceRecordTypeA);
    mResponder->StopRecordListener(fullHostName, kResourceRecordTypeAaaa);
}

void MdnsMojoPublisher::NotifyDiscoveredServiceInstance(const std::string &                          aInstanceName,
                                                        const std::string &                          aServiceName,
                                                        const std::string &                          aTransport,
                                                        chromecast::mojom::MdnsDiscoveredInstancePtr aInfo)
{
    std::vector<uint8_t> &    addressBytes = aInfo->address->address->address_bytes;
    std::vector<std::string> &txtVector    = aInfo->text;

    Publisher::DiscoveredInstanceInfo instanceInfo;

    instanceInfo.mName     = aInstanceName;
    instanceInfo.mHostName = NormalizeDomain(aInfo->host_name);

    if (addressBytes.size() == sizeof(Ip6Address))
    {
        instanceInfo.mAddresses.push_back(Ip6Address(*reinterpret_cast<const uint8_t(*)[16]>(&addressBytes[0])));
    }

    instanceInfo.mPort     = aInfo->address->port;
    instanceInfo.mPriority = aInfo->priority;
    instanceInfo.mWeight   = aInfo->weight;
    instanceInfo.mTxtData  = EncodeTxtRdata(txtVector);
    instanceInfo.mTtl      = kMojoServiceInstanceDefaultTtl;

    mMainloopTaskRunner.Post([=](void) {
        if (mDiscoveredServiceInstanceCallback != nullptr)
        {
            mDiscoveredServiceInstanceCallback(aServiceName + "." + aTransport, instanceInfo);
        }
    });
}

void MdnsMojoPublisher::NotifyDiscoveredRecord(chromecast::mojom::MdnsDiscoveredRecordPtr aInfo)
{
    otbrLogInfo("[MdnsMojo] record discovered, name:%s type:%d len:%d", aInfo->name.c_str(), aInfo->type,
                aInfo->rdata.size());

    switch (aInfo->type)
    {
    case kResourceRecordTypeAaaa:
    {
        std::string                   hostName            = NormalizeDomain(aInfo->name);
        std::string                   hostNameStripDomain = StripLocalDomain(hostName);
        Ip6Address &                  address             = *reinterpret_cast<Ip6Address *>(&aInfo->rdata[0]);
        Publisher::DiscoveredHostInfo hostInfo;

        VerifyOrExit(aInfo->rdata.size() == sizeof(Ip6Address));

        otbrLogInfo("[MdnsMojo] Host %s AAAA RR found: %s = %s", hostNameStripDomain.c_str(), hostName.c_str(),
                    address.ToString().c_str());

        hostInfo.mHostName = hostName;
        hostInfo.mAddresses.push_back(address);
        hostInfo.mTtl = aInfo->ttl;

        mMainloopTaskRunner.Post([=](void) {
            if (mDiscoveredHostCallback != nullptr)
            {
                mDiscoveredHostCallback(hostNameStripDomain, hostInfo);
            }
        });
    }

    break;
    default:
        break;
    }

exit:
    return;
}

std::vector<uint8_t> MdnsMojoPublisher::EncodeTxtRdata(const std::vector<std::string> &aTxtVector)
{
    std::vector<uint8_t> txtData;

    if (aTxtVector.empty())
    {
        txtData.emplace_back('\0');
    }
    else
    {
        for (const std::string &txtKv : aTxtVector)
        {
            txtData.emplace_back(static_cast<uint8_t>(txtKv.size()));
            txtData.insert(txtData.end(), txtKv.begin(), txtKv.end());
        }
    }

    return txtData;
}

std::string MdnsMojoPublisher::StripLocalDomain(const std::string &aName)
{
    size_t      dotpos   = aName.find_last_of('.');
    std::string stripped = aName;

    VerifyOrExit(dotpos != std::string::npos);

    // Ignore the dot if it's at the end.
    if (dotpos == aName.size() - 1)
    {
        dotpos = aName.find_last_of('.', dotpos - 1);
        VerifyOrExit(dotpos != std::string::npos);
        VerifyOrExit(aName.substr(dotpos + 1, aName.size() - 2 - dotpos) == "local");
    }
    else
    {
        VerifyOrExit(aName.substr(dotpos + 1) == "local");
    }

    stripped = aName.substr(0, dotpos);

exit:
    return stripped;
}

std::string MdnsMojoPublisher::NormalizeDomain(const std::string &aName)
{
    std::string normalizedName = aName;

    if (normalizedName.empty() || normalizedName.back() != '.')
    {
        normalizedName.append(".");
    }

    return normalizedName;
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

void MdnsMojoPublisher::MdnsDiscoveredServiceListenerImpl::OnServiceDiscovered(
    const std::string &                          aInstanceName,
    const std::string &                          aServiceName,
    const std::string &                          aTransport,
    chromecast::mojom::MdnsDiscoveredInstancePtr aInfo)
{
    otbrLogInfo("[MdnsMojo] Service is updated: %s.%s.%s host %s", aInstanceName.c_str(), aServiceName.c_str(),
                aTransport.c_str(), aInfo->host_name.c_str());

    mOwner->NotifyDiscoveredServiceInstance(aInstanceName, aServiceName, aTransport, std::move(aInfo));
}

void MdnsMojoPublisher::MdnsDiscoveredServiceListenerImpl::OnServiceUpdated(
    const std::string &                          aInstanceName,
    const std::string &                          aServiceName,
    const std::string &                          aTransport,
    chromecast::mojom::MdnsDiscoveredInstancePtr aInfo)
{
    otbrLogInfo("[MdnsMojo] Service is updated: %s.%s.%s host %s", aInstanceName.c_str(), aServiceName.c_str(),
                aTransport.c_str(), aInfo->host_name.c_str());

    mOwner->NotifyDiscoveredServiceInstance(aInstanceName, aServiceName, aTransport, std::move(aInfo));
}

void MdnsMojoPublisher::MdnsDiscoveredServiceListenerImpl::OnServiceRemoved(const std::string &aInstanceName,
                                                                            const std::string &aServiceName,
                                                                            const std::string &aTransport)
{
    otbrLogInfo("[MdnsMojo] Service is removed: %s.%s.%s", aInstanceName.c_str(), aServiceName.c_str(),
                aTransport.c_str());
}

void MdnsMojoPublisher::MdnsDiscoveredRecordListenerImpl::OnRecordDiscovered(
    chromecast::mojom::MdnsDiscoveredRecordPtr aInfo)
{
    otbrLogInfo("[MdnsMojo] Record is discovered, name:%s type:%d len:%dB", aInfo->name.c_str(), aInfo->type,
                aInfo->rdata.size());

    mOwner->NotifyDiscoveredRecord(std::move(aInfo));
}

void MdnsMojoPublisher::MdnsDiscoveredRecordListenerImpl::OnRecordUpdated(
    chromecast::mojom::MdnsDiscoveredRecordPtr aInfo)
{
    otbrLogInfo("[MdnsMojo] Record is updated, name:%s type:%d len:%dB", aInfo->name.c_str(), aInfo->type,
                aInfo->rdata.size());

    mOwner->NotifyDiscoveredRecord(std::move(aInfo));
}

void MdnsMojoPublisher::MdnsDiscoveredRecordListenerImpl::OnRecordRemoved(const std::string &aName, uint16_t aType)
{
    otbrLogInfo("[MdnsMojo] Record is removed, name:%s type:%d", aName.c_str(), aType);
}

} // namespace Mdns
} // namespace otbr
