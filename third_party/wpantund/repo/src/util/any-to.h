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
 *      Declaration of utility functions related to boost::any.
 *
 */

#ifndef wpantund_any_to_h
#define wpantund_any_to_h

#include <string>
#include <boost/any.hpp>
#include "Data.h"
#include <set>
#include <arpa/inet.h>

extern nl::Data any_to_data(const boost::any& value);
extern int any_to_int(const boost::any& value);
extern uint64_t any_to_uint64(const boost::any& value);
extern struct in6_addr any_to_ipv6(const boost::any& value);
extern bool any_to_bool(const boost::any& value);
extern std::string any_to_string(const boost::any& value);
extern std::set<int> any_to_int_set(const boost::any& value);
#endif
