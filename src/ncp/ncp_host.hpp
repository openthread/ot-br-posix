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
#include "ncp/ncp_spinel.hpp"
#include "ncp/thread_host.hpp"
#include "posix/netif.hpp"

namespace otbr {
namespace Ncp {

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
    otDeviceRole GetDeviceRole(void) const override;
    bool         Ip6IsEnabled(void) const override;
    uint32_t     GetPartitionId(void) const override;
    void         GetDatasetActiveTlvs(otOperationalDatasetTlvs &aDatasetTlvs) const override;
    void         GetDatasetPendingTlvs(otOperationalDatasetTlvs &aDatasetTlvs) const override;

private:
    // PropsObserver methods
    void SetDeviceRole(otDeviceRole aRole) override;
    void SetDatasetActiveTlvs(const otOperationalDatasetTlvs &aActiveOpDatasetTlvs) override;

    otDeviceRole             mDeviceRole;
    otOperationalDatasetTlvs mDatasetActiveTlvs;
};

class NcpHost : public MainloopProcessor, public ThreadHost, public NcpNetworkProperties
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
    void Leave(const AsyncResultReceiver &aReceiver) override;
    void ScheduleMigration(const otOperationalDatasetTlvs &aPendingOpDatasetTlvs,
                           const AsyncResultReceiver       aReceiver) override;
    void SetThreadEnabled(bool aEnabled, const AsyncResultReceiver aReceiver) override;
    void SetCountryCode(const std::string &aCountryCode, const AsyncResultReceiver &aReceiver) override;
    void GetChannelMasks(const ChannelMasksReceiver &aReceiver, const AsyncResultReceiver &aErrReceiver) override;
#if OTBR_ENABLE_POWER_CALIBRATION
    void SetChannelMaxPowers(const std::vector<ChannelMaxPower> &aChannelMaxPowers,
                             const AsyncResultReceiver          &aReceiver) override;
#endif
    void            AddThreadStateChangedCallback(ThreadStateChangedCallback aCallback) override;
    void            AddThreadEnabledStateChangedCallback(ThreadEnabledStateCallback aCallback) override;
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

    // MainloopProcessor methods
    void Update(MainloopContext &aMainloop) override;
    void Process(const MainloopContext &aMainloop) override;

private:
    ot::Spinel::SpinelDriver &mSpinelDriver;
    otPlatformConfig          mConfig;
    NcpSpinel                 mNcpSpinel;
    TaskRunner                mTaskRunner;
    Netif                     mNetif;
    InfraIf                   mInfraIf;
};

} // namespace Ncp
} // namespace otbr

#endif // OTBR_AGENT_NCP_HOST_HPP_
