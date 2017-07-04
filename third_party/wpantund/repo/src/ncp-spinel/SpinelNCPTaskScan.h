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

#ifndef __wpantund__SpinelNCPTaskScan__
#define __wpantund__SpinelNCPTaskScan__

#include "SpinelNCPTask.h"
#include "SpinelNCPInstance.h"

using namespace nl;
using namespace nl::wpantund;

namespace nl {
namespace wpantund {

class SpinelNCPTaskScan : public SpinelNCPTask
{
public:
	enum ScanType {
		kScanTypeNet = 0,
		kScanTypeEnergy,
		kScanTypeDiscover,
	};

	enum {
		kDefaultScanPeriod = 200,
	};

	SpinelNCPTaskScan(
		SpinelNCPInstance* instance,
		CallbackWithStatusArg1 cb,
		uint32_t channel_mask,
		uint16_t channel_scan_period = kDefaultScanPeriod,   // per channel in ms
		ScanType scan_type = kScanTypeNet,
		bool joiner_flag = false,          // Scan for joiner only devices (used in discover scan).
		bool enable_filtering = false,     // Enable scan result filtering (used in discover scan).
		uint16_t pan_id_filter = 0xffff    // PANID used for filtering, 0xFFFF to disable (used in discover scan).
	);
	virtual int vprocess_event(int event, va_list args);
	virtual void finish(int status, const boost::any& value = boost::any());

private:
	uint8_t mChannelMaskData[32];
	uint8_t mChannelMaskLen;
	uint16_t mScanPeriod;  // per channel
	ScanType mScanType;
	bool mJoinerFlag;
	bool mEnablerFiltering;
	uint16_t mPanId;
	bool mShouldInterfaceDown;

};

}; // namespace wpantund
}; // namespace nl


#endif /* defined(__wpantund__SpinelNCPTaskScan__) */
