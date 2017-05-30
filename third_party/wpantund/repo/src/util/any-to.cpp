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
 *      Implementation of utility functions related to boost::any.
 *
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdint.h>
#include "any-to.h"
#include <exception>
#include <stdexcept>
#include <ctype.h>
#include <list>
#include "string-utils.h"

using namespace nl;

nl::Data any_to_data(const boost::any& value)
{
	nl::Data ret;

	if (value.type() == typeid(std::string)) {
		std::string key_string = boost::any_cast<std::string>(value);
		uint8_t data[key_string.size()/2];

		int length = parse_string_into_data(data,
											key_string.size()/2,
											key_string.c_str());

		ret = nl::Data(data, length);
	} else if (value.type() == typeid(nl::Data)) {
		ret = boost::any_cast<nl::Data>(value);
	} else if (value.type() == typeid(uint64_t)) {
		union {
			uint64_t val;
			uint8_t data[sizeof(uint64_t)];
		} x;

		x.val = boost::any_cast<uint64_t>(value);

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
		reverse_bytes(x.data, sizeof(uint64_t));
#endif

		ret.append(x.data,sizeof(uint64_t));
	} else {
		ret = nl::Data(boost::any_cast<std::vector<uint8_t> >(value));
	}
	return ret;
}

int any_to_int(const boost::any& value)
{
	int32_t ret = 0;

	if (value.type() == typeid(std::string)) {
		std::string key_string = boost::any_cast<std::string>(value);
		ret = (int)strtol(key_string.c_str(), NULL, 0);
	} else if (value.type() == typeid(uint8_t)) {
		ret = boost::any_cast<uint8_t>(value);
	} else if (value.type() == typeid(int8_t)) {
		ret = boost::any_cast<int8_t>(value);
	} else if (value.type() == typeid(uint16_t)) {
		ret = boost::any_cast<uint16_t>(value);
	} else if (value.type() == typeid(int16_t)) {
		ret = boost::any_cast<int16_t>(value);
	} else if (value.type() == typeid(uint32_t)) {
		ret = boost::any_cast<uint32_t>(value);
	} else if (value.type() == typeid(int32_t)) {
		ret = boost::any_cast<int32_t>(value);
	} else if (value.type() == typeid(bool)) {
		ret = boost::any_cast<bool>(value);
	} else if (value.type() == typeid(unsigned int)) {
		ret = boost::any_cast<unsigned int>(value);
	} else {
		ret = boost::any_cast<int>(value);
	}
	return ret;
}

struct in6_addr
any_to_ipv6(const boost::any& value)
{
	struct in6_addr ret = {};

	if (value.type() == typeid(std::string)) {
		std::string str(boost::any_cast<std::string>(value));
		size_t lastchar(str.find_first_not_of("0123456789abcdefABCDEF:."));
		int bytes;

		if (lastchar != std::string::npos) {
			str.erase(str.begin() + lastchar, str.end());
		}

		bytes = inet_pton(
			AF_INET6,
			str.c_str(),
			ret.s6_addr
		);
		if (bytes <= 0) {
			throw std::invalid_argument("String not IPv6 address");
		}
	} else if (value.type() == typeid(nl::Data)) {
		nl::Data data(boost::any_cast<nl::Data>(value));
		if (data.size() <= sizeof(ret)) {
			memcpy(ret.s6_addr, data.data(), data.size());
		}
	} else {
		ret = boost::any_cast<struct in6_addr>(value);
	}

	return ret;
}

uint64_t
any_to_uint64(const boost::any& value)
{
	uint64_t ret(0);

	if (value.type() == typeid(std::string)) {
		const std::string& key_string = boost::any_cast<std::string>(value);

		if (key_string.size() != 16) {
			throw std::invalid_argument("String not 16 characters long");
		}

		ret = static_cast<uint64_t>(strtoull(key_string.c_str(), NULL, 16));

	} else if (value.type() == typeid(nl::Data)) {
		const nl::Data& data = boost::any_cast<nl::Data>(value);
		union {
			uint64_t val;
			uint8_t data[sizeof(uint64_t)];
		} x;

		if (data.size() != sizeof(uint64_t)) {
			throw std::invalid_argument("Data not 8 bytes long");
		}

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
		memcpyrev(x.data, data.data(), sizeof(uint64_t));
#else
		memcpy(x.data, data.data(), sizeof(uint64_t));
#endif

		ret = x.val;

	} else {
		ret = boost::any_cast<uint64_t>(value);
	}


	return ret;
}

bool any_to_bool(const boost::any& value)
{
	bool ret = 0;

	if (value.type() == typeid(std::string)) {
		std::string key_string = boost::any_cast<std::string>(value);
		if (key_string=="true" || key_string=="yes" || key_string=="1" || key_string == "TRUE" || key_string == "YES")
			ret = true;
		else if (key_string=="false" || key_string=="no" || key_string=="0" || key_string == "FALSE" || key_string == "NO")
			ret = false;
		else
			ret = (bool)strtol(key_string.c_str(), NULL, 0);
	} else if (value.type() == typeid(bool)) {
		ret = boost::any_cast<bool>(value);
	} else {
		ret = any_to_int(value) != 0;
	}
	return ret;
}

std::string any_to_string(const boost::any& value)
{
	std::string ret;

	if (value.type() == typeid(std::string)) {
		ret = boost::any_cast<std::string>(value);
	} else if (value.type() == typeid(uint8_t)) {
		char tmp[10];
		snprintf(tmp, sizeof(tmp), "%u", boost::any_cast<uint8_t>(value));
		ret = tmp;
	} else if (value.type() == typeid(int8_t)) {
		char tmp[10];
		snprintf(tmp, sizeof(tmp), "%d", boost::any_cast<int8_t>(value));
		ret = tmp;
	} else if (value.type() == typeid(uint16_t)) {
		char tmp[10];
		snprintf(tmp, sizeof(tmp), "%u", boost::any_cast<uint16_t>(value));
		ret = tmp;
	} else if (value.type() == typeid(int16_t)) {
		char tmp[10];
		snprintf(tmp, sizeof(tmp), "%d", boost::any_cast<int16_t>(value));
		ret = tmp;
	} else if (value.type() == typeid(uint32_t)) {
		char tmp[10];
		snprintf(tmp, sizeof(tmp), "%u", (unsigned int)boost::any_cast<uint32_t>(value));
		ret = tmp;
	} else if (value.type() == typeid(int32_t)) {
		char tmp[10];
		snprintf(tmp, sizeof(tmp), "%d", (int)boost::any_cast<int32_t>(value));
		ret = tmp;
	} else if (value.type() == typeid(uint64_t)) {
		char tmp[20];
		uint64_t u64_val = boost::any_cast<uint64_t>(value);
		snprintf(tmp,
		         sizeof(tmp),
		         "%08x%08x",
		         static_cast<uint32_t>(u64_val >> 32),
		         static_cast<uint32_t>(u64_val & 0xFFFFFFFF));
		ret = tmp;
	} else if (value.type() == typeid(unsigned int)) {
		char tmp[10];
		snprintf(tmp, sizeof(tmp), "%u", boost::any_cast<unsigned int>(value));
		ret = tmp;
	} else if (value.type() == typeid(int)) {
		char tmp[10];
		snprintf(tmp, sizeof(tmp), "%d", boost::any_cast<int>(value));
		ret = tmp;
	} else if (value.type() == typeid(bool)) {
		ret = (boost::any_cast<bool>(value))? "true" : "false";
	} else if (value.type() == typeid(nl::Data)) {
		nl::Data data = boost::any_cast<nl::Data>(value);
		ret = std::string(data.size()*2,0);

		// Reserve the zero termination
		ret.reserve(data.size()*2+1);

		encode_data_into_string(data.data(),
								data.size(),
								&ret[0],
								ret.capacity(),
								0);
	} else if (value.type() == typeid(std::list<std::string>)) {
		std::list<std::string> l = boost::any_cast<std::list<std::string> >(value);
		if (!l.empty()) {
			std::list<std::string>::const_iterator iter;
			ret = "{\n";
			for (iter = l.begin(); iter != l.end(); ++iter) {
				ret += "\t\"" + *iter + "\"\n";
			}
			ret += "}";
		} else {
			ret = "{ }";
		}
	} else {
		ret += "<";
		ret += value.type().name();
		ret += ">";
	}
	return ret;
}

std::set<int>
any_to_int_set(const boost::any& value)
{
	std::set<int> ret;

	if (value.type() == typeid(std::string)) {
		std::string key_string = boost::any_cast<std::string>(value);
		if (key_string.empty()) {
			// Empty set. Do nothing.
		} else if (key_string.find(',') != std::string::npos) {
			// List of values. Not yet supported.
			throw std::invalid_argument("integer mask string format not yet implemented");
		} else if (isdigit(key_string[0])) {
			// Special case, only one value.
			ret.insert((int)strtol(key_string.c_str(), NULL, 0));
		} else {
			throw std::invalid_argument(key_string);
		}
	} else if (value.type() == typeid(uint8_t)) {
		ret.insert(boost::any_cast<uint8_t>(value));
	} else if (value.type() == typeid(int8_t)) {
		ret.insert(boost::any_cast<int8_t>(value));
	} else if (value.type() == typeid(uint16_t)) {
		ret.insert(boost::any_cast<uint16_t>(value));
	} else if (value.type() == typeid(int16_t)) {
		ret.insert(boost::any_cast<int16_t>(value));
	} else if (value.type() == typeid(uint32_t)) {
		ret.insert(boost::any_cast<uint32_t>(value));
	} else if (value.type() == typeid(int32_t)) {
		ret.insert(boost::any_cast<int32_t>(value));
	} else if (value.type() == typeid(bool)) {
		ret.insert(boost::any_cast<bool>(value));
	} else if (value.type() == typeid(std::list<int>)) {
		std::list<int> number_list(boost::any_cast< std::list<int> >(value));
		ret.insert(number_list.begin(), number_list.end());
	} else if (value.type() == typeid(std::list<boost::any>)) {
		std::list<boost::any> any_list(boost::any_cast< std::list<boost::any> >(value));
		std::list<boost::any>::const_iterator iter;
		for(iter = any_list.begin(); iter != any_list.end(); ++iter) {
			ret.insert(any_to_int(*iter));
		}
	} else {
		ret = boost::any_cast< std::set<int> >(value);
	}
	return ret;
}
