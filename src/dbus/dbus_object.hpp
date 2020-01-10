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

#ifndef OTBR_DBUS_DBUS_OBJECT_HPP_
#define OTBR_DBUS_DBUS_OBJECT_HPP_

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include <dbus/dbus.h>

#include "common/code_utils.hpp"
#include "common/types.hpp"
#include "dbus/dbus_message_helper.hpp"
#include "dbus/dbus_request.hpp"
#include "dbus/dbus_resources.hpp"

namespace otbr {
namespace dbus {

/**
 * This class is a base class for implementing a d-bus object.
 *
 */
class DBusObject
{
public:
    using HandlerType = std::function<void(DBusRequest &)>;

    /**
     * The constructor of a d-bus object.
     *
     * @param[in]   aConnection   The dbus-connection the object bounds to
     * @param[in]   aObjectPath   The path of the object
     */
    DBusObject(DBusConnection *aConnection, const std::string &aObjectPath);

    /**
     * This method initializes the d-bus object.
     *
     * This method will register the object to the d-bus library.
     *
     * @retval  OTBR_ERROR_NONE   Successfully registered the object
     * @retval  OTBR_ERROR_DBUS   Failed to ragister an object
     */
    virtual otbrError Init(void);

    /**
     * This method registers the method handler.
     *
     * @param[in]   aInterfaceName    The interface name
     * @param[in]   aMethodName       The method name
     * @param[in]   aHandler          The method handler
     */
    void RegisterMethod(const std::string &aInterfaceName, const std::string &aMethodName, const HandlerType &aHandler);

    /**
     * This method sends a signal.
     *
     * @param[in]   aInterfaceName    The interface name
     * @param[in]   aSignalName       The signal name
     * @param[in]   aArgs             The tuple to be encoded into the signal
     */
    template <typename... FieldTypes>
    void Signal(const std::string &              aInterfaceName,
                const std::string &              aSignalName,
                const std::tuple<FieldTypes...> &aArgs)
    {
        DBusMessage *signalMsg =
            dbus_message_new_signal(mObjectPath.c_str(), aInterfaceName.c_str(), aSignalName.c_str());

        VerifyOrExit(signalMsg != nullptr);
        VerifyOrExit(otbr::dbus::TupleToDBusMessage(*signalMsg, aArgs) == OTBR_ERROR_NONE);

        dbus_connection_send(mConnection, signalMsg, nullptr);
    exit:
        if (signalMsg)
        {
            dbus_message_unref(signalMsg);
        }
    }

    /**
     * The destructor of a d-bus object.
     *
     */
    virtual ~DBusObject(void);

private:
    static DBusHandlerResult sMessageHandler(DBusConnection *aConnection, DBusMessage *aMessage, void *aData);
    DBusHandlerResult        MessageHandler(DBusConnection *aConnection, DBusMessage *aMessage);

    std::unordered_map<std::string, HandlerType> mHandlers;
    DBusConnection *                             mConnection;
    std::string                                  mObjectPath;
};

} // namespace dbus
} // namespace otbr

#endif // OTBR_DBUS_DBUS_SERVER_HPP_
