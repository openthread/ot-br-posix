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

#include "NCPTypes.h"
#include "string-utils.h"
#include <string>
#include "wpan-properties.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

using namespace nl;
using namespace wpantund;


// ----------------------------------------------------------------------------
// MARK: -
// MARK: Static Methods

bool
nl::wpantund::ncp_state_is_sleeping(NCPState x)
{
	switch(x) {
	case DEEP_SLEEP:
	case NET_WAKE_ASLEEP:
		return true;
	default:
		return false;
	}
}

bool
nl::wpantund::ncp_state_has_joined(NCPState x)
{
	switch(x) {
	case ASSOCIATED:
	case ISOLATED:
	case NET_WAKE_ASLEEP:
	case NET_WAKE_WAKING:
		return true;
	default:
		return false;
	}
}

bool
nl::wpantund::ncp_state_is_joining(NCPState x)
{
	switch(x) {
	case ASSOCIATING:
	case CREDENTIALS_NEEDED:
		return true;
	default:
		return false;
	}
}

bool
nl::wpantund::ncp_state_is_interface_up(NCPState x)
{
	switch(x) {
	case CREDENTIALS_NEEDED:
	case ASSOCIATED:
	case NET_WAKE_ASLEEP:
		return true;
	default:
		return false;
	}
}

bool
nl::wpantund::ncp_state_is_commissioned(NCPState x)
{
	switch(x) {
	case COMMISSIONED:
	case ASSOCIATED:
	case NET_WAKE_ASLEEP:
	case ISOLATED:
	case NET_WAKE_WAKING:
		return true;
	default:
		return false;
	}
}

bool
nl::wpantund::ncp_state_is_initializing(NCPState x)
{
	switch(x) {
	case UNINITIALIZED:
	case UPGRADING:
		return true;
	default:
		return false;
	}
}

bool
nl::wpantund::ncp_state_is_joining_or_joined(NCPState x)
{
	switch(x) {
	case CREDENTIALS_NEEDED:
	case ASSOCIATING:
	case ASSOCIATED:
	case ISOLATED:
	case NET_WAKE_WAKING:
	case NET_WAKE_ASLEEP:
		return true;
	default:
		return false;
	}
}

bool
nl::wpantund::ncp_state_is_associated(NCPState x)
{
	switch(x) {
	case ASSOCIATED:
	case ISOLATED:
	case NET_WAKE_WAKING:
	case NET_WAKE_ASLEEP:
		return true;
	default:
		return false;
	}
}

bool
nl::wpantund::ncp_state_is_detached_from_ncp(NCPState x)
{
	switch(x) {
	case FAULT:
	case UPGRADING:
		return true;
	default:
		return false;
	}
}

bool
nl::wpantund::ncp_state_is_busy(NCPState x)
{
	switch(x) {
	case DEEP_SLEEP:
	case OFFLINE:
	case NET_WAKE_ASLEEP:
	case ISOLATED:
	case ASSOCIATED:
	case FAULT:
		return false;
	default:
		return true;
	}
}

std::string
nl::wpantund::ncp_state_to_string(NCPState state)
{
	switch (state) {
	case UNINITIALIZED:      return kWPANTUNDStateUninitialized;
	case FAULT:              return kWPANTUNDStateFault;
	case UPGRADING:          return kWPANTUNDStateUpgrading;
	case DEEP_SLEEP:         return kWPANTUNDStateDeepSleep;
	case OFFLINE:            return kWPANTUNDStateOffline;
	case COMMISSIONED:       return kWPANTUNDStateCommissioned;
	case ASSOCIATING:        return kWPANTUNDStateAssociating;
	case CREDENTIALS_NEEDED: return kWPANTUNDStateCredentialsNeeded;
	case ASSOCIATED:         return kWPANTUNDStateAssociated;
	case ISOLATED:           return kWPANTUNDStateIsolated;
	case NET_WAKE_ASLEEP:    return kWPANTUNDStateNetWake_Asleep;
	case NET_WAKE_WAKING:    return kWPANTUNDStateNetWake_Waking;
	}
	return std::string("unknown-state");
}

std::string
nl::wpantund::node_type_to_string(NodeType node_type)
{
	std::string ret;

	switch (node_type) {
	case UNKNOWN:
		ret = kWPANTUNDNodeType_Unknown;
		break;
	case END_DEVICE:
		ret = kWPANTUNDNodeType_EndDevice;
		break;
	case SLEEPY_END_DEVICE:
		ret = kWPANTUNDNodeType_SleepyEndDevice;
		break;
	case ROUTER:
		ret = kWPANTUNDNodeType_Router;
		break;
	case LURKER:
		ret = kWPANTUNDNodeType_NestLurker;
		break;
	case LEADER:
		ret = kWPANTUNDNodeType_Leader;
		break;
	case COMMISSIONER:
		ret = kWPANTUNDNodeType_Commissioner;
		break;
	default:
		{
			char cstr[16];
			snprintf(cstr, sizeof(cstr), "(node-type-%u)", (unsigned int)node_type);
			ret = cstr;
		}
		break;
	}

	return ret;
}

nl::wpantund::NodeType
nl::wpantund::string_to_node_type(const std::string& node_type_string)
{
	if (strcaseequal(node_type_string.c_str(), kWPANTUNDNodeType_EndDevice) ||
	    strcaseequal(node_type_string.c_str(), "end") ||
	    strcaseequal(node_type_string.c_str(), "e") ||
	    (node_type_string == "3"))
	{
		return END_DEVICE;
	}

	if (strcaseequal(node_type_string.c_str(), kWPANTUNDNodeType_SleepyEndDevice) ||
	    strcaseequal(node_type_string.c_str(), "sleepy") ||
	    strcaseequal(node_type_string.c_str(), "sed") ||
	    strcaseequal(node_type_string.c_str(), "s") ||
	    (node_type_string == "4"))
	{
		return SLEEPY_END_DEVICE;
	}

	if (strcaseequal(node_type_string.c_str(), kWPANTUNDNodeType_Router) ||
	    strcaseequal(node_type_string.c_str(), "r") ||
	    (node_type_string == "2"))
	{
		return ROUTER;
	}

	if (strcaseequal(node_type_string.c_str(), kWPANTUNDNodeType_Leader))
	{
		return LEADER;
	}

	if (strcaseequal(node_type_string.c_str(), kWPANTUNDNodeType_NestLurker) ||
	    strcaseequal(node_type_string.c_str(), "lurker") ||
	    (node_type_string == "6"))
	{
		return LURKER;
	}

	return UNKNOWN;
}

std::string
nl::wpantund::address_flags_to_string(uint8_t flags)
{
	std::string ret;
	int i;
	for (i = 7; i >=0 ; --i) {
		uint8_t mask = (1<<i);
		if (i == 3) {
			ret += ' ';
		}
		if (!(mask & flags)) {
			ret += '-';
			continue;
		}
		switch (mask) {
		case GA_AM_GATEWAY:			//0
			ret += 'G';
			break;
		case GA_AM_DHCP_SERVER:		//1
			ret += 'D';
			break;
		case GA_AM_SLAAC_SERVER:	//2
			ret += 'S';
			break;
		case GA_DHCP:				// 3
			ret += 'd';
			break;
		case GA_SLAAC:				// 4
			ret += 's';
			break;
		case GA_CONFIGURED:			// 5
			ret += 'C';
			break;
		case GA_REQUEST_SENT:		//6
			ret += 'R';
			break;
		case GA_REQUEST_FAILED:		//7
			ret += 'F';
			break;
		default:
			ret += '0' + i;
			break;
		}
	}
	return ret;
}

std::string
nl::wpantund::flags_to_string(uint8_t flags, const char flag_lookup[8])
{
	std::string ret;
	int i;
	if (flag_lookup == NULL) {
		flag_lookup = "76543210";
	}
	for (i = 7; i >= 0 ; --i) {
		uint8_t mask = (1<<i);
		if (i == 3) {
			ret += ' ';
		}
		if (!(mask & flags)) {
			ret += '-';
			continue;
		}
		ret += flag_lookup[7-i];
	}
	return ret;
}

NCPState
nl::wpantund::string_to_ncp_state(const std::string& state_string)
{
	if (state_string == kWPANTUNDStateFault) {
		return FAULT;
	} else if (state_string == kWPANTUNDStateUpgrading) {
		return UPGRADING;
	} else if (state_string == kWPANTUNDStateDeepSleep) {
		return DEEP_SLEEP;
	} else if (state_string == kWPANTUNDStateCommissioned) {
		return COMMISSIONED;
	} else if (state_string == kWPANTUNDStateCredentialsNeeded) {
		return CREDENTIALS_NEEDED;
	} else if (state_string == kWPANTUNDStateIsolated) {
		return ISOLATED;
	} else if (state_string == kWPANTUNDStateNetWake_Asleep) {
		return NET_WAKE_ASLEEP;
	} else if (state_string == kWPANTUNDStateNetWake_Waking) {
		return NET_WAKE_WAKING;
	}

	if (strnequal(state_string.c_str(), kWPANTUNDStateUninitialized, sizeof(kWPANTUNDStateUninitialized)-1)) {
		return UNINITIALIZED;
	} else if (strnequal(state_string.c_str(), kWPANTUNDStateOffline, sizeof(kWPANTUNDStateOffline)-1)) {
		return OFFLINE;
	} else if (strnequal(state_string.c_str(), kWPANTUNDStateAssociating, sizeof(kWPANTUNDStateAssociating)-1)) {
		return ASSOCIATING;
	} else if (strnequal(state_string.c_str(), kWPANTUNDStateAssociated, sizeof(kWPANTUNDStateAssociated)-1)) {
		return ASSOCIATED;
	}

	// Unknown
	return UNINITIALIZED;
}

std::string
GlobalAddressEntry::get_description() const
{
	char c_string[200];

	snprintf(c_string, sizeof(c_string), "valid: %u  preferred: %u  flags: %s (0x%02X)",
				mValidLifetime,
				mPreferredLifetime,
				address_flags_to_string(mFlags).c_str(),
				mFlags);

	return std::string(c_string);
}
