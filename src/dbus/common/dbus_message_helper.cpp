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

#include "dbus/common/dbus_message_helper.hpp"

namespace otbr {
namespace DBus {

otbrError DbusMessageIterRecurse(DBusMessageIter *aIter, DBusMessageIter *aSubIter, int aType)
{
    otbrError error = OTBR_ERROR_NONE;

    VerifyOrExit(dbus_message_iter_get_arg_type(aIter) == aType, error = OTBR_ERROR_DBUS);
    dbus_message_iter_recurse(aIter, aSubIter);

exit:
    return error;
}

otbrError DBusMessageExtract(DBusMessageIter *aIter, bool &aValue)
{
    otbrError   error = OTBR_ERROR_DBUS;
    dbus_bool_t val;

    VerifyOrExit(dbus_message_iter_get_arg_type(aIter) == DBUS_TYPE_BOOLEAN);
    dbus_message_iter_get_basic(aIter, &val);
    dbus_message_iter_next(aIter);
    aValue = (val != 0);
    error  = OTBR_ERROR_NONE;

exit:
    return error;
}

otbrError DBusMessageExtract(DBusMessageIter *aIter, int8_t &aValue)
{
    otbrError error = OTBR_ERROR_NONE;
    uint8_t   val;

    SuccessOrExit(error = DBusMessageExtract(aIter, val));
    aValue = static_cast<int8_t>(val);
exit:
    return error;
}

otbrError DBusMessageExtract(DBusMessageIter *aIter, std::string &aValue)
{
    const char *buf;
    otbrError   error = OTBR_ERROR_NONE;

    VerifyOrExit(dbus_message_iter_get_arg_type(aIter) == DBUS_TYPE_STRING, error = OTBR_ERROR_DBUS);
    dbus_message_iter_get_basic(aIter, &buf);
    dbus_message_iter_next(aIter);
    aValue = buf;

exit:
    return error;
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
    dbus_bool_t val   = aValue ? 1 : 0;
    otbrError   error = OTBR_ERROR_NONE;

    VerifyOrExit(dbus_message_iter_append_basic(aIter, DBUS_TYPE_BOOLEAN, &val), error = OTBR_ERROR_DBUS);

exit:
    return error;
}

otbrError DBusMessageEncode(DBusMessageIter *aIter, int8_t aValue)
{
    return DBusMessageEncode(aIter, static_cast<uint8_t>(aValue));
}

otbrError DBusMessageEncode(DBusMessageIter *aIter, const std::string &aValue)
{
    otbrError   error = OTBR_ERROR_NONE;
    const char *buf   = aValue.c_str();

    VerifyOrExit(dbus_message_iter_append_basic(aIter, DBUS_TYPE_STRING, &buf), error = OTBR_ERROR_DBUS);

exit:
    return error;
}

otbrError DBusMessageEncode(DBusMessageIter *aIter, const char *aValue)
{
    otbrError error = OTBR_ERROR_NONE;

    VerifyOrExit(dbus_message_iter_append_basic(aIter, DBUS_TYPE_STRING, &aValue), error = OTBR_ERROR_DBUS);
exit:
    return error;
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

bool IsDBusMessageEmpty(DBusMessage &aMessage)
{
    DBusMessageIter iter;

    if (!dbus_message_iter_init(&aMessage, &iter))
    {
        return true;
    }
    return dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_INVALID;
}

otbrError DBusMessageCopy(DBusMessageIter *aDest, DBusMessageIter *aSrc)
{
    otbrError error   = OTBR_ERROR_NONE;
    int       argType = dbus_message_iter_get_arg_type(aSrc);
    char     *sig     = nullptr;

    VerifyOrExit(argType != DBUS_TYPE_INVALID, error = OTBR_ERROR_DBUS);

    if (dbus_type_is_basic(argType))
    {
        switch (argType)
        {
        case DBUS_TYPE_BYTE:
        {
            uint8_t val;

            dbus_message_iter_get_basic(aSrc, &val);
            VerifyOrExit(dbus_message_iter_append_basic(aDest, argType, &val), error = OTBR_ERROR_DBUS);
            break;
        }
        case DBUS_TYPE_BOOLEAN:
        {
            dbus_bool_t val;

            dbus_message_iter_get_basic(aSrc, &val);
            VerifyOrExit(dbus_message_iter_append_basic(aDest, argType, &val), error = OTBR_ERROR_DBUS);
            break;
        }
        case DBUS_TYPE_INT16:
        {
            int16_t val;

            dbus_message_iter_get_basic(aSrc, &val);
            VerifyOrExit(dbus_message_iter_append_basic(aDest, argType, &val), error = OTBR_ERROR_DBUS);
            break;
        }
        case DBUS_TYPE_UINT16:
        {
            uint16_t val;

            dbus_message_iter_get_basic(aSrc, &val);
            VerifyOrExit(dbus_message_iter_append_basic(aDest, argType, &val), error = OTBR_ERROR_DBUS);
            break;
        }
        case DBUS_TYPE_INT32:
        {
            int32_t val;

            dbus_message_iter_get_basic(aSrc, &val);
            VerifyOrExit(dbus_message_iter_append_basic(aDest, argType, &val), error = OTBR_ERROR_DBUS);
            break;
        }
        case DBUS_TYPE_UINT32:
        {
            uint32_t val;

            dbus_message_iter_get_basic(aSrc, &val);
            VerifyOrExit(dbus_message_iter_append_basic(aDest, argType, &val), error = OTBR_ERROR_DBUS);
            break;
        }
        case DBUS_TYPE_INT64:
        {
            int64_t val;

            dbus_message_iter_get_basic(aSrc, &val);
            VerifyOrExit(dbus_message_iter_append_basic(aDest, argType, &val), error = OTBR_ERROR_DBUS);
            break;
        }
        case DBUS_TYPE_UINT64:
        {
            uint64_t val;

            dbus_message_iter_get_basic(aSrc, &val);
            VerifyOrExit(dbus_message_iter_append_basic(aDest, argType, &val), error = OTBR_ERROR_DBUS);
            break;
        }
        case DBUS_TYPE_DOUBLE:
        {
            double val;

            dbus_message_iter_get_basic(aSrc, &val);
            VerifyOrExit(dbus_message_iter_append_basic(aDest, argType, &val), error = OTBR_ERROR_DBUS);
            break;
        }
        case DBUS_TYPE_STRING:
        case DBUS_TYPE_OBJECT_PATH:
        case DBUS_TYPE_SIGNATURE:
        {
            const char *val = nullptr;

            dbus_message_iter_get_basic(aSrc, &val);
            VerifyOrExit(dbus_message_iter_append_basic(aDest, argType, &val), error = OTBR_ERROR_DBUS);
            break;
        }
        case DBUS_TYPE_UNIX_FD:
        {
            int val;

            dbus_message_iter_get_basic(aSrc, &val);
            VerifyOrExit(dbus_message_iter_append_basic(aDest, argType, &val), error = OTBR_ERROR_DBUS);
            break;
        }
        default:
            ExitNow(error = OTBR_ERROR_DBUS);
        }
    }
    else if (dbus_type_is_container(argType))
    {
        DBusMessageIter subSrc;
        DBusMessageIter subDest;
        const char     *containedSig = nullptr;

        dbus_message_iter_recurse(aSrc, &subSrc);

        if (argType == DBUS_TYPE_ARRAY)
        {
            sig = dbus_message_iter_get_signature(aSrc);
            VerifyOrExit(sig != nullptr, error = OTBR_ERROR_DBUS);
            containedSig = sig + 1;
        }
        else if (argType == DBUS_TYPE_VARIANT)
        {
            sig = dbus_message_iter_get_signature(&subSrc);
            VerifyOrExit(sig != nullptr, error = OTBR_ERROR_DBUS);
            containedSig = sig;
        }

        VerifyOrExit(dbus_message_iter_open_container(aDest, argType, containedSig, &subDest), error = OTBR_ERROR_DBUS);

        while (dbus_message_iter_get_arg_type(&subSrc) != DBUS_TYPE_INVALID)
        {
            SuccessOrExit(error = DBusMessageCopy(&subDest, &subSrc));
            dbus_message_iter_next(&subSrc);
        }

        VerifyOrExit(dbus_message_iter_close_container(aDest, &subDest), error = OTBR_ERROR_DBUS);
    }
    else
    {
        ExitNow(error = OTBR_ERROR_DBUS);
    }

exit:
    if (sig != nullptr)
    {
        dbus_free(sig);
    }
    return error;
}

} // namespace DBus
} // namespace otbr
