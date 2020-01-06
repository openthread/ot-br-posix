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

#include <string>
#include <tuple>
#include <vector>

#include <dbus/dbus.h>

#include "common/code_utils.hpp"
#include "common/types.hpp"
#include "dbus/dbus_resources.hpp"

#include "openthread/border_router.h"
#include "openthread/link.h"
#include "openthread/thread.h"

namespace otbr {
namespace dbus {

otbrError DBusMessageExtract(DBusMessageIter *aIter, otError &aValue);
otbrError DBusMessageExtract(DBusMessageIter *aIter, otActiveScanResult &aValue);
otbrError DBusMessageEncode(DBusMessageIter *aIter, const otError &aValue);
otbrError DBusMessageEncode(DBusMessageIter *aIter, const otActiveScanResult &aValue);

template <typename T> struct DBusTypeTrait;

template <> struct DBusTypeTrait<uint8_t>
{
    static constexpr int         TYPE           = DBUS_TYPE_BYTE;
    static constexpr const char *TYPE_AS_STRING = DBUS_TYPE_BYTE_AS_STRING;
};

template <> struct DBusTypeTrait<uint16_t>
{
    static constexpr int         TYPE           = DBUS_TYPE_UINT16;
    static constexpr const char *TYPE_AS_STRING = DBUS_TYPE_UINT16_AS_STRING;
};

template <> struct DBusTypeTrait<uint32_t>
{
    static constexpr int         TYPE           = DBUS_TYPE_UINT32;
    static constexpr const char *TYPE_AS_STRING = DBUS_TYPE_UINT32_AS_STRING;
};

template <> struct DBusTypeTrait<uint64_t>
{
    static constexpr int         TYPE           = DBUS_TYPE_UINT64;
    static constexpr const char *TYPE_AS_STRING = DBUS_TYPE_UINT64_AS_STRING;
};

template <> struct DBusTypeTrait<int16_t>
{
    static constexpr int         TYPE           = DBUS_TYPE_INT16;
    static constexpr const char *TYPE_AS_STRING = DBUS_TYPE_INT16_AS_STRING;
};

template <> struct DBusTypeTrait<int32_t>
{
    static constexpr int         TYPE           = DBUS_TYPE_INT32;
    static constexpr const char *TYPE_AS_STRING = DBUS_TYPE_INT32_AS_STRING;
};

template <> struct DBusTypeTrait<int64_t>
{
    static constexpr int         TYPE           = DBUS_TYPE_INT64;
    static constexpr const char *TYPE_AS_STRING = DBUS_TYPE_INT64_AS_STRING;
};

otbrError DBusMessageEncode(DBusMessageIter *aIter, bool aValue);
otbrError DBusMessageEncode(DBusMessageIter *aIter, int8_t aValue);
otbrError DBusMessageEncode(DBusMessageIter *aIter, const std::string &aValue);
otbrError DBusMessageEncode(DBusMessageIter *aIter, const char *aValue);
otbrError DBusMessageEncode(DBusMessageIter *aIter, const std::vector<uint8_t> &aValue);
otbrError DBusMessageEncode(DBusMessageIter *aIter, const std::vector<uint16_t> &aValue);
otbrError DBusMessageEncode(DBusMessageIter *aIter, const std::vector<uint32_t> &aValue);
otbrError DBusMessageEncode(DBusMessageIter *aIter, const std::vector<uint64_t> &aValue);
otbrError DBusMessageEncode(DBusMessageIter *aIter, const std::vector<int16_t> &aValue);
otbrError DBusMessageEncode(DBusMessageIter *aIter, const std::vector<int32_t> &aValue);
otbrError DBusMessageEncode(DBusMessageIter *aIter, const std::vector<int64_t> &aValue);
otbrError DBusMessageExtract(DBusMessageIter *aIter, bool &aValue);
otbrError DBusMessageExtract(DBusMessageIter *aIter, int8_t &aValue);
otbrError DBusMessageExtract(DBusMessageIter *aIter, std::string &aValue);
otbrError DBusMessageExtract(DBusMessageIter *aIter, std::vector<uint8_t> &aValue);
otbrError DBusMessageExtract(DBusMessageIter *aIter, std::vector<uint16_t> &aValue);
otbrError DBusMessageExtract(DBusMessageIter *aIter, std::vector<uint32_t> &aValue);
otbrError DBusMessageExtract(DBusMessageIter *aIter, std::vector<uint64_t> &aValue);
otbrError DBusMessageExtract(DBusMessageIter *aIter, std::vector<int16_t> &aValue);
otbrError DBusMessageExtract(DBusMessageIter *aIter, std::vector<int32_t> &aValue);
otbrError DBusMessageExtract(DBusMessageIter *aIter, std::vector<int64_t> &aValue);

template <typename T> otbrError DBusMessageExtract(DBusMessageIter *aIter, T &aValue)
{
    otbrError err = OTBR_ERROR_DBUS;

    VerifyOrExit(dbus_message_iter_get_arg_type(aIter) == DBusTypeTrait<T>::TYPE);
    dbus_message_iter_get_basic(aIter, &aValue);
    dbus_message_iter_next(aIter);
    err = OTBR_ERROR_NONE;
exit:
    return err;
}

template <typename T> otbrError DBusMessageExtract(DBusMessageIter *aIter, std::vector<T> &aValue)
{
    otbrError err = OTBR_ERROR_NONE;
    uint32_t  size;

    VerifyOrExit(dbus_message_iter_get_arg_type(aIter) == DBUS_TYPE_UINT32, err = OTBR_ERROR_DBUS);
    SuccessOrExit(err = DBusMessageExtract(aIter, size));

    aValue.clear();
    for (uint32_t i = 0; i < size; i++)
    {
        aValue.emplace_back();
        DBusMessageExtract(aIter, aValue.back());
    }

    err = OTBR_ERROR_NONE;
exit:
    return err;
}

template <typename T> otbrError DBusMessageExtractPrimitive(DBusMessageIter *aIter, std::vector<T> &aValue)
{
    DBusMessageIter subIter;
    otbrError       err = OTBR_ERROR_NONE;
    T *             val;
    int             n;
    int             subtype;

    VerifyOrExit(dbus_message_iter_get_arg_type(aIter) == DBUS_TYPE_ARRAY, err = OTBR_ERROR_DBUS);
    dbus_message_iter_recurse(aIter, &subIter);

    subtype = dbus_message_iter_get_arg_type(&subIter);
    if (subtype != DBUS_TYPE_INVALID)
    {
        VerifyOrExit(dbus_message_iter_get_arg_type(&subIter) == DBusTypeTrait<T>::TYPE, err = OTBR_ERROR_DBUS);
        dbus_message_iter_get_fixed_array(&subIter, &val, &n);

        if (val != nullptr)
        {
            aValue = std::vector<T>(val, val + n);
        }
    }
    dbus_message_iter_next(aIter);
exit:
    return err;
}

template <typename T> otbrError DBusMessageEncode(DBusMessageIter *aIter, T aValue)
{
    otbrError err = OTBR_ERROR_NONE;
    VerifyOrExit(dbus_message_iter_append_basic(aIter, DBusTypeTrait<T>::TYPE, &aValue), err = OTBR_ERROR_DBUS);
exit:
    return err;
}

template <typename T> otbrError DBusMessageEncode(DBusMessageIter *aIter, const std::vector<T> &aValue)
{
    otbrError err  = OTBR_ERROR_NONE;
    uint32_t  size = static_cast<uint32_t>(aValue.size());

    SuccessOrExit(err = DBusMessageEncode(aIter, size));
    for (const auto &v : aValue)
    {
        SuccessOrExit(err = DBusMessageEncode(aIter, v));
    }
exit:
    return err;
}

template <typename T> otbrError DBusMessageEncodePrimitive(DBusMessageIter *aIter, const std::vector<T> &aValue)
{
    DBusMessageIter subIter;
    otbrError       err = OTBR_ERROR_NONE;

    VerifyOrExit(dbus_message_iter_open_container(aIter, DBUS_TYPE_ARRAY, DBusTypeTrait<T>::TYPE_AS_STRING, &subIter),
                 err = OTBR_ERROR_DBUS);

    if (!aValue.empty())
    {
        const T *buf = &aValue.front();

        VerifyOrExit(dbus_message_iter_append_fixed_array(&subIter, DBusTypeTrait<T>::TYPE, &buf,
                                                          static_cast<int>(aValue.size())),
                     err = OTBR_ERROR_DBUS);
    }
    VerifyOrExit(dbus_message_iter_close_container(aIter, &subIter), err = OTBR_ERROR_DBUS);
exit:
    return err;
}

template <size_t I, typename... FieldTypes> struct ElementType
{
    using ValueType         = typename std::tuple_element<I, std::tuple<FieldTypes...>>::type;
    using NonconstValueType = typename std::remove_cv<ValueType>::type;
    using RawValueType      = typename std::remove_reference<NonconstValueType>::type;
};

template <size_t I, size_t N, typename... FieldTypes> class DBusMessageIterFor
{
public:
    static otbrError ConvertToTuple(DBusMessageIter *aIter, std::tuple<FieldTypes...> &aValues)
    {
        using RawValueType = typename ElementType<N - I, FieldTypes...>::RawValueType;
        RawValueType &val  = std::get<N - I>(aValues);
        otbrError     err  = DBusMessageExtract(aIter, val);

        SuccessOrExit(err);
        err = DBusMessageIterFor<I - 1, N, FieldTypes...>::ConvertToTuple(aIter, aValues);
    exit:
        return err;
    }

    static otbrError ConvertToDBusMessage(DBusMessageIter *aIter, const std::tuple<FieldTypes...> &aValues)
    {
        otbrError err = DBusMessageEncode(aIter, std::get<N - I>(aValues));

        SuccessOrExit(err);
        err = DBusMessageIterFor<I - 1, N, FieldTypes...>::ConvertToDBusMessage(aIter, aValues);
    exit:
        return err;
    }
};

template <size_t N, typename... FieldTypes> class DBusMessageIterFor<1, N, FieldTypes...>
{
public:
    static otbrError ConvertToTuple(DBusMessageIter *aIter, std::tuple<FieldTypes...> &aValues)
    {
        using RawValueType = typename ElementType<N - 1, FieldTypes...>::RawValueType;
        RawValueType &val  = std::get<N - 1>(aValues);
        otbrError     err  = DBusMessageExtract(aIter, val);

        return err;
    }

    static otbrError ConvertToDBusMessage(DBusMessageIter *aIter, const std::tuple<FieldTypes...> &aValues)
    {
        otbrError err = DBusMessageEncode(aIter, std::get<N - 1>(aValues));

        return err;
    }
};

template <typename... FieldTypes>
otbrError ConvertToDBusMessage(DBusMessageIter *aIter, const std::tuple<FieldTypes...> &aValues)
{
    return DBusMessageIterFor<sizeof...(FieldTypes), sizeof...(FieldTypes), FieldTypes...>::ConvertToDBusMessage(
        aIter, aValues);
}

template <typename... FieldTypes>
constexpr otbrError ConvertToTuple(DBusMessageIter *aIter, std::tuple<FieldTypes...> &aValues)
{
    return DBusMessageIterFor<sizeof...(FieldTypes), sizeof...(FieldTypes), FieldTypes...>::ConvertToTuple(aIter,
                                                                                                           aValues);
}

/**
 * This function converts a d-bus message to a tuple of C++ types.
 *
 * @param[out]  aMsg      The dbus message to decode
 * @param[out]  aValues   The tuple output
 *
 * @retval  OTBR_ERROR_NONE   Successfully decoded the message
 * @retval  OTBR_ERROR_DBUS   Failed to decode the message
 */
template <typename... FieldTypes> otbrError DBusMessageToTuple(DBusMessage &aMsg, std::tuple<FieldTypes...> &aValues)
{
    otbrError       err = OTBR_ERROR_NONE;
    DBusMessageIter iter;

    VerifyOrExit(dbus_message_iter_init(&aMsg, &iter), err = OTBR_ERROR_DBUS);

    err = ConvertToTuple(&iter, aValues);
exit:
    return err;
}

/**
 * This function converts a tuple of C++ types to a d-bus message.
 *
 * @param[out]  aMsg      The dbus message output
 * @param[in]   aValues   The tuple to encode
 *
 * @retval  OTBR_ERROR_NONE   Successfully encoded the message
 * @retval  OTBR_ERROR_DBUS   Failed to encode the message
 */
template <typename... FieldTypes>
otbrError TupleToDBusMessage(DBusMessage &aMsg, const std::tuple<FieldTypes...> &aValues)
{
    DBusMessageIter iter;

    dbus_message_iter_init_append(&aMsg, &iter);
    return ConvertToDBusMessage(&iter, aValues);
}

/**
 * This function converts a d-bus message to a tuple of C++ types
 *
 * @param[out]  aMsg      The dbus message to decode
 * @param[out]  aValues   The tuple output
 *
 * @retval  OTBR_ERROR_NONE   Successfully decoded the message
 * @retval  OTBR_ERROR_DBUS   Failed to decode the message
 */
template <typename... FieldTypes>
otbrError DBusMessageToTuple(SharedDBusMessage aMsg, std::tuple<FieldTypes...> &aValues)
{
    return DBusMessageToTuple(*aMsg.GetRaw(), aValues);
}

} // namespace dbus
} // namespace otbr

#endif
