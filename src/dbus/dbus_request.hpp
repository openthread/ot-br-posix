/*
 *    Copyright (c) 2019, The OpenThread Authors.
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

#include "dbus/dbus_message_helper.hpp"
#include "dbus/dbus_resources.hpp"

namespace otbr {
namespace dbus {

/**
 * This class represents a incoming call for a d-bus method.
 *
 */
class DBusRequest
{
public:
    DBusRequest(DBusConnection *aConnection, DBusMessage *aMessage)
        : mConnection(aConnection)
        , mMessage(aMessage)
    {
    }

    /**
     * This method returns the message sent to call the d-bus method.
     *
     * @returns   The dbus message
     */
    SharedDBusMessage GetMessage(void) { return mMessage; }

    /**
     * This method replys to the d-bus method call.
     *
     * @param[in] aReply  The tuple to be sent
     *
     */
    template <typename... Args> void Reply(const std::tuple<Args...> &aReply)
    {
        DBusMessage *reply = dbus_message_new_method_return(mMessage.GetRaw());

        VerifyOrExit(reply != nullptr);
        VerifyOrExit(otbr::dbus::TupleToDBusMessage(*reply, aReply) == OTBR_ERROR_NONE);

        dbus_connection_send(mConnection.GetRaw(), reply, nullptr);
    exit:
        if (reply)
        {
            dbus_message_unref(reply);
        }
    }

private:
    SharedDBusConnection mConnection;
    SharedDBusMessage    mMessage;
};

} // namespace dbus
} // namespace otbr
