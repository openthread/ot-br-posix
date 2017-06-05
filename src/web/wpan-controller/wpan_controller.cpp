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

int WPANController::Scan(void)
{
    int      ret = 0;
    DBusScan scanNetwork;
    char     path[DBUS_MAXIMUM_NAME_LENGTH + 1];

    scanNetwork.SetChannelMask(0);
    scanNetwork.SetInterfaceName(mIfName);
    snprintf(path, sizeof(path), "%s/%s", WPANTUND_DBUS_PATH,
             mIfName);
    scanNetwork.SetPath(path);
    scanNetwork.SetDestination(this->GetDBusInterfaceName());
    scanNetwork.SetIface(WPANTUND_DBUS_APIv1_INTERFACE);
    VerifyOrExit((ret = scanNetwork.ProcessReply()) == 0);

    VerifyOrExit((mScannedNetworkCount = scanNetwork.GetNetworksCount()) > 0,
                 ret = kWpantundStatus_NetworkNotFound);
    memcpy(mScanedNetowrks, scanNetwork.GetNetworks(),
           mScannedNetworkCount * sizeof(Dbus::WpanNetworkInfo));
exit:
    return ret;
}

const WpanNetworkInfo *WPANController::GetScanNetworksInfo(void)
{
    return mScanedNetowrks;
}

int WPANController::GetScanNetworksInfoCount(void)
{
    return mScannedNetworkCount;
}

const char *WPANController::GetDBusInterfaceName(void)
{

    DBusIfname dbusIfName;
    int        ret = kWpantundStatus_Ok;

    dbusIfName.SetInterfaceName(mIfName);
    VerifyOrExit(dbusIfName.ProcessReply() == kWpantundStatus_Ok, ret = kWpantundStatus_InvalidDBusName);
exit:
    return ret ? dbusIfName.GetDBusName() : NULL;
}

int WPANController::Leave(void)
{
    DBusLeave leaveNetwork;
    char      path[DBUS_MAXIMUM_NAME_LENGTH + 1];

    leaveNetwork.SetDestination(this->GetDBusInterfaceName());
    leaveNetwork.SetInterfaceName(mIfName);
    snprintf(path, sizeof(path), "%s/%s", WPANTUND_DBUS_PATH,
             mIfName);
    leaveNetwork.SetPath(path);
    leaveNetwork.SetIface(WPANTUND_DBUS_APIv1_INTERFACE);
    return leaveNetwork.ProcessReply();
}

int WPANController::Form(const char *aNetworkname, int aChannel)
{
    DBusForm formNetwork;
    int      ret = 0;
    char     path[DBUS_MAXIMUM_NAME_LENGTH + 1];

    VerifyOrExit(aNetworkname != NULL, ret = kWpantundStatus_InvalidArgument);
    formNetwork.SetNetworkName(aNetworkname);
    VerifyOrExit((aChannel <= 26 && aChannel >= 11), ret = kWpantundStatus_InvalidArgument);
    formNetwork.SetChannelMask(aChannel);
    formNetwork.SetInterfaceName(mIfName);
    formNetwork.SetNodeType(LEADER_ROLE);
    snprintf(path, sizeof(path), "%s/%s", WPAN_TUNNEL_DBUS_PATH,
             mIfName);
    formNetwork.SetPath(path);
    formNetwork.SetDestination(this->GetDBusInterfaceName());
    formNetwork.SetIface(WPAN_TUNNEL_DBUS_INTERFACE);
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
    char     path[DBUS_MAXIMUM_NAME_LENGTH + 1];

    VerifyOrExit(aNetworkname != NULL, ret = kWpantundStatus_InvalidArgument);
    joinNetwork.SetNetworkName(aNetworkname);
    joinNetwork.SetNodeType(ROUTER_ROLE);
    joinNetwork.SetInterfaceName(mIfName);
    snprintf(path, sizeof(path), "%s/%s", WPAN_TUNNEL_DBUS_PATH,
             mIfName);
    joinNetwork.SetPath(path);
    VerifyOrExit((aChannel <= 26 && aChannel >= 11), ret = kWpantundStatus_InvalidArgument);
    joinNetwork.SetChannel(aChannel);
    VerifyOrExit(aExtPanId != 0, ret = kWpantundStatus_InvalidArgument);
    joinNetwork.SetExtPanId(aExtPanId);
    VerifyOrExit(aPanId != 0, ret = kWpantundStatus_InvalidArgument);
    joinNetwork.SetPanId(aPanId);
    joinNetwork.SetDestination(this->GetDBusInterfaceName());
    joinNetwork.SetIface(WPAN_TUNNEL_DBUS_INTERFACE);
    ret = joinNetwork.ProcessReply();
exit:
    return ret;
}

const char *WPANController::Get(const char *aPropertyName)
{
    DBusGet getProp;
    int     ret = kWpantundStatus_Ok;
    char    path[DBUS_MAXIMUM_NAME_LENGTH + 1];

    VerifyOrExit(aPropertyName != NULL, ret = kWpantundStatus_InvalidArgument);
    getProp.SetInterfaceName(mIfName);
    snprintf(path, sizeof(path), "%s/%s", WPANTUND_DBUS_PATH,
             mIfName);
    getProp.SetIface(WPANTUND_DBUS_APIv1_INTERFACE);
    getProp.SetPath(path);
    getProp.SetDestination(this->GetDBusInterfaceName());

exit:

    if (ret != kWpantundStatus_Ok)
    {
        syslog(LOG_ERR, "property name is NULL; error: %d\n", ret);
    }

    return ret ? "" : getProp.GetPropertyValue(aPropertyName);
}

int WPANController::Set(uint8_t aType, const char *aPropertyName, const char *aPropertyValue)
{
    DBusSet setProp;
    int     ret = kWpantundStatus_Ok;
    char    path[DBUS_MAXIMUM_NAME_LENGTH + 1];

    VerifyOrExit((aType == kPropertyType_String || aType == kPropertyType_Data),
                 ret = kWpantundStatus_InvalidArgument);
    setProp.SetPropertyType(aType);
    VerifyOrExit(aPropertyName != NULL, ret = kWpantundStatus_InvalidArgument);
    setProp.SetPropertyName(aPropertyName);
    VerifyOrExit(aPropertyValue != NULL, ret = kWpantundStatus_InvalidArgument);
    setProp.SetPropertyValue(aPropertyValue);
    setProp.SetDestination(this->GetDBusInterfaceName());
    setProp.SetInterfaceName(mIfName);
    snprintf(path, sizeof(path), "%s/%s", WPANTUND_DBUS_PATH,
             mIfName);
    setProp.SetPath(path);
    setProp.SetIface(WPANTUND_DBUS_APIv1_INTERFACE);
    ret = setProp.ProcessReply();
exit:
    return ret;
}

int WPANController::Gateway(const char *aPrefix, uint8_t aLength, bool aIsDefaultRoute)
{
    DBusGateway gateway;
    int         ret = kWpantundStatus_Ok;
    char        path[DBUS_MAXIMUM_NAME_LENGTH + 1];

    gateway.SetDefaultRoute(aIsDefaultRoute);
    VerifyOrExit(aPrefix != NULL, ret = kWpantundStatus_InvalidArgument);
    gateway.SetPrefix(aPrefix);
    VerifyOrExit((aLength > 0 && aLength < 128), ret = kWpantundStatus_InvalidArgument);
    gateway.SetPrefixLength(aLength);
    gateway.SetDestination(this->GetDBusInterfaceName());
    gateway.SetInterfaceName(mIfName);
    snprintf(path, sizeof(path), "%s/%s", WPANTUND_DBUS_PATH,
             mIfName);
    gateway.SetPath(path);
    gateway.SetIface(WPANTUND_DBUS_APIv1_INTERFACE);
    ret = gateway.ProcessReply();
exit:
    return ret;
}

int WPANController::RemoveGateway(const char *aPrefix, uint8_t aLength)
{
    DBusGateway gateway;
    int         ret = kWpantundStatus_Ok;
    char        path[DBUS_MAXIMUM_NAME_LENGTH + 1];

    gateway.SetDefaultRoute(true);
    VerifyOrExit(aPrefix != NULL, ret = kWpantundStatus_InvalidArgument);
    gateway.SetPrefix(aPrefix);
    VerifyOrExit((aLength > 0 && aLength < 128), ret = kWpantundStatus_InvalidArgument);
    gateway.SetPrefixLength(aLength);
    gateway.SetValidLifeTime(0);
    gateway.SetPreferredLifetime(0);
    gateway.SetDestination(this->GetDBusInterfaceName());
    gateway.SetInterfaceName(mIfName);
    snprintf(path, sizeof(path), "%s/%s", WPANTUND_DBUS_PATH,
             mIfName);
    gateway.SetPath(path);
    gateway.SetIface(WPANTUND_DBUS_APIv1_INTERFACE);
    ret = gateway.ProcessReply();
exit:
    return ret;
}

void WPANController::SetInterfaceName(const char *aIfName)
{
    strcpy(mIfName, aIfName);
}

} //namespace Dbus
} //namespace ot
