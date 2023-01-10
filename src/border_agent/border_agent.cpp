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

#define OTBR_LOG_TAG "BA"

#include "border_agent/border_agent.hpp"

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <iomanip>
#include <random>
#include <sstream>

#include <openthread/border_agent.h>
#include <openthread/border_routing.h>
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

namespace otbr {

static const char kVendorName[]             = OTBR_VENDOR_NAME;
static const char kProductName[]            = OTBR_PRODUCT_NAME;
static const char kBorderAgentServiceType[] = "_meshcop._udp"; ///< Border agent service type of mDNS
static const char kBorderAgentServiceInstanceName[] =
    OTBR_MESHCOP_SERVICE_INSTANCE_NAME; ///< Border agent service name of mDNS
static constexpr int kBorderAgentServiceDummyPort = 49152;

/**
 * Locators
 *
 */
enum
{
    kAloc16Leader   = 0xfc00, ///< leader anycast locator.
    kInvalidLocator = 0xffff, ///< invalid locator.
};

enum : uint8_t
{
    kConnectionModeDisabled = 0,
    kConnectionModePskc     = 1,
    kConnectionModePskd     = 2,
    kConnectionModeVendor   = 3,
    kConnectionModeX509     = 4,
};

enum : uint8_t
{
    kThreadIfStatusNotInitialized = 0,
    kThreadIfStatusInitialized    = 1,
    kThreadIfStatusActive         = 2,
};

enum : uint8_t
{
    kAvailabilityInfrequent = 0,
    kAvailabilityHigh       = 1,
};

struct StateBitmap
{
    uint32_t mConnectionMode : 3;
    uint32_t mThreadIfStatus : 2;
    uint32_t mAvailability : 2;
    uint32_t mBbrIsActive : 1;
    uint32_t mBbrIsPrimary : 1;

    StateBitmap(void)
        : mConnectionMode(0)
        , mThreadIfStatus(0)
        , mAvailability(0)
        , mBbrIsActive(0)
        , mBbrIsPrimary(0)
    {
    }

    uint32_t ToUint32(void) const
    {
        uint32_t bitmap = 0;

        bitmap |= mConnectionMode << 0;
        bitmap |= mThreadIfStatus << 3;
        bitmap |= mAvailability << 5;
        bitmap |= mBbrIsActive << 7;
        bitmap |= mBbrIsPrimary << 8;

        return bitmap;
    }
};

BorderAgent::BorderAgent(otbr::Ncp::ControllerOpenThread &aNcp)
    : mNcp(aNcp)
    , mPublisher(Mdns::Publisher::Create([this](Mdns::Publisher::State aNewState) { HandleMdnsState(aNewState); }))
#if OTBR_ENABLE_SRP_ADVERTISING_PROXY
    , mAdvertisingProxy(aNcp, *mPublisher)
#endif
#if OTBR_ENABLE_DNSSD_DISCOVERY_PROXY
    , mDiscoveryProxy(aNcp, *mPublisher)
#endif
#if OTBR_ENABLE_TREL
    , mTrelDnssd(aNcp, *mPublisher)
#endif
{
}

void BorderAgent::Init(void)
{
    mNcp.AddThreadStateChangedCallback([this](otChangedFlags aFlags) { HandleThreadStateChanged(aFlags); });

#if OTBR_ENABLE_DBUS_SERVER
    mNcp.GetThreadHelper()->SetUpdateMeshCopTxtHandler([this](std::map<std::string, std::vector<uint8_t>> aUpdate) {
        HandleUpdateVendorMeshCoPTxtEntries(std::move(aUpdate));
    });
    mNcp.RegisterResetHandler([this]() {
        mNcp.GetThreadHelper()->SetUpdateMeshCopTxtHandler([this](std::map<std::string, std::vector<uint8_t>> aUpdate) {
            HandleUpdateVendorMeshCoPTxtEntries(std::move(aUpdate));
        });
    });
#endif

    mServiceInstanceName = BaseServiceInstanceName();

    Start();
}

void BorderAgent::Deinit(void)
{
    Stop();
}

void BorderAgent::Start(void)
{
    otbrError error = OTBR_ERROR_NONE;

    SuccessOrExit(error = mPublisher->Start());
#if OTBR_ENABLE_SRP_ADVERTISING_PROXY
    mAdvertisingProxy.Start();
#endif
#if OTBR_ENABLE_DNSSD_DISCOVERY_PROXY
    mDiscoveryProxy.Start();
#endif

exit:
    otbrLogResult(error, "Start Thread Border Agent");
}

void BorderAgent::Stop(void)
{
    otbrLogInfo("Stop Thread Border Agent");

#if OTBR_ENABLE_SRP_ADVERTISING_PROXY
    mAdvertisingProxy.Stop();
#endif

#if OTBR_ENABLE_DNSSD_DISCOVERY_PROXY
    mDiscoveryProxy.Stop();
#endif

    UnpublishMeshCopService();
    mPublisher->Stop();
}

BorderAgent::~BorderAgent(void)
{
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
#if OTBR_ENABLE_TREL
        mTrelDnssd.OnMdnsPublisherReady();
#endif
        break;
    default:
        otbrLogWarning("mDNS publisher not available!");
        break;
    }
}

static uint64_t ConvertTimestampToUint64(const otTimestamp &aTimestamp)
{
    // 64 bits Timestamp fields layout
    //-----48 bits------//-----15 bits-----//-------1 bit-------//
    //     Seconds      //      Ticks      //  Authoritative    //
    return (aTimestamp.mSeconds << 16) | static_cast<uint64_t>(aTimestamp.mTicks << 1) |
           static_cast<uint64_t>(aTimestamp.mAuthoritative);
}

#if OTBR_ENABLE_BORDER_ROUTING
void AppendOmrTxtEntry(otInstance &aInstance, Mdns::Publisher::TxtList &aTxtList)
{
    otIp6Prefix       omrPrefix;
    otRoutePreference preference;

    if (OT_ERROR_NONE == otBorderRoutingGetFavoredOmrPrefix(&aInstance, &omrPrefix, &preference))
    {
        std::vector<uint8_t> omrData;

        omrData.reserve(1 + OT_IP6_PREFIX_SIZE);
        omrData.push_back(omrPrefix.mLength);
        std::copy(omrPrefix.mPrefix.mFields.m8, omrPrefix.mPrefix.mFields.m8 + (omrPrefix.mLength + 7) / 8,
                  std::back_inserter(omrData));
        aTxtList.emplace_back("omr", omrData.data(), omrData.size());
    }
}
#endif

StateBitmap GetStateBitmap(otInstance &aInstance)
{
    StateBitmap state;

    state.mConnectionMode = kConnectionModePskc;
    state.mAvailability   = kAvailabilityHigh;

    switch (otThreadGetDeviceRole(&aInstance))
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

#if OTBR_ENABLE_BACKBONE_ROUTER
    state.mBbrIsActive = state.mThreadIfStatus == kThreadIfStatusActive &&
                         otBackboneRouterGetState(&aInstance) != OT_BACKBONE_ROUTER_STATE_DISABLED;
    state.mBbrIsPrimary = state.mThreadIfStatus == kThreadIfStatusActive &&
                          otBackboneRouterGetState(&aInstance) == OT_BACKBONE_ROUTER_STATE_PRIMARY;
#endif

    return state;
}

#if OTBR_ENABLE_BACKBONE_ROUTER
void AppendBbrTxtEntries(otInstance &aInstance, StateBitmap aState, Mdns::Publisher::TxtList &aTxtList)
{
    if (aState.mBbrIsActive)
    {
        otBackboneRouterConfig bbrConfig;
        uint16_t               bbrPort = htobe16(BackboneRouter::BackboneAgent::kBackboneUdpPort);

        otBackboneRouterGetConfig(&aInstance, &bbrConfig);
        aTxtList.emplace_back("sq", &bbrConfig.mSequenceNumber, sizeof(bbrConfig.mSequenceNumber));
        aTxtList.emplace_back("bb", reinterpret_cast<const uint8_t *>(&bbrPort), sizeof(bbrPort));
    }

    aTxtList.emplace_back("dn", otThreadGetDomainName(&aInstance));
}
#endif

void AppendActiveTimestampTxtEntry(otInstance &aInstance, Mdns::Publisher::TxtList &aTxtList)
{
    otError              error;
    otOperationalDataset activeDataset;

    if ((error = otDatasetGetActive(&aInstance, &activeDataset)) != OT_ERROR_NONE)
    {
        otbrLogWarning("Failed to get active dataset: %s", otThreadErrorToString(error));
    }
    else
    {
        uint64_t activeTimestampValue = ConvertTimestampToUint64(activeDataset.mActiveTimestamp);

        activeTimestampValue = htobe64(activeTimestampValue);
        aTxtList.emplace_back("at", reinterpret_cast<uint8_t *>(&activeTimestampValue), sizeof(activeTimestampValue));
    }
}

#if OTBR_ENABLE_DBUS_SERVER
void AppendVendorTxtEntries(const std::map<std::string, std::vector<uint8_t>> &aVendorEntries,
                            Mdns::Publisher::TxtList                          &aTxtList)
{
    for (const auto &entry : aVendorEntries)
    {
        const std::string          &key   = entry.first;
        const std::vector<uint8_t> &value = entry.second;
        bool                        found = false;

        for (auto &addedEntry : aTxtList)
        {
            if (addedEntry.mName == key)
            {
                addedEntry.mValue = value;
                found             = true;
                break;
            }
        }
        if (!found)
        {
            aTxtList.emplace_back(key.c_str(), value.data(), value.size());
        }
    }
}
#endif

void BorderAgent::PublishMeshCopService(void)
{
    StateBitmap              state;
    uint32_t                 stateUint32;
    otInstance              *instance    = mNcp.GetInstance();
    const otExtendedPanId   *extPanId    = otThreadGetExtendedPanId(instance);
    const otExtAddress      *extAddr     = otLinkGetExtendedAddress(instance);
    const char              *networkName = otThreadGetNetworkName(instance);
    Mdns::Publisher::TxtList txtList{{"rv", "1"}};
    int                      port;

    otbrLogInfo("Publish meshcop service %s.%s.local.", mServiceInstanceName.c_str(), kBorderAgentServiceType);

    txtList.emplace_back("vn", kVendorName);
    txtList.emplace_back("mn", kProductName);
    txtList.emplace_back("nn", networkName);
    txtList.emplace_back("xp", extPanId->m8, sizeof(extPanId->m8));
    txtList.emplace_back("tv", mNcp.GetThreadVersion());

    // "xa" stands for Extended MAC Address (64-bit) of the Thread Interface of the Border Agent.
    txtList.emplace_back("xa", extAddr->m8, sizeof(extAddr->m8));

    state       = GetStateBitmap(*instance);
    stateUint32 = htobe32(state.ToUint32());
    txtList.emplace_back("sb", reinterpret_cast<uint8_t *>(&stateUint32), sizeof(stateUint32));

    if (state.mThreadIfStatus == kThreadIfStatusActive)
    {
        uint32_t partitionId;

        AppendActiveTimestampTxtEntry(*instance, txtList);
        partitionId = otThreadGetPartitionId(instance);
        txtList.emplace_back("pt", reinterpret_cast<uint8_t *>(&partitionId), sizeof(partitionId));
    }

#if OTBR_ENABLE_BACKBONE_ROUTER
    AppendBbrTxtEntries(*instance, state, txtList);
#endif
#if OTBR_ENABLE_BORDER_ROUTING
    AppendOmrTxtEntry(*instance, txtList);
#endif
#if OTBR_ENABLE_DBUS_SERVER
    AppendVendorTxtEntries(mMeshCopTxtUpdate, txtList);
#endif

    if (otBorderAgentGetState(instance) != OT_BORDER_AGENT_STATE_STOPPED)
    {
        port = otBorderAgentGetUdpPort(instance);
    }
    else
    {
        // When thread interface is not active, the border agent is not started, thus it's not listening to any port and
        // not handling requests. In such situation, we use a dummy port number for publishing the MeshCoP service to
        // advertise the status of the border router. One can learn the thread interface status from `sb` entry so it
        // doesn't have to send requests to the dummy port when border agent is not running.
        port = kBorderAgentServiceDummyPort;
    }

    mPublisher->PublishService(/* aHostName */ "", mServiceInstanceName, kBorderAgentServiceType,
                               Mdns::Publisher::SubTypeList{}, port, txtList, [this](otbrError aError) {
                                   if (aError == OTBR_ERROR_ABORTED)
                                   {
                                       // OTBR_ERROR_ABORTED is thrown when an ongoing service registration is
                                       // cancelled. This can happen when the meshcop service is being updated
                                       // frequently. To avoid false alarms, it should not be logged like a real error.
                                       otbrLogInfo("Cancelled previous publishing meshcop service %s.%s.local",
                                                   mServiceInstanceName.c_str(), kBorderAgentServiceType);
                                   }
                                   else
                                   {
                                       otbrLogResult(aError, "Result of publish meshcop service %s.%s.local",
                                                     mServiceInstanceName.c_str(), kBorderAgentServiceType);
                                   }
                                   if (aError == OTBR_ERROR_DUPLICATED)
                                   {
                                       // Try to unpublish current service in case we are trying to register
                                       // multiple new services simultaneously when the original service name
                                       // is conflicted.
                                       UnpublishMeshCopService();
                                       mServiceInstanceName = GetAlternativeServiceInstanceName();
                                       PublishMeshCopService();
                                   }
                               });
}

void BorderAgent::UnpublishMeshCopService(void)
{
    otbrLogInfo("Unpublish meshcop service %s.%s.local", mServiceInstanceName.c_str(), kBorderAgentServiceType);

    mPublisher->UnpublishService(mServiceInstanceName, kBorderAgentServiceType, [this](otbrError aError) {
        otbrLogResult(aError, "Result of unpublish meshcop service %s.%s.local", mServiceInstanceName.c_str(),
                      kBorderAgentServiceType);
    });
}

void BorderAgent::UpdateMeshCopService(void)
{
    VerifyOrExit(mPublisher->IsStarted(), mPublisher->Start());
    PublishMeshCopService();
exit:
    return;
}

#if OTBR_ENABLE_DBUS_SERVER
void BorderAgent::HandleUpdateVendorMeshCoPTxtEntries(std::map<std::string, std::vector<uint8_t>> aUpdate)
{
    mMeshCopTxtUpdate = std::move(aUpdate);
    UpdateMeshCopService();
}
#endif

void BorderAgent::HandleThreadStateChanged(otChangedFlags aFlags)
{
    VerifyOrExit(mPublisher != nullptr);

    if (aFlags & OT_CHANGED_THREAD_ROLE)
    {
        otbrLogInfo("Thread is %s", (IsThreadStarted() ? "up" : "down"));
    }

    if (aFlags & (OT_CHANGED_THREAD_ROLE | OT_CHANGED_THREAD_EXT_PANID | OT_CHANGED_THREAD_NETWORK_NAME |
                  OT_CHANGED_THREAD_BACKBONE_ROUTER_STATE | OT_CHANGED_THREAD_NETDATA))
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

std::string BorderAgent::BaseServiceInstanceName() const
{
    const otExtAddress *extAddress = otLinkGetExtendedAddress(mNcp.GetInstance());
    std::stringstream   ss;

    ss << kBorderAgentServiceInstanceName << " #";
    ss << std::uppercase << std::hex << std::setfill('0');
    ss << std::setw(2) << static_cast<int>(extAddress->m8[6]);
    ss << std::setw(2) << static_cast<int>(extAddress->m8[7]);
    return ss.str();
}

std::string BorderAgent::GetAlternativeServiceInstanceName() const
{
    std::random_device                      r;
    std::default_random_engine              engine(r());
    std::uniform_int_distribution<uint16_t> uniform_dist(1, 0xFFFF);
    uint16_t                                rand = uniform_dist(engine);
    std::stringstream                       ss;

    ss << BaseServiceInstanceName() << " (" << rand << ")";
    return ss.str();
}

} // namespace otbr
