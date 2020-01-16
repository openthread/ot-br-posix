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

#include <assert.h>
#include <stdio.h>

#include <dbus/dbus.h>

#include "dbus/dbus_object.hpp"

using std::placeholders::_1;

namespace otbr {
namespace dbus {

DBusObject::DBusObject(DBusConnection *aConnection, const std::string &aObjectPath)
    : mConnection(aConnection)
    , mObjectPath(aObjectPath)
{
}

otbrError DBusObject::Init(void)
{
    otbrError            err    = OTBR_ERROR_NONE;
    DBusObjectPathVTable vTable = {
        .unregister_function = nullptr,
        .message_function    = DBusObject::sMessageHandler,
    };

    VerifyOrExit(dbus_connection_register_object_path(mConnection, mObjectPath.c_str(), &vTable, this),
                 err = OTBR_ERROR_DBUS);
    RegisterMethod(DBUS_INTERFACE_PROPERTIES, DBUS_PROPERTY_GET_METHOD,
                   std::bind(&DBusObject::GetPropertyMethodHandler, this, _1));
    RegisterMethod(DBUS_INTERFACE_PROPERTIES, DBUS_PROPERTY_SET_METHOD,
                   std::bind(&DBusObject::SetPropertyMethodHandler, this, _1));
    RegisterMethod(DBUS_INTERFACE_PROPERTIES, DBUS_PROPERTY_GET_ALL_METHOD,
                   std::bind(&DBusObject::GetAllPropertiesMethodHandler, this, _1));
exit:
    return err;
}

void DBusObject::RegisterMethod(const std::string &      aInterfaceName,
                                const std::string &      aMethodName,
                                const MethodHandlerType &aHandler)
{
    std::string fullPath = aInterfaceName + "." + aMethodName;

    assert(mMethodHandlers.find(fullPath) == mMethodHandlers.end());
    mMethodHandlers.emplace(fullPath, aHandler);
}

void DBusObject::RegisterGetPropertyHandler(const std::string &        aInterfaceName,
                                            const std::string &        aPropertyName,
                                            const PropertyHandlerType &aHandler)
{
    mGetPropertyHandlers[aInterfaceName].emplace(aPropertyName, aHandler);
}

void DBusObject::RegisterSetPropertyHandler(const std::string &        aInterfaceName,
                                            const std::string &        aPropertyName,
                                            const PropertyHandlerType &aHandler)
{
    std::string fullPath = aInterfaceName + "." + aPropertyName;

    assert(mSetPropertyHandlers.find(fullPath) == mSetPropertyHandlers.end());
    mSetPropertyHandlers.emplace(fullPath, aHandler);
}

DBusHandlerResult DBusObject::sMessageHandler(DBusConnection *aConnection, DBusMessage *aMessage, void *aData)
{
    DBusObject *server = reinterpret_cast<DBusObject *>(aData);

    return server->MessageHandler(aConnection, aMessage);
}

DBusHandlerResult DBusObject::MessageHandler(DBusConnection *aConnection, DBusMessage *aMessage)
{
    DBusHandlerResult handled = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    DBusRequest       request(aConnection, aMessage);
    std::string       interface  = dbus_message_get_interface(aMessage);
    std::string       memberName = interface + "." + dbus_message_get_member(aMessage);

    if (dbus_message_get_type(aMessage) == DBUS_MESSAGE_TYPE_METHOD_CALL &&
        mMethodHandlers.find(memberName) != mMethodHandlers.end())
    {
        mMethodHandlers.at(memberName)(request);
        handled = DBUS_HANDLER_RESULT_HANDLED;
    }

    return handled;
}

void DBusObject::GetPropertyMethodHandler(DBusRequest &aRequest)
{
    UniqueDBusMessage reply = MakeUniqueDBusMessage(dbus_message_new_method_return(aRequest.GetMessage().get()));

    DBusMessageIter iter;
    std::string     interfaceName;
    std::string     propertyName;
    otError         err = OT_ERROR_NONE;

    VerifyOrExit(reply != nullptr, err = OT_ERROR_FAILED);
    VerifyOrExit(dbus_message_iter_init(aRequest.GetMessage().get(), &iter), err = OT_ERROR_FAILED);
    VerifyOrExit(DBusMessageExtract(&iter, interfaceName) == OTBR_ERROR_NONE, err = OT_ERROR_FAILED);
    VerifyOrExit(DBusMessageExtract(&iter, propertyName) == OTBR_ERROR_NONE, err = OT_ERROR_FAILED);

    VerifyOrExit(mGetPropertyHandlers.find(interfaceName) != mGetPropertyHandlers.end(), err = OT_ERROR_NOT_FOUND);
    {
        auto &          interfaceHandlers = mGetPropertyHandlers.at(interfaceName);
        DBusMessageIter replyIter;

        VerifyOrExit(interfaceHandlers.find(propertyName) != interfaceHandlers.end(), err = OT_ERROR_NOT_FOUND);
        dbus_message_iter_init_append(reply.get(), &replyIter);
        SuccessOrExit(err = interfaceHandlers.at(propertyName)(replyIter));
    }
exit:
    if (err == OT_ERROR_NONE)
    {
        dbus_connection_send(aRequest.GetConnection().get(), reply.get(), nullptr);
    }
    else
    {
        aRequest.ReplyOtResult(err);
    }
}

void DBusObject::GetAllPropertiesMethodHandler(DBusRequest &aRequest)
{
    UniqueDBusMessage reply = MakeUniqueDBusMessage(dbus_message_new_method_return(aRequest.GetMessage().get()));
    DBusMessageIter   iter, subIter, dictEntryIter;
    std::string       interfaceName;
    auto              args = std::tie(interfaceName);
    otError           err  = OT_ERROR_NONE;

    VerifyOrExit(reply != nullptr, err = OT_ERROR_FAILED);
    VerifyOrExit(DBusMessageToTuple(aRequest.GetMessage(), args) == OTBR_ERROR_NONE, err = OT_ERROR_FAILED);
    VerifyOrExit(mGetPropertyHandlers.find(interfaceName) != mGetPropertyHandlers.end(), err = OT_ERROR_NOT_FOUND);
    dbus_message_iter_init_append(reply.get(), &iter);

    for (auto &p : mGetPropertyHandlers.at(interfaceName))
    {
        VerifyOrExit(dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY,
                                                      "{" DBUS_TYPE_STRING_AS_STRING DBUS_TYPE_VARIANT_AS_STRING "}",
                                                      &subIter),
                     err = OT_ERROR_FAILED);
        VerifyOrExit(dbus_message_iter_open_container(&subIter, DBUS_TYPE_DICT_ENTRY, nullptr, &dictEntryIter),
                     err = OT_ERROR_FAILED);
        VerifyOrExit(DBusMessageEncode(&dictEntryIter, p.first) == OTBR_ERROR_NONE, err = OT_ERROR_FAILED);

        SuccessOrExit(err = p.second(dictEntryIter));

        VerifyOrExit(dbus_message_iter_close_container(&subIter, &dictEntryIter), err = OT_ERROR_FAILED);
        VerifyOrExit(dbus_message_iter_close_container(&iter, &subIter));
    }

exit:
    if (err == OT_ERROR_NONE)
    {
        dbus_connection_send(aRequest.GetConnection().get(), reply.get(), nullptr);
    }
    else
    {
        aRequest.ReplyOtResult(err);
    }
}

void DBusObject::SetPropertyMethodHandler(DBusRequest &aRequest)
{
    DBusMessageIter iter;
    std::string     interfaceName;
    std::string     propertyName;
    std::string     propertyFullPath;
    otError         err = OT_ERROR_NONE;

    VerifyOrExit(dbus_message_iter_init(aRequest.GetMessage().get(), &iter), err = OT_ERROR_FAILED);
    VerifyOrExit(DBusMessageExtract(&iter, interfaceName) == OTBR_ERROR_NONE, err = OT_ERROR_FAILED);
    VerifyOrExit(DBusMessageExtract(&iter, propertyName) == OTBR_ERROR_NONE, err = OT_ERROR_FAILED);

    propertyFullPath = interfaceName + "." + propertyName;
    VerifyOrExit(mSetPropertyHandlers.find(propertyFullPath) != mSetPropertyHandlers.end(), err = OT_ERROR_NOT_FOUND);
    err = mSetPropertyHandlers.at(propertyFullPath)(iter);
exit:
    aRequest.ReplyOtResult(err);
    return;
}

DBusObject::~DBusObject(void)
{
}

} // namespace dbus
} // namespace otbr
