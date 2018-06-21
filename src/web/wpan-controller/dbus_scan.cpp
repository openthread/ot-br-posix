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
 *   This file implements "scan" Thread Network function.
 */

#include "common/code_utils.hpp"

#include "dbus_scan.hpp"

namespace ot {
namespace Dbus {

int             DBusScan::mAvailableNetworksCnt = 0;
WpanNetworkInfo DBusScan::mAvailableNetworks[OT_SCANNED_NET_BUFFER_SIZE];

int DBusScan::ProcessReply(void)
{
    int               ret                            = 0;
    const char *      method                         = "NetScanStart";
    DBusMessage *     messsage                       = NULL;
    DBusMessage *     reply                          = NULL;
    DBusConnection *  dbusConnection                 = NULL;
    DBusPendingCall * pending                        = NULL;
    static const char dbusObjectManagerMatchString[] = "type='signal'";
    DBusMessageIter   iter;
    DBusError         error;

    dbus_error_init(&error);
    VerifyOrExit((dbusConnection = GetConnection()) != NULL, ret = kWpantundStatus_InvalidConnection);

    dbus_bus_add_match(dbusConnection, dbusObjectManagerMatchString, &error);
    VerifyOrExit(!dbus_error_is_set(&error), ret = kWpantundStatus_Failure);
    memset(mAvailableNetworks, 0, sizeof(mAvailableNetworks));
    mAvailableNetworksCnt = 0;

    dbus_connection_add_filter(dbusConnection, &DbusBeaconHandler, NULL, NULL);
    SetMethod(method);
    VerifyOrExit((messsage = GetMessage()) != NULL, ret = kWpantundStatus_InvalidMessage);
    dbus_message_append_args(messsage, DBUS_TYPE_UINT32, &mChannelMask, DBUS_TYPE_INVALID);

    VerifyOrExit((pending = GetPending()) != NULL, ret = kWpantundStatus_Failure);
    while ((dbus_connection_get_dispatch_status(dbusConnection) == DBUS_DISPATCH_DATA_REMAINS) ||
           dbus_connection_has_messages_to_send(dbusConnection) || !dbus_pending_call_get_completed(pending))
    {
        dbus_connection_read_write_dispatch(dbusConnection, 5000 /*ms*/);
    }
    reply = dbus_pending_call_steal_reply(pending);
    dbus_message_iter_init(reply, &iter);
    VerifyOrExit(dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_INT32);

    // Get return code
    dbus_message_iter_get_basic(&iter, &ret);
exit:
    if (dbus_error_is_set(&error))
    {
        otbrLog(OTBR_LOG_ERR, "scan error: %s", error.message);
        dbus_error_free(&error);
    }
    return ret;
}

DBusHandlerResult DBusScan::DbusBeaconHandler(DBusConnection *aConnection, DBusMessage *aMessage, void *aUserData)
{
    DBusMessageIter iter;
    WpanNetworkInfo networkInfo;

    if (!dbus_message_is_signal(aMessage, WPANTUND_DBUS_APIv1_INTERFACE, WPANTUND_IF_SIGNAL_NET_SCAN_BEACON))
    {
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }
    dbus_message_iter_init(aMessage, &iter);

    DBusScan::ParseNetworkInfoFromIter(&networkInfo, &iter);

    if (networkInfo.mNetworkName[0])
    {
        if (mAvailableNetworksCnt < OT_SCANNED_NET_BUFFER_SIZE)
        {
            mAvailableNetworks[mAvailableNetworksCnt++] = networkInfo;
        }
    }

    (void)aConnection;
    (void)aUserData;

    return DBUS_HANDLER_RESULT_HANDLED;
}

int DBusScan::ParseNetworkInfoFromIter(WpanNetworkInfo *aNetworkInfo, DBusMessageIter *aIter)
{
    int             ret = 0;
    DBusMessageIter outerIter;
    DBusMessageIter dictIter;

    if (dbus_message_iter_get_arg_type(aIter) == DBUS_TYPE_ARRAY)
    {
        dbus_message_iter_recurse(aIter, &outerIter);
        aIter = &outerIter;
    }

    memset(aNetworkInfo, 0, sizeof(*aNetworkInfo));

    for (; dbus_message_iter_get_arg_type(aIter) != DBUS_TYPE_INVALID; dbus_message_iter_next(aIter))
    {
        DBusMessageIter valueIter;
        char *          key = NULL;

        if (dbus_message_iter_get_arg_type(aIter) != DBUS_TYPE_DICT_ENTRY)
        {
            otbrLog(OTBR_LOG_ERR, "error: Bad type for network (%c)", dbus_message_iter_get_arg_type(aIter));
            ret = ERRORCODE_UNKNOWN;
            return ret;
        }

        dbus_message_iter_recurse(aIter, &dictIter);

        if (dbus_message_iter_get_arg_type(&dictIter) != DBUS_TYPE_STRING)
        {
            otbrLog(OTBR_LOG_ERR, "error: Bad type for network list (%c)", dbus_message_iter_get_arg_type(&dictIter));
            ret = ERRORCODE_UNKNOWN;
            return ret;
        }

        // Get the key
        dbus_message_iter_get_basic(&dictIter, &key);
        dbus_message_iter_next(&dictIter);

        if (dbus_message_iter_get_arg_type(&dictIter) != DBUS_TYPE_VARIANT)
        {
            otbrLog(OTBR_LOG_ERR, "error: Bad type for network list (%c)", dbus_message_iter_get_arg_type(&dictIter));
            ret = ERRORCODE_UNKNOWN;
            return ret;
        }

        dbus_message_iter_recurse(&dictIter, &valueIter);

        if (strcmp(key, kWPANTUNDProperty_NetworkName) == 0)
        {
            char *networkName = NULL;
            dbus_message_iter_get_basic(&valueIter, &networkName);
            snprintf(aNetworkInfo->mNetworkName, sizeof(aNetworkInfo->mNetworkName), "%s", networkName);
        }
        else if (strcmp(key, kWPANTUNDProperty_NCPChannel) == 0)
        {
            dbus_message_iter_get_basic(&valueIter, &aNetworkInfo->mChannel);
        }
        else if (strcmp(key, kWPANTUNDProperty_NetworkPANID) == 0)
        {
            dbus_message_iter_get_basic(&valueIter, &aNetworkInfo->mPanId);
        }
        else if (strcmp(key, kWPANTUNDProperty_NestLabs_NetworkAllowingJoin) == 0)
        {
            dbus_message_iter_get_basic(&valueIter, &aNetworkInfo->mAllowingJoin);
        }
        else if (strcmp(key, "RSSI") == 0)
        {
            dbus_message_iter_get_basic(&valueIter, &aNetworkInfo->mRssi);
        }
        else if (strcmp(key, kWPANTUNDProperty_NetworkXPANID) == 0)
        {
            dbus_message_iter_get_basic(&valueIter, &aNetworkInfo->mExtPanId);
        }
        else if (strcmp(key, kWPANTUNDProperty_NCPHardwareAddress) == 0)
        {
            DBusMessageIter sub_iter;
            dbus_message_iter_recurse(&valueIter, &sub_iter);
            if (dbus_message_iter_get_arg_type(&sub_iter) == DBUS_TYPE_BYTE)
            {
                const uint8_t *value     = NULL;
                int            nelements = 0;
                dbus_message_iter_get_fixed_array(&sub_iter, &value, &nelements);
                if (nelements == 8)
                {
                    memcpy(aNetworkInfo->mHardwareAddress, value, nelements);
                }
            }
        }
    }
    return ret;
}

} // namespace Dbus
} // namespace ot
