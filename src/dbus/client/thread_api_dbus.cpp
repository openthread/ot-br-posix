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
#include "dbus/client/thread_api_dbus.hpp"
#include "dbus/common/constants.hpp"
#include "dbus/common/dbus_message_helper.hpp"
#include "dbus/common/dbus_resources.hpp"
#include "dbus/common/error_helper.hpp"

namespace otbr {
namespace DBus {

static OtbrClientError NameToDeviceRole(const std::string &aRoleName, otbrDeviceRole &aDeviceRole)
{
    static std::pair<const char *, otbrDeviceRole> sRoleMap[] = {
        {OTBR_DISABLED_ROLE_NAME, OTBR_DEVICE_ROLE_DISABLED}, {OTBR_DETACHED_ROLE_NAME, OTBR_DEVICE_ROLE_DETACHED},
        {OTBR_CHILD_ROLE_NAME, OTBR_DEVICE_ROLE_CHILD},       {OTBR_ROUTER_ROLE_NAME, OTBR_DEVICE_ROLE_ROUTER},
        {OTBR_LEADER_ROLE_NAME, OTBR_DEVICE_ROLE_LEADER},
    };
    OtbrClientError err = OtbrClientError::OT_ERROR_NOT_FOUND;

    for (const auto &p : sRoleMap)
    {
        if (p.first == aRoleName)
        {
            aDeviceRole = p.second;
            err         = OtbrClientError::ERROR_NONE;
            break;
        }
    }

    return err;
}

bool IsThreadActive(otbrDeviceRole aRole)
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
    SubscribeDeviceRoleSignal();
}

ThreadApiDBus::ThreadApiDBus(DBusConnection *aConnection, const std::string &aInterfaceName)
    : mInterfaceName(aInterfaceName)
    , mConnection(aConnection)
{
    SubscribeDeviceRoleSignal();
}

OtbrClientError ThreadApiDBus::SubscribeDeviceRoleSignal(void)
{
    std::string     matchRule = "type='signal',interface='" DBUS_INTERFACE_PROPERTIES "'";
    DBusError       err;
    OtbrClientError ret = OtbrClientError::ERROR_NONE;

    dbus_error_init(&err);
    dbus_bus_add_match(mConnection, matchRule.c_str(), &err);

    VerifyOrExit(!dbus_error_is_set(&err), ret = OtbrClientError::OT_ERROR_FAILED);

    dbus_connection_add_filter(mConnection, sDBusMessageFilter, this, nullptr);
exit:
    dbus_error_free(&err);
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

    DBusMessageIter iter, subIter, dictEntryIter, valIter;
    std::string     interfaceName, propertyName, val;
    otbrDeviceRole  role;

    VerifyOrExit(dbus_message_is_signal(aMessage, DBUS_INTERFACE_PROPERTIES, DBUS_PROPERTIES_CHANGED_SIGNAL));
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

    VerifyOrExit(propertyName == OTBR_DBUS_DEVICE_ROLE_PROPERTY);
    SuccessOrExit(NameToDeviceRole(val, role));

    for (const auto &f : mDeviceRoleHandlers)
    {
        f(role);
    }
exit:
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

void ThreadApiDBus::AddDeviceRoleHandler(const DeviceRoleHandler &aHandler)
{
    mDeviceRoleHandlers.push_back(aHandler);
}

OtbrClientError ThreadApiDBus::Scan(const ScanHandler &aHandler)
{
    OtbrClientError err = OtbrClientError::ERROR_NONE;

    VerifyOrExit(mScanHandler == nullptr, err = OtbrClientError::OT_ERROR_INVALID_STATE);
    mScanHandler = aHandler;

    err = CallDBusMethodAsync(OTBR_DBUS_SCAN_METHOD,
                              &ThreadApiDBus::sHandleDBusPendingCall<&ThreadApiDBus::ScanPendingCallHandler>);
    if (err != OtbrClientError::ERROR_NONE)
    {
        mScanHandler = nullptr;
    }
exit:
    return err;
}

void ThreadApiDBus::ScanPendingCallHandler(DBusPendingCall *aPending)
{
    std::vector<otbrActiveScanResult> scanResults;
    UniqueDBusMessage                 message(dbus_pending_call_steal_reply(aPending));
    auto                              args = std::tie(scanResults);

    if (message != nullptr)
    {
        DBusMessageToTuple(*message, args);
    }

    mScanHandler(scanResults);
    mScanHandler = nullptr;
}

OtbrClientError ThreadApiDBus::AddUnsecurePort(uint16_t aPort, uint32_t aSeconds)
{
    return CallDBusMethodSync(OTBR_DBUS_ADD_UNSECURE_PORT_METHOD, std::tie(aPort, aSeconds));
}

OtbrClientError ThreadApiDBus::Attach(const std::string &         aNetworkName,
                                      uint16_t                    aPanId,
                                      uint64_t                    aExtPanId,
                                      const std::vector<uint8_t> &aMasterKey,
                                      const std::vector<uint8_t> &aPSKc,
                                      uint32_t                    aChannelMask,
                                      const OtResultHandler &     aHandler)
{
    OtbrClientError err  = OtbrClientError::ERROR_NONE;
    const auto      args = std::tie(aMasterKey, aPanId, aNetworkName, aExtPanId, aPSKc, aChannelMask);

    VerifyOrExit(mAttachHandler == nullptr && mJoinerHandler == nullptr, err = OtbrClientError::OT_ERROR_INVALID_STATE);
    mAttachHandler = aHandler;

    if (aHandler)
    {
        err = CallDBusMethodAsync(OTBR_DBUS_ATTACH_METHOD, args,
                                  &ThreadApiDBus::sHandleDBusPendingCall<&ThreadApiDBus::AttachPendingCallHandler>);
    }
    else
    {
        err = CallDBusMethodSync(OTBR_DBUS_ATTACH_METHOD, args);
    }
    if (err != OtbrClientError::ERROR_NONE)
    {
        mAttachHandler = nullptr;
    }
exit:
    return err;
}

void ThreadApiDBus::AttachPendingCallHandler(DBusPendingCall *aPending)
{
    OtbrClientError   ret = OtbrClientError::OT_ERROR_FAILED;
    UniqueDBusMessage message(dbus_pending_call_steal_reply(aPending));
    auto              handler = mAttachHandler;

    if (message != nullptr)
    {
        ret = CheckErrorMessage(message.get());
    }

    mAttachHandler = nullptr;
    handler(ret);
}

OtbrClientError ThreadApiDBus::FactoryReset(const OtResultHandler &aHandler)
{
    OtbrClientError err = OtbrClientError::ERROR_NONE;

    VerifyOrExit(mFactoryResetHandler == nullptr, err = OtbrClientError::OT_ERROR_INVALID_STATE);
    mFactoryResetHandler = aHandler;

    if (aHandler)
    {
        err =
            CallDBusMethodAsync(OTBR_DBUS_FACTORY_RESET_METHOD,
                                &ThreadApiDBus::sHandleDBusPendingCall<&ThreadApiDBus::FactoryResetPendingCallHandler>);
    }
    else
    {
        err = CallDBusMethodSync(OTBR_DBUS_FACTORY_RESET_METHOD);
    }
    if (err != OtbrClientError::ERROR_NONE)
    {
        mFactoryResetHandler = nullptr;
    }
exit:
    return err;
}

void ThreadApiDBus::FactoryResetPendingCallHandler(DBusPendingCall *aPending)
{
    OtbrClientError   ret = OtbrClientError::OT_ERROR_FAILED;
    UniqueDBusMessage message(dbus_pending_call_steal_reply(aPending));

    if (message != nullptr)
    {
        ret = CheckErrorMessage(message.get());
    }

    mFactoryResetHandler(ret);
    mFactoryResetHandler = nullptr;
}

OtbrClientError ThreadApiDBus::Reset(void)
{
    return CallDBusMethodSync(OTBR_DBUS_RESET_METHOD);
}

OtbrClientError ThreadApiDBus::JoinerStart(const std::string &    aPskd,
                                           const std::string &    aProvisioningUrl,
                                           const std::string &    aVendorName,
                                           const std::string &    aVendorModel,
                                           const std::string &    aVendorSwVersion,
                                           const std::string &    aVendorData,
                                           const OtResultHandler &aHandler)
{
    OtbrClientError err  = OtbrClientError::ERROR_NONE;
    const auto      args = std::tie(aPskd, aProvisioningUrl, aVendorName, aVendorModel, aVendorSwVersion, aVendorData);
    DBusPendingCallNotifyFunction notifyFunc =
        aHandler ? &ThreadApiDBus::sHandleDBusPendingCall<&ThreadApiDBus::JoinerStartPendingCallHandler> : nullptr;

    VerifyOrExit(mAttachHandler == nullptr && mJoinerHandler == nullptr, err = OtbrClientError::OT_ERROR_INVALID_STATE);
    mJoinerHandler = aHandler;

    if (aHandler)
    {
        err = CallDBusMethodAsync(OTBR_DBUS_JOINER_START_METHOD, args, notifyFunc);
    }
    else
    {
        err = CallDBusMethodSync(OTBR_DBUS_JOINER_START_METHOD, args);
    }
    if (err != OtbrClientError::ERROR_NONE)
    {
        mJoinerHandler = nullptr;
    }
exit:
    return err;
}

void ThreadApiDBus::JoinerStartPendingCallHandler(DBusPendingCall *aPending)
{
    OtbrClientError   ret = OtbrClientError::ERROR_NONE;
    UniqueDBusMessage message(dbus_pending_call_steal_reply(aPending));
    auto              handler = mJoinerHandler;

    if (message != nullptr)
    {
        ret = CheckErrorMessage(message.get());
    }

    mJoinerHandler = nullptr;
    handler(ret);
}

OtbrClientError ThreadApiDBus::JoinerStop(void)
{
    return CallDBusMethodSync(OTBR_DBUS_JOINER_STOP_METHOD);
}

OtbrClientError ThreadApiDBus::AddOnMeshPrefix(const otbrOnMeshPrefix &aPrefix)
{
    return CallDBusMethodSync(OTBR_DBUS_ADD_ON_MESH_PREFIX_METHOD, std::tie(aPrefix));
}

OtbrClientError ThreadApiDBus::RemoveOnMeshPrefix(const otbrIp6Prefix &aPrefix)
{
    return CallDBusMethodSync(OTBR_DBUS_REMOVE_ON_MESH_PREFIX_METHOD, std::tie(aPrefix));
}

OtbrClientError ThreadApiDBus::SetMeshLocalPrefix(const std::array<uint8_t, OTBR_IP6_PREFIX_SIZE> &aPrefix)
{
    return SetProperty(OTBR_DBUS_MESH_LOCAL_PREFIX_PROPERTY, aPrefix);
}

OtbrClientError ThreadApiDBus::SetLegacyUlaPrefix(const std::array<uint8_t, OTBR_IP6_PREFIX_SIZE> &aPrefix)
{
    return SetProperty(OTBR_DBUS_LEGACY_ULA_PREFIX_PROPERTY, aPrefix);
}

OtbrClientError ThreadApiDBus::SetLinkMode(const otbrLinkModeConfig &aConfig)
{
    return SetProperty(OTBR_DBUS_LINK_MODE_PROPERTY, aConfig);
}

OtbrClientError ThreadApiDBus::GetLinkMode(otbrLinkModeConfig &aConfig)
{
    return GetProperty(OTBR_DBUS_LINK_MODE_PROPERTY, aConfig);
}

OtbrClientError ThreadApiDBus::GetDeviceRole(otbrDeviceRole &aRole)
{
    std::string     roleName;
    OtbrClientError err;

    SuccessOrExit(err = GetProperty(OTBR_DBUS_DEVICE_ROLE_PROPERTY, roleName));
    SuccessOrExit(err = NameToDeviceRole(roleName, aRole));
exit:
    return err;
}

OtbrClientError ThreadApiDBus::GetNetworkName(std::string &aNetworkName)
{
    return GetProperty(OTBR_DBUS_NETWORK_NAME_PROPERTY, aNetworkName);
}

OtbrClientError ThreadApiDBus::GetPanId(uint16_t &aPanId)
{
    return GetProperty(OTBR_DBUS_PANID_PROPERTY, aPanId);
}

OtbrClientError ThreadApiDBus::GetExtPanId(uint64_t &aExtPanId)
{
    return GetProperty(OTBR_DBUS_EXTPANID_PROPERTY, aExtPanId);
}

OtbrClientError ThreadApiDBus::GetChannel(uint16_t &aChannel)
{
    return GetProperty(OTBR_DBUS_CHANNEL_PROPERTY, aChannel);
}

OtbrClientError ThreadApiDBus::GetMasterKey(std::vector<uint8_t> &aMasterKey)
{
    return GetProperty(OTBR_DBUS_MASTER_KEY_PROPERTY, aMasterKey);
}

OtbrClientError ThreadApiDBus::GetCcaFailureRate(uint16_t &aFailureRate)
{
    return GetProperty(OTBR_DBUS_CCA_FAILURE_RATE_PROPERTY, aFailureRate);
}

OtbrClientError ThreadApiDBus::GetLinkCounters(otbrMacCounters &aCounters)
{
    return GetProperty(OTBR_DBUS_LINK_COUNTERS_PROPERTY, aCounters);
}

OtbrClientError ThreadApiDBus::GetIp6Counters(otbrIpCounters &aCounters)
{
    return GetProperty(OTBR_DBUS_IP6_COUNTERS_PROPERTY, aCounters);
}

OtbrClientError ThreadApiDBus::GetSupportedChannelMask(uint32_t &aChannelMask)
{
    return GetProperty(OTBR_DBUS_SUPPORTED_CHANNEL_MASK_PROPERTY, aChannelMask);
}

std::string ThreadApiDBus::GetInterfaceName(void)
{
    return mInterfaceName;
}

OtbrClientError ThreadApiDBus::CallDBusMethodSync(const std::string &aMethodName)
{
    OtbrClientError   ret = OtbrClientError::ERROR_NONE;
    UniqueDBusMessage message(dbus_message_new_method_call((OTBR_DBUS_SERVER_PREFIX + mInterfaceName).c_str(),
                                                           (OTBR_DBUS_OBJECT_PREFIX + mInterfaceName).c_str(),
                                                           OTBR_DBUS_THREAD_INTERFACE, aMethodName.c_str()));
    UniqueDBusMessage reply = nullptr;
    DBusError         err;

    dbus_error_init(&err);
    VerifyOrExit(message != nullptr, ret = OtbrClientError::ERROR_DBUS);
    reply = UniqueDBusMessage(
        dbus_connection_send_with_reply_and_block(mConnection, message.get(), DBUS_TIMEOUT_USE_DEFAULT, &err));
    VerifyOrExit(!dbus_error_is_set(&err) && reply != nullptr, ret = OtbrClientError::ERROR_DBUS);
    ret = DBus::CheckErrorMessage(reply.get());
exit:
    dbus_error_free(&err);
    return ret;
}

OtbrClientError ThreadApiDBus::CallDBusMethodAsync(const std::string &           aMethodName,
                                                   DBusPendingCallNotifyFunction aFunction)
{
    OtbrClientError   ret = OtbrClientError::ERROR_NONE;
    UniqueDBusMessage message(dbus_message_new_method_call((OTBR_DBUS_SERVER_PREFIX + mInterfaceName).c_str(),
                                                           (OTBR_DBUS_OBJECT_PREFIX + mInterfaceName).c_str(),
                                                           OTBR_DBUS_THREAD_INTERFACE, aMethodName.c_str()));
    DBusPendingCall * pending = nullptr;

    VerifyOrExit(message != nullptr, ret = OtbrClientError::OT_ERROR_FAILED);
    VerifyOrExit(dbus_connection_send_with_reply(mConnection, message.get(), &pending, DBUS_TIMEOUT_USE_DEFAULT) ==
                     true,
                 ret = OtbrClientError::ERROR_DBUS);

    VerifyOrExit(dbus_pending_call_set_notify(pending, aFunction, this, &ThreadApiDBus::EmptyFree) == true,
                 ret = OtbrClientError::ERROR_DBUS);
exit:
    return ret;
}

template <typename ArgType>
OtbrClientError ThreadApiDBus::CallDBusMethodSync(const std::string &aMethodName, const ArgType &aArgs)
{
    OtbrClientError         ret = OtbrClientError::ERROR_NONE;
    DBus::UniqueDBusMessage message(dbus_message_new_method_call((OTBR_DBUS_SERVER_PREFIX + mInterfaceName).c_str(),
                                                                 (OTBR_DBUS_OBJECT_PREFIX + mInterfaceName).c_str(),
                                                                 OTBR_DBUS_THREAD_INTERFACE, aMethodName.c_str()));
    DBus::UniqueDBusMessage reply = nullptr;
    DBusError               err;

    dbus_error_init(&err);
    VerifyOrExit(message != nullptr, ret = OtbrClientError::ERROR_DBUS);
    VerifyOrExit(otbr::DBus::TupleToDBusMessage(*message, aArgs) == OTBR_ERROR_NONE, ret = OtbrClientError::ERROR_DBUS);
    reply = DBus::UniqueDBusMessage(
        dbus_connection_send_with_reply_and_block(mConnection, message.get(), DBUS_TIMEOUT_USE_DEFAULT, &err));
    VerifyOrExit(!dbus_error_is_set(&err) && reply != nullptr, ret = OtbrClientError::ERROR_DBUS);
    ret = DBus::CheckErrorMessage(reply.get());
exit:
    dbus_error_free(&err);
    return ret;
}

template <typename ArgType>
OtbrClientError ThreadApiDBus::CallDBusMethodAsync(const std::string &           aMethodName,
                                                   const ArgType &               aArgs,
                                                   DBusPendingCallNotifyFunction aFunction)
{
    OtbrClientError ret = OtbrClientError::ERROR_NONE;

    DBus::UniqueDBusMessage message(dbus_message_new_method_call((OTBR_DBUS_SERVER_PREFIX + mInterfaceName).c_str(),
                                                                 (OTBR_DBUS_OBJECT_PREFIX + mInterfaceName).c_str(),
                                                                 OTBR_DBUS_THREAD_INTERFACE, aMethodName.c_str()));
    DBusPendingCall *       pending = nullptr;

    VerifyOrExit(message != nullptr, ret = OtbrClientError::ERROR_DBUS);
    VerifyOrExit(DBus::TupleToDBusMessage(*message, aArgs) == OTBR_ERROR_NONE, ret = OtbrClientError::ERROR_DBUS);
    VerifyOrExit(dbus_connection_send_with_reply(mConnection, message.get(), &pending, DBUS_TIMEOUT_USE_DEFAULT) ==
                     true,
                 ret = OtbrClientError::ERROR_DBUS);

    VerifyOrExit(dbus_pending_call_set_notify(pending, aFunction, this, &ThreadApiDBus::EmptyFree) == true,
                 ret = OtbrClientError::ERROR_DBUS);
exit:
    return ret;
}

template <typename ValType>
OtbrClientError ThreadApiDBus::SetProperty(const std::string &aPropertyName, const ValType &aValue)
{
    DBus::UniqueDBusMessage message(dbus_message_new_method_call((OTBR_DBUS_SERVER_PREFIX + mInterfaceName).c_str(),
                                                                 (OTBR_DBUS_OBJECT_PREFIX + mInterfaceName).c_str(),
                                                                 DBUS_INTERFACE_PROPERTIES, DBUS_PROPERTY_SET_METHOD));
    DBus::UniqueDBusMessage reply = nullptr;
    OtbrClientError         ret   = OtbrClientError::ERROR_NONE;
    DBusError               err;
    DBusMessageIter         iter;

    dbus_error_init(&err);
    VerifyOrExit(message != nullptr, ret = OtbrClientError::OT_ERROR_FAILED);

    dbus_message_iter_init_append(message.get(), &iter);
    VerifyOrExit(DBus::DBusMessageEncode(&iter, OTBR_DBUS_THREAD_INTERFACE) == OTBR_ERROR_NONE,
                 ret = OtbrClientError::ERROR_DBUS);
    VerifyOrExit(DBus::DBusMessageEncode(&iter, aPropertyName) == OTBR_ERROR_NONE, ret = OtbrClientError::ERROR_DBUS);
    VerifyOrExit(DBus::DBusMessageEncodeToVariant(&iter, aValue) == OTBR_ERROR_NONE, ret = OtbrClientError::ERROR_DBUS);

    reply = DBus::UniqueDBusMessage(
        dbus_connection_send_with_reply_and_block(mConnection, message.get(), DBUS_TIMEOUT_USE_DEFAULT, &err));

    VerifyOrExit(!dbus_error_is_set(&err) && reply != nullptr, ret = OtbrClientError::OT_ERROR_FAILED);
    ret = DBus::CheckErrorMessage(reply.get());
exit:
    dbus_error_free(&err);
    return ret;
}

template <typename ValType>
OtbrClientError ThreadApiDBus::GetProperty(const std::string &aPropertyName, ValType &aValue)
{
    DBus::UniqueDBusMessage message(dbus_message_new_method_call((OTBR_DBUS_SERVER_PREFIX + mInterfaceName).c_str(),
                                                                 (OTBR_DBUS_OBJECT_PREFIX + mInterfaceName).c_str(),
                                                                 DBUS_INTERFACE_PROPERTIES, DBUS_PROPERTY_GET_METHOD));
    DBus::UniqueDBusMessage reply = nullptr;

    OtbrClientError ret = OtbrClientError::ERROR_NONE;
    DBusError       err;
    DBusMessageIter iter;

    dbus_error_init(&err);
    VerifyOrExit(message != nullptr, ret = OtbrClientError::OT_ERROR_FAILED);
    otbr::DBus::TupleToDBusMessage(*message, std::tie(OTBR_DBUS_THREAD_INTERFACE, aPropertyName));
    reply = DBus::UniqueDBusMessage(
        dbus_connection_send_with_reply_and_block(mConnection, message.get(), DBUS_TIMEOUT_USE_DEFAULT, &err));

    VerifyOrExit(!dbus_error_is_set(&err) && reply != nullptr, ret = OtbrClientError::OT_ERROR_FAILED);
    SuccessOrExit(DBus::CheckErrorMessage(reply.get()));
    VerifyOrExit(dbus_message_iter_init(reply.get(), &iter), ret = OtbrClientError::OT_ERROR_FAILED);
    VerifyOrExit(DBus::DBusMessageExtractFromVariant(&iter, aValue) == OTBR_ERROR_NONE,
                 ret = OtbrClientError::OT_ERROR_FAILED);

exit:
    dbus_error_free(&err);
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
