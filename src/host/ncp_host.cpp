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

#include "host/async_task.hpp"
#include "lib/spinel/spinel_driver.hpp"

namespace otbr {
namespace Host {

// =============================== NcpNetworkProperties ===============================

constexpr otMeshLocalPrefix kMeshLocalPrefixInit = {
    {0xfd, 0xde, 0xad, 0x00, 0xbe, 0xef, 0x00, 0x00},
};

NcpNetworkProperties::NcpNetworkProperties(void)
    : mDeviceRole(OT_DEVICE_ROLE_DISABLED)
{
    memset(&mDatasetActiveTlvs, 0, sizeof(mDatasetActiveTlvs));
    SetMeshLocalPrefix(kMeshLocalPrefixInit);
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

void NcpNetworkProperties::SetMeshLocalPrefix(const otMeshLocalPrefix &aMeshLocalPrefix)
{
    memcpy(mMeshLocalPrefix.m8, aMeshLocalPrefix.m8, sizeof(mMeshLocalPrefix.m8));
}

const otMeshLocalPrefix *NcpNetworkProperties::GetMeshLocalPrefix(void) const
{
    return &mMeshLocalPrefix;
}

// ===================================== NcpHost ======================================

NcpHost::NcpHost(const char *aInterfaceName, const char *aBackboneInterfaceName, bool aDryRun)
    : mIsInitialized(false)
    , mSpinelDriver(*static_cast<ot::Spinel::SpinelDriver *>(otSysGetSpinelDriver()))
    , mCliDaemon(mNcpSpinel)
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
    mCliDaemon.Init(mConfig.mInterfaceName);

    mNcpSpinel.CliDaemonSetOutputCallback([this](const char *aOutput) { mCliDaemon.HandleCommandOutput(aOutput); });

#if OTBR_ENABLE_SRP_ADVERTISING_PROXY
#if OTBR_ENABLE_SRP_SERVER_AUTO_ENABLE_MODE
    // Let SRP server use auto-enable mode. The auto-enable mode delegates the control of SRP server to the Border
    // Routing Manager. SRP server automatically starts when bi-directional connectivity is ready.
    mNcpSpinel.SrpServerSetAutoEnableMode(/* aEnabled */ true);
#else
    mNcpSpinel.SrpServerSetEnabled(/* aEnabled */ true);
#endif
#endif
    mIsInitialized = true;
}

void NcpHost::Deinit(void)
{
    mIsInitialized = false;
    mNcpSpinel.Deinit();
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

void NcpHost::Leave(bool aEraseDataset, const AsyncResultReceiver &aReceiver)
{
    AsyncTaskPtr task;
    auto errorHandler = [aReceiver](otError aError, const std::string &aErrorInfo) { aReceiver(aError, aErrorInfo); };

    task = std::make_shared<AsyncTask>(errorHandler);
    task->First([this](AsyncTaskPtr aNext) { mNcpSpinel.ThreadDetachGracefully(std::move(aNext)); })
        ->Then([this, aEraseDataset](AsyncTaskPtr aNext) {
            if (aEraseDataset)
            {
                mNcpSpinel.ThreadErasePersistentInfo(std::move(aNext));
            }
            else
            {
                aNext->SetResult(OT_ERROR_NONE, "");
            }
        });
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

void NcpHost::SetHostPowerState(uint8_t aState, const AsyncResultReceiver &aReceiver)
{
    AsyncTaskPtr task;
    auto errorHandler = [aReceiver](otError aError, const std::string &aErrorInfo) { aReceiver(aError, aErrorInfo); };

    task = std::make_shared<AsyncTask>(errorHandler);
    task->First([this, aState](AsyncTaskPtr aNext) { mNcpSpinel.SetHostPowerState(aState, std::move(aNext)); });
    task->Run();
}

#if OTBR_ENABLE_EPSKC
void NcpHost::EnableEphemeralKey(bool aEnable, const AsyncResultReceiver &aReceiver)
{
    AsyncTaskPtr task;
    auto errorHandler = [aReceiver](otError aError, const std::string &aErrorInfo) { aReceiver(aError, aErrorInfo); };

    task = std::make_shared<AsyncTask>(errorHandler);
    task->First([this, aEnable](AsyncTaskPtr aNext) { mNcpSpinel.EnableEphemeralKey(aEnable, std::move(aNext)); });
    task->Run();
}

void NcpHost::ActivateEphemeralKey(const char                *aPskc,
                                   uint32_t                   aDuration,
                                   uint16_t                   aPort,
                                   const AsyncResultReceiver &aReceiver)
{
    AsyncTaskPtr task;
    auto errorHandler = [aReceiver](otError aError, const std::string &aErrorInfo) { aReceiver(aError, aErrorInfo); };

    task = std::make_shared<AsyncTask>(errorHandler);
    task->First([this, aPskc, aDuration, aPort](AsyncTaskPtr aNext) {
        mNcpSpinel.ActivateEphemeralKey(aPskc, aDuration, aPort, std::move(aNext));
    });
    task->Run();
}

void NcpHost::DeactivateEphemeralKey(bool aRetainActiveSession, const AsyncResultReceiver &aReceiver)
{
    AsyncTaskPtr task;
    auto errorHandler = [aReceiver](otError aError, const std::string &aErrorInfo) { aReceiver(aError, aErrorInfo); };

    task = std::make_shared<AsyncTask>(errorHandler);
    task->First([this, aRetainActiveSession](AsyncTaskPtr aNext) {
        mNcpSpinel.DeactivateEphemeralKey(aRetainActiveSession, std::move(aNext));
    });
    task->Run();
}
#endif // OTBR_ENABLE_EPSKC

#if OTBR_ENABLE_BACKBONE_ROUTER
void NcpHost::SetBackboneRouterEnabled(bool aEnabled)
{
    mNcpSpinel.SetBackboneRouterEnabled(aEnabled);
}

void NcpHost::SetBackboneRouterMulticastListenerCallback(BackboneRouterMulticastListenerCallback aCallback)
{
    mNcpSpinel.SetBackboneRouterMulticastListenerCallback(std::move(aCallback));
}

void NcpHost::SetBackboneRouterStateChangedCallback(BackboneRouterStateChangedCallback aCallback)
{
    mNcpSpinel.SetBackboneRouterStateChangedCallback(std::move(aCallback));
}
#endif

void NcpHost::SetBorderAgentMeshCoPServiceChangedCallback(BorderAgentMeshCoPServiceChangedCallback aCallback)
{
    mNcpSpinel.SetBorderAgentMeshCoPServiceChangedCallback(aCallback);
}

void NcpHost::AddEphemeralKeyStateChangedCallback(EphemeralKeyStateChangedCallback aCallback)
{
    mNcpSpinel.AddEphemeralKeyStateChangedCallback(aCallback);
}

#if OTBR_ENABLE_BORDER_AGENT && !OTBR_ENABLE_BORDER_AGENT_MESHCOP_SERVICE
void NcpHost::SetBorderAgentVendorTxtData(const std::vector<uint8_t> &aVendorTxtData)
{
    // To be implemented
    OTBR_UNUSED_VARIABLE(aVendorTxtData);
}
#endif

#if !defined(OTBR_VENDOR_NAME) || !defined(OTBR_PRODUCT_NAME)
void NcpHost::SetVendorName(const char *aVendorName)
{
    // TODO: Implement SetVendorName under NCP mode.
    OTBR_UNUSED_VARIABLE(aVendorName);
}

void NcpHost::SetVendorModel(const char *aVendorModel)
{
    // TODO: Implement SetVendorModel under NCP mode.
    OTBR_UNUSED_VARIABLE(aVendorModel);
}
#endif

void NcpHost::Process(const MainloopContext &aMainloop)
{
    mSpinelDriver.Process(&aMainloop);
    mCliDaemon.Process(aMainloop);
}

void NcpHost::Update(MainloopContext &aMainloop)
{
    mSpinelDriver.GetSpinelInterface()->UpdateFdSet(&aMainloop);

    if (mSpinelDriver.HasPendingFrame())
    {
        aMainloop.mTimeout.tv_sec  = 0;
        aMainloop.mTimeout.tv_usec = 0;
    }

    mCliDaemon.UpdateFdSet(aMainloop);
}

#if OTBR_ENABLE_MDNS
void NcpHost::SetMdnsPublisher(Mdns::Publisher *aPublisher)
{
    mNcpSpinel.SetMdnsPublisher(aPublisher);
}
#endif

#if OTBR_ENABLE_SRP_ADVERTISING_PROXY
void NcpHost::HandleMdnsState(Mdns::Publisher::State aState)
{
    mNcpSpinel.DnssdSetState(aState);
}
#endif

otbrError NcpHost::UdpForward(const uint8_t      *aUdpPayload,
                              uint16_t            aLength,
                              const otIp6Address &aRemoteAddr,
                              uint16_t            aRemotePort,
                              const UdpProxy     &aUdpProxy)
{
    return mNcpSpinel.UdpForward(aUdpPayload, aLength, aRemoteAddr, aRemotePort, aUdpProxy.GetThreadPort());
}

void NcpHost::SetUdpForwardToHostCallback(UdpForwardToHostCallback aCallback)
{
    mNcpSpinel.SetUdpForwardSendCallback(aCallback);
}

const otMeshLocalPrefix *NcpHost::GetMeshLocalPrefix(void) const
{
    return NcpNetworkProperties::GetMeshLocalPrefix();
}

void NcpHost::InitNetifCallbacks(Netif &aNetif)
{
    mNcpSpinel.Ip6SetAddressCallback(
        [&aNetif](const std::vector<Ip6AddressInfo> &aAddrInfos) { aNetif.UpdateIp6UnicastAddresses(aAddrInfos); });
    mNcpSpinel.Ip6SetAddressMulticastCallback(
        [&aNetif](const std::vector<Ip6Address> &aAddrs) { aNetif.UpdateIp6MulticastAddresses(aAddrs); });
    mNcpSpinel.NetifSetStateChangedCallback([&aNetif](bool aState) { aNetif.SetNetifState(aState); });
    mNcpSpinel.Ip6SetReceiveCallback(
        [&aNetif](const uint8_t *aData, uint16_t aLength) { aNetif.Ip6Receive(aData, aLength); });
}

void NcpHost::InitInfraIfCallbacks(InfraIf &aInfraIf)
{
    mNcpSpinel.InfraIfSetIcmp6NdSendCallback(
        [&aInfraIf](uint32_t aInfraIfIndex, const otIp6Address &aAddr, const uint8_t *aData, uint16_t aDataLen) {
            OTBR_UNUSED_VARIABLE(aInfraIf.SendIcmp6Nd(aInfraIfIndex, aAddr, aData, aDataLen));
        });
}

otbrError NcpHost::Ip6Send(const uint8_t *aData, uint16_t aLength)
{
    return mNcpSpinel.Ip6Send(aData, aLength);
}

otbrError NcpHost::Ip6MulAddrUpdateSubscription(const otIp6Address &aAddress, bool aIsAdded)
{
    return mNcpSpinel.Ip6MulAddrUpdateSubscription(aAddress, aIsAdded);
}

otbrError NcpHost::SetInfraIf(uint32_t aInfraIfIndex, bool aIsRunning, const std::vector<Ip6Address> &aIp6Addresses)
{
    return mNcpSpinel.SetInfraIf(aInfraIfIndex, aIsRunning, aIp6Addresses);
}

otbrError NcpHost::HandleIcmp6Nd(uint32_t          aInfraIfIndex,
                                 const Ip6Address &aIp6Address,
                                 const uint8_t    *aData,
                                 uint16_t          aDataLen)
{
    return mNcpSpinel.HandleIcmp6Nd(aInfraIfIndex, aIp6Address, aData, aDataLen);
}

} // namespace Host
} // namespace otbr
