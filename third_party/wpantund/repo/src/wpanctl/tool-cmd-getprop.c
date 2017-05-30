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
#include "tool-cmd-getprop.h"
#include "assert-macros.h"
#include "args.h"
#include "assert-macros.h"
#include "wpan-dbus-v1.h"

const char getprop_cmd_syntax[] = "[args] <property-name>";

static const arg_list_item_t getprop_option_list[] = {
	{'h', "help", NULL, "Print Help"},
	{'t', "timeout", "ms", "Set timeout period"},
	{'a', "all", NULL, "Print all supported properties"},
	{'v', "value-only", NULL, "Print only the value of the property"},
	{0}
};

int tool_cmd_getprop(int argc, char *argv[])
{
	int ret = 0;
	int c;
	int timeout = 10 * 1000;
	DBusConnection *connection = NULL;
	DBusMessage *message = NULL;
	DBusMessage *reply = NULL;
	DBusError error;
	const char *property_name = NULL;
	bool get_all = false;
	bool value_only = false;

	dbus_error_init(&error);

	optind = 0;

	while (1) {
		static struct option long_options[] = {
			{"help", no_argument, 0, 'h'},
			{"timeout", required_argument, 0, 't'},
			{"all", no_argument, 0, 'a'},
			{"value-only", no_argument, 0, 'v'},
			{0, 0, 0, 0}
		};

		int option_index = 0;
		c = getopt_long(argc, argv, "ht:av", long_options,
				&option_index);

		if (c == -1)
			break;

		switch (c) {
		case 'h':
			print_arg_list_help(getprop_option_list, argv[0],
					    getprop_cmd_syntax);
			ret = ERRORCODE_HELP;
			goto bail;

		case 't':
			timeout = strtol(optarg, NULL, 0);
			break;

		case 'a':
			get_all = true;
			break;

		case 'v':
			value_only = true;
			break;
		}
	}

	if (optind < argc) {
		property_name = argv[optind];
	}

	if (optind == argc) {
		property_name = "";
		get_all = true;
	}

	if ((optind < argc) && get_all) {
		fprintf(stderr,
		        "%s: error: Can't specify a specific property and request all properties at the same time.\n",
		        argv[0]);
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

	if (optind + 1 < argc) {
		int cmd_and_flags = optind;
		int j;
		for (j = optind; j < argc; j++) {
			optind = 0;
			argv[cmd_and_flags] = argv[j];
			ret = tool_cmd_getprop(cmd_and_flags + 1, argv);
		}
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
		         WPANTUND_DBUS_PATH,
		         gInterfaceName);

		message = dbus_message_new_method_call(
		    interface_dbus_name,
		    path,
		    WPANTUND_DBUS_APIv1_INTERFACE,
		    WPANTUND_IF_CMD_PROP_GET
		    );

		dbus_message_append_args(
		    message,
		    DBUS_TYPE_STRING, &property_name,
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

		dbus_message_iter_init(reply, &iter);

		// Get return code
		dbus_message_iter_get_basic(&iter, &ret);

		if (ret) {
			const char* error_cstr = NULL;

			// Try to see if there is an error explanation we can extract
			dbus_message_iter_next(&iter);
			if (dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_STRING) {
				dbus_message_iter_get_basic(&iter, &error_cstr);
			}

			if(!error_cstr || error_cstr[0] == 0) {
				error_cstr = (ret<0)?strerror(-ret):"Get failed";
			}

			fprintf(stderr, "%s: %s (%d)\n", property_name, error_cstr, ret);
			goto bail;
		}

		// Move to the property
		dbus_message_iter_next(&iter);

		if (get_all) {
			dbus_message_iter_recurse(&iter, &list_iter);

			for (;
			     dbus_message_iter_get_arg_type(&list_iter) == DBUS_TYPE_STRING;
			     dbus_message_iter_next(&list_iter)) {
				char* args[3] = { argv[0] };
				dbus_message_iter_get_basic(&list_iter, &args[1]);
				ret = tool_cmd_getprop(2, args);
			}
		} else {
			if(!value_only && property_name[0])
				fprintf(stdout, "%s = ", property_name);
			dump_info_from_iter(stdout, &iter, 0, false, false);
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
