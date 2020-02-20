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
 *   This file implements "set property" function.
 */

#ifndef DBUS_SET_HPP
#define DBUS_SET_HPP

#include <stdio.h>

#include <dbus/dbus.h>

#include "dbus_base.hpp"
#include "wpan_controller.hpp"

namespace otbr {
namespace Dbus {

enum
{
    kPropertyType_String,
    kPropertyType_Data,
};

class DBusSet : public DBusBase
{
public:
    const char *GetPropertyName(void) { return mPropertyName; }
    const char *GetPropertyValue(void) { return mPropertyValue; }

    void SetPropertyName(const char *aPropertyName) { mPropertyName = aPropertyName; }
    void SetPropertyValue(const char *aPropertyValue) { mPropertyValue = aPropertyValue; }

    void SetPropertyType(uint8_t aPropertyType)
    {
        mPropertyType = (aPropertyType == 0 ? kPropertyType_String : kPropertyType_Data);
    }

    int ProcessReply(void);

private:
    const char *mPropertyName;
    const char *mPropertyValue;
    uint8_t     mPropertyType;
};

} // namespace Dbus
} // namespace otbr
#endif // DBUS_SET_HPP
