/*
 *  Copyright (c) 2017, The OpenThread Authors.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 *   This file implements the function of "form" a new Thread Network.
 */

#include "common/code_utils.hpp"

#include "dbus_base.hpp"
#include "dbus_form.hpp"
#include "wpan_controller.hpp"

namespace ot {
namespace Dbus {

int DBusForm::ProcessReply(void)
{
    int          ret = 0;
    char         path[DBUS_MAXIMUM_NAME_LENGTH + 1];
    const char  *iface = "com.nestlabs.WPANTunnelDriver";
    const char  *method = "Form";
    const char  *interfaceName = "wpan0";
    DBusMessage *messsage = NULL;
    DBusMessage *reply = NULL;
    DBusError    error;

    VerifyOrExit(GetConnection() != NULL, ret = kWpantundStatus_InvalidConnection);
    snprintf(path, sizeof(path), "%s/%s", WPAN_TUNNEL_DBUS_PATH,
             interfaceName);
    SetIface(iface);
    SetMethod(method);
    SetInterfaceName(interfaceName);
    SetPath(path);

    VerifyOrExit((messsage = GetMessage()) != NULL, ret = kWpantundStatus_InvalidMessage);
    VerifyOrExit(mNetworkName != NULL, ret = kWpantundStatus_InvalidArgument);

    dbus_message_append_args(messsage, DBUS_TYPE_STRING, &mNetworkName,
                             DBUS_TYPE_INT16, &mNodeType, DBUS_TYPE_UINT32, &mChannelMask,
                             DBUS_TYPE_INVALID);

    VerifyOrExit((reply = GetReply()) != NULL, ret = kWpantundStatus_InvalidReply);
    error = GetError();
    dbus_message_get_args(reply, &error, DBUS_TYPE_INT32, &ret,
                          DBUS_TYPE_INVALID);
    if (!ret)
    {
        syslog(LOG_ERR, "Successfully formed!\n");
    }
    else
    {
        syslog(LOG_ERR, "Error: Failed to formed! %s\n", error.message);
    }
exit:
    free();
    return ret;
}

} //namespace Dbus
} //ot
