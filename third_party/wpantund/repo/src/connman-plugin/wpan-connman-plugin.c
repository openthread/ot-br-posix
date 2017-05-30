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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define ASSERT_MACROS_USE_SYSLOG 1
#include "assert-macros.h"
#include <errno.h>
#include <net/if.h>
#include <arpa/inet.h>

#define _GNU_SOURCE 1
#include <stdio.h>

#ifndef IFF_LOWER_UP
#define IFF_LOWER_UP    0x10000
#endif

#define CONNMAN_API_SUBJECT_TO_CHANGE
#include <connman/technology.h>
#include <connman/plugin.h>
#include <connman/device.h>
#include <connman/inet.h>
#include <connman/ipaddress.h>
#include <connman/log.h>
#include <connman/dbus.h>
#include <syslog.h>
#include <dbus/dbus.h>
#include <stdlib.h>
#include <glib.h>
#include <string.h>
#include <stdio.h>
#include <syslog.h>
#include "wpan-dbus-v0.h"
#include "wpan-properties.h"
#include "wpan-error.h"
#include <stdbool.h>
#include "string-utils.h"

static struct connman_technology *lowpan_tech;
static DBusConnection *connection;

//#define DEBUG 1

#define LOWPAN_AUTH_KEY     "WiFi.Passphrase"
#define LOWPAN_SECURITY_KEY     "WiFi.Security"
#define LOWPAN_PERMIT_JOINING_KEY   "LoWPAN.PermitJoining"
#define LOWPAN_PARENT_ADDRESS_KEY   "LoWPAN.ParentAddress"

#if DEBUG
#undef DBG
#define DBG(fmt, arg ...) \
	connman_debug("%s:%d:%s() " fmt, \
	              "wpan-connman-plugin.c", __LINE__, __FUNCTION__, ## arg);
#endif

static GHashTable* devices;
static GHashTable* networks;

// Simplified state representation
typedef enum {
	NCP_STATE_UNINITIALIZED,
	NCP_STATE_UPGRADING,
	NCP_STATE_OFFLINE,
	NCP_STATE_COMMISSIONED,
	NCP_STATE_ASSOCIATING,
	NCP_STATE_CREDENTIALS_NEEDED,
	NCP_STATE_ASSOCIATED,
	NCP_STATE_NET_WAKE_ASLEEP,
} ncp_state_t;

struct wpan_network_info_s {
	char network_name[17];
	dbus_bool_t allowing_join;
	uint16_t pan_id;
	int16_t channel;
	uint64_t xpanid;
	int8_t rssi;
	uint8_t hwaddr[8];
	uint8_t prefix[8];
};

struct lowpan_device_s {
	struct wpan_network_info_s current_network_info;
	struct connman_network*         current_network;
	uint8_t hwaddr[8];
	ncp_state_t ncp_state;
};

struct lowpan_network_s {
	struct wpan_network_info_s network_info;
	int16_t node_type;
};

/* -------------------------------------------------------------------------- */
// MARK: - Other Helpers


static bool
ncp_state_is_initializing(ncp_state_t ncp_state)
{
	switch (ncp_state) {
	case NCP_STATE_UNINITIALIZED:
	case NCP_STATE_UPGRADING:
		return true;
		break;

	default:
		break;
	}
	return false;
}

static bool
ncp_state_is_not_associated(ncp_state_t ncp_state)
{
	switch(ncp_state) {
	case NCP_STATE_OFFLINE:
	case NCP_STATE_UPGRADING:
	case NCP_STATE_UNINITIALIZED:
		return true;

	default:
		break;
	}
	return false;
}


static bool
ncp_state_is_has_joined(ncp_state_t ncp_state)
{
	switch(ncp_state) {
	case NCP_STATE_ASSOCIATED:
	case NCP_STATE_NET_WAKE_ASLEEP:
		return true;

	default:
		break;
	}
	return false;
}


static ncp_state_t
string_to_ncp_state(const char* new_state, ncp_state_t ncp_state) {
	DBG("string_to_ncp_state: %s",new_state);

	if(new_state == NULL) {
		DBG("Bad association state");
	} else if (0 == strcmp(new_state, kWPANTUNDStateFault)) {
		ncp_state = NCP_STATE_UNINITIALIZED;

	} else if (0 == strcmp(new_state, kWPANTUNDStateUpgrading)) {
		ncp_state = NCP_STATE_UPGRADING;

	} else if (0 == strcmp(new_state, kWPANTUNDStateCommissioned)) {
		ncp_state = NCP_STATE_COMMISSIONED;

	} else if (0 == strcmp(new_state, kWPANTUNDStateCredentialsNeeded)) {
		ncp_state = NCP_STATE_CREDENTIALS_NEEDED;

	} else if (0 == strcmp(new_state, kWPANTUNDStateNetWake_Asleep)) {
		ncp_state = NCP_STATE_NET_WAKE_ASLEEP;

	} else if (0 == strcmp(new_state, kWPANTUNDStateIsolated)) {
		ncp_state = NCP_STATE_ASSOCIATING;

	} else if (0 == strncmp(new_state, kWPANTUNDStateOffline, sizeof(kWPANTUNDStateOffline)-1)) {
		ncp_state = NCP_STATE_OFFLINE;

	} else if (0 == strncmp(new_state, kWPANTUNDStateAssociating, sizeof(kWPANTUNDStateAssociating)-1)) {
		ncp_state = NCP_STATE_ASSOCIATING;

	} else if (0 == strncmp(new_state, kWPANTUNDStateAssociated, sizeof(kWPANTUNDStateAssociated)-1)) {
		ncp_state = NCP_STATE_ASSOCIATED;

	} else if (0 == strncmp(new_state, kWPANTUNDStateUninitialized, sizeof(kWPANTUNDStateUninitialized)-1)) {
		ncp_state = NCP_STATE_UNINITIALIZED;
	}
	return ncp_state;
}

static void
parse_prefix_string(const char *prefix_cstr, uint8_t *prefix)
{
	uint8_t bytes[16];
	char str[INET6_ADDRSTRLEN + 10];
	char *p;

	memset(prefix, 0, 8);
	memset(bytes, 0, sizeof(bytes));

	// Create a copy of the prefix string so we can modify it.
	strcpy(str, prefix_cstr);

	// Search for "/64" and remove it from the str.
	p = strchr(str, '/');
	if (p != NULL) {
		*p = 0;
	}

	// Parse the str as an ipv6 address.
	if (inet_pton(AF_INET6, str, bytes) > 0)
	{
		// If successful, copy the first 8 bytes.
		memcpy(prefix, bytes, 8);
	}
}

static int
encode_data_into_b16_string(
    const uint8_t*  buffer,
    size_t len,
    char*                   c_str,
    size_t c_str_max_len,
    int pad_to
    )
{
	int ret = 0;

	while (len && (c_str_max_len > 2)) {
		uint8_t byte = *buffer++;
		len--;
		pad_to--;
		*c_str++ = int_to_hex_digit(byte >> 4);
		*c_str++ = int_to_hex_digit(byte & 0xF);
		c_str_max_len -= 2;
		ret += 2;
	}

	while (pad_to > 0 && (c_str_max_len > 2)) {
		pad_to--;
		*c_str++ = '0';
		*c_str++ = '0';
		c_str_max_len -= 2;
		ret += 2;
	}

	*c_str++ = 0;
	return ret;
}

static int
encode_ipv6_address_from_prefx_and_hwaddr(
    char*                   c_str,
    size_t c_str_max_len,
    const uint8_t prefix[8],
    const uint8_t hwaddr[8]
    )
{
	return snprintf(c_str, c_str_max_len,
	                "%02X%02X:%02X%02X:%02X%02X:%02X%02X:"
	                "%02X%02X:%02X%02X:%02X%02X:%02X%02X",
	                prefix[0], prefix[1], prefix[2], prefix[3],
	                prefix[4], prefix[5], prefix[6], prefix[7],
	                hwaddr[0] ^ 0x02, hwaddr[1], hwaddr[2], hwaddr[3],
	                hwaddr[4], hwaddr[5], hwaddr[6], hwaddr[7]
	                );
}

/* -------------------------------------------------------------------------- */
// MARK: - Static Declarations

static void lowpan_device_leave(struct connman_device *device);
static int lowpan_driver_setprop_data(struct connman_device *device, const char* key, const uint8_t* data, size_t size);
static int lowpan_driver_getprop_data(struct connman_device *device, const char* key, void(*callback)(void* context, int error, const uint8_t* data, size_t len), void* context);

static void lowpan_device_set_network(struct connman_device *device, struct connman_network *network);

static int lowpan_device_update_status(struct connman_device *device);


/* -------------------------------------------------------------------------- */
// MARK: - DBus Helpers

uint8_t
CalculateStrengthFromRSSI(int8_t rssi)
{
	uint8_t ret = 0;

	if (rssi > -120) {
		ret = 120 + rssi;
	}

	if (ret > 100) {
		ret = 100;
	}
	return ret;
}

static void
dump_info_from_iter(
    FILE* file, DBusMessageIter *iter, int indent, bool bare
    )
{
	DBusMessageIter sub_iter;
	int i;

	if (!bare) for (i = 0; i < indent; i++) fprintf(file, "\t");

	switch (dbus_message_iter_get_arg_type(iter)) {
	case DBUS_TYPE_DICT_ENTRY:
		dbus_message_iter_recurse(iter, &sub_iter);
		dump_info_from_iter(file, &sub_iter, indent + 1, true);
		fprintf(file, " => ");
		dbus_message_iter_next(&sub_iter);
		dump_info_from_iter(file, &sub_iter, indent + 1, true);
		break;
	case DBUS_TYPE_ARRAY:
		dbus_message_iter_recurse(iter, &sub_iter);
		if (dbus_message_iter_get_arg_type(&sub_iter) == DBUS_TYPE_BYTE ||
		    dbus_message_iter_get_arg_type(&sub_iter) == DBUS_TYPE_INVALID) {
			fprintf(file, "[");
			indent = 0;
		} else {
			fprintf(file, "[\n");
		}

		for (;
		     dbus_message_iter_get_arg_type(&sub_iter) != DBUS_TYPE_INVALID;
		     dbus_message_iter_next(&sub_iter)) {
			dump_info_from_iter(file,
			                    &sub_iter,
			                    indent + 1,
			                    dbus_message_iter_get_arg_type(&sub_iter) == DBUS_TYPE_BYTE);
		}
		for (i = 0; i < indent; i++) fprintf(file, "\t");
		fprintf(file, "]");

		break;
	case DBUS_TYPE_VARIANT:
		dbus_message_iter_recurse(iter, &sub_iter);
		dump_info_from_iter(file, &sub_iter, indent, true);
		break;
	case DBUS_TYPE_STRING:
	{
		const char* string;
		dbus_message_iter_get_basic(iter, &string);
		fprintf(file, "\"%s\"", string);
	}
	break;

	case DBUS_TYPE_BYTE:
	{
		uint8_t v;
		dbus_message_iter_get_basic(iter, &v);
		fprintf(file, "%02X", v);
	}
	break;
	case DBUS_TYPE_UINT16:
	{
		uint16_t v;
		dbus_message_iter_get_basic(iter, &v);
		fprintf(file, "0x%04X", v);
	}
	break;
	case DBUS_TYPE_INT16:
	{
		int16_t v;
		dbus_message_iter_get_basic(iter, &v);
		fprintf(file, "%d", v);
	}
	break;
	case DBUS_TYPE_UINT32:
	{
		uint32_t v;
		dbus_message_iter_get_basic(iter, &v);
		fprintf(file, "%d", v);
	}
	break;
	case DBUS_TYPE_BOOLEAN:
	{
		dbus_bool_t v;
		dbus_message_iter_get_basic(iter, &v);
		fprintf(file, "%s", v ? "true" : "false");
	}
	break;
	case DBUS_TYPE_INT32:
	{
		int32_t v;
		dbus_message_iter_get_basic(iter, &v);
		fprintf(file, "%d", v);
	}
	break;
	case DBUS_TYPE_UINT64:
	{
		uint64_t v;
		dbus_message_iter_get_basic(iter, &v);
		fprintf(file, "0x%016llX", (unsigned long long)v);
	}
	break;
	default:
		fprintf(file, "<%s>",
		        dbus_message_type_to_string(dbus_message_iter_get_arg_type(iter)));
		break;
	}
	if (!bare)
		fprintf(file, "\n");
}

static int
parse_network_info_from_iter(
    struct wpan_network_info_s *network_info, DBusMessageIter *iter
    )
{
	int ret = 0;
	DBusMessageIter outer_iter;
	DBusMessageIter dict_iter;

	if (dbus_message_iter_get_arg_type(iter) == DBUS_TYPE_ARRAY) {
		dbus_message_iter_recurse(iter, &outer_iter);
		iter = &outer_iter;
	}

	memset(network_info, 0, sizeof(*network_info));

	for (;
	     dbus_message_iter_get_arg_type(iter) != DBUS_TYPE_INVALID;
	     dbus_message_iter_next(iter)) {
		DBusMessageIter value_iter;
		char* key;

		if (dbus_message_iter_get_arg_type(iter) != DBUS_TYPE_DICT_ENTRY) {
			DBG(
			    "error: Bad type for network (%c)\n",
			    dbus_message_iter_get_arg_type(iter));
			ret = -1;
			goto bail;
		}

		dbus_message_iter_recurse(iter, &dict_iter);

		if (dbus_message_iter_get_arg_type(&dict_iter) != DBUS_TYPE_STRING) {
			DBG(
			    "error: Bad type for network list (%c)\n",
			    dbus_message_iter_get_arg_type(&dict_iter));
			ret = -1;
			goto bail;
		}

		// Get the key
		dbus_message_iter_get_basic(&dict_iter, &key);
		dbus_message_iter_next(&dict_iter);

		if (key == NULL) {
			ret = -EINVAL;
			goto bail;

		}

		if (dbus_message_iter_get_arg_type(&dict_iter) != DBUS_TYPE_VARIANT) {
			DBG(
			    "error: Bad type for network list (%c)\n",
			    dbus_message_iter_get_arg_type(&dict_iter));
			ret = -1;
			goto bail;
		}

		dbus_message_iter_recurse(&dict_iter, &value_iter);

		if (strcmp(key, kWPANTUNDProperty_NetworkName) == 0 || strcmp(key, "NetworkName") == 0) {
			char* network_name = NULL;
			dbus_message_iter_get_basic(&value_iter, &network_name);
			snprintf(network_info->network_name,
			         sizeof(network_info->network_name), "%s", network_name);
		} else if ((strcmp(key, kWPANTUNDProperty_NCPChannel) == 0) || strcmp(key, "Channel") == 0) {
			dbus_message_iter_get_basic(&value_iter, &network_info->channel);
		} else if ((strcmp(key, kWPANTUNDProperty_NetworkPANID) == 0) || strcmp(key, "PanId") == 0) {
			dbus_message_iter_get_basic(&value_iter, &network_info->pan_id);
		} else if ((strcmp(key, kWPANTUNDProperty_NetworkXPANID) == 0) || strcmp(key, "XPanId") == 0) {
			dbus_message_iter_get_basic(&value_iter, &network_info->xpanid);
		} else if ((strcmp(key, kWPANTUNDProperty_NestLabs_NetworkAllowingJoin) == 0) || strcmp(key, "AllowingJoin") == 0) {
			dbus_message_iter_get_basic(&value_iter,
			                            &network_info->allowing_join);
		} else if ((strcmp(key, kWPANTUNDProperty_NCPHardwareAddress) == 0) || strcmp(key, "BeaconHWAddr") == 0) {
			DBusMessageIter array_iter;
			dbus_message_iter_recurse(&value_iter, &array_iter);
			const uint8_t* value = NULL;
			int nelements = 0;
			dbus_message_iter_get_fixed_array(&array_iter, &value, &nelements);
			if (value != NULL && nelements == 8)
				memcpy(network_info->hwaddr, value, 8);
		} else if (strcmp(key, kWPANTUNDProperty_IPv6MeshLocalPrefix) == 0) {
			int value_dbus_type = dbus_message_iter_get_arg_type(&value_iter);
			if (value_dbus_type == DBUS_TYPE_STRING) {
				const char *prefix_cstr = NULL;
				dbus_message_iter_get_basic(&value_iter, &prefix_cstr);
				parse_prefix_string(prefix_cstr, network_info->prefix);
			} else if (value_dbus_type == DBUS_TYPE_ARRAY) {
				DBusMessageIter array_iter;
				dbus_message_iter_recurse(&value_iter, &array_iter);
				const uint8_t *value = NULL;
				int nelements = 0;
				dbus_message_iter_get_fixed_array(&array_iter, &value, &nelements);
				if (value != NULL && nelements == 8) {
					memcpy(network_info->prefix, value, 8);
				}
			} else {
				DBG("Unexpected dbus type %c for %s", value_dbus_type, kWPANTUNDProperty_IPv6MeshLocalPrefix);
				memset(network_info->prefix, 0 , sizeof(network_info->prefix));
			}
		} else if (strcmp(key, "RSSI") == 0) {
			int8_t rssi;
			dbus_message_iter_get_basic(&value_iter, &rssi);
			network_info->rssi = rssi;
		} else {
#if DEBUG
			DBG(
			    "info: %s -> (%c)\n",
			    key,
			    dbus_message_iter_get_arg_type(&value_iter));
#endif
		}
	}

bail:
	if (ret) DBG("Network parse failed.\n");
	return ret;
}

/* -------------------------------------------------------------------------- */
// MARK: - LoWPAN Network

static struct connman_network*
get_network_from_iter(
    struct connman_device* device, DBusMessageIter *iter
    )
{
	const char* new_state = NULL;
	struct connman_network *network = NULL;
	struct wpan_network_info_s network_info = {};
	char network_identifier[256];
	const char * group_identifier = network_identifier;
	char hwaddr_str[256];
	struct lowpan_network_s *network_data = NULL;

	if (parse_network_info_from_iter(&network_info, iter) != 0) {
		goto bail;
	}

	if (!network_info.network_name[0]) {
		goto bail;
	}

	if (network_info.xpanid == 0) {
		goto bail;
	}

	encode_data_into_b16_string(
	    (const uint8_t*)network_info.network_name,
	    strlen(network_info.network_name),
	    network_identifier,
	    sizeof(network_identifier),
	    0
    );

	encode_data_into_b16_string(
	    (const uint8_t*)network_info.hwaddr,
	    8,
	    hwaddr_str,
	    sizeof(hwaddr_str),
	    0
    );

	snprintf(network_identifier + strlen(network_identifier),
	         sizeof(network_identifier) - strlen(
	             network_identifier), "_x%016llX", (unsigned long long)network_info.xpanid);

	network = connman_device_get_network(device, network_identifier);

	if (!network) {
		network = connman_network_create(
		    network_identifier,
		    CONNMAN_NETWORK_TYPE_LOWPAN
	    );
		network_data = (struct lowpan_network_s *)calloc(1,sizeof(struct lowpan_network_s));
		network_data->network_info = network_info;
		network_data->node_type = WPAN_IFACE_ROLE_ROUTER;
		connman_network_set_data(network, network_data);
		connman_network_set_string(network, LOWPAN_SECURITY_KEY, "psk");

		connman_device_add_network(device, network);

		connman_network_unref(network);

		connman_network_set_strength(network,
		                             CalculateStrengthFromRSSI(network_info.rssi));
		connman_network_set_name(network, network_info.network_name);

		// Set network extended ID before a service is created from network
		// since this information is needed when service loads provision.
		connman_network_set_lowpan_xpan_id(network, network_data->network_info.xpanid);

		connman_network_set_group(network, group_identifier);
		connman_network_set_index(network, -1);

		DBG("New Network: %p ident:%s group:%s", network, network_identifier, group_identifier);
	} else {
		network_data = connman_network_get_data(network);
		network_data->network_info = network_info;
	}

	connman_network_set_bool(network, LOWPAN_PERMIT_JOINING_KEY, network_data->network_info.allowing_join);
	connman_network_set_string(network, LOWPAN_PARENT_ADDRESS_KEY, hwaddr_str);

bail:

	DBG("%p NetworkFromIter: %p", device, network);
	return network;
}


static void
network_unref_callback(void* user_data)
{
	struct connman_network *network = (struct connman_network *)user_data;

	connman_network_unref(network);
}

static int
lowpan_network_probe(struct connman_network *network)
{
	DBG("%p %s", network, connman_network_get_identifier(network));

	return 0;
}

static void
lowpan_network_remove(struct connman_network *network)
{
	DBG("%p %s", network, connman_network_get_identifier(network));

	struct lowpan_network_s *network_data =
	    (struct lowpan_network_s *)connman_network_get_data(network);

	if(network_data) {
		connman_network_set_name(network, "X");
		connman_network_set_group(network, "X");

		free(network_data);
		connman_network_set_data(network, NULL);
	}

}

static void
join_finished_callback(
    DBusPendingCall *pending, void* user_data
    )
{
	int32_t ret = 0;
	struct connman_network *network = (struct connman_network *)user_data;
	struct connman_device* device = NULL;
	struct lowpan_device_s *device_info = NULL;
	DBusMessage* reply = dbus_pending_call_steal_reply(pending);
	DBusMessageIter iter;
	DBusMessageIter list_iter;

	// network will never be NULL in this callback.

	DBG("%p %s", network, connman_network_get_identifier(network));

	device = connman_network_get_device(network);

	require_action(device != NULL, bail, ret = -ENODEV);

	device_info = (struct lowpan_device_s *)connman_device_get_data(device);

	require_action(device_info != NULL, bail, ret = -ENODEV);
	require_action(reply != NULL, bail, ret = -EINVAL);

	dbus_message_iter_init(reply, &iter);

	// Get return code
	dbus_message_iter_get_basic(&iter, &ret);

bail:
	if (reply) {
		dbus_message_unref(reply);
	}
	if (pending) {
		dbus_pending_call_unref(pending);
	}

	if ((ret != 0) && (ret != -EINPROGRESS) && (ret != kWPANTUNDStatus_InProgress)) {
		DBG("%p Join/Resume returned failure: %d", network, ret);

		if((device_info != NULL) && ncp_state_is_has_joined(device_info->ncp_state)) {
			DBG("%p ... But we seem to have connected anyway. Ignoring the error.", network);
			ret = 0;
		} else {
			connman_network_set_error(network, CONNMAN_NETWORK_ERROR_INVALID_KEY);
		}
	}

	if (ret == 0) {
		connman_network_set_connected(network, TRUE);
	}

	return;
}

static int
lowpan_network_connect_using_join(struct connman_network *network)
{
	int ret = -EINVAL;
	DBG("%p %s", network, connman_network_get_identifier(network));
	struct connman_device* device = connman_network_get_device(network);
	DBusPendingCall *pending_call = NULL;
	DBusMessage *message = NULL;
	char dbus_path[DBUS_MAXIMUM_NAME_LENGTH];
	struct lowpan_network_s *network_data =
	    (struct lowpan_network_s *)connman_network_get_data(network);

	require_action(device != NULL, bail, ret = -ENODEV);

	require_action(network_data != NULL, bail, ret = -ENODEV);

	struct lowpan_device_s *device_info =
	    (struct lowpan_device_s *)connman_device_get_data(device);

	require_action(device_info != NULL, bail, ret = -ENODEV);

	snprintf(dbus_path,
	         sizeof(dbus_path),
	         "%s/%s",
	         WPAN_TUNNEL_DBUS_PATH,
	         connman_device_get_ident(device));

	message = dbus_message_new_method_call(
	    WPAN_TUNNEL_DBUS_NAME,
	    dbus_path,
	    WPAN_TUNNEL_DBUS_INTERFACE,
	    WPAN_IFACE_CMD_JOIN
	    );
	const char* network_name = network_data->network_info.network_name;
	dbus_message_append_args(
	    message,
	    DBUS_TYPE_STRING, &network_name,
	    DBUS_TYPE_INVALID
	    );

	dbus_message_append_args(
	    message,
	    DBUS_TYPE_INT16, &network_data->node_type,
	    DBUS_TYPE_INVALID
	    );

	dbus_message_append_args(
	    message,
	    DBUS_TYPE_UINT64, &network_data->network_info.xpanid,
	    DBUS_TYPE_INVALID
	    );

	dbus_message_append_args(
	    message,
	    DBUS_TYPE_UINT16, &network_data->network_info.pan_id,
	    DBUS_TYPE_INVALID
	    );

	dbus_message_append_args(
	    message,
	    DBUS_TYPE_BYTE, &network_data->network_info.channel,
	    DBUS_TYPE_INVALID
	    );


	if (!dbus_connection_send_with_reply(
	        connection,
	        message,
	        &pending_call,
	        45000
	        )
	    ) {
		ret = -EINVAL;
		goto bail;
	}

	connman_network_ref(network);

	if (!dbus_pending_call_set_notify(
	        pending_call,
	        &join_finished_callback,
	        (void*)network,
	        (DBusFreeFunction) & network_unref_callback
	        )
	    ) {
		dbus_pending_call_cancel(pending_call);
		connman_network_unref(network);
		ret = -EINVAL;
		goto bail;
	}

	ret = -EINPROGRESS;


bail:
	if(message)
		dbus_message_unref(message);
	return ret;
}

static int
lowpan_network_connect_using_resume(struct connman_network *network)
{
	int ret = -EINVAL;

	DBG("%p %s", network, connman_network_get_identifier(network));
	struct connman_device* device = connman_network_get_device(network);
	DBusPendingCall *pending_call = NULL;
	DBusMessage *message = NULL;
	char dbus_path[DBUS_MAXIMUM_NAME_LENGTH];

	require_action(device != NULL, bail, ret = -ENODEV);

	struct lowpan_device_s *device_info =
	    (struct lowpan_device_s *)connman_device_get_data(device);

	require_action(device_info != NULL, bail, ret = -ENODEV);

	snprintf(dbus_path,
	         sizeof(dbus_path),
	         "%s/%s",
	         WPAN_TUNNEL_DBUS_PATH,
	         connman_device_get_ident(device));

	message = dbus_message_new_method_call(
	    WPAN_TUNNEL_DBUS_NAME,
	    dbus_path,
	    WPAN_TUNNEL_DBUS_INTERFACE,
	    WPAN_IFACE_CMD_RESUME
	    );

	if (!dbus_connection_send_with_reply(
	        connection,
	        message,
	        &pending_call,
	        30000 // Thirty seconds
	        )) {
		ret = -1;
		goto bail;
	}

	connman_network_ref(network);

	if (!dbus_pending_call_set_notify(
	        pending_call,
	        &join_finished_callback,
	        (void*)network,
	        (DBusFreeFunction) & network_unref_callback
	        )
	    ) {
		dbus_pending_call_cancel(pending_call);
		connman_network_unref(network);
		ret = -EINVAL;
		return -1;
	}

	DBG("Now waiting for resume to complete...");

	ret = -EINPROGRESS;

bail:
	if(message)
		dbus_message_unref(message);
	return ret;
}

static void
lowpan_network_update_key(struct connman_network *network, const uint8_t* data, size_t len)
{
	char key_cstr[16*2+1];

	DBG("%p %s", network, connman_network_get_identifier(network));

	if (len != 0) {
		encode_data_into_b16_string(data, len, key_cstr, sizeof(key_cstr), 0);
		connman_network_set_string(network, LOWPAN_AUTH_KEY, key_cstr);
		connman_network_update(network);
	}
}

static void
_lowpan_network_update_key_from_ncp_callback(void* context, int error, const uint8_t* data, size_t len)
{
	struct connman_network *network = context;
	DBG("%p %s", network, connman_network_get_identifier(network));
	if (error == 0) {
		lowpan_network_update_key(network, data, len);
	}

	struct connman_device* device = connman_network_get_device(network);

	if (!device) {
		DBG("NO DEVICE!");
		return;
	}

	struct lowpan_device_s *device_info =
	    (struct lowpan_device_s *)connman_device_get_data(device);

	if (!device_info) {
		DBG("NO DEVICE INFO!");
		return;
	}

	if (ncp_state_is_has_joined(device_info->ncp_state)
		&& !connman_network_get_connected(network)
		&& !connman_network_get_connecting(network)
		&& !connman_network_get_associating(network)
	) {
		DBG("We got the network key, asking connman to connect...");
		connman_service_connect(connman_service_lookup_from_network(network),
                CONNMAN_SERVICE_CONNECT_REASON_USER);
	}

	connman_network_unref(network);
}

static int
lowpan_network_update_key_from_ncp(struct connman_network *network)
{
	int ret = 0;
	struct connman_device* device = connman_network_get_device(network);
	if (!device) {
		ret = -ENODEV;
		goto bail;
	}

	DBG("%p %s", network, connman_network_get_identifier(network));

	connman_network_ref(network);

	ret = lowpan_driver_getprop_data(
		device,
		kWPANTUNDProperty_NetworkKey,
		&_lowpan_network_update_key_from_ncp_callback,
		network
	);

	if(ret != 0)
		goto bail;

bail:
	return ret;
}

static int
lowpan_network_set_key_on_ncp(struct connman_network *network)
{
	int ret = 0;
	struct connman_device* device = connman_network_get_device(network);
	DBG("%p %s", network, connman_network_get_identifier(network));
	if (!device) {
		ret = -ENODEV;
		goto bail;
	}
	uint8_t key[16];
	unsigned int size = sizeof(key);
	const char* key_cstr = connman_network_get_string(network, LOWPAN_AUTH_KEY);

	if (key_cstr) {
		size = parse_string_into_data(key, size, key_cstr);

		if (size > sizeof(key)) {
			connman_warn("Key is too large: %d (max %d)", size, (int)sizeof(key));
			ret = -EINVAL;
		} else if (sizeof(key) != size) {
			connman_warn("Key-size mismatch (Expecting %d, but key was %d bytes long)", (int)sizeof(key), size);
		}
	} else {
#if 0
		size = 16;
		memset(key,0,sizeof(key));
		size = sizeof(key);
		connman_warn("No valid key specified for %s, using all-zeros...", connman_network_get_identifier(network));
#else
		ret = -ENOKEY;
		DBG("No key to set!");
#endif
	}

	if (ret == 0) {
		DBG("Setting the key at %p for the service...", key);
		ret = lowpan_driver_setprop_data(device, kWPANTUNDProperty_NetworkKey, key, size);
	}

bail:
	return ret;
}

static int
lowpan_network_connect(struct connman_network *network)
{
	int ret = -EINVAL;
	struct connman_device* device = connman_network_get_device(network);

	DBG("%p %s", network, connman_network_get_identifier(network));

	if (!device) {
		ret = -ENODEV;
		goto bail;
	}

	struct lowpan_device_s *device_info =
	    (struct lowpan_device_s *)connman_device_get_data(device);

	if (!device_info) {
		ret = -ENODEV;
		goto bail;
	}

	if (device_info->ncp_state == NCP_STATE_UPGRADING) {
		// We can't connect while upgrading.
		ret = -EBUSY;
		goto bail;
	} else if (ncp_state_is_not_associated(device_info->ncp_state)) {
		lowpan_device_set_network(device, network);
		ret = lowpan_network_connect_using_join(network);
	} else if (device_info->ncp_state == NCP_STATE_COMMISSIONED) {
		if (device_info->current_network == network) {
			// Do not auto-resume, this is performed automatically by wpantund.
			// TODO: Check if Autoresume is enabled and allow this if not.
			// ret = lowpan_network_connect_using_resume(network);

			// Since we know Autoresume is on, we return -EINPROGRESS.
			ret = -EINPROGRESS;
		} else {
			ret = -EBUSY;
		}
	} else if (device_info->ncp_state == NCP_STATE_CREDENTIALS_NEEDED) {
		if (device_info->current_network == network) {
			ret = lowpan_network_set_key_on_ncp(network);

			if (-ENOKEY == ret) {
				ret = connman_network_needs_input(network);
				if (0 != ret) {
					DBG("connman_network_needs_input(network) failed with %d", ret);
				}
			}

			if ((0 != ret) && (-EINPROGRESS != ret)) {
				connman_network_set_associating(network, false);
				lowpan_device_leave(device);
			}

			if (0 == ret) {
				ret = -EINPROGRESS;
			}
		} else {
			DBG("%p Aborting connection in progress", network);
			lowpan_device_leave(device);
			ret = -EAGAIN;
		}
	} else if (device_info->ncp_state == NCP_STATE_ASSOCIATING) {
		if (device_info->current_network == network) {
			DBG("%p Already connecting to THIS network!", network);
			ret = -EINPROGRESS;
		} else {
			DBG("%p Already connecting to a different network!", network);
			ret = -EINVAL;
		}
	} else if (ncp_state_is_has_joined(device_info->ncp_state)) {
		if (device_info->current_network == network) {
			DBG("%p Already connected to THIS network!", network);
			ret = 0;
			connman_network_set_connected(network, TRUE);
		} else if(device_info->current_network && 0 == strcmp(connman_network_get_group(device_info->current_network),connman_network_get_group(network))) {
			DBG("%p Already connected to THIS service!", network);
			ret = 0;
			connman_network_unref(device_info->current_network);
			device_info->current_network = network;
			connman_network_ref(device_info->current_network);
			connman_network_set_connected(network, TRUE);
		} else {
			DBG("%p Already connected to an entirely different network, %s!", network, connman_network_get_identifier(device_info->current_network));
			ret = -EINVAL;
		}
	}

bail:
	DBG("%p ret=%d", network, ret);
	return ret;
}

static void
disconnect_finished_callback(
    DBusPendingCall *pending, void* user_data
    )
{
}


void
lowpan_device_leave(struct connman_device *device)
{
	DBusMessage *message = NULL;
	char dbus_path[DBUS_MAXIMUM_NAME_LENGTH];

	snprintf(dbus_path,
	         sizeof(dbus_path),
	         "%s/%s",
	         WPAN_TUNNEL_DBUS_PATH,
	         connman_device_get_ident(device));

	message = dbus_message_new_method_call(
	    WPAN_TUNNEL_DBUS_NAME,
	    dbus_path,
	    WPAN_TUNNEL_DBUS_INTERFACE,
	    WPAN_IFACE_CMD_LEAVE
	    );

	dbus_connection_send(connection, message, NULL);

	if(message)
		dbus_message_unref(message);
}

static void
lowpan_device_reset(struct connman_device *device)
{
	DBusMessage *message = NULL;
	char dbus_path[DBUS_MAXIMUM_NAME_LENGTH];

	snprintf(dbus_path,
	         sizeof(dbus_path),
	         "%s/%s",
	         WPAN_TUNNEL_DBUS_PATH,
	         connman_device_get_ident(device));

	message = dbus_message_new_method_call(
	    WPAN_TUNNEL_DBUS_NAME,
	    dbus_path,
	    WPAN_TUNNEL_DBUS_INTERFACE,
	    WPAN_IFACE_CMD_RESET
	    );

	dbus_connection_send(connection, message, NULL);

	if(message)
		dbus_message_unref(message);
}


static int
lowpan_network_disconnect(struct connman_network *network, bool user_initiated)
{
	int ret = -EINVAL;
	bool should_reset = false;
	struct connman_device* device = connman_network_get_device(network);
	struct lowpan_device_s *device_info =
	    (struct lowpan_device_s *)connman_device_get_data(device);

	DBG("%p %s", network, connman_network_get_identifier(network));

	if (!device) {
		ret = -ENODEV;
		goto bail;
	}

	if (!device_info) {
		ret = -ENODEV;
		goto bail;
	}

	if (device_info->current_network != network) {
		ret = -EINVAL;
		goto bail;
	}

	connman_network_set_connected(network, FALSE);

	switch(device_info->ncp_state) {
	case NCP_STATE_ASSOCIATING:
	case NCP_STATE_CREDENTIALS_NEEDED:
		should_reset = true;
		break;

	case NCP_STATE_OFFLINE:
		user_initiated = TRUE;
		break;
	default:
		break;
	}

	if (user_initiated) {
		lowpan_device_leave(device);
	} else if (should_reset) {
		lowpan_device_reset(device);
	}

	ret = 0;

bail:
	DBG("%p ret=%d", network, ret);
	return ret;
}

int
lowpan_driver_setprop_int32(struct connman_device *device, const char* key, int32_t value)
{
	int ret = 0;
	DBusMessage *message = NULL;
	char dbus_path[128];

	snprintf(dbus_path,
	         sizeof(dbus_path),
	         "%s/%s",
	         WPAN_TUNNEL_DBUS_PATH,
	         connman_device_get_ident(device));

	message = dbus_message_new_method_call(
	    WPAN_TUNNEL_DBUS_NAME,
	    dbus_path,
	    WPAN_TUNNEL_DBUS_INTERFACE,
	    WPAN_IFACE_CMD_SET_PROP
	    );

	dbus_message_append_args(
	    message,
	    DBUS_TYPE_STRING, &key,
	    DBUS_TYPE_INT32, &value,
	    DBUS_TYPE_INVALID
	    );

	if (!dbus_connection_send(
	        connection,
	        message,
	        NULL)) {
		dbus_message_unref(message);
		ret = -ENOMEM;
		goto bail;
	}

bail:
	if(message)
		dbus_message_unref(message);
	return ret;
}

int
lowpan_driver_setprop_data(struct connman_device *device, const char* key, const uint8_t* data, size_t size)
{
	int ret = 0;
	DBusMessage *message = NULL;
	char dbus_path[128];

	snprintf(dbus_path,
	         sizeof(dbus_path),
	         "%s/%s",
	         WPAN_TUNNEL_DBUS_PATH,
	         connman_device_get_ident(device));

	message = dbus_message_new_method_call(
	    WPAN_TUNNEL_DBUS_NAME,
	    dbus_path,
	    WPAN_TUNNEL_DBUS_INTERFACE,
	    WPAN_IFACE_CMD_SET_PROP
	    );

	dbus_message_append_args(
	    message,
	    DBUS_TYPE_STRING, &key,
	    DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE, &data, size,
	    DBUS_TYPE_INVALID
	    );

	if (!dbus_connection_send(
	        connection,
	        message,
	        NULL)) {
		dbus_message_unref(message);
		ret = -ENOMEM;
		goto bail;
	}

bail:
	if(message)
		dbus_message_unref(message);
	return ret;
}

struct lowpan_driver_getprop_state_s {
	void(*callback)(void* context, int err, const uint8_t* data, size_t len);
	void* context;
	struct connman_device* device;
};

static void
_lowpan_driver_getprop_data_free(void* user_data)
{
	struct lowpan_driver_getprop_state_s* state = user_data;
	DBG("%p device:%p", state, state->device);
//	DBG("%p %s", state->device, connman_device_get_ident(state->device));

	if (NULL != state->callback) {
		(*state->callback)(state->context, -1, NULL, 0);
		state->callback = NULL;
	}
	if(state->device)
		connman_device_unref(state->device);
	free(state);
}

static void
_lowpan_driver_getprop_data_callback(
    DBusPendingCall *pending, void* user_data
    )
{
	int32_t ret = 0;
	struct lowpan_driver_getprop_state_s* state = user_data;
	DBusMessage* reply = dbus_pending_call_steal_reply(pending);
	DBusMessageIter iter;
	const uint8_t* value = NULL;
	int nelements = 0;

	if (!state) {
		goto bail;
	}

	DBG("%p device:%p", state, state->device);
//	DBG("%p %s", state->device, connman_device_get_ident(state->device));

	if (!reply) {
		DBG("No reply...?");
		goto bail;
	}

	dbus_message_iter_init(reply, &iter);

	// Get return code
	dbus_message_iter_get_basic(&iter, &ret);
	dbus_message_iter_next(&iter);

	if (ret == 0) {
		DBusMessageIter array_iter;
		dbus_message_iter_recurse(&iter, &array_iter);
		dbus_message_iter_get_fixed_array(&array_iter, &value, &nelements);
	}

	if (NULL != state->callback) {
		(*state->callback)(state->context, ret, value, nelements);
		state->callback = NULL;
	}

bail:
	if (reply) {
		dbus_message_unref(reply);
	}
	if (pending) {
		dbus_pending_call_unref(pending);
	}

	DBG("%p ret = %d", state->device, ret);
	return;
}

int
lowpan_driver_getprop_data(struct connman_device *device, const char* key, void(*callback)(void* context, int err, const uint8_t* data, size_t len), void* context)
{
	int ret = 0;
	DBusMessage *message = NULL;
	DBusPendingCall *pending_call = NULL;
	struct lowpan_driver_getprop_state_s* state = NULL;
	char dbus_path[128];

	DBG("%p %s key:%s", device, connman_device_get_ident(device),key);

	snprintf(dbus_path,
	         sizeof(dbus_path),
	         "%s/%s",
	         WPAN_TUNNEL_DBUS_PATH,
	         connman_device_get_ident(device));

	message = dbus_message_new_method_call(
	    WPAN_TUNNEL_DBUS_NAME,
	    dbus_path,
	    WPAN_TUNNEL_DBUS_INTERFACE,
	    WPAN_IFACE_CMD_GET_PROP
	    );

	state = calloc(sizeof(*state),1);
	if(!state) {
		ret = -ENOMEM;
		goto bail;
	}
	state->context = context;
	state->callback = callback;
	state->device = device;
	connman_device_ref(device);

	dbus_message_append_args(
	    message,
	    DBUS_TYPE_STRING, &key,
	    DBUS_TYPE_INVALID
	    );

	if (!dbus_connection_send_with_reply(
	        connection,
	        message,
	        &pending_call,
	        10000 // Ten seconds
	        )
	) {
		ret = -ENOMEM;
		goto bail;
	}

	if (!dbus_pending_call_set_notify(
	        pending_call,
	        &_lowpan_driver_getprop_data_callback,
	        (void*)state,
	        &_lowpan_driver_getprop_data_free
		)
	) {
		ret = -EINVAL;
		goto bail;
	}

	DBG("state:%p device:%p ret = %d",state, state->device, ret);

	pending_call = NULL;
	state = NULL;

bail:
	DBG("device:%p ret = %d",device, ret);

	if(state)
		_lowpan_driver_getprop_data_free((void*)state);
	if(message)
		dbus_message_unref(message);
	if(pending_call)
		dbus_pending_call_cancel(pending_call);

	return ret;
}


static struct connman_network_driver lowpan_network_driver = {
	.name           = "lowpan",
	.type           = CONNMAN_NETWORK_TYPE_LOWPAN,
	.priority       = CONNMAN_NETWORK_PRIORITY_LOW,
	.probe          = lowpan_network_probe,
	.remove         = lowpan_network_remove,
	.connect        = lowpan_network_connect,
	.disconnect = lowpan_network_disconnect,
};

/* -------------------------------------------------------------------------- */
// MARK: - LoWPAN Device

void
lowpan_device_set_network(struct connman_device *device, struct connman_network *network)
{
	struct lowpan_device_s *device_info =
	    (struct lowpan_device_s *)connman_device_get_data(device);

	if (network != device_info->current_network) {
		DBG("%p Request to change current network from %p (\"%s\") to %p (\"%s\").", device, device_info->current_network,device_info->current_network?connman_network_get_identifier(device_info->current_network):NULL,network,network?connman_network_get_identifier(network):NULL);
		if(device_info->current_network
		   && network
		   && 0 == strcmp(connman_network_get_group(device_info->current_network),connman_network_get_group(network)))
		{
			if(connman_network_get_connected(device_info->current_network)
			   || connman_network_get_connecting(device_info->current_network)
			   || connman_network_get_associating(device_info->current_network)
			   ) {
				DBG("%p Networks are a part of the same group and a connection is in progress. Network change aborted.", device);

				return;
			}
		}

		if (device_info->current_network != NULL) {
			connman_network_set_index(device_info->current_network, -1);
			connman_network_unref(device_info->current_network);
		}
		device_info->current_network = network;
		if (device_info->current_network != NULL)
			connman_network_ref(device_info->current_network);
		DBG("%p Network change complete.", device);
	}
}

int
lowpan_device_handle_state_change(
    struct connman_device*  device,
    ncp_state_t new_state,
    struct connman_network* network
    )
{
	int ret;
	struct lowpan_device_s *device_info =
	    (struct lowpan_device_s *)connman_device_get_data(device);

	if (!ncp_state_is_not_associated(new_state) && !network) {
		network = device_info->current_network;
	}

	DBG("%s %d network %p", connman_device_get_ident(device), new_state, network);

	if (network == device_info->current_network
		&& (new_state == device_info->ncp_state)
		&& (new_state != NCP_STATE_ASSOCIATED)
	) {
		DBG("%s State was already %d", connman_device_get_ident(device), new_state);

		// Skip when nothing has really changed.
		return 0;
	}

	if (ncp_state_is_not_associated(new_state)) {
		// If the previous NCP state is also 'disconnected' or 'uninitialized' ignore the new state change
		if (ncp_state_is_not_associated(device_info->ncp_state)) {
			// In case of connecting to a network from 'deep-sleep' we get a 'disconnected' state before change
			// to 'joining'. In this case, we should ignore the 'disconnected' state change as to not remove the
			// current_network we are attempting to connect to and fail to connect/join.
			device_info->ncp_state = new_state;
			DBG("%s State was already effectively %d", connman_device_get_ident(device), new_state);
			return 0;
		}

		connman_device_set_disconnected(device, TRUE);

		if (device_info->current_network != NULL) {
			memset(&device_info->current_network_info,0,sizeof(device_info->current_network_info));
			if (connman_network_get_connecting(device_info->current_network))
				connman_network_set_error(device_info->current_network, CONNMAN_NETWORK_ERROR_CONNECT_FAIL);
			else if (connman_network_get_associating(device_info->current_network))
				connman_network_set_error(device_info->current_network, CONNMAN_NETWORK_ERROR_ASSOCIATE_FAIL);
			else
				connman_network_set_connected(device_info->current_network, FALSE);
		}
		network = NULL;
	}

	lowpan_device_set_network(device, network);
	network = device_info->current_network;

	if (ncp_state_is_initializing(new_state)) {
		return 0;
	}

	device_info->ncp_state = new_state;

	if (network) {
		if (new_state == NCP_STATE_COMMISSIONED) {
			if (connman_network_get_connected(network)) {
				// Do not auto-resume, this is performed automatically by wpantund.
				// TODO: Check if Autoresume is enabled and allow this if not.
				// lowpan_network_connect_using_resume(network);
			} else if (connman_network_get_connecting(network)
			           || connman_network_get_associating(network)
			           ) {
				connman_network_set_error(network, CONNMAN_NETWORK_ERROR_CONNECT_FAIL);
			} else {
				connman_service_connect(connman_service_lookup_from_network(network),
                        CONNMAN_SERVICE_CONNECT_REASON_USER);
			}
		} else if (new_state == NCP_STATE_ASSOCIATING) {

		} else if (new_state == NCP_STATE_CREDENTIALS_NEEDED) {
			if (connman_network_get_associating(network)) {
				int err;

				connman_service_create_ip6config(connman_service_lookup_from_network(network), connman_device_get_index(device));
				connman_network_set_index(network, connman_device_get_index(device));
				connman_network_set_ipv4_method(network, CONNMAN_IPCONFIG_METHOD_OFF);
				connman_network_set_ipv6_method(network, CONNMAN_IPCONFIG_METHOD_FIXED);

				err = lowpan_network_set_key_on_ncp(network);
				if (-ENOKEY == err) {
					err = connman_network_needs_input(network);
					if (0 != err) {
						DBG("connman_network_needs_input(network) failed with %d", err);
					}
				}

				if ((0 != err) && (-EINPROGRESS != err)) {
					connman_network_set_associating(network, false);
					lowpan_device_leave(device);
				}
			}


		} else if (new_state == NCP_STATE_NET_WAKE_ASLEEP) {
			// Don't do anything special when we are in the lurking state.

		} else if (new_state == NCP_STATE_ASSOCIATED) {

			connman_service_create_ip6config(connman_service_lookup_from_network(network), connman_device_get_index(device));
			connman_network_set_index(network, connman_device_get_index(device));
			connman_network_set_ipv4_method(network, CONNMAN_IPCONFIG_METHOD_OFF);
			connman_network_set_ipv6_method(network, CONNMAN_IPCONFIG_METHOD_FIXED);

#if 0
			/* This part is removed as a solution for COM-1070 (Need temporary workaround for ConnMan supporting only one IPv6 address)
			 * We'd like the connman to store just the Thread ULA address (which is set by client through network manager)
			 */

			if (buffer_is_nonzero(device_info->current_network_info.prefix, sizeof(device_info->current_network_info.prefix))) {
				struct connman_ipaddress *ip_address;
				char v6_addr_str[56];

				ip_address = connman_ipaddress_alloc(CONNMAN_IPCONFIG_TYPE_IPV6);
				encode_ipv6_address_from_prefx_and_hwaddr(v6_addr_str,
														  sizeof(v6_addr_str),
														  device_info->current_network_info.prefix,
														  device_info->hwaddr);

				connman_ipaddress_set_ipv6(ip_address, v6_addr_str, 64, "::");

				connman_network_set_ipaddress(network, ip_address);

				connman_ipaddress_free(ip_address);
			} else {
				struct connman_ipaddress *ip_address;

				// If there is no prefix, set an empty address.
				ip_address = connman_ipaddress_alloc(CONNMAN_IPCONFIG_TYPE_IPV6);
				connman_ipaddress_set_ipv6(ip_address, "::", 128, "::");
				connman_network_set_ipaddress(network, ip_address);
				connman_ipaddress_free(ip_address);
			}

#endif       /*  End of change for COM-1070 */

			lowpan_network_update_key_from_ncp(network);

			if (!connman_network_get_connected(network)) {
				if (!connman_network_get_connecting(network)
				    && !connman_network_get_associating(network)
				    ) {
					if(connman_network_get_string(network, LOWPAN_AUTH_KEY)) {
						DBG("We need to get connman to connect...");
						connman_service_connect(connman_service_lookup_from_network(network),
                                CONNMAN_SERVICE_CONNECT_REASON_USER);
					} else {
						DBG("Waiting to get network key before asking connman to connect...");
						// This should happen in a few moments, initiated
						// by the call to lowpan_network_update_key_from_ncp, above.
					}
					return 0;
				} else {
					DBG("Marking Network as connected.");
					connman_network_set_connected(network, TRUE);
				}
			} else {
				DBG("Service/Network already connected.");
			}
		}
	}

	return 0;
}


DBusHandlerResult
lowpan_device_signal_handler(
    DBusConnection *connection,
    DBusMessage *   message,
    void *                  user_data
    )
{
	DBusHandlerResult ret = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	struct connman_device* device = user_data;
	struct lowpan_device_s *device_info =
	    (struct lowpan_device_s *)connman_device_get_data(device);
	const char* msg_path = dbus_message_get_path(message);
	const char* interface_name = connman_device_get_ident(device);
	char* path = NULL;

	asprintf(&path, "%s/%s", WPAN_TUNNEL_DBUS_PATH, interface_name);


	if (!msg_path || (0 != strcmp(path, msg_path)))
		goto bail;

	if (dbus_message_is_signal(message, WPAN_TUNNEL_DBUS_INTERFACE,
	                           WPAN_IFACE_SIGNAL_STATE_CHANGED)) {
		DBusMessageIter iter;
		const char* new_state = NULL;
		struct connman_network *network = NULL;
		struct wpan_network_info_s network_info;
		bool should_change_device_power_state = false;
		bool new_device_power_state = false;
		char network_identifier[256];

		ret = DBUS_HANDLER_RESULT_HANDLED;

		dbus_message_iter_init(message, &iter);

		dbus_message_iter_get_basic(&iter, &new_state);

		if (new_state == NULL) {
			ret = -EINVAL;
			goto bail;
		}

		if (ncp_state_is_initializing(device_info->ncp_state)) {
			// Make sure that we start up in a powered state.
			should_change_device_power_state = true;
			new_device_power_state = true;
		}

		DBG("AssociationStateChanged: %s", new_state);

		dbus_message_iter_next(&iter);

		network = get_network_from_iter(device, &iter);
		parse_network_info_from_iter(&device_info->current_network_info, &iter);
		ncp_state_t ncp_state;

		ncp_state = string_to_ncp_state(new_state, device_info->ncp_state);


		DBusMessageIter outer_iter;
		DBusMessageIter dict_iter;
		if (dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_ARRAY) {
			dbus_message_iter_recurse(&iter, &outer_iter);
			iter = outer_iter;
		}

		for (;
			 dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_INVALID;
			 dbus_message_iter_next(&iter)) {
			DBusMessageIter value_iter;
			char* key = NULL;

			if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_DICT_ENTRY) {
				ret = -1;
				goto bail;
			}

			dbus_message_iter_recurse(&iter, &dict_iter);

			if (dbus_message_iter_get_arg_type(&dict_iter) != DBUS_TYPE_STRING) {
				ret = -1;
				goto bail;
			}

			// Get the key
			dbus_message_iter_get_basic(&dict_iter, &key);
			dbus_message_iter_next(&dict_iter);

			if(key == NULL) {
				ret = -EINVAL;
				goto bail;
			}

			if (dbus_message_iter_get_arg_type(&dict_iter) != DBUS_TYPE_VARIANT) {
				ret = -1;
				goto bail;
			}

			dbus_message_iter_recurse(&dict_iter, &value_iter);

			if (strcmp(key, "Enabled") == 0) {
				dbus_bool_t enabled = 0;
				bool device_power_state = (should_change_device_power_state?
						new_device_power_state : connman_device_get_powered(device) );

				dbus_message_iter_get_basic(&value_iter, &enabled);

				DBG("NCP IS %s",enabled?"ENABLED":"DISABLED");
				if (enabled != device_power_state) {
					if (ncp_state_is_initializing(device_info->ncp_state)) {
						// If this connman_device is uninitialized/initializing, then we need to make
						// sure that the NCP is in our current power state.
						enabled = !enabled;
						DBG("%sABLING NCP",enabled?"EN":"DIS");
						lowpan_driver_setprop_int32(device, kWPANTUNDProperty_DaemonEnabled, enabled);
					} else {
						// If this connman_device is initialized, then we need to make
						// sure that our power state matches the power state of the NCP.
						should_change_device_power_state = true;
						new_device_power_state = enabled;
					}
				}
			}
		}


		lowpan_device_handle_state_change(device, ncp_state, network);

		if (should_change_device_power_state) {
			DBG("%sABLING CONNMAN DEVICE",new_device_power_state?"EN":"DIS");
			connman_device_set_powered(device, new_device_power_state);
		}
	}

bail:
	free(path);
	return ret;
}


static int
lowpan_device_probe(struct connman_device *device)
{
	DBG("%p %s", device, connman_device_get_ident(device));

	if (dbus_connection_add_filter(connection,
	                           lowpan_device_signal_handler,
	                           (void*)device,
	                           NULL) == FALSE) {
		return -EIO;
	}

	return 0;
}

static void
lowpan_device_remove(struct connman_device *device)
{
	DBG("%p %s", device, connman_device_get_ident(device));

	(void)dbus_connection_remove_filter(connection, lowpan_device_signal_handler,
	                              (void*)device);
}


static int
lowpan_device_enable(struct connman_device *device)
{
	DBG("%p %s", device, connman_device_get_ident(device));
	lowpan_driver_setprop_int32(device, kWPANTUNDProperty_DaemonEnabled, 1);
	return 0;
}

static int
lowpan_device_disable(struct connman_device *device)
{
	DBG("%p %s", device, connman_device_get_ident(device));

	struct lowpan_device_s *device_info = (struct lowpan_device_s *)connman_device_get_data(device);
	if (device_info) {
		lowpan_device_set_network(device, NULL);
		connman_device_set_disconnected(device, TRUE);
	}

	lowpan_driver_setprop_int32(device, kWPANTUNDProperty_DaemonEnabled, 0);
	return 0;
}

static void
status_finished_callback(
    DBusPendingCall *pending, void* user_data
    )
{
	int32_t ret = 0;
	struct connman_device *device = (struct connman_device *)user_data;
	struct lowpan_device_s *device_info =
	    (struct lowpan_device_s *)connman_device_get_data(device);
	struct connman_network *network = NULL;
	DBusMessage* reply = dbus_pending_call_steal_reply(pending);
	DBusMessageIter iter;
	ncp_state_t ncp_state;
	bool should_change_device_power_state = false;
	bool new_device_power_state = false;

	DBG("%p %s", device, connman_device_get_ident(device));

	if (!reply) {
		DBG("%p Status callback failed", device);
		ret = -1;
		goto bail;
	}

	dbus_message_iter_init(reply, &iter);
	dump_info_from_iter(stderr, &iter, 1, false);

	if(dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_STRING) {
		char* cstr = NULL;
		dbus_message_iter_get_basic(&iter,&cstr);
		DBG("%p Status callback failed: %s", device, cstr);
		ret = -1;
		goto bail;
	}

	parse_network_info_from_iter(&device_info->current_network_info, &iter);
	network = get_network_from_iter(device, &iter);

	DBusMessageIter outer_iter;
	DBusMessageIter dict_iter;
	if (dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_ARRAY) {
		dbus_message_iter_recurse(&iter, &outer_iter);
		iter = outer_iter;
	}

	if (ncp_state_is_initializing(device_info->ncp_state)) {
		// Make sure that we start up in a powered state.
		should_change_device_power_state = true;
		new_device_power_state = true;
	}

	ncp_state = device_info->ncp_state;

	for (;
	     dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_INVALID;
	     dbus_message_iter_next(&iter)) {
		DBusMessageIter value_iter;
		char* key = NULL;

		if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_DICT_ENTRY) {
			ret = -1;
			goto bail;
		}

		dbus_message_iter_recurse(&iter, &dict_iter);

		if (dbus_message_iter_get_arg_type(&dict_iter) != DBUS_TYPE_STRING) {
			ret = -1;
			goto bail;
		}

		// Get the key
		dbus_message_iter_get_basic(&dict_iter, &key);
		dbus_message_iter_next(&dict_iter);

		if(key == NULL) {
			ret = -EINVAL;
			goto bail;
		}

		if (dbus_message_iter_get_arg_type(&dict_iter) != DBUS_TYPE_VARIANT) {
			ret = -1;
			goto bail;
		}

		dbus_message_iter_recurse(&dict_iter, &value_iter);

		if (strcmp(key, kWPANTUNDProperty_NCPHardwareAddress) == 0) {
			DBusMessageIter array_iter;
			dbus_message_iter_recurse(&value_iter, &array_iter);
			const uint8_t* value = NULL;
			int nelements = 0;
			dbus_message_iter_get_fixed_array(&array_iter, &value, &nelements);
			if (nelements == 8)
				memcpy(device_info->hwaddr, value, 8);
			else
				DBG("%p Bad HWAddr length: %d", device, nelements);

			char hwaddr_str[64];
			snprintf(hwaddr_str,
			         sizeof(hwaddr_str),
			         "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
			         device_info->hwaddr[0],
			         device_info->hwaddr[1],
			         device_info->hwaddr[2],
			         device_info->hwaddr[3],
			         device_info->hwaddr[4],
			         device_info->hwaddr[5],
			         device_info->hwaddr[6],
			         device_info->hwaddr[7]);
			DBG("%p HWAddr set to %s", device, hwaddr_str);
			connman_device_set_string(device, "Address", hwaddr_str);

			struct connman_ipdevice *ipdevice = NULL;
			ipdevice = connman_ipdevice_lookup_from_index(connman_device_get_index(device));

			if (ipdevice) {
				connman_ipdevice_set_address(ipdevice, hwaddr_str);
				DBG("Set HWAddr on ipdevice! GOOD");
			} else {
				DBG("Can't set HWAddr on ipdevice because we can't find the ipdevice!");
			}

		} else if (strcmp(key, kWPANTUNDProperty_DaemonEnabled) == 0) {
			dbus_bool_t enabled = FALSE;
			bool device_power_state = (should_change_device_power_state?
					new_device_power_state : connman_device_get_powered(device) );

			dbus_message_iter_get_basic(&value_iter, &enabled);

			DBG("NCP IS %s",enabled?"ENABLED":"DISABLED");
			if (enabled != device_power_state) {
				if (ncp_state_is_initializing(device_info->ncp_state)) {
					// If this connman_device is UNinitialized, then we need to make
					// sure that the NCP is in our current power state.
					enabled = !enabled;
					DBG("%sABLING NCP",enabled?"EN":"DIS");
					lowpan_driver_setprop_int32(device, kWPANTUNDProperty_DaemonEnabled, enabled);
				} else {
					// If this connman_device is initialized, then we need to make
					// sure that our power state matches the power state of the NCP.
					should_change_device_power_state = true;
					new_device_power_state = enabled;
				}
			}

		} else if (strcmp(key, kWPANTUNDProperty_NetworkKey) == 0) {
			DBusMessageIter array_iter;
			dbus_message_iter_recurse(&value_iter, &array_iter);
			const uint8_t* value = NULL;
			int nelements = 0;
			dbus_message_iter_get_fixed_array(&array_iter, &value, &nelements);

			lowpan_network_update_key(network, value, nelements);
		} else if (strcmp(key, kWPANTUNDProperty_NCPState) == 0) {
			const char* new_state = NULL;
			dbus_message_iter_get_basic(&value_iter, &new_state);

			ncp_state = string_to_ncp_state(new_state, ncp_state);
		} else {
#if DEBUG
			DBG(
			    "info: %s -> (%c)\n",
			    key,
			    dbus_message_iter_get_arg_type(&value_iter));
#endif
		}
	}

	lowpan_device_handle_state_change(device, ncp_state, network);

	if (should_change_device_power_state) {
		DBG("%sABLING CONNMAN DEVICE",new_device_power_state?"EN":"DIS");
		connman_device_set_powered(device, new_device_power_state);
	}

bail:
	if (reply) {
		dbus_message_unref(reply);
	}
	if (pending) {
		dbus_pending_call_unref(pending);
	}

	return;
}

static void
scan_free_callback(void* user_data)
{
	struct connman_device *device = (struct connman_device *)user_data;

	connman_device_unref(device);
}


static int
lowpan_device_update_status(struct connman_device *device)
{
	DBusPendingCall *pending_status = NULL;
	DBusMessage *message = NULL;
	char dbus_path[128];

	snprintf(dbus_path,
	         sizeof(dbus_path),
	         "%s/%s",
	         WPAN_TUNNEL_DBUS_PATH,
	         connman_device_get_ident(device));

	message = dbus_message_new_method_call(
	    WPAN_TUNNEL_DBUS_NAME,
	    dbus_path,
	    WPAN_TUNNEL_DBUS_INTERFACE,
	    WPAN_IFACE_CMD_STATUS
	);

	if (!dbus_connection_send_with_reply(
	        connection,
	        message,
	        &pending_status,
	        3000
		)
	) {
		dbus_message_unref(message);
		return -1;
	}

	dbus_message_unref(message);

	if (!dbus_pending_call_set_notify(
	        pending_status,
	        &status_finished_callback,
	        (void*)device,
	        (DBusFreeFunction) & scan_free_callback
		)
	) {
		dbus_pending_call_cancel(pending_status);
		return -1;
	}

	connman_device_ref(device);

	return 0;
}

static void
scan_finished_callback(
    DBusPendingCall *pending, void* user_data
    )
{
	int32_t ret = 0;
	struct connman_device *device = (struct connman_device *)user_data;
	struct lowpan_device_s *device_info = (struct lowpan_device_s *)connman_device_get_data(device);
	DBusMessage* reply = dbus_pending_call_steal_reply(pending);
	DBusMessageIter iter;
	DBusMessageIter list_iter;

	DBG("%p SCAN CALLBACK", device);

	if (!reply) {
		DBG("%p Scan reply was empty?", device);
		ret = -1;
		goto bail;
	}

	dbus_message_iter_init(reply, &iter);
	dump_info_from_iter(stderr, &iter, 1, false);

	if(dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_STRING) {
		char* cstr = NULL;
		dbus_message_iter_get_basic(&iter,&cstr);
		DBG("%p Scan failed: %s", device, cstr);
		ret = -1;
		connman_device_reset_scanning(device);
		goto bail;
	}


	// Get return code
	dbus_message_iter_get_basic(&iter, &ret);

	if (ret) {
		DBG("%p Scan failed: %d", device, ret);
		connman_device_reset_scanning(device);
		goto bail;
	}

	// Move to the list of networks.
	dbus_message_iter_next(&iter);

	if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_ARRAY) {
		ret = -1;
		goto bail;
	}

	dbus_message_iter_recurse(&iter, &list_iter);

	for (;
	     dbus_message_iter_get_arg_type(&list_iter) == DBUS_TYPE_ARRAY;
	     dbus_message_iter_next(&list_iter)) {
		struct connman_network *network = NULL;
		char network_name[128];

		network = get_network_from_iter(device, &list_iter);

		if(network == NULL)
			continue;

		connman_network_set_available(network, TRUE);
		connman_network_update(network);
	}


bail:
	if (reply) {
		dbus_message_unref(reply);
	}
	if (pending) {
		dbus_pending_call_unref(pending);
	}

	if ((device_info != NULL) && (device_info->current_network != NULL)) {
		// Always make sure that the current network is marked as
		// available, so that we don't end up accidentally
		// disconnecting from it.
		connman_network_set_available(device_info->current_network, TRUE);
	}

	connman_device_set_scanning(device, CONNMAN_SERVICE_TYPE_LOWPAN, FALSE);
	return;
}

static int
lowpan_device_scan(enum connman_service_type type,
    struct connman_device *device,
    const char *ssid, unsigned int ssid_len,
    const char *identity, const char* passphrase,
    const char *security, void* user_data
    )
{
	int status = 0;
	DBusPendingCall *pending_scan = NULL;
	DBusMessage *message = NULL;
	char *dbus_path = NULL;
	struct lowpan_device_s *device_info =
	    (struct lowpan_device_s *)connman_device_get_data(device);

	DBG("%p ssid=%s, id=%s, passphrate=%s", device, ssid, identity, passphrase);

	require_action(
		connman_device_get_scanning(device) == FALSE,
		bail,
		status = -EALREADY
	);

	if (device_info->current_network != NULL) {
		require_action(
			connman_network_get_associating(device_info->current_network) == FALSE,
			bail,
			status = -EBUSY
		);
	}

	require_action(device_info->ncp_state != NCP_STATE_ASSOCIATING, bail, status = -EBUSY);
	require_action(device_info->ncp_state != NCP_STATE_COMMISSIONED, bail, status = -EBUSY);
	require_action(!ncp_state_is_initializing(device_info->ncp_state), bail, status = -EBUSY);

	asprintf(&dbus_path,
	         "%s/%s",
	         WPAN_TUNNEL_DBUS_PATH,
	         connman_device_get_ident(device));

	message = dbus_message_new_method_call(
	    WPAN_TUNNEL_DBUS_NAME,
	    dbus_path,
	    WPAN_TUNNEL_DBUS_INTERFACE,
	    WPAN_IFACE_CMD_SCAN
	);

	require_action(message != NULL, bail, status = -ENOMEM);

	require_action(message != NULL, bail, status = -ENOMEM);

	require_action(
		dbus_connection_send_with_reply(
	        connection,
	        message,
	        &pending_scan,
	        45000 // Timeout, in ms
		) == TRUE,
		bail,
		status = -EIO
	);

	require_action(
		dbus_pending_call_set_notify(
	        pending_scan,
	        &scan_finished_callback,
	        (void*)device,
	        (DBusFreeFunction) & scan_free_callback
		) == TRUE,
		bail,
		status = -EIO
	);

	pending_scan = NULL;
	connman_device_ref(device);
	connman_device_set_scanning(device, CONNMAN_SERVICE_TYPE_LOWPAN, TRUE);

bail:
	free(dbus_path);

	if (pending_scan != NULL) {
		dbus_pending_call_cancel(pending_scan);
	}

	if (message != NULL) {
		dbus_message_unref(message);
	}
	return status;
}


static int lowpan_device_set_regdom(
    struct connman_device *device, const char *alpha2
    )
{
	DBG("%p", device);
	connman_device_regdom_notify(device, 0, alpha2);
	return 0;
}

static struct connman_device_driver lowpan_device_driver = {
	.name           = "lowpan",
	.type           = CONNMAN_DEVICE_TYPE_LOWPAN,
	.priority       = CONNMAN_DEVICE_PRIORITY_LOW,
	.probe          = lowpan_device_probe,
	.remove         = lowpan_device_remove,
	.enable         = lowpan_device_enable,
	.disable        = lowpan_device_disable,
	.scan           = lowpan_device_scan,
	.set_regdom = lowpan_device_set_regdom,
};

static struct connman_device*
lowpan_device_create(const char* interface_name)
{
	struct connman_device *device = NULL;

	device = g_hash_table_lookup(devices, interface_name);
	if (!device) {
		struct lowpan_device_s *device_info =
		    calloc(1, sizeof(struct lowpan_device_s));

		device = connman_device_create(
		    "lowpan",
		    lowpan_device_driver.type
		    );

		connman_device_set_data(device, device_info);

		g_hash_table_replace(devices, g_strdup(interface_name), device);

		connman_device_set_index(device, connman_inet_ifindex(interface_name));
		connman_device_set_ident(device, interface_name);
		connman_device_set_interface(device, interface_name);

		if(connman_device_register(device)<0) {
			g_hash_table_remove(devices, device);
			device = NULL;
		}
		DBG("device created: %p", device);
	}

	if(device)
		lowpan_device_update_status(device);

	return device;
}

static void
lowpan_device_finalize(gpointer data)
{
	struct connman_device *device = data;
	struct lowpan_device_s *device_info =
	    (struct lowpan_device_s *)connman_device_get_data(device);

	DBG("%p %s", device, connman_device_get_ident(device));
	if (device_info) {
		if (device_info->current_network) {
			connman_network_set_connected(device_info->current_network, FALSE);
		}

		connman_device_remove_network(device, device_info->current_network);

		lowpan_device_set_network(device, NULL);

		free(device_info);
		connman_device_set_data(device, NULL);

		connman_device_unregister(device);

		connman_device_set_ident(device, "X");
		connman_device_set_interface(device, "X");

		connman_device_unref(device);
	}
}

/* -------------------------------------------------------------------------- */
// MARK: - LoWPAN Technology

static int
lowpan_tech_probe(struct connman_technology *technology)
{
	DBG("%s: %p", __FUNCTION__, technology);
	lowpan_tech = technology;

	return 0;
}

static void
lowpan_tech_remove(struct connman_technology *technology)
{
	DBG("");
	lowpan_tech = NULL;
}

static int
lowpan_tech_set_regdom(
    struct connman_technology *technology, const char *alpha2
    )
{
	return 0;
}

static int
lowpan_tech_set_tethering(
    struct connman_technology *technology,
    const char *identifier, const char *passphrase,
    const char *bridge, bool enabled
    )
{
	return 0;
}

static struct connman_technology_driver lowpan_tech_driver = {
	.name           = "lowpan",
	.type           = CONNMAN_SERVICE_TYPE_LOWPAN,
	.probe          = lowpan_tech_probe,
	.remove         = lowpan_tech_remove,
	.set_regdom = lowpan_tech_set_regdom,
};

/* -------------------------------------------------------------------------- */
// MARK: - LoWPAN DBus

static void
lowpan_dbus_init_interfaces(void)
{
	DBusMessage *message = NULL;
	DBusMessage *reply = NULL;
	DBusError error = DBUS_ERROR_INIT;
	DBusMessageIter iter;
	DBusMessageIter list_iter;

	DBG("%s", __FUNCTION__);

	message = dbus_message_new_method_call(
	    WPAN_TUNNEL_DBUS_NAME,
	    WPAN_TUNNEL_DBUS_PATH,
	    WPAN_TUNNEL_DBUS_INTERFACE,
	    WPAN_TUNNEL_CMD_GET_INTERFACES
	    );

	if (!message) {
		DBG("%s: Unable to create dbus message.", __FUNCTION__);
		goto bail;
	}

	reply = dbus_connection_send_with_reply_and_block(
	    connection,
	    message,
	    5000,
	    &error
	    );

	if (!reply) {
		DBG("%s: DBus call to GetInterfaces failed: %s", __FUNCTION__, error.message);
		goto bail;
	}

	dbus_message_iter_init(reply, &iter);

	if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_ARRAY) {
		DBG("%s: Bad return type for GetInterfaces", __FUNCTION__);
		goto bail;
	}

	dbus_message_iter_recurse(&iter, &list_iter);

	for (;
	     dbus_message_iter_get_arg_type(&list_iter) != DBUS_TYPE_INVALID;
	     dbus_message_iter_next(&list_iter)) {
		DBusMessageIter tmp_iter;
		DBusMessageIter *this_iter = &list_iter;
		char *interface_name = NULL;

		if (dbus_message_iter_get_arg_type(this_iter) == DBUS_TYPE_ARRAY) {
			dbus_message_iter_recurse(this_iter, &tmp_iter);
			this_iter = &tmp_iter;
		}

		if (dbus_message_iter_get_arg_type(this_iter) == DBUS_TYPE_STRING) {
			dbus_message_iter_get_basic(this_iter, &interface_name);
		}

		if (interface_name != NULL) {
			DBG("%s: Interface: \"%s\"", __FUNCTION__, interface_name);
			lowpan_device_create(interface_name);
		} else {
			DBG("%s: Unable to extract interface name", __FUNCTION__);
		}
	}

bail:
	if (message)
		dbus_message_unref(message);

	if (reply)
		dbus_message_unref(reply);

	dbus_error_free(&error);
}

DBusHandlerResult
lowpan_signal_handler(
    DBusConnection *connection,
    DBusMessage *   message,
    void *                  user_data
    )
{
	DBusHandlerResult ret = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	if (dbus_message_is_signal(message, WPAN_TUNNEL_DBUS_INTERFACE,
	                           WPAN_TUNNEL_SIGNAL_INTERFACE_ADDED)) {
		const char* interface_name = NULL;
		DBG("%s", dbus_message_get_path(message));

		dbus_message_get_args(
		    message, NULL,
		    DBUS_TYPE_STRING, &interface_name,
		    DBUS_TYPE_INVALID
		    );

		DBG("%s: InterfaceAdded: %s", __FUNCTION__, interface_name);
		lowpan_device_create(interface_name);

		ret = DBUS_HANDLER_RESULT_HANDLED;
	} else if (dbus_message_is_signal(message, WPAN_TUNNEL_DBUS_INTERFACE,
	                                  WPAN_TUNNEL_SIGNAL_INTERFACE_REMOVED)) {
		const char* interface_name = NULL;
		DBG("%s", dbus_message_get_path(message));
		dbus_message_get_args(
		    message, NULL,
		    DBUS_TYPE_STRING, &interface_name,
		    DBUS_TYPE_INVALID
		    );

		DBG("%s: InterfaceRemoved: %s", __FUNCTION__, interface_name);

		struct connman_device *device = g_hash_table_lookup(devices, interface_name);

		g_hash_table_remove(devices, device);

		ret = DBUS_HANDLER_RESULT_HANDLED;
	} else if (dbus_message_is_signal(message, DBUS_INTERFACE_DBUS,
	                                  "NameOwnerChanged")) {
		const char* name = NULL;
		const char* oldOwner = NULL;
		const char* newOwner = NULL;

		dbus_message_get_args(
		    message, NULL,
		    DBUS_TYPE_STRING, &name,
		    DBUS_TYPE_STRING, &oldOwner,
		    DBUS_TYPE_STRING, &newOwner,
		    DBUS_TYPE_INVALID
		    );
		if (name && (0 == strcmp(name, WPAN_TUNNEL_DBUS_INTERFACE))) {
			if(newOwner[0]!=0 && oldOwner[0]==0) {
				DBG("%s is now ONLINE: \"%s\" (was \"%s\")",name, newOwner, oldOwner);
			}
			if(newOwner[0]!=0) {
				lowpan_dbus_init_interfaces();
			}
			if(oldOwner[0]!=0 && newOwner[0]==0) {
				DBG("%s is now OFFLINE: \"%s\" (was \"%s\")",name, newOwner, oldOwner);
				g_hash_table_remove_all(devices);
			}
		}
	}

	return ret;
}


static int
lowpan_dbus_init(void)
{
	int ret = 0;
	DBG("%s", __FUNCTION__);

	// TODO: Add free function!
	if (dbus_connection_add_filter(connection, lowpan_signal_handler, NULL, NULL) == FALSE) {
		ret = -EIO;
		goto bail;
	}

	static const char *lowpan_dbus_rule0 = "type=signal,"
	                                       "path=" DBUS_PATH_DBUS ","
	                                       "sender=" DBUS_SERVICE_DBUS ","
	                                       "interface=" DBUS_INTERFACE_DBUS ","
	                                       "member=NameOwnerChanged,"
	                                       "arg0=" WPAN_TUNNEL_DBUS_NAME;
	static const char *lowpan_dbus_rule1 = "type=signal,"
	                                       "interface=" WPAN_TUNNEL_DBUS_INTERFACE;

	dbus_bus_add_match(connection, lowpan_dbus_rule0, NULL);
	dbus_bus_add_match(connection, lowpan_dbus_rule1, NULL);

	lowpan_dbus_init_interfaces();

bail:
	return ret;
}

/* -------------------------------------------------------------------------- */
// MARK: - LoWPAN Plugin

static int
lowpan_tunnel_init(void)
{
	DBG("%s", __FUNCTION__);

	int err = -1;

	connection = connman_dbus_get_connection();
	if (connection == NULL) {
		DBG("%s: No DBUS connection...?", __FUNCTION__);
		err = -EIO;

		goto out;
	}

	devices = g_hash_table_new_full(g_str_hash,
	                                g_str_equal,
	                                g_free,
	                                lowpan_device_finalize);

	err = connman_technology_driver_register(&lowpan_tech_driver);
	if (err < 0)
		goto out;

	err = connman_network_driver_register(&lowpan_network_driver);
	if (err < 0) {
		connman_technology_driver_unregister(&lowpan_tech_driver);
		goto out;
	}

	err = connman_device_driver_register(&lowpan_device_driver);
	if (err < 0) {
		connman_network_driver_unregister(&lowpan_network_driver);
		connman_technology_driver_unregister(&lowpan_tech_driver);
		goto out;
	}

	err = lowpan_dbus_init();
	if (err < 0) {
		connman_device_driver_unregister(&lowpan_device_driver);
		connman_network_driver_unregister(&lowpan_network_driver);
		connman_technology_driver_unregister(&lowpan_tech_driver);
		goto out;
	}

#if 0


	err = connman_device_driver_register(&ethernet_driver);
	if (err < 0) {
		connman_network_driver_unregister(&cable_driver);
		goto out;
	}

	err = connman_technology_driver_register(&tech_driver);
	if (err < 0) {
		connman_device_driver_unregister(&ethernet_driver);
		connman_network_driver_unregister(&cable_driver);
		goto out;
	}
#endif

	return 0;
out:

	return err;
}

static void
lowpan_tunnel_exit(void)
{
	DBG("%s", __FUNCTION__);

	connman_network_driver_unregister(&lowpan_network_driver);

	connman_technology_driver_unregister(&lowpan_tech_driver);

	connman_device_driver_unregister(&lowpan_device_driver);

#if 0
	connman_technology_driver_unregister(&tech_driver);

	connman_network_driver_unregister(&cable_driver);

	connman_device_driver_unregister(&ethernet_driver);
#endif
}

CONNMAN_PLUGIN_DEFINE(
    lowpan_tunnel,
    "LoWPAN tunnel plugin",
    CONNMAN_VERSION,
    CONNMAN_PLUGIN_PRIORITY_DEFAULT,
    lowpan_tunnel_init,
    lowpan_tunnel_exit
    )
