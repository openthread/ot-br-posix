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

#ifndef wpantund_NilReturn_h
#define wpantund_NilReturn_h


namespace nl {
class NilReturn {
public:
	void operator()(void) {
	}
	void operator()(int) {
	}
	template<typename ARG_X, typename ARG_Y> void operator()(ARG_X, ARG_Y) {
	}
};
};

#endif
