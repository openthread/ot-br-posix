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
 *      Flavor of protothreads for handling asynchronous I/O, via select()
 *
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#define _XOPEN_SOURCE 1 // For the "fds_bits" member of "fd_set"
#define __USE_XOPEN   1 // This too

#include <stdio.h>
#include <stdint.h>
#include "nlpt.h"
#include "nlpt-select.h"
#include "assert-macros.h"

static void
fd_set_merge(const fd_set *src, fd_set *dest, int fd_count)
{
	int i;
	const int32_t* src_data = (const int32_t*)src->fds_bits;
	int32_t* dest_data = (int32_t*)dest->fds_bits;

	for (i = (fd_count+31)/32; i > 0 ; --i) {
		*dest_data++ |= *src_data++;
	}
}

void
nlpt_select_update_fd_set(
	const struct nlpt* nlpt,
	fd_set *read_fd_set,
	fd_set *write_fd_set,
	fd_set *error_fd_set,
	int *max_fd
) {
	if ((max_fd != NULL) && (*max_fd < nlpt->max_fd)) {
		*max_fd = nlpt->max_fd;
	}

	if (read_fd_set != NULL) {
		fd_set_merge(&nlpt->read_fds, read_fd_set, nlpt->max_fd + 1);
	}

	if (write_fd_set != NULL) {
		fd_set_merge(&nlpt->write_fds, write_fd_set, nlpt->max_fd + 1);
	}

	if (error_fd_set != NULL) {
		fd_set_merge(&nlpt->error_fds, error_fd_set, nlpt->max_fd + 1);
	}
}

bool
_nlpt_checkpoll(int fd, short poll_flags)
{
	bool ret = false;

	if (fd >= 0) {
		struct pollfd pollfd = { fd, poll_flags, 0 };
		IGNORE_RETURN_VALUE( poll(&pollfd, 1, 0) );
		ret = ((pollfd.revents & poll_flags) != 0);
	}

	return ret;
}

void
_nlpt_cleanup_all(struct nlpt* nlpt)
{
	nlpt->max_fd = -1;
	FD_ZERO(&nlpt->read_fds);
	FD_ZERO(&nlpt->write_fds);
	FD_ZERO(&nlpt->error_fds);
}

void
_nlpt_cleanup_read_fd_source(struct nlpt* nlpt, int fd)
{
	if (fd >= 0) {
		FD_CLR(fd, &nlpt->read_fds);
		FD_CLR(fd, &nlpt->error_fds);
	}
}

void
_nlpt_cleanup_write_fd_source(struct nlpt* nlpt, int fd)
{
	if (fd >= 0) {
		FD_CLR(fd, &nlpt->write_fds);
		FD_CLR(fd, &nlpt->error_fds);
	}
}

void
_nlpt_setup_read_fd_source(struct nlpt* nlpt, int fd)
{
	if (fd >= 0) {
		if (fd > nlpt->max_fd) {
			nlpt->max_fd = fd;
		}
		FD_SET(fd, &nlpt->read_fds);
		FD_SET(fd, &nlpt->error_fds);
	}
}

void
_nlpt_setup_write_fd_source(struct nlpt* nlpt, int fd)
{
	if (fd >= 0) {
		if (fd > nlpt->max_fd) {
			nlpt->max_fd = fd;
		}
		FD_SET(fd, &nlpt->write_fds);
		FD_SET(fd, &nlpt->error_fds);
	}
}

void
_nlpt_init(struct nlpt* nlpt)
{
	_nlpt_cleanup_all(nlpt);
}
