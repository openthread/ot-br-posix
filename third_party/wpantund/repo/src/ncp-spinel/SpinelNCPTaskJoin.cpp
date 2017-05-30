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
#include "SpinelNCPTaskJoin.h"
#include "SpinelNCPInstance.h"
#include "any-to.h"
#include "spinel-extra.h"

using namespace nl;
using namespace nl::wpantund;

nl::wpantund::SpinelNCPTaskJoin::SpinelNCPTaskJoin(
	SpinelNCPInstance* instance,
	CallbackWithStatusArg1 cb,
	const ValueMap& options
):	SpinelNCPTask(instance, cb), mOptions(options), mLastState(instance->get_ncp_state())
{
}

void
nl::wpantund::SpinelNCPTaskJoin::finish(int status, const boost::any& value)
{
	SpinelNCPTask::finish(status, value);
	if ( (status != kWPANTUNDStatus_InProgress)
	  && !ncp_state_is_associated(mInstance->get_ncp_state())
	) {
		mInstance->change_ncp_state(mLastState);
	}
}

int
nl::wpantund::SpinelNCPTaskJoin::vprocess_event(int event, va_list args)
{
	int ret = kWPANTUNDStatus_Failure;
	int last_status = peek_ncp_callback_status(event, args);

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

	if (ncp_state_is_associated(mInstance->get_ncp_state())) {
		ret = kWPANTUNDStatus_Already;
		finish(ret);
		EH_EXIT();
	}

	// The first event to a task is EVENT_STARTING_TASK. The following
	// line makes sure that we don't start processing this task
	// until it is properly scheduled. All tasks immediately receive
	// the initial `EVENT_STARTING_TASK` event, but further events
	// will only be received by that task once it is that task's turn
	// to execute.
	EH_WAIT_UNTIL(EVENT_STARTING_TASK != event);

	// Clear any previously saved network settings
	mNextCommand = SpinelPackData(SPINEL_FRAME_PACK_CMD_NET_CLEAR);
	EH_SPAWN(&mSubPT, vprocess_send_command(event, args));
	ret = mNextCommandRet;

	check_noerr(ret);

	mLastState = mInstance->get_ncp_state();
	mInstance->change_ncp_state(ASSOCIATING);

	if (mOptions.count(kWPANTUNDProperty_NetworkNodeType)) {
		NodeType node_type;
		bool isRouterRoleEnabled;

		node_type = string_to_node_type(any_to_string(mOptions[kWPANTUNDProperty_NetworkNodeType]));

		if ((node_type == ROUTER) || (node_type == LEADER)) {
			if (!mInstance->mCapabilities.count(SPINEL_CAP_ROLE_ROUTER)) {
				// We cannot join as a router/leader unless we are router-capable
				ret = kWPANTUNDStatus_FeatureNotSupported;
				goto on_error;
			}
			isRouterRoleEnabled = true;

		} else if (node_type == END_DEVICE) {
			isRouterRoleEnabled = false;

		} else if (node_type == SLEEPY_END_DEVICE) {
			if (!mInstance->mCapabilities.count(SPINEL_CAP_ROLE_SLEEPY)) {
				ret = kWPANTUNDStatus_FeatureNotSupported;
				goto on_error;
			}
			isRouterRoleEnabled = false;

		} else if (node_type == LURKER) {
			if (!mInstance->mCapabilities.count(SPINEL_CAP_NEST_LEGACY_INTERFACE)) {
				ret = kWPANTUNDStatus_FeatureNotSupported;
				goto on_error;
			}
			isRouterRoleEnabled = true;

		} else {
			ret = kWPANTUNDStatus_InvalidArgument;
			goto on_error;
		}

		mNextCommand = SpinelPackData(
			SPINEL_FRAME_PACK_CMD_PROP_VALUE_SET(SPINEL_DATATYPE_BOOL_S),
			SPINEL_PROP_THREAD_ROUTER_ROLE_ENABLED,
			isRouterRoleEnabled
		);

		EH_SPAWN(&mSubPT, vprocess_send_command(event, args));

		ret = mNextCommandRet;

		require_noerr(ret, on_error);
	}

	// Turn off promiscuous mode, if it happens to be on
	mNextCommand = SpinelPackData(
		SPINEL_FRAME_PACK_CMD_PROP_VALUE_SET(SPINEL_DATATYPE_UINT8_S),
		SPINEL_PROP_MAC_PROMISCUOUS_MODE,
		SPINEL_MAC_PROMISCUOUS_MODE_OFF
	);
	EH_SPAWN(&mSubPT, vprocess_send_command(event, args));
	ret = mNextCommandRet;
	check_noerr(ret);

	if (mOptions.count(kWPANTUNDProperty_NCPChannel)) {
		mNextCommand = SpinelPackData(
			SPINEL_FRAME_PACK_CMD_PROP_VALUE_SET(SPINEL_DATATYPE_UINT8_S),
			SPINEL_PROP_PHY_CHAN,
			any_to_int(mOptions[kWPANTUNDProperty_NCPChannel])
		);

		EH_SPAWN(&mSubPT, vprocess_send_command(event, args));

		ret = mNextCommandRet;

		require_noerr(ret, on_error);

	}

	if (mOptions.count(kWPANTUNDProperty_NetworkPANID)) {
		mNextCommand = SpinelPackData(
			SPINEL_FRAME_PACK_CMD_PROP_VALUE_SET(SPINEL_DATATYPE_UINT16_S),
			SPINEL_PROP_MAC_15_4_PANID,
			any_to_int(mOptions[kWPANTUNDProperty_NetworkPANID])
		);

		EH_SPAWN(&mSubPT, vprocess_send_command(event, args));

		ret = mNextCommandRet;

		require_noerr(ret, on_error);
	}

	if (mOptions.count(kWPANTUNDProperty_NetworkXPANID)) {
		{
			uint64_t xpanid(any_to_uint64((mOptions[kWPANTUNDProperty_NetworkXPANID])));

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
			reverse_bytes(reinterpret_cast<uint8_t*>(&xpanid), sizeof(xpanid));
#endif

			mNextCommand = SpinelPackData(
				SPINEL_FRAME_PACK_CMD_PROP_VALUE_SET(SPINEL_DATATYPE_DATA_S),
				SPINEL_PROP_NET_XPANID,
				&xpanid,
				sizeof(xpanid)
			);
		}

		EH_SPAWN(&mSubPT, vprocess_send_command(event, args));

		ret = mNextCommandRet;

		require_noerr(ret, on_error);
	}

	if (mOptions.count(kWPANTUNDProperty_NetworkName)) {
		mNextCommand = SpinelPackData(
			SPINEL_FRAME_PACK_CMD_PROP_VALUE_SET(SPINEL_DATATYPE_UTF8_S),
			SPINEL_PROP_NET_NETWORK_NAME,
			any_to_string(mOptions[kWPANTUNDProperty_NetworkName]).c_str()
		);

		EH_SPAWN(&mSubPT, vprocess_send_command(event, args));

		ret = mNextCommandRet;

		require_noerr(ret, on_error);
	}

	if (mOptions.count(kWPANTUNDProperty_NetworkKey)) {
		{
			nl::Data data(any_to_data(mOptions[kWPANTUNDProperty_NetworkKey]));
			mNextCommand = SpinelPackData(
				SPINEL_FRAME_PACK_CMD_PROP_VALUE_SET(SPINEL_DATATYPE_DATA_S),
				SPINEL_PROP_NET_MASTER_KEY,
				data.data(),
				data.size()
			);
		}

		EH_SPAWN(&mSubPT, vprocess_send_command(event, args));

		ret = mNextCommandRet;

		require_noerr(ret, on_error);
	}

	if (mOptions.count(kWPANTUNDProperty_NetworkKeyIndex)) {
		mNextCommand = SpinelPackData(
			SPINEL_FRAME_PACK_CMD_PROP_VALUE_SET(SPINEL_DATATYPE_UINT32_S),
			SPINEL_PROP_NET_KEY_SEQUENCE_COUNTER,
			any_to_int(mOptions[kWPANTUNDProperty_NetworkKeyIndex])
		);

		EH_SPAWN(&mSubPT, vprocess_send_command(event, args));

		ret = mNextCommandRet;

		require_noerr(ret, on_error);
	}

	if (mOptions.count(kWPANTUNDProperty_IPv6MeshLocalAddress)) {
		{
			struct in6_addr addr = any_to_ipv6(mOptions[kWPANTUNDProperty_IPv6MeshLocalAddress]);
			mNextCommand = SpinelPackData(
				SPINEL_FRAME_PACK_CMD_PROP_VALUE_SET(SPINEL_DATATYPE_IPv6ADDR_S SPINEL_DATATYPE_UINT8_S),
				SPINEL_PROP_IPV6_ML_PREFIX,
				&addr,
				64
			);
		}

		EH_SPAWN(&mSubPT, vprocess_send_command(event, args));

		ret = mNextCommandRet;

		require_noerr(ret, on_error);
	} else if (mOptions.count(kWPANTUNDProperty_IPv6MeshLocalPrefix)) {
		{
			struct in6_addr addr = any_to_ipv6(mOptions[kWPANTUNDProperty_IPv6MeshLocalPrefix]);
			mNextCommand = SpinelPackData(
				SPINEL_FRAME_PACK_CMD_PROP_VALUE_SET(SPINEL_DATATYPE_IPv6ADDR_S SPINEL_DATATYPE_UINT8_S),
				SPINEL_PROP_IPV6_ML_PREFIX,
				&addr,
				64
			);
		}

		EH_SPAWN(&mSubPT, vprocess_send_command(event, args));

		ret = mNextCommandRet;

		require_noerr(ret, on_error);
	}


	// Now bring up the network by bringing up the interface and the stack.

	mNextCommand = SpinelPackData(
		SPINEL_FRAME_PACK_CMD_PROP_VALUE_SET(SPINEL_DATATYPE_BOOL_S),
		SPINEL_PROP_NET_IF_UP,
		true
	);

	EH_SPAWN(&mSubPT, vprocess_send_command(event, args));

	ret = mNextCommandRet;

	require((ret == kWPANTUNDStatus_Ok) || (ret == kWPANTUNDStatus_Already), on_error);

	mNextCommand = SpinelPackData(
		SPINEL_FRAME_PACK_CMD_PROP_VALUE_SET(SPINEL_DATATYPE_BOOL_S),
		SPINEL_PROP_NET_REQUIRE_JOIN_EXISTING,
		true
	);

	EH_SPAWN(&mSubPT, vprocess_send_command(event, args));

	ret = mNextCommandRet;

	check_noerr(ret);

	mNextCommand = SpinelPackData(
		SPINEL_FRAME_PACK_CMD_PROP_VALUE_SET(SPINEL_DATATYPE_BOOL_S),
		SPINEL_PROP_NET_STACK_UP,
		true
	);

	EH_SPAWN(&mSubPT, vprocess_send_command(event, args));

	ret = mNextCommandRet;

	require_noerr(ret, on_error);

	EH_REQUIRE_WITHIN(
		NCP_JOIN_TIMEOUT,
		((last_status >= SPINEL_STATUS_JOIN__BEGIN) && (last_status < SPINEL_STATUS_JOIN__END))
		|| ncp_state_is_associated(mInstance->get_ncp_state()),
		on_error
	);

	ret = last_status
		? WPANTUND_NCPERROR_TO_STATUS(last_status)
		: kWPANTUNDStatus_Ok;

	if ( (last_status == SPINEL_STATUS_JOIN_SECURITY)
	  || (last_status == SPINEL_STATUS_JOIN_FAILURE)
	) {
		mInstance->change_ncp_state(CREDENTIALS_NEEDED);
		ret = kWPANTUNDStatus_InProgress;
	} else if ((last_status >= SPINEL_STATUS_JOIN__BEGIN) && (last_status < SPINEL_STATUS_JOIN__END)) {
		ret = kWPANTUNDStatus_JoinFailedUnknown;
	}

	finish(ret);

	EH_EXIT();

on_error:

	if (ret == kWPANTUNDStatus_Ok) {
		ret = kWPANTUNDStatus_Failure;
	}

	syslog(LOG_ERR, "Join failed: %d", ret);

	finish(ret);

	EH_END();
}
