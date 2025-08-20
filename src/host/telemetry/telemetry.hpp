/*
 *    Copyright (c) 2025, The OpenThread Authors.
 *    All rights reserved.
 *
 *    Redistribution and use in source and binary forms, with or without
 *    modification, are permitted provided that the following conditions are met:
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *    3. Neither the name of the copyright holder nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 *    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *    ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *    LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *    CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *    ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *    POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef OTBR_AGENT_TELEMETRY_HPP_
#define OTBR_AGENT_TELEMETRY_HPP_

#include "openthread-br/config.h"

#if OTBR_ENABLE_TELEMETRY_DATA_API

#include <openthread/instance.h>

#include "mdns/mdns.hpp"
#include "proto/thread_telemetry.pb.h"

#include "host/telemetry/telemetry_retriever_border_agent.hpp"

namespace otbr {
namespace Host {

class TelemetryRetriever
{
public:
    /**
     * Constructor.
     *
     * @param[in] aInstance  A pointer to the OT instance.
     */
    explicit TelemetryRetriever(otInstance *aInstance);

    /**
     * This method populates the telemetry data with best effort. The best effort means, for a given
     * telemetry, if its retrieval has error, it is left unpopulated and the process continues to
     * retrieve the remaining telemetries instead of the immediately return. The error code
     * OT_ERRROR_FAILED will be returned if there is one or more error(s) happened in the process.
     *
     * @param[in] aPublisher     The Mdns::Publisher to provide MDNS telemetry if it is not `nullptr`.
     * @param[in] telemetryData  The telemetry data to be populated.
     *
     * @retval OT_ERROR_NONE    There is no error happened in the process.
     * @retval OT_ERRROR_FAILED There is one or more error(s) happened in the process.
     */
    otError RetrieveTelemetryData(Mdns::Publisher *aPublisher, threadnetwork::TelemetryData &telemetryData);

private:
#if OTBR_ENABLE_BORDER_ROUTING
    void RetrieveInfraLinkInfo(threadnetwork::TelemetryData::InfraLinkInfo &aInfraLinkInfo);
    void RetrieveExternalRouteInfo(threadnetwork::TelemetryData::ExternalRoutes &aExternalRouteInfo);
#endif
#if OTBR_ENABLE_DHCP6_PD
    void RetrievePdInfo(threadnetwork::TelemetryData::WpanBorderRouter *aWpanBorderRouter);
    void RetrieveHashedPdPrefix(std::string *aHashedPdPrefix);
    void RetrievePdProcessedRaInfo(threadnetwork::TelemetryData::PdProcessedRaInfo *aPdProcessedRaInfo);
#endif
#if OTBR_ENABLE_BORDER_AGENT
    void RetrieveBorderAgentInfo(threadnetwork::TelemetryData::BorderAgentInfo *aBorderAgentInfo);
#endif

    otInstance *mInstance;
#if (OTBR_ENABLE_NAT64 || OTBR_ENABLE_DHCP6_PD)
    static constexpr uint8_t kNat64PdCommonHashSaltLength = 16;
    uint8_t                  mNat64PdCommonSalt[kNat64PdCommonHashSaltLength];
#endif
#if OTBR_ENABLE_BORDER_AGENT
    otbr::TelemetryRetriever::BorderAgent mTelemetryRetrieverBorderAgent;
#endif
};

} // namespace Host
} // namespace otbr

#endif // OTBR_ENABLE_TELEMETRY_DATA_API

#endif // OTBR_AGENT_TELEMETRY_HPP_
