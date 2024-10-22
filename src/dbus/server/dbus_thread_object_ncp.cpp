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

#include "common/api_strings.hpp"
#include "common/byteswap.hpp"
#include "common/code_utils.hpp"
#include "dbus/common/constants.hpp"
#include "dbus/server/dbus_agent.hpp"
#include "utils/thread_helper.hpp"

using std::placeholders::_1;
using std::placeholders::_2;

namespace otbr {
namespace DBus {

DBusThreadObjectNcp::DBusThreadObjectNcp(DBusConnection     &aConnection,
                                         const std::string  &aInterfaceName,
                                         otbr::Ncp::NcpHost &aHost)
    : DBusObject(&aConnection, OTBR_DBUS_OBJECT_PREFIX + aInterfaceName)
    , mHost(aHost)
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
    mHost.Leave([aRequest](otError aError, const std::string &aErrorInfo) mutable {
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

    SuccessOrExit(error = agent::ThreadHelper::ProcessDatasetForMigration(pendingOpDatasetTlvs, delayInMilli));

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

} // namespace DBus
} // namespace otbr
