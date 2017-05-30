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
 *      This file declares the SocketWrapper class, which is the virtual
 *      base class for using things like TCP sockets, serial file descriptors,
 *      or even other processes.
 *
 */

#ifndef __wpantund__SocketWrapper__
#define __wpantund__SocketWrapper__

#include <stdint.h>
#include <cstring>
#include <boost/shared_ptr.hpp>
#include <climits>
#include <string>
#include <sys/select.h>
#include "time-utils.h"

namespace nl {

class SocketWrapper {
public:
	virtual ~SocketWrapper();
	virtual ssize_t write(const void* data, size_t len) = 0;
	virtual ssize_t read(void* data, size_t len) = 0;
	virtual bool can_read(void)const;
	virtual bool can_write(void)const;
	virtual int process(void) = 0;
	virtual int set_log_level(int log_level);

	virtual int get_read_fd(void)const;
	virtual int get_write_fd(void)const;
	virtual cms_t get_ms_to_next_event(void)const;

	virtual void send_break(void);

	virtual void reset(void);
	virtual bool did_reset(void);

	//! Any ancilary file descriptors to update. Only really needed for adapters.
	virtual int update_fd_set(fd_set *read_fd_set, fd_set *write_fd_set, fd_set *error_fd_set, int *max_fd, cms_t *timeout);

public:
	//! Special function which closes the file descriptors. Not supported on all sockets. Call `reset()` to undo.
	virtual int hibernate(void);

}; // class SocketWrapper


}; // namespace nl


#endif /* defined(__wpantund__SocketWrapper__) */
