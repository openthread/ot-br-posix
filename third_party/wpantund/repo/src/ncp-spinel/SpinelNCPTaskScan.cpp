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
#include "SpinelNCPTaskScan.h"
#include "SpinelNCPInstance.h"
#include "spinel-extra.h"

using namespace nl;
using namespace nl::wpantund;

nl::wpantund::SpinelNCPTaskScan::SpinelNCPTaskScan(
	SpinelNCPInstance* instance,
	CallbackWithStatusArg1 cb,
	uint32_t channel_mask,
	uint16_t scan_period,
	ScanType scan_type
):	SpinelNCPTask(instance, cb), mChannelMaskLen(0), mScanPeriod(scan_period), mScanType(scan_type)
{
	uint8_t i;

	for (i = 0; i < 32; i++) {
		if (channel_mask & (1<<i)) {
			mChannelMaskData[mChannelMaskLen++] = i;
		}
	}
}

void
nl::wpantund::SpinelNCPTaskScan::finish(int status, const boost::any& value)
{
	SpinelNCPTask::finish(status, value);
}


int
nl::wpantund::SpinelNCPTaskScan::vprocess_event(int event, va_list args)
{
	int ret = 0;

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
		!ncp_state_is_initializing(mInstance->get_ncp_state())
		&& (mInstance->get_ncp_state() != ASSOCIATING)
		&& (mInstance->get_ncp_state() != CREDENTIALS_NEEDED),
		on_error
	);

	// The first event to a task is EVENT_STARTING_TASK. The following
	// line makes sure that we don't start processing this task
	// until it is properly scheduled. All tasks immediately receive
	// the initial `EVENT_STARTING_TASK` event, but further events
	// will only be received by that task once it is that task's turn
	// to execute.
	EH_WAIT_UNTIL(EVENT_STARTING_TASK != event);


	// Set channel mask
	mNextCommand = SpinelPackData(
		SPINEL_FRAME_PACK_CMD_PROP_VALUE_SET(SPINEL_DATATYPE_DATA_S),
		SPINEL_PROP_MAC_SCAN_MASK,
		mChannelMaskData,
		mChannelMaskLen
	);
	EH_SPAWN(&mSubPT, vprocess_send_command(event, args));
	ret = mNextCommandRet;
	require_noerr(ret, on_error);


	// Set delay period
	mNextCommand = SpinelPackData(
		SPINEL_FRAME_PACK_CMD_PROP_VALUE_SET(SPINEL_DATATYPE_UINT16_S),
		SPINEL_PROP_MAC_SCAN_PERIOD,
		mScanPeriod
	);
	EH_SPAWN(&mSubPT, vprocess_send_command(event, args));
	ret = mNextCommandRet;
	require_noerr(ret, on_error);


	// Start the scan.
	mNextCommand = SpinelPackData(
		SPINEL_FRAME_PACK_CMD_PROP_VALUE_SET(SPINEL_DATATYPE_UINT8_S),
		SPINEL_PROP_MAC_SCAN_STATE,
		(mScanType == kScanTypeNet) ? SPINEL_SCAN_STATE_BEACON : SPINEL_SCAN_STATE_ENERGY
	);
	EH_SPAWN(&mSubPT, vprocess_send_command(event, args));
	ret = mNextCommandRet;
	require_noerr(ret, on_error);

	do {
		EH_REQUIRE_WITHIN(
			15,
			event == EVENT_NCP_PROP_VALUE_IS
			|| event == EVENT_NCP_PROP_VALUE_INSERTED,
			on_error
		);

		spinel_prop_key_t prop_key = va_arg_small(args, spinel_prop_key_t);
		const uint8_t* data_ptr = va_arg(args, const uint8_t*);
		spinel_size_t data_len = va_arg(args, spinel_size_t);

		if ((prop_key == SPINEL_PROP_MAC_SCAN_BEACON) && (mScanType == kScanTypeNet)) {
			const spinel_eui64_t* laddr = NULL;
			const char* networkid = "";
			const uint8_t* xpanid = NULL;
			unsigned int xpanid_len = 0;
			unsigned int proto = 0;
			uint16_t panid = 0xFFFF;
			uint16_t saddr = 0xFFFF;
			uint8_t chan = 0;
			uint8_t lqi = 0x00;
			int8_t rssi = 0x00;
			uint8_t flags = 0x00;

			syslog(LOG_DEBUG, "Got a beacon");

			spinel_datatype_unpack(
				data_ptr,
				data_len,
				"Cct(ESSC)t(iCUd)",
				&chan,
				&rssi,

				&laddr,
				&saddr,
				&panid,
				&lqi,

				&proto,
				&flags,
				&networkid,
				&xpanid, &xpanid_len
			);

			if ((xpanid_len != 8) && (xpanid_len != 0)) {
				break;
			}

			WPAN::NetworkInstance network(
				networkid,
				xpanid,
				panid,
				chan,
				(flags & SPINEL_BEACON_THREAD_FLAG_JOINABLE)
			);
			network.rssi = rssi;
			network.type = proto;
			network.lqi = lqi;
			network.saddr = saddr;

			if (laddr) {
				memcpy(network.hwaddr, laddr, sizeof(network.hwaddr));
			}

			mInstance->get_control_interface().mOnNetScanBeacon(network);

		} else if ((prop_key == SPINEL_PROP_MAC_ENERGY_SCAN_RESULT) && (mScanType == kScanTypeEnergy)) {
			EnergyScanResultEntry result;

			syslog(LOG_DEBUG, "Got an Energy Scan result");

			spinel_datatype_unpack(
				data_ptr,
				data_len,
				"Cc",
				&result.mChannel,
				&result.mMaxRssi
			);

			mInstance->get_control_interface().mOnEnergyScanResult(result);

		} else if (prop_key == SPINEL_PROP_MAC_SCAN_STATE) {
			int scan_state;
			spinel_datatype_unpack(data_ptr, data_len, "i", &scan_state);

			if (scan_state == SPINEL_SCAN_STATE_IDLE) {
				break;
			}
		}

		// Change the event type to 'IDLE' so that we
		// don't try to process this event once than once.
		event = EVENT_IDLE;
	} while(true);

	finish(ret);

	EH_EXIT();

on_error:
	if (ret == kWPANTUNDStatus_Ok) {
		ret = kWPANTUNDStatus_Failure;
	}

	syslog(LOG_ERR, "Scan failed: %d", ret);

	finish(ret);

	EH_END();
}
