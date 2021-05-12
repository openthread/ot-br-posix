/*
 *    Copyright (c) 2020, The OpenThread Authors.
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

#define OTBR_LOG_TAG "DBUS"

#include <assert.h>
#include <string.h>

#include <openthread/border_router.h>
#include <openthread/channel_monitor.h>
#include <openthread/dataset.h>
#include <openthread/instance.h>
#include <openthread/joiner.h>
#include <openthread/link_raw.h>
#include <openthread/ncp.h>
#include <openthread/netdata.h>
#include <openthread/thread_ftd.h>
#include <openthread/platform/radio.h>

#include "common/byteswap.hpp"
#include "dbus/common/constants.hpp"
#include "dbus/server/dbus_agent.hpp"
#include "dbus/server/dbus_thread_object.hpp"

#if OTBR_ENABLE_LEGACY
#include <ot-legacy-pairing-ext.h>
#endif

using std::placeholders::_1;
using std::placeholders::_2;

static std::string GetDeviceRoleName(otDeviceRole aRole)
{
    std::string roleName;

    switch (aRole)
    {
    case OT_DEVICE_ROLE_DISABLED:
        roleName = OTBR_ROLE_NAME_DISABLED;
        break;
    case OT_DEVICE_ROLE_DETACHED:
        roleName = OTBR_ROLE_NAME_DETACHED;
        break;
    case OT_DEVICE_ROLE_CHILD:
        roleName = OTBR_ROLE_NAME_CHILD;
        break;
    case OT_DEVICE_ROLE_ROUTER:
        roleName = OTBR_ROLE_NAME_ROUTER;
        break;
    case OT_DEVICE_ROLE_LEADER:
        roleName = OTBR_ROLE_NAME_LEADER;
        break;
    }

    return roleName;
}

static uint64_t ConvertOpenThreadUint64(const uint8_t *aValue)
{
    uint64_t val = 0;

    for (size_t i = 0; i < sizeof(uint64_t); i++)
    {
        val = (val << 8) | aValue[i];
    }
    return val;
}

static otExtendedPanId Uint64ToExtendedPanId(uint64_t aExtPanId)
{
    otExtendedPanId extPanId;

    aExtPanId = htobe64(aExtPanId);
    memcpy(&extPanId.m8[0], &aExtPanId, sizeof(aExtPanId));

    return extPanId;
}

namespace otbr {
namespace DBus {

DBusThreadObject::DBusThreadObject(DBusConnection *                 aConnection,
                                   const std::string &              aInterfaceName,
                                   otbr::Ncp::ControllerOpenThread *aNcp)
    : DBusObject(aConnection, OTBR_DBUS_OBJECT_PREFIX + aInterfaceName)
    , mNcp(aNcp)
{
}

otbrError DBusThreadObject::Init(void)
{
    otbrError error        = DBusObject::Init();
    auto      threadHelper = mNcp->GetThreadHelper();

    threadHelper->AddDeviceRoleHandler(std::bind(&DBusThreadObject::DeviceRoleHandler, this, _1));
    mNcp->RegisterResetHandler(std::bind(&DBusThreadObject::NcpResetHandler, this));

    RegisterMethod(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_SCAN_METHOD,
                   std::bind(&DBusThreadObject::ScanHandler, this, _1));
    RegisterMethod(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_ATTACH_METHOD,
                   std::bind(&DBusThreadObject::AttachHandler, this, _1));
    RegisterMethod(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_DETACH_METHOD,
                   std::bind(&DBusThreadObject::DetachHandler, this, _1));
    RegisterMethod(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_FACTORY_RESET_METHOD,
                   std::bind(&DBusThreadObject::FactoryResetHandler, this, _1));
    RegisterMethod(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_RESET_METHOD,
                   std::bind(&DBusThreadObject::ResetHandler, this, _1));
    RegisterMethod(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_JOINER_START_METHOD,
                   std::bind(&DBusThreadObject::JoinerStartHandler, this, _1));
    RegisterMethod(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_JOINER_STOP_METHOD,
                   std::bind(&DBusThreadObject::JoinerStopHandler, this, _1));
    RegisterMethod(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PERMIT_UNSECURE_JOIN_METHOD,
                   std::bind(&DBusThreadObject::PermitUnsecureJoinHandler, this, _1));
    RegisterMethod(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_ADD_ON_MESH_PREFIX_METHOD,
                   std::bind(&DBusThreadObject::AddOnMeshPrefixHandler, this, _1));
    RegisterMethod(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_REMOVE_ON_MESH_PREFIX_METHOD,
                   std::bind(&DBusThreadObject::RemoveOnMeshPrefixHandler, this, _1));
    RegisterMethod(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_ADD_EXTERNAL_ROUTE_METHOD,
                   std::bind(&DBusThreadObject::AddExternalRouteHandler, this, _1));
    RegisterMethod(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_REMOVE_EXTERNAL_ROUTE_METHOD,
                   std::bind(&DBusThreadObject::RemoveExternalRouteHandler, this, _1));

    RegisterMethod(DBUS_INTERFACE_INTROSPECTABLE, DBUS_INTROSPECT_METHOD,
                   std::bind(&DBusThreadObject::IntrospectHandler, this, _1));

    RegisterSetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_MESH_LOCAL_PREFIX,
                               std::bind(&DBusThreadObject::SetMeshLocalPrefixHandler, this, _1));
    RegisterSetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_LEGACY_ULA_PREFIX,
                               std::bind(&DBusThreadObject::SetLegacyUlaPrefixHandler, this, _1));
    RegisterSetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_LINK_MODE,
                               std::bind(&DBusThreadObject::SetLinkModeHandler, this, _1));
    RegisterSetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_ACTIVE_DATASET_TLVS,
                               std::bind(&DBusThreadObject::SetActiveDatasetTlvsHandler, this, _1));
    RegisterSetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_RADIO_REGION,
                               std::bind(&DBusThreadObject::SetRadioRegionHandler, this, _1));

    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_LINK_MODE,
                               std::bind(&DBusThreadObject::GetLinkModeHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_DEVICE_ROLE,
                               std::bind(&DBusThreadObject::GetDeviceRoleHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_NETWORK_NAME,
                               std::bind(&DBusThreadObject::GetNetworkNameHandler, this, _1));

    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_PANID,
                               std::bind(&DBusThreadObject::GetPanIdHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_EXTPANID,
                               std::bind(&DBusThreadObject::GetExtPanIdHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_CHANNEL,
                               std::bind(&DBusThreadObject::GetChannelHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_MASTER_KEY,
                               std::bind(&DBusThreadObject::GetMasterKeyHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_CCA_FAILURE_RATE,
                               std::bind(&DBusThreadObject::GetCcaFailureRateHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_LINK_COUNTERS,
                               std::bind(&DBusThreadObject::GetLinkCountersHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_IP6_COUNTERS,
                               std::bind(&DBusThreadObject::GetIp6CountersHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_SUPPORTED_CHANNEL_MASK,
                               std::bind(&DBusThreadObject::GetSupportedChannelMaskHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_RLOC16,
                               std::bind(&DBusThreadObject::GetRloc16Handler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_EXTENDED_ADDRESS,
                               std::bind(&DBusThreadObject::GetExtendedAddressHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_ROUTER_ID,
                               std::bind(&DBusThreadObject::GetRouterIdHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_LEADER_DATA,
                               std::bind(&DBusThreadObject::GetLeaderDataHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_NETWORK_DATA_PRPOERTY,
                               std::bind(&DBusThreadObject::GetNetworkDataHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_STABLE_NETWORK_DATA_PRPOERTY,
                               std::bind(&DBusThreadObject::GetStableNetworkDataHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_LOCAL_LEADER_WEIGHT,
                               std::bind(&DBusThreadObject::GetLocalLeaderWeightHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_CHANNEL_MONITOR_SAMPLE_COUNT,
                               std::bind(&DBusThreadObject::GetChannelMonitorSampleCountHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_CHANNEL_MONITOR_ALL_CHANNEL_QUALITIES,
                               std::bind(&DBusThreadObject::GetChannelMonitorAllChannelQualities, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_CHILD_TABLE,
                               std::bind(&DBusThreadObject::GetChildTableHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_NEIGHBOR_TABLE_PROEPRTY,
                               std::bind(&DBusThreadObject::GetNeighborTableHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_PARTITION_ID_PROEPRTY,
                               std::bind(&DBusThreadObject::GetPartitionIDHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_INSTANT_RSSI,
                               std::bind(&DBusThreadObject::GetInstantRssiHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_RADIO_TX_POWER,
                               std::bind(&DBusThreadObject::GetRadioTxPowerHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_EXTERNAL_ROUTES,
                               std::bind(&DBusThreadObject::GetExternalRoutesHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_ACTIVE_DATASET_TLVS,
                               std::bind(&DBusThreadObject::GetActiveDatasetTlvsHandler, this, _1));
    RegisterGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_RADIO_REGION,
                               std::bind(&DBusThreadObject::GetRadioRegionHandler, this, _1));

    return error;
}

void DBusThreadObject::DeviceRoleHandler(otDeviceRole aDeviceRole)
{
    otbrLogInfo("Handle Device Role, Role:%s", GetDeviceRoleName(aDeviceRole).c_str());
    SignalPropertyChanged(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_DEVICE_ROLE, GetDeviceRoleName(aDeviceRole));
}

void DBusThreadObject::NcpResetHandler(void)
{
    mNcp->GetThreadHelper()->AddDeviceRoleHandler(std::bind(&DBusThreadObject::DeviceRoleHandler, this, _1));
    SignalPropertyChanged(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_DEVICE_ROLE,
                          GetDeviceRoleName(OT_DEVICE_ROLE_DISABLED));
}

void DBusThreadObject::ScanHandler(DBusRequest &aRequest)
{
    auto threadHelper = mNcp->GetThreadHelper();

    otbrLogInfo("Handle Scan");
    threadHelper->Scan(std::bind(&DBusThreadObject::ReplyScanResult, this, aRequest, _1, _2));
}

void DBusThreadObject::ReplyScanResult(DBusRequest &                          aRequest,
                                       otError                                aError,
                                       const std::vector<otActiveScanResult> &aResult)
{
    std::vector<ActiveScanResult> results;

    if (aError != OT_ERROR_NONE)
    {
        aRequest.ReplyOtResult(aError);
    }
    else
    {
        otbrLogInfo("ScanResult:");

        for (const auto &r : aResult)
        {
            ActiveScanResult result;

            result.mExtAddress    = ConvertOpenThreadUint64(r.mExtAddress.m8);
            result.mExtendedPanId = ConvertOpenThreadUint64(r.mExtendedPanId.m8);
            result.mNetworkName   = r.mNetworkName.m8;
            result.mSteeringData =
                std::vector<uint8_t>(r.mSteeringData.m8, r.mSteeringData.m8 + r.mSteeringData.mLength);
            result.mPanId         = r.mPanId;
            result.mJoinerUdpPort = r.mJoinerUdpPort;
            result.mChannel       = r.mChannel;
            result.mRssi          = r.mRssi;
            result.mLqi           = r.mLqi;
            result.mVersion       = r.mVersion;
            result.mIsNative      = r.mIsNative;
            result.mIsJoinable    = r.mIsJoinable;

            otbrLogInfo("NetworkName:%-16s, ExtPanId:0x%s, PanId:0x%04x, ExtAddress:%s, Channel:%2u: "
                        "Rssi:%3u, Lqi:%3u, Version:%u , IsNative:%u, IsJoinable:%u",
                        r.mNetworkName.m8, otbr::ExtPanId(r.mExtendedPanId).ToString().c_str(), r.mPanId,
                        otbr::ExtAddress(r.mExtAddress).ToString().c_str(), r.mChannel, r.mRssi, r.mLqi, r.mVersion,
                        r.mIsNative, r.mIsJoinable);

            results.emplace_back(result);
        }

        aRequest.Reply(std::tie(results));
    }
}

void DBusThreadObject::AttachHandler(DBusRequest &aRequest)
{
    auto                 threadHelper = mNcp->GetThreadHelper();
    std::string          name;
    uint16_t             panid;
    uint64_t             extPanId;
    std::vector<uint8_t> masterKey;
    std::vector<uint8_t> pskc;
    uint32_t             channelMask;

    auto args = std::tie(masterKey, panid, name, extPanId, pskc, channelMask);

    if (IsDBusMessageEmpty(*aRequest.GetMessage()))
    {
        threadHelper->Attach([aRequest](otError aError) mutable { aRequest.ReplyOtResult(aError); });
    }
    else if (DBusMessageToTuple(*aRequest.GetMessage(), args) != OTBR_ERROR_NONE)
    {
        aRequest.ReplyOtResult(OT_ERROR_INVALID_ARGS);
    }
    else
    {
        otbrLogInfo("Handle Attach, NetworkName:%s, PanId:0x%04x, ExtPanId:0x%s, MaskerKey:[Hiden], Pskc:[Hiden], "
                    "ChannelMask:0x%08x",
                    name.c_str(), panid, otbr::ExtPanId(Uint64ToExtendedPanId(extPanId)).ToString().c_str(),
                    channelMask);

        threadHelper->Attach(name, panid, extPanId, masterKey, pskc, channelMask,
                             [aRequest](otError aError) mutable { aRequest.ReplyOtResult(aError); });
    }
}

void DBusThreadObject::DetachHandler(DBusRequest &aRequest)
{
    aRequest.ReplyOtResult(mNcp->GetThreadHelper()->Detach());
}

void DBusThreadObject::FactoryResetHandler(DBusRequest &aRequest)
{
    otError error = OT_ERROR_NONE;

    otbrLogInfo("Handle Factory Reset");
    SuccessOrExit(error = mNcp->GetThreadHelper()->Detach());
    SuccessOrExit(otInstanceErasePersistentInfo(mNcp->GetThreadHelper()->GetInstance()));
    mNcp->Reset();

exit:
    aRequest.ReplyOtResult(error);
}

void DBusThreadObject::ResetHandler(DBusRequest &aRequest)
{
    otbrLogInfo("Handle Reset");

    mNcp->Reset();
    aRequest.ReplyOtResult(OT_ERROR_NONE);
}

void DBusThreadObject::JoinerStartHandler(DBusRequest &aRequest)
{
    auto        threadHelper = mNcp->GetThreadHelper();
    std::string pskd, provisionUrl, vendorName, vendorModel, vendorSwVersion, vendorData;
    auto        args = std::tie(pskd, provisionUrl, vendorName, vendorModel, vendorSwVersion, vendorData);

    if (DBusMessageToTuple(*aRequest.GetMessage(), args) != OTBR_ERROR_NONE)
    {
        aRequest.ReplyOtResult(OT_ERROR_INVALID_ARGS);
    }
    else
    {
        otbrLogInfo("Handle Joiner Start, Pskd:[Hiden], ProvisioningUrl:%s, VendorName:%s, VendorModel:%s, "
                    "VendorSwVersion:%s, VendorData:%s",
                    provisionUrl.c_str(), vendorName.c_str(), vendorModel.c_str(), vendorSwVersion.c_str(),
                    vendorData.c_str());
        threadHelper->JoinerStart(pskd, provisionUrl, vendorName, vendorModel, vendorSwVersion, vendorData,
                                  [aRequest](otError aError) mutable { aRequest.ReplyOtResult(aError); });
    }
}

void DBusThreadObject::JoinerStopHandler(DBusRequest &aRequest)
{
    auto threadHelper = mNcp->GetThreadHelper();

    otbrLogInfo("Handle Joiner Stop");
    otJoinerStop(threadHelper->GetInstance());
    aRequest.ReplyOtResult(OT_ERROR_NONE);
}

void DBusThreadObject::PermitUnsecureJoinHandler(DBusRequest &aRequest)
{
#ifdef OTBR_ENABLE_UNSECURE_JOIN
    auto     threadHelper = mNcp->GetThreadHelper();
    uint16_t port;
    uint32_t timeout;
    auto     args = std::tie(port, timeout);

    if (DBusMessageToTuple(*aRequest.GetMessage(), args) != OTBR_ERROR_NONE)
    {
        aRequest.ReplyOtResult(OT_ERROR_INVALID_ARGS);
    }
    else
    {
        otbrLogInfo("Handle Permit Unsecure Join, Port:%d, Seconds:%d", port, timeout);
        aRequest.ReplyOtResult(threadHelper->PermitUnsecureJoin(port, timeout));
    }
#else
    aRequest.ReplyOtResult(OT_ERROR_NOT_IMPLEMENTED);
#endif
}

void DBusThreadObject::AddOnMeshPrefixHandler(DBusRequest &aRequest)
{
    auto                 threadHelper = mNcp->GetThreadHelper();
    OnMeshPrefix         onMeshPrefix;
    auto                 args  = std::tie(onMeshPrefix);
    otError              error = OT_ERROR_NONE;
    otBorderRouterConfig config;

    VerifyOrExit(DBusMessageToTuple(*aRequest.GetMessage(), args) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

    // size is guaranteed by parsing
    std::copy(onMeshPrefix.mPrefix.mPrefix.begin(), onMeshPrefix.mPrefix.mPrefix.end(),
              &config.mPrefix.mPrefix.mFields.m8[0]);
    config.mPrefix.mLength = onMeshPrefix.mPrefix.mLength;
    config.mPreference     = onMeshPrefix.mPreference;
    config.mSlaac          = onMeshPrefix.mSlaac;
    config.mDhcp           = onMeshPrefix.mDhcp;
    config.mConfigure      = onMeshPrefix.mConfigure;
    config.mDefaultRoute   = onMeshPrefix.mDefaultRoute;
    config.mOnMesh         = onMeshPrefix.mOnMesh;
    config.mStable         = onMeshPrefix.mStable;

    SuccessOrExit(error = otBorderRouterAddOnMeshPrefix(threadHelper->GetInstance(), &config));
    SuccessOrExit(error = otBorderRouterRegister(threadHelper->GetInstance()));

    otbrLogInfo("Handle Add OnMesh Prefix, Prefix:%s, Preference:%u, Slaac:%u, Dhcp:%u, Configure:%u, DefaultRoute:%u, "
                "OnMesh:%u, Stable:%u",
                otbr::Ip6Prefix(config.mPrefix).ToString().c_str(), config.mPreference, config.mSlaac, config.mDhcp,
                config.mConfigure, config.mDefaultRoute, config.mOnMesh, config.mStable);

exit:
    aRequest.ReplyOtResult(error);
}

void DBusThreadObject::RemoveOnMeshPrefixHandler(DBusRequest &aRequest)
{
    auto        threadHelper = mNcp->GetThreadHelper();
    Ip6Prefix   onMeshPrefix;
    auto        args  = std::tie(onMeshPrefix);
    otError     error = OT_ERROR_NONE;
    otIp6Prefix prefix;

    VerifyOrExit(DBusMessageToTuple(*aRequest.GetMessage(), args) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);
    // size is guaranteed by parsing
    std::copy(onMeshPrefix.mPrefix.begin(), onMeshPrefix.mPrefix.end(), &prefix.mPrefix.mFields.m8[0]);
    prefix.mLength = onMeshPrefix.mLength;

    SuccessOrExit(error = otBorderRouterRemoveOnMeshPrefix(threadHelper->GetInstance(), &prefix));
    SuccessOrExit(error = otBorderRouterRegister(threadHelper->GetInstance()));

    otbrLogInfo("Handle Remove OnMesh Prefix, Prefix:%s", otbr::Ip6Prefix(prefix).ToString().c_str());

exit:
    aRequest.ReplyOtResult(error);
}

void DBusThreadObject::AddExternalRouteHandler(DBusRequest &aRequest)
{
    auto                  threadHelper = mNcp->GetThreadHelper();
    ExternalRoute         route;
    auto                  args  = std::tie(route);
    otError               error = OT_ERROR_NONE;
    otExternalRouteConfig otRoute;
    otIp6Prefix &         prefix = otRoute.mPrefix;

    VerifyOrExit(DBusMessageToTuple(*aRequest.GetMessage(), args) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

    // size is guaranteed by parsing
    std::copy(route.mPrefix.mPrefix.begin(), route.mPrefix.mPrefix.end(), &prefix.mPrefix.mFields.m8[0]);
    prefix.mLength      = route.mPrefix.mLength;
    otRoute.mPreference = route.mPreference;
    otRoute.mStable     = route.mStable;

    SuccessOrExit(error = otBorderRouterAddRoute(threadHelper->GetInstance(), &otRoute));
    if (route.mStable)
    {
        SuccessOrExit(error = otBorderRouterRegister(threadHelper->GetInstance()));
    }

    otbrLogInfo("Handle Add External Route, Prefix:%s, Preference:%u, Stable:%u",
                otbr::Ip6Prefix(prefix).ToString().c_str(), otRoute.mPreference, otRoute.mStable);

exit:
    aRequest.ReplyOtResult(error);
}

void DBusThreadObject::RemoveExternalRouteHandler(DBusRequest &aRequest)
{
    auto        threadHelper = mNcp->GetThreadHelper();
    Ip6Prefix   routePrefix;
    auto        args  = std::tie(routePrefix);
    otError     error = OT_ERROR_NONE;
    otIp6Prefix prefix;

    VerifyOrExit(DBusMessageToTuple(*aRequest.GetMessage(), args) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

    // size is guaranteed by parsing
    std::copy(routePrefix.mPrefix.begin(), routePrefix.mPrefix.end(), &prefix.mPrefix.mFields.m8[0]);
    prefix.mLength = routePrefix.mLength;

    SuccessOrExit(error = otBorderRouterRemoveRoute(threadHelper->GetInstance(), &prefix));
    SuccessOrExit(error = otBorderRouterRegister(threadHelper->GetInstance()));

    otbrLogInfo("Handle Remove External Route, Prefix:%s", otbr::Ip6Prefix(prefix).ToString().c_str());

exit:
    aRequest.ReplyOtResult(error);
}

void DBusThreadObject::IntrospectHandler(DBusRequest &aRequest)
{
    std::string xmlString(
#include "dbus/server/introspect.hpp"
    );

    aRequest.Reply(std::tie(xmlString));
}

otError DBusThreadObject::SetMeshLocalPrefixHandler(DBusMessageIter &aIter)
{
    auto                                      threadHelper = mNcp->GetThreadHelper();
    otMeshLocalPrefix                         prefix;
    std::array<uint8_t, OTBR_IP6_PREFIX_SIZE> data{};
    otError                                   error = OT_ERROR_NONE;

    VerifyOrExit(DBusMessageExtractFromVariant(&aIter, data) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);
    memcpy(&prefix.m8, &data.front(), sizeof(prefix.m8));
    error = otThreadSetMeshLocalPrefix(threadHelper->GetInstance(), &prefix);

    otbrLogInfo("Handle Set Mesh Local Prefix, Prefix:%s",
                otbr::Ip6NetworkPrefix(prefix.m8, sizeof(prefix.m8)).ToString().c_str());

exit:
    return error;
}

otError DBusThreadObject::SetLegacyUlaPrefixHandler(DBusMessageIter &aIter)
{
#if OTBR_ENABLE_LEGACY
    std::array<uint8_t, OTBR_IP6_PREFIX_SIZE> data;
    otError                                   error = OT_ERROR_NONE;

    VerifyOrExit(DBusMessageExtractFromVariant(&aIter, data) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);
    otSetLegacyUlaPrefix(&data[0]);

    otbrLogInfo("Handle Set Legacy Ula Prefix, Prefix:%s",
                otbr::Ip6NetworkPrefix(&data[0], static_cast<uint8_t>(data.size())).ToString().c_str());

exit:
    return error;
#else
    OTBR_UNUSED_VARIABLE(aIter);

    return OT_ERROR_NOT_IMPLEMENTED;
#endif // OTBR_ENABLE_LEGACY
}

otError DBusThreadObject::SetLinkModeHandler(DBusMessageIter &aIter)
{
    auto             threadHelper = mNcp->GetThreadHelper();
    LinkModeConfig   cfg;
    otLinkModeConfig otCfg;
    otError          error = OT_ERROR_NONE;

    VerifyOrExit(DBusMessageExtractFromVariant(&aIter, cfg) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);
    otCfg.mDeviceType   = cfg.mDeviceType;
    otCfg.mNetworkData  = cfg.mNetworkData;
    otCfg.mRxOnWhenIdle = cfg.mRxOnWhenIdle;
    error               = otThreadSetLinkMode(threadHelper->GetInstance(), otCfg);

    otbrLogInfo("Handle Set Link Mode, DeviceType:%u, NetworkData:%u, RxOnWhenIdle:%u", cfg.mDeviceType,
                cfg.mNetworkData, cfg.mRxOnWhenIdle);

exit:
    return error;
}

otError DBusThreadObject::GetLinkModeHandler(DBusMessageIter &aIter)
{
    auto             threadHelper = mNcp->GetThreadHelper();
    otLinkModeConfig otCfg        = otThreadGetLinkMode(threadHelper->GetInstance());
    LinkModeConfig   cfg;
    otError          error = OT_ERROR_NONE;

    cfg.mDeviceType   = otCfg.mDeviceType;
    cfg.mNetworkData  = otCfg.mNetworkData;
    cfg.mRxOnWhenIdle = otCfg.mRxOnWhenIdle;

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, cfg) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

    otbrLogInfo("Handle Get Link Mode, DeviceType:%u, NetworkData:%u, RxOnWhenIdle:%u", cfg.mDeviceType,
                cfg.mNetworkData, cfg.mRxOnWhenIdle);

exit:
    return error;
}

otError DBusThreadObject::GetDeviceRoleHandler(DBusMessageIter &aIter)
{
    auto         threadHelper = mNcp->GetThreadHelper();
    otDeviceRole role         = otThreadGetDeviceRole(threadHelper->GetInstance());
    std::string  roleName     = GetDeviceRoleName(role);
    otError      error        = OT_ERROR_NONE;

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, roleName) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

    otbrLogInfo("Handle Get Device Role, Role:%s", roleName.c_str());

exit:
    return error;
}

otError DBusThreadObject::GetNetworkNameHandler(DBusMessageIter &aIter)
{
    auto        threadHelper = mNcp->GetThreadHelper();
    std::string networkName  = otThreadGetNetworkName(threadHelper->GetInstance());
    otError     error        = OT_ERROR_NONE;

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, networkName) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

    otbrLogInfo("Handle Get Network Name, NetworkName:%s", networkName.c_str());

exit:
    return error;
}

otError DBusThreadObject::GetPanIdHandler(DBusMessageIter &aIter)
{
    auto     threadHelper = mNcp->GetThreadHelper();
    uint16_t panId        = otLinkGetPanId(threadHelper->GetInstance());
    otError  error        = OT_ERROR_NONE;

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, panId) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

    otbrLogInfo("Handle Get PanId, PanId:0x%04x", panId);

exit:
    return error;
}

otError DBusThreadObject::GetExtPanIdHandler(DBusMessageIter &aIter)
{
    auto                   threadHelper = mNcp->GetThreadHelper();
    const otExtendedPanId *extPanId     = otThreadGetExtendedPanId(threadHelper->GetInstance());
    uint64_t               extPanIdVal;
    otError                error = OT_ERROR_NONE;

    extPanIdVal = ConvertOpenThreadUint64(extPanId->m8);

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, extPanIdVal) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

    otbrLogInfo("Handle Get ExtPanId, ExtPanId:0x%s", otbr::ExtPanId(*extPanId).ToString().c_str());

exit:
    return error;
}

otError DBusThreadObject::GetChannelHandler(DBusMessageIter &aIter)
{
    auto     threadHelper = mNcp->GetThreadHelper();
    uint16_t channel      = otLinkGetChannel(threadHelper->GetInstance());
    otError  error        = OT_ERROR_NONE;

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, channel) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

    otbrLogInfo("Handle Get Channel, Channel:%u", channel);

exit:
    return error;
}

otError DBusThreadObject::GetMasterKeyHandler(DBusMessageIter &aIter)
{
    auto                 threadHelper = mNcp->GetThreadHelper();
    const otMasterKey *  masterKey    = otThreadGetMasterKey(threadHelper->GetInstance());
    std::vector<uint8_t> keyVal(masterKey->m8, masterKey->m8 + sizeof(masterKey->m8));
    otError              error = OT_ERROR_NONE;

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, keyVal) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

    otbrLogInfo("Handle Get Master Key, MasterKey:[Hiden]");

exit:
    return error;
}

otError DBusThreadObject::GetCcaFailureRateHandler(DBusMessageIter &aIter)
{
    auto     threadHelper = mNcp->GetThreadHelper();
    uint16_t failureRate  = otLinkGetCcaFailureRate(threadHelper->GetInstance());
    otError  error        = OT_ERROR_NONE;

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, failureRate) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);
    otbrLogInfo("Handle Get Cca Failure Rate, FailureRate:%u", failureRate);

exit:
    return error;
}

otError DBusThreadObject::GetLinkCountersHandler(DBusMessageIter &aIter)
{
    auto                 threadHelper = mNcp->GetThreadHelper();
    const otMacCounters *otCounters   = otLinkGetCounters(threadHelper->GetInstance());
    MacCounters          counters;
    otError              error = OT_ERROR_NONE;

    counters.mTxTotal              = otCounters->mTxTotal;
    counters.mTxUnicast            = otCounters->mTxUnicast;
    counters.mTxBroadcast          = otCounters->mTxBroadcast;
    counters.mTxAckRequested       = otCounters->mTxAckRequested;
    counters.mTxAcked              = otCounters->mTxAcked;
    counters.mTxNoAckRequested     = otCounters->mTxNoAckRequested;
    counters.mTxData               = otCounters->mTxData;
    counters.mTxDataPoll           = otCounters->mTxDataPoll;
    counters.mTxBeacon             = otCounters->mTxBeacon;
    counters.mTxBeaconRequest      = otCounters->mTxBeaconRequest;
    counters.mTxOther              = otCounters->mTxOther;
    counters.mTxRetry              = otCounters->mTxRetry;
    counters.mTxErrCca             = otCounters->mTxErrCca;
    counters.mTxErrAbort           = otCounters->mTxErrAbort;
    counters.mTxErrBusyChannel     = otCounters->mTxErrBusyChannel;
    counters.mRxTotal              = otCounters->mRxTotal;
    counters.mRxUnicast            = otCounters->mTxUnicast;
    counters.mRxBroadcast          = otCounters->mRxBroadcast;
    counters.mRxData               = otCounters->mRxData;
    counters.mRxDataPoll           = otCounters->mTxDataPoll;
    counters.mRxBeacon             = otCounters->mRxBeacon;
    counters.mRxBeaconRequest      = otCounters->mRxBeaconRequest;
    counters.mRxOther              = otCounters->mRxOther;
    counters.mRxAddressFiltered    = otCounters->mRxAddressFiltered;
    counters.mRxDestAddrFiltered   = otCounters->mRxDestAddrFiltered;
    counters.mRxDuplicated         = otCounters->mRxDuplicated;
    counters.mRxErrNoFrame         = otCounters->mRxErrNoFrame;
    counters.mRxErrUnknownNeighbor = otCounters->mRxErrUnknownNeighbor;
    counters.mRxErrInvalidSrcAddr  = otCounters->mRxErrInvalidSrcAddr;
    counters.mRxErrSec             = otCounters->mRxErrSec;
    counters.mRxErrFcs             = otCounters->mRxErrFcs;
    counters.mRxErrOther           = otCounters->mRxErrOther;

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, counters) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

    otbrLogInfo(
        "Handle Get Link Counters, TxTotal:%u, TxUnicast:%u, TxBroadcast:%u, TxAckRequested:%u, TxAcked:%u, "
        "TxNoAckRequested:%u, TxData:%u, TxDataPoll:%u, TxBeacon:%u, TxBeaconRequest:%u, TxOther:%u, TxRetry:%u, "
        "TxErrCca:%u, TxErrAbort:%u, TxErrBusyChannel:%u, RxTotal:%u, RxUnicast:%u, RxBroadcast:%u, RxData:%u, "
        "RxDataPoll:%u, RxBeacon:%u, RxBeaconRequest:%u, RxOther:%u, RxAddressFiltered:%u, RxDestAddrFiltered:%u, "
        "RxDuplicated:%u, RxErrNoFrame:%u, RxErrNoUnknownNeighbor:%u, RxErrInvalidSrcAddr:%u, RxErrSec:%u, "
        "RxErrFcs:%u, RxErrOther:%u",
        counters.mTxTotal, counters.mTxUnicast, counters.mTxBroadcast, counters.mTxAckRequested, counters.mTxAcked,
        counters.mTxNoAckRequested, counters.mTxData, counters.mTxDataPoll, counters.mTxBeacon,
        counters.mTxBeaconRequest, counters.mTxOther, counters.mTxRetry, counters.mTxErrCca, counters.mTxErrAbort,
        counters.mTxErrBusyChannel, counters.mRxTotal, counters.mTxUnicast, counters.mRxBroadcast, counters.mRxData,
        counters.mTxDataPoll, counters.mRxBeacon, counters.mRxBeaconRequest, counters.mRxOther,
        counters.mRxAddressFiltered, counters.mRxDestAddrFiltered, counters.mRxDuplicated, counters.mRxErrNoFrame,
        counters.mRxErrUnknownNeighbor, counters.mRxErrInvalidSrcAddr, counters.mRxErrSec, counters.mRxErrFcs,
        counters.mRxErrOther);
exit:
    return error;
}

otError DBusThreadObject::GetIp6CountersHandler(DBusMessageIter &aIter)
{
    auto                threadHelper = mNcp->GetThreadHelper();
    const otIpCounters *otCounters   = otThreadGetIp6Counters(threadHelper->GetInstance());
    IpCounters          counters;
    otError             error = OT_ERROR_NONE;

    counters.mTxSuccess = otCounters->mTxSuccess;
    counters.mTxFailure = otCounters->mTxFailure;
    counters.mRxSuccess = otCounters->mRxSuccess;
    counters.mRxFailure = otCounters->mRxFailure;

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, counters) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

    otbrLogInfo("Handle Get Ip6 Counters, TxSuccess:%u, TxFailure:%u, RxSuccess:%u, RxFailure:%u", counters.mTxSuccess,
                counters.mTxFailure, counters.mRxSuccess, counters.mRxFailure);

exit:
    return error;
}

otError DBusThreadObject::GetSupportedChannelMaskHandler(DBusMessageIter &aIter)
{
    auto     threadHelper = mNcp->GetThreadHelper();
    uint32_t channelMask  = otLinkGetSupportedChannelMask(threadHelper->GetInstance());
    otError  error        = OT_ERROR_NONE;

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, channelMask) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

    otbrLogInfo("Handle Get Supported Channel Mask, ChannelMask:0x%08x", channelMask);

exit:
    return error;
}

otError DBusThreadObject::GetRloc16Handler(DBusMessageIter &aIter)
{
    auto     threadHelper = mNcp->GetThreadHelper();
    otError  error        = OT_ERROR_NONE;
    uint16_t rloc16       = otThreadGetRloc16(threadHelper->GetInstance());

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, rloc16) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

    otbrLogInfo("Handle Get Rloc16, Rloc16:0x%04x", rloc16);

exit:
    return error;
}

otError DBusThreadObject::GetExtendedAddressHandler(DBusMessageIter &aIter)
{
    auto                threadHelper    = mNcp->GetThreadHelper();
    otError             error           = OT_ERROR_NONE;
    const otExtAddress *addr            = otLinkGetExtendedAddress(threadHelper->GetInstance());
    uint64_t            extendedAddress = ConvertOpenThreadUint64(addr->m8);

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, extendedAddress) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

    otbrLogInfo("Handle Get Extended Address, ExtAddr:%s", otbr::ExtAddress(*addr).ToString().c_str());

exit:
    return error;
}

otError DBusThreadObject::GetRouterIdHandler(DBusMessageIter &aIter)
{
    auto         threadHelper = mNcp->GetThreadHelper();
    otError      error        = OT_ERROR_NONE;
    uint16_t     rloc16       = otThreadGetRloc16(threadHelper->GetInstance());
    otRouterInfo info;

    VerifyOrExit(otThreadGetRouterInfo(threadHelper->GetInstance(), rloc16, &info) == OT_ERROR_NONE,
                 error = OT_ERROR_INVALID_STATE);
    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, info.mRouterId) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

    otbrLogInfo("Handle Get RouterId, RouterId:0x%02x", info.mRouterId);

exit:
    return error;
}

otError DBusThreadObject::GetLeaderDataHandler(DBusMessageIter &aIter)
{
    auto                threadHelper = mNcp->GetThreadHelper();
    otError             error        = OT_ERROR_NONE;
    struct otLeaderData data;
    LeaderData          leaderData;

    SuccessOrExit(error = otThreadGetLeaderData(threadHelper->GetInstance(), &data));
    leaderData.mPartitionId       = data.mPartitionId;
    leaderData.mWeighting         = data.mWeighting;
    leaderData.mDataVersion       = data.mDataVersion;
    leaderData.mStableDataVersion = data.mStableDataVersion;
    leaderData.mLeaderRouterId    = data.mLeaderRouterId;
    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, leaderData) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

    otbrLogInfo(
        "Handle Get Leader Data: PartitionId:%u, Weighting:%u, DataVersion:%u, StableDataVersion:%u, LeaderRouterId:%u",
        data.mPartitionId, data.mWeighting, data.mDataVersion, data.mStableDataVersion, data.mLeaderRouterId);

exit:
    return error;
}

otError DBusThreadObject::GetNetworkDataHandler(DBusMessageIter &aIter)
{
    static constexpr size_t kNetworkDataMaxSize = 255;
    auto                    threadHelper        = mNcp->GetThreadHelper();
    otError                 error               = OT_ERROR_NONE;
    uint8_t                 data[kNetworkDataMaxSize];
    uint8_t                 len = sizeof(data);
    std::vector<uint8_t>    networkData;

    SuccessOrExit(error = otNetDataGet(threadHelper->GetInstance(), /*stable=*/false, data, &len));
    networkData = std::vector<uint8_t>(&data[0], &data[len]);
    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, networkData) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

    otbrLogInfo("Handle Get Network Data");

exit:
    return error;
}

otError DBusThreadObject::GetStableNetworkDataHandler(DBusMessageIter &aIter)
{
    static constexpr size_t kNetworkDataMaxSize = 255;
    auto                    threadHelper        = mNcp->GetThreadHelper();
    otError                 error               = OT_ERROR_NONE;
    uint8_t                 data[kNetworkDataMaxSize];
    uint8_t                 len = sizeof(data);
    std::vector<uint8_t>    networkData;

    SuccessOrExit(error = otNetDataGet(threadHelper->GetInstance(), /*stable=*/true, data, &len));
    networkData = std::vector<uint8_t>(&data[0], &data[len]);
    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, networkData) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

    otbrLogInfo("Handle Get Stable Network Data");

exit:
    return error;
}

otError DBusThreadObject::GetLocalLeaderWeightHandler(DBusMessageIter &aIter)
{
    auto    threadHelper = mNcp->GetThreadHelper();
    otError error        = OT_ERROR_NONE;
    uint8_t weight       = otThreadGetLocalLeaderWeight(threadHelper->GetInstance());

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, weight) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

    otbrLogInfo("Handle Get Local Leader Weight, Weight:%u", weight);

exit:
    return error;
}

otError DBusThreadObject::GetChannelMonitorSampleCountHandler(DBusMessageIter &aIter)
{
#if OPENTHREAD_CONFIG_CHANNEL_MONITOR_ENABLE
    auto     threadHelper = mNcp->GetThreadHelper();
    otError  error        = OT_ERROR_NONE;
    uint32_t cnt          = otChannelMonitorGetSampleCount(threadHelper->GetInstance());

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, cnt) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

    otbrLogInfo("Handle Get Channel Monitor Sample Count, Count:%u", cnt);

exit:
    return error;
#else  // OPENTHREAD_CONFIG_CHANNEL_MONITOR_ENABLE
    OTBR_UNUSED_VARIABLE(aIter);
    return OT_ERROR_NOT_IMPLEMENTED;
#endif // OPENTHREAD_CONFIG_CHANNEL_MONITOR_ENABLE
}

otError DBusThreadObject::GetChannelMonitorAllChannelQualities(DBusMessageIter &aIter)
{
#if OPENTHREAD_CONFIG_CHANNEL_MONITOR_ENABLE
    auto                        threadHelper = mNcp->GetThreadHelper();
    otError                     error        = OT_ERROR_NONE;
    uint32_t                    channelMask  = otLinkGetSupportedChannelMask(threadHelper->GetInstance());
    constexpr uint8_t           kNumChannels = sizeof(channelMask) * 8; // 8 bit per byte
    std::vector<ChannelQuality> quality;

    otbrLogInfo("Handle Get Channel Monitor All Channel Qualities, ChannelMask:0x%08x", channelMask);

    for (uint8_t i = 0; i < kNumChannels; i++)
    {
        if (channelMask & (1U << i))
        {
            uint16_t occupancy = otChannelMonitorGetChannelOccupancy(threadHelper->GetInstance(), i);

            otbrLogInfo("Channel: %u, Occupancy: %u", i, occupancy);
            quality.emplace_back(ChannelQuality{i, occupancy});
        }
    }

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, quality) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

exit:
    return error;
#else  // OPENTHREAD_CONFIG_CHANNEL_MONITOR_ENABLE
    OTBR_UNUSED_VARIABLE(aIter);
    return OT_ERROR_NOT_IMPLEMENTED;
#endif // OPENTHREAD_CONFIG_CHANNEL_MONITOR_ENABLE
}

otError DBusThreadObject::GetChildTableHandler(DBusMessageIter &aIter)
{
    auto                   threadHelper = mNcp->GetThreadHelper();
    otError                error        = OT_ERROR_NONE;
    uint16_t               childIndex   = 0;
    otChildInfo            childInfo;
    std::vector<ChildInfo> childTable;

    otbrLogInfo("Handle Get Child Table");

    while (otThreadGetChildInfoByIndex(threadHelper->GetInstance(), childIndex, &childInfo) == OT_ERROR_NONE)
    {
        ChildInfo info;

        info.mExtAddress         = ConvertOpenThreadUint64(childInfo.mExtAddress.m8);
        info.mTimeout            = childInfo.mTimeout;
        info.mAge                = childInfo.mAge;
        info.mChildId            = childInfo.mChildId;
        info.mNetworkDataVersion = childInfo.mNetworkDataVersion;
        info.mLinkQualityIn      = childInfo.mLinkQualityIn;
        info.mAverageRssi        = childInfo.mAverageRssi;
        info.mLastRssi           = childInfo.mLastRssi;
        info.mFrameErrorRate     = childInfo.mFrameErrorRate;
        info.mMessageErrorRate   = childInfo.mMessageErrorRate;
        info.mRxOnWhenIdle       = childInfo.mRxOnWhenIdle;
        info.mFullThreadDevice   = childInfo.mFullThreadDevice;
        info.mFullNetworkData    = childInfo.mFullNetworkData;
        info.mIsStateRestoring   = childInfo.mIsStateRestoring;
        childTable.push_back(info);
        childIndex++;

        otbrLogInfo("%d: ExtAddress:%s, Timeout:%u, Age:%u, ChildId:0x%04x, NetworkDataVersion:%u, "
                    "LinkQualityIn:%u, AverageRssi:%d, LastRssi:%d, FrameErrorRate:%u, MessageErrorRate:%u, "
                    "RxOnWhenIdle:%u, FullThreadDevice:%u, FullNetworkData:%u, IsStateRestoring:%u",
                    childIndex, otbr::ExtAddress(childInfo.mExtAddress).ToString().c_str(), childInfo.mTimeout,
                    childInfo.mAge, childInfo.mChildId, childInfo.mNetworkDataVersion, childInfo.mLinkQualityIn,
                    childInfo.mAverageRssi, childInfo.mLastRssi, childInfo.mFrameErrorRate, childInfo.mMessageErrorRate,
                    childInfo.mRxOnWhenIdle, childInfo.mFullThreadDevice, childInfo.mFullNetworkData,
                    childInfo.mIsStateRestoring);
    }

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, childTable) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

exit:
    return error;
}

otError DBusThreadObject::GetNeighborTableHandler(DBusMessageIter &aIter)
{
    auto                      threadHelper = mNcp->GetThreadHelper();
    otError                   error        = OT_ERROR_NONE;
    otNeighborInfoIterator    iter         = OT_NEIGHBOR_INFO_ITERATOR_INIT;
    otNeighborInfo            neighborInfo;
    std::vector<NeighborInfo> neighborTable;

    otbrLogInfo("Handle Get Neighbor Table");

    while (otThreadGetNextNeighborInfo(threadHelper->GetInstance(), &iter, &neighborInfo) == OT_ERROR_NONE)
    {
        NeighborInfo info;

        info.mExtAddress       = ConvertOpenThreadUint64(neighborInfo.mExtAddress.m8);
        info.mAge              = neighborInfo.mAge;
        info.mRloc16           = neighborInfo.mRloc16;
        info.mLinkFrameCounter = neighborInfo.mLinkFrameCounter;
        info.mMleFrameCounter  = neighborInfo.mMleFrameCounter;
        info.mLinkQualityIn    = neighborInfo.mLinkQualityIn;
        info.mAverageRssi      = neighborInfo.mAverageRssi;
        info.mLastRssi         = neighborInfo.mLastRssi;
        info.mFrameErrorRate   = neighborInfo.mFrameErrorRate;
        info.mMessageErrorRate = neighborInfo.mMessageErrorRate;
        info.mRxOnWhenIdle     = neighborInfo.mRxOnWhenIdle;
        info.mFullThreadDevice = neighborInfo.mFullThreadDevice;
        info.mFullNetworkData  = neighborInfo.mFullNetworkData;
        info.mIsChild          = neighborInfo.mIsChild;
        neighborTable.push_back(info);

        otbrLogInfo("ExtAddress:%s, Age:%u, Rloc16:0x%04x, LinkFrameCounter:%u, MleFrameCounter:%u"
                    "LinkQualityIn:%u, AverageRssi:%d, LastRssi:%d, FrameErrorRate:%u, MessageErrorRate:%u, "
                    "RxOnWhenIdle:%u, "
                    "FullThreadDevice:%u, FullNetworkData:%u, IsChild:%u",
                    otbr::ExtAddress(neighborInfo.mExtAddress).ToString().c_str(), neighborInfo.mAge,
                    neighborInfo.mRloc16, neighborInfo.mLinkFrameCounter, neighborInfo.mMleFrameCounter,
                    neighborInfo.mLinkQualityIn, neighborInfo.mAverageRssi, neighborInfo.mLastRssi,
                    neighborInfo.mFrameErrorRate, neighborInfo.mMessageErrorRate, neighborInfo.mRxOnWhenIdle,
                    neighborInfo.mFullThreadDevice, neighborInfo.mFullNetworkData, neighborInfo.mIsChild);
    }

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, neighborTable) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

exit:
    return error;
}

otError DBusThreadObject::GetPartitionIDHandler(DBusMessageIter &aIter)
{
    auto     threadHelper = mNcp->GetThreadHelper();
    otError  error        = OT_ERROR_NONE;
    uint32_t partitionId  = otThreadGetPartitionId(threadHelper->GetInstance());

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, partitionId) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

    otbrLogInfo("Handle Get Partition Id, PartitionId:%u", partitionId);

exit:
    return error;
}

otError DBusThreadObject::GetInstantRssiHandler(DBusMessageIter &aIter)
{
    auto    threadHelper = mNcp->GetThreadHelper();
    otError error        = OT_ERROR_NONE;
    int8_t  rssi         = otPlatRadioGetRssi(threadHelper->GetInstance());

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, rssi) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

    otbrLogInfo("Handle Get Instant Rssi, Rssi:%d", rssi);

exit:
    return error;
}

otError DBusThreadObject::GetRadioTxPowerHandler(DBusMessageIter &aIter)
{
    auto    threadHelper = mNcp->GetThreadHelper();
    otError error        = OT_ERROR_NONE;
    int8_t  txPower;

    SuccessOrExit(error = otPlatRadioGetTransmitPower(threadHelper->GetInstance(), &txPower));

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, txPower) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

    otbrLogInfo("Handle Get Radio Tx Power, TxPower:%d", txPower);

exit:
    return error;
}

otError DBusThreadObject::GetExternalRoutesHandler(DBusMessageIter &aIter)
{
    auto                       threadHelper = mNcp->GetThreadHelper();
    otError                    error        = OT_ERROR_NONE;
    otNetworkDataIterator      iter         = OT_NETWORK_DATA_ITERATOR_INIT;
    otExternalRouteConfig      config;
    std::vector<ExternalRoute> externalRouteTable;

    otbrLogInfo("Handle Get External Routes");

    while (otNetDataGetNextRoute(threadHelper->GetInstance(), &iter, &config) == OT_ERROR_NONE)
    {
        ExternalRoute route;

        route.mPrefix.mPrefix      = std::vector<uint8_t>(&config.mPrefix.mPrefix.mFields.m8[0],
                                                     &config.mPrefix.mPrefix.mFields.m8[OTBR_IP6_PREFIX_SIZE]);
        route.mPrefix.mLength      = config.mPrefix.mLength;
        route.mRloc16              = config.mRloc16;
        route.mPreference          = config.mPreference;
        route.mStable              = config.mStable;
        route.mNextHopIsThisDevice = config.mNextHopIsThisDevice;
        externalRouteTable.push_back(route);

        otbrLogInfo("Prefix:%s, Rloc16:0x%04x, Preference:%u, Stable:%u, NextHopIsThisDevice:%u",
                    otbr::Ip6Prefix(config.mPrefix).ToString().c_str(), config.mRloc16, config.mPreference,
                    config.mStable, config.mNextHopIsThisDevice);
    }

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, externalRouteTable) == OTBR_ERROR_NONE,
                 error = OT_ERROR_INVALID_ARGS);

exit:
    return error;
}

otError DBusThreadObject::SetActiveDatasetTlvsHandler(DBusMessageIter &aIter)
{
    auto                     threadHelper = mNcp->GetThreadHelper();
    std::vector<uint8_t>     data;
    otOperationalDatasetTlvs datasetTlvs;
    otError                  error = OT_ERROR_NONE;

    VerifyOrExit(DBusMessageExtractFromVariant(&aIter, data) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);
    VerifyOrExit(data.size() <= sizeof(datasetTlvs.mTlvs));
    std::copy(std::begin(data), std::end(data), std::begin(datasetTlvs.mTlvs));
    datasetTlvs.mLength = data.size();
    error               = otDatasetSetActiveTlvs(threadHelper->GetInstance(), &datasetTlvs);

    otbrLogInfo("Handle Set Active Dataset");

exit:
    return error;
}

otError DBusThreadObject::GetActiveDatasetTlvsHandler(DBusMessageIter &aIter)
{
    auto                     threadHelper = mNcp->GetThreadHelper();
    otError                  error        = OT_ERROR_NONE;
    std::vector<uint8_t>     data;
    otOperationalDatasetTlvs datasetTlvs;

    SuccessOrExit(error = otDatasetGetActiveTlvs(threadHelper->GetInstance(), &datasetTlvs));
    data = std::vector<uint8_t>{std::begin(datasetTlvs.mTlvs), std::begin(datasetTlvs.mTlvs) + datasetTlvs.mLength};

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, data) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

    otbrLogInfo("Handle Get Active Dataset");

exit:
    return error;
}

otError DBusThreadObject::SetRadioRegionHandler(DBusMessageIter &aIter)
{
    auto        threadHelper = mNcp->GetThreadHelper();
    std::string radioRegion;
    uint16_t    regionCode;
    otError     error = OT_ERROR_NONE;

    VerifyOrExit(DBusMessageExtractFromVariant(&aIter, radioRegion) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);
    VerifyOrExit(radioRegion.size() == sizeof(uint16_t), error = OT_ERROR_INVALID_ARGS);
    regionCode = radioRegion[0] << 8 | radioRegion[1];

    error = otPlatRadioSetRegion(threadHelper->GetInstance(), regionCode);

    otbrLogInfo("Handle Set Radio Region, Region:%s", radioRegion.c_str());

exit:
    return error;
}

otError DBusThreadObject::GetRadioRegionHandler(DBusMessageIter &aIter)
{
    auto        threadHelper = mNcp->GetThreadHelper();
    otError     error        = OT_ERROR_NONE;
    std::string radioRegion;
    uint16_t    regionCode;

    SuccessOrExit(error = otPlatRadioGetRegion(threadHelper->GetInstance(), &regionCode));
    radioRegion.resize(sizeof(uint16_t), '\0');
    radioRegion[0] = static_cast<char>((regionCode >> 8) & 0xff);
    radioRegion[1] = static_cast<char>(regionCode & 0xff);

    VerifyOrExit(DBusMessageEncodeToVariant(&aIter, radioRegion) == OTBR_ERROR_NONE, error = OT_ERROR_INVALID_ARGS);

    otbrLogInfo("Handle Get Radio Region, Region:%s", radioRegion.c_str());

exit:
    return error;
}

} // namespace DBus
} // namespace otbr
