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
#include "SpinelNCPTaskForm.h"
#include "SpinelNCPInstance.h"
#include "SpinelNCPTaskScan.h"
#include "any-to.h"
#include "spinel-extra.h"
#include "sec-random.h"

using namespace nl;
using namespace nl::wpantund;

nl::wpantund::SpinelNCPTaskForm::SpinelNCPTaskForm(
	SpinelNCPInstance* instance,
	CallbackWithStatusArg1 cb,
	const ValueMap& options
):	SpinelNCPTask(instance, cb), mOptions(options), mLastState(instance->get_ncp_state())
{
	if (!mOptions.count(kWPANTUNDProperty_NetworkPANID)) {
		uint16_t panid = instance->mCurrentNetworkInstance.panid;

		if (panid == 0xffff) {
			sec_random_fill(reinterpret_cast<uint8_t*>(&panid), sizeof(panid));
		}

		mOptions[kWPANTUNDProperty_NetworkPANID] = panid;
	}

	if (!mOptions.count(kWPANTUNDProperty_NetworkXPANID)) {
		uint64_t xpanid = 0;

		if (instance->mXPANIDWasExplicitlySet) {
			xpanid = instance->mCurrentNetworkInstance.get_xpanid_as_uint64();
		}

		if (xpanid == 0) {
			sec_random_fill(reinterpret_cast<uint8_t*>(&xpanid), sizeof(xpanid));
		}

		mOptions[kWPANTUNDProperty_NetworkXPANID] = xpanid;
	}


	if (!mOptions.count(kWPANTUNDProperty_IPv6MeshLocalAddress)) {
		union {
			uint64_t xpanid;
			uint8_t bytes[1];
		} x = { any_to_uint64((mOptions[kWPANTUNDProperty_NetworkXPANID])) };

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
		reverse_bytes(x.bytes, sizeof(x.xpanid));
#endif

		// Default ML Prefix to be derived from XPANID.
		struct in6_addr mesh_local_prefix = {{{
			0xfd, x.bytes[0], x.bytes[1], x.bytes[2], x.bytes[3], x.bytes[4], 0, 0
		}}};

		mOptions[kWPANTUNDProperty_IPv6MeshLocalAddress] = mesh_local_prefix;
	}

	if (instance->mNetworkKey.empty()) {
		if (!mOptions.count(kWPANTUNDProperty_NetworkKey)) {
			uint8_t net_key[NCP_NETWORK_KEY_SIZE];

			sec_random_fill(net_key, sizeof(net_key));

			mOptions[kWPANTUNDProperty_NetworkKey] = Data(net_key, sizeof(net_key));
		}

		if (!mOptions.count(kWPANTUNDProperty_NetworkKeyIndex)) {
			mOptions[kWPANTUNDProperty_NetworkKeyIndex] = 1;
		}
	}
}

void
nl::wpantund::SpinelNCPTaskForm::finish(int status, const boost::any& value)
{
	if (!ncp_state_is_associated(mInstance->get_ncp_state())) {
		mInstance->change_ncp_state(mLastState);
	}
	SpinelNCPTask::finish(status, value);
}

int
nl::wpantund::SpinelNCPTaskForm::vprocess_event(int event, va_list args)
{
	int ret = kWPANTUNDStatus_Failure;

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

	if (!mInstance->mCapabilities.count(SPINEL_CAP_ROLE_ROUTER)) {
		// We can't form unless we are router-capable
		ret = kWPANTUNDStatus_FeatureNotSupported;
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

	mLastState = mInstance->get_ncp_state();
	mInstance->change_ncp_state(ASSOCIATING);

	// Clear any previously saved network settings
	mNextCommand = SpinelPackData(SPINEL_FRAME_PACK_CMD_NET_CLEAR);
	EH_SPAWN(&mSubPT, vprocess_send_command(event, args));
	ret = mNextCommandRet;

	check_noerr(ret);

	// TODO: We should do a scan to make sure we pick a good channel
	//       and don't have a panid collision.

	// Set the channel
	{
		uint8_t channel;

		if (mOptions.count(kWPANTUNDProperty_NCPChannel)) {
			channel = any_to_int(mOptions[kWPANTUNDProperty_NCPChannel]);

			// Make sure the channel is the supported channel set.
			if (mInstance->mSupprotedChannels.find(channel) == mInstance->mSupprotedChannels.end()) {
				syslog(LOG_ERR, "Channel %d is not supported by NCP. Supported channels mask is %08x",
					channel,
					mInstance->get_default_channel_mask()
				);
				ret = kWPANTUNDStatus_InvalidArgument;
				goto on_error;
			}

		} else {
			uint32_t mask;
			uint32_t default_mask = mInstance->get_default_channel_mask();

			if (mOptions.count(kWPANTUNDProperty_NCPChannelMask)) {
				mask = any_to_int(mOptions[kWPANTUNDProperty_NCPChannelMask]);
			} else {
				mask = default_mask;
			}

			if ((mask & default_mask) == 0) {
				syslog(LOG_ERR,	"Invalid channel mask 0x%08x. Supported channels mask is 0x%08x", mask, default_mask);
				ret = kWPANTUNDStatus_InvalidArgument;
				goto on_error;
			}

			mask &= default_mask;

			// Randomly pick a channel from the given channel mask for now.
			do {
				sec_random_fill(&channel, 1);
				channel = (channel % 32);
			} while (0 == ((1 << channel) & mask));
		}

		mNextCommand = SpinelPackData(
			SPINEL_FRAME_PACK_CMD_PROP_VALUE_SET(SPINEL_DATATYPE_UINT8_S),
			SPINEL_PROP_PHY_CHAN,
			channel
		);
	}

	EH_SPAWN(&mSubPT, vprocess_send_command(event, args));

	ret = mNextCommandRet;

	require_noerr(ret, on_error);

	// Turn off promiscuous mode, if it happens to be on
	mNextCommand = SpinelPackData(
		SPINEL_FRAME_PACK_CMD_PROP_VALUE_SET(SPINEL_DATATYPE_UINT8_S),
		SPINEL_PROP_MAC_PROMISCUOUS_MODE,
		SPINEL_MAC_PROMISCUOUS_MODE_OFF
	);

	EH_SPAWN(&mSubPT, vprocess_send_command(event, args));
	ret = mNextCommandRet;
	check_noerr(ret);

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

	if (mOptions.count(kWPANTUNDProperty_NestLabs_LegacyMeshLocalPrefix)) {
		if (mInstance->mCapabilities.count(SPINEL_CAP_NEST_LEGACY_INTERFACE)) {
			{
				nl::Data data(any_to_data(mOptions[kWPANTUNDProperty_NestLabs_LegacyMeshLocalPrefix]));
				mNextCommand = SpinelPackData(
					SPINEL_FRAME_PACK_CMD_PROP_VALUE_SET(SPINEL_DATATYPE_DATA_S),
					SPINEL_PROP_NEST_LEGACY_ULA_PREFIX,
					data.data(),
					data.size()
				);
			}

			EH_SPAWN(&mSubPT, vprocess_send_command(event, args));

			ret = mNextCommandRet;

			require_noerr(ret, on_error);
		}
	}

	// Now bring up the network by bringing up the interface and the stack.

	mNextCommand = SpinelPackData(
		SPINEL_FRAME_PACK_CMD_PROP_VALUE_SET(SPINEL_DATATYPE_BOOL_S),
		SPINEL_PROP_NET_IF_UP,
		true
	);

	EH_SPAWN(&mSubPT, vprocess_send_command(event, args));

	ret = mNextCommandRet;

	require(ret == kWPANTUNDStatus_Ok || ret == kWPANTUNDStatus_Already, on_error);

	mNextCommand = SpinelPackData(
		SPINEL_FRAME_PACK_CMD_PROP_VALUE_SET(SPINEL_DATATYPE_BOOL_S),
		SPINEL_PROP_NET_STACK_UP,
		true
	);

	EH_SPAWN(&mSubPT, vprocess_send_command(event, args));

	ret = mNextCommandRet;

	require_noerr(ret, on_error);

	EH_REQUIRE_WITHIN(
		NCP_FORM_TIMEOUT,
		ncp_state_is_associated(mInstance->get_ncp_state()),
		on_error
	);

	ret = kWPANTUNDStatus_Ok;

	finish(ret);

	EH_EXIT();

on_error:

	if (ret == kWPANTUNDStatus_Ok) {
		ret = kWPANTUNDStatus_Failure;
	}

	syslog(LOG_ERR, "Form failed: %d", ret);

	finish(ret);

	EH_END();
}
