/*
 *
 * Copyright (c) 2017 OpenThread Authors, Inc.
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
 *      This file implements "commissioner" command in wpanctl.
 *
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <getopt.h>
#include "wpanctl-utils.h"
#include "tool-cmd-setprop.h"
#include "tool-cmd-getprop.h"
#include "assert-macros.h"
#include "args.h"
#include "assert-macros.h"
#include "wpan-dbus-v1.h"
#include "string-utils.h"
#include "commissioner-utils.h"

const char commissioner_cmd_syntax[] = "[args] <psk> [address] [joiner_timeout [s]]";

static const arg_list_item_t commissioner_option_list[] = {
	{'h', "help", NULL, "Print Help"},
	{'t', "timeout", "ms", "Set timeout period"},
	{'e', "start", NULL, "Start native commissioner"},
	{'d', "stop", NULL, "Stop native commissioner"},
	{'a', "joiner-add", NULL, "Add joiner"},
	{'r', "joiner-remove", NULL, "Remove joiner"},
	{'s', "status", NULL, "Status information"},
	{0}
};

int tool_cmd_commissioner(int argc, char* argv[])
{
	int ret = 0;
	int c;
	int timeout = DEFAULT_TIMEOUT_IN_SECONDS * 1000;
	DBusConnection* connection = NULL;
	DBusMessage *message = NULL;
	DBusMessage *reply = NULL;
	DBusMessageIter iter;
	DBusError error;
	const char* ext_addr = NULL;
	const char* psk = NULL;
	int psk_len = 0;
	uint32_t joiner_timeout = DEFAULT_JOINER_TIMEOUT;
	dbus_bool_t enabled = false;
	const char* invalid_psk_characters = INVALID_PSK_CHARACTERS;
	const char* property_commissioner_enabled = kWPANTUNDProperty_ThreadCommissionerEnabled;
	const char* property_commissioner_enabled_value = "false";
	char path[DBUS_MAXIMUM_NAME_LENGTH+1];
	char interface_dbus_name[DBUS_MAXIMUM_NAME_LENGTH+1];

	dbus_error_init(&error);
	while (1) {
		static struct option long_options[] = {
			{"help", no_argument, 0, 'h'},
			{"timeout", required_argument, 0, 't'},
			{"start", no_argument, 0, 'e'},
			{"stop", no_argument, 0, 'd'},
			{"joiner-add", no_argument, 0, 'a'},
			{"remove", required_argument, 0, 'r'},
			{"status", no_argument, 0, 's'},
			{0, 0, 0, 0}
		};

		int option_index = 0;
		c = getopt_long(argc, argv, "hst:edr:a", long_options,
						&option_index);
		if (c == -1)
			break;

		switch (c) {
		case 'h':
			print_arg_list_help(commissioner_option_list,
								argv[0], commissioner_cmd_syntax);
			ret = ERRORCODE_HELP;
			goto bail;

		case 's':
			// status
			ret = lookup_dbus_name_from_interface(interface_dbus_name, gInterfaceName);
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
				WPANTUND_IF_CMD_PROP_GET
			);

			dbus_message_append_args(
				message,
				DBUS_TYPE_STRING, &property_commissioner_enabled,
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

				fprintf(stderr, "%s: %s (%d)\n", property_commissioner_enabled, error_cstr, ret);
				goto bail;
			}
			dbus_message_iter_next(&iter);

			dbus_bool_t en;
			dbus_message_iter_get_basic(&iter, &en);
			fprintf(stderr, "%s\n", en ? "enabled" : "disabled");
			goto bail;

		case 't':
			//timeout
			timeout = strtol(optarg, NULL, 0);
			break;

		case 'e':
			// start (enabled)
			property_commissioner_enabled_value = "true";
			// intentionally pass through
		case 'd':
			// stop (disabled)
			ret = lookup_dbus_name_from_interface(interface_dbus_name, gInterfaceName);
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
				WPANTUND_IF_CMD_PROP_SET
			);

			dbus_message_append_args(
				message,
				DBUS_TYPE_STRING, &property_commissioner_enabled,
				DBUS_TYPE_INVALID
			);

			dbus_message_append_args(
				message,
				DBUS_TYPE_STRING, &property_commissioner_enabled_value,
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
				fprintf(stderr, "Commissioner command applied.\n");
			} else {
				fprintf(stderr, "%s failed with error %d. %s\n", argv[0], ret, wpantund_status_to_cstr(ret));
				print_error_diagnosis(ret);
			}

			goto bail;

		case 'r':
			// remove
			ret = ERRORCODE_NOT_IMPLEMENTED;
			goto bail;

		case 'a':
			// add
			if (optind < argc) {
				if (!psk) {
					psk = argv[optind];
					psk_len = strnlen(psk, PSK_MAX_LENGTH+1);
					optind++;
				}
			}

			if (optind < argc) {
				if (!ext_addr) {
					ext_addr = argv[optind];
					optind++;
				}
			}

			if (optind < argc) {
				joiner_timeout = (uint32_t) strtol(argv[optind], NULL, 0);
				optind++;
			}

			if (optind < argc) {
				fprintf(stderr,
						"%s: error: Unexpected extra argument: \"%s\"\n",
						argv[0], argv[optind]);
				ret = ERRORCODE_BADARG;
				goto bail;
			}

			if (!ext_addr)
				fprintf(stderr, "%s: warning: No address value specified, any joiner knowing PSKd can join.\n", argv[0]);
			else {
				if (!is_hex(ext_addr, EXT_ADDRESS_LENGTH_CHAR)) {
					fprintf(stderr, "%s: error: Invalid address.\n", argv[0]);
					ret = ERRORCODE_BADARG;
					goto bail;
				}
				if (strnlen(ext_addr, EXT_ADDRESS_LENGTH_CHAR+1) != EXT_ADDRESS_LENGTH_CHAR) {
					fprintf(stderr, "%s: error: Wrong address length.%zu \n", argv[0], strnlen(ext_addr, EXT_ADDRESS_LENGTH_CHAR+1));
					ret = ERRORCODE_BADARG;
					goto bail;
				}
			}

			if (!psk) {
				fprintf(stderr, "%s: error: Missing PSK value.\n", argv[0]);
				ret = ERRORCODE_BADARG;
				goto bail;
			}

			if (psk_len < PSK_MIN_LENGTH || psk_len > PSK_MAX_LENGTH) {
				fprintf(stderr, "%s: error: Invalid PSK length.\n", argv[0]);
				ret = ERRORCODE_BADARG;
				goto bail;
			}

			if (strpbrk(invalid_psk_characters, psk) != 0) {
				fprintf(stderr, "%s: warning: PSK consists an invalid character\n", argv[0]);
			}

			if (!is_uppercase_or_digit(psk, psk_len)) {
				fprintf(stderr, "%s: warning: PSK should consist only uppercase letters and digits\n", argv[0]);
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
				WPANTUND_IF_CMD_JOINER_ADD
			);

			if (psk) {
				uint8_t psk_bytes[psk_len];
				memset(psk_bytes, '\0', psk_len+1);

				memcpy(psk_bytes, psk, psk_len);
				char *psk = psk_bytes;

				dbus_message_append_args(
					message,
					DBUS_TYPE_STRING, &psk,
					DBUS_TYPE_INVALID
				);

				dbus_message_append_args(
					message,
					DBUS_TYPE_UINT32, &joiner_timeout,
					DBUS_TYPE_INVALID
				);

				if (ext_addr) {
					uint8_t addr_bytes[EXT_ADDRESS_LENGTH];
					memset(addr_bytes, 0, EXT_ADDRESS_LENGTH);
					uint8_t *p_addr_bytes = addr_bytes;
					int length = parse_string_into_data(addr_bytes,
														EXT_ADDRESS_LENGTH,
														ext_addr
					);

					assert(length == EXT_ADDRESS_LENGTH);
					dbus_message_append_args(
						message,
						DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE, &p_addr_bytes, length,
						DBUS_TYPE_INVALID
					);
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

				if (!ret) {
					fprintf(stderr, "Joiner added.\n");
				} else {
					fprintf(stderr, "%s failed with error %d. %s\n", argv[0], ret, wpantund_status_to_cstr(ret));
					print_error_diagnosis(ret);
				}
			}
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
