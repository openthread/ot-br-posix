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
 *    Description:
 *      Implementation of callback timer.
 *
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "assert-macros.h"
#include "Timer.h"

using namespace nl;

const Timer::Interval Timer::kOneMilliSecond = 1;
const Timer::Interval Timer::kOneSecond      = Timer::kOneMilliSecond * 1000;
const Timer::Interval Timer::kOneMinute      = Timer::kOneSecond * 60;
const Timer::Interval Timer::kOneHour        = Timer::kOneMinute * 60;
const Timer::Interval Timer::kOneDay         = Timer::kOneHour * 24;

Timer *const Timer::kListEndMarker = (Timer *)(&Timer::mListHead);
Timer *Timer::mListHead = kListEndMarker;

static void
null_timer_callback(Timer *timer)
{
	return;
}

Timer::Timer() :
	mFireTime()
{
	mType = kOneShot;
	mInterval = 0;
	mNext = NULL;
	mCallback = &null_timer_callback;
}

Timer::~Timer()
{
	remove(this);
}

void
Timer::schedule(Interval interval, const Timer::Callback& callback, Type type)
{
	remove(this);

	require_string(interval > 0, bail, "Timer::schedule() - Input interval is invalid (is 0 or negative).");

	mInterval = interval;
	mCallback = callback;
	mType = type;

	mFireTime.set(interval + time_ms());
	add(this);

bail:
	return;
}

void
Timer::cancel(void)
{
	remove(this);
}

bool
Timer::is_expired(void) const
{
	return mFireTime.is_now_or_in_past();
}

Timer::Interval
Timer::get_interval(void) const
{
	return mInterval;
}

Timer::Type
Timer::get_type(void) const
{
	return mType;
}

void
Timer::add(Timer *timer)
{
	Timer *cur;

	// If the list is empty, insert the new timer as the new head of the list.
	if (mListHead == kListEndMarker) {
		timer->mNext = kListEndMarker;
		mListHead = timer;
		return;
	}

	// If the new timer fire-time is before the head timer's fire-time, insert it as the new head.
	if (timer->mFireTime < mListHead->mFireTime) {
		timer->mNext = mListHead;
		mListHead = timer;
		return;
	}

	// Go through the list and find the proper place to insert the new timer.
	for (cur = mListHead; cur->mNext != kListEndMarker; cur = cur->mNext) {
		if (timer->mFireTime < cur->mNext->mFireTime) {
			// Inset the new timer in between cur and cur->next
			timer->mNext = cur->mNext;
			cur->mNext = timer;
			return;
		}
	}

	// New timer's fire-time is after all other timers in the list, so insert it at the end.
	timer->mNext = kListEndMarker;
	cur->mNext = timer;
}

void
Timer::remove(Timer *timer)
{
	Timer *cur;

	// If timer is not on the list, there is nothing to do.
	if (timer->mNext == NULL) {
		goto bail;
	}

	// If timer is the head of the list, remove it and update the head.
	if (mListHead == timer) {
		mListHead = timer->mNext;
		timer->mNext = NULL;
		goto bail;
	}

	// Go through the list and find the timer.
	for (cur = mListHead; cur != kListEndMarker; cur = cur->mNext) {
		if (cur->mNext == timer) {
			cur->mNext = timer->mNext;
			timer->mNext = NULL;
			goto bail;
		}
	}

	// We should never get here, as the timer with a non-null mNext should be on the list
	// If we get here, assert and bail.
	require_string(false, bail, "Timer::remove() - Timer was not found on the list.");

bail:
	return;
}

int
Timer::process(void)
{
	Timer *timer;

	// Process all expired timers
	for (timer = mListHead; (timer != kListEndMarker) && timer->is_expired(); timer = mListHead) {
		// Remove the timer from the list and update the head.
		mListHead = timer->mNext;
		timer->mNext = NULL;

		// Restart the timer if it is periodic.
		if (timer->mType == kPeriodicFixedRate) {
			// For fixed-rate, restart the timer from last fire time.
			timer->mFireTime.set(timer->mInterval + timer->mFireTime.get());
			add(timer);
		} else if (timer->mType == kPeriodicFixedDelay) {
			// For fixed-delay, restart the timer from now.
			timer->mFireTime.set(timer->mInterval + time_ms());
			add(timer);
		}

		// Invoke the callback.
		timer->mCallback(timer);
	}

	return 0;
}

int
Timer::update_timeout(cms_t *timeout)
{
	if (timeout != NULL) {
		cms_t cms = get_ms_to_next_event();

		if (cms < *timeout) {
			*timeout = cms;
		}
	}

	return 0;
}

cms_t
Timer::get_ms_to_next_event(void)
{
	cms_t cms = CMS_DISTANT_FUTURE;

	if (mListHead != kListEndMarker) {
		cms = mListHead->mFireTime.get_ms_till_time();
		if (cms < 0) {
			cms = 0;
		}
	}
	return cms;
}
