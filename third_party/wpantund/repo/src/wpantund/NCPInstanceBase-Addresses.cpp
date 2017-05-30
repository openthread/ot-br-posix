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
NCPInstanceBase::refresh_global_addresses()
{
	// Here is where we would do any periodic global address bookkeeping,
	// which doesn't appear to be necessary yet but may become necessary
	// in the future.
}

void
NCPInstanceBase::clear_nonpermanent_global_addresses()
{
	std::map<struct in6_addr, GlobalAddressEntry>::iterator iter;

	// We want to remove all of the addresses that were
	// not user-added.
	//
	// This loop looks a little weird because we are mutating
	// the container as we are iterating through it. Whenever
	// we mutate the container we have to start over.
	do {
		for (iter = mGlobalAddresses.begin(); iter != mGlobalAddresses.end(); ++iter) {
			// Skip the removal of user-added addresses.
			if (iter->second.mUserAdded) {
				continue;
			}

			mPrimaryInterface->remove_address(&iter->first);
			mGlobalAddresses.erase(iter);

			// The following assignment is needed to avoid
			// an invalid iterator comparison in the outer loop.
			iter = mGlobalAddresses.begin();

			// Break out of the inner loop so that we start over.
			break;
		}
	} while(iter != mGlobalAddresses.end());
}

void
NCPInstanceBase::restore_global_addresses()
{
	std::map<struct in6_addr, GlobalAddressEntry>::const_iterator iter;
	std::map<struct in6_addr, GlobalAddressEntry> global_addresses(mGlobalAddresses);

	mGlobalAddresses.clear();

	for (iter = global_addresses.begin(); iter!= global_addresses.end(); ++iter) {
		if (iter->second.mUserAdded) {
			address_was_added(iter->first, 64);
		}
		mGlobalAddresses.insert(*iter);

		mPrimaryInterface->add_address(&iter->first);
	}
}

void
NCPInstanceBase::add_address(const struct in6_addr &address, uint8_t prefix, uint32_t valid_lifetime, uint32_t preferred_lifetime)
{
	GlobalAddressEntry entry = GlobalAddressEntry();

	if (mGlobalAddresses.count(address)) {
		syslog(LOG_INFO, "Updating IPv6 Address...");
		entry = mGlobalAddresses[address];
	} else {
		syslog(LOG_INFO, "Adding IPv6 Address...");
		mPrimaryInterface->add_address(&address);
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

	mGlobalAddresses[address] = entry;
}

void
NCPInstanceBase::remove_address(const struct in6_addr &address)
{
	mGlobalAddresses.erase(address);
	mPrimaryInterface->remove_address(&address);
}

bool
NCPInstanceBase::is_address_known(const struct in6_addr &address)
{
	bool ret(mGlobalAddresses.count(address) != 0);

	return ret;
}

bool
NCPInstanceBase::lookup_address_for_prefix(struct in6_addr *address, const struct in6_addr &prefix, int prefix_len_in_bits)
{
	struct in6_addr masked_prefix(prefix);

	in6_addr_apply_mask(masked_prefix, prefix_len_in_bits);

	std::map<struct in6_addr, GlobalAddressEntry>::const_iterator iter;
	for (iter = mGlobalAddresses.begin(); iter != mGlobalAddresses.end(); ++iter) {
		struct in6_addr iter_prefix(iter->first);
		in6_addr_apply_mask(iter_prefix, prefix_len_in_bits);

		if (iter_prefix == masked_prefix) {
			if (address != NULL) {
				*address = iter->first;
			}
			return true;
		}
	}
	return false;
}

void
NCPInstanceBase::address_was_added(const struct in6_addr& addr, int prefix_len)
{
	char addr_cstr[INET6_ADDRSTRLEN] = "::";
	inet_ntop(
		AF_INET6,
		&addr,
		addr_cstr,
		sizeof(addr_cstr)
	);

	syslog(LOG_NOTICE, "\"%s\" was added to \"%s\"", addr_cstr, mPrimaryInterface->get_interface_name().c_str());

	if (mGlobalAddresses.count(addr) == 0) {
		const GlobalAddressEntry entry = {
			UINT32_MAX,          // .mValidLifetime
			TIME_DISTANT_FUTURE, // .mValidLifetimeExpiration
			UINT32_MAX,          // .mPreferredLifetime
			TIME_DISTANT_FUTURE, // .mPreferredLifetimeExpiration
			0,                   // .mFlags
			1,                   // .mUserAdded
		};

		mGlobalAddresses[addr] = entry;
	}
}

void
NCPInstanceBase::address_was_removed(const struct in6_addr& addr, int prefix_len)
{
	char addr_cstr[INET6_ADDRSTRLEN] = "::";
	inet_ntop(
		AF_INET6,
		&addr,
		addr_cstr,
		sizeof(addr_cstr)
	);

	if ((mGlobalAddresses.count(addr) != 0)
	 && (mPrimaryInterface->is_online() || !mGlobalAddresses[addr].mUserAdded)
	) {
		mGlobalAddresses.erase(addr);
	}

	syslog(LOG_NOTICE, "\"%s\" was removed from \"%s\"", addr_cstr, mPrimaryInterface->get_interface_name().c_str());
}
