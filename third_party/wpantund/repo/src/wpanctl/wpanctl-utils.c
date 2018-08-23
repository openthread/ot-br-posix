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

#include "assert-macros.h"
#include "string-utils.h"
#include "wpanctl-utils.h"
#include "wpan-dbus-v0.h"
#include "wpan-dbus-v1.h"
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <arpa/inet.h>

#ifdef __APPLE__
char gInterfaceName[32] = "utun2";
#else
char gInterfaceName[32] = "wpan0";
#endif
int gRet = 0;

void dump_info_from_iter(FILE* file, DBusMessageIter *iter, int indent, bool bare, bool indentFirstLine)
{
	DBusMessageIter sub_iter;
	int i;

	if (!bare && indentFirstLine) for (i = 0; i < indent; i++) fprintf(file, "\t");

	switch (dbus_message_iter_get_arg_type(iter)) {
	case DBUS_TYPE_DICT_ENTRY:
		dbus_message_iter_recurse(iter, &sub_iter);
		dump_info_from_iter(file, &sub_iter, indent + 1, true, false);
		fprintf(file, " => ");
		dbus_message_iter_next(&sub_iter);
		dump_info_from_iter(file, &sub_iter, indent + 1, bare, false);
		bare = true;
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
		     dbus_message_iter_next(&sub_iter)
		) {
			dump_info_from_iter(file,
			                    &sub_iter,
			                    indent + 1,
			                    dbus_message_iter_get_arg_type(&sub_iter) == DBUS_TYPE_BYTE,
								true);
		}
		for (i = 0; i < indent; i++) fprintf(file, "\t");
		fprintf(file, "]");

		break;
	case DBUS_TYPE_VARIANT:
		dbus_message_iter_recurse(iter, &sub_iter);
		dump_info_from_iter(file, &sub_iter, indent, bare, false);
		bare = true;
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
		if (!bare) {
			fprintf(file, "0x%02X", v);
		} else {
			fprintf(file, "%02X", v);
		}
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

int parse_network_info_from_iter(struct wpan_network_info_s *network_info, DBusMessageIter *iter)
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
			fprintf(stderr,
			        "error: Bad type for network (%c)\n",
			        dbus_message_iter_get_arg_type(iter));
			ret = ERRORCODE_UNKNOWN;
			goto bail;
		}

		dbus_message_iter_recurse(iter, &dict_iter);

		if (dbus_message_iter_get_arg_type(&dict_iter) != DBUS_TYPE_STRING) {
			fprintf(stderr,
			        "error: Bad type for network list (%c)\n",
			        dbus_message_iter_get_arg_type(&dict_iter));
			ret = ERRORCODE_UNKNOWN;
			goto bail;
		}

		// Get the key
		dbus_message_iter_get_basic(&dict_iter, &key);
		dbus_message_iter_next(&dict_iter);

		if (dbus_message_iter_get_arg_type(&dict_iter) != DBUS_TYPE_VARIANT) {
			fprintf(stderr,
			        "error: Bad type for network list (%c)\n",
			        dbus_message_iter_get_arg_type(&dict_iter));
			ret = ERRORCODE_UNKNOWN;
			goto bail;
		}

		dbus_message_iter_recurse(&dict_iter, &value_iter);

		if (strcmp(key, kWPANTUNDProperty_NetworkName) == 0) {
			char* network_name = NULL;
			dbus_message_iter_get_basic(&value_iter, &network_name);
			snprintf(network_info->network_name,
			         sizeof(network_info->network_name), "%s", network_name);
		} else if (strcmp(key, kWPANTUNDProperty_NCPChannel) == 0) {
			dbus_message_iter_get_basic(&value_iter, &network_info->channel);
		} else if (strcmp(key, kWPANTUNDProperty_NetworkPANID) == 0) {
			dbus_message_iter_get_basic(&value_iter, &network_info->pan_id);
		} else if (strcmp(key, kWPANTUNDProperty_NestLabs_NetworkAllowingJoin) == 0) {
			dbus_message_iter_get_basic(&value_iter,
			                            &network_info->allowing_join);
		} else if (strcmp(key, "RSSI") == 0) {
			dbus_message_iter_get_basic(&value_iter, &network_info->rssi);
		} else if (strcmp(key, kWPANTUNDProperty_NetworkXPANID) == 0) {
			dbus_message_iter_get_basic(&value_iter, &network_info->xpanid);
		} else if (strcmp(key, kWPANTUNDProperty_NetworkNodeType) == 0) {
			dbus_message_iter_get_basic(&value_iter, &network_info->type);
		} else if (strcmp(key, kWPANTUNDProperty_NCPHardwareAddress) == 0) {
			DBusMessageIter sub_iter;
			dbus_message_iter_recurse(&value_iter, &sub_iter);
			if (dbus_message_iter_get_arg_type(&sub_iter) == DBUS_TYPE_BYTE) {
				const uint8_t* value = NULL;
				int nelements = 0;
				dbus_message_iter_get_fixed_array(&sub_iter, &value,
				                                  &nelements);
				if (nelements == 8)
					memcpy(network_info->hwaddr, value, nelements);
			}
		} else {
#if DEBUG
			fprintf(stderr,
			        "info: %s -> (%c)\n",
			        key,
			        dbus_message_iter_get_arg_type(&value_iter));
#endif
		}
	}

bail:
	if (ret) fprintf(stderr, "Network parse failed.\n");
	return ret;
}

int parse_energy_scan_result_from_iter(int16_t *channel, int8_t *maxRssi, DBusMessageIter *iter)
{
	int ret = 0;
	DBusMessageIter outer_iter;
	DBusMessageIter dict_iter;

	if (dbus_message_iter_get_arg_type(iter) == DBUS_TYPE_ARRAY) {
		dbus_message_iter_recurse(iter, &outer_iter);
		iter = &outer_iter;
	}

	*channel = 0;
	*maxRssi = 0;

	for (;
	     dbus_message_iter_get_arg_type(iter) != DBUS_TYPE_INVALID;
	     dbus_message_iter_next(iter)) {
		DBusMessageIter value_iter;
		char* key;

		if (dbus_message_iter_get_arg_type(iter) != DBUS_TYPE_DICT_ENTRY) {
			fprintf(stderr,
			        "error: Bad type for energy scan result (%c)\n",
			        dbus_message_iter_get_arg_type(iter));
			ret = ERRORCODE_UNKNOWN;
			goto bail;
		}

		dbus_message_iter_recurse(iter, &dict_iter);

		if (dbus_message_iter_get_arg_type(&dict_iter) != DBUS_TYPE_STRING) {
			fprintf(stderr,
			        "error: Bad type for energy scan result (%c)\n",
			        dbus_message_iter_get_arg_type(&dict_iter));
			ret = ERRORCODE_UNKNOWN;
			goto bail;
		}

		// Get the key
		dbus_message_iter_get_basic(&dict_iter, &key);
		dbus_message_iter_next(&dict_iter);

		if (dbus_message_iter_get_arg_type(&dict_iter) != DBUS_TYPE_VARIANT) {
			fprintf(stderr,
			        "error: Bad type for energy scan result (%c)\n",
			        dbus_message_iter_get_arg_type(&dict_iter));
			ret = ERRORCODE_UNKNOWN;
			goto bail;
		}

		dbus_message_iter_recurse(&dict_iter, &value_iter);

		if (strcmp(key, kWPANTUNDProperty_NCPChannel) == 0) {
			dbus_message_iter_get_basic(&value_iter, channel);
		} else if (strcmp(key, "RSSI") == 0) {
			dbus_message_iter_get_basic(&value_iter, maxRssi);
		} else {
#if DEBUG
			fprintf(stderr,
			        "info: %s -> (%c)\n",
			        key,
			        dbus_message_iter_get_arg_type(&value_iter));
#endif
		}
	}

bail:
	if (ret) fprintf(stderr, "Energy scan result parse failed.\n");
	return ret;
}

int
parse_prefix(const char *prefix_str, uint8_t *prefix)
{
	int ret = ERRORCODE_OK;

	// The prefix could either be specified like an IPv6 address, or
	// specified as a bunch of hex numbers. Presence of a colon (':')
	// is used to differentiate.

	if (strstr(prefix_str,":")) {
		uint8_t address[INET6_ADDRSTRLEN];

		int bits = inet_pton(AF_INET6, prefix_str, address);

		if (bits < 0) {
			ret = ERRORCODE_BADARG;
			goto bail;
		}

		memcpy(prefix, address, WPANCTL_PREFIX_SIZE);

	} else {

		if (parse_string_into_data(prefix, WPANCTL_PREFIX_SIZE, prefix_str) <= 0) {
			ret = ERRORCODE_BADARG;
		}
	}

bail:
	return ret;
}

const char *
parse_node_type(const char *type_str)
{
	const char *node_type = kWPANTUNDNodeType_Unknown;

	if (!strcasecmp(type_str, "router")
		|| !strcasecmp(type_str, "r")
		|| !strcasecmp(type_str, "2")
		|| !strcasecmp(type_str, kWPANTUNDNodeType_Router)
		|| !strcasecmp(type_str, kWPANTUNDNodeType_Leader)
		|| !strcasecmp(type_str, kWPANTUNDNodeType_Commissioner)
	) {
		node_type = kWPANTUNDNodeType_Router;

	} else if (!strcasecmp(type_str, "end-device")
		|| !strcasecmp(type_str, "enddevice")
		|| !strcasecmp(type_str, "end")
		|| !strcasecmp(type_str, "ed")
		|| !strcasecmp(type_str, "e")
		|| !strcasecmp(type_str, "3")
		|| !strcasecmp(type_str, kWPANTUNDNodeType_EndDevice)
	) {
		node_type = kWPANTUNDNodeType_EndDevice;

	} else if (!strcasecmp(type_str, "sleepy-end-device")
		|| !strcasecmp(type_str, "sleepy")
		|| !strcasecmp(type_str, "sed")
		|| !strcasecmp(type_str, "s")
		|| !strcasecmp(type_str, "4")
		|| !strcasecmp(type_str, kWPANTUNDNodeType_SleepyEndDevice)
	) {
		node_type = kWPANTUNDNodeType_SleepyEndDevice;

	} else if (!strcasecmp(type_str, "lurker")
		|| !strcasecmp(type_str, "nl-lurker")
		|| !strcasecmp(type_str, "l")
		|| !strcasecmp(type_str, "6")
		|| !strcasecmp(type_str, kWPANTUNDNodeType_NestLurker)
	) {
		node_type = kWPANTUNDNodeType_NestLurker;
	}

	return node_type;
}

int
lookup_dbus_name_from_interface(char* dbus_bus_name, const char* interface_name)
{
	int ret = kWPANTUNDStatus_InterfaceNotFound;
	int timeout = DEFAULT_TIMEOUT_IN_SECONDS * 1000;
	DBusConnection* connection = NULL;
	DBusMessage *message = NULL;
	DBusMessage *reply = NULL;
	DBusError error;

	dbus_error_init(&error);

	memset(dbus_bus_name, 0, DBUS_MAXIMUM_NAME_LENGTH+1);

	connection = dbus_bus_get(DBUS_BUS_SYSTEM, &error);

	require_string(connection != NULL, bail, error.message);

	{
		DBusMessageIter iter;
		DBusMessageIter list_iter;

		message = dbus_message_new_method_call(
		    WPAN_TUNNEL_DBUS_NAME,
		    WPAN_TUNNEL_DBUS_PATH,
		    WPAN_TUNNEL_DBUS_INTERFACE,
		    WPAN_TUNNEL_CMD_GET_INTERFACES
		    );

		reply = dbus_connection_send_with_reply_and_block(
		    connection,
		    message,
		    timeout,
		    &error
		    );

		if (!reply) {
			fprintf(stderr, "%s: error: %s\n", "lookup_dbus_name_from_interface", error.message);
			ret = ERRORCODE_TIMEOUT;
			goto bail;
		}

		dbus_message_iter_init(reply, &iter);

		if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_ARRAY) {
			fprintf(stderr,
			        "%s: error: Bad type for interface list (%c)\n",
			        "lookup_dbus_name_from_interface",
			        dbus_message_iter_get_element_type(&iter));
			ret = ERRORCODE_UNKNOWN;
			goto bail;
		}

		dbus_message_iter_recurse(&iter, &list_iter);

		for (;
		     dbus_message_iter_get_arg_type(&list_iter) == DBUS_TYPE_ARRAY;
		     dbus_message_iter_next(&list_iter)
		) {
			DBusMessageIter item_iter;
			char *item_interface_name = NULL;
			char *item_dbus_name = NULL;

			dbus_message_iter_recurse(&list_iter, &item_iter);

			dbus_message_iter_get_basic(&item_iter, &item_interface_name);
			dbus_message_iter_next(&item_iter);
			dbus_message_iter_get_basic(&item_iter, &item_dbus_name);
			if ((NULL != item_interface_name)
				&& (NULL != item_dbus_name)
				&& (strcmp(item_interface_name, interface_name) == 0)
			) {
				strncpy(dbus_bus_name, item_dbus_name, DBUS_MAXIMUM_NAME_LENGTH);
				ret = 0;
				break;
			}
		}
	}

bail:

	if (connection)
		dbus_connection_unref(connection);

	if (message)
		dbus_message_unref(message);

	if (reply)
		dbus_message_unref(reply);

	dbus_error_free(&error);

	return ret;
}

void print_error_diagnosis(int error)
{
	switch(error) {
	case kWPANTUNDStatus_InterfaceNotFound:
		fprintf(stderr, "\nDIAGNOSIS: The requested operation can't be completed because the given\n"
						"network interface doesn't exist or it isn't managed by wpantund. If you are\n"
						"using wpanctl in interactive mode, you can use the `ls` command to get a list\n"
						"of valid interfaces and use the `cd` command to select a valid interface.\n"
						"Otherwise, use the `-I` argument to wpanctl to select a valid interface.\n"
						"\n"
		);
		break;

	case kWPANTUNDStatus_Busy:
	case -EBUSY:
		fprintf(stderr, "\nDIAGNOSIS: The requested operation can't be completed because the NCP\n"
						"is busy doing something else, like scanning or joining. If you are persistently\n"
						"getting this error, try resetting the NCP via the \"reset\" command. You can\n"
						"help diagnose why this is occuring using the \"state\" command.\n"
						"\n"
		);
		break;


	case kWPANTUNDStatus_Canceled:
	case -ECONNABORTED:
		fprintf(stderr, "\nDIAGNOSIS: This action was aborted due to a change in the NCP's state.\n"
						"This can occur if the interface is disabled while you were trying to join,\n"
						"or if AutoDeepSleep kicked in for some reason.\n"
						"\n"
		);
		break;

	case kWPANTUNDStatus_NCP_Crashed:
	case -ECONNRESET:
		fprintf(stderr, "\nDIAGNOSIS: The NCP has unexpectedly crashed and rebooted. Please see the\n"
						"wpantund logs for more information and try again.\n"
						"\n"
		);
		break;

	case kWPANTUNDStatus_InvalidArgument:
	case -EINVAL:
		fprintf(stderr, "\nDIAGNOSIS: This error indicates that either the device in a state where your\n"
						"request makes no sense or the parameters of your request were invalid. Check your\n"
						"arguments and verify that you are allowed to perform the given operation when the\n"
						"NCP is in its current state.\n"
						"\n"
		);
		break;

	case kWPANTUNDStatus_InvalidWhenDisabled:
		fprintf(stderr, "\nDIAGNOSIS: This error indicates that this operation is not valid when the interface\n"
						"is disabled. Enable the interface first and try again. You can enable the interface\n"
						"with the command `setprop enabled true`.\n"
						"\n"
		);
		break;

	case kWPANTUNDStatus_InvalidForCurrentState:
	case kWPANTUNDStatus_InProgress:
	case -EALREADY:
		fprintf(stderr, "\nDIAGNOSIS: This error indicates that the device is not in a state where\n"
						"it can complete your request, typically because a request is already in progress or\n"
						"the NCP is already in the requested state.\n"
						"If you are getting this error persistently, you should try reseting the network\n"
						"settings on the NCP (via the \"leave\" command). The \"status\" command can be\n"
						"helpful to further diagnose the issue.\n"
						"\n"
		);
		break;

	case kWPANTUNDStatus_JoinFailedAtScan:
		fprintf(stderr, "\nDIAGNOSIS: This error indicates that the NCP could not find a device in\n"
						"range that would allow it to join the given network. This can occur if\n"
						"the closest device on the network you are trying to join is out of range,\n"
						"the devices on the network you are trying to join are running an\n"
						"incompatible network stack, or if there are no devices on the target\n"
						"network which are permitting joining.\n"
						"\n"
		);
		break;

	case kWPANTUNDStatus_JoinFailedAtAuthenticate:
		fprintf(stderr, "\nDIAGNOSIS: Join failed while authenticating. This is typically due to using the wrong\n"
		                "key or because this NCP's network stack is not compatible with this network.\n"
						"\n"
		);
		break;
	}

	if (WPANTUND_STATUS_IS_NCPERROR(error)) {
		fprintf(stderr, "\nDIAGNOSIS: This error is specific to this specific type of NCP. The error\n"
		                "code is %d (0x%02X). Consult the NCP documentation for an explantion of this\n"
						"error code.\n"
						"\n", WPANTUND_STATUS_TO_NCPERROR(error), WPANTUND_STATUS_TO_NCPERROR(error)
		);
	}
}

uint16_t
node_type_str2int(const char *node_type)
{
	if (strcasecmp(node_type, "router") == 0) {
		return WPAN_IFACE_ROLE_ROUTER;
	} else if (strcasecmp(node_type, "r") == 0) {
		return WPAN_IFACE_ROLE_ROUTER;

	} else if (strcasecmp(node_type, "end-device") == 0) {
		return WPAN_IFACE_ROLE_END_DEVICE;
	} else if (strcasecmp(node_type, "end") == 0) {
		return WPAN_IFACE_ROLE_END_DEVICE;
	} else if (strcasecmp(node_type, "e") == 0) {
		return WPAN_IFACE_ROLE_END_DEVICE;

	} else if (strcasecmp(node_type, "sleepy-end-device") == 0) {
		return WPAN_IFACE_ROLE_SLEEPY_END_DEVICE;
	} else if (strcasecmp(node_type, "sleepy") == 0) {
		return WPAN_IFACE_ROLE_SLEEPY_END_DEVICE;
	} else if (strcasecmp(node_type, "sed") == 0) {
		return WPAN_IFACE_ROLE_SLEEPY_END_DEVICE;
	} else if (strcasecmp(node_type, "s") == 0) {
		return WPAN_IFACE_ROLE_SLEEPY_END_DEVICE;

	} else if (strcasecmp(node_type, "lurker") == 0) {
		return WPAN_IFACE_ROLE_LURKER;
	} else if (strcasecmp(node_type, "nl-lurker") == 0) {
		return WPAN_IFACE_ROLE_LURKER;
	} else if (strcasecmp(node_type, "l") == 0) {
		return WPAN_IFACE_ROLE_LURKER;

	} else {
		// At this moment it should be a number
		return strtol(node_type, NULL, 0);
	}
}

const char *
node_type_int2str(uint16_t node_type)
{
	switch (node_type)
	{
	case WPAN_IFACE_ROLE_ROUTER:            return "router";
	case WPAN_IFACE_ROLE_END_DEVICE:        return "end-device";
	case WPAN_IFACE_ROLE_SLEEPY_END_DEVICE: return "sleepy-end-device";
	case WPAN_IFACE_ROLE_LURKER:            return "nl-lurker";
	default: break;
	}

	return "unknown";
}

const char *
joiner_state_int2str(uint8_t state)
{
    const char *ret = "idle";

    switch (state)
    {
    case JOINER_STATE_IDLE:
        ret = "idle";
        break;

    case JOINER_STATE_DISCOVER:
        ret = "discover";
        break;

    case JOINER_STATE_CONNECT:
        ret = "connect";
        break;

    case JOINER_STATE_CONNECTED:
        ret = "connected";
        break;

    case JOINER_STATE_ENTRUST:
        ret = "entrust";
        break;

    case JOINER_STATE_JOINED:
        ret = "joined";
        break;

    default:
        break;
    }

    return ret;
}

int
create_new_wpan_dbus_message(DBusMessage **message, const char *dbus_command)
{
	int ret = 0;
	char path[DBUS_MAXIMUM_NAME_LENGTH + 1];
	char interface_dbus_name[DBUS_MAXIMUM_NAME_LENGTH + 1];

	assert(*message == NULL);

	ret = lookup_dbus_name_from_interface(interface_dbus_name, gInterfaceName);
	require_quiet(ret == 0, bail);

	snprintf(path, sizeof(path), "%s/%s", WPANTUND_DBUS_PATH, gInterfaceName);
	*message = dbus_message_new_method_call(interface_dbus_name, path, WPANTUND_DBUS_APIv1_INTERFACE, dbus_command);

	if (*message == NULL) {
		ret = ERRORCODE_ALLOC;
	}

bail:
	return ret;
}

void
append_dbus_dict_entry_basic(DBusMessageIter *dict_iter, const char *key, char dbus_basic_type, void *value)
{
	DBusMessageIter entry_iter;
	DBusMessageIter value_iter;
	char dbus_signature[2] = { dbus_basic_type, '\0' };

	dbus_message_iter_open_container(dict_iter, DBUS_TYPE_DICT_ENTRY, NULL, &entry_iter);

	// Append dictionary key as String
	dbus_message_iter_append_basic(&entry_iter, DBUS_TYPE_STRING, &key);

	// Append dictionary value as Variant with the given basic type
	dbus_message_iter_open_container(&entry_iter, DBUS_TYPE_VARIANT, dbus_signature, &value_iter);
	dbus_message_iter_append_basic(&value_iter, dbus_basic_type, value);
	dbus_message_iter_close_container(&entry_iter, &value_iter);

	dbus_message_iter_close_container(dict_iter, &entry_iter);
}

void
append_dbus_dict_entry_byte_array(DBusMessageIter *dict_iter, const char *key, const uint8_t *data, int data_len)
{
	DBusMessageIter entry_iter;
	DBusMessageIter value_iter;
	DBusMessageIter array_iter;

	dbus_message_iter_open_container(dict_iter, DBUS_TYPE_DICT_ENTRY, NULL, &entry_iter);

	// Append dictionary key as String
	dbus_message_iter_append_basic(&entry_iter, DBUS_TYPE_STRING, &key);

	// Append dictionary value as Variant with type Array of Bytes
	dbus_message_iter_open_container(
		&entry_iter,
		DBUS_TYPE_VARIANT,
		DBUS_TYPE_ARRAY_AS_STRING DBUS_TYPE_BYTE_AS_STRING,
		&value_iter
	);

	// Append byte array
	dbus_message_iter_open_container(&value_iter, DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE_AS_STRING, &array_iter);
	dbus_message_iter_append_fixed_array(&array_iter, DBUS_TYPE_BYTE, &data, data_len);
	dbus_message_iter_close_container(&value_iter, &array_iter);

	dbus_message_iter_close_container(&entry_iter, &value_iter);

	dbus_message_iter_close_container(dict_iter, &entry_iter);
}
