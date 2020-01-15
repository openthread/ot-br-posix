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

#include "dbus/constants.hpp"
#include "dbus/dbus_message_helper.hpp"
#include "dbus/error.hpp"

#include <unordered_map>

static const std::unordered_map<otError, std::string> *sErrorNameMap = new std::unordered_map<otError, std::string>{
    {OT_ERROR_NONE, "io.openthread.OK"},
    {OT_ERROR_FAILED, "io.openthread.Failed"},
    {OT_ERROR_DROP, "io.openthread.Drop"},
    {OT_ERROR_NO_BUFS, "io.openthread.NoBufs"},
    {OT_ERROR_NO_ROUTE, "io.openthread.NoRoute"},
    {OT_ERROR_BUSY, "io.openthread.Busy"},
    {OT_ERROR_PARSE, "io.openthread.Parse"},
    {OT_ERROR_INVALID_ARGS, "io.openthread.InvalidArgs"},
    {OT_ERROR_SECURITY, "io.openthread.Security"},
    {OT_ERROR_ADDRESS_QUERY, "io.openthread.AddressQuery"},
    {OT_ERROR_NO_ADDRESS, "io.openthread.NoAddress"},
    {OT_ERROR_ABORT, "io.openthread.Abort"},
    {OT_ERROR_NOT_IMPLEMENTED, "io.openthread.NotImplemented"},
    {OT_ERROR_INVALID_STATE, "io.openthread.InvalidState"},
    {OT_ERROR_NO_ACK, "io.openthread.NoAck"},
    {OT_ERROR_CHANNEL_ACCESS_FAILURE, "io.openthread.ChannelAccessFailure"},
    {OT_ERROR_DETACHED, "io.openthread.Detached"},
    {OT_ERROR_FCS, "io.openthread.FcsErr"},
    {OT_ERROR_NO_FRAME_RECEIVED, "io.openthread.NoFrameReceived"},
    {OT_ERROR_UNKNOWN_NEIGHBOR, "io.openthread.UnknownNeighbor"},
    {OT_ERROR_INVALID_SOURCE_ADDRESS, "io.openthread.InvalidSourceAddress"},
    {OT_ERROR_ADDRESS_FILTERED, "io.openthread.AddressFiltered"},
    {OT_ERROR_DESTINATION_ADDRESS_FILTERED, "io.openthread.DestinationAddressFiltered"},
    {OT_ERROR_NOT_FOUND, "io.openthread.NotFound"},
    {OT_ERROR_ALREADY, "io.openthread.Already"},
    {OT_ERROR_IP6_ADDRESS_CREATION_FAILURE, "io.openthread.Ipv6AddressCreationFailure"},
    {OT_ERROR_NOT_CAPABLE, "io.openthread.NotCapable"},
    {OT_ERROR_RESPONSE_TIMEOUT, "io.openthread.ResponseTimeout"},
    {OT_ERROR_DUPLICATED, "io.openthread.Duplicated"},
    {OT_ERROR_REASSEMBLY_TIMEOUT, "io.openthread.ReassemblyTimeout"},
    {OT_ERROR_NOT_TMF, "io.openthread.NotTmf"},
    {OT_ERROR_NOT_LOWPAN_DATA_FRAME, "io.openthread.NonLowpanDatatFrame"},
    {OT_ERROR_GENERIC, "io.openthread.GenericError"},
    {OT_ERROR_LINK_MARGIN_LOW, "io.openthread.LinkMarginLow"},
};

static std::unordered_map<std::string, otError> sErrorCodeMap = {
    {"io.openthread.OK", OT_ERROR_NONE},
    {"io.openthread.Failed", OT_ERROR_FAILED},
    {"io.openthread.Drop", OT_ERROR_DROP},
    {"io.openthread.NoBufs", OT_ERROR_NO_BUFS},
    {"io.openthread.NoRoute", OT_ERROR_NO_ROUTE},
    {"io.openthread.Busy", OT_ERROR_BUSY},
    {"io.openthread.Parse", OT_ERROR_PARSE},
    {"io.openthread.InvalidArgs", OT_ERROR_INVALID_ARGS},
    {"io.openthread.Security", OT_ERROR_SECURITY},
    {"io.openthread.AddressQuery", OT_ERROR_ADDRESS_QUERY},
    {"io.openthread.NoAddress", OT_ERROR_NO_ADDRESS},
    {"io.openthread.Abort", OT_ERROR_ABORT},
    {"io.openthread.NotImplemented", OT_ERROR_NOT_IMPLEMENTED},
    {"io.openthread.InvalidState", OT_ERROR_INVALID_STATE},
    {"io.openthread.NoAck", OT_ERROR_NO_ACK},
    {"io.openthread.ChannelAccessFailure", OT_ERROR_CHANNEL_ACCESS_FAILURE},
    {"io.openthread.Detached", OT_ERROR_DETACHED},
    {"io.openthread.FcsErr", OT_ERROR_FCS},
    {"io.openthread.NoFrameReceived", OT_ERROR_NO_FRAME_RECEIVED},
    {"io.openthread.UnknownNeighbor", OT_ERROR_UNKNOWN_NEIGHBOR},
    {"io.openthread.InvalidSourceAddress", OT_ERROR_INVALID_SOURCE_ADDRESS},
    {"io.openthread.AddressFiltered", OT_ERROR_ADDRESS_FILTERED},
    {"io.openthread.DestinationAddressFiltered", OT_ERROR_DESTINATION_ADDRESS_FILTERED},
    {"io.openthread.NotFound", OT_ERROR_NOT_FOUND},
    {"io.openthread.Already", OT_ERROR_ALREADY},
    {"io.openthread.Ipv6AddressCreationFailure", OT_ERROR_IP6_ADDRESS_CREATION_FAILURE},
    {"io.openthread.NotCapable", OT_ERROR_NOT_CAPABLE},
    {"io.openthread.ResponseTimeout", OT_ERROR_RESPONSE_TIMEOUT},
    {"io.openthread.Duplicated", OT_ERROR_DUPLICATED},
    {"io.openthread.ReassemblyTimeout", OT_ERROR_REASSEMBLY_TIMEOUT},
    {"io.openthread.NotTmf", OT_ERROR_NOT_TMF},
    {"io.openthread.NonLowpanDatatFrame", OT_ERROR_NOT_LOWPAN_DATA_FRAME},
    {"io.openthread.GenericError", OT_ERROR_GENERIC},
    {"io.openthread.LinkMarginLow", OT_ERROR_LINK_MARGIN_LOW},
};

namespace otbr {
namespace dbus {

std::string ConvertToDBusErrorName(otError aError)
{
    return (*sErrorNameMap)[aError];
}

otError ConvertFromDBusErrorName(const std::string &aErrorName)
{
    if (sErrorCodeMap.find(aErrorName) != sErrorCodeMap.end())
    {
        return sErrorCodeMap.at(aErrorName);
    }
    else
    {
        return OT_ERROR_INVALID_ARGS;
    }
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
