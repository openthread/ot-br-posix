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
 *      This file defindes the SuperSocket class, which provides an
 *      easy way to create various types of sockets using a convenient
 *      path syntax.
 *
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "assert-macros.h"

#include "SuperSocket.h"
#include "socket-utils.h"
#include "time-utils.h"
#include <stdexcept>
#include <syslog.h>
#include <errno.h>
#include <unistd.h>
#include <termios.h>
#include <sys/file.h>

using namespace nl;

SuperSocket::SuperSocket(const std::string& path)
	:UnixSocket(-1, false), mPath(path)
{
	int fd = open_super_socket(path.c_str());

	mFDRead = mFDWrite = fd;

	if (0 > fd) {
		syslog(LOG_ERR, "Unable to open socket with path <%s>, errno=%d (%s)", path.c_str(), errno, strerror(errno));
		throw std::runtime_error("Unable to open socket");
	}

	// Lock the file descriptor if it is to a device. It does not make sense
	// to allow someone else to use this file descriptor at the same time,
	// or to use the device while someone else is using it.
	if ( (SUPER_SOCKET_TYPE_DEVICE == get_super_socket_type_from_path(path.c_str()))
	  && (flock(fd, LOCK_EX|LOCK_NB) < 0)
	) {
		// The only error we care about is EWOULDBLOCK. EINVAL is fine,
		// it just means this file descriptor doesn't support locking.
		if (EWOULDBLOCK == errno) {
			syslog(LOG_ERR, "Socket \"%s\" is locked by another process", path.c_str());
			throw std::runtime_error("Socket is locked by another process");
		}
	}
}

SuperSocket::~SuperSocket()
{
	SuperSocket::hibernate();
}

boost::shared_ptr<SocketWrapper>
SuperSocket::create(const std::string& path)
{
	return boost::shared_ptr<SocketWrapper>(new SuperSocket(path));
}

int
SuperSocket::hibernate(void)
{
	if (mFDRead >= 0) {
		// Unlock the FD.
		IGNORE_RETURN_VALUE(flock(mFDRead, LOCK_UN));

		// Close the existing FD.
		IGNORE_RETURN_VALUE(close_super_socket(mFDRead));
	}

	mFDRead = -1;
	mFDWrite = -1;

	return 0;
}

void
SuperSocket::reset()
{
	syslog(LOG_DEBUG, "SuperSocket::reset()");

	SuperSocket::hibernate();

	// Sleep for 200ms to wait for things to settle down.
	usleep(MSEC_PER_SEC * 200);

	mFDRead = mFDWrite = open_super_socket(mPath.c_str());

	if (mFDRead < 0) {
		// Unable to reopen socket...!
		syslog(LOG_ERR, "SuperSocket::Reset: Unable to reopen socket <%s>, errno=%d (%s)", mPath.c_str(), errno, strerror(errno));
		throw std::runtime_error("Unable to reopen socket");
	}

	// Lock the file descriptor. It does not make sense to allow someone else
	// to use this file descriptor at the same time, or to use the device
	// while someone else is using it.
	if ( (SUPER_SOCKET_TYPE_DEVICE == get_super_socket_type_from_path(mPath.c_str()))
	  && (flock(mFDRead, LOCK_EX|LOCK_NB) < 0)
	) {
		// The only error we care about is EWOULDBLOCK. EINVAL is fine,
		// it just means this file descriptor doesn't support locking.
		if (EWOULDBLOCK == errno) {
			syslog(LOG_ERR, "Socket is locked by another process");
			throw std::runtime_error("Socket is locked by another process");
		}
	}
}
