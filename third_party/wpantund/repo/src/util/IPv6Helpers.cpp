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
 *      This file contains helper glue code for manipulating IPv6 addresses.
 *
 */

#include "IPv6Helpers.h"

std::string
in6_addr_to_string(const struct in6_addr &addr)
{
	char address_string[INET6_ADDRSTRLEN] = "::";
	inet_ntop(AF_INET6, (const void *)&addr, address_string, sizeof(address_string));
	return std::string(address_string);
}

struct in6_addr
make_slaac_addr_from_eui64(const uint8_t prefix[8], const uint8_t eui64[8])
{
	struct in6_addr ret;

	// Construct the IPv6 address from the prefix and EUI64.
	memcpy(&ret.s6_addr[0], prefix, 8);
	memcpy(&ret.s6_addr[8], eui64, 8);

	// Flip the administratively assigned bit.
	ret.s6_addr[8] ^= 0x02;

	return ret;
}

void
in6_addr_apply_mask(struct in6_addr &address, uint8_t mask)
{
	if (mask > 128) {
		mask = 128;
	}

	memset(
		(void*)(address.s6_addr + ((mask + 7) / 8)),
		0,
		16 - ((mask + 7) / 8)
	);

	if (mask % 8) {
		address.s6_addr[mask / 8] &= ~(0xFF >> (mask % 8));
	}
}
