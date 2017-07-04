/*
 *
 * Copyright (c) 2017 Nest Labs, Inc.
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

#include "assert-macros.h"
#include "NCPInstanceBase.h"
#include "tunnel.h"
#include <syslog.h>
#include <errno.h>
#include "nlpt.h"
#include <algorithm>
#include "socket-utils.h"
#include "SuperSocket.h"

using namespace nl;
using namespace wpantund;

void
NCPInstanceBase::add_prefix(const struct in6_addr &address, uint32_t valid_lifetime, uint32_t preferred_lifetime, uint8_t flags)
{
	GlobalAddressEntry entry = GlobalAddressEntry();

	if (mOnMeshPrefixes.count(address)) {
		syslog(LOG_INFO, "Updating IPv6 prefix...");
		entry = mOnMeshPrefixes[address];
	} else {
		syslog(LOG_INFO, "Adding IPv6 prefix...");
		mOnMeshPrefixes.insert(std::pair<struct in6_addr, GlobalAddressEntry>(address, entry));
	}

	entry.mValidLifetime = valid_lifetime;
	entry.mPreferredLifetime = preferred_lifetime;
	entry.mValidLifetimeExpiration = ((valid_lifetime == UINT32_MAX)
		? TIME_DISTANT_FUTURE
		: time_get_monotonic() + valid_lifetime
	);
	entry.mPreferredLifetimeExpiration = ((valid_lifetime == UINT32_MAX)
		? TIME_DISTANT_FUTURE
		: time_get_monotonic() + preferred_lifetime
	);
	entry.mFlags = flags;
	mOnMeshPrefixes[address] = entry;
}
