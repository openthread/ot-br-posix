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
 *      NetworkRetain class implementation.
 *
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "assert-macros.h"
#include "wpan-properties.h"
#include "NetworkRetain.h"
#include "socket-utils.h"

#include <syslog.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#if HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

using namespace nl;
using namespace wpantund;

NetworkRetain::NetworkRetain(void) :
	mNetworkRetainFD(-1)
{
}

NetworkRetain::~NetworkRetain()
{
	close_network_retain_fd();
}

void
NetworkRetain::handle_ncp_state_change(NCPState new_ncp_state, NCPState old_ncp_state)
{
	require_quiet(mNetworkRetainFD >= 0, bail);

	// Not-joined --> joined
	if (!ncp_state_has_joined(old_ncp_state) && ncp_state_has_joined(new_ncp_state)) {
		save_network_info();
	}

	// Initializing --> Offline
	else if (ncp_state_is_initializing(old_ncp_state) && (new_ncp_state == OFFLINE)) {
		recall_network_info();
	}

	// Joined --> Offline
	else if (ncp_state_has_joined(old_ncp_state) && (new_ncp_state == OFFLINE)) {
		erase_network_info();
	}

bail:
	return;
}

void
NetworkRetain::close_network_retain_fd(void)
{
	if (mNetworkRetainFD >= 0)
	{
		IGNORE_RETURN_VALUE(write(mNetworkRetainFD, "X", 1));
		close(mNetworkRetainFD);
		mNetworkRetainFD = -1;
	}
}

void
NetworkRetain::set_network_retain_command(const std::string& command)
{
	int status = -1;
	pid_t pid = -1;

	if (mNetworkRetainFD >= 0) {
		close_network_retain_fd();
	}

	pid = fork_unixdomain_socket(&mNetworkRetainFD);

	if (pid < 0) {
		return;
	}

	if (pid == 0) {
		int stdout_fd_copy = dup(STDOUT_FILENO);
		int stdin_fd_copy = dup(STDIN_FILENO);
		FILE* stdin_copy = NULL;
		FILE* stdout_copy = NULL;

		dup2(STDERR_FILENO,STDOUT_FILENO);

		if (stdin_fd_copy >= 0) {
			close(STDIN_FILENO);
			stdin_copy = fdopen(stdin_fd_copy, "r");
		}

		if (stdout_fd_copy >= 0) {
			stdout_copy = fdopen(stdout_fd_copy, "w");
		}

		// Double fork to avoid leaking zombie processes.
		pid = fork();
		if (pid < 0) {
			syslog(LOG_ERR, "Call to fork() failed: %s (%d)", strerror(errno), errno);

			_exit(errno);
		}

		if (0 == pid)  // In child process.
		{
			// Set the shell environment variable if it isn't set already.
			setenv("SHELL",SOCKET_UTILS_DEFAULT_SHELL,0);

			while ((ferror(stdin_copy) == 0) && (feof(stdin_copy) == 0)) {
				int c;
				std::string args;

				c = fgetc(stdin_copy);

				switch (c) {
				case 'R':
				case 'E':
				case 'S':
					args = " ";
					args += c;

					// Execute the requested command.
					IGNORE_RETURN_VALUE(system((command + args).c_str()));
					break;

				case 'X':
					_exit(EXIT_SUCCESS);
					break;

				default:
					syslog(LOG_WARNING, "Got unrecognized char 0x%x in NetworkRetain child process.", (int)c);
					break;
				}
			}

			_exit(EXIT_FAILURE);
		}

		_exit(EXIT_SUCCESS);
	}

	// Wait for the first fork to return, and place the return value in errno
	if (waitpid(pid, &status, 0) < 0) {
		syslog(LOG_ERR, "Call to waitpid() failed: %s (%d)", strerror(errno), errno);
	}

	if (0 != WEXITSTATUS(status)) {
		// If this has happened then the double fork failed. Clean up
		// and pass this status along to the caller as errno.

		syslog(LOG_ERR, "Child process failed: %s (%d)", strerror(WEXITSTATUS(status)), WEXITSTATUS(status));

		close(mNetworkRetainFD);
		mNetworkRetainFD = -1;

		errno = WEXITSTATUS(status);
	}
}

void
NetworkRetain::save_network_info(void)
{
	syslog(LOG_NOTICE, "NetworkRetain - Saving network info...");
	require_string(write(mNetworkRetainFD, "S", 1) == 1, bail, strerror(errno));
bail:
	return;
}

void
NetworkRetain::recall_network_info(void)
{
	syslog(LOG_NOTICE, "NetworkRetain - Recalling/restoring network info...");
	require_string(write(mNetworkRetainFD, "R", 1) == 1, bail, strerror(errno));
bail:
	return;
}

void
NetworkRetain::erase_network_info(void)
{
	syslog(LOG_NOTICE, "NetworkRetaine - Erasing network info...");
	require_string(write(mNetworkRetainFD, "E", 1) == 1, bail, strerror(errno));
bail:
	return;
}

