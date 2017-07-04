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
 *    Description:
 *      This file...
 *
 */

#include <getopt.h>
#include "wpanctl-utils.h"
#include "tool-cmd-begin-net-wake.h"
#include "assert-macros.h"
#include "args.h"
#include "assert-macros.h"
#include "wpan-dbus-v0.h"

const char begin_net_wake_cmd_syntax[] = "[args] <data>";

static const arg_list_item_t begin_net_wake_option_list[] = {
	{'h', "help", NULL, "Print Help"},
	{'t', "timeout", "ms", "Set timeout period"},
	{0}
};

int tool_cmd_begin_net_wake(int argc, char *argv[])
{
	int ret = 0;
	int c;
	int timeout = DEFAULT_TIMEOUT_IN_SECONDS * 1000;
	DBusConnection* connection = NULL;
	DBusMessage *message = NULL;
	DBusMessage *reply = NULL;
	DBusError error;

	uint8_t net_wake_data = 0;
	uint32_t net_wake_flags = -1;

	dbus_error_init(&error);

	while (1) {
		static struct option long_options[] = {
			{"help", no_argument, 0, 'h'},
			{"timeout", required_argument, 0, 't'},
			{0, 0, 0, 0}
		};

		int option_index = 0;
		c = getopt_long(argc, argv, "ht:", long_options, &option_index);

		if (c == -1)
			break;

		switch (c) {
		case 'h':
			print_arg_list_help(begin_net_wake_option_list,
					    argv[0], begin_net_wake_cmd_syntax);
			ret = ERRORCODE_HELP;
			goto bail;

		case 't':
			timeout = strtol(optarg, NULL, 0);
			break;
		}
	}

	if (optind < argc) {
		net_wake_data = strtol(argv[optind], NULL, 0);
		optind++;
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

		message = dbus_message_new_method_call(
		    interface_dbus_name,
		    path,
		    WPAN_TUNNEL_DBUS_INTERFACE,
		    WPAN_IFACE_CMD_BEGIN_NET_WAKE
		    );

		dbus_message_append_args(
		    message,
		    DBUS_TYPE_BYTE, &net_wake_data,
		    DBUS_TYPE_UINT32, &net_wake_flags,
		    DBUS_TYPE_INVALID
		    );

		fprintf(stderr,
				"Begin Net Wake, data = 0x%02X\n",
				net_wake_data);

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

		if (ret) {
			fprintf(stderr, "%s failed with error %d. %s\n", argv[0], ret, (ret<0)?strerror(-ret):"");
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
