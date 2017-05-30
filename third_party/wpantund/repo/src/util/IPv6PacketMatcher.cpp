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

#include "IPv6PacketMatcher.h"
#include <syslog.h>
#include <stdio.h>

#ifndef IPV6_PACKET_MATCHER_DEBUG
#define IPV6_PACKET_MATCHER_DEBUG 0
#endif

using namespace nl;

const uint8_t IPv6PacketMatcherRule::TYPE_ALL = 0xFF;
const uint8_t IPv6PacketMatcherRule::TYPE_NONE = 0xFE;
const uint8_t IPv6PacketMatcherRule::TYPE_UDP = 17;
const uint8_t IPv6PacketMatcherRule::TYPE_TCP = 6;
const uint8_t IPv6PacketMatcherRule::TYPE_ICMP = 58;
const uint8_t IPv6PacketMatcherRule::TYPE_HOP_BY_HOP = 0;

const uint8_t IPv6PacketMatcherRule::SUBTYPE_ALL = 0xFF;
const uint8_t IPv6PacketMatcherRule::SUBTYPE_ICMP_NEIGHBOR_ADV = 136;
const uint8_t IPv6PacketMatcherRule::SUBTYPE_ICMP_NEIGHBOR_SOL = 135;
const uint8_t IPv6PacketMatcherRule::SUBTYPE_ICMP_ROUTER_SOL = 133;
const uint8_t IPv6PacketMatcherRule::SUBTYPE_ICMP_ROUTER_ADV = 134;
const uint8_t IPv6PacketMatcherRule::SUBTYPE_ICMP_REDIRECT = 137;

#define IPV6_HEADER_LENGTH              40
#define IPV6_TCP_HEADER_CHECKSUM_OFFSET (IPV6_HEADER_LENGTH + 16)
#define IPV6_UDP_HEADER_CHECKSUM_OFFSET (IPV6_HEADER_LENGTH + 6)

#define PACKET_IS_IPV6(x)        ((static_cast<const uint8_t*>(x)[0] & 0xF0) == 0x60)
#define IPV6_GET_TYPE(x)         (static_cast<const uint8_t*>(x)[6])
#define IPV6_GET_SRC_PORT(x)     (in_port_t)(*(const uint16_t*)((const uint8_t*)(x)+40))
#define IPV6_GET_DEST_PORT(x)    (in_port_t)(*(const uint16_t*)((const uint8_t*)(x)+42))
#define IPV6_GET_SRC_ADDR(t,f)   memcpy(&t, static_cast<const uint8_t*>(f) + 8, 16)
#define IPV6_GET_DEST_ADDR(t,f)  memcpy(&t, static_cast<const uint8_t*>(f) + 24, 16)
#define IPV6_ICMP_GET_SUBTYPE(x) (static_cast<const uint8_t*>(x)[40])
#define IPv6_TCP_GET_CHECKSUM(p,l)  IPV6_GET_UINT16(p, l, IPV6_TCP_HEADER_CHECKSUM_OFFSET)
#define IPv6_UDP_GET_CHECKSUM(p,l)  IPV6_GET_UINT16(p, l, IPV6_UDP_HEADER_CHECKSUM_OFFSET)

static inline uint16_t IPV6_GET_UINT16(const uint8_t *packet, ssize_t len, size_t offset)
{
	uint16_t ret;

	ret = (len >= offset + sizeof(uint16_t))
		? (packet[offset]<<8) | (packet[offset + 1]<<0)
		: 0;

	return ret;
}

static void ipv6_add_extra_description(char *buffer, size_t buffer_size, const uint8_t *packet, ssize_t len)
{
	uint8_t type(IPV6_GET_TYPE(packet));

	switch (type)
	{
	case IPv6PacketMatcherRule::TYPE_TCP:
		snprintf(buffer, buffer_size, "(cksum 0x%04x)", IPv6_TCP_GET_CHECKSUM(packet, len));
		break;

	case IPv6PacketMatcherRule::TYPE_UDP:
		snprintf(buffer, buffer_size, "(cksum 0x%04x)", IPv6_UDP_GET_CHECKSUM(packet, len));
		break;

	default:
		buffer[0] = 0;
	}
}

void
IPv6PacketMatcherRule::clear()
{
	memset((void*)this, 0, sizeof(*this));
	type = TYPE_ALL;
	subtype = SUBTYPE_ALL;
}

IPv6PacketMatcherRule&
IPv6PacketMatcherRule::update_from_inbound_packet(const uint8_t* packet)
{
	struct in6_addr address;

	clear();

	if (!PACKET_IS_IPV6(packet)) {
		goto bail;
	}

	type = IPV6_GET_TYPE(packet);

	subtype = IPv6PacketMatcherRule::SUBTYPE_ALL;

	if (type == IPv6PacketMatcherRule::TYPE_TCP || type == IPv6PacketMatcherRule::TYPE_UDP) {
		remote_port = IPV6_GET_SRC_PORT(packet);
		remote_port_match = true;

		local_port = IPV6_GET_DEST_PORT(packet);
		local_port_match = true;
	} else {
		remote_port = 0;
		remote_port_match = false;
		local_port = 0;
		local_port_match = false;
		if (type == IPv6PacketMatcherRule::TYPE_ICMP) {
			subtype = IPV6_ICMP_GET_SUBTYPE(packet);
		}
	}

	IPV6_GET_DEST_ADDR(address, packet);
	if (!IN6_IS_ADDR_MULTICAST(&address)) {
		local_address = address;
		local_match_mask = 128;
	} else {
		local_match_mask = 0;
	}

	IPV6_GET_SRC_ADDR(address, packet);
	remote_address = address;
	remote_match_mask = 128;

bail:
	return *this;
}


bool
IPv6PacketMatcherRule::match_inbound(const uint8_t* packet) const
{
	if (!PACKET_IS_IPV6(packet)) {
		return false;
	}

	if (type == TYPE_NONE) {
		return false;
	}

	if (type != IPv6PacketMatcherRule::TYPE_ALL) {
		if (type != IPV6_GET_TYPE(packet)) {
			return false;
		}
		if (subtype != IPv6PacketMatcherRule::SUBTYPE_ALL) {
			if (subtype != IPV6_ICMP_GET_SUBTYPE(packet)) {
				return false;
			}
		}
	}

	if (local_port_match) {
		in_port_t port(IPV6_GET_DEST_PORT(packet));
		if (port != local_port)
			return false;
	}

	if (remote_port_match) {
		in_port_t port(IPV6_GET_SRC_PORT(packet));
		if (port != remote_port)
			return false;
	}

	if (local_match_mask) {
		struct in6_addr address;
		IPV6_GET_DEST_ADDR(address, packet);
		in6_addr_apply_mask(address, local_match_mask);
		if (address != local_address)
			return false;
	}

	if (remote_match_mask) {
		struct in6_addr address;
		IPV6_GET_SRC_ADDR(address, packet);
		in6_addr_apply_mask(address, remote_match_mask);
		if (address != remote_address)
			return false;
	}
	return true;
}

IPv6PacketMatcherRule&
IPv6PacketMatcherRule::update_from_outbound_packet(const uint8_t* packet)
{
	struct in6_addr address;

	clear();

	if (!PACKET_IS_IPV6(packet)) {
		goto bail;
	}

	type = IPV6_GET_TYPE(packet);

	subtype = IPv6PacketMatcherRule::SUBTYPE_ALL;

	if (type == IPv6PacketMatcherRule::TYPE_TCP || type == IPv6PacketMatcherRule::TYPE_UDP) {
		remote_port = IPV6_GET_DEST_PORT(packet);
		remote_port_match = true;

		local_port = IPV6_GET_SRC_PORT(packet);
		local_port_match = true;
	} else {
		remote_port = 0;
		remote_port_match = false;
		local_port = 0;
		local_port_match = false;
		if (type == IPv6PacketMatcherRule::TYPE_ICMP) {
			subtype = IPV6_ICMP_GET_SUBTYPE(packet);
		}
	}

	IPV6_GET_SRC_ADDR(address, packet);
	local_address = address;
	local_match_mask = 128;

	IPV6_GET_DEST_ADDR(address, packet);
	remote_address = address;
	remote_match_mask = 128;

bail:
	return *this;
}

bool
IPv6PacketMatcherRule::match_outbound(const uint8_t* packet) const
{
	if (!PACKET_IS_IPV6(packet)) {
		return false;
	}

	if (type == TYPE_NONE) {
		return false;
	}

	if (type != IPv6PacketMatcherRule::TYPE_ALL) {
		if (type != IPV6_GET_TYPE(packet)) {
			return false;
		}
		if (subtype != IPv6PacketMatcherRule::SUBTYPE_ALL) {
			if (subtype != IPV6_ICMP_GET_SUBTYPE(packet)) {
				return false;
			}
		}
	}

	if (local_port_match) {
		in_port_t port(IPV6_GET_SRC_PORT(packet));
		if (port != local_port) {
			return false;
		}
	}

	if (remote_port_match) {
		in_port_t port(IPV6_GET_DEST_PORT(packet));
		if (port != remote_port) {
			return false;
		}
	}

	if (local_match_mask) {
		struct in6_addr address;
		IPV6_GET_SRC_ADDR(address, packet);
		in6_addr_apply_mask(address, local_match_mask);
		if (address != local_address) {
			return false;
		}
	}

	if (remote_match_mask) {
		struct in6_addr address;
		IPV6_GET_DEST_ADDR(address, packet);
		in6_addr_apply_mask(address, remote_match_mask);
		if (address != remote_address) {
			return false;
		}
	}
	return true;
}

bool
IPv6PacketMatcherRule::operator==(const IPv6PacketMatcherRule& lhs) const
{
#if IPV6_PACKET_MATCHER_DEBUG
	syslog(LOG_DEBUG, "IPv6PacketMatcherRule operator==():\n");

#define __DUMP(x,f,t) syslog(LOG_DEBUG, "\t lhs." #x "=" f "\t%s\trhs." #x "=" f "\n",(t)lhs.x,(lhs.x < x)?">":(x > lhs.x)?">":"==",(t)x);
#define __DUMP_NTOHS(x,f) syslog(LOG_DEBUG, "\t lhs." #x "=" f "\t%s\trhs." #x "=" f "\n",ntohs(lhs.x),(ntohs(lhs.x) < ntohs(x))?">":(ntohs(x) > ntohs(lhs.x))?">":"==",ntohs(x));
#define __DUMP_MEM(x) syslog(LOG_DEBUG, "\t lhs." #x "\t%s\trhs." #x "\n",(memcmp(&x, &lhs.x, 16) < 0)?">":(memcmp(&x, &lhs.x, 16) > 0)?">":"==");

	__DUMP(type,"%d",int);
	__DUMP(subtype,"%d",int);
	__DUMP_NTOHS(local_port,"%d");
	__DUMP(local_port_match,"%d",int);
	__DUMP(local_match_mask,"%d",int);
	__DUMP_MEM(local_address);

	__DUMP_NTOHS(remote_port,"%d");
	__DUMP(remote_port_match,"%d",int);
	__DUMP(remote_match_mask,"%d",int);
	__DUMP_MEM(remote_address);

#undef __DUMP
#undef __DUMP_NTOHS
#undef __DUMP_MEM

#endif // IPV6_PACKET_MATCHER_DEBUG

	if (type != lhs.type) {
		return false;
	}
	if (subtype != lhs.subtype) {
		return false;
	}
	if (local_port != lhs.local_port) {
		return false;
	}
	if (local_port_match != lhs.local_port_match) {
		return false;
	}
	if (local_match_mask != lhs.local_match_mask) {
		return false;
	}
	if (local_address != lhs.local_address) {
		return false;
	}
	if (remote_port != lhs.remote_port) {
		return false;
	}
	if (remote_port_match != lhs.remote_port_match) {
		return false;
	}
	if (remote_match_mask != lhs.remote_match_mask) {
		return false;
	}
	if (remote_address != lhs.remote_address) {
		return false;
	}
	return true;

	return 0 == memcmp(this,
	                   &lhs,
	                   (uint8_t*)&remote_match_mask - (uint8_t*)this + 1);
}

bool
IPv6PacketMatcherRule::operator<(const IPv6PacketMatcherRule& lhs) const
{
	if (type < lhs.type) {
		return true;
	} else if (type > lhs.type) {
		return false;
	}

	if (subtype < lhs.subtype) {
		return true;
	} else if (subtype > lhs.subtype) {
		return false;
	}

	if (local_port < lhs.local_port) {
		return true;
	} else if (local_port > lhs.local_port) {
		return false;
	}

	if (local_port_match < lhs.local_port_match) {
		return true;
	} else if (local_port_match > lhs.local_port_match) {
		return false;
	}

	if (local_match_mask < lhs.local_match_mask) {
		return true;
	} else if (local_match_mask > lhs.local_match_mask) {
		return false;
	}

	if (memcmp(&local_address, &lhs.local_address, 16) < 0) {
		return true;
	} else if (memcmp(&local_address, &lhs.local_address, 16) > 0) {
		return false;
	}

	if (remote_port < lhs.remote_port) {
		return true;
	} else if (remote_port > lhs.remote_port) {
		return false;
	}

	if (remote_port_match < lhs.remote_port_match) {
		return true;
	} else if (remote_port_match > lhs.remote_port_match) {
		return false;
	}

	if (remote_match_mask < lhs.remote_match_mask) {
		return true;
	} else if (remote_match_mask > lhs.remote_match_mask) {
		return false;
	}

	if (memcmp(&remote_address, &lhs.remote_address, 16) < 0) {
		return true;
	} else if (memcmp(&remote_address, &lhs.remote_address, 16) > 0) {
		return false;
	}

	return false;
}

IPv6PacketMatcher::const_iterator
IPv6PacketMatcher::match_outbound(const uint8_t* packet) const
{
	iterator iter;
	for(iter = begin(); iter != end(); ++iter) {
		if(iter->match_outbound(packet)) {
			break;
		}
	}
	return iter;
}

IPv6PacketMatcher::const_iterator
IPv6PacketMatcher::match_inbound(const uint8_t* packet) const
{
	iterator iter;
	for(iter = begin(); iter != end(); ++iter) {
		if(iter->match_inbound(packet)) {
			break;
		}
	}
	return iter;
}

void
nl::dump_outbound_ipv6_packet(const uint8_t* packet, ssize_t len, const char* extra, bool dropped)
{
	if(!(setlogmask(0)&LOG_MASK(LOG_INFO))) {
		return;
	}
	char to_addr_cstr[INET6_ADDRSTRLEN] = "::";
	char from_addr_cstr[INET6_ADDRSTRLEN] = "::";
	uint8_t type(IPV6_GET_TYPE(packet));
	char type_extra[32];
	struct in6_addr addr;

	ipv6_add_extra_description(type_extra, sizeof(type_extra), packet, len);

	syslog(LOG_INFO,
		   "[->NCP] IPv6 len:%d type:%d%s [%s]%s",
		   (int)len,
		   type,
		   type_extra,
		   extra,
		   dropped?" [DROPPED]":""
	);

	IPV6_GET_SRC_ADDR(addr, packet);
	inet_ntop(AF_INET6, addr.s6_addr, from_addr_cstr, sizeof(from_addr_cstr));

	IPV6_GET_DEST_ADDR(addr, packet);
	inet_ntop(AF_INET6, addr.s6_addr, to_addr_cstr, sizeof(to_addr_cstr));

	if ((type == IPv6PacketMatcherRule::TYPE_TCP)
		|| (type == IPv6PacketMatcherRule::TYPE_UDP)
	) {
		in_port_t to_port(IPV6_GET_DEST_PORT(packet));
		in_port_t from_port(IPV6_GET_SRC_PORT(packet));

		syslog(LOG_INFO,
			   "\tto(remote):[%s]:%d",
			   to_addr_cstr,
			   htons(to_port));

		syslog(LOG_INFO,
			   "\tfrom(local):[%s]:%d",
			   from_addr_cstr,
			   htons(from_port));
	} else {
		syslog(LOG_INFO, "\tto(remote):[%s]", to_addr_cstr);
		syslog(LOG_INFO, "\tfrom(local):[%s]", from_addr_cstr);
	}
}

void
nl::dump_inbound_ipv6_packet(const uint8_t* packet, ssize_t len, const char* extra, bool dropped)
{
	if(!(setlogmask(0)&LOG_MASK(LOG_INFO))) {
		return;
	}
	char to_addr_cstr[INET6_ADDRSTRLEN] = "::";
	char from_addr_cstr[INET6_ADDRSTRLEN] = "::";
	uint8_t type(IPV6_GET_TYPE(packet));
	char type_extra[32];
	struct in6_addr addr;

	ipv6_add_extra_description(type_extra, sizeof(type_extra), packet, len);

	syslog(LOG_INFO,
		   "[NCP->] IPv6 len:%d type:%d%s [%s]%s",
		   (int)len,
		   type,
		   type_extra,
		   extra,
		   dropped?" [DROPPED]":""
	);

	IPV6_GET_SRC_ADDR(addr, packet);
	inet_ntop(AF_INET6, addr.s6_addr, from_addr_cstr, sizeof(from_addr_cstr));

	IPV6_GET_DEST_ADDR(addr, packet);
	inet_ntop(AF_INET6, addr.s6_addr, to_addr_cstr, sizeof(to_addr_cstr));
	if ((type == IPv6PacketMatcherRule::TYPE_TCP)
		|| (type == IPv6PacketMatcherRule::TYPE_UDP)
	) {
		in_port_t to_port(IPV6_GET_DEST_PORT(packet));
		in_port_t from_port(IPV6_GET_SRC_PORT(packet));

		syslog(LOG_INFO,
			   "\tto(local):[%s]:%d",
			   to_addr_cstr,
			   htons(to_port));

		syslog(LOG_INFO,
			   "\tfrom(remote):[%s]:%d",
			   from_addr_cstr,
			   htons(from_port));
	} else {
		syslog(LOG_INFO, "\tto(local):[%s]", to_addr_cstr);
		syslog(LOG_INFO, "\tfrom(remote):[%s]", from_addr_cstr);
	}
}
