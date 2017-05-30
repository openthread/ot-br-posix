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
 *      Type definitions and utility functions related to callback objects.
 *
 */

#ifndef wpantund_Callbacks_h
#define wpantund_Callbacks_h

#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS 1
#endif

#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS 1
#endif

#include <stdint.h>
#include <string>
#include <vector>

#include <boost/signals2/signal.hpp>
#include <boost/bind.hpp>
#include <boost/any.hpp>

#include "NilReturn.h"

namespace nl {

typedef boost::function<void(void)> CallbackSimple;
typedef boost::function<void(int)> CallbackWithStatus;
typedef boost::function<void(int, const boost::any&)> CallbackWithStatusArg1;

typedef boost::signals2::signal<void(int)> SignalWithStatus;

static inline void
split_cb_on_status(int status, CallbackSimple cb_success, CallbackWithStatus cb_error = nl::NilReturn())
{
	if (status == 0) {
		cb_success();
	} else {
		cb_error(status);
	}
}

#define CALLBACK_FUNC_SPLIT(success,failure) \
		CallbackWithStatus( \
			boost::bind( \
				&split_cb_on_status, \
				_1, \
				CallbackSimple(success), \
				CallbackWithStatus(failure) \
			) \
		)

}; // namespace nl

#endif
