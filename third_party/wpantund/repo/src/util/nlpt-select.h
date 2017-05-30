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

#ifndef wpantund_nlpt_select_h
#define wpantund_nlpt_select_h

#include <stdbool.h>
#include <poll.h>
#include <sys/select.h>
#include <sys/cdefs.h>

__BEGIN_DECLS

struct nlpt {
	struct pt pt;
	struct pt sub_pt;
	size_t byte_count;
	int last_errno;

	fd_set read_fds;
	fd_set write_fds;
	fd_set error_fds;
	int max_fd;
};

/* ========================================================================= */
/* Private, Back-end-specific functions */

extern void _nlpt_init(struct nlpt* nlpt);
extern void _nlpt_cleanup_all(struct nlpt* nlpt);
extern void _nlpt_cleanup_read_fd_source(struct nlpt* nlpt, int fd);
extern void _nlpt_cleanup_write_fd_source(struct nlpt* nlpt, int fd);
extern void _nlpt_setup_read_fd_source(struct nlpt* nlpt, int fd);
extern void _nlpt_setup_write_fd_source(struct nlpt* nlpt, int fd);

/* ========================================================================= */
/* Protected, Back-end-specific hooks (for async I/O) */

extern bool nlpt_hook_check_read_fd_source(struct nlpt* nlpt, int fd);
extern bool nlpt_hook_check_write_fd_source(struct nlpt* nlpt, int fd);

/* ========================================================================= */
/* Public, Back-end-specific functions (for async I/O) */

extern void nlpt_select_update_fd_set(
	const struct nlpt* nlpt,
	fd_set *read_fd_set,
	fd_set *write_fd_set,
	fd_set *error_fd_set,
	int *max_fd
);
extern bool _nlpt_checkpoll(int fd, short poll_flags);

__END_DECLS

#endif
