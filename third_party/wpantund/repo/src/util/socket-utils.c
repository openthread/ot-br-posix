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

#define _GNU_SOURCE 1

#include "assert-macros.h"

#include <stdio.h>
#include "socket-utils.h"
#include <ctype.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <ctype.h>
#include <signal.h>

#if HAVE_SYS_UN_H
#include <sys/un.h>
#endif

#if HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#if HAVE_PTY_H
#include <pty.h>
#endif

#if HAVE_UTIL_H
#include <util.h>
#endif

#if HAVE_SYS_PRCTL_H
#include <sys/prctl.h>
#endif


#if !defined(HAVE_PTSNAME) && __APPLE__
#define HAVE_PTSNAME 1
#endif

#if !HAVE_GETDTABLESIZE
#define getdtablesize()     (int)sysconf(_SC_OPEN_MAX)
#endif

#ifndef O_NONBLOCK
#define O_NONBLOCK          O_NDELAY
#endif

#include <poll.h>

static struct {
	int fd;
	pid_t pid;
} gSystemSocketTable[5];

static void
system_socket_table_close_alarm_(int sig)
{
	static const char message[] = "\nclose_super_socket: Unable to terminate child in a timely manner, watchdog fired\n";

	// Can't use syslog here, write to stderr instead.
	(void)write(STDERR_FILENO, message, sizeof(message) - 1);

	_exit(EXIT_FAILURE);
}

static void
system_socket_table_atexit_(void)
{
	int i;

	for (i = 0; i < sizeof(gSystemSocketTable)/sizeof(gSystemSocketTable[0]); i++) {
		if (gSystemSocketTable[i].pid != 0) {
			kill(gSystemSocketTable[i].pid, SIGTERM);
		}
	}
}

static void
system_socket_table_add_(int fd, pid_t pid)
{
	static bool did_init = false;
	int i;

	if (!did_init) {
		atexit(&system_socket_table_atexit_);
		did_init = true;
	}

	for (i = 0; i < sizeof(gSystemSocketTable)/sizeof(gSystemSocketTable[0]); i++) {
		if (gSystemSocketTable[i].pid == 0) {
			break;
		}
	}

	// If we have more than 5 of these types of sockets
	// open, then there is likely a serious problem going
	// on that we should immediately deal with.
	assert(i < sizeof(gSystemSocketTable)/sizeof(gSystemSocketTable[0]));

	if (i < sizeof(gSystemSocketTable)/sizeof(gSystemSocketTable[0])) {
		gSystemSocketTable[i].fd = fd;
		gSystemSocketTable[i].pid = pid;
	}
}

int
close_super_socket(int fd)
{
	int i;
	int ret;
	for (i = 0; i < sizeof(gSystemSocketTable)/sizeof(gSystemSocketTable[0]); i++) {
		if ( gSystemSocketTable[i].fd == fd
		  && gSystemSocketTable[i].pid != 0
		) {
			break;
		}
	}

	ret = close(fd);

	if (i < sizeof(gSystemSocketTable)/sizeof(gSystemSocketTable[0])) {
		pid_t pid = gSystemSocketTable[i].pid;
		int x = 0, j = 0;

		kill(gSystemSocketTable[i].pid, SIGHUP);

		for (j = 0; j < 100; ++j) {
			pid = waitpid(gSystemSocketTable[i].pid, &x, WNOHANG);
			if (pid > 0) {
				break;
			}
			usleep(100000);
		}

		if (pid <= 0) {
			// Five second watchdog.
			void (*prev_alarm_handler)(int) = signal(SIGALRM, &system_socket_table_close_alarm_);
			unsigned int prev_alarm_remaining = alarm(5);

			syslog(LOG_WARNING, "close_super_socket: PID %d didn't respond to SIGHUP, trying SIGTERM", gSystemSocketTable[i].pid);

			kill(gSystemSocketTable[i].pid, SIGTERM);

			do {
				errno = 0;
				pid = waitpid(gSystemSocketTable[i].pid, &x, 0);
			} while ((pid == -1) && (errno == EINTR));

			// Disarm watchdog.
			alarm(prev_alarm_remaining);
			signal(SIGALRM, prev_alarm_handler);
		}

		if (pid == -1) {
			perror(strerror(errno));
		}


		gSystemSocketTable[i].pid = 0;
		gSystemSocketTable[i].fd = -1;
	}

	return ret;
}

int
fd_has_error(int fd)
{
	const int flags = (POLLPRI|POLLRDBAND|POLLERR|POLLHUP|POLLNVAL);
	struct pollfd pollfd = { fd, flags, 0 };
	int count = poll(&pollfd, 1, 0);

	if (count<0) {
		return -errno;
	} else if (count > 0) {
		if (pollfd.revents&POLLHUP) {
			return -EPIPE;
		}

		if (pollfd.revents&(POLLRDBAND|POLLPRI)) {
			return -EPIPE;
		}

		if (pollfd.revents&POLLNVAL) {
			return -EINVAL;
		}

		if (pollfd.revents&POLLERR) {
			return -EIO;
		}
	}
	return 0;
}

int gSocketWrapperBaud = 115200;

static bool
socket_name_is_system_command(const char* socket_name)
{
	return strncmp(socket_name,SOCKET_SYSTEM_COMMAND_PREFIX,strlen(SOCKET_SYSTEM_COMMAND_PREFIX)) == 0
	    || strncmp(socket_name,SOCKET_SYSTEM_FORKPTY_COMMAND_PREFIX,strlen(SOCKET_SYSTEM_FORKPTY_COMMAND_PREFIX)) == 0
	    || strncmp(socket_name,SOCKET_SYSTEM_SOCKETPAIR_COMMAND_PREFIX,strlen(SOCKET_SYSTEM_SOCKETPAIR_COMMAND_PREFIX)) == 0
	;
}

static bool
socket_name_is_port(const char* socket_name)
{
	// It's a port if the string is just a number.
	while (*socket_name) {
		if (!isdigit(*socket_name++)) {
			return false;
		}
	}

	return true;
}

static bool
socket_name_is_inet(const char* socket_name)
{
	// It's an inet address if it Contains no slashes or starts with a '['.
	if (*socket_name == '[') {
		return true;
	}
	do {
		if (*socket_name == '/') {
			return false;
		}
	} while (*++socket_name);
	return !socket_name_is_port(socket_name) && !socket_name_is_system_command(socket_name);
}

bool
socket_name_is_device(const char* socket_name)
{
	return !socket_name_is_system_command(socket_name) && !socket_name_is_inet(socket_name);
}

int
lookup_sockaddr_from_host_and_port(
    struct sockaddr_in6* outaddr, const char* host, const char* port
    )
{
	int ret = 0;
	struct addrinfo hint = {
	};

	hint.ai_flags = AI_ADDRCONFIG | AI_V4MAPPED | AI_ALL;
	hint.ai_family = AF_INET6;

	struct addrinfo *results = NULL;
	struct addrinfo *iter = NULL;

	if (!port) {
		port = "4951";
	}

	if (!host) {
		host = "::1";
	}

	syslog(LOG_INFO, "Looking up [%s]:%s", host, port);

	if (isdigit(port[0]) && strcmp(host, "::1") == 0) {
		// Special case to avoid calling getaddrinfo() when
		// we really don't need to.
		memset(outaddr, 0, sizeof(struct sockaddr_in6));
		outaddr->sin6_family = AF_INET6;
		outaddr->sin6_addr.s6_addr[15] = 1;
		outaddr->sin6_port = htons(atoi(port));
	} else if (isdigit(port[0]) && inet_addr(host) != 0) {
		in_addr_t v4addr = inet_addr(host);
		memset(outaddr, 0, sizeof(struct sockaddr_in6));
		outaddr->sin6_family = AF_INET6;
		outaddr->sin6_addr.s6_addr[10] = 0xFF;
		outaddr->sin6_addr.s6_addr[11] = 0xFF;
		outaddr->sin6_port = htons(atoi(port));
		memcpy(outaddr->sin6_addr.s6_addr + 12, &v4addr, sizeof(v4addr));
		outaddr->sin6_port = htons(atoi(port));
	} else {
		int error = getaddrinfo(host, port, &hint, &results);

		require_action_string(
		    !error,
		    bail,
		    ret = -1,
		    gai_strerror(error)
		    );

		for (iter = results;
		     iter && (iter->ai_family != AF_INET6);
		     iter = iter->ai_next) ;

		require_action(NULL != iter, bail, ret = -1);

		memcpy(outaddr, iter->ai_addr, iter->ai_addrlen);
	}

bail:
	if (results)
		freeaddrinfo(results);

	return ret;
}

#if HAVE_FORKPTY
static int
diagnose_forkpty_problem()
{
	int ret = -1;
	// COM-S-7530: Do some more diagnostics on the situation, because
	// sort of failure is weird. We step through some of the same sorts of
	// things that forkpty would do so that we can see where we are failing.
	int pty_master_fd = -1;
	int pty_slave_fd = -1;
	do {
		if( access( "/dev/ptmx", F_OK ) < 0 ) {
			syslog(LOG_WARNING, "Call to access(\"/dev/ptmx\",F_OK) failed: %s (%d)", strerror(errno), errno);
			perror("access(\"/dev/ptmx\",F_OK)");
		}

		if( access( "/dev/ptmx", R_OK|W_OK ) < 0 ) {
			syslog(LOG_WARNING, "Call to access(\"/dev/ptmx\",R_OK|W_OK) failed: %s (%d)", strerror(errno), errno);
			perror("access(\"/dev/ptmx\",R_OK|W_OK)");
		}

		pty_master_fd = posix_openpt(O_NOCTTY | O_RDWR);

		if (pty_master_fd < 0) {
			syslog(LOG_CRIT, "Call to posix_openpt() failed: %s (%d)", strerror(errno), errno);
			perror("posix_openpt(O_NOCTTY | O_RDWR)");
			break;
		}

		if (grantpt(pty_master_fd) < 0) {
			syslog(LOG_CRIT, "Call to grantpt() failed: %s (%d)", strerror(errno), errno);
			perror("grantpt");
		}

		if (unlockpt(pty_master_fd) < 0) {
			syslog(LOG_CRIT, "Call to unlockpt() failed: %s (%d)", strerror(errno), errno);
			perror("unlockpt");
		}

#if HAVE_PTSNAME
		if (NULL == ptsname(pty_master_fd)) {
			syslog(LOG_CRIT, "Call to ptsname() failed: %s (%d)", strerror(errno), errno);
			perror("ptsname");
			break;
		}

		pty_slave_fd = open(ptsname(pty_master_fd), O_RDWR | O_NOCTTY);

		if (pty_slave_fd < 0) {
			syslog(LOG_CRIT, "Call to open(\"%s\",O_RDWR|O_NOCTTY) failed: %s (%d)", ptsname(pty_master_fd), strerror(errno), errno);
			perror("open(ptsname(pty_master_fd),O_RDWR|O_NOCTTY)");
			break;
		}
#endif

		ret = 0;
	} while(0);
	close(pty_master_fd);
	close(pty_slave_fd);
	return ret;
}

static int
open_system_socket_forkpty(const char* command)
{
	int ret_fd = -1;
	int stderr_copy_fd;
	pid_t pid;
	struct termios tios = { .c_cflag = CS8|HUPCL|CREAD|CLOCAL };

	cfmakeraw(&tios);

	// Duplicate stderr so that we can hook it back up in the forked process.
	stderr_copy_fd = dup(STDERR_FILENO);
	if (stderr_copy_fd < 0) {
		syslog(LOG_ERR, "Call to dup() failed: %s (%d)", strerror(errno), errno);
		goto cleanup_and_fail;
	}

	pid = forkpty(&ret_fd, NULL, &tios, NULL);
	if (pid < 0) {
		syslog(LOG_ERR, "Call to forkpty() failed: %s (%d)", strerror(errno), errno);

		if (0 == diagnose_forkpty_problem()) {
			syslog(LOG_CRIT, "FORKPTY() FAILED BUT NOTHING WAS OBVIOUSLY WRONG!!!");
		}

		goto cleanup_and_fail;
	}

	// Check to see if we are the forked process or not.
	if (0 == pid) {
		// We are the forked process.
		const int dtablesize = getdtablesize();
		int i;

#if defined(_LINUX_PRCTL_H)
		prctl(PR_SET_PDEATHSIG, SIGHUP);
#endif

		// Re-instate our original stderr.
		dup2(stderr_copy_fd, STDERR_FILENO);

		syslog(LOG_DEBUG, "Forked!");

		// Re-instate our original stderr (clobbered by login_tty)
		dup2(stderr_copy_fd, STDERR_FILENO);

		// Set the shell environment variable if it isn't set already.
		setenv("SHELL", SOCKET_UTILS_DEFAULT_SHELL, 0);

		// Close all file descriptors larger than STDERR_FILENO.
		for (i = (STDERR_FILENO + 1); i < dtablesize; i++) {
			close(i);
		}

		syslog(LOG_NOTICE,"About to exec \"%s\"",command);

		execl(getenv("SHELL"),getenv("SHELL"),"-c",command,NULL);

		syslog(LOG_ERR,"Failed for fork and exec of \"%s\": %s (%d)", command, strerror(errno), errno);

		_exit(errno);
	}

	// Clean up our copy of stderr
	close(stderr_copy_fd);

#if HAVE_PTSNAME
	// See http://stackoverflow.com/questions/3486491/
	close(open(ptsname(ret_fd), O_RDWR | O_NOCTTY));
#endif

	system_socket_table_add_(ret_fd, pid);

	return ret_fd;

cleanup_and_fail:
	{
		int prevErrno = errno;

		close(ret_fd);
		close(stderr_copy_fd);

		errno = prevErrno;
	}
	return -1;
}
#endif // HAVE_FORKPTY

#ifdef PF_UNIX

int
fork_unixdomain_socket(int* fd_pointer)
{
	int fd[2] = { -1, -1 };
	int i;
	pid_t pid = -1;

	if (socketpair(PF_UNIX, SOCK_STREAM, 0, fd) < 0) {
		syslog(LOG_ERR, "Call to socketpair() failed: %s (%d)", strerror(errno), errno);
		goto bail;
	}

	pid = fork();
	if (pid < 0) {
		syslog(LOG_ERR, "Call to fork() failed: %s (%d)", strerror(errno), errno);
		goto bail;
	}

	// Check to see if we are the forked process or not.
	if (0 == pid) {
		const int dtablesize = getdtablesize();
		// We are the forked process.

#if defined(_LINUX_PRCTL_H)
		prctl(PR_SET_PDEATHSIG, SIGHUP);
#endif

		close(fd[0]);

		dup2(fd[1], STDIN_FILENO);
		dup2(fd[1], STDOUT_FILENO);

		syslog(LOG_DEBUG, "Forked!");

        // Close all file descriptors larger than STDERR_FILENO.
        for (i = (STDERR_FILENO + 1); i < dtablesize; i++) {
            close(i);
		}

		*fd_pointer = STDIN_FILENO;
	} else {
		close(fd[1]);
		*fd_pointer = fd[0];
	}

	fd[0] = -1;
	fd[1] = -1;

bail:

	{
		int prevErrno = errno;
		if (fd[0] >= 0) {
			close(fd[0]);
		}
		if (fd[1] >= 0) {
			close(fd[1]);
		}
		errno = prevErrno;
	}

	return pid;
}

static int
open_system_socket_unix_domain(const char* command)
{
	int fd = -1;
	pid_t pid = -1;

	pid = fork_unixdomain_socket(&fd);

	if (pid < 0) {
		syslog(LOG_ERR, "Call to fork() failed: %s (%d)", strerror(errno), errno);
		goto cleanup_and_fail;
	}

	// Check to see if we are the forked process or not.
	if (0 == pid) {
		// Set the shell environment variable if it isn't set already.
		setenv("SHELL","/bin/sh",0);

		syslog(LOG_NOTICE, "About to exec \"%s\"", command);

		execl(getenv("SHELL"), getenv("SHELL"),"-c", command, NULL);

		syslog(LOG_ERR, "Failed for fork and exec of \"%s\": %s (%d)", command, strerror(errno), errno);

		_exit(EXIT_FAILURE);
	}

	system_socket_table_add_(fd, pid);

	return fd;

cleanup_and_fail:

	if (fd >= 0) {
		int prevErrno = errno;

		close(fd);

		errno = prevErrno;
	}

	return -1;
}
#endif // PF_UNIX

static int
open_system_socket(const char* command)
{
	int ret_fd = -1;

#if HAVE_FORKPTY
	ret_fd = open_system_socket_forkpty(command);
#endif

#if defined(PF_UNIX)
	// Fall back to unix-domain socket-based mechanism:
	if (ret_fd < 0) {
		ret_fd = open_system_socket_unix_domain(command);
	}
#endif

#if !HAVE_FORKPTY && !defined(PF_UNIX)
	assert_printf("%s","Process pipe sockets are not supported in current configuration");
	errno = ENOTSUP;
#endif

	return ret_fd;
}

int
get_super_socket_type_from_path(const char* socket_name)
{
	int socket_type = SUPER_SOCKET_TYPE_UNKNOWN;

	if (strncasecmp(socket_name, SOCKET_SYSTEM_COMMAND_PREFIX, sizeof(SOCKET_SYSTEM_COMMAND_PREFIX)-1) == 0) {
		socket_type = SUPER_SOCKET_TYPE_SYSTEM;
	} else if (strncasecmp(socket_name, SOCKET_SYSTEM_FORKPTY_COMMAND_PREFIX, sizeof(SOCKET_SYSTEM_FORKPTY_COMMAND_PREFIX)-1) == 0) {
		socket_type = SUPER_SOCKET_TYPE_SYSTEM_FORKPTY;
	} else if (strncasecmp(socket_name, SOCKET_SYSTEM_SOCKETPAIR_COMMAND_PREFIX, sizeof(SOCKET_SYSTEM_SOCKETPAIR_COMMAND_PREFIX)-1) == 0) {
		socket_type = SUPER_SOCKET_TYPE_SYSTEM_SOCKETPAIR;
	} else if (strncasecmp(socket_name, SOCKET_FD_COMMAND_PREFIX, sizeof(SOCKET_FD_COMMAND_PREFIX)-1) == 0) {
		socket_type = SUPER_SOCKET_TYPE_FD;
	} else if (strncasecmp(socket_name, SOCKET_FILE_COMMAND_PREFIX, sizeof(SOCKET_FILE_COMMAND_PREFIX)-1) == 0) {
		socket_type = SUPER_SOCKET_TYPE_DEVICE;
	} else if (strncasecmp(socket_name, SOCKET_SERIAL_COMMAND_PREFIX, sizeof(SOCKET_SERIAL_COMMAND_PREFIX)-1) == 0) {
		socket_type = SUPER_SOCKET_TYPE_DEVICE;
	} else if (strncasecmp(socket_name, SOCKET_TCP_COMMAND_PREFIX, sizeof(SOCKET_TCP_COMMAND_PREFIX)-1) == 0) {
		socket_type = SUPER_SOCKET_TYPE_TCP;
	} else if (socket_name_is_inet(socket_name) || socket_name_is_port(socket_name)) {
		socket_type = SUPER_SOCKET_TYPE_TCP;
	} else if (socket_name_is_device(socket_name)) {
		socket_type = SUPER_SOCKET_TYPE_DEVICE;
	}
	return socket_type;
}

int
baud_rate_to_termios_constant(int baud)
{
	int ret;
	// Standard "TERMIOS" uses constants to get and set
	// the baud rate, but we use the actual baud rate.
	// This function converts from the actual baud rate
	// to the constant supported by this platform. It
	// returns zero if the baud rate is unsupported.
	switch(baud) {
	case 9600:
		ret = B9600;
		break;
	case 19200:
		ret = B19200;
		break;
	case 38400:
		ret = B38400;
		break;
	case 57600:
		ret = B57600;
		break;
	case 115200:
		ret = B115200;
		break;
#ifdef B230400
	case 230400:
		ret = B230400;
		break;
#endif
	default:
		ret = 0;
		break;
	}
	return ret;
}

int
open_super_socket(const char* socket_name)
{
	int fd = -1;
	char* host = NULL; // Needs to be freed if set
	const char* port = NULL;
	char* options = strchr(socket_name, ',');
	char* filename = strchr(socket_name, ':');
	bool socket_name_is_well_formed = true; // True if socket has type name and options
	int socket_type = get_super_socket_type_from_path(socket_name);

	// Move past the colon, if there was one.
	if (NULL != filename && socket_name[0] != '[') {
		filename++;
	} else {
		filename = "";
	}

	if (socket_type == SUPER_SOCKET_TYPE_DEVICE) {
		socket_name_is_well_formed =
			(strncasecmp(socket_name, SOCKET_SERIAL_COMMAND_PREFIX, sizeof(SOCKET_SERIAL_COMMAND_PREFIX)-1) == 0)
			|| (strncasecmp(socket_name, SOCKET_FILE_COMMAND_PREFIX, sizeof(SOCKET_FILE_COMMAND_PREFIX)-1) == 0);
	}

	if (socket_type == SUPER_SOCKET_TYPE_TCP) {
		socket_name_is_well_formed =
			(strncasecmp(socket_name, SOCKET_TCP_COMMAND_PREFIX, sizeof(SOCKET_TCP_COMMAND_PREFIX)-1) == 0);
		if (!socket_name_is_well_formed) {
			filename = (char*)socket_name;
		}
	}

	if (!socket_name_is_well_formed) {
		filename = (char*)socket_name;
		if (socket_type == SUPER_SOCKET_TYPE_DEVICE) {
			options = ",default";
		} else {
			options = NULL;
		}
	}

	filename = strdup(filename);

	if (NULL == filename) {
		perror("strdup");
		syslog(LOG_ERR, "strdup failed. \"%s\" (%d)", strerror(errno), errno);
		goto bail;
	}

	// Make sure we zero terminate the file name before
	// any options. (this is why we needed to strdup)
	if (socket_name_is_well_formed && (NULL != options)) {
		const char* ptr = strchr(socket_name, ':');
		if (NULL == ptr) {
			ptr = socket_name;
		} else {
			ptr++;
		}
		filename[options-ptr] = 0;
	}

	if (SUPER_SOCKET_TYPE_SYSTEM == socket_type) {
		fd = open_system_socket(filename);
#if HAVE_FORKPTY
	} else if (SUPER_SOCKET_TYPE_SYSTEM_FORKPTY == socket_type) {
		fd = open_system_socket_forkpty(filename);
#endif
	} else if (SUPER_SOCKET_TYPE_SYSTEM_SOCKETPAIR == socket_type) {
		fd = open_system_socket_unix_domain(filename);
	} else if (SUPER_SOCKET_TYPE_FD == socket_type) {
		fd = dup((int)strtol(filename, NULL, 0));
	} else if (SUPER_SOCKET_TYPE_DEVICE == socket_type) {
		fd = open(filename, O_RDWR | O_NOCTTY | O_NONBLOCK);

		if (fd >= 0) {
			fcntl(fd, F_SETFL, O_NONBLOCK);
			tcflush(fd, TCIOFLUSH);
		}
	} else if (SUPER_SOCKET_TYPE_TCP == socket_type) {
		struct sockaddr_in6 addr;

		if (socket_name_is_port(socket_name)) {
			port = socket_name;
		} else {
			ssize_t i;
			if (filename[0] == '[') {
				host = strdup(filename + 1);
			} else {
				host = strdup(filename);
			}
			for (i = strlen(host) - 1; i >= 0; --i) {
				if (host[i] == ':' && port == NULL) {
					host[i] = 0;
					port = host + i + 1;
					continue;
				}
				if (host[i] == ']') {
					host[i] = 0;
					break;
				}
				if (!isdigit(host[i]))
					break;
			}
		}

		if (0 != lookup_sockaddr_from_host_and_port(&addr, host, port)) {
			syslog(LOG_ERR, "Unable to lookup \"%s\"", socket_name);
			goto bail;
		}

		fd = socket(AF_INET6, SOCK_STREAM, 0);

		if (fd < 0) {
			syslog(LOG_ERR, "Unable to open socket. \"%s\" (%d)", strerror(errno), errno);
			goto bail;
		}

		if (0 != connect(fd, (struct sockaddr*)&addr, sizeof(addr))) {
			syslog(LOG_ERR, "Call to connect() failed. \"%s\" (%d)", strerror(errno), errno);
			close(fd);
			fd = -1;
			goto bail;
		}
	} else {
		syslog(LOG_ERR, "I don't know how to open \"%s\" (socket type %d)", socket_name, (int)socket_type);
	}

	if (fd < 0) {
		syslog(LOG_ERR, "Unable to open socket. \"%s\" (%d) (filename = %s, type = %d)", strerror(errno), errno, filename, (int)socket_type);
		goto bail;
	}

	// Configure the socket.
	{
		int flags = fcntl(fd, F_GETFL);
		int i;
		struct termios tios;
		fcntl(fd, F_SETFL, flags | O_NONBLOCK);

#ifdef SO_NOSIGPIPE
		int set = 0;
		setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, (void*)&set, sizeof(int));
#endif

#define FETCH_TERMIOS() do { if(0 != tcgetattr(fd, &tios)) { \
			syslog(LOG_DEBUG, "tcgetattr() failed. \"%s\" (%d)", strerror(errno), errno); \
		} } while(0)

#define COMMIT_TERMIOS() do { if(0 != tcsetattr(fd, TCSANOW, &tios)) { \
			syslog(LOG_DEBUG, "tcsetattr() failed. \"%s\" (%d)", strerror(errno), errno); \
		} } while(0)

		// Parse the options, if any
		for (; NULL != options; options = strchr(options+1, ',')) {
			if (strncasecmp(options, ",b", 2) == 0 && isdigit(options[2])) {
				// Change Baud rate
				int baud = baud_rate_to_termios_constant((int)strtol(options+2,NULL,10));
				FETCH_TERMIOS();
				cfsetspeed(&tios, baud);
				COMMIT_TERMIOS();
			} else if (strncasecmp(options, ",default", strlen(",default")) == 0) {
				FETCH_TERMIOS();
				for (i=0; i < NCCS; i++) {
					tios.c_cc[i] = _POSIX_VDISABLE;
				}

				tios.c_cflag = (CS8|HUPCL|CREAD|CLOCAL);
				tios.c_iflag = 0;
				tios.c_oflag = 0;
				tios.c_lflag = 0;

				cfmakeraw(&tios);
				cfsetspeed(&tios, baud_rate_to_termios_constant(gSocketWrapperBaud));
				COMMIT_TERMIOS();
			} else if (strncasecmp(options, ",raw", 4) == 0) {
				// Raw mode
				FETCH_TERMIOS();
				cfmakeraw(&tios);
				COMMIT_TERMIOS();
			} else if (strncasecmp(options, ",clocal=", 8) == 0) {
				FETCH_TERMIOS();
				options = strchr(options,'=');
				if (options[1] == '1') {
					tios.c_cflag |= CLOCAL;
				} else if (options[1] == '0') {
					tios.c_cflag &= ~CLOCAL;
				}
				COMMIT_TERMIOS();
			} else if (strncasecmp(options, ",ixoff=", 6) == 0) {
				FETCH_TERMIOS();
				options = strchr(options,'=');
				if (options[1] == '1') {
					tios.c_iflag |= IXOFF;
				} else if (options[1] == '0') {
					tios.c_iflag &= ~IXOFF;
				}
				COMMIT_TERMIOS();
			} else if (strncasecmp(options, ",ixon=", 6) == 0) {
				FETCH_TERMIOS();
				options = strchr(options,'=');
				if (options[1] == '1') {
					tios.c_iflag |= IXON;
				} else if (options[1] == '0') {
					tios.c_iflag &= ~IXON;
				}
				COMMIT_TERMIOS();
			} else if (strncasecmp(options, ",ixany=", 6) == 0) {
				FETCH_TERMIOS();
				options = strchr(options,'=');
				if (options[1] == '1') {
					tios.c_iflag |= IXANY;
				} else if (options[1] == '0') {
					tios.c_iflag &= ~IXANY;
				}
				COMMIT_TERMIOS();
			} else if (strncasecmp(options, ",crtscts=", 9) == 0) {
				FETCH_TERMIOS();
				options = strchr(options,'=');
				// Hardware flow control
				if (options[1] == '1') {
					syslog(LOG_DEBUG, "Using hardware flow control for serial socket.");
					tios.c_cflag |= CRTSCTS;
				} else if (options[1] == '0') {
					tios.c_cflag &= ~CRTSCTS;
				}
				COMMIT_TERMIOS();

#ifdef CCTS_OFLOW
			} else if (strncasecmp(options, ",ccts_oflow=", 12) == 0) {
				FETCH_TERMIOS();
				options = strchr(options,'=');
				// Hardware output flow control
				if (options[1] == '1') {
					syslog(LOG_DEBUG, "Using hardware output flow control for serial socket.");
					tios.c_cflag |= CCTS_OFLOW;
				} else if (options[1] == '0') {
					tios.c_cflag &= ~CCTS_OFLOW;
				}
				COMMIT_TERMIOS();
#endif
#ifdef CRTS_IFLOW
			} else if (strncasecmp(options, ",crts_iflow=", 12) == 0) {
				FETCH_TERMIOS();
				options = strchr(options,'=');
				// Hardware input flow control
				if (options[1] == '1') {
					syslog(LOG_DEBUG, "Using hardware input flow control for serial socket.");
					tios.c_cflag |= CRTS_IFLOW;
				} else if (options[1] == '0') {
					tios.c_cflag &= ~CRTS_IFLOW;
				}
				COMMIT_TERMIOS();
#endif
			} else {
				syslog(LOG_ERR, "Unknown option (%s)", options);
			}
		}
	}
bail:
	free(filename);
	free(host);
	return fd;
}
