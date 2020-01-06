#include <assert.h>
#include <string.h>

#include "openthread/border_router.h"
#include "openthread/channel_manager.h"
#include "openthread/jam_detection.h"
#include "openthread/joiner.h"
#include "openthread/platform/radio.h"
#include "openthread/thread_ftd.h"

#include "agent/thread_api_impl.hpp"
#include "common/code_utils.hpp"

namespace otbr {
namespace agent {

ThreadApiImpl::ThreadApiImpl(otInstance *aInstance)
    : mInstance(aInstance)
{
}

otError ThreadApiImpl::Scan(const ScanHandler &aHandler)
{
    otError err;

    mScanHandler = std::unique_ptr<ScanHandler>(new ScanHandler(aHandler));
    mScanResults.clear();

    err =
        otLinkActiveScan(mInstance, /*scanChannels =*/0, /*scanDuration=*/0, &ThreadApiImpl::sActiveScanHandler, this);

    if (err != OT_ERROR_NONE)
    {
        mScanHandler = nullptr;
    }

    return err;
}

void ThreadApiImpl::sActiveScanHandler(otActiveScanResult *aResult, void *aContext)
{
    ThreadApiImpl *network = reinterpret_cast<ThreadApiImpl *>(aContext);

    network->ActiveScanHandler(aResult);
}

void ThreadApiImpl::ActiveScanHandler(otActiveScanResult *aResult)
{
    if (aResult == nullptr)
    {
        if (mScanHandler != nullptr)
        {
            (*mScanHandler)(mScanResults);
        }
    }
    else
    {
        mScanResults.push_back(*aResult);
    }
}

} // namespace agent
} // namespace otbr
