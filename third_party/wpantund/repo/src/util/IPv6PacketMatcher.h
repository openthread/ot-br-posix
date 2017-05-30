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

#ifndef __wpantund__IPv6PacketMatcherRule__
#define __wpantund__IPv6PacketMatcherRule__

#include <stdint.h>
#include <arpa/inet.h>
#include <string.h>
#include <set>
#include "IPv6Helpers.h"


namespace nl {

void dump_outbound_ipv6_packet(const uint8_t* packet, ssize_t len, const char* extra, bool dropped = false);
void dump_inbound_ipv6_packet(const uint8_t* packet, ssize_t len, const char* extra, bool dropped = false);

struct IPv6PacketMatcherRule {
	static const uint8_t TYPE_ALL;
	static const uint8_t TYPE_NONE;
	static const uint8_t TYPE_UDP;
	static const uint8_t TYPE_TCP;
	static const uint8_t TYPE_ICMP;
	static const uint8_t TYPE_HOP_BY_HOP;
	static const uint8_t SUBTYPE_ALL;
	static const uint8_t SUBTYPE_ICMP_NEIGHBOR_ADV;
	static const uint8_t SUBTYPE_ICMP_NEIGHBOR_SOL;
	static const uint8_t SUBTYPE_ICMP_ROUTER_ADV;
	static const uint8_t SUBTYPE_ICMP_ROUTER_SOL;
	static const uint8_t SUBTYPE_ICMP_REDIRECT;

	uint8_t type;
	uint8_t subtype;
	in_port_t local_port;
	bool local_port_match;
	struct in6_addr local_address;
	uint8_t local_match_mask;

	in_port_t remote_port;
	bool remote_port_match;
	struct in6_addr remote_address;
	uint8_t remote_match_mask;

	void clear();
	IPv6PacketMatcherRule&        update_from_inbound_packet(const uint8_t* packet);
	bool                            match_inbound(const uint8_t* packet) const;
	IPv6PacketMatcherRule&        update_from_outbound_packet(const uint8_t* packet);
	bool                            match_outbound(const uint8_t* packet) const;
	bool operator==(const IPv6PacketMatcherRule& lhs) const;
	bool operator<(const IPv6PacketMatcherRule& lhs) const;

	bool operator!=(const IPv6PacketMatcherRule& lhs) const { return !(*this == lhs); }
	bool operator>=(const IPv6PacketMatcherRule& lhs) const { return !(*this < lhs); }

	bool operator<=(const IPv6PacketMatcherRule& lhs) const { return (*this < lhs) || (*this == lhs); }
	bool operator>(const IPv6PacketMatcherRule& lhs) const { return !(*this <= lhs); }
};

class IPv6PacketMatcher : public std::set<IPv6PacketMatcherRule> {
public:

	const_iterator match_outbound(const uint8_t* packet) const;
	const_iterator match_inbound(const uint8_t* packet) const;
};

}; // namespace nl

#endif /* defined(__wpantund__IPv6PacketMatcherRule__) */
