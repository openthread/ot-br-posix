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

#ifndef OTBR_DBUS_CONSTANTS_HPP_
#define OTBR_DBUS_CONSTANTS_HPP_

#define DBUS_PROPERTY_GET_METHOD "Get"
#define DBUS_PROPERTY_SET_METHOD "Set"
#define DBUS_PROPERTY_GET_ALL_METHOD "GetAll"
#define DBUS_PROPERTIES_CHANGED_SIGNAL "PropertiesChanged"

#define OTBR_DBUS_SERVER_PREFIX "io.openthread.BorderRouter."
#define OTBR_DBUS_THREAD_INTERFACE "io.openthread.BorderRouter"
#define OTBR_DBUS_OBJECT_PREFIX "/io/openthread/BorderRouter/"

#define OTBR_DBUS_SCAN_METHOD "Scan"
#define OTBR_DBUS_ATTACH_METHOD "Attach"
#define OTBR_DBUS_FACTORY_RESET_METHOD "FactoryReset"
#define OTBR_DBUS_RESET_METHOD "Reset"
#define OTBR_DBUS_ADD_ON_MESH_PREFIX_METHOD "AddOnMeshPrefix"
#define OTBR_DBUS_REMOVE_ON_MESH_PREFIX_METHOD "RemoveOnMeshPrefix"
#define OTBR_DBUS_ADD_UNSECURE_PORT_METHOD "AddUnsecurePort"
#define OTBR_DBUS_JOINER_START_METHOD "JoinerStart"
#define OTBR_DBUS_JOINER_STOP_METHOD "JoinerStop"
#define OTBR_DBUS_MESH_LOCAL_PREFIX_PROPERTY "MeshLocalPrefix"
#define OTBR_DBUS_LEGACY_ULA_PREFIX_PROPERTY "LegacyULAPrefix"
#define OTBR_DBUS_LINK_MODE_PROPERTY "LinkMode"
#define OTBR_DBUS_DEVICE_ROLE_PROPERTY "DeviceRole"
#define OTBR_DBUS_NETWORK_NAME_PROPERTY "NetworkName"
#define OTBR_DBUS_PANID_PROPERTY "PanId"
#define OTBR_DBUS_EXTPANID_PROPERTY "ExtPanId"
#define OTBR_DBUS_CHANNEL_PROPERTY "Channel"
#define OTBR_DBUS_MASTER_KEY_PROPERTY "MasterKey"
#define OTBR_DBUS_CCA_FAILURE_RATE_PROPERTY "CcaFailureRate"
#define OTBR_DBUS_LINK_COUNTERS_PROPERTY "LinkCounters"
#define OTBR_DBUS_IP6_COUNTERS_PROPERTY "Ip6Counters"
#define OTBR_DBUS_SUPPORTED_CHANNEL_MASK_PROPERTY "LinkSupportedChannelMask"
#define OTBR_DISABLED_ROLE_NAME "disabled"
#define OTBR_DETACHED_ROLE_NAME "detached"
#define OTBR_CHILD_ROLE_NAME "child"
#define OTBR_ROUTER_ROLE_NAME "router"
#define OTBR_LEADER_ROLE_NAME "leader"

#endif // OTBR_DBUS_CONSTANTS_HPP_
