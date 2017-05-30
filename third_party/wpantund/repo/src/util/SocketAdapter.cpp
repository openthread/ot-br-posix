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
 *      This file contains the implementation of the SocketAdapter class,
 *      which is a base class for implementing "soft" sockets that
 *      use other sockets (for things like reliability layers, etc).
 *
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "SocketAdapter.h"
#include <errno.h>

using namespace nl;

const boost::shared_ptr<SocketWrapper>&
SocketAdapter::set_parent(boost::shared_ptr<SocketWrapper> parent)
{
	return mParent = parent;
}

const boost::shared_ptr<SocketWrapper>&
SocketAdapter::get_parent()
{
	return mParent;
}

SocketAdapter::SocketAdapter(boost::shared_ptr<SocketWrapper> parent)
	:mParent(parent)
{
}

int
SocketAdapter::hibernate(void)
{
	return mParent ? mParent->hibernate() : -EINVAL;
}

ssize_t
SocketAdapter::write(const void* data, size_t len)
{
	return mParent ? mParent->write(data, len) : -EINVAL;
}

ssize_t
SocketAdapter::read(void* data, size_t len)
{
	return mParent ? mParent->read(data, len) : -EINVAL;
}

bool
SocketAdapter::can_read(void)const
{
	return mParent ? mParent->can_read() : false;
}

bool
SocketAdapter::can_write(void)const
{
	return mParent ? mParent->can_write() : false;
}

int
SocketAdapter::get_read_fd(void)const
{
	return mParent ? mParent->get_read_fd() : -EINVAL;
}

int
SocketAdapter::get_write_fd(void)const
{
	return mParent ? mParent->get_write_fd() : -EINVAL;
}

int
SocketAdapter::process(void)
{
	return mParent ? mParent->process() : 0;
}

void
SocketAdapter::reset()
{
	if(mParent)
		mParent->reset();
}

void
SocketAdapter::send_break()
{
	if(mParent)
		mParent->send_break();
};

bool
SocketAdapter::did_reset()
{
	return mParent ? mParent->did_reset() : false;
}

cms_t
SocketAdapter::get_ms_to_next_event(void)const
{
	return mParent ? mParent->get_ms_to_next_event() : CMS_DISTANT_FUTURE;
}

int
SocketAdapter::update_fd_set(fd_set *read_fd_set, fd_set *write_fd_set, fd_set *error_fd_set, int *max_fd, cms_t *timeout)
{
	return mParent ? mParent->update_fd_set(read_fd_set, write_fd_set, error_fd_set, max_fd, timeout) : 0;
}
