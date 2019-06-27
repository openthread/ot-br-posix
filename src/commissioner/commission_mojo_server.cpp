#include "commission_mojo_server.hpp"

#include "common/code_utils.hpp"
#include "common/logging.hpp"
#include "utils/hex.hpp"

namespace ot {
namespace BorderRouter {

const char CommissionMojoServer::kBorderAgentPort[] = "49191";
const char CommissionMojoServer::kLocalHostIP[]     = "127.0.0.1";

void CommissionMojoServer::AddBinding(chromecast::mojom::CommissionerRequest aRequest)
{
    mBindings.AddBinding(this, std::move(aRequest));
}

void CommissionMojoServer::Petition(const std::string &aNetworkname,
                                    const std::string &aExtPanId,
                                    const std::string &aNetworkPassword,
                                    PetitionCallback   aCallback)
{
    ot::Psk::Pskc                       pskc;
    chromecast::mojom::CommissionResult res = chromecast::mojom::CommissionResult::SUCCESS;
    uint8_t                             xPanId[kXPanIdLength];
    int                                 ret = 0;

    if (mCommissioner)
    {
        otbrLog(OTBR_LOG_INFO, "stop commissiner thread\n");
        StopCommissionerThread();
    }

    VerifyOrExit(sizeof(xPanId) == Utils::Hex2Bytes(aExtPanId.c_str(), xPanId, sizeof(xPanId)),
                 res = chromecast::mojom::CommissionResult::INVALID_PARAMS);

    mCommissioner = std::make_unique<ot::BorderRouter::Commissioner>(
        pskc.ComputePskc(xPanId, aNetworkname.c_str(), aNetworkPassword.c_str()), kDefaultKeepAliveRate);
    VerifyOrExit(mCommissioner != nullptr, res = chromecast::mojom::CommissionResult::COMMISSIONER_FAIL);
    mCommissioner->InitDtls(kLocalHostIP, kBorderAgentPort);

    otbrLog(OTBR_LOG_INFO, "try dtls handshake\n");
    do
    {
        ret = mCommissioner->TryDtlsHandshake();
    } while (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE);

    if (mCommissioner->IsValid())
    {
        otbrLog(OTBR_LOG_INFO, "start petition\n");
        mCommissioner->CommissionerPetition();
        otbrLog(OTBR_LOG_INFO, "launch commissioner thread\n");
        LaunchCommissionerThread();
        WaitForCommissionerComplete();
    }
    else
    {
        res = chromecast::mojom::CommissionResult::COMMISSIONER_FAIL;
    }

exit:
    std::move(aCallback).Run(res);
}

void CommissionMojoServer::SetJoiner(uint32_t                           aJoinerId,
                                     const std::string &                aJoinerPskd,
                                     bool                               aAllowAll,
                                     const base::Optional<std::string> &aJoinerEui64,
                                     SetJoinerCallback                  aCallback)
{
    chromecast::mojom::CommissionResult res = chromecast::mojom::CommissionResult::SUCCESS;
    ot::SteeringData                    steeringData;

    {
        std::lock_guard<std::mutex> l(mCommissionerMtx);
        VerifyOrExit(mCommissioner->IsValid() && mCommissioner->IsCommissionerAccepted(),
                     res = chromecast::mojom::CommissionResult::COMMISSIONER_FAIL);
    }

    if (aAllowAll)
    {
        steeringData.Init(1); // minimal length
        steeringData.Set();
    }
    else
    {
        uint8_t joinerEui64[kEui64Len];

        steeringData.Init(kSteeringDefaultLength);
        VerifyOrExit(aJoinerEui64.has_value(), res = chromecast::mojom::CommissionResult::INVALID_PARAMS);
        VerifyOrExit(sizeof(joinerEui64) ==
                         Utils::Hex2Bytes(aJoinerEui64.value().c_str(), joinerEui64, sizeof(joinerEui64)),
                     res = chromecast::mojom::CommissionResult::INVALID_PARAMS);
        steeringData.ComputeJoinerId(joinerEui64, joinerEui64);
        steeringData.ComputeBloomFilter(joinerEui64);
    }
    mCommissioner->SetJoiner(aJoinerPskd.c_str(), steeringData);

exit:
    std::move(aCallback).Run(aJoinerId, res);
}

void CommissionMojoServer::LaunchCommissionerThread(void)
{
    mCommisserThreadExitFlag.store(false);
    mCommissionerThread = std::thread(&CommissionMojoServer::CommissionerWorker, this);
}

void CommissionMojoServer::StopCommissionerThread(void)
{
    mCommisserThreadExitFlag.store(true);
    mCommissionerThread.join();
}

void CommissionMojoServer::WaitForCommissionerComplete(void)
{
    // TODO: add timeout
    std::unique_lock<std::mutex> l(mCommissionerMtx);
    mPetitionCv.wait(l, [this]() { return mCommissioner->IsCommissionerAccepted(); });
}

void CommissionMojoServer::CommissionerWorker(void)
{
    otbrLog(OTBR_LOG_DEBUG, "Start commissioner worker\n");
    bool valid = true;
    while (valid && !mCommisserThreadExitFlag.load())
    {
        int            maxFd   = -1;
        struct timeval timeout = {10, 0};
        int            rval;

        fd_set readFdSet;
        fd_set writeFdSet;
        fd_set errorFdSet;

        FD_ZERO(&readFdSet);
        FD_ZERO(&writeFdSet);
        FD_ZERO(&errorFdSet);
        {
            std::lock_guard<std::mutex> l(mCommissionerMtx);
            mCommissioner->UpdateFdSet(readFdSet, writeFdSet, errorFdSet, maxFd, timeout);
        }

        rval = select(maxFd + 1, &readFdSet, &writeFdSet, &errorFdSet, &timeout);
        if (rval < 0)
        {
            break;
        }

        {
            std::unique_lock<std::mutex> l(mCommissionerMtx);
            mCommissioner->Process(readFdSet, writeFdSet, errorFdSet);
            if (mCommissioner->IsCommissionerAccepted())
            {
                mPetitionCv.notify_one();
            }

            valid = mCommissioner->IsValid();
        }
    }
}

} // namespace BorderRouter
} // namespace ot
