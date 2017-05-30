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
#include "tool-cmd-list.h"
#include "assert-macros.h"
#include "wpan-dbus-v0.h"
#include "args.h"

const char list_cmd_syntax[] = "[args] <duration>]";

static const arg_list_item_t list_option_list[] = {
	{'h', "help", NULL, "Print Help"},
	{'t', "timeout", "ms", "Set timeout period"},
	{0}
};

int tool_cmd_list(int argc, char* argv[])
{
	int ret = 0;
	int c;
	int timeout = DEFAULT_TIMEOUT_IN_SECONDS * 1000;
	DBusConnection* connection = NULL;
	DBusMessage *message = NULL;
	DBusMessage *reply = NULL;
	DBusError error;

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
			print_arg_list_help(list_option_list, argv[0],
					    list_cmd_syntax);
			ret = ERRORCODE_HELP;
			goto bail;

		case 't':
			timeout = strtol(optarg, NULL, 0);
			break;
		}
	}

	if (optind < argc) {
		fprintf(stderr,
		        "%s: error: Unexpected extra argument: \"%s\"\n",
			argv[0], argv[optind]);
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
			fprintf(stderr, "%s: error: %s\n", argv[0], error.message);
			ret = ERRORCODE_TIMEOUT;
			goto bail;
		}

		dbus_message_iter_init(reply, &iter);

		if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_ARRAY) {
			fprintf(stderr,
			        "%s: error: Bad type for interface list (%c)\n",
			        argv[0],
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
			char *interface_name;
			char *dbus_name;

			dbus_message_iter_recurse(&list_iter, &item_iter);

			dbus_message_iter_get_basic(&item_iter, &interface_name);
			dbus_message_iter_next(&item_iter);
			dbus_message_iter_get_basic(&item_iter, &dbus_name);

			printf("%s (%s)\n", interface_name, dbus_name);
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
