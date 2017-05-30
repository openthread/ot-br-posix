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

#include "UnixSocket.h"
#include <errno.h>
#include "socket-utils.h"
#include <termios.h>
#include <sys/file.h>
#include <syslog.h>
#include <poll.h>

using namespace nl;

#define SOCKET_DEBUG_BYTES_PER_LINE			16

UnixSocket::UnixSocket(int rfd, int wfd, bool should_close)
	:mShouldClose(should_close), mFDRead(rfd), mFDWrite(wfd), mLogLevel(-1)
{
}

UnixSocket::UnixSocket(int fd, bool should_close)
	:mShouldClose(should_close), mFDRead(fd), mFDWrite(fd), mLogLevel(-1)
{
}

UnixSocket::~UnixSocket()
{
	if (mShouldClose) {
		close(mFDRead);

		if(mFDWrite != mFDRead) {
			close(mFDWrite);
		}
	}
}

boost::shared_ptr<SocketWrapper>
UnixSocket::create(int rfd, int wfd, bool should_close)
{
	return boost::shared_ptr<SocketWrapper>(new UnixSocket(rfd,wfd,should_close));
}

boost::shared_ptr<SocketWrapper>
UnixSocket::create(int fd, bool should_close)
{
	return UnixSocket::create(fd, fd, should_close);
}

ssize_t
UnixSocket::write(const void* data, size_t len)
{
	ssize_t ret = ::write(mFDWrite, data, len);
	if(ret<0) {
		ret = -errno;
	} else if(ret == 0) {
		ret = fd_has_error(mFDWrite);
#if DEBUG
	} else if (mLogLevel != -1) {
		const uint8_t* byte = (uint8_t*)data;
		ssize_t i = 0;

		while(i<ret) {
			ssize_t j = 0;
			char dump_string[SOCKET_DEBUG_BYTES_PER_LINE*3+1];

			for (j = 0;i<ret && j<SOCKET_DEBUG_BYTES_PER_LINE;i++,j++) {
				sprintf(dump_string+j*3, "%02X ", byte[i]);
			}

			syslog(mLogLevel, "UnixSocket: %3d Byte(s) sent to FD%d:   %s%s", (int)j, mFDWrite, dump_string, (i<ret)?" ...":"");
		}
#endif
	}
	return ret;
}

ssize_t
UnixSocket::read(void* data, size_t len)
{
	ssize_t ret = ::read(mFDRead, data, len);
	if(ret<0) {
		if(EAGAIN == errno) {
			ret = 0;
		} else {
			ret = -errno;
		}
#if DEBUG
	} else if ((ret > 0) && (mLogLevel != -1)) {
		const uint8_t* byte = (uint8_t*)data;
		ssize_t i = 0;

		while(i<ret) {
			ssize_t j = 0;
			char dump_string[SOCKET_DEBUG_BYTES_PER_LINE*3+1];

			for (j = 0;i<ret && j<SOCKET_DEBUG_BYTES_PER_LINE;i++,j++) {
				sprintf(dump_string+j*3, "%02X ", byte[i]);
			}

			syslog(mLogLevel, "UnixSocket: %3d Byte(s) read from FD%d: %s%s", (int)j, mFDWrite, dump_string, (i<ret)?" ...":"");
		}
#endif
	}

	if(ret==0) {
		ret = fd_has_error(mFDRead);
	}

	return ret;
}

bool
UnixSocket::can_read(void)const
{
	bool ret = false;
	const int flags = POLLRDNORM|POLLERR|POLLNVAL|POLLHUP;
	struct pollfd pollfd = { mFDRead, flags, 0 };
	int count = poll(&pollfd, 1, 0);
	ret = (count>0) && ((pollfd.revents & flags) != 0);
	return ret;
}

bool
UnixSocket::can_write(void)const
{
	const int flags = POLLOUT|POLLERR|POLLNVAL|POLLHUP;
	struct pollfd pollfd = { mFDWrite, flags, 0 };

	return (poll(&pollfd, 1, 0)>0) && ((pollfd.revents & flags) != 0);
}

void
UnixSocket::send_break()
{
	if (isatty(mFDWrite)) {
#if DEBUG
		syslog(mLogLevel, "UnixSocket: Sending BREAK");
#endif
		tcsendbreak(mFDWrite, 0);
	}
};

int
UnixSocket::get_read_fd(void)const
{
	return mFDRead;
}

int
UnixSocket::get_write_fd(void)const
{
	return mFDWrite;
}

int
UnixSocket::process(void)
{
	return 0;
}

int
UnixSocket::set_log_level(int log_level)
{
	mLogLevel = log_level;

	return 0;
}
