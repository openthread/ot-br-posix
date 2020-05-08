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

#include <unistd.h>

#include <base/at_exit.h>
#include <base/bind.h>
#include <base/bind_helpers.h>
#include <base/command_line.h>
#include <base/logging.h>
#include <mojo/core/embedder/embedder.h>
#include <mojo/core/embedder/scoped_ipc_support.h>

#include "common/code_utils.hpp"
#ifndef TEST_IN_CHROMIUM
#include "common/logging.hpp"
#include "mdns/mdns_mojo.hpp"
#else
#include "mdns_mojo.hpp"
#define otbrLog(level, format, ...)             \
    do                                          \
    {                                           \
        fprintf(stderr, format, ##__VA_ARGS__); \
        fprintf(stderr, "\r\n");                \
    } while (0)
#include "base/task/single_thread_task_executor.h"
#endif

namespace otbr {
namespace Mdns {

void MdnsMojoPublisher::LaunchMojoThreads(void)
{
    otbrLog(OTBR_LOG_INFO, "chromeTask");
    base::CommandLine::Init(0, NULL);
    base::AtExitManager exitManager;

#ifndef TEST_IN_CHROMIUM
    base::MessageLoopForIO mainLoop;
    base::RunLoop          runLoop;

    mojo::core::Init();
    mojo::core::ScopedIPCSupport ipcSupport(mainLoop.task_runner(),
                                            mojo::core::ScopedIPCSupport::ShutdownPolicy::CLEAN);

    mMojoTaskRunner = mainLoop.task_runner();
#else
    base::SingleThreadTaskExecutor ioTaskExecutor(base::MessagePumpType::IO);
    base::RunLoop                  runLoop;

    mojo::core::Init();
    mojo::core::ScopedIPCSupport ipcSupport(ioTaskExecutor.task_runner(),
                                            mojo::core::ScopedIPCSupport::ShutdownPolicy::CLEAN);

    mMojoTaskRunner = ioTaskExecutor.task_runner();
#endif // TEST_IN_CHROMIUM

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

#ifndef TEST_IN_CHROMIUM
    mResponder = nullptr;
#else
    mResponder.reset();
    mResponder = mojo::Remote<chromecast::mojom::MdnsResponder>();
#endif

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
#ifndef TEST_IN_CHROMIUM
        aConnector->set_connection_error_callback(
            base::BindOnce(&MdnsMojoPublisher::mMojoDisconnectedCb, base::Unretained(this)));
        aConnector->BindInterface("chromecast", &mResponder);
#else
        aConnector->SetConnectionErrorCallback(
            base::BindOnce(&MdnsMojoPublisher::mMojoDisconnectedCb, base::Unretained(this)));
        aConnector->BindInterface("chromecast", mResponder.BindNewPipeAndPassReceiver());
#endif
        mConnector = std::move(aConnector);
        mStateHandler(mContext, kStateReady);
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
        mStateHandler(mContext, kStateReady);
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
}

otbrError MdnsMojoPublisher::PublishService(uint16_t aPort, const char *aName, const char *aType, ...)
{
    otbrError                error = OTBR_ERROR_NONE;
    std::vector<std::string> text;
    va_list                  args;

    va_start(args, aType);

    VerifyOrExit(mConnector != nullptr, error = OTBR_ERROR_MDNS);
    for (const char *name = va_arg(args, const char *); name; name = va_arg(args, const char *))
    {
        const char *value = va_arg(args, const char *);

        text.emplace_back(std::string(name) + "=" + std::string(value));
    }
    mMojoTaskRunner->PostTask(FROM_HERE, base::BindOnce(&MdnsMojoPublisher::PublishServiceTask, base::Unretained(this),
                                                        aPort, aType, aName, text));

exit:
    va_end(args);
    return error;
}

void MdnsMojoPublisher::PublishServiceTask(uint16_t                        aPort,
                                           const std::string &             aType,
                                           const std::string &             aInstanceName,
                                           const std::vector<std::string> &aText)
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

    VerifyOrExit(!serviceName.empty() && !serviceProtocol.empty());

    mResponder->UnregisterServiceInstance(serviceName, aInstanceName, base::DoNothing());

    otbrLog(OTBR_LOG_INFO, "service name %s, protocol %s, instance %s", serviceName.c_str(), serviceProtocol.c_str(),
            aInstanceName.c_str());
    mResponder->RegisterServiceInstance(serviceName, serviceProtocol, aInstanceName, aPort, aText,
                                        base::BindOnce([](chromecast::mojom::MdnsResult r) {
                                            otbrLog(OTBR_LOG_INFO, "register result %d", static_cast<int32_t>(r));
                                        }));
    mPublishedServices.emplace_back(std::make_pair(serviceName, aInstanceName));

exit:
    return;
}

void MdnsMojoPublisher::UpdateFdSet(fd_set & aReadFdSet,
                                    fd_set & aWriteFdSet,
                                    fd_set & aErrorFdSet,
                                    int &    aMaxFd,
                                    timeval &aTimeout)
{
    (void)aReadFdSet;
    (void)aWriteFdSet;
    (void)aErrorFdSet;
    (void)aMaxFd;
    (void)aTimeout;
}

void MdnsMojoPublisher::Process(const fd_set &aReadFdSet, const fd_set &aWriteFdSet, const fd_set &aErrorFdSet)
{
    (void)aReadFdSet;
    (void)aWriteFdSet;
    (void)aErrorFdSet;
}

MdnsMojoPublisher::~MdnsMojoPublisher()
{
    mMojoTaskRunner->PostTask(FROM_HERE,
                              base::BindOnce(&MdnsMojoPublisher::TearDownMojoThreads, base::Unretained(this)));

    mMojoCoreThread->join();
}

Publisher *Publisher::Create(int aFamily, const char *aHost, const char *aDomain, StateHandler aHandler, void *aContext)
{
    (void)aFamily;
    (void)aHost;
    (void)aDomain;

    return new MdnsMojoPublisher(aHandler, aContext);
}

void Publisher::Destroy(Publisher *aPublisher)
{
    delete aPublisher;
}

} // namespace Mdns
} // namespace otbr
