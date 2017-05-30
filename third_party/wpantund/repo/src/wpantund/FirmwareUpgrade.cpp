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
 *      Firmware Upgrade Manager
 *
 */

/*

This class does what may, at first, appear to be somewhat
reminiscent of a "Rube Goldberg" contraption: When we
set the check and upgrade commands, we end up forking and
spinning off entirely new processes which we communicate
with via a socket/pipe. Why on earth would I do this?

The answer is security. While wpantund doesn't drop
privileges yet, it will have that capability one day.
This setup allows the upgrade and check scripts to run
in a privileged fashion while the rest of wpantund runs
with lower privileges. The idea is that wpantund would be
run initially in a privileged context and then, after
setting up this object, drop privileges.

There are other places where we will likely want to have
this sort of behavior (meaning that this doesn't yield any
immediate increase in security), but this is a good start
in that direction.

-- RQ 2015-10-23

*/

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "assert-macros.h"
#include <stdio.h>
#include <stdlib.h>
#include "FirmwareUpgrade.h"
#include "socket-utils.h"
#include <errno.h>
#include <syslog.h>
#include "fgetln.h"
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#if HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

using namespace nl::wpantund;

FirmwareUpgrade::FirmwareUpgrade(): mUpgradeStatus(0), mFirmwareCheckFD(-1), mFirmwareUpgradeFD(-1)
{
}

FirmwareUpgrade::~FirmwareUpgrade()
{
	close_check_fd();
	close_upgrade_fd();
}

void
FirmwareUpgrade::close_check_fd(void)
{
	if (mFirmwareCheckFD >= 0) {
		IGNORE_RETURN_VALUE(write(mFirmwareCheckFD, "X\n", 1));
		close(mFirmwareCheckFD);
		mFirmwareCheckFD = -1;
	}
}

void
FirmwareUpgrade::close_upgrade_fd(void)
{
	if (mFirmwareUpgradeFD >= 0) {
		IGNORE_RETURN_VALUE(write(mFirmwareUpgradeFD, "X", 1));
		close(mFirmwareUpgradeFD);
		mFirmwareUpgradeFD = -1;
	}
}

bool
FirmwareUpgrade::is_firmware_upgrade_required(const std::string& version)
{
	bool ret = false;
	uint8_t c = 1;

	require(!version.empty(), bail);

	require_quiet(mFirmwareCheckFD >= 0, bail);

	require_string(write(mFirmwareCheckFD, version.c_str(), version.size()) >= 0, bail, strerror(errno));

	require_string(write(mFirmwareCheckFD, "\n", 1) == 1, bail, strerror(errno));

	require_string(read(mFirmwareCheckFD, &c, 1) == 1, bail, strerror(errno));

	ret = (c == 0);

bail:

	// If this check determined that a firmware upgrade was not required,
	// go ahead and close out our check process so that we don't
	// waste resources.
	if (ret == false) {
		close_check_fd();
		close_upgrade_fd();
	}

	return ret;
}

void
FirmwareUpgrade::upgrade_firmware(void)
{
	require(mUpgradeStatus != EINPROGRESS, bail);

	mUpgradeStatus = EINVAL;

	require(mFirmwareUpgradeFD >= 0, bail);

	require_string(write(mFirmwareUpgradeFD, "1", 1) == 1, bail, strerror(errno));

	mUpgradeStatus = EINPROGRESS;

bail:
	return;
}

void
FirmwareUpgrade::set_firmware_upgrade_command(const std::string& command)
{
	int status = -1;
	pid_t pid = -1;

	if (mFirmwareUpgradeFD >= 0) {
		close(mFirmwareUpgradeFD);
		mFirmwareUpgradeFD = -1;
	}

	pid = fork_unixdomain_socket(&mFirmwareUpgradeFD);

	if (pid < 0) {
		return;
	}

	if (pid == 0) {
		int stdout_fd_copy = dup(STDOUT_FILENO);
		int stdin_fd_copy = dup(STDIN_FILENO);
		FILE* stdin_copy = NULL;
		FILE* stdout_copy = NULL;

		dup2(STDERR_FILENO,STDOUT_FILENO);
		close(STDIN_FILENO);

		if (stdin_fd_copy >= 0) {
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

		if (0 == pid)
		{
			// Set the shell environment variable if it isn't set already.
			setenv("SHELL",SOCKET_UTILS_DEFAULT_SHELL,0);

			while ((ferror(stdin_copy) == 0) && (feof(stdin_copy) == 0)) {
				int c;
				int ret;

				c = fgetc(stdin_copy);

				switch (c) {
				case '1':
					// Go ahead and leave the parent's process group
					setsid();

					// Execute the requested command.
					ret = system(command.c_str());

					// Inform our parent process of the return value for the command.
					fputc(WEXITSTATUS(ret), stdout_copy);
					fflush(stdout_copy);
					break;

				case 'X':
					fputc(0, stdout_copy);
					fflush(stdout_copy);
					_exit(EXIT_SUCCESS);
					break;

				default:
					fputc(1, stdout_copy);
					fflush(stdout_copy);
					_exit(EXIT_FAILURE);
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

		close(mFirmwareUpgradeFD);
		mFirmwareUpgradeFD = -1;

		errno = WEXITSTATUS(status);

	} else {
		int saved_flags = fcntl(mFirmwareUpgradeFD, F_GETFL, 0);
		fcntl(mFirmwareUpgradeFD, F_SETFL, saved_flags | O_NONBLOCK);
	}
}

void
FirmwareUpgrade::set_firmware_check_command(const std::string& command)
{
	int status = -1;
	pid_t pid = -1;

	if (mFirmwareCheckFD >= 0) {
		close(mFirmwareCheckFD);
		mFirmwareCheckFD = -1;
	}

	pid = fork_unixdomain_socket(&mFirmwareCheckFD);

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

		if (0 == pid)
		{
			// Set the shell environment variable if it isn't set already.
			setenv("SHELL",SOCKET_UTILS_DEFAULT_SHELL,0);

			while ((ferror(stdin_copy) == 0) && (feof(stdin_copy) == 0)) {
				int ret;
				const char* line = NULL;
				size_t line_len = 0;
				std::string escaped_line;

				line = fgetln(stdin_copy, &line_len);

				if (!line || (line_len == 0)) {
					continue;
				}

				if ((line[0] == 'X') && (line[1] == 0)) {
					fputc(0, stdout_copy);
					fflush(stdout_copy);
					_exit(EXIT_SUCCESS);
				}

				// Open quotation
				escaped_line = " '";

				// Sanitize and escape the string
				for(;line_len != 0;line_len--, line++) {
					char c = *line;
					if ((c < 32) && (c != '\t') && (c != '\n') && (c != '\r')) {
						// Control characters aren't allowed.
						syslog(LOG_ERR, "FirmwareCheck: Prohibited character (%d) in version string", c);
						fputc('E', stdout_copy);
						fflush(stdout_copy);
						_exit(EXIT_FAILURE);
					}
					switch (c) {
					case '\'':
						escaped_line += "'\''";
						break;
					case '\n':
					case '\r':
						break;
					default:
						escaped_line += c;
						break;
					}
				}

				// Close quotation
				escaped_line += "'";

				ret = system((command + escaped_line).c_str());

				fputc(WEXITSTATUS(ret), stdout_copy);
				fflush(stdout_copy);
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

		close(mFirmwareCheckFD);
		mFirmwareCheckFD = -1;

		errno = WEXITSTATUS(status);
	}
}

bool
FirmwareUpgrade::can_upgrade_firmware(void)
{
	return (mFirmwareUpgradeFD >= 0);
}

int
FirmwareUpgrade::get_upgrade_status(void)
{
	return mUpgradeStatus;
}

int
FirmwareUpgrade::update_fd_set(fd_set *read_fd_set, fd_set *write_fd_set, fd_set *error_fd_set, int *max_fd, cms_t *timeout)
{
	if ((mUpgradeStatus == EINPROGRESS) && (mFirmwareUpgradeFD >= 0)) {
		if (read_fd_set != NULL) {
			FD_SET(mFirmwareUpgradeFD, read_fd_set);
		}
		if (error_fd_set != NULL) {
			FD_SET(mFirmwareUpgradeFD, error_fd_set);
		}
		if (max_fd != NULL) {
			*max_fd = std::max(*max_fd, mFirmwareUpgradeFD);
		}
	}

	return 0;
}

void
FirmwareUpgrade::process(void)
{
	if (mUpgradeStatus == EINPROGRESS) {
		ssize_t bytes_read;
		uint8_t value;

		bytes_read = read(mFirmwareUpgradeFD, &value, 1);

		if ((bytes_read < 0) && (errno != EWOULDBLOCK)) {
			mUpgradeStatus = errno;
		} else if (bytes_read == 1) {
			mUpgradeStatus = value;

			// If the upgrade status was successful, go ahead and close
			// down both checks so that we don't waste resources. This
			// also has the added benefit of preventing upgrade loops.
			if (mUpgradeStatus == 0) {
				close_check_fd();
				close_upgrade_fd();
			}
		}
	}
}
