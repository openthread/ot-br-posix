#ifndef OTBR_THREAD_API_HPP_
#define OTBR_THREAD_API_HPP_

#include <functional>
#include <memory>
#include <vector>

#include <openthread/instance.h>
#include <openthread/ip6.h>
#include <openthread/jam_detection.h>
#include <openthread/joiner.h>
#include <openthread/netdata.h>
#include <openthread/thread.h>

namespace otbr {
namespace agent {

class ThreadApi
{
public:
    using ScanHandler = std::function<void(const std::vector<otActiveScanResult> &)>;

    virtual otError Scan(const ScanHandler &aHandler) = 0;

    virtual ~ThreadApi() {}
};

} // namespace agent
} // namespace otbr
#endif // OTBR_THREAD_API_HPP_
