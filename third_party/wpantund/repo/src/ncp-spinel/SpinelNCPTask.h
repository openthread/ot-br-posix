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

#ifndef __wpantund__SpinelNCPTask__
#define __wpantund__SpinelNCPTask__

#include "NCPInstanceBase.h"
#include "nlpt.h"
#include "SocketWrapper.h"
#include "SocketAsyncOp.h"

#include <boost/enable_shared_from_this.hpp>
#include <queue>
#include <set>
#include <map>
#include <errno.h>
#include "spinel.h"

namespace nl {
namespace wpantund {

class SpinelNCPInstance;

class SpinelNCPTask
	: public boost::enable_shared_from_this<SpinelNCPTask>
	, public boost::signals2::trackable
	, public nl::EventHandler
{
public:
	SpinelNCPTask(SpinelNCPInstance* _instance, CallbackWithStatusArg1 cb);
	virtual ~SpinelNCPTask();

	virtual int vprocess_event(int event, va_list args) = 0;

	virtual void finish(int status, const boost::any& value = boost::any());

	bool peek_callback_is_prop_value_is(int event, va_list args, spinel_prop_key_t);

	SpinelNCPInstance* mInstance;

	int vprocess_send_command(int event, va_list args);

protected:
	CallbackWithStatusArg1 mCB;
	uint8_t mLastHeader;
	PT mSubPT;
	Data mNextCommand;
	int mNextCommandRet;
	int mNextCommandTimeout;
};

nl::Data SpinelPackData(const char* pack_format, ...);

}; // namespace wpantund
}; // namespace nl

#endif /* defined(__wpantund__SpinelNCPInstance__) */
