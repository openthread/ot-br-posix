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
 *      This file contains the default implementations for certain members
 *      of the SocketWrapper class, which is the virtual base class for using
 *      things like TCP sockets, serial file descriptors, or even other
 *      processes.
 *
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "SocketWrapper.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <poll.h>
#include <errno.h>
#include <syslog.h>
#include <sys/file.h>

using namespace nl;

SocketWrapper::~SocketWrapper()
{
}

bool
SocketWrapper::can_read(void)const
{
	return false;
}

bool
SocketWrapper::can_write(void)const
{
	return false;
}

int
SocketWrapper::set_log_level(int log_level)
{
	return -ENOTSUP;
}

int
SocketWrapper::get_read_fd(void)const
{
	return -1;
}

int
SocketWrapper::get_write_fd(void)const
{
	return -1;
}

cms_t
SocketWrapper::get_ms_to_next_event(void)const
{
	return CMS_DISTANT_FUTURE;
}

void
SocketWrapper::send_break(void)
{
}

void
SocketWrapper::reset(void)
{
}

int
SocketWrapper::hibernate(void)
{
	return -1;
}

bool
SocketWrapper::did_reset(void)
{
	return false;
}

int
SocketWrapper::update_fd_set(fd_set *read_fd_set, fd_set *write_fd_set, fd_set *error_fd_set, int *max_fd, cms_t *timeout)
{
	if (timeout != NULL) {
		*timeout = std::min(*timeout, get_ms_to_next_event());
	}
	return 0;
}
