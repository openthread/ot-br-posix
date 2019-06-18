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
 *   This file includes stub service for MDNS client based on mojo.
 */

#include <base/at_exit.h>
#include <base/bind.h>
#include <base/bind_helpers.h>
#include <base/command_line.h>
#include <base/logging.h>
#include <base/message_loop/message_loop.h>
#include <base/message_loop/message_pump_for_io.h>
#include <base/run_loop.h>
#include <chromecast/external_mojo/external_service_support/external_connector.h>
#include <chromecast/external_mojo/external_service_support/external_service.h>
#include <chromecast/external_mojo/public/cpp/common.h>
#include <mojo/core/embedder/embedder.h>
#include <mojo/core/embedder/scoped_ipc_support.h>
#include <mojo/public/cpp/bindings/binding_set.h>

#include <utility>

#include "mdns_mojo_test/public/mojom/mdns.mojom.h"

class StubMdnsMojomResponder;

struct GlobalState
{
    std::unique_ptr<chromecast::external_service_support::ExternalConnector> connector;
    std::unique_ptr<chromecast::external_service_support::ExternalService>   service;
    std::unique_ptr<StubMdnsMojomResponder>                                  responder;
};

class StubMdnsMojomResponder : public chromecast::mojom::MdnsResponder
{
public:
    StubMdnsMojomResponder() {}

    void AddBinding(chromecast::mojom::MdnsResponderRequest aRequest)
    {
        mBindings.AddBinding(this, std::move(aRequest));
    }

    void RegisterServiceInstance(const std::string &                             aServiceName,
                                 const std::string &                             aServiceTransport,
                                 const std::string &                             aInstanceName,
                                 int32_t                                         aPort,
                                 const base::Optional<std::vector<std::string>> &aText,
                                 RegisterServiceInstanceCallback                 aCallback) override
    {
        (void)aServiceName;
        (void)aServiceTransport;
        (void)aInstanceName;
        (void)aPort;
        (void)aCallback;

        std::move(aCallback).Run(chromecast::mojom::MdnsResult::SUCCESS);
    }

    void RegisterDynamicServiceInstance(const std::string &                               aServiceName,
                                        const std::string &                               aInstanceName,
                                        chromecast::mojom::MdnsDynamicServiceResponderPtr aResponder,
                                        chromecast::mojom::MdnsPublicationPtr             aInitializer) override
    {
        (void)aServiceName;
        (void)aInstanceName;
        (void)aResponder;
        (void)aInitializer;
    }

    void UnregisterServiceInstance(const std::string &               aServiceName,
                                   const std::string &               aInstanceName,
                                   UnregisterServiceInstanceCallback aCallback) override
    {
        (void)aServiceName;
        (void)aInstanceName;

        std::move(aCallback).Run(chromecast::mojom::MdnsResult::SUCCESS);
    }

    void ReannounceInstance(const std::string &aServiceName, const std::string &aInstanceName) override
    {
        (void)aServiceName;
        (void)aInstanceName;
    }

    void UpdateTxtRecord(const std::string &             aServiceName,
                         const std::string &             aInstanceName,
                         const std::vector<std::string> &aText,
                         UpdateTxtRecordCallback         aCallback) override
    {
        (void)aServiceName;
        (void)aInstanceName;
        (void)aText;

        std::move(aCallback).Run(chromecast::mojom::MdnsResult::SUCCESS);
    }

    void UpdateSrvRecord(const std::string &     aServiceName,
                         const std::string &     aInstanceName,
                         int32_t                 aPort,
                         int32_t                 aPriority,
                         int32_t                 aWeight,
                         UpdateSrvRecordCallback aCallback) override
    {
        (void)aServiceName;
        (void)aInstanceName;
        (void)aPort;
        (void)aPriority;
        (void)aWeight;

        std::move(aCallback).Run(chromecast::mojom::MdnsResult::SUCCESS);
    }

    void UpdateSubtypes(const std::string &             aServiceName,
                        const std::string &             aInstanceName,
                        const std::vector<std::string> &aFixedSubtypes,
                        UpdateSubtypesCallback          aCallback) override
    {
        (void)aServiceName;
        (void)aInstanceName;
        (void)aFixedSubtypes;

        std::move(aCallback).Run(chromecast::mojom::MdnsResult::SUCCESS);
    }

    void ClearPublicationCache(const std::string &aServiceName,
                               const std::string &aInstanceName,
                               const std::string &aSubType) override
    {
        (void)aServiceName;
        (void)aInstanceName;
        (void)aSubType;
    }

private:
    mojo::BindingSet<chromecast::mojom::MdnsResponder> mBindings;

    DISALLOW_COPY_AND_ASSIGN(StubMdnsMojomResponder);
};

void OnConnected(GlobalState *                                                            aState,
                 std::unique_ptr<chromecast::external_service_support::ExternalConnector> aConnector)
{
    aState->connector = std::move(aConnector);
    aState->service   = std::make_unique<chromecast::external_service_support::ExternalService>();
    aState->responder = std::make_unique<StubMdnsMojomResponder>();
    aState->service->AddInterface(
        base::BindRepeating(&StubMdnsMojomResponder::AddBinding, base::Unretained(aState->responder.get())));
    aState->connector->RegisterService("chromecast", aState->service.get());
}

int main()
{
    GlobalState state;

    base::CommandLine::Init(0, NULL);
    base::AtExitManager exitManager;

    base::MessageLoopForIO mainLoop;
    base::RunLoop          runLoop;

    mojo::core::Init();
    mojo::core::ScopedIPCSupport ipcSupport(mainLoop.task_runner(),
                                            mojo::core::ScopedIPCSupport::ShutdownPolicy::CLEAN);

    chromecast::external_service_support::ExternalConnector::Connect(chromecast::external_mojo::GetBrokerPath(),
                                                                     base::BindOnce(OnConnected, &state));
    runLoop.Run();

    return 0;
}
