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

#ifndef DBUS_MESSAGE_HELPER_HPP_
#define DBUS_MESSAGE_HELPER_HPP_

#include <tuple>

#include "dbus/dbus.h"

#include "common/code_utils.hpp"
#include "common/types.hpp"

namespace ot {
namespace dbus {

inline otbrError DBusMessageExtract(DBusMessageIter *aIter, uint8_t &aValue)
{
    otbrError err = OTBR_ERROR_DBUS;

    VerifyOrExit(dbus_message_iter_get_arg_type(aIter) == DBUS_TYPE_BYTE);
    dbus_message_iter_get_basic(aIter, &aValue);
    err = OTBR_ERROR_NONE;
exit:
    return err;
}

inline otbrError DBusMessageExtract(DBusMessageIter *aIter, uint16_t &aValue)
{
    otbrError err = OTBR_ERROR_DBUS;

    VerifyOrExit(dbus_message_iter_get_arg_type(aIter) == DBUS_TYPE_UINT16);
    dbus_message_iter_get_basic(aIter, &aValue);
    err = OTBR_ERROR_NONE;
exit:
    return err;
}

inline otbrError DBusMessageExtract(DBusMessageIter *aIter, uint32_t &aValue)
{
    otbrError err = OTBR_ERROR_DBUS;

    VerifyOrExit(dbus_message_iter_get_arg_type(aIter) == DBUS_TYPE_UINT32);
    dbus_message_iter_get_basic(aIter, &aValue);
    err = OTBR_ERROR_NONE;
exit:
    return err;
}

inline otbrError DBusMessageExtract(DBusMessageIter *aIter, uint64_t &aValue)
{
    otbrError err = OTBR_ERROR_DBUS;

    VerifyOrExit(dbus_message_iter_get_arg_type(aIter) == DBUS_TYPE_UINT64);
    dbus_message_iter_get_basic(aIter, &aValue);
    err = OTBR_ERROR_NONE;
exit:
    return err;
}

inline otbrError DBusMessageExtract(DBusMessageIter *aIter, bool &aValue)
{
    otbrError err = OTBR_ERROR_DBUS;

    VerifyOrExit(dbus_message_iter_get_arg_type(aIter) == DBUS_TYPE_BOOLEAN);
    dbus_message_iter_get_basic(aIter, &aValue);
    err = OTBR_ERROR_NONE;
exit:
    return err;
}

inline otbrError DBusMessageExtract(DBusMessageIter *aIter, int16_t &aValue)
{
    otbrError err = OTBR_ERROR_DBUS;

    VerifyOrExit(dbus_message_iter_get_arg_type(aIter) == DBUS_TYPE_INT16);
    dbus_message_iter_get_basic(aIter, &aValue);
    err = OTBR_ERROR_NONE;
exit:
    return err;
}

inline otbrError DBusMessageExtract(DBusMessageIter *aIter, int32_t &aValue)
{
    otbrError err = OTBR_ERROR_DBUS;

    VerifyOrExit(dbus_message_iter_get_arg_type(aIter) == DBUS_TYPE_INT32);
    dbus_message_iter_get_basic(aIter, &aValue);
    err = OTBR_ERROR_NONE;
exit:
    return err;
}

inline otbrError DBusMessageExtract(DBusMessageIter *aIter, int64_t &aValue)
{
    otbrError err = OTBR_ERROR_DBUS;

    VerifyOrExit(dbus_message_iter_get_arg_type(aIter) == DBUS_TYPE_INT64);
    dbus_message_iter_get_basic(aIter, &aValue);
    err = OTBR_ERROR_NONE;
exit:
    return err;
}

inline otbrError DBusMessageExtract(DBusMessageIter *aIter, std::string &aValue)
{
    const char *buf;
    otbrError   err = OTBR_ERROR_DBUS;

    VerifyOrExit(dbus_message_iter_get_arg_type(aIter) == DBUS_TYPE_STRING);
    dbus_message_iter_get_basic(aIter, &buf);
    aValue = buf;
    err    = OTBR_ERROR_NONE;
exit:
    return err;
}

inline otbrError DBusMessageEncode(DBusMessageIter *aIter, uint8_t aValue)
{
    otbrError err = OTBR_ERROR_DBUS;
    VerifyOrExit(dbus_message_iter_append_basic(aIter, DBUS_TYPE_BYTE, &aValue) == true);
    err = OTBR_ERROR_NONE;
exit:
    return err;
}

inline otbrError DBusMessageEncode(DBusMessageIter *aIter, uint16_t aValue)
{
    otbrError err = OTBR_ERROR_DBUS;

    VerifyOrExit(dbus_message_iter_append_basic(aIter, DBUS_TYPE_UINT16, &aValue) == true);
    err = OTBR_ERROR_NONE;
exit:
    return err;
}

inline otbrError DBusMessageEncode(DBusMessageIter *aIter, uint32_t aValue)
{
    otbrError err = OTBR_ERROR_DBUS;

    VerifyOrExit(dbus_message_iter_append_basic(aIter, DBUS_TYPE_UINT32, &aValue) == true);
    err = OTBR_ERROR_NONE;
exit:
    return err;
}

inline otbrError DBusMessageEncode(DBusMessageIter *aIter, uint64_t aValue)
{
    otbrError err = OTBR_ERROR_DBUS;

    VerifyOrExit(dbus_message_iter_append_basic(aIter, DBUS_TYPE_UINT64, &aValue) == true);
    err = OTBR_ERROR_NONE;
exit:
    return err;
}

inline otbrError DBusMessageEncode(DBusMessageIter *aIter, bool aValue)
{
    otbrError err = OTBR_ERROR_DBUS;

    VerifyOrExit(dbus_message_iter_append_basic(aIter, DBUS_TYPE_BOOLEAN, &aValue) == true);
    err = OTBR_ERROR_NONE;
exit:
    return err;
}

inline otbrError DBusMessageEncode(DBusMessageIter *aIter, int16_t aValue)
{
    otbrError err = OTBR_ERROR_DBUS;

    VerifyOrExit(dbus_message_iter_append_basic(aIter, DBUS_TYPE_INT16, &aValue) == true);
    err = OTBR_ERROR_NONE;
exit:
    return err;
}

inline otbrError DBusMessageEncode(DBusMessageIter *aIter, int32_t aValue)
{
    otbrError err = OTBR_ERROR_DBUS;

    VerifyOrExit(dbus_message_iter_append_basic(aIter, DBUS_TYPE_INT32, &aValue) == true);
    err = OTBR_ERROR_NONE;
exit:
    return err;
}

inline otbrError DBusMessageEncode(DBusMessageIter *aIter, int64_t aValue)
{
    otbrError err = OTBR_ERROR_DBUS;

    VerifyOrExit(dbus_message_iter_append_basic(aIter, DBUS_TYPE_INT64, &aValue) == true);
    err = OTBR_ERROR_NONE;
exit:
    return err;
}

inline otbrError DBusMessageEncode(DBusMessageIter *aIter, const std::string &aValue)
{
    otbrError   err = OTBR_ERROR_DBUS;
    const char *buf = aValue.c_str();

    VerifyOrExit(dbus_message_iter_append_basic(aIter, DBUS_TYPE_STRING, &buf) == true);
    err = OTBR_ERROR_NONE;
exit:
    return err;
}

inline otbrError DBusMessageEncode(DBusMessageIter *aIter, const char *aValue)
{
    otbrError err = OTBR_ERROR_DBUS;

    VerifyOrExit(dbus_message_iter_append_basic(aIter, DBUS_TYPE_STRING, &aValue) == true);
    err = OTBR_ERROR_NONE;
exit:
    return err;
}

template <size_t I, typename... Args> struct ElementType
{
    using ValueType         = typename std::tuple_element<I, std::tuple<Args...>>::type;
    using NonconstValueType = typename std::remove_cv<ValueType>::type;
    using RawValueType      = typename std::remove_reference<NonconstValueType>::type;
};

template <size_t I, size_t N, typename... Args> class DBusMessageIterFor
{
public:
    static otbrError ConvertToTuple(DBusMessageIter *aIter, std::tuple<Args...> &aValues)
    {
        using RawValueType = typename ElementType<N - I, Args...>::RawValueType;
        RawValueType &val  = std::get<N - I>(aValues);
        otbrError     err  = DBusMessageExtract(aIter, val);

        SuccessOrExit(err);
        VerifyOrExit(dbus_message_iter_next(aIter) == true, err = OTBR_ERROR_DBUS);
        err = DBusMessageIterFor<I - 1, N, Args...>::ConvertToTuple(aIter, aValues);
    exit:
        return err;
    }

    static otbrError ConvertToDBusMessage(DBusMessageIter *aIter, std::tuple<Args...> &aValues)
    {
        otbrError err = DBusMessageEncode(aIter, std::get<N - I>(aValues));

        SuccessOrExit(err);
        err = DBusMessageIterFor<I - 1, N, Args...>::ConvertToDBusMessage(aIter, aValues);
    exit:
        return err;
    }

    static constexpr decltype(auto) MakeDefaultTuple(void)
    {
        using RawValueType = typename ElementType<N - I, Args...>::RawValueType;
        return std::tuple_cat(std::make_tuple(RawValueType()),
                              DBusMessageIterFor<I - 1, N, Args...>::MakeDefaultTuple());
    }
};

template <size_t N, typename... Args> class DBusMessageIterFor<1, N, Args...>
{
public:
    // XXX: not sure how goto will affect compiler's inline
    static otbrError ConvertToTuple(DBusMessageIter *aIter, std::tuple<Args...> &aValues)
    {
        using RawValueType = typename ElementType<N - 1, Args...>::RawValueType;
        RawValueType &val  = std::get<N - 1>(aValues);
        otbrError     err  = DBusMessageExtract(aIter, val);

        return err;
    }

    static otbrError ConvertToDBusMessage(DBusMessageIter *aIter, std::tuple<Args...> &aValues)
    {
        otbrError err = DBusMessageEncode(aIter, std::get<N - 1>(aValues));

        return err;
    }

    static constexpr decltype(auto) MakeDefaultTuple(void)
    {
        using RawValueType = typename ElementType<N - 1, Args...>::RawValueType;
        return std::make_tuple(RawValueType());
    }
};

template <typename... Args> otbrError DBusMessageToTuple(DBusMessage *aMsg, std::tuple<Args...> &aValues)
{
    otbrError       err = OTBR_ERROR_NONE;
    DBusMessageIter iter;

    VerifyOrExit(dbus_message_iter_init(aMsg, &iter) == true, err = OTBR_ERROR_DBUS);

    err = DBusMessageIterFor<sizeof...(Args), sizeof...(Args), Args...>::ConvertToTuple(&iter, aValues);
exit:
    return err;
}

template <typename... Args> otbrError TupleToDBusMessage(DBusMessage *aMsg, std::tuple<Args...> &aValues)
{
    otbrError       err = OTBR_ERROR_NONE;
    DBusMessageIter iter;

    dbus_message_iter_init_append(aMsg, &iter);

    err = DBusMessageIterFor<sizeof...(Args), sizeof...(Args), Args...>::ConvertToDBusMessage(&iter, aValues);
    return err;
}

template <typename... Args> constexpr decltype(auto) MakeDefaultTuple(void)
{
    return DBusMessageIterFor<sizeof...(Args), sizeof...(Args), Args...>::MakeDefaultTuple();
}

} // namespace dbus
} // namespace ot

#endif
