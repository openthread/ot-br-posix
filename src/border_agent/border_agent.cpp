/*
 *    Copyright (c) 2017, The OpenThread Authors.
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

/**
 * @file
 *   The file implements the Thread border agent.
 */

#define OTBR_LOG_TAG "AGENT"

#include "border_agent/border_agent.hpp"

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <openthread/border_agent.h>
#include <openthread/thread_ftd.h>
#include <openthread/platform/toolchain.h>

#include "agent/uris.hpp"
#include "ncp/ncp_openthread.hpp"
#if OTBR_ENABLE_BACKBONE_ROUTER
#include "backbone_router/backbone_agent.hpp"
#endif
#include "common/byteswap.hpp"
#include "common/code_utils.hpp"
#include "common/logging.hpp"
#include "common/tlv.hpp"
#include "common/types.hpp"
#include "utils/hex.hpp"
#include "utils/strcpy_utils.hpp"

namespace otbr {

static const char kBorderAgentServiceType[] = "_meshcop._udp"; ///< Border agent service type of mDNS
static const char kBorderAgentServiceInstanceName[] =
    OTBR_MESHCOP_SERVICE_INSTANCE_NAME; ///< Border agent service name of mDNS

/**
 * Locators
 *
 */
enum
{
    kAloc16Leader   = 0xfc00, ///< leader anycast locator.
    kInvalidLocator = 0xffff, ///< invalid locator.
};

uint32_t BorderAgent::StateBitmap::ToUint32(void) const
{
    uint32_t bitmap = 0;

    bitmap |= mConnectionMode << 0;
    bitmap |= mThreadIfStatus << 3;
    bitmap |= mAvailability << 5;
    bitmap |= mBbrIsActive << 7;
    bitmap |= mBbrIsPrimary << 8;

    return bitmap;
}

BorderAgent::BorderAgent(otbr::Ncp::ControllerOpenThread &aNcp)
    : mNcp(aNcp)
#if OTBR_ENABLE_MDNS_AVAHI || OTBR_ENABLE_MDNS_MDNSSD || OTBR_ENABLE_MDNS_MOJO
    , mPublisher(Mdns::Publisher::Create(AF_UNSPEC, /* aDomain */ nullptr, HandleMdnsState, this))
#if OTBR_ENABLE_SRP_ADVERTISING_PROXY
    , mAdvertisingProxy(aNcp, *mPublisher)
#endif
#else
    , mPublisher(nullptr)
#endif
#if OTBR_ENABLE_DNSSD_DISCOVERY_PROXY
    , mDiscoveryProxy(aNcp, *mPublisher)
#endif
#if OTBR_ENABLE_BACKBONE_ROUTER
    , mBackboneAgent(aNcp)
#endif
{
}

void BorderAgent::Init(void)
{
    mNcp.AddThreadStateChangedCallback([this](otChangedFlags aFlags) { HandleThreadStateChanged(aFlags); });

#if OTBR_ENABLE_BACKBONE_ROUTER
    mBackboneAgent.Init();
#endif

    if (IsThreadStarted())
    {
        Start();
    }
    else
    {
        Stop();
    }
}

otbrError BorderAgent::Start(void)
{
    otbrError error = OTBR_ERROR_NONE;

    VerifyOrExit(IsThreadStarted(), errno = EAGAIN, error = OTBR_ERROR_ERRNO);

    // In case we didn't receive Thread down event.
    Stop();

#if OTBR_ENABLE_MDNS_AVAHI || OTBR_ENABLE_MDNS_MDNSSD || OTBR_ENABLE_MDNS_MOJO
#if OTBR_ENABLE_SRP_ADVERTISING_PROXY
    mAdvertisingProxy.Start();
#endif
    UpdateMeshCopService();

#if OTBR_ENABLE_DNSSD_DISCOVERY_PROXY
    mDiscoveryProxy.Start();
#endif

#endif // OTBR_ENABLE_MDNS_AVAHI || OTBR_ENABLE_MDNS_MDNSSD || OTBR_ENABLE_MDNS_MOJO

exit:
    otbrLogResult(error, "Start Thread Border Agent");
    return error;
}

void BorderAgent::Stop(void)
{
    otbrLogInfo("Stop Thread Border Agent");

#if OTBR_ENABLE_MDNS_AVAHI || OTBR_ENABLE_MDNS_MDNSSD || OTBR_ENABLE_MDNS_MOJO
    mPublisher->Stop();
#if OTBR_ENABLE_SRP_ADVERTISING_PROXY
    mAdvertisingProxy.Stop();
#endif

#if OTBR_ENABLE_DNSSD_DISCOVERY_PROXY
    mDiscoveryProxy.Stop();
#endif

#endif
}

void BorderAgent::HandleMdnsState(void *aContext, Mdns::Publisher::State aState)
{
    static_cast<BorderAgent *>(aContext)->HandleMdnsState(aState);
}

BorderAgent::~BorderAgent(void)
{
    Stop();

    if (mPublisher != nullptr)
    {
        delete mPublisher;
        mPublisher = nullptr;
    }
}

void BorderAgent::HandleMdnsState(Mdns::Publisher::State aState)
{
    switch (aState)
    {
    case Mdns::Publisher::State::kReady:
        UpdateMeshCopService();
#if OTBR_ENABLE_SRP_ADVERTISING_PROXY
        mAdvertisingProxy.PublishAllHostsAndServices();
#endif
        break;
    default:
        otbrLogWarning("mDNS service not available!");
        break;
    }
}

void BorderAgent::PublishMeshCopService(void)
{
    StateBitmap              state;
    uint32_t                 stateUint32;
    otInstance *             instance    = mNcp.GetInstance();
    const otExtendedPanId *  extPanId    = otThreadGetExtendedPanId(instance);
    const otExtAddress *     extAddr     = otLinkGetExtendedAddress(instance);
    const char *             networkName = otThreadGetNetworkName(instance);
    Mdns::Publisher::TxtList txtList{{"rv", "1"}};

    otbrLogInfo("Publish meshcop service %s.%s.local.", kBorderAgentServiceInstanceName, kBorderAgentServiceType);

    txtList.emplace_back("nn", networkName);
    txtList.emplace_back("xp", extPanId->m8, sizeof(extPanId->m8));
    txtList.emplace_back("tv", mNcp.GetThreadVersion());

    // "dd" represents for device discriminator which
    // should always be the IEEE 802.15.4 extended address.
    txtList.emplace_back("dd", extAddr->m8, sizeof(extAddr->m8));

    state.mConnectionMode = kConnectionModePskc;
    state.mAvailability   = kAvailabilityHigh;
#if OTBR_ENABLE_BACKBONE_ROUTER
    state.mBbrIsActive  = otBackboneRouterGetState(instance) != OT_BACKBONE_ROUTER_STATE_DISABLED;
    state.mBbrIsPrimary = otBackboneRouterGetState(instance) == OT_BACKBONE_ROUTER_STATE_PRIMARY;
#endif
    switch (otThreadGetDeviceRole(instance))
    {
    case OT_DEVICE_ROLE_DISABLED:
        state.mThreadIfStatus = kThreadIfStatusNotInitialized;
        break;
    case OT_DEVICE_ROLE_DETACHED:
        state.mThreadIfStatus = kThreadIfStatusInitialized;
        break;
    default:
        state.mThreadIfStatus = kThreadIfStatusActive;
    }

    stateUint32 = htobe32(state.ToUint32());
    txtList.emplace_back("sb", reinterpret_cast<uint8_t *>(&stateUint32), sizeof(stateUint32));

    if (state.mThreadIfStatus == kThreadIfStatusActive)
    {
        otError              error;
        otOperationalDataset activeDataset;
        uint64_t             activeTimestamp;
        uint32_t             partitionId;

        if ((error = otDatasetGetActive(instance, &activeDataset)) != OT_ERROR_NONE)
        {
            otbrLogWarning("Failed to get active dataset: %s", otThreadErrorToString(error));
        }
        else
        {
            activeTimestamp = htobe64(activeDataset.mActiveTimestamp);
            txtList.emplace_back("at", reinterpret_cast<uint8_t *>(&activeTimestamp), sizeof(activeTimestamp));
        }

        partitionId = otThreadGetPartitionId(instance);
        txtList.emplace_back("pt", reinterpret_cast<uint8_t *>(&partitionId), sizeof(partitionId));
    }

#if OTBR_ENABLE_BACKBONE_ROUTER
    if (state.mBbrIsActive)
    {
        otBackboneRouterConfig bbrConfig;
        uint16_t               bbrPort = htobe16(BackboneRouter::BackboneAgent::kBackboneUdpPort);

        otBackboneRouterGetConfig(instance, &bbrConfig);
        txtList.emplace_back("sq", &bbrConfig.mSequenceNumber, sizeof(bbrConfig.mSequenceNumber));
        txtList.emplace_back("bb", reinterpret_cast<const uint8_t *>(&bbrPort), sizeof(bbrPort));
    }

    txtList.emplace_back("dn", otThreadGetDomainName(instance));
#endif

    mPublisher->PublishService(/* aHostName */ nullptr, otBorderAgentGetUdpPort(instance),
                               kBorderAgentServiceInstanceName, kBorderAgentServiceType, Mdns::Publisher::SubTypeList{},
                               txtList);
}

void BorderAgent::UnpublishMeshCopService(void)
{
    assert(IsThreadStarted());
    VerifyOrExit(!mNetworkName.empty());

    otbrLogInfo("Unpublish meshcop service %s.%s.local.", kBorderAgentServiceInstanceName, kBorderAgentServiceType);

    mPublisher->UnpublishService(kBorderAgentServiceInstanceName, kBorderAgentServiceType);

exit:
    return;
}

void BorderAgent::UpdateMeshCopService(void)
{
    assert(IsThreadStarted());

    const char *networkName = otThreadGetNetworkName(mNcp.GetInstance());

    VerifyOrExit(mPublisher->IsStarted(), mPublisher->Start());
    VerifyOrExit(IsPskcInitialized(), UnpublishMeshCopService());

    PublishMeshCopService();
    mNetworkName = networkName;

exit:
    return;
}

void BorderAgent::HandleThreadStateChanged(otChangedFlags aFlags)
{
    VerifyOrExit(mPublisher != nullptr);
    VerifyOrExit(aFlags & (OT_CHANGED_THREAD_ROLE | OT_CHANGED_THREAD_EXT_PANID | OT_CHANGED_THREAD_NETWORK_NAME |
                           OT_CHANGED_ACTIVE_DATASET | OT_CHANGED_THREAD_PARTITION_ID |
                           OT_CHANGED_THREAD_BACKBONE_ROUTER_STATE | OT_CHANGED_PSKC));

    if (aFlags & OT_CHANGED_THREAD_ROLE)
    {
        otbrLogInfo("Thread is %s", (IsThreadStarted() ? "up" : "down"));

        if (IsThreadStarted())
        {
            Start();
        }
        else
        {
            Stop();
        }
    }
    else if (IsThreadStarted())
    {
        UpdateMeshCopService();
    }

exit:
    return;
}

bool BorderAgent::IsThreadStarted(void) const
{
    otDeviceRole role = otThreadGetDeviceRole(mNcp.GetInstance());

    return role == OT_DEVICE_ROLE_CHILD || role == OT_DEVICE_ROLE_ROUTER || role == OT_DEVICE_ROLE_LEADER;
}

bool BorderAgent::IsPskcInitialized(void) const
{
    bool   initialized = false;
    otPskc pskc;

    otThreadGetPskc(mNcp.GetInstance(), &pskc);

    for (uint8_t byte : pskc.m8)
    {
        if (byte != 0x00)
        {
            initialized = true;
            break;
        }
    }

    return initialized;
}

} // namespace otbr
