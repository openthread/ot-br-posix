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

#ifndef __wpantund__UnixSocket__
#define __wpantund__UnixSocket__

#include "SocketWrapper.h"
#include <stdio.h>
#include <unistd.h>

namespace nl {
class UnixSocket : public SocketWrapper {
protected:
	UnixSocket(int rfd, int wfd, bool should_close);
	UnixSocket(int fd, bool should_close);

public:
	static boost::shared_ptr<SocketWrapper> create(int fd, bool should_close = false);
	static boost::shared_ptr<SocketWrapper> create(int rfd, int wfd, bool should_close = false);
	virtual ~UnixSocket();
	virtual ssize_t write(const void* data, size_t len);
	virtual ssize_t read(void* data, size_t len);
	virtual bool can_read(void)const;
	virtual bool can_write(void)const;
	virtual int get_read_fd(void)const;
	virtual int get_write_fd(void)const;
	virtual int process(void);
	virtual void send_break();
	virtual int set_log_level(int log_level);

protected:
	bool mShouldClose;
	int mFDRead;
	int mFDWrite;
	int mLogLevel;
}; // class UnixSocket

}; // namespace nl

#endif /* defined(__wpantund__UnixSocket__) */
