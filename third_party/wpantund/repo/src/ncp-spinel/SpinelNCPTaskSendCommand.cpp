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
#include "SpinelNCPTaskSendCommand.h"
#include "SpinelNCPInstance.h"
#include "spinel-extra.h"

using namespace nl;
using namespace nl::wpantund;

static int simple_unpacker(const uint8_t* data_in, spinel_size_t data_len, const std::string& pack_format,
							boost::any& result);

SpinelNCPTaskSendCommand::Factory::Factory(SpinelNCPInstance* instance):
	mInstance(instance),
	mCb(NilReturn()),
	mTimeout(NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT),
	mLockProperty(0)
{
}

SpinelNCPTaskSendCommand::Factory&
SpinelNCPTaskSendCommand::Factory::set_callback(const CallbackWithStatusArg1 &cb)
{
	mCb = cb;
	return *this;
}

SpinelNCPTaskSendCommand::Factory&
SpinelNCPTaskSendCommand::Factory::set_callback(const CallbackWithStatus &cb)
{
	mCb = boost::bind(cb, _1);
	return *this;
}

SpinelNCPTaskSendCommand::Factory&
SpinelNCPTaskSendCommand::Factory::add_command(const Data& command)
{
	mCommandList.insert(mCommandList.end(), command);
	return *this;
}

SpinelNCPTaskSendCommand::Factory&
SpinelNCPTaskSendCommand::Factory::set_timeout(int timeout)
{
	mTimeout = timeout;
	return *this;
}

SpinelNCPTaskSendCommand::Factory&
SpinelNCPTaskSendCommand::Factory::set_reply_format(const std::string& packed_format)
{
	mReplyUnpacker = boost::bind(simple_unpacker, _1, _2, packed_format, _3);
	return *this;
}

SpinelNCPTaskSendCommand::Factory&
SpinelNCPTaskSendCommand::Factory::set_reply_unpacker(const ReplyUnpacker& reply_unpacker)
{
	mReplyUnpacker = reply_unpacker;
	return *this;
}

SpinelNCPTaskSendCommand::Factory&
SpinelNCPTaskSendCommand::Factory::set_lock_property(int lock_property)
{
	mLockProperty = lock_property;
	return *this;
}

boost::shared_ptr<SpinelNCPTask>
SpinelNCPTaskSendCommand::Factory::finish(void)
{
	return boost::shared_ptr<SpinelNCPTask>(new SpinelNCPTaskSendCommand(*this));
}

nl::wpantund::SpinelNCPTaskSendCommand::SpinelNCPTaskSendCommand(
	const Factory& factory
):	SpinelNCPTask(factory.mInstance, factory.mCb),
	mCommandList(factory.mCommandList),
	mLockProperty(factory.mLockProperty),
	mReplyUnpacker(factory.mReplyUnpacker),
	mRetVal(kWPANTUNDStatus_Failure)
{
	mNextCommandTimeout = factory.mTimeout;
}

static boost::any
spinel_iter_to_any(spinel_datatype_iter_t *iter)
{
	boost::any ret;
	spinel_status_t status;

	switch(iter->pack_format[0]) {
	case SPINEL_DATATYPE_BOOL_C:
		{
			bool val(0);
			status = spinel_datatype_iter_unpack(iter, &val);
			require_noerr(status, bail);
			ret = val;
		}
		break;

	case SPINEL_DATATYPE_UINT8_C:
		{
			uint8_t val(0);
			status = spinel_datatype_iter_unpack(iter, &val);
			require_noerr(status, bail);
			ret = val;
		}
		break;

	case SPINEL_DATATYPE_INT8_C:
		{
			int8_t val(0);
			status = spinel_datatype_iter_unpack(iter, &val);
			require_noerr(status, bail);
			ret = (int)val;
		}
		break;

	case SPINEL_DATATYPE_UINT16_C:
		{
			uint16_t val(0);
			status = spinel_datatype_iter_unpack(iter, &val);
			require_noerr(status, bail);
			ret = val;
		}
		break;

	case SPINEL_DATATYPE_INT16_C:
		{
			int16_t val(0);
			status = spinel_datatype_iter_unpack(iter, &val);
			require_noerr(status, bail);
			ret = val;
		}
		break;

	case SPINEL_DATATYPE_UINT32_C:
		{
			uint32_t val(0);
			status = spinel_datatype_iter_unpack(iter, &val);
			require_noerr(status, bail);
			ret = val;
		}
		break;

	case SPINEL_DATATYPE_INT32_C:
		{
			int32_t val(0);
			status = spinel_datatype_iter_unpack(iter, &val);
			require_noerr(status, bail);
			ret = val;
		}
		break;

	case SPINEL_DATATYPE_UINT_PACKED_C:
		{
			unsigned int val(0);
			status = spinel_datatype_iter_unpack(iter, &val);
			require_noerr(status, bail);
			ret = val;
		}
		break;

	case SPINEL_DATATYPE_IPv6ADDR_C:
		{
			struct in6_addr *val = NULL;
			status = spinel_datatype_iter_unpack(iter, &val);
			require_noerr(status, bail);
			ret = in6_addr_to_string(*val);
		}
		break;

	case SPINEL_DATATYPE_EUI64_C:
		{
			const spinel_eui64_t *val(NULL);
			status = spinel_datatype_iter_unpack(iter, &val);
			require_noerr(status, bail);
			ret = Data(val->bytes, sizeof(val->bytes));
		}
		break;

	case SPINEL_DATATYPE_EUI48_C:
		{
			const spinel_eui48_t *val(NULL);
			status = spinel_datatype_iter_unpack(iter, &val);
			require_noerr(status, bail);
			ret = Data(val->bytes, sizeof(val->bytes));
		}
		break;

	case SPINEL_DATATYPE_DATA_C:
		{
			const uint8_t *val_ptr(NULL);
			spinel_size_t val_len;
			status = spinel_datatype_iter_unpack(iter, &val_ptr, &val_len);
			require_noerr(status, bail);
			ret = Data(val_ptr, val_len);
		}
		break;

	case SPINEL_DATATYPE_UTF8_C:
		{
			const char *val(NULL);
			status = spinel_datatype_iter_unpack(iter, &val);
			require_noerr(status, bail);
			ret = std::string(val);
		}
		break;

	case SPINEL_DATATYPE_STRUCT_C:
		goto bail;

	case SPINEL_DATATYPE_ARRAY_C:
		// TODO: Recursively parse this
		goto bail;

	default:
		goto bail;

	}

bail:
	return ret;
}

static boost::any
spinel_packed_to_any(const uint8_t* data_in, spinel_size_t data_len, const char* pack_format)
{
	spinel_datatype_iter_t spinel_iter = {};
	spinel_datatype_iter_start(&spinel_iter, data_in, data_len, pack_format);

	return spinel_iter_to_any(&spinel_iter);
}

static int
simple_unpacker(const uint8_t* data_in, spinel_size_t data_len, const std::string& pack_format, boost::any& result)
{
	int retval = kWPANTUNDStatus_Ok;

	try {
		result = spinel_packed_to_any(data_in, data_len, pack_format.c_str());

	} catch(...) {
		retval = kWPANTUNDStatus_Failure;
	}

	return retval;
}

int
nl::wpantund::SpinelNCPTaskSendCommand::vprocess_event(int event, va_list args)
{
	EH_BEGIN();

	mRetVal = kWPANTUNDStatus_Failure;

	// The first event to a task is EVENT_STARTING_TASK. The following
	// line makes sure that we don't start processing this task
	// until it is properly scheduled. All tasks immediately receive
	// the initial `EVENT_STARTING_TASK` event, but further events
	// will only be received by that task once it is that task's turn
	// to execute.
	EH_WAIT_UNTIL(EVENT_STARTING_TASK != event);

	if (mLockProperty != 0) {
		mNextCommand = SpinelPackData(
			SPINEL_FRAME_PACK_CMD_PROP_VALUE_SET(SPINEL_DATATYPE_BOOL_S),
			mLockProperty,
			true
		);

		EH_SPAWN(&mSubPT, vprocess_send_command(event, args));

		mRetVal = mNextCommandRet;

		// In case of BUSY error status (meaning the `ALLOW_LOCAL_NET_DATA_CHANGE`
		// was already true), allow the operation to proceed.

		require((mRetVal == kWPANTUNDStatus_Ok) || (mRetVal == kWPANTUNDStatus_Busy), on_error);
	}

	mRetVal = kWPANTUNDStatus_Ok;

	mCommandIter = mCommandList.begin();

	while ( (mRetVal == kWPANTUNDStatus_Ok)
	     && (mCommandList.end() != mCommandIter)
	) {
		mNextCommand = *mCommandIter++;

		EH_SPAWN(&mSubPT, vprocess_send_command(event, args));
	}

	mRetVal = mNextCommandRet;

	require_noerr(mRetVal, on_error);

	if ( mReplyUnpacker
	  && (kWPANTUNDStatus_Ok == mRetVal)
	  && (EVENT_NCP_PROP_VALUE_IS == event)
	) {
		// Handle the packed response from the last command

		unsigned int key = va_arg(args, unsigned int);
		const uint8_t* data_in = va_arg(args, const uint8_t*);
		spinel_size_t data_len = va_arg_small(args, spinel_size_t);
		(void) key; // Ignored

		mRetVal = mReplyUnpacker(data_in, data_len, mReturnValue);
	}

on_error:

	if (mRetVal != kWPANTUNDStatus_Ok) {
		syslog(LOG_ERR, "SendCommand task encountered an error: %d (0x%08X)", mRetVal, mRetVal);
	}

	// Even in case of failure we proceed to set mLockProperty
	// to `false`. The error status is checked after this. It is stored in
	// a class instance variable `mRetVal` so that the value is preserved
	// over the protothread EH_SPAWN() call.

	if (mLockProperty != 0) {
		mNextCommand = SpinelPackData(
			SPINEL_FRAME_PACK_CMD_PROP_VALUE_SET(SPINEL_DATATYPE_BOOL_S),
			mLockProperty,
			false
		);

		EH_SPAWN(&mSubPT, vprocess_send_command(event, args));

		if (mNextCommandRet != kWPANTUNDStatus_Ok)
		{
			mRetVal = mNextCommandRet;
		}

		check_noerr(mNextCommandRet);
	}

	finish(mRetVal, mReturnValue);

	EH_END();
}
