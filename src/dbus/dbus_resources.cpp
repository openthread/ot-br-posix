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

#include <utility>

#include "dbus/dbus_resources.hpp"

namespace ot {
namespace dbus {

SharedDBusConnection::SharedDBusConnection(DBusConnection *aConnection)
    : mConnection(aConnection)
{
    dbus_connection_ref(mConnection);
}

SharedDBusConnection::SharedDBusConnection(const SharedDBusConnection &aOther)
{
    CopyFrom(aOther);
}

SharedDBusConnection &SharedDBusConnection::operator=(const SharedDBusConnection &aOther)
{
    return CopyFrom(aOther);
}

SharedDBusConnection &SharedDBusConnection::CopyFrom(const SharedDBusConnection &aOther)
{
    this->mConnection = aOther.mConnection;
    dbus_connection_ref(mConnection);
    return *this;
}

SharedDBusConnection::SharedDBusConnection(SharedDBusConnection &&aOther)
{
    Reset(std::move(aOther));
}

SharedDBusConnection &SharedDBusConnection::operator=(SharedDBusConnection &&aOther)
{
    return Reset(std::move(aOther));
}

SharedDBusConnection &SharedDBusConnection::Reset(SharedDBusConnection &&aOther)
{
    this->mConnection  = aOther.mConnection;
    aOther.mConnection = nullptr;
    return *this;
}

DBusConnection *SharedDBusConnection::operator->(void)
{
    return RawConnection();
}

DBusConnection *SharedDBusConnection::RawConnection(void)
{
    return mConnection;
}

SharedDBusConnection::~SharedDBusConnection()
{
    if (mConnection != nullptr)
    {
        dbus_connection_unref(mConnection);
        mConnection = nullptr;
    }
}

SharedDBusMessage::SharedDBusMessage(DBusMessage *aMessage)
    : mMessage(aMessage)
{
    dbus_message_ref(mMessage);
}

SharedDBusMessage::SharedDBusMessage(const SharedDBusMessage &aOther)
{
    CopyFrom(aOther);
}

SharedDBusMessage &SharedDBusMessage::operator=(const SharedDBusMessage &aOther)
{
    return CopyFrom(aOther);
}

SharedDBusMessage &SharedDBusMessage::CopyFrom(const SharedDBusMessage &aOther)
{
    this->mMessage = aOther.mMessage;
    dbus_message_ref(mMessage);
    return *this;
}

SharedDBusMessage::SharedDBusMessage(SharedDBusMessage &&aOther)
{
    Reset(std::move(aOther));
}

SharedDBusMessage &SharedDBusMessage::operator=(SharedDBusMessage &&aOther)
{
    return Reset(std::move(aOther));
}

SharedDBusMessage &SharedDBusMessage::Reset(SharedDBusMessage &&aOther)
{
    this->mMessage  = aOther.mMessage;
    aOther.mMessage = nullptr;
    return *this;
}

DBusMessage *SharedDBusMessage::operator->(void)
{
    return RawMessage();
}

DBusMessage *SharedDBusMessage::RawMessage(void)
{
    return mMessage;
}

SharedDBusMessage::~SharedDBusMessage()
{
    if (mMessage != nullptr)
    {
        dbus_message_unref(mMessage);
        mMessage = nullptr;
    }
}

} // namespace dbus
} // namespace ot
