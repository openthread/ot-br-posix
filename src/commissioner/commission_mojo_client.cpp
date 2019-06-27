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
#include <mojo/public/cpp/bindings/binding_set.h>

#include "src/commissioner/mojom/thread_commissioner.mojom.h"

struct GlobalState
{
    std::unique_ptr<chromecast::external_mojo::ExternalService>   service;
    std::unique_ptr<chromecast::external_mojo::ExternalConnector> connector;
    chromecast::mojom::CommissionerPtr                            commissioner;
    base::Closure                                                 quitClosure;
};

void SetJoinerCallback(GlobalState *aState, uint32_t aJoinerId, chromecast::mojom::CommissionResult aResult)
{
    printf("joiner %u: result %d\n", aJoinerId, static_cast<int>(aResult));
    aState->quitClosure.Run();
}

void PetitionCallback(GlobalState *aState, chromecast::mojom::CommissionResult aResult)
{
    if (aResult == chromecast::mojom::CommissionResult::SUCCESS)
    {
        aState->commissioner->SetJoiner(0, "ABCDEF", true, "", base::BindOnce(SetJoinerCallback, aState));
    }
    else
    {
        printf("Petition failed: %d\n", static_cast<int>(aResult));
        aState->quitClosure.Run();
    }
}

void OnConnected(GlobalState *aState, std::unique_ptr<chromecast::external_mojo::ExternalConnector> aConnector)
{
    printf("connected to external mojo\n");
    aState->connector = std::move(aConnector);
    aState->connector->BindInterface("otbr", &(aState->commissioner));
    printf("call petition\n");
    aState->commissioner->Petition("OpenThread", "dead00beef00cafe", "123456",
                                   base::BindOnce(PetitionCallback, aState));
}

int main(int argc, char *argv[])
{
    GlobalState state;

    base::CommandLine::Init(0, NULL);
    base::AtExitManager exitManager;

    base::MessageLoopForIO mainLoop;
    base::RunLoop          runLoop;

    mojo::core::Init();
    mojo::core::ScopedIPCSupport ipcSupport(mainLoop.task_runner(),
                                            mojo::core::ScopedIPCSupport::ShutdownPolicy::CLEAN);

    state.quitClosure = runLoop.QuitClosure();

    chromecast::external_mojo::ExternalConnector::Connect(chromecast::external_mojo::GetBrokerPath(),
                                                          base::BindOnce(OnConnected, &state));

    runLoop.Run();

    return 0;
}
