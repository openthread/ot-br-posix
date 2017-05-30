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

#ifndef wpantund_NetworkInstance_h
#define wpantund_NetworkInstance_h

#include <stdint.h>
#include <string>
#include <cstring>
#include "string-utils.h"

namespace nl {


namespace wpantund {
namespace WPAN {
struct NetworkId {
	std::string name;
	uint8_t xpanid[8];

	uint64_t
	get_xpanid_as_uint64() const
	{
		union {
			uint64_t val;
			uint8_t data[8];
		} x;
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
		memcpyrev(x.data, xpanid, 8);
#else
		memcpy(x.data, xpanid, 8);
#endif
		return x.val;
	}

	void
	set_xpanid_as_uint64(const uint64_t &_xpanid)
	{
		union {
			uint64_t val;
			uint8_t data[8];
		} x;
		x.val = _xpanid;
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
		memcpyrev(xpanid, x.data, 8);
#else
		memcpy(xpanid, x.data, 8);
#endif
	}

	NetworkId(const std::string& _name = "")
		: name(_name)
	{
	}

	NetworkId(
	    const std::string& _name, const uint8_t _xpanid[8]
	    )
		: name(_name)
	{
		if (_xpanid) {
			memcpy(xpanid, _xpanid, sizeof(xpanid));
		} else {
			memset(xpanid, 0, sizeof(xpanid));
		}
	}

	NetworkId(
	    const std::string& _name, const uint64_t &_xpanid
	    )
		: name(_name)
	{
		set_xpanid_as_uint64(_xpanid);
	}

	bool operator==(const NetworkId& rhs) const
	{
		return (name == rhs.name) &&
		       (0 == memcmp(xpanid, rhs.xpanid, sizeof(xpanid)));
	}
};

struct NetworkInstance : public NetworkId {
	uint16_t panid;
	uint8_t channel;
	bool joinable;
	int8_t rssi;
	uint8_t lqi;
	uint8_t type;
	uint8_t hwaddr[8];
	uint16_t saddr;
	uint8_t version;

public:
	NetworkInstance(
	    const std::string& _name = "",
	    const uint8_t _xpanid[8] = NULL,
	    uint16_t _panid = 0xFFFF, int _channel = 0,
	    bool _joinable = false
	    ) :
		NetworkId(_name, _xpanid),
		panid(_panid),
		channel(_channel),
		joinable(_joinable),
		rssi(-128),
		type(0),
		hwaddr(),
		version(0)
	{
	}
	NetworkInstance(
	    const std::string& _name,
	    const uint64_t _xpanid,
	    uint16_t _panid = 0xFFFF, int _channel = 0,
	    bool _joinable = false
	    ) :
		NetworkId(_name, _xpanid),
		panid(_panid),
		channel(_channel),
		joinable(_joinable),
		rssi(-128),
		type(0),
		hwaddr(),
		version(0)
	{
	}

	bool operator==(const NetworkInstance& rhs) const
	{
		return NetworkId::operator==(rhs)
		       && (panid == rhs.panid)
		       && (channel == rhs.channel)
		       && (type == rhs.type)
		       && (0 == memcmp(hwaddr, rhs.hwaddr, 8));
	}

	bool operator!=(const NetworkInstance& rhs) const
	{
		return !(*this == rhs);
	}

	uint64_t get_hwaddr_as_uint64() const
	{
		union {
			uint64_t ret;
			uint8_t data[8];
		};
		memcpyrev(data, hwaddr, 8);
		return ret;
	}
};
}; // namespace WPAN
}; // namespace wpantund
}; // namespace nl

#endif
