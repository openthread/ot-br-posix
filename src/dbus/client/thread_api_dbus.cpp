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

#include <map>
#include <string.h>

#include "common/code_utils.hpp"
#include "dbus/client/client_error.hpp"
#include "dbus/client/thread_api_dbus.hpp"
#include "dbus/common/constants.hpp"
#include "dbus/common/dbus_message_helper.hpp"
#include "dbus/common/dbus_resources.hpp"

namespace otbr {
namespace DBus {

static ClientError NameToDeviceRole(const std::string &aRoleName, DeviceRole &aDeviceRole)
{
    static std::pair<const char *, DeviceRole> sRoleMap[] = {
        {OTBR_ROLE_NAME_DISABLED, OTBR_DEVICE_ROLE_DISABLED}, {OTBR_ROLE_NAME_DETACHED, OTBR_DEVICE_ROLE_DETACHED},
        {OTBR_ROLE_NAME_CHILD, OTBR_DEVICE_ROLE_CHILD},       {OTBR_ROLE_NAME_ROUTER, OTBR_DEVICE_ROLE_ROUTER},
        {OTBR_ROLE_NAME_LEADER, OTBR_DEVICE_ROLE_LEADER},
    };
    ClientError error = ClientError::OT_ERROR_NOT_FOUND;

    for (const auto &p : sRoleMap)
    {
        if (p.first == aRoleName)
        {
            aDeviceRole = p.second;
            error       = ClientError::ERROR_NONE;
            break;
        }
    }

    return error;
}

bool IsThreadActive(DeviceRole aRole)
{
    bool isActive = false;

    switch (aRole)
    {
    case OTBR_DEVICE_ROLE_DISABLED:
    case OTBR_DEVICE_ROLE_DETACHED:
        isActive = false;
        break;
    case OTBR_DEVICE_ROLE_CHILD:
    case OTBR_DEVICE_ROLE_ROUTER:
    case OTBR_DEVICE_ROLE_LEADER:
        isActive = true;
        break;
    }

    return isActive;
}

ThreadApiDBus::ThreadApiDBus(DBusConnection *aConnection)
    : mInterfaceName("wpan0")
    , mConnection(aConnection)
{
    SubscribeSignals();
}

ThreadApiDBus::ThreadApiDBus(DBusConnection *aConnection, const std::string &aInterfaceName)
    : mInterfaceName(aInterfaceName)
    , mConnection(aConnection)
{
    SubscribeSignals();
}

ClientError ThreadApiDBus::SubscribeSignals(void)
{
    std::string propertyMatchRule = "type='signal',interface='" DBUS_INTERFACE_PROPERTIES "'";
    std::string threadMatchRule   = "type='signal',interface='" OTBR_DBUS_THREAD_INTERFACE "'";
    DBusError   error;
    ClientError ret = ClientError::ERROR_NONE;

    dbus_error_init(&error);
    dbus_bus_add_match(mConnection, propertyMatchRule.c_str(), &error);
    VerifyOrExit(!dbus_error_is_set(&error), ret = ClientError::OT_ERROR_FAILED);
    dbus_bus_add_match(mConnection, threadMatchRule.c_str(), &error);
    VerifyOrExit(!dbus_error_is_set(&error), ret = ClientError::OT_ERROR_FAILED);
    dbus_connection_add_filter(mConnection, sDBusMessageFilter, this, nullptr);

exit:
    dbus_error_free(&error);
    return ret;
}

DBusHandlerResult ThreadApiDBus::sDBusMessageFilter(DBusConnection *aConnection,
                                                    DBusMessage *   aMessage,
                                                    void *          aThreadApiDBus)
{
    ThreadApiDBus *api = static_cast<ThreadApiDBus *>(aThreadApiDBus);

    return api->DBusMessageFilter(aConnection, aMessage);
}

DBusHandlerResult ThreadApiDBus::DBusMessageFilter(DBusConnection *aConnection, DBusMessage *aMessage)
{
    (void)aConnection;

    DBusHandlerResult result = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    if (dbus_message_is_signal(aMessage, DBUS_INTERFACE_PROPERTIES, DBUS_PROPERTIES_CHANGED_SIGNAL))
    {
        result = HandlePropertySignal(aMessage);
    }
    else if (dbus_message_is_signal(aMessage, OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_COMMISSIONER_STATE_SIGNAL))
    {
        result = HandleCommissionerStateSignal(aMessage);
    }
    else if (dbus_message_is_signal(aMessage, OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_JOINER_EVENT_SIGNAL))
    {
        result = HandleJoinerEventSignal(aMessage);
    }

    return result;
}

DBusHandlerResult ThreadApiDBus::HandlePropertySignal(DBusMessage *aMessage)
{
    DBusMessageIter iter, subIter, dictEntryIter, valIter;
    DeviceRole      role = OTBR_DEVICE_ROLE_DISABLED;
    std::string     interfaceName, propertyName, val;

    VerifyOrExit(dbus_message_iter_init(aMessage, &iter));
    SuccessOrExit(DBusMessageExtract(&iter, interfaceName));
    VerifyOrExit(interfaceName == OTBR_DBUS_THREAD_INTERFACE);

    VerifyOrExit(dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_ARRAY);
    dbus_message_iter_recurse(&iter, &subIter);
    VerifyOrExit(dbus_message_iter_get_arg_type(&subIter) == DBUS_TYPE_DICT_ENTRY);
    dbus_message_iter_recurse(&subIter, &dictEntryIter);
    SuccessOrExit(DBusMessageExtract(&dictEntryIter, propertyName));
    VerifyOrExit(dbus_message_iter_get_arg_type(&dictEntryIter) == DBUS_TYPE_VARIANT);
    dbus_message_iter_recurse(&dictEntryIter, &valIter);
    SuccessOrExit(DBusMessageExtract(&valIter, val));

    VerifyOrExit(propertyName == OTBR_DBUS_PROPERTY_DEVICE_ROLE);
    SuccessOrExit(NameToDeviceRole(val, role));

    for (const auto &f : mDeviceRoleHandlers)
    {
        f(role);
    }

exit:
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

DBusHandlerResult ThreadApiDBus::HandleCommissionerStateSignal(DBusMessage *aMessage)
{
    DBusMessageIter   iter;
    uint8_t           stateData;
    CommissionerState state;

    VerifyOrExit(dbus_message_iter_init(aMessage, &iter));
    SuccessOrExit(DBusMessageExtract(&iter, stateData));
    state = static_cast<CommissionerState>(stateData);

    if (mCommissionerStateHandler)
    {
        mCommissionerStateHandler(state);
    }

    if (state == COMMISSIONER_STATE_DISABLED)
    {
        mCommissionerStateHandler = nullptr;
        mJoinerEventHandler       = nullptr;
    }

exit:
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

DBusHandlerResult ThreadApiDBus::HandleJoinerEventSignal(DBusMessage *aMessage)
{
    DBusMessageIter         iter;
    uint8_t                 eventData;
    CommissionerJoinerEvent event;
    JoinerInfo              info;
    uint64_t                joinerId;
    bool                    joinerIdPresent;

    VerifyOrExit(dbus_message_iter_init(aMessage, &iter));
    SuccessOrExit(DBusMessageExtract(&iter, eventData));
    event = static_cast<CommissionerJoinerEvent>(eventData);
    SuccessOrExit(DBusMessageExtract(&iter, info));
    joinerIdPresent = (DBusMessageExtract(&iter, joinerId) == OTBR_ERROR_NONE);

    if (mJoinerEventHandler)
    {
        mJoinerEventHandler(event, info, joinerId, joinerIdPresent);
    }

exit:
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

void ThreadApiDBus::AddDeviceRoleHandler(const DeviceRoleHandler &aHandler)
{
    mDeviceRoleHandlers.push_back(aHandler);
}

ClientError ThreadApiDBus::Scan(const ScanHandler &aHandler)
{
    ClientError error = ClientError::ERROR_NONE;

    VerifyOrExit(mScanHandler == nullptr, error = ClientError::OT_ERROR_INVALID_STATE);
    mScanHandler = aHandler;

    error = CallDBusMethodAsync(OTBR_DBUS_SCAN_METHOD,
                                &ThreadApiDBus::sHandleDBusPendingCall<&ThreadApiDBus::ScanPendingCallHandler>);
    if (error != ClientError::ERROR_NONE)
    {
        mScanHandler = nullptr;
    }
exit:
    return error;
}

void ThreadApiDBus::ScanPendingCallHandler(DBusPendingCall *aPending)
{
    std::vector<ActiveScanResult> scanResults;
    UniqueDBusMessage             message(dbus_pending_call_steal_reply(aPending));
    auto                          args = std::tie(scanResults);

    if (message != nullptr)
    {
        DBusMessageToTuple(*message, args);
    }

    mScanHandler(scanResults);
    mScanHandler = nullptr;
}

ClientError ThreadApiDBus::PermitUnsecureJoin(uint16_t aPort, uint32_t aSeconds)
{
    return CallDBusMethodSync(OTBR_DBUS_PERMIT_UNSECURE_JOIN_METHOD, std::tie(aPort, aSeconds));
}

ClientError ThreadApiDBus::Attach(const std::string &         aNetworkName,
                                  uint16_t                    aPanId,
                                  uint64_t                    aExtPanId,
                                  const std::vector<uint8_t> &aMasterKey,
                                  const std::vector<uint8_t> &aPSKc,
                                  uint32_t                    aChannelMask,
                                  const OtResultHandler &     aHandler)
{
    ClientError error = ClientError::ERROR_NONE;
    const auto  args  = std::tie(aMasterKey, aPanId, aNetworkName, aExtPanId, aPSKc, aChannelMask);

    VerifyOrExit(mAttachHandler == nullptr && mJoinerHandler == nullptr, error = ClientError::OT_ERROR_INVALID_STATE);
    mAttachHandler = aHandler;

    if (aHandler)
    {
        error = CallDBusMethodAsync(OTBR_DBUS_ATTACH_METHOD, args,
                                    &ThreadApiDBus::sHandleDBusPendingCall<&ThreadApiDBus::AttachPendingCallHandler>);
    }
    else
    {
        error = CallDBusMethodSync(OTBR_DBUS_ATTACH_METHOD, args);
    }
    if (error != ClientError::ERROR_NONE)
    {
        mAttachHandler = nullptr;
    }
exit:
    return error;
}

void ThreadApiDBus::AttachPendingCallHandler(DBusPendingCall *aPending)
{
    ClientError       ret = ClientError::OT_ERROR_FAILED;
    UniqueDBusMessage message(dbus_pending_call_steal_reply(aPending));
    auto              handler = mAttachHandler;

    if (message != nullptr)
    {
        ret = CheckErrorMessage(message.get());
    }

    mAttachHandler = nullptr;
    handler(ret);
}

ClientError ThreadApiDBus::FactoryReset(const OtResultHandler &aHandler)
{
    ClientError error = ClientError::ERROR_NONE;

    VerifyOrExit(mFactoryResetHandler == nullptr, error = ClientError::OT_ERROR_INVALID_STATE);
    mFactoryResetHandler = aHandler;

    if (aHandler)
    {
        error =
            CallDBusMethodAsync(OTBR_DBUS_FACTORY_RESET_METHOD,
                                &ThreadApiDBus::sHandleDBusPendingCall<&ThreadApiDBus::FactoryResetPendingCallHandler>);
    }
    else
    {
        error = CallDBusMethodSync(OTBR_DBUS_FACTORY_RESET_METHOD);
    }
    if (error != ClientError::ERROR_NONE)
    {
        mFactoryResetHandler = nullptr;
    }
exit:
    return error;
}

void ThreadApiDBus::FactoryResetPendingCallHandler(DBusPendingCall *aPending)
{
    ClientError       ret = ClientError::OT_ERROR_FAILED;
    UniqueDBusMessage message(dbus_pending_call_steal_reply(aPending));

    if (message != nullptr)
    {
        ret = CheckErrorMessage(message.get());
    }

    mFactoryResetHandler(ret);
    mFactoryResetHandler = nullptr;
}

ClientError ThreadApiDBus::Reset(void)
{
    return CallDBusMethodSync(OTBR_DBUS_RESET_METHOD);
}

ClientError ThreadApiDBus::JoinerStart(const std::string &    aPskd,
                                       const std::string &    aProvisioningUrl,
                                       const std::string &    aVendorName,
                                       const std::string &    aVendorModel,
                                       const std::string &    aVendorSwVersion,
                                       const std::string &    aVendorData,
                                       const OtResultHandler &aHandler)
{
    ClientError error = ClientError::ERROR_NONE;
    const auto  args  = std::tie(aPskd, aProvisioningUrl, aVendorName, aVendorModel, aVendorSwVersion, aVendorData);
    DBusPendingCallNotifyFunction notifyFunc =
        aHandler ? &ThreadApiDBus::sHandleDBusPendingCall<&ThreadApiDBus::JoinerStartPendingCallHandler> : nullptr;

    VerifyOrExit(mAttachHandler == nullptr && mJoinerHandler == nullptr, error = ClientError::OT_ERROR_INVALID_STATE);
    mJoinerHandler = aHandler;

    if (aHandler)
    {
        error = CallDBusMethodAsync(OTBR_DBUS_JOINER_START_METHOD, args, notifyFunc);
    }
    else
    {
        error = CallDBusMethodSync(OTBR_DBUS_JOINER_START_METHOD, args);
    }
    if (error != ClientError::ERROR_NONE)
    {
        mJoinerHandler = nullptr;
    }
exit:
    return error;
}

void ThreadApiDBus::JoinerStartPendingCallHandler(DBusPendingCall *aPending)
{
    ClientError       ret = ClientError::ERROR_NONE;
    UniqueDBusMessage message(dbus_pending_call_steal_reply(aPending));
    auto              handler = mJoinerHandler;

    if (message != nullptr)
    {
        ret = CheckErrorMessage(message.get());
    }

    mJoinerHandler = nullptr;
    handler(ret);
}

ClientError ThreadApiDBus::JoinerStop(void)
{
    return CallDBusMethodSync(OTBR_DBUS_JOINER_STOP_METHOD);
}

ClientError ThreadApiDBus::AddOnMeshPrefix(const OnMeshPrefix &aPrefix)
{
    return CallDBusMethodSync(OTBR_DBUS_ADD_ON_MESH_PREFIX_METHOD, std::tie(aPrefix));
}

ClientError ThreadApiDBus::RemoveOnMeshPrefix(const Ip6Prefix &aPrefix)
{
    return CallDBusMethodSync(OTBR_DBUS_REMOVE_ON_MESH_PREFIX_METHOD, std::tie(aPrefix));
}

ClientError ThreadApiDBus::AddExternalRoute(const ExternalRoute &aExternalRoute)
{
    return CallDBusMethodSync(OTBR_DBUS_ADD_EXTERNAL_ROUTE_METHOD, std::tie(aExternalRoute));
}

ClientError ThreadApiDBus::RemoveExternalRoute(const Ip6Prefix &aPrefix)
{
    return CallDBusMethodSync(OTBR_DBUS_REMOVE_EXTERNAL_ROUTE_METHOD, std::tie(aPrefix));
}

ClientError ThreadApiDBus::CommissionerStart(CommissionerStateHandler aCommissionerStateHandler,
                                             JoinerEventHandler       aJoinerEventHandler)
{
    ClientError error = ClientError::ERROR_NONE;

    VerifyOrExit(mCommissionerStateHandler == nullptr && mJoinerEventHandler == nullptr,
                 error = ClientError::OT_ERROR_INVALID_STATE);
    mCommissionerStateHandler = aCommissionerStateHandler;
    mJoinerEventHandler       = aJoinerEventHandler;

    error = CallDBusMethodSync(OTBR_DBUS_COMMISSIONER_START_METHOD);

exit:
    return error;
}

ClientError ThreadApiDBus::CommissionerStop()
{
    mCommissionerStateHandler = nullptr;
    mJoinerEventHandler       = nullptr;
    return CallDBusMethodSync(OTBR_DBUS_COMMISSIONER_STOP_METHOD);
}

ClientError ThreadApiDBus::CommissionerAddJoiner(const JoinerInfo &aJoinerInfo)
{
    return CallDBusMethodSync(OTBR_DBUS_COMMISSIONER_ADD_JOINER_METHOD, std::tie(aJoinerInfo));
}

ClientError ThreadApiDBus::SetMeshLocalPrefix(const std::array<uint8_t, OTBR_IP6_PREFIX_SIZE> &aPrefix)
{
    return SetProperty(OTBR_DBUS_PROPERTY_MESH_LOCAL_PREFIX, aPrefix);
}

ClientError ThreadApiDBus::SetLegacyUlaPrefix(const std::array<uint8_t, OTBR_IP6_PREFIX_SIZE> &aPrefix)
{
    return SetProperty(OTBR_DBUS_PROPERTY_LEGACY_ULA_PREFIX, aPrefix);
}

ClientError ThreadApiDBus::SetLinkMode(const LinkModeConfig &aConfig)
{
    return SetProperty(OTBR_DBUS_PROPERTY_LINK_MODE, aConfig);
}

ClientError ThreadApiDBus::GetLinkMode(LinkModeConfig &aConfig)
{
    return GetProperty(OTBR_DBUS_PROPERTY_LINK_MODE, aConfig);
}

ClientError ThreadApiDBus::GetDeviceRole(DeviceRole &aRole)
{
    std::string roleName;
    ClientError error;

    SuccessOrExit(error = GetProperty(OTBR_DBUS_PROPERTY_DEVICE_ROLE, roleName));
    SuccessOrExit(error = NameToDeviceRole(roleName, aRole));
exit:
    return error;
}

ClientError ThreadApiDBus::GetNetworkName(std::string &aNetworkName)
{
    return GetProperty(OTBR_DBUS_PROPERTY_NETWORK_NAME, aNetworkName);
}

ClientError ThreadApiDBus::GetPanId(uint16_t &aPanId)
{
    return GetProperty(OTBR_DBUS_PROPERTY_PANID, aPanId);
}

ClientError ThreadApiDBus::GetExtPanId(uint64_t &aExtPanId)
{
    return GetProperty(OTBR_DBUS_PROPERTY_EXTPANID, aExtPanId);
}

ClientError ThreadApiDBus::GetChannel(uint16_t &aChannel)
{
    return GetProperty(OTBR_DBUS_PROPERTY_CHANNEL, aChannel);
}

ClientError ThreadApiDBus::GetMasterKey(std::vector<uint8_t> &aMasterKey)
{
    return GetProperty(OTBR_DBUS_PROPERTY_MASTER_KEY, aMasterKey);
}

ClientError ThreadApiDBus::GetCcaFailureRate(uint16_t &aFailureRate)
{
    return GetProperty(OTBR_DBUS_PROPERTY_CCA_FAILURE_RATE, aFailureRate);
}

ClientError ThreadApiDBus::GetLinkCounters(MacCounters &aCounters)
{
    return GetProperty(OTBR_DBUS_PROPERTY_LINK_COUNTERS, aCounters);
}

ClientError ThreadApiDBus::GetIp6Counters(IpCounters &aCounters)
{
    return GetProperty(OTBR_DBUS_PROPERTY_IP6_COUNTERS, aCounters);
}

ClientError ThreadApiDBus::GetSupportedChannelMask(uint32_t &aChannelMask)
{
    return GetProperty(OTBR_DBUS_PROPERTY_SUPPORTED_CHANNEL_MASK, aChannelMask);
}

ClientError ThreadApiDBus::GetRloc16(uint16_t &aRloc16)
{
    return GetProperty(OTBR_DBUS_PROPERTY_RLOC16, aRloc16);
}

ClientError ThreadApiDBus::GetExtendedAddress(uint64_t &aExtendedAddress)
{
    return GetProperty(OTBR_DBUS_PROPERTY_EXTENDED_ADDRESS, aExtendedAddress);
}

ClientError ThreadApiDBus::GetRouterId(uint8_t &aRouterId)
{
    return GetProperty(OTBR_DBUS_PROPERTY_ROUTER_ID, aRouterId);
}

ClientError ThreadApiDBus::GetLeaderData(LeaderData &aLeaderData)
{
    return GetProperty(OTBR_DBUS_PROPERTY_LEADER_DATA, aLeaderData);
}

ClientError ThreadApiDBus::GetNetworkData(std::vector<uint8_t> &aNetworkData)
{
    return GetProperty(OTBR_DBUS_PROPERTY_NETWORK_DATA_PRPOERTY, aNetworkData);
}

ClientError ThreadApiDBus::GetStableNetworkData(std::vector<uint8_t> &aNetworkData)
{
    return GetProperty(OTBR_DBUS_PROPERTY_STABLE_NETWORK_DATA_PRPOERTY, aNetworkData);
}

ClientError ThreadApiDBus::GetLocalLeaderWeight(uint8_t &aWeight)
{
    return GetProperty(OTBR_DBUS_PROPERTY_LOCAL_LEADER_WEIGHT, aWeight);
}

ClientError ThreadApiDBus::GetChannelMonitorSampleCount(uint32_t &aSampleCount)
{
    return GetProperty(OTBR_DBUS_PROPERTY_CHANNEL_MONITOR_SAMPLE_COUNT, aSampleCount);
}

ClientError ThreadApiDBus::GetChannelMonitorAllChannelQualities(std::vector<ChannelQuality> &aChannelQualities)
{
    return GetProperty(OTBR_DBUS_PROPERTY_CHANNEL_MONITOR_ALL_CHANNEL_QUALITIES, aChannelQualities);
}

ClientError ThreadApiDBus::GetChildTable(std::vector<ChildInfo> &aChildTable)
{
    return GetProperty(OTBR_DBUS_PROPERTY_CHILD_TABLE, aChildTable);
}

ClientError ThreadApiDBus::GetNeighborTable(std::vector<NeighborInfo> &aNeighborTable)
{
    return GetProperty(OTBR_DBUS_PROPERTY_NEIGHBOR_TABLE_PROEPRTY, aNeighborTable);
}

ClientError ThreadApiDBus::GetPartitionId(uint32_t &aPartitionId)
{
    return GetProperty(OTBR_DBUS_PROPERTY_PARTITION_ID_PROEPRTY, aPartitionId);
}

ClientError ThreadApiDBus::GetInstantRssi(int8_t &aRssi)
{
    return GetProperty(OTBR_DBUS_PROPERTY_INSTANT_RSSI, aRssi);
}

ClientError ThreadApiDBus::GetRadioTxPower(int8_t &aTxPower)
{
    return GetProperty(OTBR_DBUS_PROPERTY_RADIO_TX_POWER, aTxPower);
}

ClientError ThreadApiDBus::GetExternalRoutes(std::vector<ExternalRoute> &aExternalRoutes)
{
    return GetProperty(OTBR_DBUS_PROPERTY_EXTERNAL_ROUTES, aExternalRoutes);
}

std::string ThreadApiDBus::GetInterfaceName(void)
{
    return mInterfaceName;
}

ClientError ThreadApiDBus::CallDBusMethodSync(const std::string &aMethodName)
{
    ClientError       ret = ClientError::ERROR_NONE;
    UniqueDBusMessage message(dbus_message_new_method_call((OTBR_DBUS_SERVER_PREFIX + mInterfaceName).c_str(),
                                                           (OTBR_DBUS_OBJECT_PREFIX + mInterfaceName).c_str(),
                                                           OTBR_DBUS_THREAD_INTERFACE, aMethodName.c_str()));
    UniqueDBusMessage reply = nullptr;
    DBusError         error;

    dbus_error_init(&error);
    VerifyOrExit(message != nullptr, ret = ClientError::ERROR_DBUS);
    reply = UniqueDBusMessage(
        dbus_connection_send_with_reply_and_block(mConnection, message.get(), DBUS_TIMEOUT_USE_DEFAULT, &error));
    VerifyOrExit(!dbus_error_is_set(&error) && reply != nullptr, ret = ClientError::ERROR_DBUS);
    ret = DBus::CheckErrorMessage(reply.get());
exit:
    dbus_error_free(&error);
    return ret;
}

ClientError ThreadApiDBus::CallDBusMethodAsync(const std::string &aMethodName, DBusPendingCallNotifyFunction aFunction)
{
    ClientError       ret = ClientError::ERROR_NONE;
    UniqueDBusMessage message(dbus_message_new_method_call((OTBR_DBUS_SERVER_PREFIX + mInterfaceName).c_str(),
                                                           (OTBR_DBUS_OBJECT_PREFIX + mInterfaceName).c_str(),
                                                           OTBR_DBUS_THREAD_INTERFACE, aMethodName.c_str()));
    DBusPendingCall * pending = nullptr;

    VerifyOrExit(message != nullptr, ret = ClientError::OT_ERROR_FAILED);
    VerifyOrExit(dbus_connection_send_with_reply(mConnection, message.get(), &pending, DBUS_TIMEOUT_USE_DEFAULT) ==
                     true,
                 ret = ClientError::ERROR_DBUS);

    VerifyOrExit(dbus_pending_call_set_notify(pending, aFunction, this, &ThreadApiDBus::EmptyFree) == true,
                 ret = ClientError::ERROR_DBUS);
exit:
    return ret;
}

template <typename ArgType>
ClientError ThreadApiDBus::CallDBusMethodSync(const std::string &aMethodName, const ArgType &aArgs)
{
    ClientError             ret = ClientError::ERROR_NONE;
    DBus::UniqueDBusMessage message(dbus_message_new_method_call((OTBR_DBUS_SERVER_PREFIX + mInterfaceName).c_str(),
                                                                 (OTBR_DBUS_OBJECT_PREFIX + mInterfaceName).c_str(),
                                                                 OTBR_DBUS_THREAD_INTERFACE, aMethodName.c_str()));
    DBus::UniqueDBusMessage reply = nullptr;
    DBusError               error;

    dbus_error_init(&error);
    VerifyOrExit(message != nullptr, ret = ClientError::ERROR_DBUS);
    VerifyOrExit(otbr::DBus::TupleToDBusMessage(*message, aArgs) == OTBR_ERROR_NONE, ret = ClientError::ERROR_DBUS);
    reply = DBus::UniqueDBusMessage(
        dbus_connection_send_with_reply_and_block(mConnection, message.get(), DBUS_TIMEOUT_USE_DEFAULT, &error));
    VerifyOrExit(!dbus_error_is_set(&error) && reply != nullptr, ret = ClientError::ERROR_DBUS);
    ret = DBus::CheckErrorMessage(reply.get());
exit:
    dbus_error_free(&error);
    return ret;
}

template <typename ArgType>
ClientError ThreadApiDBus::CallDBusMethodAsync(const std::string &           aMethodName,
                                               const ArgType &               aArgs,
                                               DBusPendingCallNotifyFunction aFunction)
{
    ClientError ret = ClientError::ERROR_NONE;

    DBus::UniqueDBusMessage message(dbus_message_new_method_call((OTBR_DBUS_SERVER_PREFIX + mInterfaceName).c_str(),
                                                                 (OTBR_DBUS_OBJECT_PREFIX + mInterfaceName).c_str(),
                                                                 OTBR_DBUS_THREAD_INTERFACE, aMethodName.c_str()));
    DBusPendingCall *       pending = nullptr;

    VerifyOrExit(message != nullptr, ret = ClientError::ERROR_DBUS);
    VerifyOrExit(DBus::TupleToDBusMessage(*message, aArgs) == OTBR_ERROR_NONE, ret = ClientError::ERROR_DBUS);
    VerifyOrExit(dbus_connection_send_with_reply(mConnection, message.get(), &pending, DBUS_TIMEOUT_USE_DEFAULT) ==
                     true,
                 ret = ClientError::ERROR_DBUS);

    VerifyOrExit(dbus_pending_call_set_notify(pending, aFunction, this, &ThreadApiDBus::EmptyFree) == true,
                 ret = ClientError::ERROR_DBUS);
exit:
    return ret;
}

template <typename ValType>
ClientError ThreadApiDBus::SetProperty(const std::string &aPropertyName, const ValType &aValue)
{
    DBus::UniqueDBusMessage message(dbus_message_new_method_call((OTBR_DBUS_SERVER_PREFIX + mInterfaceName).c_str(),
                                                                 (OTBR_DBUS_OBJECT_PREFIX + mInterfaceName).c_str(),
                                                                 DBUS_INTERFACE_PROPERTIES, DBUS_PROPERTY_SET_METHOD));
    DBus::UniqueDBusMessage reply = nullptr;
    ClientError             ret   = ClientError::ERROR_NONE;
    DBusError               error;
    DBusMessageIter         iter;

    dbus_error_init(&error);
    VerifyOrExit(message != nullptr, ret = ClientError::OT_ERROR_FAILED);

    dbus_message_iter_init_append(message.get(), &iter);
    VerifyOrExit(DBus::DBusMessageEncode(&iter, OTBR_DBUS_THREAD_INTERFACE) == OTBR_ERROR_NONE,
                 ret = ClientError::ERROR_DBUS);
    VerifyOrExit(DBus::DBusMessageEncode(&iter, aPropertyName) == OTBR_ERROR_NONE, ret = ClientError::ERROR_DBUS);
    VerifyOrExit(DBus::DBusMessageEncodeToVariant(&iter, aValue) == OTBR_ERROR_NONE, ret = ClientError::ERROR_DBUS);

    reply = DBus::UniqueDBusMessage(
        dbus_connection_send_with_reply_and_block(mConnection, message.get(), DBUS_TIMEOUT_USE_DEFAULT, &error));

    VerifyOrExit(!dbus_error_is_set(&error) && reply != nullptr, ret = ClientError::OT_ERROR_FAILED);
    ret = DBus::CheckErrorMessage(reply.get());
exit:
    dbus_error_free(&error);
    return ret;
}

template <typename ValType> ClientError ThreadApiDBus::GetProperty(const std::string &aPropertyName, ValType &aValue)
{
    DBus::UniqueDBusMessage message(dbus_message_new_method_call((OTBR_DBUS_SERVER_PREFIX + mInterfaceName).c_str(),
                                                                 (OTBR_DBUS_OBJECT_PREFIX + mInterfaceName).c_str(),
                                                                 DBUS_INTERFACE_PROPERTIES, DBUS_PROPERTY_GET_METHOD));
    DBus::UniqueDBusMessage reply = nullptr;

    ClientError     ret = ClientError::ERROR_NONE;
    DBusError       error;
    DBusMessageIter iter;

    dbus_error_init(&error);
    VerifyOrExit(message != nullptr, ret = ClientError::OT_ERROR_FAILED);
    otbr::DBus::TupleToDBusMessage(*message, std::tie(OTBR_DBUS_THREAD_INTERFACE, aPropertyName));
    reply = DBus::UniqueDBusMessage(
        dbus_connection_send_with_reply_and_block(mConnection, message.get(), DBUS_TIMEOUT_USE_DEFAULT, &error));

    VerifyOrExit(!dbus_error_is_set(&error) && reply != nullptr, ret = ClientError::OT_ERROR_FAILED);
    SuccessOrExit(DBus::CheckErrorMessage(reply.get()));
    VerifyOrExit(dbus_message_iter_init(reply.get(), &iter), ret = ClientError::OT_ERROR_FAILED);
    VerifyOrExit(DBus::DBusMessageExtractFromVariant(&iter, aValue) == OTBR_ERROR_NONE,
                 ret = ClientError::OT_ERROR_FAILED);

exit:
    dbus_error_free(&error);
    return ret;
}

template <void (ThreadApiDBus::*Handler)(DBusPendingCall *aPending)>
void ThreadApiDBus::sHandleDBusPendingCall(DBusPendingCall *aPending, void *aThreadApiDBus)
{
    ThreadApiDBus *api = static_cast<ThreadApiDBus *>(aThreadApiDBus);

    (api->*Handler)(aPending);
}

} // namespace DBus
} // namespace otbr
