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
 *      This file contains the declaration of the SocketAdapter class,
 *      which is a base class for implementing "soft" sockets that
 *      use other sockets (for things like reliability layers, etc).
 *
 */

#ifndef __wpantund__SocketAdapter__
#define __wpantund__SocketAdapter__

#include "SocketWrapper.h"

namespace nl {
class SocketAdapter : public SocketWrapper {
public:
	virtual ~SocketAdapter() {}
	virtual const boost::shared_ptr<SocketWrapper>& set_parent(boost::shared_ptr<SocketWrapper> parent);
	const boost::shared_ptr<SocketWrapper>& get_parent();

	virtual ssize_t write(const void* data, size_t len);
	virtual ssize_t read(void* data, size_t len);
	virtual bool can_read(void)const;
	virtual bool can_write(void)const;
	virtual int get_read_fd(void)const;
	virtual int get_write_fd(void)const;
	virtual int process(void);
	virtual cms_t get_ms_to_next_event(void)const;
	virtual void send_break();

	virtual void reset();
	virtual bool did_reset();
	virtual int update_fd_set(fd_set *read_fd_set, fd_set *write_fd_set, fd_set *error_fd_set, int *max_fd, cms_t *timeout);

	virtual int hibernate(void);

protected:
	SocketAdapter(boost::shared_ptr<SocketWrapper> parent);
	boost::shared_ptr<SocketWrapper> mParent;
}; // class SocketAdapter


}; // namespace nl

#endif /* defined(__wpantund__SocketAdapter__) */
