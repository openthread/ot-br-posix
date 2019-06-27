#include <memory>
#include <unordered_map>

#include <atomic>
#include <mutex>
#include <thread>

#include <mojo/public/cpp/bindings/binding_set.h>

#include "commissioner.hpp"
#include "src/commissioner/mojom/thread_commissioner.mojom.h"

namespace ot {
namespace BorderRouter {

class CommissionMojoServer : public chromecast::mojom::Commissioner
{
public:
    CommissionMojoServer() = default;
    CommissionMojoServer(const CommissionMojoServer &) = delete;
    CommissionMojoServer &operator=(const CommissionMojoServer &) = delete;

    void AddBinding(chromecast::mojom::CommissionerRequest aRequest);

    void Petition(const std::string &aNetworkname,
                  const std::string &aExtPanId,
                  const std::string &aNetworkPassword,
                  PetitionCallback   aCallback) override;

    void SetJoiner(uint32_t                           aJoinerId,
                   const std::string &                aJoinerPskd,
                   bool                               aAllowAll,
                   const base::Optional<std::string> &aJoinerEui64,
                   SetJoinerCallback                  aCallback) override;


private:
    void WaitForCommissionerComplete(void);

    void LaunchCommissionerThread(void);

    void StopCommissionerThread(void);

    void CommissionerWorker(void);

    std::unique_ptr<ot::BorderRouter::Commissioner> mCommissioner;
    std::condition_variable                         mPetitionCv;
    std::mutex                                      mCommissionerMtx;
    std::atomic_bool                                mCommisserThreadExitFlag;
    std::thread                                     mCommissionerThread;

    mojo::BindingSet<chromecast::mojom::Commissioner> mBindings;

    static const int      kDefaultKeepAliveRate = 15;
    static const char kBorderAgentPort[];
    static const char kLocalHostIP[];
};

} // namespace BorderRouter
} // namespace ot
