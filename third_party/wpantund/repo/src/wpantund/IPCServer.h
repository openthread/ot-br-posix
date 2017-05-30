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

#ifndef wpantund_ipc_server_h
#define wpantund_ipc_server_h

#include <sys/select.h>
#include <unistd.h>
#include "time-utils.h"

namespace nl {
namespace wpantund {

class NCPControlInterface;

class IPCServer {
public:
	virtual ~IPCServer() {}
	virtual cms_t get_ms_to_next_event(void) = 0;
	virtual void process(void) = 0;

	virtual int update_fd_set(fd_set *read_fd_set, fd_set *write_fd_set, fd_set *error_fd_set, int *max_fd, cms_t *timeout) = 0;

	virtual int add_interface(NCPControlInterface* instance) = 0;
};

};
};

#endif
