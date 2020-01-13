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
#include "dbus/constants.hpp"

namespace otbr {
namespace dbus {

otbrError DBusMessageExtract(DBusMessageIter *aIter, bool &aValue)
{
    otbrError err = OTBR_ERROR_NONE;
    uint8_t   val;

    SuccessOrExit(err = DBusMessageExtract(aIter, val));
    aValue = (val != 0);
exit:
    return err;
}

otbrError DBusMessageExtract(DBusMessageIter *aIter, std::string &aValue)
{
    const char *buf;
    otbrError   err = OTBR_ERROR_NONE;

    VerifyOrExit(dbus_message_iter_get_arg_type(aIter) == DBUS_TYPE_STRING, err = OTBR_ERROR_DBUS);
    dbus_message_iter_get_basic(aIter, &buf);
    dbus_message_iter_next(aIter);
    aValue = buf;
exit:
    return err;
}

otbrError DBusMessageExtract(DBusMessageIter *aIter, std::vector<uint8_t> &aValue)
{
    return DBusMessageExtractPrimitive(aIter, aValue);
}

otbrError DBusMessageExtract(DBusMessageIter *aIter, std::vector<uint16_t> &aValue)
{
    return DBusMessageExtractPrimitive(aIter, aValue);
}

otbrError DBusMessageExtract(DBusMessageIter *aIter, std::vector<uint32_t> &aValue)
{
    return DBusMessageExtractPrimitive(aIter, aValue);
}

otbrError DBusMessageExtract(DBusMessageIter *aIter, std::vector<uint64_t> &aValue)
{
    return DBusMessageExtractPrimitive(aIter, aValue);
}

otbrError DBusMessageExtract(DBusMessageIter *aIter, std::vector<int16_t> &aValue)
{
    return DBusMessageExtractPrimitive(aIter, aValue);
}

otbrError DBusMessageExtract(DBusMessageIter *aIter, std::vector<int32_t> &aValue)
{
    return DBusMessageExtractPrimitive(aIter, aValue);
}

otbrError DBusMessageExtract(DBusMessageIter *aIter, std::vector<int64_t> &aValue)
{
    return DBusMessageExtractPrimitive(aIter, aValue);
}

otbrError DBusMessageEncode(DBusMessageIter *aIter, bool aValue)
{
    uint8_t val = aValue;

    return DBusMessageEncode(aIter, val);
}

otbrError DBusMessageEncode(DBusMessageIter *aIter, const std::string &aValue)
{
    otbrError   err = OTBR_ERROR_NONE;
    const char *buf = aValue.c_str();

    VerifyOrExit(dbus_message_iter_append_basic(aIter, DBUS_TYPE_STRING, &buf), err = OTBR_ERROR_DBUS);
exit:
    return err;
}

otbrError DBusMessageEncode(DBusMessageIter *aIter, const char *aValue)
{
    otbrError err = OTBR_ERROR_NONE;

    VerifyOrExit(dbus_message_iter_append_basic(aIter, DBUS_TYPE_STRING, &aValue), err = OTBR_ERROR_DBUS);
exit:
    return err;
}

otbrError DBusMessageEncode(DBusMessageIter *aIter, const std::vector<uint8_t> &aValue)
{
    return DBusMessageEncodePrimitive(aIter, aValue);
}

otbrError DBusMessageEncode(DBusMessageIter *aIter, const std::vector<uint16_t> &aValue)
{
    return DBusMessageEncodePrimitive(aIter, aValue);
}

otbrError DBusMessageEncode(DBusMessageIter *aIter, const std::vector<uint32_t> &aValue)
{
    return DBusMessageEncodePrimitive(aIter, aValue);
}

otbrError DBusMessageEncode(DBusMessageIter *aIter, const std::vector<uint64_t> &aValue)
{
    return DBusMessageEncodePrimitive(aIter, aValue);
}

otbrError DBusMessageEncode(DBusMessageIter *aIter, const std::vector<int16_t> &aValue)
{
    return DBusMessageEncodePrimitive(aIter, aValue);
}

otbrError DBusMessageEncode(DBusMessageIter *aIter, const std::vector<int32_t> &aValue)
{
    return DBusMessageEncodePrimitive(aIter, aValue);
}

otbrError DBusMessageEncode(DBusMessageIter *aIter, const std::vector<int64_t> &aValue)
{
    return DBusMessageEncodePrimitive(aIter, aValue);
}

} // namespace dbus
} // namespace otbr
