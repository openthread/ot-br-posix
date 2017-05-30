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
 *   This file provides DBus operations APIs for other modules to control the WPAN interface.
 */

#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>

#include <dbus/dbus.h>

#include "common/code_utils.hpp"

#include "dbus_base.hpp"
#include "dbus_form.hpp"
#include "dbus_gateway.hpp"
#include "dbus_get.hpp"
#include "dbus_ifname.hpp"
#include "dbus_join.hpp"
#include "dbus_leave.hpp"
#include "dbus_scan.hpp"
#include "dbus_set.hpp"
#include "wpan_controller.hpp"

namespace ot {
namespace Dbus {

WpanNetworkInfo    gScanedNetowrks[SCANNED_NET_BUFFER_SIZE];
int                gScannedNetworkCount = 0;
static DBusIfname *gInterfaceName = NULL;

int WPANController::Scan(void)
{
    int      ret = 0;
    DBusScan scanNetwork;

    scanNetwork.SetChannelMask(0);
    scanNetwork.SetDestination(gInterfaceName->GetDBusName());
    VerifyOrExit((ret = scanNetwork.ProcessReply()) == 0);

    VerifyOrExit((gScannedNetworkCount = scanNetwork.GetNetworksCount()) > 0,
                 ret = kWpantundStatus_NetworkNotFound);
    memcpy(gScanedNetowrks, scanNetwork.GetNetworks(),
           gScannedNetworkCount * sizeof(Dbus::WpanNetworkInfo));
exit:
    return ret;
}

const WpanNetworkInfo *WPANController::GetScanNetworksInfo(void)
{
    return gScanedNetowrks;
}

int WPANController::GetScanNetworksInfoCount(void)
{
    return gScannedNetworkCount;
}

int WPANController::Start(void)
{
    gInterfaceName = new DBusIfname();
    return gInterfaceName->ProcessReply();
}

int WPANController::Leave(void)
{
    DBusLeave leaveNetwork;

    leaveNetwork.SetDestination(gInterfaceName->GetDBusName());
    return leaveNetwork.ProcessReply();
}

int WPANController::Form(const char *aNetworkname, int aChannel)
{
    DBusForm formNetwork;
    int      ret = 0;

    VerifyOrExit(aNetworkname != NULL, ret = kWpantundStatus_InvalidArgument);
    formNetwork.SetNetworkName(aNetworkname);
    VerifyOrExit((aChannel <= 26 && aChannel >= 11), ret = kWpantundStatus_InvalidArgument);
    formNetwork.SetChannelMask(aChannel);
    formNetwork.SetNodeType(LEADER_ROLE);
    formNetwork.SetDestination(gInterfaceName->GetDBusName());
    ret = formNetwork.ProcessReply();
exit:
    return ret;
}

int WPANController::Join(char *aNetworkname,
                         int16_t aChannel,
                         uint64_t aExtPanId,
                         uint16_t aPanId)
{
    int      ret = 0;
    DBusJoin joinNetwork;

    VerifyOrExit(aNetworkname != NULL, ret = kWpantundStatus_InvalidArgument);
    joinNetwork.SetNetworkName(aNetworkname);
    joinNetwork.SetNodeType(ROUTER_ROLE);
    VerifyOrExit((aChannel <= 26 && aChannel >= 11), ret = kWpantundStatus_InvalidArgument);
    joinNetwork.SetChannel(aChannel);
    VerifyOrExit(aExtPanId != 0, ret = kWpantundStatus_InvalidArgument);
    joinNetwork.SetExtPanId(aExtPanId);
    VerifyOrExit(aPanId != 0, ret = kWpantundStatus_InvalidArgument);
    joinNetwork.SetPanId(aPanId);
    joinNetwork.SetDestination(gInterfaceName->GetDBusName());
    ret = joinNetwork.ProcessReply();
exit:
    return ret;
}

const char *WPANController::Get(const char *aPropertyName)
{
    DBusGet getProp;

    int ret = kWpantundStatus_Ok;

    VerifyOrExit(aPropertyName != NULL, ret = kWpantundStatus_InvalidArgument);
    getProp.SetDestination(gInterfaceName->GetDBusName());

exit:

    if (ret != kWpantundStatus_Ok)
    {
        syslog(LOG_ERR, "Property name is NULL; error: %d\n", ret);
    }

    return ret ? "" : getProp.GetPropertyValue(aPropertyName);
}

int WPANController::Set(uint8_t aType, const char *aPropertyName, const char *aPropertyValue)
{
    DBusSet setProp;
    int     ret = 0;

    VerifyOrExit((aType == kPropertyType_String || aType == kPropertyType_Data),
                 ret = kWpantundStatus_InvalidArgument);
    setProp.SetPropertyType(aType);
    VerifyOrExit(aPropertyName != NULL, ret = kWpantundStatus_InvalidArgument);
    setProp.SetPropertyName(aPropertyName);
    VerifyOrExit(aPropertyValue != NULL, ret = kWpantundStatus_InvalidArgument);
    setProp.SetPropertyValue(aPropertyValue);
    setProp.SetDestination(gInterfaceName->GetDBusName());
    ret = setProp.ProcessReply();
exit:
    return ret;
}

int WPANController::Gateway(const char *aPrefix, uint8_t aLength, bool aIsDefaultRoute)
{
    DBusGateway gateway;
    int         ret = 0;

    gateway.SetDefaultRoute(aIsDefaultRoute);
    VerifyOrExit(aPrefix != NULL, ret = kWpantundStatus_InvalidArgument);
    gateway.SetPrefix(aPrefix);
    VerifyOrExit((aLength > 0 && aLength < 128), ret = kWpantundStatus_InvalidArgument);
    gateway.SetPrefixLength(aLength);
    gateway.SetDestination(gInterfaceName->GetDBusName());
    ret = gateway.ProcessReply();
exit:
    return ret;
}

int WPANController::RemoveGateway(const char *aPrefix, uint8_t aLength)
{
    DBusGateway gateway;
    int         ret = 0;

    gateway.SetDefaultRoute(true);
    VerifyOrExit(aPrefix != NULL, ret = kWpantundStatus_InvalidArgument);
    gateway.SetPrefix(aPrefix);
    VerifyOrExit((aLength > 0 && aLength < 128), ret = kWpantundStatus_InvalidArgument);
    gateway.SetPrefixLength(aLength);
    gateway.SetValidLifeTime(0);
    gateway.SetPreferredLifetime(0);
    gateway.SetDestination(gInterfaceName->GetDBusName());
    ret = gateway.ProcessReply();
exit:
    return ret;
}

} //namespace Dbus
} //namespace ot
