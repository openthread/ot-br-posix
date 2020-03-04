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
#include "dbus/common/dbus_message_helper.hpp"

#include <CppUTest/TestHarness.h>

using std::array;
using std::string;
using std::tuple;
using std::vector;

using otbr::DBus::DBusMessageEncode;
using otbr::DBus::DBusMessageExtract;
using otbr::DBus::DBusMessageToTuple;
using otbr::DBus::TupleToDBusMessage;

struct TestStruct
{
    uint8_t     tag;
    uint32_t    val;
    std::string name;
};

bool operator==(const TestStruct &aLhs, const TestStruct &aRhs)
{
    return aLhs.tag == aRhs.tag && aLhs.val == aRhs.val && aLhs.name == aRhs.name;
}

inline otbrError DBusMessageEncode(DBusMessageIter *aIter, const TestStruct &aValue)
{
    otbrError err = OTBR_ERROR_DBUS;

    SuccessOrExit(DBusMessageEncode(aIter, aValue.tag));
    SuccessOrExit(DBusMessageEncode(aIter, aValue.val));
    SuccessOrExit(DBusMessageEncode(aIter, aValue.name));
    err = OTBR_ERROR_NONE;
exit:
    return err;
}

inline otbrError DBusMessageExtract(DBusMessageIter *aIter, TestStruct &aValue)
{
    otbrError err = OTBR_ERROR_DBUS;

    SuccessOrExit(DBusMessageExtract(aIter, aValue.tag));
    SuccessOrExit(DBusMessageExtract(aIter, aValue.val));
    SuccessOrExit(DBusMessageExtract(aIter, aValue.name));
    err = OTBR_ERROR_NONE;
exit:
    return err;
}

TEST_GROUP(DBusMessage){};

TEST(DBusMessage, TestVectorMessage)
{
    DBusMessage *msg = dbus_message_new(DBUS_MESSAGE_TYPE_METHOD_RETURN);
    tuple<vector<uint8_t>, vector<uint16_t>, vector<uint32_t>, vector<uint64_t>, vector<int16_t>, vector<int32_t>,
          vector<int64_t>>
        setVals({0, 1}, {2, 3}, {4, 5}, {6, 7, 8}, {}, {9, 10}, {11, 12});

    tuple<vector<uint8_t>, vector<uint16_t>, vector<uint32_t>, vector<uint64_t>, vector<int16_t>, vector<int32_t>,
          vector<int64_t>>
        getVals({}, {}, {}, {}, {}, {}, {});
    assert(msg != NULL);

    TupleToDBusMessage(*msg, setVals);
    DBusMessageToTuple(*msg, getVals);

    assert(setVals == getVals);

    dbus_message_unref(msg);
}

TEST(DBusMessage, TestArrayMessage)
{
    DBusMessage *            msg = dbus_message_new(DBUS_MESSAGE_TYPE_METHOD_RETURN);
    tuple<array<uint8_t, 4>> setVals({1, 2, 3, 4});
    tuple<array<uint8_t, 4>> getVals({0, 0, 0, 0});

    assert(msg != NULL);

    TupleToDBusMessage(*msg, setVals);
    DBusMessageToTuple(*msg, getVals);

    assert(setVals == getVals);

    dbus_message_unref(msg);
}

TEST(DBusMessage, TestNumberMessage)
{
    DBusMessage *msg = dbus_message_new(DBUS_MESSAGE_TYPE_METHOD_RETURN);
    tuple<uint8_t, uint16_t, uint32_t, uint64_t, bool, int16_t, int32_t, int64_t> setVals =
        std::make_tuple<uint8_t, uint16_t, uint32_t, uint64_t, bool, int16_t, int32_t, int64_t>(1, 2, 3, 4, true, 5, 6,
                                                                                                7);
    tuple<uint8_t, uint16_t, uint32_t, uint64_t, bool, int16_t, int32_t, int64_t> getVals =
        std::make_tuple<uint8_t, uint16_t, uint32_t, uint64_t, bool, int16_t, int32_t, int64_t>(0, 0, 0, 0, false, 0, 0,
                                                                                                0);

    assert(msg != NULL);

    TupleToDBusMessage(*msg, setVals);
    DBusMessageToTuple(*msg, getVals);

    assert(setVals == getVals);

    dbus_message_unref(msg);
}

TEST(DBusMessage, TestStructMessage)
{
    DBusMessage *msg = dbus_message_new(DBUS_MESSAGE_TYPE_METHOD_RETURN);
    tuple<uint8_t, vector<int32_t>, vector<string>, vector<TestStruct>> setVals(
        0x03, {0x04, 0x05}, {"hello", "world"}, {{1, 0xf0a, "test1"}, {2, 0xf0b, "test2"}});
    tuple<uint8_t, vector<int32_t>, vector<string>, vector<TestStruct>> getVals(0, {}, {}, {});

    assert(msg != NULL);

    TupleToDBusMessage(*msg, setVals);
    DBusMessageToTuple(*msg, getVals);

    assert(setVals == getVals);

    dbus_message_unref(msg);
}
