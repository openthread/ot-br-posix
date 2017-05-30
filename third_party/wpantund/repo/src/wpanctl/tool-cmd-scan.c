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

#include <getopt.h>
#include "wpanctl-utils.h"
#include "tool-cmd-scan.h"
#include "assert-macros.h"
#include "wpan-dbus-v1.h"
#include "string-utils.h"
#include "args.h"

const char scan_cmd_syntax[] = "[args] [seconds-to-scan]";

static const arg_list_item_t scan_option_list[] = {
	{'h', "help", NULL, "Print Help"},
	{'t', "timeout", "ms", "Set timeout period"},
	{'c', "channel", "channel", "Set the desired channel"},
	{'e', "energy", NULL, "Perform an energy scan"},
	{0}
};

int gScannedNetworkCount = 0;
struct wpan_network_info_s gScannedNetworks[SCANNED_NET_BUFFER_SIZE];

static bool sEnergyScan;

static void
print_scan_header(void)
{
	if (sEnergyScan) {
		printf("    Ch | RSSI\n");
		printf("   ----+-------\n");

		return;
	}

	printf(
		"   | Joinable | NetworkName        | PAN ID | Ch | XPanID           | HWAddr           | RSSI\n");
	printf(
		"---+----------+--------------------+--------+----+------------------+------------------+------\n");

}

static DBusHandlerResult
dbus_beacon_handler(
    DBusConnection *connection,
    DBusMessage *   message,
    void *          user_data
) {
	DBusMessageIter iter;
	int ret;
	struct wpan_network_info_s network_info;

	if (!dbus_message_is_signal(message, WPANTUND_DBUS_APIv1_INTERFACE, WPANTUND_IF_SIGNAL_NET_SCAN_BEACON)) {
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	}

	dbus_message_iter_init(message, &iter);

	ret = parse_network_info_from_iter(&network_info, &iter);

	require_noerr(ret, bail);

	if (network_info.network_name[0]) {
		if (gScannedNetworkCount < SCANNED_NET_BUFFER_SIZE) {
			gScannedNetworks[gScannedNetworkCount++] = network_info;
			printf("%2d", gScannedNetworkCount);
		} else {
			printf("--");  //This means that we cannot act on the PAN as we do not save the info
		}
	} else {
		printf("  ");
	}

	printf(" | %s", network_info.allowing_join ? "     YES" : "      NO");

	if (network_info.network_name[0]) {
		printf(" | \"%s\"%s",
			   network_info.network_name,
			   &"                "[strlen(network_info.network_name)]);
	} else {
		printf(" | ------ NONE ------");
	}

	printf(" | 0x%04X", network_info.pan_id);
	printf(" | %2d", network_info.channel);
	printf(" | %016llX", (unsigned long long)network_info.xpanid);
	printf(" | %02X%02X%02X%02X%02X%02X%02X%02X",
		   network_info.hwaddr[0],
		   network_info.hwaddr[1],
		   network_info.hwaddr[2],
		   network_info.hwaddr[3],
		   network_info.hwaddr[4],
		   network_info.hwaddr[5],
		   network_info.hwaddr[6],
		   network_info.hwaddr[7]);
	printf(" | %4d", network_info.rssi);
	printf("\n");

bail:
	return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult
dbus_energy_scan_handler(
    DBusConnection *connection,
    DBusMessage *   message,
    void *          user_data
) {
	DBusMessageIter iter;
	int ret;
	int16_t channel;
	int8_t maxRssi;

	if (!dbus_message_is_signal(message, WPANTUND_DBUS_APIv1_INTERFACE, WPANTUND_IF_SIGNAL_ENERGY_SCAN_RESULT)) {
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	}

	dbus_message_iter_init(message, &iter);

	ret = parse_energy_scan_result_from_iter(&channel, &maxRssi, &iter);

	require_noerr(ret, bail);

	printf("  %4d | %4d\n", channel, maxRssi);

bail:
	return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult
dbus_signal_handler(
    DBusConnection *connection,
    DBusMessage *   message,
    void *          user_data
) {
	if (sEnergyScan) {
		return dbus_energy_scan_handler(connection, message, user_data);
	}

	return dbus_beacon_handler(connection, message, user_data);
}

static const char gDBusObjectManagerMatchString[] =
	"type='signal'"
//	",interface='" WPANTUND_DBUS_APIv1_INTERFACE "'"
	;


int tool_cmd_scan(int argc, char *argv[])
{
	int ret = 0;
	int c;
	int timeout = DEFAULT_TIMEOUT_IN_SECONDS * 1000;
	DBusConnection* connection = NULL;
	DBusMessage *message = NULL;
	DBusMessage *reply = NULL;
	DBusPendingCall *pending = NULL;
	DBusError error;
	int32_t scan_period = 0;
	uint32_t channel_mask = 0;

	dbus_error_init(&error);

	sEnergyScan = false;

	while (1) {
		static struct option long_options[] = {
			{"help", no_argument, 0, 'h'},
			{"timeout", required_argument, 0, 't'},
			{"channel", required_argument, 0, 'c'},
			{"energy", no_argument, 0, 'e'},
			{0, 0, 0, 0}
		};

		int option_index = 0;
		c = getopt_long(argc, argv, "hc:t:e", long_options,
				&option_index);

		if (c == -1)
			break;

		switch (c) {
		case 'h':
			print_arg_list_help(scan_option_list, argv[0],
					    scan_cmd_syntax);
			ret = ERRORCODE_HELP;
			goto bail;

		case 't':
			timeout = strtol(optarg, NULL, 0);
			break;

		case 'c':
			channel_mask = strtomask_uint32(optarg);
			break;

		case 'e':
			sEnergyScan = true;
			break;
		}
	}

	if (optind < argc) {
		if (scan_period == 0) {
			scan_period = strtol(argv[optind], NULL, 0);
			optind++;
		}
	}

	if (optind < argc) {
			fprintf(stderr,
			        "%s: error: Unexpected extra argument: \"%s\"\n",
			argv[0], argv[optind]);
			ret = ERRORCODE_BADARG;
			goto bail;
		}

	if (gInterfaceName[0] == 0) {
		fprintf(stderr,
		        "%s: error: No WPAN interface set (use the `cd` command, or the `-I` argument for `wpanctl`).\n",
		        argv[0]);
		ret = ERRORCODE_BADARG;
		goto bail;
	}

	connection = dbus_bus_get(DBUS_BUS_STARTER, &error);

	if (!connection) {
		dbus_error_free(&error);
		dbus_error_init(&error);
		connection = dbus_bus_get(DBUS_BUS_SYSTEM, &error);
	}

	require_string(connection != NULL, bail, error.message);

	dbus_bus_add_match(connection, gDBusObjectManagerMatchString, &error);

	require_string(error.name == NULL, bail, error.message);

	dbus_connection_add_filter(connection, &dbus_signal_handler, NULL, NULL);

	{
		char path[DBUS_MAXIMUM_NAME_LENGTH+1];
		char interface_dbus_name[DBUS_MAXIMUM_NAME_LENGTH+1];
		DBusMessageIter iter;
		ret = lookup_dbus_name_from_interface(interface_dbus_name, gInterfaceName);

		if (ret != 0) {
			print_error_diagnosis(ret);
			goto bail;
		}

		snprintf(
			path,
			sizeof(path),
			"%s/%s",
			WPANTUND_DBUS_PATH,
			gInterfaceName
		);

		message = dbus_message_new_method_call(
			interface_dbus_name,
			path,
			WPANTUND_DBUS_APIv1_INTERFACE,
			sEnergyScan? WPANTUND_IF_CMD_ENERGY_SCAN_START : WPANTUND_IF_CMD_NET_SCAN_START
		);

		dbus_message_append_args(
			message,
			DBUS_TYPE_UINT32, &channel_mask,
			DBUS_TYPE_INVALID
		);

		print_scan_header();

		if (!sEnergyScan) {
			gScannedNetworkCount = 0;
		}

		if(!dbus_connection_send_with_reply(
		    connection,
		    message,
			&pending,
		    timeout
	    )) {
			fprintf(stderr, "%s: error: IPC failure\n", argv[0]);
			ret = ERRORCODE_UNKNOWN;
			goto bail;
		}

		while ((dbus_connection_get_dispatch_status(connection) == DBUS_DISPATCH_DATA_REMAINS)
			|| dbus_connection_has_messages_to_send(connection)
			|| !dbus_pending_call_get_completed(pending)
		) {
			dbus_connection_read_write_dispatch(connection, 5000 /*ms*/);
		}

		reply = dbus_pending_call_steal_reply(pending);

		require(reply!=NULL, bail);


		dbus_message_iter_init(reply, &iter);

		if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_INT32) {
			fprintf(stderr, "%s: error: Server returned a bad response ('%c')\n",
			        argv[0], dbus_message_iter_get_arg_type(&iter));
			ret = ERRORCODE_UNKNOWN;
			goto bail;
		}

		// Get return code
		dbus_message_iter_get_basic(&iter, &ret);

		if (ret) {
			fprintf(stderr, "%s failed with error %d. %s\n", argv[0], ret, wpantund_status_to_cstr(ret));
			print_error_diagnosis(ret);
			goto bail;
		}
	}


bail:

	if (reply) {
		dbus_message_unref(reply);
	}

	if (pending != NULL) {
		dbus_pending_call_unref(pending);
	}

	if (message) {
		dbus_message_unref(message);
	}

	if (connection) {
		dbus_bus_remove_match(connection, gDBusObjectManagerMatchString, NULL);
		dbus_connection_remove_filter(connection,&dbus_signal_handler,NULL);
		dbus_connection_unref(connection);
	}

	dbus_error_free(&error);

	return ret;
}
