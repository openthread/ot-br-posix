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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>

#include <string.h>
#include <errno.h>
#include <algorithm>

#include <boost/bind.hpp>

#include <dbus/dbus.h>

#include "DBusIPCAPI_v0.h"
#include "wpan-dbus-v0.h"

#include "NCPControlInterface.h"
#include "NCPTypes.h"
#include "NCPMfgInterface_v0.h"
#include "assert-macros.h"

#include "DBUSHelpers.h"
#include "any-to.h"
#include "wpan-error.h"

using namespace DBUSHelpers;
using namespace nl;
using namespace nl::wpantund;

DBusIPCAPI_v0::DBusIPCAPI_v0(DBusConnection *connection)
	:mConnection(connection)
{
	dbus_connection_ref(mConnection);
	init_callback_tables();
}

DBusIPCAPI_v0::~DBusIPCAPI_v0()
{
	dbus_connection_unref(mConnection);
}

void
DBusIPCAPI_v0::init_callback_tables()
{
#define INTERFACE_CALLBACK_CONNECT(cmd_name, member_func) \
	mInterfaceCallbackTable[(cmd_name)] = boost::bind( \
		&DBusIPCAPI_v0::member_func, this, _1, _2)

	INTERFACE_CALLBACK_CONNECT(WPAN_IFACE_CMD_JOIN, interface_join_handler);
	INTERFACE_CALLBACK_CONNECT(WPAN_IFACE_CMD_FORM, interface_form_handler);
	INTERFACE_CALLBACK_CONNECT(WPAN_IFACE_CMD_BEGIN_NET_WAKE, interface_begin_net_wake_handler);
	INTERFACE_CALLBACK_CONNECT(WPAN_IFACE_CMD_PERMIT_JOIN, interface_permit_join_handler);
	INTERFACE_CALLBACK_CONNECT(WPAN_IFACE_CMD_LEAVE, interface_leave_handler);
	INTERFACE_CALLBACK_CONNECT(WPAN_IFACE_CMD_DATA_POLL, interface_data_poll_handler);
	INTERFACE_CALLBACK_CONNECT(WPAN_IFACE_CMD_CONFIG_GATEWAY, interface_config_gateway_handler);
	INTERFACE_CALLBACK_CONNECT(WPAN_IFACE_CMD_ADD_ROUTE, interface_add_route_handler);
	INTERFACE_CALLBACK_CONNECT(WPAN_IFACE_CMD_REMOVE_ROUTE, interface_remove_route_handler);
	INTERFACE_CALLBACK_CONNECT(WPAN_IFACE_CMD_BEGIN_LOW_POWER, interface_begin_low_power_handler);
	INTERFACE_CALLBACK_CONNECT(WPAN_IFACE_CMD_PING, interface_ping_handler);
	INTERFACE_CALLBACK_CONNECT(WPAN_IFACE_CMD_HOST_DID_WAKE, interface_host_did_wake_handler);
	INTERFACE_CALLBACK_CONNECT(WPAN_IFACE_CMD_STOP_SCAN, interface_stop_scan_handler);
	INTERFACE_CALLBACK_CONNECT(WPAN_IFACE_CMD_GET_PROP, interface_get_prop_handler);
	INTERFACE_CALLBACK_CONNECT(WPAN_IFACE_CMD_SET_PROP, interface_set_prop_handler);
	INTERFACE_CALLBACK_CONNECT(WPAN_IFACE_CMD_INSERT_PROP, interface_insert_prop_handler);
	INTERFACE_CALLBACK_CONNECT(WPAN_IFACE_CMD_REMOVE_PROP, interface_remove_prop_handler);
	INTERFACE_CALLBACK_CONNECT(WPAN_IFACE_CMD_RESET, interface_reset_handler);
	INTERFACE_CALLBACK_CONNECT(WPAN_IFACE_CMD_STATUS, interface_status_handler);
	INTERFACE_CALLBACK_CONNECT(WPAN_IFACE_CMD_ACTIVE_SCAN, interface_active_scan_handler);
	INTERFACE_CALLBACK_CONNECT(WPAN_IFACE_CMD_RESUME, interface_resume_handler);
	INTERFACE_CALLBACK_CONNECT(WPAN_IFACE_CMD_MFG_BEGIN_TEST, interface_mfg_begin_test_handler);
	INTERFACE_CALLBACK_CONNECT(WPAN_IFACE_CMD_MFG_END_TEST, interface_mfg_end_test_handler);
	INTERFACE_CALLBACK_CONNECT(WPAN_IFACE_CMD_MFG_TX_PACKET, interface_mfg_tx_packet_handler);
	INTERFACE_CALLBACK_CONNECT(WPAN_IFACE_CMD_MFG_FINISH, interface_mfg_finish_handler);

	INTERFACE_CALLBACK_CONNECT(WPAN_IFACE_CMD_MFG_CLOCKMON, interface_mfg_clockmon_handler);
	INTERFACE_CALLBACK_CONNECT(WPAN_IFACE_CMD_MFG_GPIO_SET, interface_mfg_gpio_set_handler);
	INTERFACE_CALLBACK_CONNECT(WPAN_IFACE_CMD_MFG_GPIO_GET, interface_mfg_gpio_get_handler);
	INTERFACE_CALLBACK_CONNECT(WPAN_IFACE_CMD_MFG_CHANNELCAL, interface_mfg_channelcal_handler);
	INTERFACE_CALLBACK_CONNECT(WPAN_IFACE_CMD_MFG_CHANNELCAL_GET, interface_mfg_channelcal_get_handler);
}

void
DBusIPCAPI_v0::CallbackWithStatus_Helper(
    int ret, DBusMessage *original_message
    )
{
	DBusMessage *reply = dbus_message_new_method_return(original_message);

	syslog(LOG_DEBUG, "Sending DBus response for \"%s\" to \"%s\"", dbus_message_get_member(original_message), dbus_message_get_sender(original_message));

	if(reply) {
		dbus_message_append_args(
			reply,
			DBUS_TYPE_INT32, &ret,
			DBUS_TYPE_INVALID
			);

		dbus_connection_send(mConnection, reply, NULL);
		dbus_message_unref(reply);
	}
	dbus_message_unref(original_message);
}

static void
ipc_append_network_properties(
    DBusMessageIter *iter, const WPAN::NetworkInstance& network
    )
{
	const char* network_name = network.name.c_str();

	if (network_name[0])
		append_dict_entry(iter,
		                  "NetworkName",
		                  DBUS_TYPE_STRING,
		                  &network_name);

	if (network.get_xpanid_as_uint64() != 0) {
		uint64_t xpan_id = network.get_xpanid_as_uint64();
		append_dict_entry(iter, "XPanId", DBUS_TYPE_UINT64, &xpan_id);
	}

	if (network.panid && network.panid != 0xFFFF) {
		uint16_t pan_id = network.panid;
		append_dict_entry(iter, "PanId", DBUS_TYPE_UINT16, &pan_id);
	}

	if (network.channel) {
		uint16_t channel = network.channel;
		append_dict_entry(iter, "Channel", DBUS_TYPE_INT16, &channel);

		if (network.rssi != -128) {
			int8_t rssi = network.rssi;
			append_dict_entry(iter, "RSSI", DBUS_TYPE_BYTE, &rssi);
		}

		dbus_bool_t allowing_join = network.joinable;
		append_dict_entry(iter,
		                  "AllowingJoin",
		                  DBUS_TYPE_BOOLEAN,
		                  &allowing_join);
	}

	if (network.type != 0) {
		int32_t type = network.type;
		append_dict_entry(iter, "Type", DBUS_TYPE_INT32, &type);
	}

	if (network.get_hwaddr_as_uint64() != 0)
		append_dict_entry(iter, "BeaconHWAddr",
		                  nl::Data(network.hwaddr, 8));

}

static void
ipc_append_network_dict(
    DBusMessageIter *iter, const WPAN::NetworkInstance& network
    )
{
	DBusMessageIter dict;

	dbus_message_iter_open_container(
	    iter,
	    DBUS_TYPE_ARRAY,
	    DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
	    DBUS_TYPE_STRING_AS_STRING
	    DBUS_TYPE_VARIANT_AS_STRING
	    DBUS_DICT_ENTRY_END_CHAR_AS_STRING,
	    &dict
	    );
	ipc_append_network_properties(&dict, network);


	dbus_message_iter_close_container(iter, &dict);
}

static void
ipc_append_networks(
    DBusMessageIter *iter, const std::list<WPAN::NetworkInstance>& networks
    )
{
	DBusMessageIter array;

	dbus_message_iter_open_container(
	    iter,
	    DBUS_TYPE_ARRAY,
	    DBUS_TYPE_ARRAY_AS_STRING
	    DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
	    DBUS_TYPE_STRING_AS_STRING
	    DBUS_TYPE_VARIANT_AS_STRING
	    DBUS_DICT_ENTRY_END_CHAR_AS_STRING,
	    &array
	    );

	std::list<WPAN::NetworkInstance>::const_iterator i;
	std::list<WPAN::NetworkInstance>::const_iterator end = networks.end();
	for (i = networks.begin(); i != end; ++i) {
		ipc_append_network_dict(&array, *i);
	}

	dbus_message_iter_close_container(iter, &array);
}

void
DBusIPCAPI_v0::received_beacon(NCPControlInterface* interface, const WPAN::NetworkInstance& network)
{
	mReceivedBeacons.push_back(network);
}

void
DBusIPCAPI_v0::scan_response_helper(
    int ret,
    DBusMessage *                                                   original_message
)
{
	DBusMessage *reply = dbus_message_new_method_return(original_message);

	if(reply) {
		DBusMessageIter msg_iter;
		dbus_message_append_args(
			reply,
			DBUS_TYPE_INT32, &ret,
			DBUS_TYPE_INVALID
			);

		dbus_message_iter_init_append(reply, &msg_iter);

		ipc_append_networks(&msg_iter, mReceivedBeacons);

		dbus_connection_send(mConnection, reply, NULL);
		dbus_message_unref(reply);
	}
	dbus_message_unref(original_message);
}

void
DBusIPCAPI_v0::status_response_helper(
    int ret, NCPControlInterface* interface, DBusMessage *message
    )
{
	DBusMessage *reply = dbus_message_new_method_return(message);

	if (reply) {
		DBusMessageIter iter;
		dbus_message_iter_init_append(reply, &iter);

		DBusMessageIter dict;
		boost::any value;
		NCPState ncp_state = UNINITIALIZED;
		std::string ncp_state_string;
		const char* ncp_state_cstr = kWPANTUNDStateUninitialized;

		dbus_message_iter_open_container(
			&iter,
			DBUS_TYPE_ARRAY,
			DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
			DBUS_TYPE_STRING_AS_STRING
			DBUS_TYPE_VARIANT_AS_STRING
			DBUS_DICT_ENTRY_END_CHAR_AS_STRING,
			&dict
		);

		value = interface->property_get_value(kWPANTUNDProperty_NCPState);

		if (!value.empty()) {
			ncp_state_string = any_to_string(value);
			ncp_state = string_to_ncp_state(ncp_state_string);
			ncp_state_cstr = ncp_state_string.c_str();
		}

		append_dict_entry(&dict,
						  kWPANTUNDProperty_NCPState,
						  DBUS_TYPE_STRING,
						  &ncp_state_cstr);

		if (ncp_state_is_commissioned(ncp_state))
		{
			value = interface->property_get_value(kWPANTUNDProperty_NetworkName);
			if (!value.empty()) {
				append_dict_entry(&dict, kWPANTUNDProperty_NetworkName, value);
			}

			value = interface->property_get_value(kWPANTUNDProperty_NetworkXPANID);
			if (!value.empty()) {
				append_dict_entry(&dict, kWPANTUNDProperty_NetworkXPANID, value);
			}

			value = interface->property_get_value(kWPANTUNDProperty_NetworkPANID);
			if (!value.empty()) {
				append_dict_entry(&dict, kWPANTUNDProperty_NetworkPANID, value);
			}

			value = interface->property_get_value(kWPANTUNDProperty_NCPChannel);
			if (!value.empty()) {
				append_dict_entry(&dict, kWPANTUNDProperty_NCPChannel, value);
			}

			value = interface->property_get_value(kWPANTUNDProperty_IPv6LinkLocalAddress);
			if (!value.empty()) {
				append_dict_entry(&dict, kWPANTUNDProperty_IPv6LinkLocalAddress, value);
			}

			value = interface->property_get_value(kWPANTUNDProperty_IPv6MeshLocalAddress);
			if (!value.empty()) {
				append_dict_entry(&dict, kWPANTUNDProperty_IPv6MeshLocalAddress, value);
			}

			value = interface->property_get_value(kWPANTUNDProperty_NestLabs_LegacyMeshLocalAddress);
			if (!value.empty()) {
				append_dict_entry(&dict, kWPANTUNDProperty_NestLabs_LegacyMeshLocalAddress, value);
			}

			value = interface->property_get_value(kWPANTUNDProperty_IPv6MeshLocalPrefix);
			if (!value.empty()) {
				append_dict_entry(&dict, kWPANTUNDProperty_IPv6MeshLocalPrefix, value);
			}

			value = interface->property_get_value(kWPANTUNDProperty_NestLabs_LegacyMeshLocalPrefix);
			if (!value.empty()) {
				append_dict_entry(&dict, kWPANTUNDProperty_NestLabs_LegacyMeshLocalPrefix, value);
			}

			value = interface->property_get_value(kWPANTUNDProperty_NestLabs_NetworkAllowingJoin);
			if (!value.empty()) {
				append_dict_entry(&dict, kWPANTUNDProperty_NestLabs_NetworkAllowingJoin, value);
			}

			value = interface->property_get_value(kWPANTUNDProperty_NetworkNodeType);
			if (!value.empty()) {
				append_dict_entry(&dict, kWPANTUNDProperty_NetworkNodeType, value);
			}
		}

		value = interface->property_get_value(kWPANTUNDProperty_DaemonEnabled);
		if (!value.empty()) {
			append_dict_entry(&dict, kWPANTUNDProperty_DaemonEnabled, value);
		}

		value = interface->property_get_value(kWPANTUNDProperty_NCPVersion);
		if (!value.empty()) {
			append_dict_entry(&dict, kWPANTUNDProperty_NCPVersion, value);
		}

		value = interface->property_get_value(kWPANTUNDProperty_DaemonVersion);
		if (!value.empty()) {
			append_dict_entry(&dict, kWPANTUNDProperty_DaemonVersion, value);
		}

		value = interface->property_get_value(kWPANTUNDProperty_NCPHardwareAddress);
		if (!value.empty()) {
			append_dict_entry(&dict, kWPANTUNDProperty_NCPHardwareAddress, value);
		}

		dbus_message_iter_close_container(&iter, &dict);

		dbus_connection_send(mConnection, reply, NULL);
		dbus_message_unref(reply);
	}
	dbus_message_unref(message);
}

void
DBusIPCAPI_v0::CallbackWithStatusArg1_Helper(
    int status, const boost::any& value, DBusMessage *message
)
{
	DBusMessage *reply = dbus_message_new_method_return(message);
	DBusMessageIter iter;

	syslog(LOG_DEBUG, "Sending getprop response");

	dbus_message_iter_init_append(reply, &iter);

	if (!status && value.empty()) {
		status = kWPANTUNDStatus_PropertyEmpty;
	}

	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &status);

	if (value.empty()) {
		append_any_to_dbus_iter(&iter, std::string("<empty>"));
	} else {
		append_any_to_dbus_iter(&iter, value);
	}

	dbus_connection_send(mConnection, reply, NULL);
	dbus_message_unref(message);
	dbus_message_unref(reply);
}

DBusHandlerResult
DBusIPCAPI_v0::interface_join_handler(
	NCPControlInterface* interface,
	DBusMessage *        message
) {
	DBusHandlerResult ret = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	const char* network_name = NULL;
	int16_t node_type_int16 = ROUTER;
	ValueMap options;
	uint64_t xpanid;
	uint16_t panid = 0xFFFF;
	uint8_t channel = 0;

	dbus_message_get_args(
		message, NULL,
		DBUS_TYPE_STRING, &network_name,
		DBUS_TYPE_INT16, &node_type_int16,
		DBUS_TYPE_UINT64, &xpanid,
		DBUS_TYPE_UINT16, &panid,
		DBUS_TYPE_BYTE, &channel,
		DBUS_TYPE_INVALID
	);

	require(network_name != NULL, bail);

	options[kWPANTUNDProperty_NetworkName] = std::string(network_name);
	options[kWPANTUNDProperty_NetworkXPANID] = xpanid;
	options[kWPANTUNDProperty_NetworkPANID] = panid;
	options[kWPANTUNDProperty_NCPChannel] = channel;

	if (node_type_int16) {
		options[kWPANTUNDProperty_NetworkNodeType] = (int)node_type_int16;
	}

	dbus_message_ref(message);

	interface->join(
		options,
		boost::bind(
			&DBusIPCAPI_v0::CallbackWithStatus_Helper,
			this,
			_1,
			message
		)
	);

bail:

	ret = DBUS_HANDLER_RESULT_HANDLED;

	return ret;
}

DBusHandlerResult
DBusIPCAPI_v0::interface_form_handler(
	NCPControlInterface* interface,
	DBusMessage *        message
) {
	DBusHandlerResult ret = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	const char* network_name;
	ValueMap options;
	int16_t node_type_int16 = 0;
	NCPControlInterface::ChannelMask channel_mask = 0;
	uint8_t *ula_prefix = NULL;
	int ula_prefix_len = 0;

	dbus_message_get_args(
		message, NULL,
		DBUS_TYPE_STRING, &network_name,
		DBUS_TYPE_INT16, &node_type_int16,
		DBUS_TYPE_UINT32, &channel_mask,
		DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE, &ula_prefix, &ula_prefix_len,
		DBUS_TYPE_INVALID
	);

	if (node_type_int16) {
		options[kWPANTUNDProperty_NetworkNodeType] = (int)node_type_int16;
	}

	if (channel_mask) {
		options[kWPANTUNDProperty_NCPChannelMask] = channel_mask;
	}

	if (ula_prefix_len) {
		options[kWPANTUNDProperty_NestLabs_LegacyMeshLocalPrefix] = Data(ula_prefix, ula_prefix_len);
	}

	{	// The mesh local prefix can be set by setting it before forming.
		boost::any value;
		value = interface->property_get_value(kWPANTUNDProperty_IPv6MeshLocalPrefix);
		if (!value.empty()) {
			options[kWPANTUNDProperty_IPv6MeshLocalPrefix] = value;
		}
	}

	options[kWPANTUNDProperty_NetworkName] = std::string(network_name);

	dbus_message_ref(message);

	interface->form(
		options,
		boost::bind(
			&DBusIPCAPI_v0::CallbackWithStatus_Helper,
			this,
			_1,
			message
		)
	);

	ret = DBUS_HANDLER_RESULT_HANDLED;

	return ret;
}

DBusHandlerResult
DBusIPCAPI_v0::interface_begin_net_wake_handler(
	NCPControlInterface* interface,
	DBusMessage *        message
) {
	DBusHandlerResult ret = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	uint8_t data = 0;
	uint32_t flags = 0;

	dbus_message_ref(message);
	dbus_message_get_args(
		message, NULL,
		DBUS_TYPE_BYTE, &data,
		DBUS_TYPE_UINT32, &flags,
		DBUS_TYPE_INVALID
		);

	interface->begin_net_wake(
		data,
		flags,
		boost::bind(&DBusIPCAPI_v0::CallbackWithStatus_Helper, this, _1,
					message)
	);
	ret = DBUS_HANDLER_RESULT_HANDLED;

	return ret;
}

DBusHandlerResult
DBusIPCAPI_v0::interface_permit_join_handler(
	NCPControlInterface* interface,
	DBusMessage *        message
) {
	DBusHandlerResult ret = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	int32_t seconds = -1;
	dbus_bool_t network_wide = FALSE;
	uint8_t commissioning_traffic_type = 0xFF;
	in_port_t commissioning_traffic_port = 0;

	dbus_message_ref(message);
	dbus_message_get_args(
		message, NULL,
		DBUS_TYPE_INT32, &seconds,
		DBUS_TYPE_BOOLEAN, &network_wide,
		DBUS_TYPE_UINT16, &commissioning_traffic_port,
		DBUS_TYPE_BYTE, &commissioning_traffic_type,
		DBUS_TYPE_INVALID
		);
	commissioning_traffic_port = htons(commissioning_traffic_port);

	if(seconds==-1)
		seconds = 5*60;

	interface->permit_join(
		seconds,
		commissioning_traffic_type,
		commissioning_traffic_port,
		network_wide,
		boost::bind(&DBusIPCAPI_v0::CallbackWithStatus_Helper, this, _1,
					message)
		);
	ret = DBUS_HANDLER_RESULT_HANDLED;

	return ret;
}

DBusHandlerResult
DBusIPCAPI_v0::interface_leave_handler(
	NCPControlInterface* interface,
	DBusMessage *        message
) {
	DBusHandlerResult ret = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	dbus_message_ref(message);

	interface->leave(boost::bind(&DBusIPCAPI_v0::CallbackWithStatus_Helper,
								 this, _1, message));
	ret = DBUS_HANDLER_RESULT_HANDLED;

	return ret;
}

DBusHandlerResult
DBusIPCAPI_v0::interface_data_poll_handler(
	NCPControlInterface* interface,
	DBusMessage *        message
) {
	DBusHandlerResult ret = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	dbus_message_ref(message);

	interface->data_poll(boost::bind(&DBusIPCAPI_v0::CallbackWithStatus_Helper,
								 this, _1, message));
	ret = DBUS_HANDLER_RESULT_HANDLED;

	return ret;
}

DBusHandlerResult
DBusIPCAPI_v0::interface_config_gateway_handler(
	NCPControlInterface* interface,
	DBusMessage *        message
) {
	DBusHandlerResult ret = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	dbus_message_ref(message);

	dbus_bool_t defaultRoute = FALSE;
	dbus_bool_t preferred = TRUE;
	dbus_bool_t slaac = TRUE;
	dbus_bool_t onMesh = TRUE;
	int16_t priority_raw;
	NCPControlInterface::OnMeshPrefixPriority priority;
	uint32_t preferredLifetime = 0;
	uint32_t validLifetime = 0;
	uint8_t *prefix = NULL;
	struct in6_addr addr = {};
	int prefixLen = 0;

	dbus_message_ref(message);
	dbus_message_get_args(
		message, NULL,
		DBUS_TYPE_BOOLEAN, &defaultRoute,
		DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE, &prefix, &prefixLen,
		DBUS_TYPE_UINT32, &preferredLifetime,
		DBUS_TYPE_UINT32, &validLifetime,
		DBUS_TYPE_INVALID
	);

	if (prefixLen > 16) {
		prefixLen = 16;
	}

	memcpy(addr.s6_addr, prefix, prefixLen);

	priority = NCPControlInterface::PREFIX_MEDIUM_PREFERENCE;

	if (validLifetime == 0) {
		interface->remove_on_mesh_prefix(
			&addr,
			boost::bind(&DBusIPCAPI_v0::CallbackWithStatus_Helper,this, _1, message)
		);
	} else {
		interface->add_on_mesh_prefix(
			&addr,
			defaultRoute,
			preferred,
			slaac,
			onMesh,
			priority,
			boost::bind(&DBusIPCAPI_v0::CallbackWithStatus_Helper,this, _1, message)
		);
	}
	ret = DBUS_HANDLER_RESULT_HANDLED;

	return ret;
}

DBusHandlerResult
DBusIPCAPI_v0::interface_add_route_handler(
   NCPControlInterface* interface,
   DBusMessage *        message
) {
	DBusHandlerResult ret = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	dbus_message_ref(message);

	uint8_t *prefix = NULL;
	struct in6_addr addr = {};
	int prefix_len = 0;
	uint16_t domain_id = 0;
	int16_t priority_raw;
	NCPControlInterface::ExternalRoutePriority priority;

	dbus_message_ref(message);
	dbus_message_get_args(
		message, NULL,
		DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE, &prefix, &prefix_len,
		DBUS_TYPE_UINT16, &domain_id,
		DBUS_TYPE_INT16, &priority_raw,
		DBUS_TYPE_INVALID
	);

	if (prefix_len > 16) {
		prefix_len = 16;
	}

	memcpy(addr.s6_addr, prefix, prefix_len);

	priority = NCPControlInterface::ROUTE_MEDIUM_PREFERENCE;
	if  (priority_raw > 0) {
		priority = NCPControlInterface::ROUTE_HIGH_PREFERENCE;
	} else if (priority_raw < 0) {
		priority = NCPControlInterface::ROUTE_LOW_PREFRENCE;
	}

	interface->add_external_route(
		&addr,
		IPV6_PREFIX_BYTES_TO_BITS(prefix_len),
		domain_id,
		priority,
		boost::bind(&DBusIPCAPI_v0::CallbackWithStatus_Helper, this, _1, message)
	);

	ret = DBUS_HANDLER_RESULT_HANDLED;

	return ret;
}

DBusHandlerResult
DBusIPCAPI_v0::interface_remove_route_handler(
   NCPControlInterface* interface,
   DBusMessage *        message
) {
	DBusHandlerResult ret = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	dbus_message_ref(message);

	uint8_t *prefix = NULL;
	struct in6_addr addr = {};
	int prefix_len = 0;
	uint16_t domain_id = 0;

	dbus_message_ref(message);
	dbus_message_get_args(
		message, NULL,
		DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE, &prefix, &prefix_len,
		DBUS_TYPE_UINT16, &domain_id,
		DBUS_TYPE_INVALID
	);

	if (prefix_len > 16) {
		prefix_len = 16;
	}

	memcpy(addr.s6_addr, prefix, prefix_len);

	interface->remove_external_route(
		&addr,
		IPV6_PREFIX_BYTES_TO_BITS(prefix_len),
		domain_id,
		boost::bind(&DBusIPCAPI_v0::CallbackWithStatus_Helper, this, _1, message)
	);

	ret = DBUS_HANDLER_RESULT_HANDLED;

	return ret;
}

DBusHandlerResult
DBusIPCAPI_v0::interface_begin_low_power_handler(
	NCPControlInterface* interface,
	DBusMessage *        message
) {
	DBusHandlerResult ret = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	dbus_message_ref(message);

	interface->begin_low_power(boost::bind(&DBusIPCAPI_v0::CallbackWithStatus_Helper,
								 this, _1, message));
	ret = DBUS_HANDLER_RESULT_HANDLED;

	return ret;
}

DBusHandlerResult
DBusIPCAPI_v0::interface_ping_handler(
	NCPControlInterface* interface,
	DBusMessage *        message
) {
	DBusHandlerResult ret = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	dbus_message_ref(message);

	interface->refresh_state(boost::bind(&DBusIPCAPI_v0::CallbackWithStatus_Helper,
								 this, _1, message));
	ret = DBUS_HANDLER_RESULT_HANDLED;

	return ret;
}

DBusHandlerResult
DBusIPCAPI_v0::interface_host_did_wake_handler(
	NCPControlInterface* interface,
	DBusMessage *        message
) {
	DBusHandlerResult ret = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	dbus_message_ref(message);

	interface->host_did_wake(boost::bind(&DBusIPCAPI_v0::CallbackWithStatus_Helper,
								 this, _1, message));
	ret = DBUS_HANDLER_RESULT_HANDLED;

	return ret;
}

DBusHandlerResult
DBusIPCAPI_v0::interface_stop_scan_handler(
	NCPControlInterface* interface,
	DBusMessage *        message
) {
	DBusHandlerResult ret = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	dbus_message_ref(message);

	interface->netscan_stop(boost::bind(&DBusIPCAPI_v0::CallbackWithStatus_Helper,
									 this, _1, message));
	ret = DBUS_HANDLER_RESULT_HANDLED;

	return ret;
}

DBusHandlerResult
DBusIPCAPI_v0::interface_get_prop_handler(
	NCPControlInterface* interface,
	DBusMessage *        message
) {
	DBusHandlerResult ret = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	const char* property_key_cstr = "";
	std::string property_key;
	dbus_message_get_args(
		message, NULL,
		DBUS_TYPE_STRING, &property_key_cstr,
		DBUS_TYPE_INVALID
		);

	property_key = property_key_cstr;

	if (interface->translate_deprecated_property(property_key)) {
		syslog(LOG_WARNING, "GetProp: Property \"%s\" is deprecated. Please use \"%s\" instead.", property_key_cstr, property_key.c_str());
	}

	dbus_message_ref(message);
	interface->property_get_value(property_key,
							boost::bind(&DBusIPCAPI_v0::CallbackWithStatusArg1_Helper, this,
										_1, _2, message));

	ret = DBUS_HANDLER_RESULT_HANDLED;

	return ret;
}

DBusHandlerResult
DBusIPCAPI_v0::interface_set_prop_handler(
	NCPControlInterface* interface,
	DBusMessage *        message
) {
	DBusHandlerResult ret = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	DBusMessageIter iter;
	const char* property_key_cstr = "";
	std::string property_key;
	boost::any property_value;

	dbus_message_iter_init(message, &iter);

	require (dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_STRING, bail);

	dbus_message_iter_get_basic(&iter, &property_key_cstr);
	dbus_message_iter_next(&iter);

	property_value = any_from_dbus_iter(&iter);
	property_key = property_key_cstr;

	if (interface->translate_deprecated_property(property_key, property_value)) {
		syslog(LOG_WARNING, "SetProp: Property \"%s\" is deprecated. Please use \"%s\" instead.", property_key_cstr, property_key.c_str());
	}

	dbus_message_ref(message);
	interface->property_set_value(
		property_key,
		property_value,
		boost::bind(&DBusIPCAPI_v0::CallbackWithStatus_Helper, this, _1,
					message)
		);
	ret = DBUS_HANDLER_RESULT_HANDLED;

bail:
	return ret;
}

DBusHandlerResult
DBusIPCAPI_v0::interface_insert_prop_handler(
	NCPControlInterface* interface,
	DBusMessage *        message
) {
	DBusHandlerResult ret = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	DBusMessageIter iter;
	const char* property_key_cstr = "";
	std::string property_key;
	boost::any property_value;

	dbus_message_iter_init(message, &iter);

	require (dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_STRING, bail);

	dbus_message_iter_get_basic(&iter, &property_key_cstr);
	dbus_message_iter_next(&iter);

	property_value = any_from_dbus_iter(&iter);
	property_key = property_key_cstr;

	if (interface->translate_deprecated_property(property_key, property_value)) {
		syslog(LOG_WARNING, "InsertProp: Property \"%s\" is deprecated. Please use \"%s\" instead.", property_key_cstr, property_key.c_str());
	}

	dbus_message_ref(message);
	interface->property_insert_value(
		property_key,
		property_value,
		boost::bind(&DBusIPCAPI_v0::CallbackWithStatus_Helper, this, _1,
					message)
		);
	ret = DBUS_HANDLER_RESULT_HANDLED;

bail:
	return ret;
}

DBusHandlerResult
DBusIPCAPI_v0::interface_remove_prop_handler(
	NCPControlInterface* interface,
	DBusMessage *        message
) {
	DBusHandlerResult ret = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	DBusMessageIter iter;
	const char* property_key_cstr = "";
	std::string property_key;
	boost::any property_value;

	dbus_message_iter_init(message, &iter);

	require (dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_STRING, bail);

	dbus_message_iter_get_basic(&iter, &property_key_cstr);
	dbus_message_iter_next(&iter);

	property_value = any_from_dbus_iter(&iter);
	property_key = property_key_cstr;

	if (interface->translate_deprecated_property(property_key, property_value)) {
		syslog(LOG_WARNING, "RemoveProp: Property \"%s\" is deprecated. Please use \"%s\" instead.", property_key_cstr, property_key.c_str());
	}

	dbus_message_ref(message);
	interface->property_remove_value(
		property_key,
		property_value,
		boost::bind(&DBusIPCAPI_v0::CallbackWithStatus_Helper, this, _1,
					message)
		);
	ret = DBUS_HANDLER_RESULT_HANDLED;

bail:
	return ret;
}

DBusHandlerResult
DBusIPCAPI_v0::interface_reset_handler(
	NCPControlInterface* interface,
	DBusMessage *        message
) {
	DBusHandlerResult ret = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	dbus_message_ref(message);

	interface->reset(boost::bind(&DBusIPCAPI_v0::CallbackWithStatus_Helper,
								 this, _1, message));
	ret = DBUS_HANDLER_RESULT_HANDLED;

	return ret;
}

DBusHandlerResult
DBusIPCAPI_v0::interface_status_handler(
	NCPControlInterface* interface,
	DBusMessage *        message
) {
	DBusHandlerResult ret = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	dbus_message_ref(message);

	NCPState ncp_state = UNINITIALIZED;
	boost::any value(interface->property_get_value(kWPANTUNDProperty_NCPState));

	if (!value.empty()) {
		ncp_state = string_to_ncp_state(any_to_string(value));
	}

	if (ncp_state_is_sleeping(ncp_state)
	 || ncp_state_is_detached_from_ncp(ncp_state)
	 || (ncp_state == UNINITIALIZED)
	) {
		status_response_helper(0, interface, message);
	} else {
		interface->refresh_state(boost::bind(&DBusIPCAPI_v0::
										 status_response_helper, this, _1, interface, message));
	}
	ret = DBUS_HANDLER_RESULT_HANDLED;

	return ret;
}

DBusHandlerResult
DBusIPCAPI_v0::interface_active_scan_handler(
	NCPControlInterface* interface,
	DBusMessage *        message
) {
	DBusHandlerResult ret = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	int32_t period = 0;
	ValueMap options;
	NCPControlInterface::ChannelMask channel_mask = 0;

	dbus_message_ref(message);

	dbus_message_get_args(
		message, NULL,
		DBUS_TYPE_INT32, &period,
		DBUS_TYPE_UINT32, &channel_mask,
		DBUS_TYPE_INVALID
	);

	if (channel_mask) {
		options[kWPANTUNDProperty_NCPChannelMask] = channel_mask;
	}

	if (period) {
		// Ignoring period for now
	}

	mReceivedBeacons.clear();

	interface->netscan_start(
		options,
		boost::bind(
			&DBusIPCAPI_v0::scan_response_helper,
			this,
			_1,
			message
		)
	);
	ret = DBUS_HANDLER_RESULT_HANDLED;

	return ret;
}

DBusHandlerResult
DBusIPCAPI_v0::interface_resume_handler(
	NCPControlInterface* interface,
	DBusMessage *        message
) {
	DBusHandlerResult ret = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	dbus_message_ref(message);

	interface->attach(boost::bind(&DBusIPCAPI_v0::CallbackWithStatus_Helper,
								  this, _1, message));
	ret = DBUS_HANDLER_RESULT_HANDLED;

	return ret;
}

DBusHandlerResult
DBusIPCAPI_v0::interface_mfg_finish_handler(
	NCPControlInterface* interface,
	DBusMessage *        message
) {
	DBusHandlerResult ret = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	NCPMfgInterface_v0* mfg_interface(dynamic_cast<NCPMfgInterface_v0*>(interface));

	if (mfg_interface) {
		dbus_message_ref(message);

		mfg_interface->mfg_finish(
			boost::bind(
				&DBusIPCAPI_v0::CallbackWithStatusArg1_Helper,
				this,
				_1,
				_2,
				message
			)
		);

		ret = DBUS_HANDLER_RESULT_HANDLED;
	}

	return ret;
}

DBusHandlerResult
DBusIPCAPI_v0::interface_mfg_begin_test_handler(
	NCPControlInterface* interface,
	DBusMessage *        message
) {
	DBusHandlerResult ret = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	NCPMfgInterface_v0* mfg_interface(dynamic_cast<NCPMfgInterface_v0*>(interface));

	if (mfg_interface) {
		dbus_message_ref(message);

		int16_t test_type = 0;

		dbus_message_get_args(
			message, NULL,
			DBUS_TYPE_INT16, &test_type
		);

		mfg_interface->mfg_begin_test(
			test_type,
			boost::bind(
				&DBusIPCAPI_v0::CallbackWithStatus_Helper,
				this,
				_1,
				message
			)
		);

		ret = DBUS_HANDLER_RESULT_HANDLED;
	}

	return ret;
}

DBusHandlerResult
DBusIPCAPI_v0::interface_mfg_end_test_handler(
	NCPControlInterface* interface,
	DBusMessage *        message
) {
	DBusHandlerResult ret = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	int16_t test_type = 0;
	NCPMfgInterface_v0* mfg_interface(dynamic_cast<NCPMfgInterface_v0*>(interface));

	if (mfg_interface) {
		dbus_message_ref(message);

		dbus_message_get_args(
			message, NULL,
			DBUS_TYPE_INT16, &test_type
		);

		mfg_interface->mfg_end_test(test_type, boost::bind(&DBusIPCAPI_v0::CallbackWithStatus_Helper,this, _1, message));

		ret = DBUS_HANDLER_RESULT_HANDLED;
	}

	return ret;
}

DBusHandlerResult
DBusIPCAPI_v0::interface_mfg_tx_packet_handler(
	NCPControlInterface* interface,
	DBusMessage *        message
) {
	DBusHandlerResult ret = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	const uint8_t *packet_data = NULL;
	int packet_length = 0;
	int16_t repeat = 1;
	NCPMfgInterface_v0* mfg_interface(dynamic_cast<NCPMfgInterface_v0*>(interface));

	if (mfg_interface) {
		dbus_message_ref(message);

		dbus_message_get_args(
			message, NULL,
			DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE, &packet_data, &packet_length,
			DBUS_TYPE_INT16, &repeat,
			DBUS_TYPE_INVALID
		);

		mfg_interface->mfg_tx_packet(
			Data(packet_data, packet_length),
			repeat,
			boost::bind(&DBusIPCAPI_v0::CallbackWithStatus_Helper,this, _1, message)
		);

		ret = DBUS_HANDLER_RESULT_HANDLED;
	}

	return ret;
}

DBusHandlerResult
DBusIPCAPI_v0::message_handler(
    NCPControlInterface* interface,
    DBusConnection *        connection,
    DBusMessage *           message
    )
{
	if ((dbus_message_get_type(message) == DBUS_MESSAGE_TYPE_METHOD_CALL)
		&& dbus_message_has_interface(message, WPAN_TUNNEL_DBUS_INTERFACE)
		&& mInterfaceCallbackTable.count(dbus_message_get_member(message))
	) {
		return mInterfaceCallbackTable[dbus_message_get_member(message)](
			interface,
			message
		);
	}

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

DBusHandlerResult
DBusIPCAPI_v0::interface_mfg_clockmon_handler(
	NCPControlInterface* interface,
	DBusMessage *        message
) {
	DBusHandlerResult ret = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	NCPMfgInterface_v0* mfg_interface(dynamic_cast<NCPMfgInterface_v0*>(interface));

	if (mfg_interface) {
		dbus_message_ref(message);

		dbus_bool_t enabled;
		uint32_t timerId;

		dbus_message_get_args(
			message, NULL,
			DBUS_TYPE_BOOLEAN, &enabled,
			DBUS_TYPE_UINT32, &timerId,
			DBUS_TYPE_INVALID
		);

		mfg_interface->mfg_clockmon(
			static_cast<bool>(enabled),
			timerId,
			boost::bind(
				&DBusIPCAPI_v0::CallbackWithStatus_Helper,
				this,
				_1,
				message
			)
		);

		ret = DBUS_HANDLER_RESULT_HANDLED;
	}

	return ret;
}

DBusHandlerResult
DBusIPCAPI_v0::interface_mfg_gpio_set_handler(
	NCPControlInterface* interface,
	DBusMessage *        message
) {
	DBusHandlerResult ret = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	NCPMfgInterface_v0* mfg_interface(dynamic_cast<NCPMfgInterface_v0*>(interface));

	if (mfg_interface) {
		dbus_message_ref(message);

		uint8_t port_pin;
		uint8_t config;
		uint8_t value;

		dbus_message_get_args(
			message, NULL,
			DBUS_TYPE_BYTE, &port_pin,
			DBUS_TYPE_BYTE, &config,
			DBUS_TYPE_BYTE, &value,
			DBUS_TYPE_INVALID
		);

		mfg_interface->mfg_gpio_set(
			port_pin,
			config,
			value,
			boost::bind(
				&DBusIPCAPI_v0::CallbackWithStatus_Helper,
				this,
				_1,
				message
			)
		);

		ret = DBUS_HANDLER_RESULT_HANDLED;
	}

	return ret;
}

DBusHandlerResult
DBusIPCAPI_v0::interface_mfg_gpio_get_handler(
	NCPControlInterface* interface,
	DBusMessage *        message
) {
	DBusHandlerResult ret = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	NCPMfgInterface_v0* mfg_interface(dynamic_cast<NCPMfgInterface_v0*>(interface));

	if (mfg_interface) {
		dbus_message_ref(message);

		uint8_t port_pin;

		dbus_message_get_args(
			message, NULL,
			DBUS_TYPE_BYTE, &port_pin,
			DBUS_TYPE_INVALID
		);

		mfg_interface->mfg_gpio_get(
			port_pin,
			boost::bind(
				&DBusIPCAPI_v0::CallbackWithStatusArg1_Helper,
				this,
				_1,
				_2,
				message
			)
		);

		ret = DBUS_HANDLER_RESULT_HANDLED;
	}

	return ret;
}


DBusHandlerResult
DBusIPCAPI_v0::interface_mfg_channelcal_handler(
	NCPControlInterface* interface,
	DBusMessage *        message
) {
	DBusHandlerResult ret = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	NCPMfgInterface_v0* mfg_interface(dynamic_cast<NCPMfgInterface_v0*>(interface));

	if (mfg_interface) {
		dbus_message_ref(message);

		uint8_t channel;
		uint32_t duration;

		dbus_message_get_args(
			message, NULL,
			DBUS_TYPE_BYTE, &channel,
			DBUS_TYPE_UINT32, &duration,
			DBUS_TYPE_INVALID
		);

		mfg_interface->mfg_channelcal(
			channel,
			duration,
			boost::bind(
				&DBusIPCAPI_v0::CallbackWithStatus_Helper,
				this,
				_1,
				message
			)
		);

		ret = DBUS_HANDLER_RESULT_HANDLED;
	}

	return ret;
}

DBusHandlerResult
DBusIPCAPI_v0::interface_mfg_channelcal_get_handler(
	NCPControlInterface* interface,
	DBusMessage *        message
) {
	DBusHandlerResult ret = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	NCPMfgInterface_v0* mfg_interface(dynamic_cast<NCPMfgInterface_v0*>(interface));

	if (mfg_interface) {
		dbus_message_ref(message);

		uint8_t channel;

		dbus_message_get_args(
			message, NULL,
			DBUS_TYPE_BYTE, &channel,
			DBUS_TYPE_INVALID
		);

		mfg_interface->mfg_channelcal_get(
			channel,
			boost::bind(
				&DBusIPCAPI_v0::CallbackWithStatusArg1_Helper,
				this,
				_1,
				_2,
				message
			)
		);

		ret = DBUS_HANDLER_RESULT_HANDLED;
	}

	return ret;
}

void
DBusIPCAPI_v0::ncp_state_changed(NCPControlInterface* interface)
{
	DBusMessage* signal;
	boost::any value;
	NCPState ncp_state = UNINITIALIZED;
	std::string ncp_state_str;
	const char* ncp_state_cstr = kWPANTUNDStateUninitialized;

	value = interface->property_get_value(kWPANTUNDProperty_NCPState);

	if (!value.empty()) {
		ncp_state_str = any_to_string(value);
		ncp_state = string_to_ncp_state(ncp_state_str);
		ncp_state_cstr = ncp_state_str.c_str();
	}

	DBusMessageIter iter;

	syslog(LOG_DEBUG,
	       "DBus Sending Association State Changed to %s",
	       ncp_state_cstr);
	signal = dbus_message_new_signal(
	    (std::string(WPAN_TUNNEL_DBUS_PATH) + "/" + interface->get_name()).c_str(),
	    WPAN_TUNNEL_DBUS_INTERFACE,
	    WPAN_IFACE_SIGNAL_STATE_CHANGED
	    );
	dbus_message_append_args(
	    signal,
	    DBUS_TYPE_STRING, &ncp_state_cstr,
	    DBUS_TYPE_INVALID
	    );

	dbus_message_iter_init_append(signal, &iter);

	DBusMessageIter dict;

	dbus_message_iter_open_container(
	    &iter,
	    DBUS_TYPE_ARRAY,
	    DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
	    DBUS_TYPE_STRING_AS_STRING
	    DBUS_TYPE_VARIANT_AS_STRING
	    DBUS_DICT_ENTRY_END_CHAR_AS_STRING,
	    &dict
	    );

	append_dict_entry(&dict, kWPANTUNDProperty_DaemonEnabled, interface->property_get_value(kWPANTUNDProperty_DaemonEnabled));

	if (ncp_state_is_commissioned(ncp_state)) {
		ipc_append_network_properties(&dict,
	                              interface->get_current_network_instance());
		boost::any prefix = interface->property_get_value(kWPANTUNDProperty_IPv6MeshLocalPrefix);
		if (!prefix.empty()) {
			append_dict_entry(&dict, kWPANTUNDProperty_IPv6MeshLocalPrefix, prefix);
		}

		prefix = interface->property_get_value(kWPANTUNDProperty_NestLabs_LegacyMeshLocalAddress);
		if (!prefix.empty()) {
			append_dict_entry(&dict, kWPANTUNDProperty_NestLabs_LegacyMeshLocalAddress, prefix);
		}

		append_dict_entry(&dict, kWPANTUNDProperty_NetworkNodeType, interface->property_get_value(kWPANTUNDProperty_NetworkNodeType));

		if (ncp_state >= ASSOCIATED) {
			boost::any networkKey = interface->property_get_value(kWPANTUNDProperty_NetworkKey);
			if (!networkKey.empty()) {
				append_dict_entry(&dict, kWPANTUNDProperty_NetworkKey, networkKey);
			}
		}
	}

	dbus_message_iter_close_container(&iter, &dict);

	dbus_connection_send(mConnection, signal, NULL);
	dbus_message_unref(signal);
}


static void
ObjectPathUnregisterFunction_cb(DBusConnection *connection, void *user_data)
{
	delete (std::pair<NCPControlInterface*, DBusIPCAPI_v0*> *)user_data;
}

int
DBusIPCAPI_v0::add_interface(NCPControlInterface* interface)
{
	static const DBusObjectPathVTable ipc_interface_vtable = {
		&ObjectPathUnregisterFunction_cb,
		&DBusIPCAPI_v0::dbus_message_handler,
	};

	std::string name = interface->get_name();
	std::string path = std::string(WPAN_TUNNEL_DBUS_PATH) + "/" + name;

	std::pair<NCPControlInterface*, DBusIPCAPI_v0*> *cb_data =
	    new std::pair<NCPControlInterface*, DBusIPCAPI_v0*>(interface, this);

	NCPMfgInterface_v0* mfg_interface(dynamic_cast<NCPMfgInterface_v0*>(interface));

	require(dbus_connection_register_object_path(
	            mConnection,
	            path.c_str(),
	            &ipc_interface_vtable,
	            (void*)cb_data
	            ), bail);

	if (mfg_interface) {
		mfg_interface->mOnMfgRXPacket.connect(
			boost::bind(
				&DBusIPCAPI_v0::mfg_rx_packet,
				this,
				interface,
				_1,
				_2,
				_3
			)
		);
	}

	interface->mOnPropertyChanged.connect(
	    boost::bind(
			&DBusIPCAPI_v0::property_changed,
			this,
			interface,
			_1,
			_2
		)
	);

	interface->mOnNetScanBeacon.connect(
	    boost::bind(
			&DBusIPCAPI_v0::received_beacon,
			this,
			interface,
			_1
		)
	);

bail:
	return 0;
}

void
DBusIPCAPI_v0::property_changed(NCPControlInterface* interface,const std::string& key, const boost::any& value)
{
	DBusMessage* signal;
	DBusMessageIter iter;
	std::string key_as_path;
	std::string path;

	// Transform the key into a DBus-compatible path
	for (std::string::const_iterator i = key.begin();
		i != key.end();
		++i
	) {
		const char c = *i;
		if (isalnum(c) || (c == '_')) {
			key_as_path += c;
		} else if (c == ':') {
			key_as_path += '/';
		} else if (c == '.') {
			key_as_path += '_';
		}
	}

	if (key == kWPANTUNDProperty_NestLabs_NetworkWakeRemaining) {
		uint8_t data = static_cast<uint8_t>(any_to_int(interface->property_get_value(kWPANTUNDProperty_NestLabs_NetworkWakeData)));
		net_wake_event(interface, data, any_to_int(value));
	} else if (key == kWPANTUNDProperty_DaemonReadyForHostSleep) {
		if (any_to_bool(value)) {
			allow_sleep(interface);
		} else {
			prevent_sleep(interface);
		}
	} else if (key == kWPANTUNDProperty_NCPState) {
		ncp_state_changed(interface);
	} else if (key == kWPANTUNDProperty_NetworkNodeType) {
		property_changed(interface, "NCPNodeType", value);
	}

	signal = dbus_message_new_signal(
		(
			std::string(WPAN_TUNNEL_DBUS_PATH) + "/" + interface->get_name()
			+ "/" + WPAN_TUNNEL_DBUS_PATH_PROPERTIES + "/" + key_as_path
		).c_str(),
		WPAN_TUNNEL_DBUS_INTERFACE,
		WPAN_IFACE_SIGNAL_PROPERTY_CHANGED
	);

	dbus_message_iter_init_append(signal, &iter);

	append_any_to_dbus_iter(&iter, key);
	append_any_to_dbus_iter(&iter, value);

	dbus_connection_send(mConnection, signal, NULL);
	dbus_message_unref(signal);
}

void
DBusIPCAPI_v0::net_wake_event(NCPControlInterface* interface,uint8_t data, cms_t ms_remaining)
{
	DBusMessage* signal;

	signal = dbus_message_new_signal(
	    (std::string(WPAN_TUNNEL_DBUS_PATH) + "/" + interface->get_name()).c_str(),
	    WPAN_TUNNEL_DBUS_INTERFACE,
	    WPAN_IFACE_SIGNAL_NET_WAKE
	    );
	dbus_message_append_args(
	    signal,
	    DBUS_TYPE_BYTE, &data,
	    DBUS_TYPE_INT32, &ms_remaining,
	    DBUS_TYPE_INVALID
	);

	dbus_connection_send(mConnection, signal, NULL);
	dbus_message_unref(signal);
}

void
DBusIPCAPI_v0::mfg_rx_packet(NCPControlInterface* interface, Data packet, uint8_t lqi, int8_t rssi)
{
	DBusMessageIter iter;
	DBusMessage* signal;

	signal = dbus_message_new_signal(
	    (std::string(WPAN_TUNNEL_DBUS_PATH) + "/" + interface->get_name()).c_str(),
	    WPAN_TUNNEL_DBUS_INTERFACE,
	    WPAN_IFACE_SIGNAL_MFG_RX
	    );

	dbus_message_iter_init_append(signal, &iter);

	append_any_to_dbus_iter(&iter, packet);
	append_any_to_dbus_iter(&iter, lqi);
	append_any_to_dbus_iter(&iter, rssi);

	dbus_connection_send(mConnection, signal, NULL);
	dbus_message_unref(signal);
}

void
DBusIPCAPI_v0::prevent_sleep(NCPControlInterface* interface)
{
	DBusMessage* signal;

	signal = dbus_message_new_signal(
	    (std::string(WPAN_TUNNEL_DBUS_PATH) + "/" + interface->get_name()).c_str(),
	    WPAN_TUNNEL_DBUS_INTERFACE,
	    WPAN_IFACE_SIGNAL_PREVENT_SLEEP
	);
	dbus_connection_send(mConnection, signal, NULL);
	dbus_message_unref(signal);
}

void
DBusIPCAPI_v0::allow_sleep(NCPControlInterface* interface)
{
	DBusMessage* signal;

	signal = dbus_message_new_signal(
	    (std::string(WPAN_TUNNEL_DBUS_PATH) + "/" + interface->get_name()).c_str(),
	    WPAN_TUNNEL_DBUS_INTERFACE,
	    WPAN_IFACE_SIGNAL_ALLOW_SLEEP
	);
	dbus_connection_send(mConnection, signal, NULL);
	dbus_message_unref(signal);
}


DBusHandlerResult
DBusIPCAPI_v0::dbus_message_handler(
    DBusConnection *connection,
    DBusMessage *   message,
    void *                  user_data
    )
{
	if (dbus_message_get_type(message) != DBUS_MESSAGE_TYPE_SIGNAL) {
		syslog(LOG_INFO, "Inbound DBus message for INTERFACE \"%s\" from \"%s\"", dbus_message_get_member(message), dbus_message_get_sender(message));
	}
	std::pair<NCPControlInterface*,
	          DBusIPCAPI_v0*> *cb_data =
	    (std::pair<NCPControlInterface*, DBusIPCAPI_v0*> *)user_data;
	return cb_data->second->message_handler(cb_data->first,
	                                                  connection,
	                                                  message);
}
