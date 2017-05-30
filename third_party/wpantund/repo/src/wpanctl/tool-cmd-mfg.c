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
#include "tool-cmd-status.h"
#include "assert-macros.h"
#include "wpan-dbus-v1.h"
#include "args.h"

int tool_cmd_mfg(int argc, char *argv[])
{
	int ret = 0;
	int c;
	int timeout = 10 * 1000;
        char output[100];
	DBusConnection *connection = NULL;
	DBusMessage *message = NULL;
	DBusMessage *reply = NULL;
	const char* result_cstr = NULL;
	DBusError error;

	dbus_error_init(&error);

	connection = dbus_bus_get(DBUS_BUS_STARTER, &error);

	if (!connection) {
		dbus_error_free(&error);
		dbus_error_init(&error);
		connection = dbus_bus_get(DBUS_BUS_SYSTEM, &error);
	}

	require_string(error.name == NULL, bail, error.message);

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
		    WPANTUND_DBUS_NLAPIv1_INTERFACE,
		    WPANTUND_IF_CMD_MFG
		);

		char string[100];
		if (argc == 1)
			sprintf(string, "%s", argv[0]);
		if (argc == 2)
			sprintf(string, "%s %s", argv[0], argv[1]);
		if (argc == 3)
			sprintf(string, "%s %s %s", argv[0], argv[1], argv[2]);
		if (argc == 4)
			sprintf(string, "%s %s %s %s", argv[0], argv[1], argv[2], argv[3]);
		char *cmd = string;
		dbus_message_append_args(
		    message,
		    DBUS_TYPE_STRING, &cmd,
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

		dbus_message_iter_get_basic(&iter, &ret);

		dbus_message_iter_next(&iter);

		if (dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_STRING) {
			dbus_message_iter_get_basic(&iter, &result_cstr);
			printf("%s", result_cstr);
		} else {
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
