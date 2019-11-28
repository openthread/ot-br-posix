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

#include <utility>

#include <dbus/dbus.h>

namespace otbr {
namespace dbus {

template <typename T, T *(*refFunc)(T *), void (*unrefFunc)(T *)> class SharedDBusResource final
{
public:
    explicit SharedDBusResource(T *aResource)
        : mResource(aResource)
    {
        refFunc(aResource);
    }

    SharedDBusResource(const SharedDBusResource &aOther) { CopyFrom(aOther); }

    SharedDBusResource &operator=(const SharedDBusResource &aOther) { return CopyFrom(aOther); }

    SharedDBusResource(SharedDBusResource &&aOther) { Reset(std::move(aOther)); }

    SharedDBusResource &operator=(SharedDBusResource &&aOther) { return Reset(std::move(aOther)); }

    T *operator->(void) { return mResource; }
    T *GetRaw(void) { return mResource; }

    ~SharedDBusResource(void)
    {
        unrefFunc(mResource);
        mResource = nullptr;
    }

private:
    SharedDBusResource &CopyFrom(const SharedDBusResource &aOther)
    {
        this->mResource = aOther.mResource;
        refFunc(this->mResource);

        return *this;
    }

    SharedDBusResource &Reset(SharedDBusResource &&aOther)
    {
        this->mResource  = aOther.mResource;
        aOther.mResource = nullptr;

        return *this;
    }

    T *mResource;
};

using SharedDBusConnection = SharedDBusResource<DBusConnection, dbus_connection_ref, dbus_connection_unref>;
using SharedDBusMessage    = SharedDBusResource<DBusMessage, dbus_message_ref, dbus_message_unref>;

} // namespace dbus
} // namespace otbr

#endif // OTBR_DBUS_DBUS_RESOURCES_HPP_
