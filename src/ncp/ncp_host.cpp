/*
 *  Copyright (c) 2024, The OpenThread Authors.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

#define OTBR_LOG_TAG "NCP_HOST"

#include "ncp_host.hpp"

#include <memory>

#include <openthread/error.h>
#include <openthread/thread.h>

#include <openthread/openthread-system.h>

#include "lib/spinel/spinel_driver.hpp"

#include "ncp/async_task.hpp"

namespace otbr {
namespace Ncp {

// =============================== NcpNetworkProperties ===============================

NcpNetworkProperties::NcpNetworkProperties(void)
    : mDeviceRole(OT_DEVICE_ROLE_DISABLED)
{
    memset(&mDatasetActiveTlvs, 0, sizeof(mDatasetActiveTlvs));
}

otDeviceRole NcpNetworkProperties::GetDeviceRole(void) const
{
    return mDeviceRole;
}

void NcpNetworkProperties::SetDeviceRole(otDeviceRole aRole)
{
    mDeviceRole = aRole;
}

bool NcpNetworkProperties::Ip6IsEnabled(void) const
{
    // TODO: Implement the method under NCP mode.
    return false;
}

uint32_t NcpNetworkProperties::GetPartitionId(void) const
{
    // TODO: Implement the method under NCP mode.
    return 0;
}

void NcpNetworkProperties::SetDatasetActiveTlvs(const otOperationalDatasetTlvs &aActiveOpDatasetTlvs)
{
    mDatasetActiveTlvs.mLength = aActiveOpDatasetTlvs.mLength;
    memcpy(mDatasetActiveTlvs.mTlvs, aActiveOpDatasetTlvs.mTlvs, aActiveOpDatasetTlvs.mLength);
}

void NcpNetworkProperties::GetDatasetActiveTlvs(otOperationalDatasetTlvs &aDatasetTlvs) const
{
    aDatasetTlvs.mLength = mDatasetActiveTlvs.mLength;
    memcpy(aDatasetTlvs.mTlvs, mDatasetActiveTlvs.mTlvs, mDatasetActiveTlvs.mLength);
}

void NcpNetworkProperties::GetDatasetPendingTlvs(otOperationalDatasetTlvs &aDatasetTlvs) const
{
    // TODO: Implement the method under NCP mode.
    OTBR_UNUSED_VARIABLE(aDatasetTlvs);
}

// ===================================== NcpHost ======================================

NcpHost::NcpHost(const char *aInterfaceName, const char *aBackboneInterfaceName, bool aDryRun)
    : mSpinelDriver(*static_cast<ot::Spinel::SpinelDriver *>(otSysGetSpinelDriver()))
    , mNetif(mNcpSpinel)
    , mInfraIf(mNcpSpinel)
{
    memset(&mConfig, 0, sizeof(mConfig));
    mConfig.mInterfaceName         = aInterfaceName;
    mConfig.mBackboneInterfaceName = aBackboneInterfaceName;
    mConfig.mDryRun                = aDryRun;
    mConfig.mSpeedUpFactor         = 1;
}

const char *NcpHost::GetCoprocessorVersion(void)
{
    return mSpinelDriver.GetVersion();
}

void NcpHost::Init(void)
{
    otSysInit(&mConfig);
    mNcpSpinel.Init(mSpinelDriver, *this);
    mNetif.Init(mConfig.mInterfaceName);
    mInfraIf.Init();

    mNcpSpinel.Ip6SetAddressCallback(
        [this](const std::vector<Ip6AddressInfo> &aAddrInfos) { mNetif.UpdateIp6UnicastAddresses(aAddrInfos); });
    mNcpSpinel.Ip6SetAddressMulticastCallback(
        [this](const std::vector<Ip6Address> &aAddrs) { mNetif.UpdateIp6MulticastAddresses(aAddrs); });
    mNcpSpinel.NetifSetStateChangedCallback([this](bool aState) { mNetif.SetNetifState(aState); });
    mNcpSpinel.Ip6SetReceiveCallback(
        [this](const uint8_t *aData, uint16_t aLength) { mNetif.Ip6Receive(aData, aLength); });
    mNcpSpinel.InfraIfSetIcmp6NdSendCallback(
        [this](uint32_t aInfraIfIndex, const otIp6Address &aAddr, const uint8_t *aData, uint16_t aDataLen) {
            OTBR_UNUSED_VARIABLE(mInfraIf.SendIcmp6Nd(aInfraIfIndex, aAddr, aData, aDataLen));
        });

    if (mConfig.mBackboneInterfaceName != nullptr && strlen(mConfig.mBackboneInterfaceName) > 0)
    {
        mInfraIf.SetInfraIf(mConfig.mBackboneInterfaceName);
    }
}

void NcpHost::Deinit(void)
{
    mNcpSpinel.Deinit();
    mNetif.Deinit();
    otSysDeinit();
}

void NcpHost::Join(const otOperationalDatasetTlvs &aActiveOpDatasetTlvs, const AsyncResultReceiver &aReceiver)
{
    AsyncTaskPtr task;
    auto errorHandler = [aReceiver](otError aError, const std::string &aErrorInfo) { aReceiver(aError, aErrorInfo); };

    task = std::make_shared<AsyncTask>(errorHandler);
    task->First([this, aActiveOpDatasetTlvs](AsyncTaskPtr aNext) {
            mNcpSpinel.DatasetSetActiveTlvs(aActiveOpDatasetTlvs, std::move(aNext));
        })
        ->Then([this](AsyncTaskPtr aNext) { mNcpSpinel.Ip6SetEnabled(true, std::move(aNext)); })
        ->Then([this](AsyncTaskPtr aNext) { mNcpSpinel.ThreadSetEnabled(true, std::move(aNext)); });
    task->Run();
}

void NcpHost::Leave(const AsyncResultReceiver &aReceiver)
{
    AsyncTaskPtr task;
    auto errorHandler = [aReceiver](otError aError, const std::string &aErrorInfo) { aReceiver(aError, aErrorInfo); };

    task = std::make_shared<AsyncTask>(errorHandler);
    task->First([this](AsyncTaskPtr aNext) { mNcpSpinel.ThreadDetachGracefully(std::move(aNext)); })
        ->Then([this](AsyncTaskPtr aNext) { mNcpSpinel.ThreadErasePersistentInfo(std::move(aNext)); });
    task->Run();
}

void NcpHost::ScheduleMigration(const otOperationalDatasetTlvs &aPendingOpDatasetTlvs,
                                const AsyncResultReceiver       aReceiver)
{
    otDeviceRole role  = GetDeviceRole();
    otError      error = OT_ERROR_NONE;
    auto errorHandler  = [aReceiver](otError aError, const std::string &aErrorInfo) { aReceiver(aError, aErrorInfo); };

    VerifyOrExit(role != OT_DEVICE_ROLE_DISABLED && role != OT_DEVICE_ROLE_DETACHED, error = OT_ERROR_INVALID_STATE);

    mNcpSpinel.DatasetMgmtSetPending(std::make_shared<otOperationalDatasetTlvs>(aPendingOpDatasetTlvs),
                                     std::make_shared<AsyncTask>(errorHandler));

exit:
    if (error != OT_ERROR_NONE)
    {
        mTaskRunner.Post(
            [aReceiver, error](void) { aReceiver(error, "Cannot schedule migration when this device is detached"); });
    }
}

void NcpHost::SetThreadEnabled(bool aEnabled, const AsyncResultReceiver aReceiver)
{
    OT_UNUSED_VARIABLE(aEnabled);

    // TODO: Implement SetThreadEnabled under NCP mode.
    mTaskRunner.Post([aReceiver](void) { aReceiver(OT_ERROR_NOT_IMPLEMENTED, "Not implemented!"); });
}

void NcpHost::SetCountryCode(const std::string &aCountryCode, const AsyncResultReceiver &aReceiver)
{
    OT_UNUSED_VARIABLE(aCountryCode);

    // TODO: Implement SetCountryCode under NCP mode.
    mTaskRunner.Post([aReceiver](void) { aReceiver(OT_ERROR_NOT_IMPLEMENTED, "Not implemented!"); });
}

void NcpHost::GetChannelMasks(const ChannelMasksReceiver &aReceiver, const AsyncResultReceiver &aErrReceiver)
{
    OT_UNUSED_VARIABLE(aReceiver);

    // TODO: Implement GetChannelMasks under NCP mode.
    mTaskRunner.Post([aErrReceiver](void) { aErrReceiver(OT_ERROR_NOT_IMPLEMENTED, "Not implemented!"); });
}

#if OTBR_ENABLE_POWER_CALIBRATION
void NcpHost::SetChannelMaxPowers(const std::vector<ChannelMaxPower> &aChannelMaxPowers,
                                  const AsyncResultReceiver          &aReceiver)
{
    OT_UNUSED_VARIABLE(aChannelMaxPowers);

    // TODO: Implement SetChannelMaxPowers under NCP mode.
    mTaskRunner.Post([aReceiver](void) { aReceiver(OT_ERROR_NOT_IMPLEMENTED, "Not implemented!"); });
}
#endif

void NcpHost::AddThreadStateChangedCallback(ThreadStateChangedCallback aCallback)
{
    // TODO: Implement AddThreadStateChangedCallback under NCP mode.
    OT_UNUSED_VARIABLE(aCallback);
}

void NcpHost::AddThreadEnabledStateChangedCallback(ThreadEnabledStateCallback aCallback)
{
    // TODO: Implement AddThreadEnabledStateChangedCallback under NCP mode.
    OT_UNUSED_VARIABLE(aCallback);
}

void NcpHost::Process(const MainloopContext &aMainloop)
{
    mSpinelDriver.Process(&aMainloop);

    mNetif.Process(&aMainloop);
}

void NcpHost::Update(MainloopContext &aMainloop)
{
    mSpinelDriver.GetSpinelInterface()->UpdateFdSet(&aMainloop);

    if (mSpinelDriver.HasPendingFrame())
    {
        aMainloop.mTimeout.tv_sec  = 0;
        aMainloop.mTimeout.tv_usec = 0;
    }

    mNetif.UpdateFdSet(&aMainloop);
}

} // namespace Ncp
} // namespace otbr
