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
#include "utils/strcpy_utils.hpp"

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

int WPANController::Scan(void)
{
    int      ret = 0;
    DBusScan scannedNetwork;
    char     path[DBUS_MAXIMUM_NAME_LENGTH + 1];

    scannedNetwork.SetChannelMask(0);
    scannedNetwork.SetInterfaceName(mIfName);
    snprintf(path, sizeof(path), "%s/%s", WPANTUND_DBUS_PATH, mIfName);
    scannedNetwork.SetPath(path);
    scannedNetwork.SetDestination(GetDBusInterfaceName());
    scannedNetwork.SetInterface(WPANTUND_DBUS_APIv1_INTERFACE);
    VerifyOrExit((ret = scannedNetwork.ProcessReply()) == 0);

    VerifyOrExit((mScannedNetworkCount = scannedNetwork.GetNetworksCount()) > 0, ret = kWpantundStatus_NetworkNotFound);
    memcpy(mScannedNetworks, scannedNetwork.GetNetworks(), mScannedNetworkCount * sizeof(Dbus::WpanNetworkInfo));
exit:
    return ret;
}

const WpanNetworkInfo *WPANController::GetScanNetworksInfo(void) const
{
    return mScannedNetworks;
}

int WPANController::GetScanNetworksInfoCount(void) const
{
    return mScannedNetworkCount;
}

const char *WPANController::GetDBusInterfaceName(void) const
{
    DBusIfname dbusIfName;
    char *     destination;
    int        ret = kWpantundStatus_Ok;

    dbusIfName.SetInterfaceName(mIfName);
    VerifyOrExit(dbusIfName.ProcessReply() == kWpantundStatus_Ok, ret = kWpantundStatus_InvalidDBusName);
    destination = dbusIfName.GetDBusName();
exit:
    return ret ? NULL : destination;
}

int WPANController::Leave(void)
{
    DBusLeave leaveNetwork;
    char      path[DBUS_MAXIMUM_NAME_LENGTH + 1];

    leaveNetwork.SetDestination(GetDBusInterfaceName());
    leaveNetwork.SetInterfaceName(mIfName);
    snprintf(path, sizeof(path), "%s/%s", WPANTUND_DBUS_PATH, mIfName);
    leaveNetwork.SetPath(path);
    leaveNetwork.SetInterface(WPANTUND_DBUS_APIv1_INTERFACE);
    return leaveNetwork.ProcessReply();
}

int WPANController::Form(const char *aNetworkName, uint16_t aChannel)
{
    DBusForm formNetwork;
    int      ret = 0;
    char     path[DBUS_MAXIMUM_NAME_LENGTH + 1];

    VerifyOrExit(aNetworkName != NULL, ret = kWpantundStatus_InvalidArgument);
    formNetwork.SetNetworkName(aNetworkName);
    VerifyOrExit((aChannel <= 26 && aChannel >= 11), ret = kWpantundStatus_InvalidArgument);
    formNetwork.SetChannelMask(aChannel);
    formNetwork.SetInterfaceName(mIfName);
    formNetwork.SetNodeType(OT_ROUTER_ROLE);
    snprintf(path, sizeof(path), "%s/%s", WPAN_TUNNEL_DBUS_PATH, mIfName);
    formNetwork.SetPath(path);
    formNetwork.SetDestination(GetDBusInterfaceName());
    formNetwork.SetInterface(WPAN_TUNNEL_DBUS_INTERFACE);
    ret = formNetwork.ProcessReply();
exit:
    return ret;
}

int WPANController::Join(const char *aNetworkName, uint16_t aChannel, uint64_t aExtPanId, uint16_t aPanId)
{
    int      ret = 0;
    DBusJoin joinNetwork;
    char     path[DBUS_MAXIMUM_NAME_LENGTH + 1];

    VerifyOrExit(aNetworkName != NULL, ret = kWpantundStatus_InvalidArgument);
    joinNetwork.SetNetworkName(aNetworkName);
    joinNetwork.SetNodeType(OT_ROUTER_ROLE);
    joinNetwork.SetInterfaceName(mIfName);
    snprintf(path, sizeof(path), "%s/%s", WPAN_TUNNEL_DBUS_PATH, mIfName);
    joinNetwork.SetPath(path);
    VerifyOrExit((aChannel <= 26 && aChannel >= 11), ret = kWpantundStatus_InvalidArgument);
    joinNetwork.SetChannel(aChannel);
    VerifyOrExit(aExtPanId != 0, ret = kWpantundStatus_InvalidArgument);
    joinNetwork.SetExtPanId(aExtPanId);
    VerifyOrExit(aPanId != 0xffff, ret = kWpantundStatus_InvalidArgument);
    joinNetwork.SetPanId(aPanId);
    joinNetwork.SetDestination(GetDBusInterfaceName());
    joinNetwork.SetInterface(WPAN_TUNNEL_DBUS_INTERFACE);
    ret = joinNetwork.ProcessReply();
exit:
    return ret;
}

const char *WPANController::Get(const char *aPropertyName) const
{
    DBusGet getProp;
    int     ret = kWpantundStatus_Ok;
    char    path[DBUS_MAXIMUM_NAME_LENGTH + 1];

    VerifyOrExit(aPropertyName != NULL, ret = kWpantundStatus_InvalidArgument);
    getProp.SetInterfaceName(mIfName);
    snprintf(path, sizeof(path), "%s/%s", WPANTUND_DBUS_PATH, mIfName);
    getProp.SetInterface(WPANTUND_DBUS_APIv1_INTERFACE);
    getProp.SetPath(path);
    getProp.SetDestination(GetDBusInterfaceName());

exit:

    if (ret != kWpantundStatus_Ok)
    {
        otbrLog(OTBR_LOG_ERR, "error: %d", ret);
    }

    return ret ? "" : getProp.GetPropertyValue(aPropertyName);
}

int WPANController::Set(uint8_t aType, const char *aPropertyName, const char *aPropertyValue)
{
    DBusSet setProp;
    int     ret = kWpantundStatus_Ok;
    char    path[DBUS_MAXIMUM_NAME_LENGTH + 1];

    VerifyOrExit((aType == kPropertyType_String || aType == kPropertyType_Data), ret = kWpantundStatus_InvalidArgument);
    setProp.SetPropertyType(aType);
    VerifyOrExit(aPropertyName != NULL, ret = kWpantundStatus_InvalidArgument);
    setProp.SetPropertyName(aPropertyName);
    VerifyOrExit(aPropertyValue != NULL, ret = kWpantundStatus_InvalidArgument);
    setProp.SetPropertyValue(aPropertyValue);
    setProp.SetDestination(GetDBusInterfaceName());
    setProp.SetInterfaceName(mIfName);
    snprintf(path, sizeof(path), "%s/%s", WPANTUND_DBUS_PATH, mIfName);
    setProp.SetPath(path);
    setProp.SetInterface(WPANTUND_DBUS_APIv1_INTERFACE);
    ret = setProp.ProcessReply();
exit:
    return ret;
}

int WPANController::AddGateway(const char *aPrefix, bool aIsDefaultRoute)
{
    DBusGateway gateway;
    int         ret = kWpantundStatus_Ok;
    char        path[DBUS_MAXIMUM_NAME_LENGTH + 1];

    gateway.SetDefaultRoute(aIsDefaultRoute);
    VerifyOrExit(aPrefix != NULL, ret = kWpantundStatus_InvalidArgument);
    gateway.SetPrefix(aPrefix);
    gateway.SetDestination(GetDBusInterfaceName());
    gateway.SetInterfaceName(mIfName);
    snprintf(path, sizeof(path), "%s/%s", WPANTUND_DBUS_PATH, mIfName);
    gateway.SetPath(path);
    gateway.SetInterface(WPANTUND_DBUS_APIv1_INTERFACE);
    ret = gateway.ProcessReply();
exit:
    return ret;
}

int WPANController::RemoveGateway(const char *aPrefix)
{
    DBusGateway gateway;
    int         ret = kWpantundStatus_Ok;
    char        path[DBUS_MAXIMUM_NAME_LENGTH + 1];

    gateway.SetDefaultRoute(true);
    VerifyOrExit(aPrefix != NULL, ret = kWpantundStatus_InvalidArgument);
    gateway.SetPrefix(aPrefix);
    gateway.SetValidLifeTime(0);
    gateway.SetPreferredLifetime(0);
    gateway.SetDestination(GetDBusInterfaceName());
    gateway.SetInterfaceName(mIfName);
    snprintf(path, sizeof(path), "%s/%s", WPANTUND_DBUS_PATH, mIfName);
    gateway.SetPath(path);
    gateway.SetInterface(WPANTUND_DBUS_APIv1_INTERFACE);
    ret = gateway.ProcessReply();
exit:
    return ret;
}

void WPANController::SetInterfaceName(const char *aIfName)
{
    strcpy_safe(mIfName, sizeof(mIfName), aIfName);
}

} // namespace Dbus
} // namespace ot
