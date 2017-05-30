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
 *      ValueMap is a dictionary-like key-value store
 *
 */

#include "ValueMap.h"

nl::ValueMap
nl::ValueMapWithKeysAndValues(const char* key, ...)
{
	ValueMap ret;
	va_list args;
	va_start(args, key);
	while (key != NULL) {
		boost::any* value = va_arg(args, boost::any*);
		if (value == NULL) {
			ret[key] = boost::any();
		} else {
			ret[key] = *value;
		}
		key = va_arg(args, const char*);
	}
	va_end(args);
	return ret;
}
