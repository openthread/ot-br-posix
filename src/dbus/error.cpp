/*
 *    Copyright (c) 2020, The OpenThread Authors.
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

#include "dbus/error.hpp"

#include "dbus/constants.hpp"
#include "dbus/dbus_message_helper.hpp"

#include <unordered_map>

static const std::pair<otError, const char *> sErrorNameMap[] = {
    {OT_ERROR_GENERIC, "io.openthread.Error.GenericError"},
    {OT_ERROR_NONE, "io.openthread.Error.OK"},
    {OT_ERROR_FAILED, "io.openthread.Error.Failed"},
    {OT_ERROR_DROP, "io.openthread.Error.Drop"},
    {OT_ERROR_NO_BUFS, "io.openthread.Error.NoBufs"},
    {OT_ERROR_NO_ROUTE, "io.openthread.Error.NoRoute"},
    {OT_ERROR_BUSY, "io.openthread.Error.Busy"},
    {OT_ERROR_PARSE, "io.openthread.Error.Parse"},
    {OT_ERROR_INVALID_ARGS, "io.openthread.Error.InvalidArgs"},
    {OT_ERROR_SECURITY, "io.openthread.Error.Security"},
    {OT_ERROR_ADDRESS_QUERY, "io.openthread.Error.AddressQuery"},
    {OT_ERROR_NO_ADDRESS, "io.openthread.Error.NoAddress"},
    {OT_ERROR_ABORT, "io.openthread.Error.Abort"},
    {OT_ERROR_NOT_IMPLEMENTED, "io.openthread.Error.NotImplemented"},
    {OT_ERROR_INVALID_STATE, "io.openthread.Error.InvalidState"},
    {OT_ERROR_NO_ACK, "io.openthread.Error.NoAck"},
    {OT_ERROR_CHANNEL_ACCESS_FAILURE, "io.openthread.Error.ChannelAccessFailure"},
    {OT_ERROR_DETACHED, "io.openthread.Error.Detached"},
    {OT_ERROR_FCS, "io.openthread.Error.FcsErr"},
    {OT_ERROR_NO_FRAME_RECEIVED, "io.openthread.Error.NoFrameReceived"},
    {OT_ERROR_UNKNOWN_NEIGHBOR, "io.openthread.Error.UnknownNeighbor"},
    {OT_ERROR_INVALID_SOURCE_ADDRESS, "io.openthread.Error.InvalidSourceAddress"},
    {OT_ERROR_ADDRESS_FILTERED, "io.openthread.Error.AddressFiltered"},
    {OT_ERROR_DESTINATION_ADDRESS_FILTERED, "io.openthread.Error.DestinationAddressFiltered"},
    {OT_ERROR_NOT_FOUND, "io.openthread.Error.NotFound"},
    {OT_ERROR_ALREADY, "io.openthread.Error.Already"},
    {OT_ERROR_IP6_ADDRESS_CREATION_FAILURE, "io.openthread.Error.Ipv6AddressCreationFailure"},
    {OT_ERROR_NOT_CAPABLE, "io.openthread.Error.NotCapable"},
    {OT_ERROR_RESPONSE_TIMEOUT, "io.openthread.Error.ResponseTimeout"},
    {OT_ERROR_DUPLICATED, "io.openthread.Error.Duplicated"},
    {OT_ERROR_REASSEMBLY_TIMEOUT, "io.openthread.Error.ReassemblyTimeout"},
    {OT_ERROR_NOT_TMF, "io.openthread.Error.NotTmf"},
    {OT_ERROR_NOT_LOWPAN_DATA_FRAME, "io.openthread.Error.NonLowpanDatatFrame"},
    {OT_ERROR_LINK_MARGIN_LOW, "io.openthread.Error.LinkMarginLow"},
};

namespace otbr {
namespace dbus {

const char *ConvertToDBusErrorName(otError aError)
{
    const char *name = sErrorNameMap[0].second;

    for (auto &&p : sErrorNameMap)
    {
        if (p.first == aError)
        {
            name = p.second;
        }
    }
    return name;
}

otError ConvertFromDBusErrorName(const std::string &aErrorName)
{
    otError err = OT_ERROR_GENERIC;

    for (auto &&p : sErrorNameMap)
    {
        if (p.second == aErrorName)
        {
            err = p.first;
        }
    }
    return err;
}

otError CheckErrorMessage(DBusMessage *aMessage)
{
    otError err = OT_ERROR_NONE;

    if (dbus_message_get_type(aMessage) == DBUS_MESSAGE_TYPE_ERROR)
    {
        std::string errMsg;
        auto        args = std::tie(errMsg);

        VerifyOrExit(DBusMessageToTuple(*aMessage, args) == OTBR_ERROR_NONE, err = OT_ERROR_FAILED);
        err = ConvertFromDBusErrorName(errMsg);
    }
exit:
    return err;
}

} // namespace dbus
} // namespace otbr
