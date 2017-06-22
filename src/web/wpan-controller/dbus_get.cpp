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
 *   This file implements the function of "get property"
 */

#include "common/code_utils.hpp"

#include "dbus_get.hpp"

namespace ot {
namespace Dbus {

static void DumpInfoFromIter(char *aOutput, DBusMessageIter *aIter, int aIndent,
                             bool aBare)
{
    DBusMessageIter subIter;

    switch (dbus_message_iter_get_arg_type(aIter))
    {
    case DBUS_TYPE_DICT_ENTRY:
        dbus_message_iter_recurse(aIter, &subIter);
        DumpInfoFromIter(aOutput, &subIter, aIndent + 1, true);
        dbus_message_iter_next(&subIter);
        DumpInfoFromIter(aOutput, &subIter, aIndent + 1, true);
        break;
    case DBUS_TYPE_ARRAY:
        dbus_message_iter_recurse(aIter, &subIter);
        if (dbus_message_iter_get_arg_type(&subIter) == DBUS_TYPE_BYTE
            || dbus_message_iter_get_arg_type(&subIter)
            == DBUS_TYPE_INVALID)
        {
            strcat(aOutput, "[");
            aIndent = 0;
        }
        else
        {
            strcat(aOutput, "[");
        }

        for ( ; dbus_message_iter_get_arg_type(&subIter) != DBUS_TYPE_INVALID;
              dbus_message_iter_next(&subIter))
        {
            DumpInfoFromIter(aOutput, &subIter, aIndent + 1,
                             dbus_message_iter_get_arg_type(&subIter)
                             == DBUS_TYPE_BYTE);
        }
        for (int i = 0; i < aIndent; i++)
        {
            strcat(aOutput, "\t");
        }
        strcat(aOutput, "]");
        break;
    case DBUS_TYPE_VARIANT:
        dbus_message_iter_recurse(aIter, &subIter);
        DumpInfoFromIter(aOutput, &subIter, aIndent, true);
        break;
    case DBUS_TYPE_STRING:
    {
        const char *string;
        dbus_message_iter_get_basic(aIter, &string);
        strcat(aOutput, string);
        break;
    }

    case DBUS_TYPE_BYTE:
    {
        uint8_t v;
        char    temp[3];
        dbus_message_iter_get_basic(aIter, &v);
        snprintf(temp, 3, "%02X", v);
        strcat(aOutput, temp);
        break;
    }
    case DBUS_TYPE_UINT16:
    {
        uint16_t v;
        char     temp[9];
        dbus_message_iter_get_basic(aIter, &v);
        snprintf(temp, 9, "0x%04X", v);
        strcat(aOutput, temp);
        break;
    }
    case DBUS_TYPE_INT16:
    {
        int16_t v;
        char    temp[5];
        dbus_message_iter_get_basic(aIter, &v);
        int vLen = floor(log10(abs(v))) + 2;
        snprintf(temp, vLen, "%d", v);
        strcat(aOutput, temp);
        break;
    }
    case DBUS_TYPE_UINT32:
    {
        uint32_t v;
        char     temp[10];
        dbus_message_iter_get_basic(aIter, &v);
        int vLen = floor(log10(abs(v))) + 2;
        snprintf(temp, vLen, "%d", v);
        strcat(aOutput, temp);
        break;
    }
    case DBUS_TYPE_BOOLEAN:
    {
        dbus_bool_t v;
        char        temp[6];
        dbus_message_iter_get_basic(aIter, &v);
        snprintf(temp, 6, "%s", v ? "true" : "false");
        strcat(aOutput, temp);
        break;
    }
    case DBUS_TYPE_INT32:
    {
        int32_t v;
        char    temp[10];
        dbus_message_iter_get_basic(aIter, &v);
        int vLen = floor(log10(abs(v))) + 2;
        snprintf(temp, vLen, "%d", v);
        strcat(aOutput, temp);
        break;
    }
    case DBUS_TYPE_UINT64:
    {
        uint64_t v;
        char     temp[19];
        dbus_message_iter_get_basic(aIter, &v);
        snprintf(temp, 19, "0x%016llX", (unsigned long long) v);
        strcat(aOutput, temp);
        break;
    }
    default:
        char temp[100];
        snprintf(temp, 100, "<%s>",
                 dbus_message_type_to_string(
                     dbus_message_iter_get_arg_type(aIter)));
        strcat(aOutput, temp);
        break;
    }

    (void)aBare;
}

int DBusGet::ProcessReply(void)
{
    int          ret = 0;
    const char  *method = "PropGet";
    DBusMessage *messsage = NULL;
    DBusMessage *reply = NULL;

    VerifyOrExit(GetConnection() != NULL, ret = kWpantundStatus_InvalidConnection);
    SetMethod(method);
    VerifyOrExit((messsage = GetMessage()) != NULL, ret = kWpantundStatus_InvalidMessage);
    dbus_message_append_args(messsage, DBUS_TYPE_STRING, &mPropertyName,
                             DBUS_TYPE_INVALID);
    VerifyOrExit((reply = GetReply()) != NULL, ret =  kWpantundStatus_InvalidReply);
    dbus_message_iter_init(reply, &mIter);
    // Get return code
    dbus_message_iter_get_basic(&mIter, &ret);

    if (ret)
    {
        const char *error = NULL;

        // Try to see if there is an error explanation we can extract
        dbus_message_iter_next(&mIter);
        if (dbus_message_iter_get_arg_type(&mIter) == DBUS_TYPE_STRING)
        {
            dbus_message_iter_get_basic(&mIter, &error);
        }

        if (!error || error[0] == 0)
        {
            error = (ret < 0) ? strerror(-ret) : "Get failed";
        }
    }
    else
    {
        dbus_message_iter_next(&mIter);
    }

exit:

    free();
    return ret;
}

const char *DBusGet::GetPropertyValue(const char *aPropertyName)
{
    SetPropertyName(aPropertyName);
    memset(mPropertyValue, 0, OT_PROPERTY_VALUE_SIZE);
    VerifyOrExit(ProcessReply() == kWpantundStatus_Ok);
    DumpInfoFromIter(mPropertyValue, &mIter, 0, false);
exit:
    return mPropertyValue;
}

int DBusGet::GetAllPropertyNames(void)
{
    DBusMessageIter listIter;

    int propCnt = 0;

    SetPropertyName("");
    ProcessReply();
    dbus_message_iter_recurse(&mIter, &listIter);

    for ( ; dbus_message_iter_get_arg_type(&listIter) == DBUS_TYPE_STRING;
          dbus_message_iter_next(&listIter))
    {
        char *pName;
        dbus_message_iter_get_basic(&listIter, &pName);
        strncpy(mPropertyList[propCnt].name, pName, sizeof(mPropertyList[propCnt].name));
        propCnt++;
    }
    return propCnt;
}

void DBusGet::GetAllPropertyValues(int propCnt)
{
    for (int i = 0; i < propCnt; i++)
    {
        strncpy(mPropertyList[i].value, GetPropertyValue(mPropertyList[i].name),
                sizeof(mPropertyList[i].value));
    }
}

PropertyNameValue *DBusGet::GetPropertyList(void)
{
    int propCnt;

    propCnt = GetAllPropertyNames();
    GetAllPropertyValues(propCnt);
    return mPropertyList;

}

} //namespace Dbus
} //namespace ot
