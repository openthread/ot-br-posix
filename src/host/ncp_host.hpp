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

/**
 * @file
 *   This file includes definitions of OpenThead Host for NCP.
 */

#ifndef OTBR_AGENT_NCP_HOST_HPP_
#define OTBR_AGENT_NCP_HOST_HPP_

#include "lib/spinel/coprocessor_type.h"
#include "lib/spinel/spinel_driver.hpp"

#include "common/mainloop.hpp"
#include "host/ncp_spinel.hpp"
#include "host/thread_host.hpp"
#include "posix/cli_daemon.hpp"
#include "posix/infra_if.hpp"
#include "posix/netif.hpp"

namespace otbr {
namespace Host {

/**
 * This class implements the NetworkProperties under NCP mode.
 */
class NcpNetworkProperties : virtual public NetworkProperties, public PropsObserver
{
public:
    /**
     * Constructor
     */
    explicit NcpNetworkProperties(void);

    // NetworkProperties methods
    otDeviceRole             GetDeviceRole(void) const override;
    bool                     Ip6IsEnabled(void) const override;
    uint32_t                 GetPartitionId(void) const override;
    void                     GetDatasetActiveTlvs(otOperationalDatasetTlvs &aDatasetTlvs) const override;
    void                     GetDatasetPendingTlvs(otOperationalDatasetTlvs &aDatasetTlvs) const override;
    const otMeshLocalPrefix *GetMeshLocalPrefix(void) const override;

private:
    // PropsObserver methods
    void SetDeviceRole(otDeviceRole aRole) override;
    void SetDatasetActiveTlvs(const otOperationalDatasetTlvs &aActiveOpDatasetTlvs) override;
    void SetMeshLocalPrefix(const otMeshLocalPrefix &aMeshLocalPrefix) override;

    otDeviceRole             mDeviceRole;
    otOperationalDatasetTlvs mDatasetActiveTlvs;
    otMeshLocalPrefix        mMeshLocalPrefix;
};

class NcpHost : public MainloopProcessor,
                public ThreadHost,
                public NcpNetworkProperties,
                public Netif::Dependencies,
                public InfraIf::Dependencies
#if OTBR_ENABLE_SRP_ADVERTISING_PROXY
    ,
                public Mdns::StateObserver
#endif
{
public:
    /**
     * Constructor.
     *
     * @param[in]   aInterfaceName          A string of the NCP interface name.
     * @param[in]   aBackboneInterfaceName  A string of the backbone interface name.
     * @param[in]   aDryRun                 TRUE to indicate dry-run mode. FALSE otherwise.
     */
    NcpHost(const char *aInterfaceName, const char *aBackboneInterfaceName, bool aDryRun);

    /**
     * Destructor.
     */
    ~NcpHost(void) override = default;

    // ThreadHost methods
    void Join(const otOperationalDatasetTlvs &aActiveOpDatasetTlvs, const AsyncResultReceiver &aReceiver) override;
    void Leave(bool aEraseDataset, const AsyncResultReceiver &aReceiver) override;
    void ScheduleMigration(const otOperationalDatasetTlvs &aPendingOpDatasetTlvs,
                           const AsyncResultReceiver       aReceiver) override;
    void SetThreadEnabled(bool aEnabled, const AsyncResultReceiver aReceiver) override;
    void SetCountryCode(const std::string &aCountryCode, const AsyncResultReceiver &aReceiver) override;
    void GetChannelMasks(const ChannelMasksReceiver &aReceiver, const AsyncResultReceiver &aErrReceiver) override;
#if OTBR_ENABLE_POWER_CALIBRATION
    void SetChannelMaxPowers(const std::vector<ChannelMaxPower> &aChannelMaxPowers,
                             const AsyncResultReceiver          &aReceiver) override;
#endif
    void AddThreadStateChangedCallback(ThreadStateChangedCallback aCallback) override;
    void AddThreadEnabledStateChangedCallback(ThreadEnabledStateCallback aCallback) override;
#if OTBR_ENABLE_BACKBONE_ROUTER
    void SetBackboneRouterEnabled(bool aEnabled) override;
    void SetBackboneRouterMulticastListenerCallback(BackboneRouterMulticastListenerCallback aCallback) override;
    void SetBackboneRouterStateChangedCallback(BackboneRouterStateChangedCallback aCallback) override;
#endif
    void SetBorderAgentMeshCoPServiceChangedCallback(BorderAgentMeshCoPServiceChangedCallback aCallback) override;
    void AddEphemeralKeyStateChangedCallback(EphemeralKeyStateChangedCallback aCallback) override;
    void SetUdpForwardToHostCallback(UdpForwardToHostCallback aCallback) override;
    const otMeshLocalPrefix *GetMeshLocalPrefix(void) const override;

    CoprocessorType GetCoprocessorType(void) override
    {
        return OT_COPROCESSOR_NCP;
    }
    const char *GetCoprocessorVersion(void) override;
    const char *GetInterfaceName(void) const override
    {
        return mConfig.mInterfaceName;
    }
    void Init(void) override;
    void Deinit(void) override;
    bool IsInitialized(void) const override
    {
        return mIsInitialized;
    }

    // MainloopProcessor methods
    void Update(MainloopContext &aMainloop) override;
    void Process(const MainloopContext &aMainloop) override;

#if OTBR_ENABLE_SRP_ADVERTISING_PROXY
    void SetMdnsPublisher(Mdns::Publisher *aPublisher);
#endif

    void InitNetifCallbacks(Netif &aNetif);
    void InitInfraIfCallbacks(InfraIf &aInfraIf);

private:
#if OTBR_ENABLE_SRP_ADVERTISING_PROXY
    void HandleMdnsState(Mdns::Publisher::State aState) override;
#endif
    otbrError UdpForward(const uint8_t      *aUdpPayload,
                         uint16_t            aLength,
                         const otIp6Address &aRemoteAddr,
                         uint16_t            aRemotePort,
                         const UdpProxy     &aUdpProxy) override;

    otbrError Ip6Send(const uint8_t *aData, uint16_t aLength) override;
    otbrError Ip6MulAddrUpdateSubscription(const otIp6Address &aAddress, bool aIsAdded) override;
    otbrError SetInfraIf(uint32_t                       aInfraIfIndex,
                         bool                           aIsRunning,
                         const std::vector<Ip6Address> &aIp6Addresses) override;
    otbrError HandleIcmp6Nd(uint32_t          aInfraIfIndex,
                            const Ip6Address &aIp6Address,
                            const uint8_t    *aData,
                            uint16_t          aDataLen) override;

    bool                      mIsInitialized;
    ot::Spinel::SpinelDriver &mSpinelDriver;
    otPlatformConfig          mConfig;
    NcpSpinel                 mNcpSpinel;
    TaskRunner                mTaskRunner;
    CliDaemon                 mCliDaemon;
};

} // namespace Host
} // namespace otbr

#endif // OTBR_AGENT_NCP_HOST_HPP_
