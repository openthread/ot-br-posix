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
#include "SpinelNCPTaskGetMsgBufferCounters.h"
#include "SpinelNCPInstance.h"
#include "spinel-extra.h"

using namespace nl;
using namespace nl::wpantund;

nl::wpantund::SpinelNCPTaskGetMsgBufferCounters::SpinelNCPTaskGetMsgBufferCounters(
	SpinelNCPInstance* instance,
	CallbackWithStatusArg1 cb,
	ResultFormat result_format
):	SpinelNCPTask(instance, cb), mResultFormat(result_format)
{
}

int
nl::wpantund::SpinelNCPTaskGetMsgBufferCounters::vprocess_event(int event, va_list args)
{
	int ret = kWPANTUNDStatus_Failure;
	unsigned int prop_key;
	const uint8_t *data_in;
	spinel_size_t data_len;
	spinel_ssize_t len;
	MsgBufferCounters counters;

	EH_BEGIN();

	if (!mInstance->mEnabled) {
		ret = kWPANTUNDStatus_InvalidWhenDisabled;
		finish(ret);
		EH_EXIT();
	}

	if (mInstance->get_ncp_state() == UPGRADING) {
		ret = kWPANTUNDStatus_InvalidForCurrentState;
		finish(ret);
		EH_EXIT();
	}

	// Wait for a bit to see if the NCP will enter the right state.
	EH_REQUIRE_WITHIN(
		NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
		!ncp_state_is_initializing(mInstance->get_ncp_state()),
		on_error
	);

	// The first event to a task is EVENT_STARTING_TASK. The following
	// line makes sure that we don't start processing this task
	// until it is properly scheduled. All tasks immediately receive
	// the initial `EVENT_STARTING_TASK` event, but further events
	// will only be received by that task once it is that task's turn
	// to execute.
	EH_WAIT_UNTIL(EVENT_STARTING_TASK != event);

	mNextCommand = SpinelPackData(
		SPINEL_FRAME_PACK_CMD_PROP_VALUE_GET,
		SPINEL_PROP_MSG_BUFFER_COUNTERS
	);

	EH_SPAWN(&mSubPT, vprocess_send_command(event, args));

	ret = mNextCommandRet;

	require_noerr(ret, on_error);

	require(EVENT_NCP_PROP_VALUE_IS == event, on_error);

	prop_key = va_arg(args, unsigned int);
	data_in = va_arg(args, const uint8_t*);
	data_len = va_arg_small(args, spinel_size_t);

	require(prop_key == SPINEL_PROP_MSG_BUFFER_COUNTERS, on_error);

	len = spinel_datatype_unpack(
		data_in,
		data_len,
		"SSSSSSSSSSSSSSSS",
		&counters.mTotalBuffers,
		&counters.mFreeBuffers,
		&counters.m6loSendMessages,
		&counters.m6loSendBuffers,
		&counters.m6loReassemblyMessages,
		&counters.m6loReassemblyBuffers,
		&counters.mIp6Messages,
		&counters.mIp6Buffers,
		&counters.mMplMessages,
		&counters.mMplBuffers,
		&counters.mMleMessages,
		&counters.mMleBuffers,
		&counters.mArpMessages,
		&counters.mArpBuffers,
		&counters.mCoapClientMessages,
		&counters.mCoapClientBuffers
	);

	require(len >=0, on_error);

	ret = kWPANTUNDStatus_Ok;

	if (mResultFormat == kResultFormat_StringArray)
	{
		finish(ret, counters.get_as_string_array());
	}
	else if (mResultFormat == kResultFormat_String)
	{
		finish(ret, counters.get_as_string());
	}
	else if (mResultFormat == kResultFormat_ValueMap)
	{
		finish(ret, counters.get_as_valuemap());
	}
	else
	{
		finish(ret);
	}

	EH_EXIT();

on_error:

	if (ret == kWPANTUNDStatus_Ok) {
		ret = kWPANTUNDStatus_Failure;
	}

	syslog(LOG_ERR, "Getting msg buffer counter failed: %d", ret);

	finish(ret);

	EH_END();
}

std::list<std::string>
SpinelNCPTaskGetMsgBufferCounters::MsgBufferCounters::get_as_string_array(void) const
{
	std::list<std::string> slist;
	char c_string[100];

	snprintf(c_string, sizeof(c_string), "mTotalBuffers = %d", mTotalBuffers);
	slist.push_back(c_string);
	snprintf(c_string, sizeof(c_string), "mFreeBuffers = %d", mFreeBuffers);
	slist.push_back(c_string);
	snprintf(c_string, sizeof(c_string), "m6loSendMessages = %d", m6loSendMessages);
	slist.push_back(c_string);
	snprintf(c_string, sizeof(c_string), "m6loSendBuffers = %d", m6loSendBuffers);
	slist.push_back(c_string);
	snprintf(c_string, sizeof(c_string), "m6loReassemblyMessages = %d", m6loReassemblyMessages);
	slist.push_back(c_string);
	snprintf(c_string, sizeof(c_string), "m6loReassemblyBuffers = %d", m6loReassemblyBuffers);
	slist.push_back(c_string);
	snprintf(c_string, sizeof(c_string), "mIp6Messages = %d", mIp6Messages);
	slist.push_back(c_string);
	snprintf(c_string, sizeof(c_string), "mIp6Buffers = %d", mIp6Buffers);
	slist.push_back(c_string);
	snprintf(c_string, sizeof(c_string), "mMplMessages = %d", mMplMessages);
	slist.push_back(c_string);
	snprintf(c_string, sizeof(c_string), "mMplBuffers = %d", mMplBuffers);
	slist.push_back(c_string);
	snprintf(c_string, sizeof(c_string), "mMleMessages = %d", mMleMessages);
	slist.push_back(c_string);
	snprintf(c_string, sizeof(c_string), "mMleBuffers = %d", mMleBuffers);
	slist.push_back(c_string);
	snprintf(c_string, sizeof(c_string), "mArpMessages = %d", mArpMessages);
	slist.push_back(c_string);
	snprintf(c_string, sizeof(c_string), "mArpBuffers = %d", mArpBuffers);
	slist.push_back(c_string);
	snprintf(c_string, sizeof(c_string), "mCoapClientMessages = %d", mCoapClientMessages);
	slist.push_back(c_string);
	snprintf(c_string, sizeof(c_string), "mCoapClientBuffers = %d", mCoapClientBuffers);
	slist.push_back(c_string);

	return slist;
}

std::string
SpinelNCPTaskGetMsgBufferCounters::MsgBufferCounters::get_as_string(void) const
{
	char c_string[800];

	snprintf(c_string, sizeof(c_string),
		"TotalBuffers = %-3d, "
		"FreeBuffers = %-3d, "
		"6loSendMessages = %-3d, "
		"6loSendBuffers = %-3d, "
		"6loReassemblyMessages = %-3d, "
		"6loReassemblyBuffers = %-3d, "
		"Ip6Messages = %-3d, "
		"Ip6Buffers = %-3d, "
		"MplMessages = %-3d, "
		"MplBuffers = %-3d, "
		"MleMessages = %-3d, "
		"MleBuffers = %-3d, "
		"ArpMessages = %-3d, "
		"ArpBuffers = %-3d, "
		"CoapClientMessages = %-3d, "
		"CoapClientBuffers = %-3d, ",
		mTotalBuffers,
		mFreeBuffers,
		m6loSendMessages,
		m6loSendBuffers,
		m6loReassemblyMessages,
		m6loReassemblyBuffers,
		mIp6Messages,
		mIp6Buffers,
		mMplMessages,
		mMplBuffers,
		mMleMessages,
		mMleBuffers,
		mArpMessages,
		mArpBuffers,
		mCoapClientMessages,
		mCoapClientBuffers
	);

	return std::string(c_string);
}

ValueMap
SpinelNCPTaskGetMsgBufferCounters::MsgBufferCounters::get_as_valuemap(void) const
{
	ValueMap vm;

	vm["TotalBuffers"] = mTotalBuffers;
	vm["FreeBuffers"] = mFreeBuffers;
	vm["6loSendMessages"] = m6loSendMessages;
	vm["6loSendBuffers"] = m6loSendBuffers;
	vm["6loReassemblyMessages"] = m6loReassemblyMessages;
	vm["6loReassemblyBuffers"] = m6loReassemblyBuffers;
	vm["Ip6Messages"] = mIp6Messages;
	vm["Ip6Buffers"] = mIp6Buffers;
	vm["MplMessages"] = mMplMessages;
	vm["MplBuffers"] = mMplBuffers;
	vm["MleMessages"] = mMleMessages;
	vm["MleBuffers"] = mMleBuffers;
	vm["ArpMessages"] = mArpMessages;
	vm["ArpBuffers"] = mArpBuffers;
	vm["CoapClientMessages"] = mCoapClientMessages;
	vm["CoapClientBuffers"] = mCoapClientBuffers;

	return vm;
}
