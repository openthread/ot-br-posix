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

#ifndef __WPANTUND_RUNAWAY_RESET_BACKOFF_MANAGER_H__
#define __WPANTUND_RUNAWAY_RESET_BACKOFF_MANAGER_H__ 1

#include "time-utils.h"

namespace nl {
namespace wpantund {

class RunawayResetBackoffManager {
public:
	RunawayResetBackoffManager();

	//! Returns the number of seconds that we should sleep after the next reset.
	float delay_for_unexpected_reset(void);

	//! Called when an unexpected reset occurs.
	void count_unexpected_reset(void);

	//! Called for every main loop to update the windowed reset count.
	void update(void);

private:
	static const int kDecayPeriod;
	static const int kBackoffThreshold;

	int mWindowedResetCount;
	time_t mDecrementAt;
};

}; // namespace wpantund
}; // namespace nl

#endif // __WPANTUND_RUNAWAY_RESET_BACKOFF_MANAGER_H__
