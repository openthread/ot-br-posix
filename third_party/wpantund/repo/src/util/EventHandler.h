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

#ifndef __wpantund__EventHandler__
#define __wpantund__EventHandler__

#include <cstdio>
#include <cstdarg>
#include <boost/signals2/signal.hpp>

#include "assert-macros.h"
#include "time-utils.h"
#include "nlpt.h"

namespace nl {

#define EVENT_NULL		0
#define EVENT_IDLE		1
#define EVENT_STARTING_TASK		2

#define EH_BEGIN_SUB(pt)	{ bool eh_did_timeout(false); PT*const __eh_pt = (pt); (void)eh_did_timeout; PT_BEGIN(__eh_pt)
#define EH_BEGIN()			EH_BEGIN_SUB(&mControlPT)
#define EH_END()			PT_END(__eh_pt) }

#define EH_SPAWN(child, proc)	PT_SPAWN(__eh_pt, (child), (proc))

// Explicitly terminate the protothread.
#define EH_EXIT()		PT_EXIT(__eh_pt)

// Explicitly restart the protothread.
#define EH_RESTART()	PT_RESTART(__eh_pt)

// Yield execution for one cycle.
#define EH_YIELD()		PT_YIELD(__eh_pt)

// Suspend execution until condition `c` is satisfied.
#define EH_WAIT_UNTIL(c) PT_WAIT_UNTIL(__eh_pt, c)

// Suspend execution until condition `c` is satisfied.
#define EH_YIELD_UNTIL(c) PT_YIELD_UNTIL(__eh_pt, c)

// Unconditionally suspend execution for `s` seconds.
#define EH_SLEEP_FOR(s) do { \
		schedule_next_event(s); \
		EH_YIELD_UNTIL(EventHandler::get_ms_to_next_event() == 0); \
		unschedule_next_event(); \
	} while (0)

// Wait for condition `c` be satisfied for no more than `s` seconds.
#define EH_WAIT_UNTIL_WITH_TIMEOUT(s, c) do { \
		eh_did_timeout = false; \
		schedule_next_event(s); \
		EH_WAIT_UNTIL((c) || (eh_did_timeout=(EventHandler::get_ms_to_next_event() == 0))); \
		unschedule_next_event(); \
	} while (0)

// Require that condition `c` be satisfied within `s` seconds. If not, goto `l`.
#define EH_REQUIRE_WITHIN(s, c, l) do { \
		EH_WAIT_UNTIL_WITH_TIMEOUT(s, c); \
		require_string(!eh_did_timeout, l, # c); \
	} while (0)


// This class is the base class for tasks (like joining or forming).
// They allow you to implement complex, stateful processes without being
// too unwieldly.
class EventHandler
{
public:
	typedef struct pt PT;

	EventHandler();

	virtual ~EventHandler();

	virtual cms_t get_ms_to_next_event();

	virtual int vprocess_event(int event, va_list args) = 0;

	int process_event(int event, ...);

	void schedule_next_event(float seconds_until_event);
	void unschedule_next_event(void);

public:
	PT mControlPT;

private:
	cms_t mControlTime;
};

}; // namespace nl

#endif /* defined(__wpantund__EventHandler__) */
