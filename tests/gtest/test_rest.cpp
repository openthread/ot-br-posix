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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <cJSON.h>

#include <openthread/dataset.h>
#include <openthread/dataset_ftd.h>
#include <openthread/ip6.h>
#include <openthread/link.h>
#include <openthread/netdiag.h>
#include <openthread/thread.h>

#include "common/mainloop.hpp"
#include "common/mainloop_manager.hpp"
#include "host/rcp_host.hpp"
#include "rest/actions/network_diagnostic.hpp"
#include "rest/actions_list.hpp"
#include "rest/network_diag_handler.hpp"
#include "rest/rest_devices_coll.hpp"
#include "rest/rest_diagnostics_coll.hpp"
#include "rest/rest_server_common.hpp"
#include "rest/services.hpp"
#include "utils/hex.hpp"
#include "utils/string_utils.hpp"

#include "fake_platform.hpp"

namespace otbr {
namespace rest {

static void MainloopProcessUntil(otbr::MainloopContext    &aMainloop,
                                 uint32_t                  aTimeoutSec,
                                 std::function<bool(void)> aCondition)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(aTimeoutSec);

    while (!aCondition())
    {
        otbr::MainloopManager::GetInstance().Update(aMainloop);
        otbr::MainloopManager::GetInstance().Process(aMainloop);

        if (std::chrono::steady_clock::now() > deadline)
        {
            break;
        }
    }
}

class RestNetworkDiagTest : public testing::Test
{
protected:
    void SetUp() override
    {
        mHost.reset(new otbr::Host::RcpHost("wpan0", std::vector<const char *>(), /* aBackboneInterfaceName */ "",
                                            /* aDryRun */ false, /* aEnableAutoAttach */ false));
        mHost->Init();

        otInstance *instance = ot::FakePlatform::CurrentInstance();
        ASSERT_NE(instance, nullptr);

        otOperationalDataset     dataset;
        otOperationalDatasetTlvs datasetTlvs;
        EXPECT_EQ(otDatasetCreateNewNetwork(instance, &dataset), OT_ERROR_NONE);
        otDatasetConvertToTlvs(&dataset, &datasetTlvs);
        EXPECT_EQ(otDatasetSetActiveTlvs(instance, &datasetTlvs), OT_ERROR_NONE);
        EXPECT_EQ(otIp6SetEnabled(instance, true), OT_ERROR_NONE);
        EXPECT_EQ(otThreadSetEnabled(instance, true), OT_ERROR_NONE);

        MainloopProcessUntil(mMainloop, 2, [this]() { return mHost->GetDeviceRole() == OT_DEVICE_ROLE_LEADER; });
        ASSERT_EQ(mHost->GetDeviceRole(), OT_DEVICE_ROLE_LEADER);

        mServices.reset(new Services());
        mServices->Init(instance);

        RegisterSelfDevice();
    }

    void TearDown() override
    {
        mServices.reset();
        if (mHost)
        {
            mHost->Deinit();
            mHost.reset();
        }
    }

    otInstance *GetInstance() const { return ot::FakePlatform::CurrentInstance(); }

    NetworkDiagHandler &GetDiagHandler() { return mServices->GetNetworkDiagHandler(); }

    auto &GetDiagSet() { return GetDiagHandler().mDiagSet; }

    auto &GetChildTables() { return GetDiagHandler().mChildTables; }

    auto &GetChildIps() { return GetDiagHandler().mChildIps; }

    auto &GetRouterNeighbors() { return GetDiagHandler().mRouterNeighbors; }

    void SetFinishedState()
    {
        GetDiagHandler().mRequestState          = NetworkDiagHandler::RequestState::kDone;
        GetDiagHandler().mDiagQueryRequestState = NetworkDiagHandler::RequestState::kDone;
    }

    void SetWaitingSingleRequestState(Seconds aTimeout, std::chrono::steady_clock::time_point aStartTime)
    {
        GetDiagHandler().mIsDiscoveryRequest    = false;
        GetDiagHandler().mTimeout               = aStartTime + aTimeout;
        GetDiagHandler().mTimeLastAttempt       = aStartTime;
        GetDiagHandler().mRequestStartTime      = aStartTime;
        GetDiagHandler().mMaxRetries            = 1;
        GetDiagHandler().mRetries               = 1;
        GetDiagHandler().mDiagReqTlvs[0]        = OT_NETWORK_DIAGNOSTIC_TLV_EXT_ADDRESS;
        GetDiagHandler().mDiagReqTlvs[1]        = OT_NETWORK_DIAGNOSTIC_TLV_SHORT_ADDRESS;
        GetDiagHandler().mDiagReqTlvsCount      = 2;
        GetDiagHandler().mRequestState          = NetworkDiagHandler::RequestState::kWaiting;
        GetDiagHandler().mDiagQueryRequestState = NetworkDiagHandler::RequestState::kDone;
    }

    void SeedRetainedSubTables(uint16_t aRloc16)
    {
        GetChildTables()[aRloc16]     = NetworkDiagHandler::RouterChildTable{};
        GetChildIps()[aRloc16]        = NetworkDiagHandler::RouterChildIp6Addrs{};
        GetRouterNeighbors()[aRloc16] = NetworkDiagHandler::RouterNeighbors{};
    }

    std::chrono::steady_clock::time_point GetRequestStartTime() const { return GetDiagHandler().mRequestStartTime; }

    const NetworkDiagHandler &GetDiagHandler() const { return mServices->GetNetworkDiagHandler(); }

    void ProcessDiagHandler() { GetDiagHandler().Process(); }

    void RegisterSelfDevice()
    {
        otInstance         *instance = GetInstance();
        const otExtAddress *extAddr  = otLinkGetExtendedAddress(instance);
        const otIp6Address *mlEid    = otThreadGetMeshLocalEid(instance);
        std::string extAddrStr = StringUtils::ToLowercase(otbr::Utils::Bytes2Hex(extAddr->m8, OT_EXT_ADDRESS_SIZE));
        DeviceInfo  deviceInfo = {};

        deviceInfo.mExtAddress = *extAddr;
        memcpy(&deviceInfo.mMlEidIid, &mlEid->mFields.m8[8], sizeof(deviceInfo.mMlEidIid));

        GetDiagHandler().SetDeviceItemAttributes(extAddrStr, deviceInfo);
    }

    std::string GetSelfExtAddrString() const
    {
        const otExtAddress *extAddr = otLinkGetExtendedAddress(GetInstance());
        return StringUtils::ToLowercase(otbr::Utils::Bytes2Hex(extAddr->m8, OT_EXT_ADDRESS_SIZE));
    }

    std::string GetSelfRlocHexString(bool aWithPrefix = true) const
    {
        uint16_t rloc16 = otThreadGetRloc16(GetInstance());
        char     buf[8];
        snprintf(buf, sizeof(buf), aWithPrefix ? "0x%04x" : "00%04x", rloc16);
        return std::string(buf);
    }

    std::string GetSelfMlEidIidString() const
    {
        const otIp6Address *mlEid = otThreadGetMeshLocalEid(GetInstance());
        return StringUtils::ToLowercase(otbr::Utils::Bytes2Hex(&mlEid->mFields.m8[8], OT_EXT_ADDRESS_SIZE));
    }

    static DiagInfo MakeDiagInfo(uint16_t                              aRloc16,
                                 const otExtAddress                   &aExtAddr,
                                 const otIp6Address                   *aPeerAddr,
                                 const std::vector<otIp6Address>      &aIp6Addrs,
                                 std::chrono::steady_clock::time_point aStartTime)
    {
        DiagInfo         info = {};
        otNetworkDiagTlv tlv  = {};

        info.mStartTime = aStartTime;
        if (aPeerAddr != nullptr)
        {
            info.mPeerAddr = *aPeerAddr;
        }

        tlv.mType             = OT_NETWORK_DIAGNOSTIC_TLV_EXT_ADDRESS;
        tlv.mData.mExtAddress = aExtAddr;
        info.mDiagContent.push_back(tlv);

        tlv               = {};
        tlv.mType         = OT_NETWORK_DIAGNOSTIC_TLV_SHORT_ADDRESS;
        tlv.mData.mAddr16 = aRloc16;
        info.mDiagContent.push_back(tlv);

        if (!aIp6Addrs.empty())
        {
            tlv                           = {};
            tlv.mType                     = OT_NETWORK_DIAGNOSTIC_TLV_IP6_ADDR_LIST;
            tlv.mData.mIp6AddrList.mCount = static_cast<uint8_t>(aIp6Addrs.size());
            for (size_t i = 0; i < aIp6Addrs.size() && i < OT_NETWORK_BASE_TLV_MAX_LENGTH / OT_IP6_ADDRESS_SIZE; ++i)
            {
                tlv.mData.mIp6AddrList.mList[i] = aIp6Addrs[i];
            }
            info.mDiagContent.push_back(tlv);
        }

        return info;
    }

    static cJSON *BuildNetworkDiagnosticActionRequest(const std::string &aDestination,
                                                      const std::string &aDestinationType,
                                                      uint32_t           aTimeoutSec = 5)
    {
        cJSON *root       = cJSON_CreateObject();
        cJSON *attributes = cJSON_CreateObject();
        cJSON *types      = cJSON_CreateArray();

        cJSON_AddItemToObject(root, "type", cJSON_CreateString("getNetworkDiagnosticTask"));
        cJSON_AddItemToObject(attributes, "destination", cJSON_CreateString(aDestination.c_str()));
        cJSON_AddItemToObject(attributes, "destinationType", cJSON_CreateString(aDestinationType.c_str()));
        cJSON_AddItemToArray(types, cJSON_CreateString("extAddress"));
        cJSON_AddItemToArray(types, cJSON_CreateString("rloc16"));
        cJSON_AddItemToObject(attributes, "types", types);
        cJSON_AddItemToObject(attributes, "timeout", cJSON_CreateNumber(aTimeoutSec));
        cJSON_AddItemToObject(root, "attributes", attributes);

        return root;
    }

    otbr::MainloopContext                mMainloop;
    std::unique_ptr<otbr::Host::RcpHost> mHost;
    std::unique_ptr<Services>            mServices;
};

// 1. Completed extended, rloc, and mleid tasks have non-empty result IDs, and each ID retrieves its diagnostic item.
TEST_F(RestNetworkDiagTest, CompletedExtendedRlocAndMleidTasksReturnValidDiagnosticItem)
{
    struct TestCase
    {
        std::string mDestination;
        std::string mDestinationType;
    };

    const std::string expectedExtAddr = GetSelfExtAddrString();
    const std::string expectedRloc    = GetSelfRlocHexString(/* aWithPrefix */ true);

    std::vector<TestCase> cases = {
        {GetSelfExtAddrString(), "extended"},
        {GetSelfRlocHexString(/* aWithPrefix */ true), "rloc"},
        {GetSelfRlocHexString(/* aWithPrefix */ false), "rloc"},
        {GetSelfMlEidIidString(), "mleid"},
    };

    for (const auto &tc : cases)
    {
        cJSON *req = BuildNetworkDiagnosticActionRequest(tc.mDestination, tc.mDestinationType);
        ASSERT_NE(req, nullptr);
        EXPECT_TRUE(mServices->GetActionsList().ValidateRequest(req))
            << "Failed ValidateRequest for type=" << tc.mDestinationType << " dest=" << tc.mDestination;

        std::string actionUuid;
        EXPECT_EQ(mServices->GetActionsList().CreateAction(req, actionUuid), OT_ERROR_NONE);
        cJSON_Delete(req);
        ASSERT_FALSE(actionUuid.empty());

        actions::BasicActions *action = mServices->GetActionsList().GetItem(actionUuid);
        ASSERT_NE(action, nullptr);

        MainloopProcessUntil(mMainloop, 3, [action]() { return !action->IsPendingOrActive(); });
        EXPECT_EQ(action->GetStatus(), actions::kActionStatusCompleted)
            << "Task did not complete for type=" << tc.mDestinationType << " dest=" << tc.mDestination;

        cJSON *actionJson = cJSON_Parse(action->ToJsonApiItem({}).c_str());
        ASSERT_NE(actionJson, nullptr);

        cJSON *attributes = cJSON_GetObjectItemCaseSensitive(actionJson, "attributes");
        ASSERT_NE(attributes, nullptr);
        cJSON *status = cJSON_GetObjectItemCaseSensitive(attributes, "status");
        ASSERT_TRUE(cJSON_IsString(status));
        EXPECT_STREQ(status->valuestring, "completed");

        cJSON *relationships = cJSON_GetObjectItemCaseSensitive(actionJson, "relationships");
        ASSERT_NE(relationships, nullptr) << "Missing relationships for type=" << tc.mDestinationType;
        cJSON *result = cJSON_GetObjectItemCaseSensitive(relationships, "result");
        ASSERT_NE(result, nullptr);
        cJSON *data = cJSON_GetObjectItemCaseSensitive(result, "data");
        ASSERT_NE(data, nullptr);
        cJSON *resultId = cJSON_GetObjectItemCaseSensitive(data, "id");
        ASSERT_TRUE(cJSON_IsString(resultId));
        std::string diagUuid = resultId->valuestring;
        EXPECT_FALSE(diagUuid.empty()) << "Empty result ID for type=" << tc.mDestinationType;
        cJSON_Delete(actionJson);

        BasicDiagnostics *diagItem = mServices->GetDiagnosticsCollection().GetItem(diagUuid);
        ASSERT_NE(diagItem, nullptr) << "Diagnostic item not found for result ID " << diagUuid;

        cJSON *diagJson = cJSON_Parse(diagItem->ToJsonApiItem({}).c_str());
        ASSERT_NE(diagJson, nullptr);
        cJSON *diagAttrs = cJSON_GetObjectItemCaseSensitive(diagJson, "attributes");
        ASSERT_NE(diagAttrs, nullptr);
        cJSON *diagExtAddr = cJSON_GetObjectItemCaseSensitive(diagAttrs, "extAddress");
        ASSERT_TRUE(cJSON_IsString(diagExtAddr));
        EXPECT_EQ(StringUtils::ToLowercase(diagExtAddr->valuestring), expectedExtAddr);
        cJSON_Delete(diagJson);
    }
}

// 2. Invalid destination inputs cannot complete successfully with an unfiltered or empty relationship.
TEST_F(RestNetworkDiagTest, InvalidDestinationInputRejectedAndCannotCompleteUnfiltered)
{
    struct InvalidCase
    {
        std::string mDestination;
        std::string mDestinationType;
        AddressType mAddressType;
    };

    std::vector<InvalidCase> invalidCases = {
        {"0123456789abcde", "extended", kAddressTypeExt},   // Too short (15 chars)
        {"0123456789abcdef0", "extended", kAddressTypeExt}, // Too long (17 chars)
        {"0123456789abcdeg", "extended", kAddressTypeExt},  // Non-hex character
        {"0x123", "rloc", kAddressTypeRloc},                // Too short
        {"0x12345", "rloc", kAddressTypeRloc},              // Too long
        {"011234", "rloc", kAddressTypeRloc},               // Invalid prefix (neither 0x nor 00)
        {"0x123g", "rloc", kAddressTypeRloc},               // Non-hex character
        {"0123456789abcde", "mleid", kAddressTypeMleid},    // Too short
        {"0123456789abcdeg", "mleid", kAddressTypeMleid},   // Non-hex character
    };

    // Seed mDiagSet with a valid finished diagnostic entry to prove that an invalid input
    // cannot accidentally match or dump unfiltered diagnostics.
    const auto          now     = std::chrono::steady_clock::now();
    const otExtAddress *extAddr = otLinkGetExtendedAddress(GetInstance());
    const otIp6Address *mlEid   = otThreadGetMeshLocalEid(GetInstance());
    uint16_t            rloc16  = otThreadGetRloc16(GetInstance());

    SetWaitingSingleRequestState(Seconds(5), now);
    GetDiagSet()[rloc16] = MakeDiagInfo(rloc16, *extAddr, mlEid, {*mlEid}, now);
    SetFinishedState();

    const size_t initialDiagCount = mServices->GetDiagnosticsCollection().Size();

    for (const auto &ic : invalidCases)
    {
        // (a) ValidateRequest must reject invalid destination strings.
        cJSON *req = BuildNetworkDiagnosticActionRequest(ic.mDestination, ic.mDestinationType);
        ASSERT_NE(req, nullptr);
        EXPECT_FALSE(mServices->GetActionsList().ValidateRequest(req))
            << "Expected ValidateRequest to reject dest=" << ic.mDestination << " type=" << ic.mDestinationType;

        // (b) Direct GetDiagnosticsStatus / FillDiagnosticCollection must return OT_ERROR_PARSE,
        // leave resultUuid empty, and not add any unfiltered item to DiagnosticsCollection.
        std::string resultUuid;
        EXPECT_EQ(GetDiagHandler().GetDiagnosticsStatus(ic.mDestination.c_str(), ic.mAddressType, resultUuid),
                  OT_ERROR_PARSE)
            << "Expected OT_ERROR_PARSE for dest=" << ic.mDestination;
        EXPECT_TRUE(resultUuid.empty());
        EXPECT_EQ(mServices->GetDiagnosticsCollection().Size(), initialDiagCount);

        // (c) Even if a NetworkDiagnostic action is constructed directly with the invalid attributes,
        // Update() must immediately fail and never produce a completed action or empty result ID.
        cJSON                     *attributes = cJSON_GetObjectItemCaseSensitive(req, "attributes");
        actions::NetworkDiagnostic action(*attributes, *mServices);
        action.Update();
        EXPECT_EQ(action.GetStatus(), actions::kActionStatusFailed);

        cJSON *actionJson = cJSON_Parse(action.ToJsonApiItem({}).c_str());
        ASSERT_NE(actionJson, nullptr);
        EXPECT_EQ(cJSON_GetObjectItemCaseSensitive(actionJson, "relationships"), nullptr);
        cJSON_Delete(actionJson);

        cJSON_Delete(req);
    }
}

// 3. Multiple candidate responses yield only the fresh response matching the requested destination;
// an earlier matching response must not cause later non-matching responses to be included.
TEST_F(RestNetworkDiagTest, MultipleCandidateResponsesSelectOnlyMatchingDestination)
{
    const auto now = std::chrono::steady_clock::now();

    otExtAddress ext1 = {{0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x01}};
    otExtAddress ext2 = {{0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x02}};
    otExtAddress ext3 = {{0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x03}};

    const otIp6NetworkPrefix *mlPrefix = otThreadGetMeshLocalPrefix(GetInstance());
    ASSERT_NE(mlPrefix, nullptr);

    auto makeMlEid = [mlPrefix](const char *aIidHex) {
        otIp6Address addr = {};
        memcpy(addr.mFields.m8, mlPrefix->m8, sizeof(mlPrefix->m8));
        EXPECT_EQ(str_to_m8(&addr.mFields.m8[8], aIidHex, OT_IP6_IID_SIZE), OT_ERROR_NONE);
        return addr;
    };

    otIp6Address peer1     = makeMlEid("a1a1a1a1a1a10001");
    otIp6Address peer2     = makeMlEid("b2b2b2b2b2b20002");
    otIp6Address peer3     = makeMlEid("c3c3c3c3c3c30003");
    otIp6Address peer3Rloc = makeMlEid("000000fffe000c00");

    auto populateCandidates = [&](const otIp6Address *aCandidate3PeerAddr = nullptr) {
        GetDiagSet().clear();
        SetWaitingSingleRequestState(Seconds(5), now);
        GetDiagSet()[0x0400] = MakeDiagInfo(0x0400, ext1, &peer1, {peer1}, now);
        GetDiagSet()[0x0800] = MakeDiagInfo(0x0800, ext2, &peer2, {peer2}, now);
        // Candidate 3 exercises the IP6_ADDR_LIST TLV fallback for mleid.
        GetDiagSet()[0x0c00] = MakeDiagInfo(0x0c00, ext3, aCandidate3PeerAddr, {peer3Rloc, peer3}, now);
        SetFinishedState();
    };

    auto verifySelectedDiag = [&](const char *aDest, AddressType aType, const std::string &aExpectedExtAddr,
                                  const otIp6Address *aCandidate3PeerAddr = nullptr) {
        populateCandidates(aCandidate3PeerAddr);
        const size_t beforeSize = mServices->GetDiagnosticsCollection().Size();
        std::string  resultUuid;

        EXPECT_EQ(GetDiagHandler().GetDiagnosticsStatus(aDest, aType, resultUuid), OT_ERROR_NONE)
            << "Failed for dest=" << aDest;
        ASSERT_FALSE(resultUuid.empty());
        // Exactly one diagnostic item must be added (later non-matching entries must not also be added).
        EXPECT_EQ(mServices->GetDiagnosticsCollection().Size(), beforeSize + 1);

        BasicDiagnostics *item = mServices->GetDiagnosticsCollection().GetItem(resultUuid);
        ASSERT_NE(item, nullptr);

        cJSON *json = cJSON_Parse(item->ToJsonApiItem({}).c_str());
        ASSERT_NE(json, nullptr);
        cJSON *attrs   = cJSON_GetObjectItemCaseSensitive(json, "attributes");
        cJSON *extItem = cJSON_GetObjectItemCaseSensitive(attrs, "extAddress");
        ASSERT_TRUE(cJSON_IsString(extItem));
        EXPECT_EQ(StringUtils::ToLowercase(extItem->valuestring), aExpectedExtAddr);
        cJSON_Delete(json);
    };

    // Match the first entry (0x0400) so later entries (0x0800, 0x0c00) are iterated after a match:
    verifySelectedDiag("1111111111111101", kAddressTypeExt, "1111111111111101");
    verifySelectedDiag("0x0400", kAddressTypeRloc, "1111111111111101");
    verifySelectedDiag("a1a1a1a1a1a10001", kAddressTypeMleid, "1111111111111101");

    // Match the middle entry (0x0800):
    verifySelectedDiag("2222222222222202", kAddressTypeExt, "2222222222222202");
    verifySelectedDiag("000800", kAddressTypeRloc, "2222222222222202");
    verifySelectedDiag("b2b2b2b2b2b20002", kAddressTypeMleid, "2222222222222202");

    // Match the last entry (0x0c00, including IP6_ADDR_LIST fallback for mleid with empty or RLOC mPeerAddr):
    verifySelectedDiag("3333333333333303", kAddressTypeExt, "3333333333333303");
    verifySelectedDiag("0x0c00", kAddressTypeRloc, "3333333333333303");
    verifySelectedDiag("c3c3c3c3c3c30003", kAddressTypeMleid, "3333333333333303", nullptr);
    verifySelectedDiag("c3c3c3c3c3c30003", kAddressTypeMleid, "3333333333333303", &peer3Rloc);
}

// 4. Retained diagnostics from a previous task are not returned as the result of a new RLOC or MLEID task.
TEST_F(RestNetworkDiagTest, RetainedDiagnosticsFromPreviousTaskNotReturnedForNewTask)
{
    const auto          staleTime = std::chrono::steady_clock::now() - std::chrono::seconds(10);
    const otExtAddress *extAddr   = otLinkGetExtendedAddress(GetInstance());
    const otIp6Address *mlEid     = otThreadGetMeshLocalEid(GetInstance());
    uint16_t            rloc16    = otThreadGetRloc16(GetInstance());

    // Populate retained state from a previous task.
    GetDiagSet()[rloc16] = MakeDiagInfo(rloc16, *extAddr, mlEid, {*mlEid}, staleTime);
    SeedRetainedSubTables(rloc16);

    // Starting a new request must clear retained diagnostic state from the previous task.
    otIp6Address unreachableAddr = {};
    EXPECT_EQ(otIp6AddressFromString("fdde:ad00:beef:0:0:ff:fe00:2000", &unreachableAddr), OT_ERROR_NONE);
    uint8_t typeList[] = {OT_NETWORK_DIAGNOSTIC_TLV_EXT_ADDRESS, OT_NETWORK_DIAGNOSTIC_TLV_SHORT_ADDRESS};

    EXPECT_EQ(GetDiagHandler().StartDiagnosticsRequest(unreachableAddr, typeList, sizeof(typeList), Seconds(1)),
              OT_ERROR_NONE);
    EXPECT_TRUE(GetDiagSet().empty());
    EXPECT_TRUE(GetChildTables().empty());
    EXPECT_TRUE(GetChildIps().empty());
    EXPECT_TRUE(GetRouterNeighbors().empty());

    GetDiagHandler().StopDiagnosticsRequest();

    // Even if a stale entry (mStartTime < mRequestStartTime) is in mDiagSet when a new RLOC or MLEID task finishes,
    // FillDiagnosticCollection must ignore it rather than returning stale diagnostics.
    const auto newRequestStart = std::chrono::steady_clock::now();
    SetWaitingSingleRequestState(Seconds(5), newRequestStart);
    GetDiagSet()[rloc16] = MakeDiagInfo(rloc16, *extAddr, mlEid, {*mlEid}, staleTime);
    SetFinishedState();

    std::string resultUuid;
    EXPECT_EQ(GetDiagHandler().GetDiagnosticsStatus(GetSelfRlocHexString().c_str(), kAddressTypeRloc, resultUuid),
              OT_ERROR_NOT_FOUND);
    EXPECT_TRUE(resultUuid.empty());

    EXPECT_EQ(GetDiagHandler().GetDiagnosticsStatus(GetSelfMlEidIidString().c_str(), kAddressTypeMleid, resultUuid),
              OT_ERROR_NOT_FOUND);
    EXPECT_TRUE(resultUuid.empty());
}

// 5. No-response and no-match cases do not produce a successful completed action with an empty result ID.
TEST_F(RestNetworkDiagTest, NoResponseAndNoMatchFailWithoutEmptyResultId)
{
    // Case A: No-response timeout on a single-target diagnostic request.
    {
        cJSON *req = BuildNetworkDiagnosticActionRequest("0x2000", "rloc", /* aTimeoutSec */ 1);
        ASSERT_NE(req, nullptr);
        cJSON *attributes = cJSON_GetObjectItemCaseSensitive(req, "attributes");

        actions::NetworkDiagnostic action(*attributes, *mServices);
        action.Update();
        EXPECT_EQ(action.GetStatus(), actions::kActionStatusActive);

        // Simulate the diagnostic request timing out with no responses in mDiagSet.
        const auto pastStart = std::chrono::steady_clock::now() - std::chrono::seconds(5);
        SetWaitingSingleRequestState(Seconds(1), pastStart);
        GetDiagSet().clear();
        ProcessDiagHandler();

        action.Update();
        EXPECT_EQ(action.GetStatus(), actions::kActionStatusFailed);

        cJSON *actionJson = cJSON_Parse(action.ToJsonApiItem({}).c_str());
        ASSERT_NE(actionJson, nullptr);
        cJSON *jsonAttrs  = cJSON_GetObjectItemCaseSensitive(actionJson, "attributes");
        cJSON *statusItem = cJSON_GetObjectItemCaseSensitive(jsonAttrs, "status");
        ASSERT_TRUE(cJSON_IsString(statusItem));
        EXPECT_STREQ(statusItem->valuestring, "failed");
        EXPECT_EQ(cJSON_GetObjectItemCaseSensitive(actionJson, "relationships"), nullptr);
        cJSON_Delete(actionJson);
        cJSON_Delete(req);
    }

    // Case B: Response received from a non-matching node (no-match case for both rloc and mleid).
    for (const auto &pair :
         std::vector<std::pair<std::string, std::string>>{{"0x2000", "rloc"}, {"deadbeefdeadbeef", "mleid"}})
    {
        cJSON *req = BuildNetworkDiagnosticActionRequest(pair.first, pair.second, /* aTimeoutSec */ 5);
        ASSERT_NE(req, nullptr);
        cJSON *attributes = cJSON_GetObjectItemCaseSensitive(req, "attributes");

        actions::NetworkDiagnostic action(*attributes, *mServices);
        action.Update();
        EXPECT_EQ(action.GetStatus(), actions::kActionStatusActive);

        // Populate a fresh response from a different node (self node) and mark handler finished.
        const auto          now     = std::chrono::steady_clock::now();
        const otExtAddress *extAddr = otLinkGetExtendedAddress(GetInstance());
        const otIp6Address *mlEid   = otThreadGetMeshLocalEid(GetInstance());
        uint16_t            rloc16  = otThreadGetRloc16(GetInstance());

        GetDiagSet().clear();
        GetDiagSet()[rloc16] = MakeDiagInfo(rloc16, *extAddr, mlEid, {*mlEid}, now);
        SetFinishedState();

        action.Update();
        EXPECT_EQ(action.GetStatus(), actions::kActionStatusFailed)
            << "Expected failed status when no response matches for type=" << pair.second;

        cJSON *actionJson = cJSON_Parse(action.ToJsonApiItem({}).c_str());
        ASSERT_NE(actionJson, nullptr);
        cJSON *jsonAttrs  = cJSON_GetObjectItemCaseSensitive(actionJson, "attributes");
        cJSON *statusItem = cJSON_GetObjectItemCaseSensitive(jsonAttrs, "status");
        ASSERT_TRUE(cJSON_IsString(statusItem));
        EXPECT_STREQ(statusItem->valuestring, "failed");
        EXPECT_EQ(cJSON_GetObjectItemCaseSensitive(actionJson, "relationships"), nullptr);
        cJSON_Delete(actionJson);
        cJSON_Delete(req);
    }
}

} // namespace rest
} // namespace otbr
