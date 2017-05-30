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
#include "SpinelNCPTaskWake.h"
#include "SpinelNCPInstance.h"

using namespace nl;
using namespace nl::wpantund;

nl::wpantund::SpinelNCPTaskWake::SpinelNCPTaskWake(
	SpinelNCPInstance* instance,
	CallbackWithStatusArg1 cb
):	SpinelNCPTask(instance, cb)
{
}

void
nl::wpantund::SpinelNCPTaskWake::finish(int status, const boost::any& value)
{
	mInstance->mResetIsExpected = false;

	SpinelNCPTask::finish(status, value);
}


int
nl::wpantund::SpinelNCPTaskWake::vprocess_event(int event, va_list args)
{
	int ret = kWPANTUNDStatus_Failure;

	EH_BEGIN();

	// The first event to a task is EVENT_STARTING_TASK. The following
	// line makes sure that we don't start processing this task
	// until it is properly scheduled. All tasks immediately receive
	// the initial `EVENT_STARTING_TASK` event, but further events
	// will only be received by that task once it is that task's turn
	// to execute.
	EH_WAIT_UNTIL(EVENT_STARTING_TASK != event);

	mInstance->set_ncp_power(true);
	mInstance->mResetIsExpected = true;

	CONTROL_REQUIRE_PREP_TO_SEND_COMMAND_WITHIN(NCP_DEFAULT_COMMAND_SEND_TIMEOUT, on_error);
	GetInstance(this)->mOutboundBufferLen = spinel_datatype_pack(GetInstance(this)->mOutboundBuffer, sizeof(GetInstance(this)->mOutboundBuffer), "Ci", 0, SPINEL_CMD_NOOP);
	CONTROL_REQUIRE_OUTBOUND_BUFFER_FLUSHED_WITHIN(NCP_DEFAULT_COMMAND_SEND_TIMEOUT, on_error);

	EH_REQUIRE_WITHIN(
		NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
		!ncp_state_is_sleeping(mInstance->get_ncp_state())
		  && (!ncp_state_is_initializing(mInstance->get_ncp_state())),
		on_error
	);

	ret = kWPANTUNDStatus_Ok;

	finish(ret);

	EH_EXIT();

on_error:

	if (ret == kWPANTUNDStatus_Ok) {
		ret = kWPANTUNDStatus_Failure;
	}

	syslog(LOG_ERR, "Wake failed: %d", ret);

	mInstance->reinitialize_ncp();

	finish(ret);

	EH_END();
}
