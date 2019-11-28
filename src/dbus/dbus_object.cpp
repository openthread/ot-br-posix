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

#include <dbus/dbus.h>
#include <stdio.h>

#include "dbus/dbus_object.hpp"

namespace ot {
namespace dbus {

DBusObjectPathVTable DBusObject::sVTable = {
    .message_function = DBusObject::sMessageHandler,
};

DBusObject::DBusObject(DBusConnection *aConnection, const std::string &aObjectPath)
    : mConnection(aConnection)
    , mObjectPath(aObjectPath)
{
}

otbrError DBusObject::Launch(void)
{
    otbrError err = OTBR_ERROR_NONE;

    VerifyOrExit(dbus_connection_register_object_path(mConnection, mObjectPath.c_str(), &DBusObject::sVTable, this) ==
                     true,
                 err = OTBR_ERROR_DBUS);
exit:
    return err;
}

void DBusObject::Register(const std::string &aMethodName, const HandlerType &handler)
{
    mHandlers.emplace(aMethodName, handler);
}

DBusHandlerResult DBusObject::sMessageHandler(DBusConnection *aConnection, DBusMessage *aMessage, void *aData)
{
    DBusObject *server = reinterpret_cast<DBusObject *>(aData);

    return server->MessageHandler(aConnection, aMessage);
}

DBusHandlerResult DBusObject::MessageHandler(DBusConnection *aConnection, DBusMessage *aMessage)
{
    DBusHandlerResult    handled = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    SharedDBusConnection connection(aConnection);
    SharedDBusMessage    msg(aMessage);
    std::string          interface  = dbus_message_get_interface(aMessage);
    std::string          memberName = interface + "." + dbus_message_get_member(aMessage);

    if (dbus_message_get_type(aMessage) == DBUS_MESSAGE_TYPE_METHOD_CALL && mHandlers.count(memberName) != 0)
    {
        mHandlers.at(memberName)(std::move(connection), std::move(msg));
        handled = DBUS_HANDLER_RESULT_HANDLED;
    }

    return handled;
}

DBusObject::~DBusObject()
{
}

} // namespace dbus
} // namespace ot
