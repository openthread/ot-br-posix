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

#ifndef OTBR_DBUS_DBUS_RESOURCES_HPP_
#define OTBR_DBUS_DBUS_RESOURCES_HPP_

#include "dbus/dbus.h"

namespace ot {
namespace dbus {

class SharedDBusConnection final
{
public:
    explicit SharedDBusConnection(DBusConnection *aConnection);

    SharedDBusConnection(const SharedDBusConnection &);
    SharedDBusConnection &operator=(const SharedDBusConnection &);
    SharedDBusConnection(SharedDBusConnection &&);
    SharedDBusConnection &operator=(SharedDBusConnection &&);

    DBusConnection *operator->(void);
    DBusConnection *RawConnection(void);

    ~SharedDBusConnection(void);

private:
    SharedDBusConnection &CopyFrom(const SharedDBusConnection &aOther);
    SharedDBusConnection &Reset(SharedDBusConnection &&aOther);

    DBusConnection *mConnection;
};

class SharedDBusMessage final
{
public:
    explicit SharedDBusMessage(DBusMessage *aMessage);

    SharedDBusMessage(SharedDBusMessage &&aSharedMessage);
    SharedDBusMessage &operator=(SharedDBusMessage &&aSharedMessage);
    SharedDBusMessage(const SharedDBusMessage &);
    SharedDBusMessage &operator=(const SharedDBusMessage &);

    DBusMessage *operator->();
    DBusMessage *RawMessage();

    ~SharedDBusMessage();

private:
    SharedDBusMessage &CopyFrom(const SharedDBusMessage &aOther);
    SharedDBusMessage &Reset(SharedDBusMessage &&aOther);

    DBusMessage *mMessage;
};

} // namespace dbus
} // namespace ot

#endif // OTBR_DBUS_DBUS_RESOURCES_HPP_
