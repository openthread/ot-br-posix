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
 *      This file declares a callback timer module for use in wpantund.
 *
 */

#ifndef __wpantund__Timer__
#define __wpantund__Timer__

#include <list>
#include <boost/function.hpp>

#include "time-utils.h"

namespace nl {

// A callback timer class for use in wpantund.
class Timer {
public:
	// Timer interval in ms - defined as int32_t --> can go up to 24.8 days
	typedef cms_t Interval;

	// A timer callback
	typedef boost::function<void (Timer *)> Callback;

	// Timer type
	enum Type {
		kOneShot,              // One-shot timer (runs once and expires)
		kPeriodicFixedRate,    // Restart the timer from last fire time
		kPeriodicFixedDelay    // Restart the timer after processing the timer
	};

	// Time interval constants
	static const Interval kOneMilliSecond;
	static const Interval kOneSecond;
	static const Interval kOneMinute;
	static const Interval kOneHour;
	static const Interval kOneDay;

public:
	Timer();
	virtual ~Timer();

	// Schedules the timer with given interval/period. At fire-time the given callback is invoked.
	//    Three types of timers are supported: One-shot, fixed-rate periodic, fixed-delay periodic.
	//    Timer gets started at the time of the call to schedule(). For periodic timer, the first
	//    invocation of the callback happens after the first interval/period expires.
	//    A subsequent call to schedule() will stop an already running timer and overwrite all the
	//    timer parameters (interval, callback, and its type), basically starting a new timer.
	void schedule(Interval interval, const Callback &callback, Type type = kOneShot);

	// Cancels/Stops the timer (callback will not be invoked).
	//    If timer is already expired or stopped, calling cancel() does nothing (and is ok).
	void cancel(void);

	// Returns true if the timer is expired or not running, otherwise returns false.
	bool is_expired(void) const;

	// Returns the current interval/period of the timer
	Interval get_interval(void) const;

	// Returns the type of the timer.
	Type get_type(void) const;


public:
	static int process(void);
	static int update_timeout(cms_t *timeout);

private:
	struct ClockTime {
	public:
		ClockTime()           { mTime = 0;    }
		ClockTime(cms_t time) { mTime = time; }

		void set(cms_t time)  { mTime = time; }
		cms_t get(void) 	  { return mTime; }

		bool operator<(const ClockTime& lhs)  const  { return (mTime - lhs.mTime) < 0;  }
		bool operator<=(const ClockTime& lhs) const  { return (mTime - lhs.mTime) <= 0; }
		bool operator==(const ClockTime &lhs) const  { return (mTime == lhs.mTime);     }

		bool is_now_or_in_past(void)  const    { return (*this) <= now(); }
		bool is_in_future(void) const          { return now() < (*this); }

		cms_t get_ms_till_time(void) const     { return mTime - time_ms(); }

		static ClockTime now(void)  { return ClockTime(time_ms()); }

	private:
		cms_t mTime;
	};

	ClockTime mFireTime;
	cms_t mInterval;
	Callback mCallback;
	Type mType;
	Timer *mNext;       // for linked-list

private:
	static void remove(Timer *timer);       // Removes timer from linked-list
	static void add(Timer *timer);          // Adds timer to list (list sorted based on fire-time)

	static cms_t get_ms_to_next_event(void);

	static Timer * mListHead;   			// Head of the LinkedList
	static Timer * const kListEndMarker;	// Marker for end of list
};

}; // namespace nl

#endif  // ifndef __wpantund__Timer__
