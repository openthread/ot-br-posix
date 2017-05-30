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
#include <sys/socket.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include "wpanctl-utils.h"
#include "tool-cmd-pcap.h"
#include "assert-macros.h"
#include "args.h"
#include "assert-macros.h"
#include "wpan-dbus-v1.h"

const char pcap_cmd_syntax[] = "[args] <capture-file>";

static const arg_list_item_t pcap_option_list[] = {
	{'h', "help", NULL, "Print Help"},
	{'t', "timeout", "ms", "Set timeout period"},
	{'f', NULL, NULL, "Allow packet capture to controlling TTY"},
	{0}
};


static int
do_pcap_to_fd(int fd, int timeout, DBusError *error)
{
	int ret = ERRORCODE_UNKNOWN;
	DBusConnection *connection = NULL;
	DBusMessage *message = NULL;
	DBusMessage *reply = NULL;
	char path[DBUS_MAXIMUM_NAME_LENGTH+1];
	char interface_dbus_name[DBUS_MAXIMUM_NAME_LENGTH+1];

	connection = dbus_bus_get(DBUS_BUS_STARTER, error);

	if (connection == NULL) {
		if (error != NULL) {
			dbus_error_free(error);
			dbus_error_init(error);
		}
		connection = dbus_bus_get(DBUS_BUS_SYSTEM, error);
	}

	require(connection != NULL, bail);

	ret = lookup_dbus_name_from_interface(interface_dbus_name, gInterfaceName);

	require_noerr(ret, bail);

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
		WPANTUND_IF_CMD_PCAP_TO_FD
	);

	dbus_message_append_args(
		message,
		DBUS_TYPE_UNIX_FD, &fd,
		DBUS_TYPE_INVALID
	);

	ret = ERRORCODE_TIMEOUT;

	reply = dbus_connection_send_with_reply_and_block(
		connection,
		message,
		timeout,
		error
	);

	require(reply != NULL, bail);

	dbus_message_get_args(
		reply,
		error,
		DBUS_TYPE_INT32, &ret,
		DBUS_TYPE_INVALID
	);

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

	return ret;
}

//! Checks to see if the given FD is the controlling TTY.
bool
is_descriptor_ctty(int fd)
{
	bool ret = false;

	if (isatty(fd)) {
		char tty_name_copy[L_ctermid] = "";
		const char *tty_name = ttyname(fd);

		if (tty_name == NULL) {
			goto bail;
		}

		// Just use snprintf as a safe string copy.
		// It is more portable than strlcpy and we don't care about
		// performance in this context.
		snprintf(tty_name_copy, sizeof(tty_name_copy), "%s", tty_name);

		if (NULL == (tty_name = ttyname(STDIN_FILENO))) {
			goto bail;
		}

		ret = (0 == strcmp(tty_name_copy, tty_name));
	}

bail:
	return ret;
}

int
tool_cmd_pcap(int argc, char *argv[])
{
	static const int set = 1;
	int ret = 0;
	int timeout = 10 * 1000;

	int fd_out = -1;
	int fd_pair[2] = { -1, -1 };
	bool force_ctty = false;
	bool stdout_was_closed = false;

	DBusError error;

	dbus_error_init(&error);

	while (1) {
		static struct option long_options[] = {
			{"help", no_argument, 0, 'h'},
			{"timeout", required_argument, 0, 't'},
			{0, 0, 0, 0}
		};

		int option_index = 0;
		int c;

		c = getopt_long(argc, argv, "fht:", long_options, &option_index);

		if (c == -1) {
			break;
		}

		switch (c) {
		case 'h':
			print_arg_list_help(pcap_option_list, argv[0],
					    pcap_cmd_syntax);
			ret = ERRORCODE_HELP;
			goto bail;

		case 'f':
			force_ctty = true;
			break;

		case 't':
			timeout = strtol(optarg, NULL, 0);
			break;
		}
	}

	// We use a socket pair for the socket we hand to wpantund,
	// so that the termination of this process will stop packet
	// capture.
	if (socketpair(PF_UNIX, SOCK_DGRAM, 0, fd_pair) < 0) {
		perror("socketpair");
		ret = ERRORCODE_UNKNOWN;
		goto bail;
	}

	if (optind < argc) {
		// Capture packets to a file path

		fd_out = open(argv[optind], O_WRONLY|O_CREAT|O_TRUNC, 0666);

		optind++;

	} else {
		// Capture packets to stdout

		if (!force_ctty && is_descriptor_ctty(STDOUT_FILENO)) {
			fprintf(stderr, "%s: error: Cowardly refusing write binary data to controlling tty, use -f to override\n", argv[0]);
			ret = ERRORCODE_REFUSED;
			goto bail;
		}

		// Update fd_out, close the original stdout.
		fd_out = dup(STDOUT_FILENO);
		close(STDOUT_FILENO);
		stdout_was_closed = true;
	}

	if (optind < argc) {
		fprintf(
			stderr,
		    "%s: error: Unexpected extra argument: \"%s\"\n",
			argv[0],
			argv[optind]
		);
		ret = ERRORCODE_BADARG;
		goto bail;
	}

	if (fd_out < 0) {
		fprintf(
			stderr,
			"%s: error: Unable to open file for pcap: \"%s\"\n",
			argv[0],
			strerror(errno)
		);
		ret = ERRORCODE_BADARG;
		goto bail;
	}

	if (gInterfaceName[0] == 0) {
		fprintf(
			stderr,
		    "%s: error: No WPAN interface set (use the `cd` command, or the `-I` argument for `wpanctl`).\n",
		    argv[0]
		);
		ret = ERRORCODE_BADARG;
		goto bail;
	}

	// Have wpantund start writing PCAP data to one end of our socket pair.
	ret = do_pcap_to_fd(fd_pair[1], timeout, &error);

	if (ret) {
		if (error.message != NULL) {
			fprintf(stderr, "%s: error: %s\n", argv[0], error.message);
		}
		goto bail;
	}

	// Close this side of the socket pair so that we can
	// detect if wpantund closed the socket.
	close(fd_pair[1]);

#ifdef SO_NOSIGPIPE
	// Turn off sigpipe so we can gracefully exit.
	setsockopt(fd_pair[0], SOL_SOCKET, SO_NOSIGPIPE, (void *)&set, sizeof(int));
	setsockopt(fd_out, SOL_SOCKET, SO_NOSIGPIPE, (void *)&set, sizeof(int));
#endif

	fprintf(stderr, "%s: Capture started\n", argv[0]);

	// Data pump
	while (true) {
		char buffer[2048];
		ssize_t buffer_len;

		// Read in from datagram socket
		buffer_len = read(fd_pair[0], buffer, sizeof(buffer));

		if (buffer_len <= 0) {
			break;
		}

		// Write out the buffer to our file descriptor
		buffer_len = write(fd_out, buffer, buffer_len);
		if (buffer_len <= 0) {
			break;
		}
	}

	fprintf(stderr, "%s: Capture terminated\n", argv[0]);

bail:

	if (ret) {
		fprintf(stderr, "pcap: failed with error %d. %s\n", ret, wpantund_status_to_cstr(ret));
		print_error_diagnosis(ret);
	}

	// Clean up.

	close(fd_out);

	close(fd_pair[0]);

	close(fd_pair[1]);

	dbus_error_free(&error);

	if (stdout_was_closed) {
		exit (ret);
	}

	return ret;
}
