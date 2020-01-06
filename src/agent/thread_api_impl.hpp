#ifndef OTBR_THREAD_API_IMPL_HPP_
#define OTBR_THREAD_API_IMPL_HPP_

#include <chrono>
#include <functional>
#include <list>
#include <map>
#include <memory>

#include <openthread/instance.h>
#include <openthread/ip6.h>
#include <openthread/jam_detection.h>
#include <openthread/joiner.h>
#include <openthread/netdata.h>
#include <openthread/thread.h>

#include "agent/ncp_openthread.hpp"
#include "agent/thread_api.hpp"

namespace otbr {
namespace agent {

class ThreadApiImpl : public ThreadApi
{
public:
    ThreadApiImpl(otInstance *aInstance);

    otError Scan(const ScanHandler &aHandler) override;

private:
    static void sActiveScanHandler(otActiveScanResult *aResult, void *aContext);
    void        ActiveScanHandler(otActiveScanResult *aResult);

    otInstance *mInstance;

    std::unique_ptr<ScanHandler>    mScanHandler;
    std::vector<otActiveScanResult> mScanResults;
};

} // namespace agent
} // namespace otbr

#endif // OTBR_THREAD_API_IMPL_HPP_
