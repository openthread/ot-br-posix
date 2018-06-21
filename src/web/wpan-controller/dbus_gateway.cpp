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
 *   This file implements the function of "configure gateway" function.
 */

#include "common/code_utils.hpp"
#include "utils/hex.hpp"

#include "dbus_gateway.hpp"

namespace ot {
namespace Dbus {

DBusGateway::DBusGateway(void)
    : mPreferredLifetime(0xFFFFFFFF)
    , mValidLifetime(0xFFFFFFFF)
{
}

int DBusGateway::ProcessReply(void)
{
    int          ret      = 0;
    const char * method   = "ConfigGateway";
    DBusMessage *messsage = NULL;
    DBusMessage *reply    = NULL;
    DBusError    error;

    dbus_error_init(&error);
    VerifyOrExit(GetConnection() != NULL, ret = kWpantundStatus_InvalidConnection);
    SetMethod(method);

    VerifyOrExit((messsage = GetMessage()) != NULL, ret = kWpantundStatus_InvalidMessage);

    dbus_message_append_args(messsage, DBUS_TYPE_BOOLEAN, &mDefaultRoute, DBUS_TYPE_INVALID);

    if (mPrefix)
    {
        if (strstr(mPrefix, ":"))
        {
            int bits = inet_pton(AF_INET6, mPrefix, mPrefixBytes);
            VerifyOrExit(bits > 0, ret = kWpantundStatus_InvalidArgument);
        }
        else
        {
            // DATA-style
            int length = ot::Utils::Hex2Bytes(mPrefix, mPrefixBytes, 8);
            VerifyOrExit(length > 0, ret = kWpantundStatus_InvalidArgument);
        }

        inet_ntop(AF_INET6, (const void *)&mPrefixBytes, mAddressString, sizeof(mAddressString));
    }

    mAddr = mPrefixBytes;

    dbus_message_append_args(messsage, DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE, &mAddr, 16, DBUS_TYPE_UINT32,
                             &mPreferredLifetime, DBUS_TYPE_UINT32, &mValidLifetime, DBUS_TYPE_INVALID);

    VerifyOrExit((reply = GetReply()) != NULL, ret = kWpantundStatus_InvalidReply);
    dbus_message_get_args(reply, &error, DBUS_TYPE_INT32, &ret, DBUS_TYPE_INVALID);

exit:
    if (dbus_error_is_set(&error))
    {
        otbrLog(OTBR_LOG_ERR, "gateway error: %s", error.message);
        dbus_error_free(&error);
    }
    free();
    return ret;
}

} // namespace Dbus
} // namespace ot
