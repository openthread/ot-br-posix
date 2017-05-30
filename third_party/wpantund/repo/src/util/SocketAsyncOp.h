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

#ifndef __wpantund__SocketAsyncOp__
#define __wpantund__SocketAsyncOp__

#include <stdint.h>
#include "SocketWrapper.h"
#include "nlpt.h"
#include <errno.h>

namespace nl {

static inline int
read_stream_pt(struct nlpt *pt, nl::SocketWrapper* socket, void* data, size_t len)
{
	const int fd = socket->get_read_fd();

	PT_BEGIN(&pt->sub_pt);
	pt->byte_count = 0;
	pt->last_errno = 0;

	while (pt->byte_count < len) {
		ssize_t bytes_read;

		// Wait for the socket to become readable...
		_nlpt_setup_read_fd_source(pt, fd);
		PT_WAIT_UNTIL(&pt->sub_pt, nlpt_hook_check_read_fd_source(pt, fd) || socket->can_read());
		_nlpt_cleanup_read_fd_source(pt, fd);

		// Read what is left of the packet on the socket.
		bytes_read = socket->read(
			static_cast<void*>(static_cast<uint8_t*>(data) + pt->byte_count),
			len - pt->byte_count
		);

		if (0 > bytes_read) {
			pt->last_errno = errno;
			break;
		}

		pt->byte_count += bytes_read;
	}

	PT_END(&pt->sub_pt);
}

static inline int
write_stream_pt(struct nlpt *pt, nl::SocketWrapper* socket, const void* data, size_t len)
{
	const int fd = socket->get_write_fd();

	PT_BEGIN(&pt->sub_pt);
	pt->byte_count = 0;
	pt->last_errno = 0;

	while (pt->byte_count < len) {
		ssize_t bytes_written;

		// Wait for the socket to become writable...
		_nlpt_setup_write_fd_source(pt, fd);
		PT_WAIT_UNTIL(&pt->sub_pt, nlpt_hook_check_write_fd_source(pt, fd) || socket->can_write());
		_nlpt_cleanup_write_fd_source(pt, fd);

		// Attempt to write out what is left of the packet to the socket.
		bytes_written = socket->write(
			static_cast<const void*>(static_cast<const uint8_t*>(data) + pt->byte_count),
			len - pt->byte_count
		);

		if (0 > bytes_written) {
			pt->last_errno = errno;
			break;
		}

		pt->byte_count += bytes_written;
	}

	PT_END(&pt->sub_pt);
}

static inline int
write_packet_pt(struct nlpt *pt, nl::SocketWrapper* socket, const void* data, size_t len)
{
	const int fd = socket->get_write_fd();

	PT_BEGIN(&pt->sub_pt);
	ssize_t bytes_written;
	pt->byte_count = 0;
	pt->last_errno = 0;

	// Wait for the socket to become writable...
	_nlpt_setup_write_fd_source(pt, fd);
	PT_WAIT_UNTIL(&pt->sub_pt, nlpt_hook_check_write_fd_source(pt, fd) || socket->can_write());
	_nlpt_cleanup_write_fd_source(pt, fd);

	// Write out the packet
	bytes_written = socket->write(
		static_cast<const void*>(static_cast<const uint8_t*>(data) + pt->byte_count),
		len - pt->byte_count
	);

	if (0 > bytes_written) {
		pt->last_errno = errno;
	} else {
		pt->byte_count += bytes_written;
	}

	PT_END(&pt->sub_pt);
}

#define NLPT_ASYNC_READ_STREAM(pt, sock, data, len) \
		PT_SPAWN( \
			&(pt)->pt, \
			&(pt)->sub_pt, \
			::nl::read_stream_pt( \
				(pt), \
				(sock), \
				static_cast<void*>(data), \
				(len) \
			) \
		)

#define NLPT_ASYNC_WRITE_STREAM(pt, sock, data, len) \
		PT_SPAWN( \
			&(pt)->pt, \
			&(pt)->sub_pt, \
			::nl::write_stream_pt( \
				(pt), \
				(sock), \
				static_cast<const void*>(data), \
				(len) \
			) \
		)

#define NLPT_ASYNC_WRITE_PACKET(pt, sock, data, len) \
		PT_SPAWN( \
			&(pt)->pt, \
			&(pt)->sub_pt, \
			::nl::write_packet_pt( \
				(pt), \
				(sock), \
				static_cast<const void*>(data), \
				(len) \
			) \
		)

}; // namespace nl


#endif /* defined(__wpantund__SocketAsyncOp__) */
