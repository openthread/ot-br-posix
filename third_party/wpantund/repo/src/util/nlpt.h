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
 *      Flavor of protothreads for handling asynchronous I/O.
 *
 */

#ifndef wpantund_nlpt_h
#define wpantund_nlpt_h

#include "pt.h"

#if USING_GLIB
#include "nlpt-glib.h"
#else
#include "nlpt-select.h"
#endif

#define NLPT_INIT(nlpt) do { memset(nlpt, 0, sizeof(*nlpt)); PT_INIT(&(nlpt)->pt); _nlpt_init(nlpt); } while (0)

#define NLPT_THREAD(name_args)          PT_THREAD(name_args)
#define NLPT_BEGIN(nlpt)                PT_BEGIN(&(nlpt)->pt)
#define NLPT_END(nlpt)                  PT_END(&(nlpt)->pt)
#define NLPT_SPAWN(nlpt, child, thread) PT_SPAWN(&(nlpt)->pt, &(child)->pt, thread)
#define NLPT_WAIT_UNTIL(nlpt, cond)     PT_WAIT_UNTIL(&(nlpt)->pt, cond)
#define NLPT_WAIT_WHILE(nlpt, cond)     PT_WAIT_WHILE(&(nlpt)->pt, cond)
#define NLPT_RESTART(nlpt)              PT_RESTART(&(nlpt)->pt)
#define NLPT_EXIT(nlpt)                 PT_EXIT(&(nlpt)->pt)
#define NLPT_YIELD(nlpt)                PT_YIELD(&(nlpt)->pt)
#define NLPT_YIELD_UNTIL(nlpt, cond)    PT_YIELD_UNTIL(&(nlpt)->pt, cond)

//! Waits until one of the two given file descriptors are readable or the condition is satisfied.
#define NLPT_WAIT_UNTIL_READABLE2_OR_COND(nlpt, fd_, fd2_, c) \
	do {                                                     \
		_nlpt_setup_read_fd_source(nlpt, fd_); \
		_nlpt_setup_read_fd_source(nlpt, fd2_); \
		NLPT_WAIT_UNTIL(nlpt, \
						 nlpt_hook_check_read_fd_source(nlpt, fd_) \
						 || nlpt_hook_check_read_fd_source(nlpt, fd2_) \
						 || (c)); \
		_nlpt_cleanup_read_fd_source(nlpt, fd2_); \
		_nlpt_cleanup_read_fd_source(nlpt, fd_); \
	} while (0)

#define NLPT_WAIT_UNTIL_READABLE_OR_COND(nlpt, fd_, c) \
	do {                                                     \
		_nlpt_setup_read_fd_source(nlpt, fd_); \
		NLPT_WAIT_UNTIL(nlpt, \
						 nlpt_hook_check_read_fd_source(nlpt, fd_) || (c)); \
		_nlpt_cleanup_read_fd_source(nlpt, fd_); \
	} while (0)

#define NLPT_WAIT_UNTIL_WRITABLE_OR_COND(nlpt, fd_, c) \
	do {                                                     \
		_nlpt_setup_write_fd_source(nlpt, fd_); \
		NLPT_WAIT_UNTIL(nlpt, \
						 nlpt_hook_check_write_fd_source(nlpt, fd_) || (c)); \
		_nlpt_cleanup_write_fd_source(nlpt, fd_); \
	} while (0)

#define NLPT_YIELD_UNTIL_READABLE2_OR_COND(nlpt, fd_, fd2_, c) \
	do {                                                     \
		_nlpt_setup_read_fd_source(nlpt, fd_); \
		_nlpt_setup_read_fd_source(nlpt, fd2_); \
		NLPT_YIELD_UNTIL(nlpt, \
						 nlpt_hook_check_read_fd_source(nlpt, fd_) \
						 || nlpt_hook_check_read_fd_source(nlpt, fd2_) \
						 || (c)); \
		_nlpt_cleanup_read_fd_source(nlpt, fd2_); \
		_nlpt_cleanup_read_fd_source(nlpt, fd_); \
	} while (0)

#define NLPT_YIELD_UNTIL_READABLE_OR_COND(nlpt, fd_, c) \
	do {                                                     \
		_nlpt_setup_read_fd_source(nlpt, fd_); \
		NLPT_YIELD_UNTIL(nlpt, \
						 nlpt_hook_check_read_fd_source(nlpt, fd_) || (c)); \
		_nlpt_cleanup_read_fd_source(nlpt, fd_); \
	} while (0)

#define NLPT_YIELD_UNTIL_WRITABLE_OR_COND(nlpt, fd_, c) \
	do {                                                     \
		_nlpt_setup_write_fd_source(nlpt, fd_); \
		NLPT_YIELD_UNTIL(nlpt, \
						 nlpt_hook_check_write_fd_source(nlpt, fd_) || (c)); \
		_nlpt_cleanup_write_fd_source(nlpt, fd_); \
	} while (0)

#define NLPT_WAIT_UNTIL_READABLE(nlpt, fd) \
	NLPT_WAIT_UNTIL_READABLE_OR_COND( \
	    nlpt, \
	    fd, \
	    0)

#define NLPT_WAIT_UNTIL_WRITABLE(nlpt, fd) \
	NLPT_WAIT_UNTIL_WRITABLE_OR_COND( \
	    nlpt, \
	    fd, \
	    0)

#define NLPT_YIELD_UNTIL_READABLE(nlpt, fd) \
	NLPT_YIELD_UNTIL_READABLE_OR_COND( \
	    nlpt, \
	    fd, \
	    0)

#define NLPT_YIELD_UNTIL_WRITABLE(nlpt, fd) \
	NLPT_YIELD_UNTIL_WRITABLE_OR_COND( \
	    nlpt, \
	    fd, \
	    0)


#endif
