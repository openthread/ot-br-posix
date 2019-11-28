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
#include "dbus/dbus_object.hpp"

using namespace ot::dbus;
using namespace std::placeholders;

class TestObject : public DBusObject
{
public:
    TestObject(DBusConnection *aConnection)
        : DBusObject(aConnection, "/org/otbr/testobj")
    {
        Register("org.otbr.Ping", std::bind(&TestObject::PingHandler, this, _1, _2));
    }

private:
    void PingHandler(SharedDBusConnection aConnection, SharedDBusMessage aMessage)
    {
        uint32_t    id;
        std::string pingMessage;
        auto        args = std::tie(id, pingMessage);

        if (DBusMessageToTuple(aMessage.RawMessage(), args) == OTBR_ERROR_NONE)
        {
            Reply(std::move(aConnection), std::move(aMessage), std::make_tuple(id, pingMessage + "Pong"));
        }
        else
        {
            Reply(std::move(aConnection), std::move(aMessage), std::make_tuple("hello"));
        }
        return;
    }
};

int main()
{
    int       requestReply;
    DBusError dbusErr;

    dbus_error_init(&dbusErr);

    DBusConnection *connection = dbus_bus_get(DBUS_BUS_SESSION, &dbusErr);
    VerifyOrExit(connection != nullptr);
    dbus_bus_register(connection, &dbusErr);

    requestReply = dbus_bus_request_name(connection, "org.otbr.TestServer", DBUS_NAME_FLAG_REPLACE_EXISTING, &dbusErr);
    VerifyOrExit(requestReply == DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER ||
                 requestReply == DBUS_REQUEST_NAME_REPLY_ALREADY_OWNER);

    {
        TestObject s(connection);
        s.Launch();

        while (true)
        {
            dbus_connection_read_write_dispatch(connection, 0);
        }
    }
exit:
    dbus_connection_unref(connection);
    return 0;
}
