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

#include "EventHandler.h"

using namespace nl;

EventHandler::EventHandler():
	mControlPT(), mControlTime()
{
	PT_INIT(&mControlPT);
}

EventHandler::~EventHandler()
{
}

cms_t
EventHandler::get_ms_to_next_event()
{
	cms_t cms = CMS_DISTANT_FUTURE;

	if (mControlTime) {
		cms = mControlTime - time_ms();

		if (cms < 0) {
			cms = 0;
		}
	}

	return cms;

}

void
EventHandler::schedule_next_event(float seconds_until_event)
{
	mControlTime = time_ms() + (cms_t)(seconds_until_event * MSEC_PER_SEC);
}

void
EventHandler::unschedule_next_event(void)
{
	mControlTime = 0;
}

int
EventHandler::process_event(int event, ...)
{
	int ret;
	va_list args;
	va_start(args, event);
	ret = vprocess_event(event, args);
	va_end(args);
	return ret;
}
