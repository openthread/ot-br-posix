/*
 *
 * Copyright (c) 2017 Nest Labs, Inc.
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
#include "tool-updateprop.h"
#include "assert-macros.h"
#include "args.h"
#include "assert-macros.h"
#include "wpan-dbus-v1.h"
#include "string-utils.h"

const char updateprop_syntax[] = "[args] <property-name> <property-value>";

static const arg_list_item_t insertprop_option_list[] = {
	{'h', "help", NULL, "Print Help"},
	{'t', "timeout", "ms", "Set timeout period"},
	{'d', "data", NULL, "Value is binary data (in hex)"},
	{'s', "string", NULL, "Value is a string"},
	{'v', "value", "property-value", "Useful when the value starts with a '-'"},
	{0}
};

int tool_updateprop(const char *dbus_method_name, int argc, char* argv[])
{
	int ret = 0;
	int c;
	int timeout = 30 * 1000;
	DBusConnection* connection = NULL;
	DBusMessage *message = NULL;
	DBusMessage *reply = NULL;
	DBusError error;
	const char* property_name = NULL;
	char* property_value = NULL;

	enum {
		kPropertyType_String,
		kPropertyType_Data,
	} property_type = kPropertyType_String;

	dbus_error_init(&error);

	while (1) {
		static struct option long_options[] = {
			{"help", no_argument, 0, 'h'},
			{"timeout", required_argument, 0, 't'},
			{"data", no_argument, 0, 'd'},
			{"string", no_argument, 0, 's'},
			{"value", required_argument, 0, 'v'},
			{0, 0, 0, 0}
		};

		int option_index = 0;
		c = getopt_long(argc, argv, "ht:dsv:", long_options,
				&option_index);

		if (c == -1) {
			break;
		}

		switch (c) {
		case 'h':
			print_arg_list_help(insertprop_option_list,
					    argv[0], updateprop_syntax);
			ret = ERRORCODE_HELP;
			goto bail;

		case 't':
			timeout = strtol(optarg, NULL, 0);
			break;

		case 'd':
			property_type = kPropertyType_Data;
			break;

		case 's':
			property_type = kPropertyType_String;
			break;

		case 'v':
			property_value = optarg;
			break;
		}
	}

	if (optind < argc) {
		if (!property_name) {
			property_name = argv[optind];
			optind++;
		}
	}

	if (optind < argc) {
		if (!property_value) {
			property_value = argv[optind];
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

	if (!property_name) {
		fprintf(stderr, "%s: error: Missing property name.\n", argv[0]);
		ret = ERRORCODE_BADARG;
		goto bail;
	}

	if (!property_value) {
		fprintf(stderr, "%s: error: Missing property value.\n", argv[0]);
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
			print_error_diagnosis(ret);
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
		    dbus_method_name
		    );

		dbus_message_append_args(
		    message,
		    DBUS_TYPE_STRING, &property_name,
		    DBUS_TYPE_INVALID
		    );

		if (property_type == kPropertyType_String) {
			dbus_message_append_args(
			    message,
			    DBUS_TYPE_STRING, &property_value,
			    DBUS_TYPE_INVALID
			    );
		} else if (property_type == kPropertyType_Data) {
			int length = parse_string_into_data((uint8_t*)property_value,
			                                    strlen(property_value),
			                                    property_value);
			dbus_message_append_args(
			    message,
			    DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE, &property_value, length,
			    DBUS_TYPE_INVALID
			    );
		} else {
			fprintf(stderr, "%s: error: Bad property type\n", argv[0]);
			ret = ERRORCODE_UNKNOWN;
			goto bail;
		}

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
			fprintf(stderr, "%s failed with error %d. %s\n", argv[0], ret, wpantund_status_to_cstr(ret));
		}
	}

bail:

	if (connection) {
		dbus_connection_unref(connection);
	}

	if (message) {
		dbus_message_unref(message);
	}

	if (reply) {
		dbus_message_unref(reply);
	}

	dbus_error_free(&error);

	return ret;
}
