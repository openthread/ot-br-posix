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

#include <stdlib.h>
#include <getopt.h>
#include "wpanctl-utils.h"
#include "tool-cmd-join.h"
#include "tool-cmd-scan.h"
#include "assert-macros.h"
#include "args.h"
#include "assert-macros.h"
#include "wpan-dbus-v0.h"

#include <errno.h>

const char join_cmd_syntax[] = "[args] [network-name]";

static const arg_list_item_t join_option_list[] = {
	{'h', "help", NULL, "Print Help"},
	{'t', "timeout", "ms", "Set timeout period"},
	{'T', "type", "node-type: router(r,2), end-device(end,e,3), sleepy-end-device(sleepy,sed,4), nl-lurker(lurker,l,6)",
		"Join as a specific node type"},
	{'p', "panid", "panid", "Specify a specific PAN ID"},
	{'x', "xpanid", "xpanid", "Specify a specific Extended PAN ID"},
	{'c', "channel", "channel", "Specify a specific channel"},
	{0}
};

int tool_cmd_join(int argc, char* argv[])
{
	int ret = 0;
	int c;
	int timeout = DEFAULT_TIMEOUT_IN_SECONDS * 1000;
	DBusConnection* connection = NULL;
	DBusMessage *message = NULL;
	DBusMessage *reply = NULL;
	DBusError error;
	uint16_t node_type = WPAN_IFACE_ROLE_END_DEVICE;
	struct wpan_network_info_s target_network = {};

	dbus_error_init(&error);

	while (1) {
		static struct option long_options[] = {
			{"help", no_argument, 0, 'h'},
			{"timeout", required_argument, 0, 't'},
			{"type", required_argument, 0, 'T'},
			{"panid", required_argument, 0, 'p'},
			{"xpanid", required_argument, 0, 'x'},
			{"channel", required_argument, 0, 'c'},
			{0, 0, 0, 0}
		};

		int option_index = 0;
		c = getopt_long(argc, argv, "hc:t:T:x:p:", long_options,
				&option_index);

		if (c == -1)
			break;

		switch (c) {
		case 'h':
			print_arg_list_help(join_option_list, argv[0],
					    join_cmd_syntax);
			ret = ERRORCODE_HELP;
			goto bail;

		case 't':
			timeout = strtol(optarg, NULL, 0);
			break;

		case 'p':
			target_network.pan_id = strtol(optarg, NULL, 16);
			break;

		case 'c':
			target_network.channel = strtol(optarg, NULL, 0);
			break;

		case 'x':
			target_network.xpanid = strtoull(optarg, NULL, 16);
			break;

		case 'T':
			node_type = node_type_str2int(optarg);
			break;
		}
	}

	if (optind < argc) {
		if (!target_network.network_name[0]) {
			int index;
			if (1 == sscanf(argv[optind], "%d",
					&index) && index
			    && (index <= gScannedNetworkCount)) {
				target_network = gScannedNetworks[index - 1];
			} else {
				strncpy(target_network.network_name,
					argv[optind], 16);
				target_network.network_name[16] = 0;
			}
			optind++;
		}
	}

	if (optind < argc) {
		if (!target_network.xpanid) {
			target_network.xpanid = strtoull(argv[optind], NULL, 16);
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

	if (!target_network.network_name[0]) {
		fprintf(stderr, "%s: error: Missing network name.\n", argv[0]);
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

	{
		DBusMessageIter iter;
		DBusMessageIter list_iter;
		char path[DBUS_MAXIMUM_NAME_LENGTH+1];
		char interface_dbus_name[DBUS_MAXIMUM_NAME_LENGTH+1];
		ret = lookup_dbus_name_from_interface(interface_dbus_name, gInterfaceName);
		if (ret != 0) {
			goto bail;
		}
		snprintf(path,
		         sizeof(path),
		         "%s/%s",
		         WPAN_TUNNEL_DBUS_PATH,
		         gInterfaceName);
		char* network_name = target_network.network_name;

		message = dbus_message_new_method_call(
		    interface_dbus_name,
		    path,
		    WPAN_TUNNEL_DBUS_INTERFACE,
		    WPAN_IFACE_CMD_JOIN
		    );

		fprintf(stderr,
		        "Joining \"%s\" %016llX as node type \"%s\"\n",
		        target_network.network_name,
		        (unsigned long long)target_network.xpanid,
		        node_type_int2str(node_type));

		dbus_message_append_args(
		    message,
		    DBUS_TYPE_STRING, &network_name,
		    DBUS_TYPE_INVALID
		    );

		dbus_message_append_args(
		    message,
		    DBUS_TYPE_INT16, &node_type,
		    DBUS_TYPE_INVALID
		    );

		dbus_message_append_args(
		    message,
		    DBUS_TYPE_UINT64, &target_network.xpanid,
		    DBUS_TYPE_INVALID
		    );

		dbus_message_append_args(
		    message,
		    DBUS_TYPE_UINT16, &target_network.pan_id,
		    DBUS_TYPE_INVALID
		    );

		dbus_message_append_args(
		    message,
		    DBUS_TYPE_BYTE, &target_network.channel,
		    DBUS_TYPE_INVALID
		    );

		reply = dbus_connection_send_with_reply_and_block(
		    connection,
		    message,
		    timeout,
		    &error
		    );

		if (!reply) {
			fprintf(stderr, "%s: error: %s\n", argv[0], error.message);
			ret = ERRORCODE_TIMEOUT;
			goto bail;
		}

		dbus_message_get_args(reply, &error,
		                      DBUS_TYPE_INT32, &ret,
		                      DBUS_TYPE_INVALID
		                      );

		if (!ret) {
			fprintf(stderr, "Successfully Joined!\n");
		} else if ((ret == -EINPROGRESS) || (ret == kWPANTUNDStatus_InProgress)) {
			fprintf(stderr,
			        "Partial (insecure) join. Credentials needed. Update key to continue.\n");
			ret = 0;
		} else {
			fprintf(stderr, "%s failed with error %d. %s\n", argv[0], ret, wpantund_status_to_cstr(ret));

			print_error_diagnosis(ret);
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
