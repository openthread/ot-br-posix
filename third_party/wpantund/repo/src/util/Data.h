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
 *      Class definition for binary data container.
 *
 */

#ifndef __wpantund__Data__
#define __wpantund__Data__

#include <vector>
#include <stdint.h>
#include <stdlib.h>

namespace nl {
class Data : public std::vector<uint8_t> {
public:
	Data(const std::vector<uint8_t>& x) : std::vector<uint8_t>(x) {
	}
	Data(
	    const uint8_t* ptr, size_t len
	    ) : std::vector<uint8_t>(ptr, ptr + len) {
	}

	template<typename _InputIterator>
	Data(_InputIterator __first, _InputIterator __last)
	: std::vector<uint8_t>(__first, __last) { }

	Data(size_t len = 0) : std::vector<uint8_t>(len) {
	}

	Data& append(const Data& d) {
		insert(end(),d.begin(),d.end());
		return *this;
	}

	Data& append(const uint8_t* ptr, size_t len) {
		insert(end(),ptr,ptr+len);
		return *this;
	}

	void pop_front(size_t len) {
		erase(begin(), begin()+len);
	}

	uint8_t* data() {
		return &*begin();
	}

	const uint8_t* data() const {
		return &*begin();
	}
};
};

#endif /* defined(__wpantund__Data__) */
