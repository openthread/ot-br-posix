/*
 *    Copyright (c) 2026, The OpenThread Authors.
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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <string.h>

#include <openthread/ip6.h>
#include <openthread/srp_server.h>
#include <openthread/thread.h>
#include <openthread/platform/radio.h>

#include "common/code_utils.hpp"
#include "host/rcp_host.hpp"
#include "mdns/mdns.hpp"
#include "sdp_proxy/advertising_proxy.hpp"

#include "mock_mdns_publisher.hpp"

#if OTBR_ENABLE_SRP_ADVERTISING_PROXY

using ::testing::_;
using ::testing::Return;
using ::testing::StrEq;

struct otSrpServerService
{
    std::string              mInstanceName;
    bool                     mIsDeleted = false;
    uint16_t                 mPort      = 0;
    std::vector<uint8_t>     mTxtData;
    std::vector<std::string> mSubTypes;
};

struct otSrpServerHost
{
    std::string                     mFullName;
    std::vector<otIp6Address>       mAddresses;
    bool                            mIsDeleted = false;
    std::vector<otSrpServerService> mServices;
};

namespace {

enum class SrpEventType
{
    kSetHandler,
    kHandleResult,
};

struct SrpEvent
{
    SrpEventType               mType;
    otInstance                *mInstance      = nullptr;
    otSrpServerServiceUpdateId mId            = 0;
    otError                    mError         = OT_ERROR_NONE;
    bool                       mHandlerIsNull = true;
};

struct FakeSrpServerState
{
    otSrpServerServiceUpdateHandler      mHandler        = nullptr;
    void                                *mHandlerContext = nullptr;
    std::vector<SrpEvent>                mEvents;
    std::vector<const otSrpServerHost *> mHosts;
    otIp6Address                         mMeshLocalEid = {};

    void Reset(void)
    {
        mHandler        = nullptr;
        mHandlerContext = nullptr;
        mEvents.clear();
        mHosts.clear();
        memset(&mMeshLocalEid, 0, sizeof(mMeshLocalEid));
    }
};

FakeSrpServerState sFakeSrp;

} // namespace

extern "C" {

void otSrpServerSetServiceUpdateHandler(otInstance                     *aInstance,
                                        otSrpServerServiceUpdateHandler aServiceHandler,
                                        void                           *aContext)
{
    SrpEvent event;

    EXPECT_NE(aInstance, nullptr);

    event.mType          = SrpEventType::kSetHandler;
    event.mInstance      = aInstance;
    event.mHandlerIsNull = (aServiceHandler == nullptr);
    sFakeSrp.mEvents.push_back(event);

    sFakeSrp.mHandler        = aServiceHandler;
    sFakeSrp.mHandlerContext = aContext;
}

void otSrpServerHandleServiceUpdateResult(otInstance *aInstance, otSrpServerServiceUpdateId aId, otError aError)
{
    SrpEvent event;

    EXPECT_NE(aInstance, nullptr);

    event.mType     = SrpEventType::kHandleResult;
    event.mInstance = aInstance;
    event.mId       = aId;
    event.mError    = aError;
    sFakeSrp.mEvents.push_back(event);
}

const otSrpServerHost *otSrpServerGetNextHost(otInstance *aInstance, const otSrpServerHost *aHost)
{
    EXPECT_NE(aInstance, nullptr);

    if (sFakeSrp.mHosts.empty())
    {
        return nullptr;
    }
    if (aHost == nullptr)
    {
        return sFakeSrp.mHosts.front();
    }
    for (size_t i = 0; i + 1 < sFakeSrp.mHosts.size(); ++i)
    {
        if (sFakeSrp.mHosts[i] == aHost)
        {
            return sFakeSrp.mHosts[i + 1];
        }
    }
    return nullptr;
}

const char *otSrpServerHostGetFullName(const otSrpServerHost *aHost)
{
    return aHost->mFullName.c_str();
}

const otIp6Address *otSrpServerHostGetAddresses(const otSrpServerHost *aHost, uint8_t *aAddressesNum)
{
    *aAddressesNum = static_cast<uint8_t>(aHost->mAddresses.size());
    return aHost->mAddresses.empty() ? nullptr : aHost->mAddresses.data();
}

bool otSrpServerHostIsDeleted(const otSrpServerHost *aHost)
{
    return aHost->mIsDeleted;
}

const otSrpServerService *otSrpServerHostGetNextService(const otSrpServerHost    *aHost,
                                                        const otSrpServerService *aService)
{
    if (aHost == nullptr || aHost->mServices.empty())
    {
        return nullptr;
    }
    if (aService == nullptr)
    {
        return &aHost->mServices.front();
    }

    size_t index = static_cast<size_t>(aService - aHost->mServices.data()) + 1;

    return (index < aHost->mServices.size()) ? &aHost->mServices[index] : nullptr;
}

const char *otSrpServerServiceGetInstanceName(const otSrpServerService *aService)
{
    return aService->mInstanceName.c_str();
}

bool otSrpServerServiceIsDeleted(const otSrpServerService *aService)
{
    return aService->mIsDeleted;
}

uint16_t otSrpServerServiceGetPort(const otSrpServerService *aService)
{
    return aService->mPort;
}

const uint8_t *otSrpServerServiceGetTxtData(const otSrpServerService *aService, uint16_t *aDataLength)
{
    *aDataLength = static_cast<uint16_t>(aService->mTxtData.size());
    return aService->mTxtData.empty() ? nullptr : aService->mTxtData.data();
}

const char *otSrpServerServiceGetSubTypeServiceNameAt(const otSrpServerService *aService, uint16_t aIndex)
{
    if (aIndex >= aService->mSubTypes.size())
    {
        return nullptr;
    }
    return aService->mSubTypes[aIndex].c_str();
}

otError otSrpServerParseSubTypeServiceName(const char *aSubTypeServiceName, char *aLabel, uint8_t aLabelSize)
{
    const char *subPos = strstr(aSubTypeServiceName, "._sub.");

    if (subPos == nullptr)
    {
        return OT_ERROR_INVALID_ARGS;
    }

    size_t len = static_cast<size_t>(subPos - aSubTypeServiceName);

    if (len + 1 > aLabelSize)
    {
        return OT_ERROR_NO_BUFS;
    }

    memcpy(aLabel, aSubTypeServiceName, len);
    aLabel[len] = '\0';
    return OT_ERROR_NONE;
}

const otIp6Address *otThreadGetMeshLocalEid(otInstance *aInstance)
{
    EXPECT_NE(aInstance, nullptr);
    return &sFakeSrp.mMeshLocalEid;
}

uint8_t otIp6PrefixMatch(const otIp6Address *aFirst, const otIp6Address *aSecond)
{
    uint8_t matchedBits = 0;

    for (size_t i = 0; i < sizeof(otIp6Address); ++i)
    {
        uint8_t diff = aFirst->mFields.m8[i] ^ aSecond->mFields.m8[i];

        if (diff == 0)
        {
            matchedBits += 8;
        }
        else
        {
            while ((diff & 0x80) == 0)
            {
                matchedBits++;
                diff <<= 1;
            }
            break;
        }
    }

    return matchedBits;
}

const char *otPlatRadioGetVersionString(otInstance *aInstance)
{
    OTBR_UNUSED_VARIABLE(aInstance);
    return "test-rcp";
}

} // extern "C"

namespace otbr {
namespace Host {

OtNetworkProperties::OtNetworkProperties(void)
    : mInstance(nullptr)
{
}

otDeviceRole OtNetworkProperties::GetDeviceRole(void) const
{
    return OT_DEVICE_ROLE_DISABLED;
}

bool OtNetworkProperties::Ip6IsEnabled(void) const
{
    return false;
}

uint32_t OtNetworkProperties::GetPartitionId(void) const
{
    return 0;
}

void OtNetworkProperties::GetDatasetActiveTlvs(otOperationalDatasetTlvs &aDatasetTlvs) const
{
    aDatasetTlvs.mLength = 0;
}

void OtNetworkProperties::GetDatasetPendingTlvs(otOperationalDatasetTlvs &aDatasetTlvs) const
{
    aDatasetTlvs.mLength = 0;
}

const otMeshLocalPrefix *OtNetworkProperties::GetMeshLocalPrefix(void) const
{
    return nullptr;
}

void OtNetworkProperties::SetInstance(otInstance *aInstance)
{
    mInstance = aInstance;
}

RcpHost::RcpHost(const char                      *aInterfaceName,
                 const std::vector<const char *> &aRadioUrls,
                 const char                      *aBackboneInterfaceName,
                 bool                             aDryRun,
                 bool                             aEnableAutoAttach,
                 const char                      *aDataPath,
                 uint8_t                          aDaemonMode)
    : mInstance(nullptr)
    , mEnableAutoAttach(aEnableAutoAttach)
    , mThreadEnabledState(ThreadEnabledState::kStateDisabled)
{
    OTBR_UNUSED_VARIABLE(aInterfaceName);
    OTBR_UNUSED_VARIABLE(aRadioUrls);
    OTBR_UNUSED_VARIABLE(aBackboneInterfaceName);
    OTBR_UNUSED_VARIABLE(aDryRun);
    OTBR_UNUSED_VARIABLE(aDataPath);
    OTBR_UNUSED_VARIABLE(aDaemonMode);
    memset(&mConfig, 0, sizeof(mConfig));
}

RcpHost::~RcpHost(void) = default;

void RcpHost::Init(void)
{
    static uintptr_t sNextInstanceId = 0;

    mInstance = reinterpret_cast<otInstance *>(++sNextInstanceId);
}

void RcpHost::Deinit(void)
{
    mInstance = nullptr;
}

void RcpHost::Update(MainloopContext &aMainloop)
{
    OTBR_UNUSED_VARIABLE(aMainloop);
}

void RcpHost::Process(const MainloopContext &aMainloop)
{
    OTBR_UNUSED_VARIABLE(aMainloop);
}

void RcpHost::RegisterResetHandler(std::function<void(void)> aHandler)
{
    mResetHandlers.push_back(std::move(aHandler));
}

void RcpHost::Reset(void)
{
    Init();
    for (auto &handler : mResetHandlers)
    {
        handler();
    }
}

void RcpHost::Join(const otOperationalDatasetTlvs &aActiveOpDatasetTlvs, const AsyncResultReceiver &aRecevier)
{
    OTBR_UNUSED_VARIABLE(aActiveOpDatasetTlvs);
    OTBR_UNUSED_VARIABLE(aRecevier);
}

void RcpHost::Leave(bool aEraseDataset, const AsyncResultReceiver &aRecevier)
{
    OTBR_UNUSED_VARIABLE(aEraseDataset);
    OTBR_UNUSED_VARIABLE(aRecevier);
}

void RcpHost::ScheduleMigration(const otOperationalDatasetTlvs &aPendingOpDatasetTlvs,
                                const AsyncResultReceiver       aReceiver)
{
    OTBR_UNUSED_VARIABLE(aPendingOpDatasetTlvs);
    OTBR_UNUSED_VARIABLE(aReceiver);
}

void RcpHost::SetThreadEnabled(bool aEnabled, const AsyncResultReceiver aReceiver)
{
    OTBR_UNUSED_VARIABLE(aEnabled);
    OTBR_UNUSED_VARIABLE(aReceiver);
}

void RcpHost::SetCountryCode(const std::string &aCountryCode, const AsyncResultReceiver &aReceiver)
{
    OTBR_UNUSED_VARIABLE(aCountryCode);
    OTBR_UNUSED_VARIABLE(aReceiver);
}

void RcpHost::GetChannelMasks(const ChannelMasksReceiver &aReceiver, const AsyncResultReceiver &aErrReceiver)
{
    OTBR_UNUSED_VARIABLE(aReceiver);
    OTBR_UNUSED_VARIABLE(aErrReceiver);
}

#if OTBR_ENABLE_POWER_CALIBRATION
void RcpHost::SetChannelMaxPowers(const std::vector<ChannelMaxPower> &aChannelMaxPowers,
                                  const AsyncResultReceiver          &aReceiver)
{
    OTBR_UNUSED_VARIABLE(aChannelMaxPowers);
    OTBR_UNUSED_VARIABLE(aReceiver);
}
#endif

void RcpHost::AddThreadStateChangedCallback(ThreadStateChangedCallback aCallback)
{
    OTBR_UNUSED_VARIABLE(aCallback);
}

void RcpHost::AddThreadEnabledStateChangedCallback(ThreadEnabledStateCallback aCallback)
{
    OTBR_UNUSED_VARIABLE(aCallback);
}

#if OTBR_ENABLE_BACKBONE_ROUTER
void RcpHost::SetBackboneRouterEnabled(bool aEnabled)
{
    OTBR_UNUSED_VARIABLE(aEnabled);
}

void RcpHost::SetBackboneRouterMulticastListenerCallback(BackboneRouterMulticastListenerCallback aCallback)
{
    OTBR_UNUSED_VARIABLE(aCallback);
}

void RcpHost::SetBackboneRouterStateChangedCallback(BackboneRouterStateChangedCallback aCallback)
{
    OTBR_UNUSED_VARIABLE(aCallback);
}
#endif

void RcpHost::SetBorderAgentMeshCoPServiceChangedCallback(BorderAgentMeshCoPServiceChangedCallback aCallback)
{
    OTBR_UNUSED_VARIABLE(aCallback);
}

void RcpHost::AddEphemeralKeyStateChangedCallback(EphemeralKeyStateChangedCallback aCallback)
{
    OTBR_UNUSED_VARIABLE(aCallback);
}

void RcpHost::SetUdpForwardToHostCallback(UdpForwardToHostCallback aCallback)
{
    OTBR_UNUSED_VARIABLE(aCallback);
}

#if OTBR_ENABLE_BORDER_AGENT && !OTBR_ENABLE_BORDER_AGENT_MESHCOP_SERVICE
void RcpHost::SetBorderAgentVendorTxtData(const std::vector<uint8_t> &aVendorTxtData)
{
    OTBR_UNUSED_VARIABLE(aVendorTxtData);
}

otError RcpHost::SetBorderAgentMeshCoPServiceBaseName(const char *aBaseName)
{
    OTBR_UNUSED_VARIABLE(aBaseName);
    return OT_ERROR_NONE;
}
#endif

#ifndef OTBR_VENDOR_NAME
otError RcpHost::SetVendorName(const char *aVendorName)
{
    OTBR_UNUSED_VARIABLE(aVendorName);
    return OT_ERROR_NONE;
}
#endif

#ifndef OTBR_PRODUCT_NAME
otError RcpHost::SetVendorModel(const char *aVendorModel)
{
    OTBR_UNUSED_VARIABLE(aVendorModel);
    return OT_ERROR_NONE;
}
#endif

otbrError RcpHost::UdpForward(const uint8_t      *aUdpPayload,
                              uint16_t            aLength,
                              const otIp6Address &aRemoteAddr,
                              uint16_t            aRemotePort,
                              const UdpProxy     &aUdpProxy)
{
    OTBR_UNUSED_VARIABLE(aUdpPayload);
    OTBR_UNUSED_VARIABLE(aLength);
    OTBR_UNUSED_VARIABLE(aRemoteAddr);
    OTBR_UNUSED_VARIABLE(aRemotePort);
    OTBR_UNUSED_VARIABLE(aUdpProxy);
    return OTBR_ERROR_NONE;
}

} // namespace Host

otbrError UdpProxy::Dependencies::UdpForward(const uint8_t      *aUdpPayload,
                                             uint16_t            aLength,
                                             const otIp6Address &aRemoteAddr,
                                             uint16_t            aRemotePort,
                                             const UdpProxy     &aUdpProxy)
{
    OTBR_UNUSED_VARIABLE(aUdpPayload);
    OTBR_UNUSED_VARIABLE(aLength);
    OTBR_UNUSED_VARIABLE(aRemoteAddr);
    OTBR_UNUSED_VARIABLE(aRemotePort);
    OTBR_UNUSED_VARIABLE(aUdpProxy);
    return OTBR_ERROR_NONE;
}

otbrError CliDaemon::Dependencies::InputCommandLine(const char *aLine)
{
    OTBR_UNUSED_VARIABLE(aLine);
    return OTBR_ERROR_NONE;
}

} // namespace otbr

class AdvertisingProxyTest : public ::testing::Test
{
protected:
    AdvertisingProxyTest(void)
        : mHost("wpan0",
                std::vector<const char *>(),
                /* aBackboneInterfaceName */ "",
                /* aDryRun */ false,
                /* aEnableAutoAttach */ false)
    {
    }

    void SetUp(void) override
    {
        sFakeSrp.Reset();
        sFakeSrp.mMeshLocalEid.mFields.m8[0] = 0xfd;
        sFakeSrp.mMeshLocalEid.mFields.m8[1] = 0xde;

        mHost.Init();
        mPublisher = std::make_unique<::testing::StrictMock<MockMdnsPublisher>>();
        mAdvProxy  = std::make_unique<otbr::AdvertisingProxy>(mHost, *mPublisher);
    }

    void TearDown(void) override
    {
        mAdvProxy.reset();
        mPublisher.reset();
        sFakeSrp.Reset();
    }

    static otSrpServerHost MakeHostWithOneService(const std::string &aHostFullName,
                                                  const std::string &aServiceInstanceName)
    {
        otSrpServerHost    host;
        otSrpServerService service;
        otIp6Address       address = {};

        address.mFields.m8[0]  = 0x20;
        address.mFields.m8[1]  = 0x01;
        address.mFields.m8[2]  = 0x0d;
        address.mFields.m8[3]  = 0xb8;
        address.mFields.m8[15] = 0x01;

        service.mInstanceName = aServiceInstanceName;
        service.mIsDeleted    = false;
        service.mPort         = 12345;

        host.mFullName  = aHostFullName;
        host.mIsDeleted = false;
        host.mAddresses.push_back(address);
        host.mServices.push_back(service);

        return host;
    }

    void TriggerSrpUpdate(otSrpServerServiceUpdateId aId, const otSrpServerHost &aHost, uint32_t aTimeout = 1000)
    {
        ASSERT_NE(sFakeSrp.mHandler, nullptr);
        sFakeSrp.mHandler(aId, &aHost, aTimeout, sFakeSrp.mHandlerContext);
    }

    otbr::Host::RcpHost                                       mHost;
    std::unique_ptr<::testing::StrictMock<MockMdnsPublisher>> mPublisher;
    std::unique_ptr<otbr::AdvertisingProxy>                   mAdvProxy;
};

TEST_F(AdvertisingProxyTest, PublishHostAndServiceSuccessNotifiesSrpServer)
{
    otSrpServerHost host =
        MakeHostWithOneService("host1.default.service.arpa.", "ins1._test._tcp.default.service.arpa.");
    otbr::Mdns::Publisher::ResultCallback serviceCallback(nullptr);
    otbr::Mdns::Publisher::ResultCallback hostCallback(nullptr);

    mAdvProxy->SetEnabled(true);
    ASSERT_NE(sFakeSrp.mHandler, nullptr);
    sFakeSrp.mEvents.clear();

    EXPECT_CALL(*mPublisher, PublishServiceImpl(StrEq("host1"), StrEq("ins1"), StrEq("_test._tcp"), _, 12345, _, _))
        .WillOnce([&serviceCallback](const std::string &, const std::string &, const std::string &,
                                     const otbr::Mdns::Publisher::SubTypeList &, uint16_t,
                                     const otbr::Mdns::Publisher::TxtData &,
                                     otbr::Mdns::Publisher::ResultCallback &&aCallback) {
            serviceCallback = std::move(aCallback);
            return OTBR_ERROR_NONE;
        });

    EXPECT_CALL(*mPublisher, PublishHostImpl(StrEq("host1"), _, _))
        .WillOnce([&hostCallback](const std::string &, const otbr::Mdns::Publisher::AddressList &,
                                  otbr::Mdns::Publisher::ResultCallback &&aCallback) {
            hostCallback = std::move(aCallback);
            return OTBR_ERROR_NONE;
        });

    TriggerSrpUpdate(1, host);
    EXPECT_TRUE(sFakeSrp.mEvents.empty());

    ASSERT_FALSE(serviceCallback.IsNull());
    std::move(serviceCallback)(OTBR_ERROR_NONE);
    EXPECT_TRUE(sFakeSrp.mEvents.empty());

    ASSERT_FALSE(hostCallback.IsNull());
    std::move(hostCallback)(OTBR_ERROR_NONE);

    ASSERT_EQ(sFakeSrp.mEvents.size(), 1u);
    EXPECT_EQ(sFakeSrp.mEvents[0].mType, SrpEventType::kHandleResult);
    EXPECT_EQ(sFakeSrp.mEvents[0].mInstance, mHost.GetInstance());
    EXPECT_EQ(sFakeSrp.mEvents[0].mId, 1u);
    EXPECT_EQ(sFakeSrp.mEvents[0].mError, OT_ERROR_NONE);
}

TEST_F(AdvertisingProxyTest, StopUnregistersHandlerBeforeAbortingOutstandingUpdates)
{
    otSrpServerHost host1 =
        MakeHostWithOneService("host1.default.service.arpa.", "ins1._test._tcp.default.service.arpa.");
    otSrpServerHost host2 =
        MakeHostWithOneService("host2.default.service.arpa.", "ins2._test._tcp.default.service.arpa.");

    mAdvProxy->SetEnabled(true);
    sFakeSrp.mEvents.clear();

    EXPECT_CALL(*mPublisher, PublishServiceImpl(StrEq("host1"), StrEq("ins1"), StrEq("_test._tcp"), _, 12345, _, _))
        .WillOnce(Return(OTBR_ERROR_NONE));
    EXPECT_CALL(*mPublisher, PublishHostImpl(StrEq("host1"), _, _)).WillOnce(Return(OTBR_ERROR_NONE));

    EXPECT_CALL(*mPublisher, PublishServiceImpl(StrEq("host2"), StrEq("ins2"), StrEq("_test._tcp"), _, 12345, _, _))
        .WillOnce(Return(OTBR_ERROR_NONE));
    EXPECT_CALL(*mPublisher, PublishHostImpl(StrEq("host2"), _, _)).WillOnce(Return(OTBR_ERROR_NONE));

    TriggerSrpUpdate(10, host1);
    TriggerSrpUpdate(20, host2);
    EXPECT_TRUE(sFakeSrp.mEvents.empty());

    mAdvProxy->SetEnabled(false);

    ASSERT_EQ(sFakeSrp.mEvents.size(), 3u);
    EXPECT_EQ(sFakeSrp.mEvents[0].mType, SrpEventType::kSetHandler);
    EXPECT_TRUE(sFakeSrp.mEvents[0].mHandlerIsNull);

    EXPECT_EQ(sFakeSrp.mEvents[1].mType, SrpEventType::kHandleResult);
    EXPECT_EQ(sFakeSrp.mEvents[1].mInstance, mHost.GetInstance());
    EXPECT_EQ(sFakeSrp.mEvents[1].mId, 10u);
    EXPECT_EQ(sFakeSrp.mEvents[1].mError, OT_ERROR_ABORT);

    EXPECT_EQ(sFakeSrp.mEvents[2].mType, SrpEventType::kHandleResult);
    EXPECT_EQ(sFakeSrp.mEvents[2].mInstance, mHost.GetInstance());
    EXPECT_EQ(sFakeSrp.mEvents[2].mId, 20u);
    EXPECT_EQ(sFakeSrp.mEvents[2].mError, OT_ERROR_ABORT);
}

TEST_F(AdvertisingProxyTest, LateMdnsCallbackAfterStopIsIgnored)
{
    otSrpServerHost host =
        MakeHostWithOneService("host1.default.service.arpa.", "ins1._test._tcp.default.service.arpa.");
    otbr::Mdns::Publisher::ResultCallback serviceCallback(nullptr);
    otbr::Mdns::Publisher::ResultCallback hostCallback(nullptr);

    mAdvProxy->SetEnabled(true);

    EXPECT_CALL(*mPublisher, PublishServiceImpl(StrEq("host1"), StrEq("ins1"), StrEq("_test._tcp"), _, 12345, _, _))
        .WillOnce([&serviceCallback](const std::string &, const std::string &, const std::string &,
                                     const otbr::Mdns::Publisher::SubTypeList &, uint16_t,
                                     const otbr::Mdns::Publisher::TxtData &,
                                     otbr::Mdns::Publisher::ResultCallback &&aCallback) {
            serviceCallback = std::move(aCallback);
            return OTBR_ERROR_NONE;
        });
    EXPECT_CALL(*mPublisher, PublishHostImpl(StrEq("host1"), _, _))
        .WillOnce([&hostCallback](const std::string &, const otbr::Mdns::Publisher::AddressList &,
                                  otbr::Mdns::Publisher::ResultCallback &&aCallback) {
            hostCallback = std::move(aCallback);
            return OTBR_ERROR_NONE;
        });

    TriggerSrpUpdate(42, host);
    mAdvProxy->SetEnabled(false);

    sFakeSrp.mEvents.clear();

    ASSERT_FALSE(serviceCallback.IsNull());
    ASSERT_FALSE(hostCallback.IsNull());

    // Even after AdvertisingProxy is re-enabled, stale callbacks from before Stop()
    // must not match any entry in mOutstandingUpdates or report a second result.
    mAdvProxy->SetEnabled(true);
    sFakeSrp.mEvents.clear();
    std::move(serviceCallback)(OTBR_ERROR_NONE);
    std::move(hostCallback)(OTBR_ERROR_NONE);

    EXPECT_TRUE(sFakeSrp.mEvents.empty());
}

TEST_F(AdvertisingProxyTest, LateMdnsCallbackAfterHostDeinitDoesNotCallSrpWithNullInstance)
{
    otSrpServerHost host =
        MakeHostWithOneService("host1.default.service.arpa.", "ins1._test._tcp.default.service.arpa.");
    otbr::Mdns::Publisher::ResultCallback serviceCallback(nullptr);
    otbr::Mdns::Publisher::ResultCallback hostCallback(nullptr);

    mAdvProxy->SetEnabled(true);

    EXPECT_CALL(*mPublisher, PublishServiceImpl(StrEq("host1"), StrEq("ins1"), StrEq("_test._tcp"), _, 12345, _, _))
        .WillOnce([&serviceCallback](const std::string &, const std::string &, const std::string &,
                                     const otbr::Mdns::Publisher::SubTypeList &, uint16_t,
                                     const otbr::Mdns::Publisher::TxtData &,
                                     otbr::Mdns::Publisher::ResultCallback &&aCallback) {
            serviceCallback = std::move(aCallback);
            return OTBR_ERROR_NONE;
        });
    EXPECT_CALL(*mPublisher, PublishHostImpl(StrEq("host1"), _, _))
        .WillOnce([&hostCallback](const std::string &, const otbr::Mdns::Publisher::AddressList &,
                                  otbr::Mdns::Publisher::ResultCallback &&aCallback) {
            hostCallback = std::move(aCallback);
            return OTBR_ERROR_NONE;
        });

    TriggerSrpUpdate(99, host);
    sFakeSrp.mEvents.clear();

    // Simulate RcpHost::Deinit() happening before AdvertisingProxy::Stop() or late mDNS callbacks.
    mHost.Deinit();
    ASSERT_EQ(mHost.GetInstance(), nullptr);

    ASSERT_FALSE(serviceCallback.IsNull());
    ASSERT_FALSE(hostCallback.IsNull());
    std::move(serviceCallback)(OTBR_ERROR_NONE);
    std::move(hostCallback)(OTBR_ERROR_NONE);

    EXPECT_TRUE(sFakeSrp.mEvents.empty());

    // mDNS state notifications and PublishAllHostsAndServices() after RcpHost::Deinit()
    // must also not call OpenThread APIs with a null instance.
    sFakeSrp.mHosts.push_back(&host);
    mAdvProxy->HandleMdnsState(otbr::Mdns::Publisher::State::kReady);
    mAdvProxy->PublishAllHostsAndServices();
    EXPECT_TRUE(sFakeSrp.mEvents.empty());

    // Stopping and starting after RcpHost::Deinit() must also not call OpenThread SRP APIs with a null instance.
    mAdvProxy->SetEnabled(false);
    mAdvProxy->SetEnabled(true);
    EXPECT_TRUE(sFakeSrp.mEvents.empty());
}

TEST_F(AdvertisingProxyTest, ResetClearsOutstandingUpdatesAndRespectsEnabledState)
{
    otSrpServerHost host =
        MakeHostWithOneService("host1.default.service.arpa.", "ins1._test._tcp.default.service.arpa.");
    otbr::Mdns::Publisher::ResultCallback serviceCallback(nullptr);
    otbr::Mdns::Publisher::ResultCallback hostCallback(nullptr);

    // Resetting while AdvertisingProxy is disabled must not register the SRP handler.
    mHost.Reset();
    EXPECT_TRUE(sFakeSrp.mEvents.empty());
    EXPECT_EQ(sFakeSrp.mHandler, nullptr);

    mAdvProxy->SetEnabled(true);

    EXPECT_CALL(*mPublisher, PublishServiceImpl(StrEq("host1"), StrEq("ins1"), StrEq("_test._tcp"), _, 12345, _, _))
        .WillOnce([&serviceCallback](const std::string &, const std::string &, const std::string &,
                                     const otbr::Mdns::Publisher::SubTypeList &, uint16_t,
                                     const otbr::Mdns::Publisher::TxtData &,
                                     otbr::Mdns::Publisher::ResultCallback &&aCallback) {
            serviceCallback = std::move(aCallback);
            return OTBR_ERROR_NONE;
        });
    EXPECT_CALL(*mPublisher, PublishHostImpl(StrEq("host1"), _, _))
        .WillOnce([&hostCallback](const std::string &, const otbr::Mdns::Publisher::AddressList &,
                                  otbr::Mdns::Publisher::ResultCallback &&aCallback) {
            hostCallback = std::move(aCallback);
            return OTBR_ERROR_NONE;
        });

    TriggerSrpUpdate(55, host);
    sFakeSrp.mEvents.clear();

    otInstance *oldInstance = mHost.GetInstance();
    mHost.Reset();
    EXPECT_NE(mHost.GetInstance(), oldInstance);

    ASSERT_EQ(sFakeSrp.mEvents.size(), 1u);
    EXPECT_EQ(sFakeSrp.mEvents[0].mType, SrpEventType::kSetHandler);
    EXPECT_EQ(sFakeSrp.mEvents[0].mInstance, mHost.GetInstance());
    EXPECT_FALSE(sFakeSrp.mEvents[0].mHandlerIsNull);

    // Late mDNS callbacks from before the reset must not report stale update IDs to the new instance.
    sFakeSrp.mEvents.clear();
    ASSERT_FALSE(serviceCallback.IsNull());
    ASSERT_FALSE(hostCallback.IsNull());
    std::move(serviceCallback)(OTBR_ERROR_NONE);
    std::move(hostCallback)(OTBR_ERROR_NONE);
    EXPECT_TRUE(sFakeSrp.mEvents.empty());
}

#endif // OTBR_ENABLE_SRP_ADVERTISING_PROXY
