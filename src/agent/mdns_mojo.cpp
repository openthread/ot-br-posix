#include <base/at_exit.h>
#include <base/bind.h>
#include <base/bind_helpers.h>
#include <base/command_line.h>
#include <base/logging.h>
#include <base/task_scheduler/post_task.h>
#include <mojo/core/embedder/embedder.h>
#include <mojo/core/embedder/scoped_ipc_support.h>

#include <functional>

#include "agent/mdns_mojo.hpp"
#include "common/code_utils.hpp"
#include "common/logging.hpp"

namespace ot {
namespace BorderRouter {
namespace Mdns {

void MdnsMojoPublisher::LaunchMojoThreads()
{
    otbrLog(OTBR_LOG_INFO, "chromeTask");
    base::CommandLine::Init(0, NULL);
    base::AtExitManager exit_manager;

    base::MessageLoopForIO main_loop;
    base::RunLoop          run_loop;

    mojo::core::Init();
    mojo::core::ScopedIPCSupport ipc_support(main_loop.task_runner(),
                                             mojo::core::ScopedIPCSupport::ShutdownPolicy::CLEAN);

    mMojoTaskRunner = main_loop.task_runner();
    mMojoTaskRunner->PostTask(FROM_HERE, base::BindOnce(&MdnsMojoPublisher::ConnectToMojo, base::Unretained(this)));
    run_loop.Run();
}

MdnsMojoPublisher::MdnsMojoPublisher(StateHandler aHandler, void *aContext)
    : mConnector(nullptr)
    , mStateHandler(aHandler)
    , mContext(aContext)
    , mStarted(false)
{
    mLaunchThread = std::make_unique<std::thread>(&MdnsMojoPublisher::LaunchMojoThreads, this);
}

void MdnsMojoPublisher::ConnectToMojo()
{
    otbrLog(OTBR_LOG_INFO, "Connecting to Mojo");
    chromecast::external_mojo::ExternalConnector::Connect(
        chromecast::external_mojo::GetBrokerPath(),
        base::BindOnce(&MdnsMojoPublisher::mMojoConnectCb, base::Unretained(this)));
}

void MdnsMojoPublisher::mMojoConnectCb(std::unique_ptr<chromecast::external_mojo::ExternalConnector> aConnector)
{
    if (aConnector)
    {
        otbrLog(OTBR_LOG_INFO, "Mojo connected");
        aConnector->set_connection_error_callback(
            base::BindOnce(&MdnsMojoPublisher::mMojoDisconnectedCb, base::Unretained(this)));
        aConnector->BindInterface("chromecast", &mResponder);
        mConnector = std::move(aConnector);
        Start();
    }
    else
    {
        mMojoTaskRunner->PostTask(FROM_HERE, base::BindOnce(&MdnsMojoPublisher::ConnectToMojo, base::Unretained(this)));
    }
}

void MdnsMojoPublisher::mMojoDisconnectedCb()
{
    mConnector = nullptr;
}

otbrError MdnsMojoPublisher::Start()
{
    otbrError err = OTBR_ERROR_NONE;

    VerifyOrExit(mConnector != nullptr, err = OTBR_ERROR_MDNS);

    mStarted = true;
    mStateHandler(mContext, kStateReady);
exit:
    return err;
}

bool MdnsMojoPublisher::IsStarted() const
{
    return mStarted;
}

void MdnsMojoPublisher::Stop()
{
    mMojoTaskRunner->PostTask(FROM_HERE, base::BindOnce(&MdnsMojoPublisher::StopPublishTask, base::Unretained(this)));
}

void MdnsMojoPublisher::StopPublishTask(void)
{
    if (!mLastServiceName.empty())
    {
        mResponder->UnregisterServiceInstance(mLastServiceName, mLastInstanceName, base::DoNothing());
    }
    mLastServiceName.clear();
    mLastInstanceName.clear();
}

otbrError MdnsMojoPublisher::PublishService(uint16_t aPort, const char *aName, const char *aType, ...)
{
    otbrError                err = OTBR_ERROR_NONE;
    std::vector<std::string> text;
    va_list                  args;

    va_start(args, aType);

    VerifyOrExit(mConnector != nullptr, err = OTBR_ERROR_MDNS);
    for (const char *name = va_arg(args, const char *); name; name = va_arg(args, const char *))
    {
        const char *value = va_arg(args, const char *);

        text.emplace_back(std::string(name) + "=" + std::string(value));
    }
    mMojoTaskRunner->PostTask(FROM_HERE, base::BindOnce(&MdnsMojoPublisher::PublishServiceTask, base::Unretained(this),
                                                        aPort, aType, aName, text));

exit:
    va_end(args);
    return err;
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

    VerifyOrExit(!serviceName.empty() && !serviceProtocol.empty());

    // Remove the last trailing dot since the cast mdns will add one
    if (serviceProtocol.back() == '.')
    {
        serviceProtocol.erase(serviceProtocol.size() - 1);
    }

    mResponder->UnregisterServiceInstance(serviceName, aInstanceName, base::DoNothing());
    if (!mLastServiceName.empty())
    {
        mResponder->UnregisterServiceInstance(mLastServiceName, mLastInstanceName, base::DoNothing());
    }

    otbrLog(OTBR_LOG_INFO, "service name %s, protocol %s, instance %s", serviceName.c_str(), serviceProtocol.c_str(),
            aInstanceName.c_str());
    mResponder->RegisterServiceInstance(serviceName, serviceProtocol, aInstanceName, aPort, aText,
                                        base::BindOnce([](chromecast::mojom::MdnsResult r) {
                                            otbrLog(OTBR_LOG_INFO, "register result %d", static_cast<int32_t>(r));
                                        }));
    mLastServiceName  = serviceName;
    mLastInstanceName = aInstanceName;

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
} // namespace BorderRouter
} // namespace ot
