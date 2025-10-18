/*
 *    Copyright (c) 2024, The OpenThread Authors.
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

#include "dbus_thread_object_ncp.hpp"
#include <openthread/border_agent.h>
#include <openthread/border_router.h>

#include "common/api_strings.hpp"
#include "common/byteswap.hpp"
#include "common/code_utils.hpp"
#include "dbus/server/dbus_agent.hpp"
#include "host/thread_helper.hpp"

using std::placeholders::_1;
using std::placeholders::_2;

namespace otbr {
namespace DBus {

DBusThreadObjectNcp::DBusThreadObjectNcp(DBusConnection            &aConnection,
                                         const std::string         &aInterfaceName,
                                         const DependentComponents &aDeps)
    : DBusObject(&aConnection, OTBR_DBUS_OBJECT_PREFIX + aInterfaceName)
    , mHost(static_cast<Host::NcpHost &>(aDeps.mHost))
#if OTBR_ENABLE_BORDER_AGENT
    , mBorderAgent(aDeps.mBorderAgent)
#endif
{
}

otbrError DBusThreadObjectNcp::Init(void)
{
    otbrError error = OTBR_ERROR_NONE;

    SuccessOrExit(error = DBusObject::Initialize(true));

    RegisterAsyncGetPropertyHandler(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_DEVICE_ROLE,
                                    std::bind(&DBusThreadObjectNcp::AsyncGetDeviceRoleHandler, this, _1));

    RegisterMethod(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_JOIN_METHOD,
                   std::bind(&DBusThreadObjectNcp::JoinHandler, this, _1));
    RegisterMethod(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_LEAVE_NETWORK_METHOD,
                   std::bind(&DBusThreadObjectNcp::LeaveHandler, this, _1));
    RegisterMethod(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_SCHEDULE_MIGRATION_METHOD,
                   std::bind(&DBusThreadObjectNcp::ScheduleMigrationHandler, this, _1));
    RegisterMethod(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_HOST_POWER_STATE_METHOD,
                   std::bind(&DBusThreadObjectNcp::HostPowerStateHandler, this, _1));
#if OTBR_ENABLE_EPSKC
    RegisterMethod(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_PROPERTY_EPHEMERAL_KEY_ENABLED,
                   std::bind(&DBusThreadObjectNcp::EnableEphemeralKeyModeHandler, this, _1));
    RegisterMethod(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_ACTIVATE_EPHEMERAL_KEY_MODE_METHOD,
                   std::bind(&DBusThreadObjectNcp::ActivateEphemeralKeyModeHandler, this, _1));
    RegisterMethod(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_DEACTIVATE_EPHEMERAL_KEY_MODE_METHOD,
                   std::bind(&DBusThreadObjectNcp::DeactivateEphemeralKeyModeHandler, this, _1));
#endif

    SuccessOrExit(error = Signal(OTBR_DBUS_THREAD_INTERFACE, OTBR_DBUS_SIGNAL_READY, std::make_tuple()));
exit:
    return error;
}

void DBusThreadObjectNcp::AsyncGetDeviceRoleHandler(DBusRequest &aRequest)
{
    otDeviceRole role = mHost.GetDeviceRole();

    ReplyAsyncGetProperty(aRequest, GetDeviceRoleName(role));
}

void DBusThreadObjectNcp::ReplyAsyncGetProperty(DBusRequest &aRequest, const std::string &aContent)
{
    UniqueDBusMessage reply{dbus_message_new_method_return(aRequest.GetMessage())};
    DBusMessageIter   replyIter;
    otError           error = OT_ERROR_NONE;

    dbus_message_iter_init_append(reply.get(), &replyIter);
    SuccessOrExit(error = OtbrErrorToOtError(DBusMessageEncodeToVariant(&replyIter, aContent)));

exit:
    if (error == OT_ERROR_NONE)
    {
        dbus_connection_send(aRequest.GetConnection(), reply.get(), nullptr);
    }
    else
    {
        aRequest.ReplyOtResult(error);
    }
}

void DBusThreadObjectNcp::JoinHandler(DBusRequest &aRequest)
{
    std::vector<uint8_t>     dataset;
    otOperationalDatasetTlvs activeOpDatasetTlvs;
    otError                  error = OT_ERROR_NONE;

    auto args = std::tie(dataset);

    SuccessOrExit(DBusMessageToTuple(*aRequest.GetMessage(), args), error = OT_ERROR_INVALID_ARGS);

    VerifyOrExit(dataset.size() <= sizeof(activeOpDatasetTlvs.mTlvs), error = OT_ERROR_INVALID_ARGS);
    std::copy(dataset.begin(), dataset.end(), activeOpDatasetTlvs.mTlvs);
    activeOpDatasetTlvs.mLength = dataset.size();

    mHost.Join(activeOpDatasetTlvs, [aRequest](otError aError, const std::string &aErrorInfo) mutable {
        OT_UNUSED_VARIABLE(aErrorInfo);
        aRequest.ReplyOtResult(aError);
    });

exit:
    if (error != OT_ERROR_NONE)
    {
        aRequest.ReplyOtResult(error);
    }
}

void DBusThreadObjectNcp::LeaveHandler(DBusRequest &aRequest)
{
    mHost.Leave(true /* aEraseDataset */, [aRequest](otError aError, const std::string &aErrorInfo) mutable {
        OT_UNUSED_VARIABLE(aErrorInfo);
        aRequest.ReplyOtResult(aError);
    });
}

void DBusThreadObjectNcp::ScheduleMigrationHandler(DBusRequest &aRequest)
{
    std::vector<uint8_t>     dataset;
    uint32_t                 delayInMilli;
    otOperationalDatasetTlvs pendingOpDatasetTlvs;
    otError                  error = OT_ERROR_NONE;

    auto args = std::tie(dataset, delayInMilli);

    SuccessOrExit(DBusMessageToTuple(*aRequest.GetMessage(), args), error = OT_ERROR_INVALID_ARGS);

    VerifyOrExit(dataset.size() <= sizeof(pendingOpDatasetTlvs.mTlvs), error = OT_ERROR_INVALID_ARGS);
    std::copy(dataset.begin(), dataset.end(), pendingOpDatasetTlvs.mTlvs);
    pendingOpDatasetTlvs.mLength = dataset.size();

    SuccessOrExit(error = Host::ThreadHelper::ProcessDatasetForMigration(pendingOpDatasetTlvs, delayInMilli));

    mHost.ScheduleMigration(pendingOpDatasetTlvs, [aRequest](otError aError, const std::string &aErrorInfo) mutable {
        OT_UNUSED_VARIABLE(aErrorInfo);
        aRequest.ReplyOtResult(aError);
    });

exit:
    if (error != OT_ERROR_NONE)
    {
        aRequest.ReplyOtResult(error);
    }
}

void DBusThreadObjectNcp::HostPowerStateHandler(DBusRequest &aRequest)
{
    otError error = OT_ERROR_NONE;
    uint8_t state = 0;
    auto    args  = std::tie(state);

    SuccessOrExit(DBusMessageToTuple(*aRequest.GetMessage(), args), error = OT_ERROR_INVALID_ARGS);

    mHost.SetHostPowerState(state, [aRequest](otError aError, const std::string &aErrorInfo) mutable {
        OT_UNUSED_VARIABLE(aErrorInfo);
        aRequest.ReplyOtResult(aError);
    });

exit:
    if (error != OT_ERROR_NONE)
    {
        aRequest.ReplyOtResult(error);
    }
}

#if OTBR_ENABLE_EPSKC
void DBusThreadObjectNcp::EnableEphemeralKeyModeHandler(DBusRequest &aRequest)
{
    otError error = OT_ERROR_NONE;
    bool    enable;
    auto    args = std::tie(enable);
    SuccessOrExit(DBusMessageToTuple(*aRequest.GetMessage(), args), error = OT_ERROR_INVALID_ARGS);

    mHost.EnableEphemeralKey(enable, [aRequest](otError aError, const std::string &aErrorInfo) mutable {
        OT_UNUSED_VARIABLE(aErrorInfo);
        aRequest.ReplyOtResult(aError);
    });
exit:
    if (error != OT_ERROR_NONE)
    {
        aRequest.ReplyOtResult(error);
    }
}

void DBusThreadObjectNcp::ActivateEphemeralKeyModeHandler(DBusRequest &aRequest)
{
    otError     error    = OT_ERROR_NONE;
    uint32_t    lifetime = 0;
    std::string ePskc;
    auto        args = std::tie(lifetime);

    SuccessOrExit(DBusMessageToTuple(*aRequest.GetMessage(), args), error = OT_ERROR_INVALID_ARGS);

    VerifyOrExit(lifetime <= OT_BORDER_AGENT_MAX_EPHEMERAL_KEY_TIMEOUT, error = OT_ERROR_INVALID_ARGS);
    SuccessOrExit(mBorderAgent.CreateEphemeralKey(ePskc), error = OT_ERROR_INVALID_ARGS);
    otbrLogInfo("Created Ephemeral Key: %s", ePskc.c_str());

    mHost.ActivateEphemeralKey(ePskc.c_str(), lifetime, OTBR_CONFIG_BORDER_AGENT_MESHCOP_E_UDP_PORT,
                               [aRequest](otError aError, const std::string &aErrorInfo) mutable {
                                   OT_UNUSED_VARIABLE(aErrorInfo);
                                   aRequest.ReplyOtResult(aError);
                               });

exit:
    if (error != OT_ERROR_NONE)
    {
        aRequest.ReplyOtResult(error);
    }
}

void DBusThreadObjectNcp::DeactivateEphemeralKeyModeHandler(DBusRequest &aRequest)
{
    otError error = OT_ERROR_NONE;
    bool    retain_active_session;
    auto    args = std::tie(retain_active_session);

    SuccessOrExit(DBusMessageToTuple(*aRequest.GetMessage(), args), error = OT_ERROR_INVALID_ARGS);

    mHost.DeactivateEphemeralKey(retain_active_session,
                                 [aRequest](otError aError, const std::string &aErrorInfo) mutable {
                                     OT_UNUSED_VARIABLE(aErrorInfo);
                                     aRequest.ReplyOtResult(aError);
                                 });

exit:
    if (error != OT_ERROR_NONE)
    {
        aRequest.ReplyOtResult(error);
    }
}
#endif

} // namespace DBus
} // namespace otbr
