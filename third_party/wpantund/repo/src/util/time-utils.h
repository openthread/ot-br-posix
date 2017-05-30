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

#ifndef wpantund_timeutils_h
#define wpantund_timeutils_h

#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS 1
#endif

#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS 1
#endif

#include <stdint.h>
#include <stdbool.h>
#include <sys/cdefs.h>
#include <time.h>

#ifndef MSEC_PER_SEC
#define MSEC_PER_SEC	1000
#endif

#ifndef USEC_PER_MSEC
#define USEC_PER_MSEC	1000
#endif

#ifndef NSEC_PER_MSEC
#define NSEC_PER_MSEC	1000000
#endif

#ifndef CMS_DISTANT_FUTURE
#define CMS_DISTANT_FUTURE			INT32_MAX
#endif

#ifndef TIME_DISTANT_FUTURE
#define TIME_DISTANT_FUTURE			INTPTR_MAX
#endif

#define CMS_SINCE(x)	(time_ms() - (x))

__BEGIN_DECLS
typedef int32_t cms_t;

extern cms_t time_ms(void);
extern time_t time_get_monotonic(void);
extern cms_t cms_until_time(time_t time);
__END_DECLS

#endif
