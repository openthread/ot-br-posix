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

exit:
    return error;
}

void DBusThreadObjectNcp::AsyncGetDeviceRoleHandler(DBusRequest &aRequest)
{
    mHost.GetDeviceRole([this, aRequest](otError aError, otDeviceRole aRole) mutable {
        if (aError == OT_ERROR_NONE)
        {
            this->ReplyAsyncGetProperty(aRequest, GetDeviceRoleName(aRole));
        }
        else
        {
            aRequest.ReplyOtResult(aError);
        }
    });
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

} // namespace DBus
} // namespace otbr
