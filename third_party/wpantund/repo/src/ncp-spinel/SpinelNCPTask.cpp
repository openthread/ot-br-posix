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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "assert-macros.h"
#include <syslog.h>
#include <errno.h>
#include "SpinelNCPTask.h"
#include "SpinelNCPInstance.h"

using namespace nl;
using namespace nl::wpantund;

SpinelNCPTask::SpinelNCPTask(SpinelNCPInstance* _instance, CallbackWithStatusArg1 cb):
	mInstance(_instance), mCB(cb), mNextCommandTimeout(NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT)
{
}

SpinelNCPTask::~SpinelNCPTask()
{
	finish(kWPANTUNDStatus_Canceled);
}

void
SpinelNCPTask::finish(int status, const boost::any& value)
{
	if (!mCB.empty()) {
		mCB(status, value);
		mCB = CallbackWithStatusArg1();
	}
}

static bool
spinel_callback_is_reset(int event, va_list args)
{
	int status = peek_ncp_callback_status(event, args);
	return (status >= SPINEL_STATUS_RESET__BEGIN)
	    && (status < SPINEL_STATUS_RESET__END);
}

int
SpinelNCPTask::vprocess_send_command(int event, va_list args)
{
	EH_BEGIN_SUB(&mSubPT);

	require(mNextCommand.size() < sizeof(GetInstance(this)->mOutboundBuffer), on_error);

	CONTROL_REQUIRE_PREP_TO_SEND_COMMAND_WITHIN(NCP_DEFAULT_COMMAND_SEND_TIMEOUT, on_error);
	memcpy(GetInstance(this)->mOutboundBuffer, mNextCommand.data(), mNextCommand.size());
	GetInstance(this)->mOutboundBufferLen = static_cast<spinel_ssize_t>(mNextCommand.size());
	CONTROL_REQUIRE_OUTBOUND_BUFFER_FLUSHED_WITHIN(NCP_DEFAULT_COMMAND_SEND_TIMEOUT, on_error);

	if (mNextCommand[1] == SPINEL_CMD_RESET) {
		mInstance->mResetIsExpected = true;
		EH_REQUIRE_WITHIN(
			mNextCommandTimeout,
			IS_EVENT_FROM_NCP(event)
			  && ( (GetInstance(this)->mInboundHeader == mLastHeader)
			    || spinel_callback_is_reset(event, args)
			  ),
			on_error
		);
		mNextCommandRet = kWPANTUNDStatus_Ok;

	} else {
		CONTROL_REQUIRE_COMMAND_RESPONSE_WITHIN(mNextCommandTimeout, on_error);
		mNextCommandRet = peek_ncp_callback_status(event, args);
	}

	if (mNextCommandRet) {
		mNextCommandRet = spinel_status_to_wpantund_status(mNextCommandRet);
	}

	EH_EXIT();

on_error:
	mNextCommandRet = kWPANTUNDStatus_Timeout;

	EH_END();
}

nl::Data
nl::wpantund::SpinelPackData(const char* pack_format, ...)
{
	Data ret(64);

	va_list args;
	va_start(args, pack_format);

	do {
		spinel_ssize_t packed_size = spinel_datatype_vpack(ret.data(), (spinel_size_t)ret.size(), pack_format, args);

		if (packed_size < 0) {
			ret.clear();
		} else if (packed_size > ret.size()) {
			ret.resize(packed_size);
			continue;
		} else {
			ret.resize(packed_size);
		}
		break;
	} while(true);

	va_end(args);
	return ret;
}
