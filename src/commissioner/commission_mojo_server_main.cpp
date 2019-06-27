#include <memory>

#include <base/at_exit.h>
#include <base/bind.h>
#include <base/bind_helpers.h>
#include <base/command_line.h>
#include <base/logging.h>
#include <base/message_loop/message_loop.h>
#include <base/message_loop/message_pump_for_io.h>
#include <base/run_loop.h>

#include <chromecast/external_mojo/public/cpp/common.h>
#include <chromecast/external_mojo/public/cpp/external_connector.h>
#include <chromecast/external_mojo/public/cpp/external_service.h>

#include <mojo/core/embedder/embedder.h>
#include <mojo/core/embedder/scoped_ipc_support.h>

#include "commission_mojo_server.hpp"

struct GlobalState
{
    std::unique_ptr<chromecast::external_mojo::ExternalService>   service;
    std::unique_ptr<chromecast::external_mojo::ExternalConnector> connector;
    std::unique_ptr<ot::BorderRouter::CommissionMojoServer>       server;
};

void OnConnected(GlobalState *aState, std::unique_ptr<chromecast::external_mojo::ExternalConnector> aConnector)
{
    fprintf(stderr, "External mojo connected\n");
    aState->connector = std::move(aConnector);
    aState->service   = std::make_unique<chromecast::external_mojo::ExternalService>();
    aState->service->AddInterface(base::BindRepeating(&ot::BorderRouter::CommissionMojoServer::AddBinding,
                                                      base::Unretained(aState->server.get())));
    fprintf(stderr, "RegisterService done\n");
    aState->connector->RegisterService("otbr", aState->service.get());
}

int main(int argc, char *argv[])
{
    GlobalState state;
    state.server = std::make_unique<ot::BorderRouter::CommissionMojoServer>();

    base::CommandLine::Init(argc, argv);
    base::AtExitManager exitManager;

    base::MessageLoopForIO mainLoop;
    base::RunLoop          runLoop;

    mojo::core::Init();
    mojo::core::ScopedIPCSupport ipcSupport(mainLoop.task_runner(),
                                            mojo::core::ScopedIPCSupport::ShutdownPolicy::CLEAN);

    fprintf(stderr, "Start connect to external mojo\n");
    chromecast::external_mojo::ExternalConnector::Connect(chromecast::external_mojo::GetBrokerPath(),
                                                          base::BindOnce(OnConnected, &state));
    runLoop.Run();

    return 0;
}
