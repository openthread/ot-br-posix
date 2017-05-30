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
 *      Declaration of NetworkRetain class (Hooks for saving/restoring network info)
 *
 */

#ifndef __wpantund__NetworkRetain__
#define __wpantund__NetworkRetain__

#include <string>
#include "NCPTypes.h"

namespace nl {
namespace wpantund {

class NetworkRetain
{
public:
	NetworkRetain();
	~NetworkRetain();
	void set_network_retain_command(const std::string& command);
	void handle_ncp_state_change(NCPState new_ncp_state, NCPState old_ncp_state);

private:
	void save_network_info(void);
	void recall_network_info(void);
	void erase_network_info(void);
	void close_network_retain_fd(void);

private:
	int mNetworkRetainFD;
};

}; // namespace wpantund
}; // namespace nl

#endif /* defined(__wpantund__NetworkRetain__) */
