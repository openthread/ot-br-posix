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

#ifndef wpantund_wpan_dbus_v1_h
#define wpantund_wpan_dbus_v1_h

#include "wpan-properties.h"

#define WPANTUND_DBUS_NAME                      "org.wpantund"
#define WPANTUND_DBUS_PATH                      "/org/wpantund"

// ============================================================================
// Base Interface

#define WPANTUND_DBUS_BASE_INTERFACE            "org.wpantund"

#define WPANTUND_BASE_CMD_GET_INTERFACES        "GetInterfaces"

#define WPANTUND_BASE_SIGNAL_INTERFACE_ADDED    "InterfaceAdded"
#define WPANTUND_BASE_SIGNAL_INTERFACE_REMOVED  "InterfaceRemoved"

#define WPANTUND_IF_GET_VERSION                 "GetVersion"

// ============================================================================
// Standard API Interface

// Note that this API is not yet fully baked and may change before
// being considered frozen.

#define WPANTUND_DBUS_APIv1_INTERFACE         WPANTUND_DBUS_BASE_INTERFACE".v1"

#define WPANTUND_IF_CMD_JOIN                  "Join"
#define WPANTUND_IF_CMD_FORM                  "Form"
#define WPANTUND_IF_CMD_LEAVE                 "Leave"
#define WPANTUND_IF_CMD_ATTACH                "Attach"

#define WPANTUND_IF_CMD_RESET                 "Reset"
#define WPANTUND_IF_CMD_STATUS                "Status"

#define WPANTUND_IF_CMD_ROUTE_ADD             "RouteAdd"
#define WPANTUND_IF_CMD_ROUTE_REMOVE          "RouteRemove"
#define WPANTUND_IF_CMD_CONFIG_GATEWAY        "ConfigGateway"
#define WPANTUND_IF_CMD_DATA_POLL             "DataPoll"

#define WPANTUND_IF_CMD_BEGIN_LOW_POWER       "BeginLowPower"
#define WPANTUND_IF_CMD_HOST_DID_WAKE         "HostDidWake"

#define WPANTUND_IF_CMD_PCAP_TO_FD            "PcapToFd"
#define WPANTUND_IF_CMD_PCAP_TERMINATE        "PcapTerminate"

#define WPANTUND_IF_CMD_NET_SCAN_START        "NetScanStart"
#define WPANTUND_IF_CMD_NET_SCAN_STOP         "NetScanStop"
#define WPANTUND_IF_SIGNAL_NET_SCAN_BEACON    "NetScanBeacon"

#define WPANTUND_IF_CMD_ENERGY_SCAN_START     "EnergyScanStart"
#define WPANTUND_IF_CMD_ENERGY_SCAN_STOP      "EnergyScanStop"
#define WPANTUND_IF_SIGNAL_ENERGY_SCAN_RESULT "EnergyScanResult"

#define WPANTUND_IF_CMD_PROP_GET              "PropGet"
#define WPANTUND_IF_CMD_PROP_SET              "PropSet"
#define WPANTUND_IF_SIGNAL_PROP_CHANGED       "PropChanged"

// ============================================================================
// NestLabs Internal API Interface

#define WPANTUND_DBUS_NLAPIv1_INTERFACE       "com.nestlabs.wpantund.v1"

#define WPANTUND_IF_CMD_PERMIT_JOIN           "PermitJoin"
#define WPANTUND_IF_CMD_NETWORK_WAKE_BEGIN    "NetworkWakeBegin"

#define WPANTUND_IF_CMD_MFG                   "Mfg"

#endif
