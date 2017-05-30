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

#ifndef wpantund_socket_utils_h
#define wpantund_socket_utils_h

#include <stdbool.h>
#include <netinet/in.h>
#include <net/if.h>
#include <netdb.h>
#include <arpa/inet.h>

#define SOCKET_SYSTEM_COMMAND_PREFIX	"system:"
#define SOCKET_FD_COMMAND_PREFIX	"fd:"
#define SOCKET_FILE_COMMAND_PREFIX	"file:"
#define SOCKET_SERIAL_COMMAND_PREFIX	"serial:"
#define SOCKET_TCP_COMMAND_PREFIX	"tcp:"
#define SOCKET_SYSTEM_FORKPTY_COMMAND_PREFIX	"system-forkpty:"
#define SOCKET_SYSTEM_SOCKETPAIR_COMMAND_PREFIX	"system-socketpair:"

#ifndef SOCKET_UTILS_DEFAULT_SHELL
#define SOCKET_UTILS_DEFAULT_SHELL         "/bin/sh"
#endif

__BEGIN_DECLS
extern int gSocketWrapperBaud;
bool socket_name_is_device(const char* socket_name);
int lookup_sockaddr_from_host_and_port( struct sockaddr_in6* outaddr, const char* host, const char* port);
int open_super_socket(const char* socket_name);
int close_super_socket(int fd);
int fd_has_error(int fd);

enum {
	SUPER_SOCKET_TYPE_UNKNOWN,
	SUPER_SOCKET_TYPE_SYSTEM,
	SUPER_SOCKET_TYPE_SYSTEM_FORKPTY,
	SUPER_SOCKET_TYPE_SYSTEM_SOCKETPAIR,
	SUPER_SOCKET_TYPE_FD,
	SUPER_SOCKET_TYPE_TCP,
	SUPER_SOCKET_TYPE_DEVICE
};

int get_super_socket_type_from_path(const char* path);

int fork_unixdomain_socket(int* fd_pointer);

__END_DECLS


#endif
