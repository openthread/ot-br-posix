/*
 *
 * Copyright (c) 2016 Nest Labs, Inc.
 * All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#ifndef wpantund_wpan_dbus_v0_h
#define wpantund_wpan_dbus_v0_h

#include "wpan-properties.h"

#if !defined(wpantund_DBusIPCAPI_v0_h) && !defined(BUILD_FEATURE_WPANTUND)
// Disabling these warnings for now.
// Will re-enable once more stuff has moved over.
//#warning wpan-dbus-v0.h is deprecated. Please migrate to the API in wpan-dbus-v1.h.
#endif

#define WPAN_TUNNEL_DBUS_NAME   "com.nestlabs.WPANTunnelDriver"
#define WPAN_TUNNEL_DBUS_INTERFACE  "com.nestlabs.WPANTunnelDriver"
#define WPAN_TUNNEL_DBUS_PATH   "/com/nestlabs/WPANTunnelDriver"
#define WPAN_TUNNEL_DBUS_VERSION   2
#define WPAN_TUNNEL_DBUS_PATH_PROPERTIES "Properties"

#define WPAN_TUNNEL_CMD_GET_INTERFACES  "GetInterfaces"
#define WPAN_TUNNEL_CMD_GET_VERSION     "GetVersion"

#define WPAN_TUNNEL_SIGNAL_INTERFACE_ADDED			"InterfaceAdded"
#define WPAN_TUNNEL_SIGNAL_INTERFACE_REMOVED		"InterfaceRemoved"

#define WPAN_IFACE_SIGNAL_STATE_CHANGED	"AssociationStateChanged"
#define WPAN_IFACE_SIGNAL_PROPERTY_CHANGED	"PropertyChanged"
#define WPAN_IFACE_SIGNAL_NET_WAKE	    "NetWake"
#define WPAN_IFACE_SIGNAL_PREVENT_SLEEP	"PreventSleep"
#define WPAN_IFACE_SIGNAL_ALLOW_SLEEP	"AllowSleep"

#define WPAN_IFACE_CMD_JOIN             "Join"
#define WPAN_IFACE_CMD_FORM             "Form"
#define WPAN_IFACE_CMD_LEAVE            "Leave"
#define WPAN_IFACE_CMD_ACTIVE_SCAN      "Scan"
#define WPAN_IFACE_CMD_RESUME           "Resume"
#define WPAN_IFACE_CMD_PERMIT_JOIN      "PermitJoin"
#define WPAN_IFACE_CMD_STOP_SCAN        "StopScan"
#define WPAN_IFACE_CMD_RESET            "Reset"
#define WPAN_IFACE_CMD_STATUS           "Status"
#define WPAN_IFACE_CMD_PING             "Ping"
#define WPAN_IFACE_CMD_SCAN             "Scan"
#define WPAN_IFACE_CMD_BEGIN_NET_WAKE   "BeginNetWake"
#define WPAN_IFACE_CMD_CONFIG_GATEWAY   "ConfigGateway"
#define WPAN_IFACE_CMD_ADD_ROUTE        "AddRoute"
#define WPAN_IFACE_CMD_REMOVE_ROUTE     "RemoveRoute"
#define WPAN_IFACE_CMD_DATA_POLL        "DataPoll"
#define WPAN_IFACE_CMD_HOST_DID_WAKE    "HostDidWake"

#define WPAN_IFACE_CMD_BEGIN_LOW_POWER  "BeginLowPower"

/* The property "MfgTestMode" must be enabled in order to use these commands */
#define WPAN_IFACE_CMD_MFG_FINISH      "MfgFinish"
#define WPAN_IFACE_CMD_MFG_BEGIN_TEST  "MfgBeginTest"	// Argument is test type
#define WPAN_IFACE_CMD_MFG_END_TEST    "MfgEndTest"		// Argument is test type
#define WPAN_IFACE_CMD_MFG_TX_PACKET   "MfgTXPacket"	// Argument is packet
#define WPAN_IFACE_SIGNAL_MFG_RX       "MfgRXPacket"    // packet, lqi, rssi
#define WPAN_IFACE_CMD_MFG_CLOCKMON       "MfgClockMon"
#define WPAN_IFACE_CMD_MFG_GPIO_SET       "MfgGPIOSet"
#define WPAN_IFACE_CMD_MFG_GPIO_GET       "MfgGPIOGet"
#define WPAN_IFACE_CMD_MFG_CHANNELCAL     "MfgChannelCal"
#define WPAN_IFACE_CMD_MFG_CHANNELCAL_GET "MfgChannelCalGet"
/* Also, there are the following properties that can be set when in MFG mode:
 *	"Channel", "TXPower", "SYNOffset"
 */


#define WPAN_IFACE_CMD_GET_PROP         "GetProp"
#define WPAN_IFACE_CMD_SET_PROP         "SetProp"
#define WPAN_IFACE_CMD_INSERT_PROP      "InsertProp"
#define WPAN_IFACE_CMD_REMOVE_PROP      "RemoveProp"


#define WPAN_IFACE_PROTOCOL_TCPUDP      0xFF
#define WPAN_IFACE_PROTOCOL_TCP         6
#define WPAN_IFACE_PROTOCOL_UDP         17

#define WPAN_IFACE_ROLE_UNKNOWN             0
#define WPAN_IFACE_ROLE_ROUTER              2
#define WPAN_IFACE_ROLE_END_DEVICE          3
#define WPAN_IFACE_ROLE_SLEEPY_END_DEVICE   4
#define WPAN_IFACE_ROLE_LURKER              6

#endif
