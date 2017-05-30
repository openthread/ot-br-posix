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

#include "RunawayResetBackoffManager.h"

#include <cstring>
#include <syslog.h>
#include <sys/time.h>

using namespace nl;
using namespace nl::wpantund;

// -----------------------------

const int RunawayResetBackoffManager::kDecayPeriod = 15;
const int RunawayResetBackoffManager::kBackoffThreshold = 4;

RunawayResetBackoffManager::RunawayResetBackoffManager():
	mWindowedResetCount(0),
	mDecrementAt(0)
{
}

float
RunawayResetBackoffManager::delay_for_unexpected_reset(void)
{
	float ret = 0;
	if (mWindowedResetCount > kBackoffThreshold) {
		int count = mWindowedResetCount - kBackoffThreshold;
		ret = (count * count) / 2.0f;
		syslog(LOG_ERR, "RunawayResetBackoffManager: mWindowedResetCount = %d, will delay for %f seconds", mWindowedResetCount, ret);
	}
	return ret;
}

void
RunawayResetBackoffManager::count_unexpected_reset(void)
{
	mWindowedResetCount++;
	mDecrementAt = time_get_monotonic() + kDecayPeriod;
}

void
RunawayResetBackoffManager::update(void)
{
	if ((mWindowedResetCount > 0) && (mDecrementAt < time_get_monotonic())) {
		mWindowedResetCount--;
		mDecrementAt = time_get_monotonic() + kDecayPeriod;
	}
}
