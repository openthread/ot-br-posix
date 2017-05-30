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
#include "tool-cmd-config-gateway.h"
#include "assert-macros.h"
#include "args.h"
#include "assert-macros.h"
#include "wpan-dbus-v1.h"
#include "string-utils.h"

#include <arpa/inet.h>
#include <errno.h>

const char config_gateway_cmd_syntax[] = "[args] <prefix>";

static const arg_list_item_t config_gateway_option_list[] = {
	{'h', "help", NULL, "Print Help"},
	{'t', "timeout", "ms", "Set timeout period"},

	{'p', "preferred-lifetime", "seconds",
	 "Set the preferred lifetime (Default: infinite)"},
	{'v', "valid-lifetime", "seconds",
	 "Set the valid lifetime (Default: infinite)"},
	{'d', "default", NULL, "Indicates that we can be a default route"},
	{0}
};

int tool_cmd_config_gateway(int argc, char* argv[])
{
	int ret = 0;
	int c;
	int timeout = DEFAULT_TIMEOUT_IN_SECONDS * 1000;
	DBusConnection* connection = NULL;
	DBusMessage *message = NULL;
	DBusMessage *reply = NULL;
	DBusError error;
	dbus_bool_t defaultRoute = FALSE;
	uint32_t preferredLifetime = 0xFFFFFFFF;
	uint32_t validLifetime = 0xFFFFFFFF;
	const char* prefix = NULL;
	uint8_t prefix_length = 64;
	char address_string[INET6_ADDRSTRLEN] = "::";
	uint8_t prefix_bytes[16] = {};
	uint8_t *addr = prefix_bytes;

	dbus_error_init(&error);

	while (1) {
		static struct option long_options[] = {
			{"help", no_argument, 0, 'h'},
			{"timeout", required_argument, 0, 't'},
			{"preferred-lifetime", required_argument, 0, 'p'},
			{"valid-lifetime", required_argument, 0, 'v'},
			{"default", no_argument, 0, 'd'},
			{0, 0, 0, 0}
		};

		int option_index = 0;
		c = getopt_long(argc, argv, "ht:p:v:d", long_options,
				&option_index);

		if (c == -1)
			break;

		switch (c) {
		case 'h':
			print_arg_list_help(config_gateway_option_list, argv[0],
					    config_gateway_cmd_syntax);
			ret = ERRORCODE_HELP;
			goto bail;

		case 'd':
			defaultRoute = TRUE;
			break;

		case 't':
			timeout = strtol(optarg, NULL, 0);
			break;

		case 'p':
			preferredLifetime = (uint32_t) strtol(optarg, NULL, 0);
			break;

		case 'v':
			validLifetime = (uint32_t) strtol(optarg, NULL, 0);
			break;

		}
	}

	if (optind < argc) {
		if (!prefix) {
			prefix = argv[optind];
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
		    WPANTUND_IF_CMD_CONFIG_GATEWAY
		    );

		dbus_message_append_args(
		    message,
		    DBUS_TYPE_BOOLEAN, &defaultRoute,
		    DBUS_TYPE_INVALID
		);

		if(prefix) {

			// So the prefix could either be
			// specified like an IPv6 address, or
			// specified as a bunch of hex numbers.
			// We use the presence of a colon (':')
			// to differentiate.
			if(strstr(prefix,":")) {

				// Address-style
				int bits = inet_pton(AF_INET6,prefix,prefix_bytes);
				if(bits<0) {
					fprintf(stderr,
					        "Bad Prefix \"%s\", errno=%d (%s)\n",
					        prefix,
					        errno,
					        strerror(errno));
					goto bail;
				} else if(!bits) {
					fprintf(stderr, "Bad prefix \"%s\"\n", prefix);
					goto bail;
				}
			} else {
				// DATA-style
				int length = parse_string_into_data(prefix_bytes,
				                                    8,
				                                    prefix);
				if(length<=0) {
					fprintf(stderr, "Bad prefix \"%s\"\n", prefix);
					goto bail;
				}
			}

			inet_ntop(AF_INET6, (const void *)&prefix_bytes, address_string, sizeof(address_string));

			fprintf(stderr, "Using prefix \"%s\"\n", address_string);

		}

		addr = prefix_bytes;

		dbus_message_append_args(
		    message,
			DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE, &addr, 16,
		    DBUS_TYPE_UINT32, &preferredLifetime,
		    DBUS_TYPE_UINT32, &validLifetime,
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
			fprintf(stderr, "Gateway configured.\n");
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
