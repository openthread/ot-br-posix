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
#include "tool-cmd-form.h"
#include "assert-macros.h"
#include "wpan-dbus-v0.h"
#include "args.h"
#include "string-utils.h"

#include <arpa/inet.h>
#include <errno.h>

const char form_cmd_syntax[] = "[args] [network-name]";

static const arg_list_item_t form_option_list[] = {
	{'h', "help", NULL, "Print Help"},
	{'t', "timeout", "ms", "Set timeout period"},
	{'c', "channel", "channel", "Set the desired channel"},
	{'T', "type", "node-type: router(r,2), end-device(end,e,3), sleepy-end-device(sleepy,sed,4), nl-lurker(lurker,l,6)",
		"Join as a specific node type" },
//	{'u', "ula-prefix", "ULA Prefix", "Specify a specific *LEGACY* ULA prefix"},
	{'M', "mesh-local-prefix", "Mesh-Local IPv6 Prefix", "Specify a non-default mesh-local IPv6 prefix"},
	{'L', "legacy-prefix", "Legacy IPv6 Prefix", "Specify a specific *LEGACY* IPv6 prefix"},
	{0}
};

int tool_cmd_form(int argc, char* argv[])
{
	int ret = 0;
	int c;
	int timeout = DEFAULT_TIMEOUT_IN_SECONDS * 1000;
	DBusConnection* connection = NULL;
	DBusMessage *message = NULL;
	DBusMessage *reply = NULL;
	DBusError error;
	const char* network_name = NULL;
	const char* ula_prefix = NULL;
	uint16_t node_type = WPAN_IFACE_ROLE_ROUTER; // Default to router for form
	uint32_t channel_mask = 0;

	dbus_error_init(&error);

	while (1) {
		static struct option long_options[] = {
			{"help", no_argument, 0, 'h'},
			{"timeout", required_argument, 0, 't'},
			{"channel", required_argument, 0, 'c'},
			{"ula-prefix", required_argument, 0, 'u'},
			{"mesh-local-prefix", required_argument, 0, 'M'},
			{"legacy-prefix", required_argument, 0, 'L'},
			{"type", required_argument, 0, 'T'},
			{0, 0, 0, 0}
		};

		int option_index = 0;
		c = getopt_long(argc, argv, "hc:t:T:u:", long_options,
				&option_index);

		if (c == -1)
			break;

		switch (c) {
		case 'h':
			print_arg_list_help(form_option_list, argv[0],
					    form_cmd_syntax);
			ret = ERRORCODE_HELP;
			goto bail;

		case 't':
			timeout = strtol(optarg, NULL, 0);
			break;

		case 'c':
			channel_mask = (1 << strtol(optarg, NULL, 0));
			break;

		case 'M':
			fprintf(stderr,
					"%s: error: Setting the mesh local address at the command line isn't yet implemented. Set it as a property instead.\n",
					argv[0]);
			ret = ERRORCODE_BADARG;
			goto bail;
			break;

		case 'L':
		case 'u':
			ula_prefix = optarg;
			break;

		case 'T':
			node_type = node_type_str2int(optarg);
			break;
		}
	}

	if (optind < argc) {
		if (!network_name) {
			network_name = argv[optind];
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

	if (!network_name) {
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

		message = dbus_message_new_method_call(
		    interface_dbus_name,
		    path,
		    WPAN_TUNNEL_DBUS_INTERFACE,
		    WPAN_IFACE_CMD_FORM
		    );

		dbus_message_append_args(
		    message,
		    DBUS_TYPE_STRING, &network_name,
		    DBUS_TYPE_INT16, &node_type,
		    DBUS_TYPE_UINT32, &channel_mask,
		    DBUS_TYPE_INVALID
		    );

		if(ula_prefix) {
			uint8_t ula_bytes[16] = {};

			// So the ULA prefix could either be
			// specified like an IPv6 address, or
			// specified as a bunch of hex numbers.
			// We use the presence of a colon (':')
			// to differentiate.
			if(strstr(ula_prefix,":")) {

				// Address-style
				int bits = inet_pton(AF_INET6,ula_prefix,ula_bytes);
				if(bits<0) {
					fprintf(stderr,
					        "Bad ULA \"%s\", errno=%d (%s)\n",
					        ula_prefix,
					        errno,
					        strerror(errno));
					goto bail;
				} else if(!bits) {
					fprintf(stderr, "Bad ULA \"%s\"\n", ula_prefix);
					goto bail;
				}
			} else {
				// DATA-style
				int length = parse_string_into_data(ula_bytes,
				                                    8,
				                                    ula_prefix);
				if(length<=0) {
					fprintf(stderr, "Bad ULA \"%s\"\n", ula_prefix);
					goto bail;
				}
			}

			fprintf(stderr, "Using ULA prefix \"%s\"\n", ula_prefix);

			uint8_t *addr = ula_bytes;
			dbus_message_append_args(
			    message,
			    DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE, &addr, 8,
			    DBUS_TYPE_INVALID
			    );
		}

		fprintf(stderr,
		        "Forming WPAN \"%s\" as node type \"%s\"\n",
		        network_name,
		        node_type_int2str(node_type));


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
			fprintf(stderr, "Successfully formed!\n");
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
