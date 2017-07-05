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
NCPInstanceBase::link_state_changed(bool isUp, bool isRunning)
{
	syslog(LOG_INFO, "Primary link state changed: UP=%d RUNNING=%d", isUp, isRunning);

	// The big take away from this callback is "isUp",
	// because theoretically we are the one who is in charge
	// of "isRunning". We interpret isUp as meaning if
	// we should autoconnect or not.

	if (isUp) {
		property_set_value(kWPANTUNDProperty_DaemonAutoAssociateAfterReset, true);

		if (!mEnabled) {
			property_set_value(kWPANTUNDProperty_DaemonEnabled, true);
		} else if (get_ncp_state() == COMMISSIONED) {
			property_set_value(kWPANTUNDProperty_InterfaceUp, true);
		}
	} else {
		property_set_value(kWPANTUNDProperty_DaemonAutoAssociateAfterReset, false);
		property_set_value(kWPANTUNDProperty_InterfaceUp, false);
	}
}

void
NCPInstanceBase::legacy_link_state_changed(bool isUp, bool isRunning)
{
	syslog(LOG_INFO, "Legacy link state changed: UP=%d RUNNING=%d", isUp, isRunning);

	// TODO: Not sure what the best course of action is here.
}

int
NCPInstanceBase::set_online(bool x)
{
	int ret;

	ret = mPrimaryInterface->set_online(x);

	restore_global_addresses();

	if (IN6_IS_ADDR_LINKLOCAL(&mNCPLinkLocalAddress)) {
		add_address(mNCPLinkLocalAddress);
	}

	if (buffer_is_nonzero(mNCPMeshLocalAddress.s6_addr, sizeof(mNCPMeshLocalAddress)))	{
		add_address(mNCPMeshLocalAddress);
	}

	if ((ret == 0) && static_cast<bool>(mLegacyInterface)) {
		if (x && mNodeTypeSupportsLegacy) {
			ret = mLegacyInterface->set_online(true);

			if (IN6_IS_ADDR_LINKLOCAL(&mNCPLinkLocalAddress)) {
				mLegacyInterface->add_address(&mNCPLinkLocalAddress);
			}
		} else {
			ret = mLegacyInterface->set_online(false);
		}
	}

	return ret;
}

void
NCPInstanceBase::set_mac_address(const uint8_t x[8])
{
	if (0 != memcmp(x, mMACAddress, sizeof(mMACAddress))) {
		memcpy(mMACAddress, x, sizeof(mMACAddress));

		syslog(
			LOG_INFO,
			"NCP Status: MACAddr:           %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
			mMACAddress[0],mMACAddress[1],mMACAddress[2],mMACAddress[3],
			mMACAddress[4],mMACAddress[5],mMACAddress[6],mMACAddress[7]
		);

		if ((x[0] & 1) == 1) {
			syslog(LOG_WARNING,"MAC ADDRESS IS INVALID, MULTICAST BIT IS SET!");
		}

		signal_property_changed(kWPANTUNDProperty_NCPMACAddress, Data(mMACAddress, sizeof(mMACAddress)));

	}

	if (!buffer_is_nonzero(mMACHardwareAddress, sizeof(mMACHardwareAddress))) {
		set_mac_hardware_address(x);
	}
}

void
NCPInstanceBase::set_mac_hardware_address(const uint8_t x[8])
{
	if (0 != memcmp(x, mMACHardwareAddress, sizeof(mMACHardwareAddress))) {
		memcpy(mMACHardwareAddress, x, sizeof(mMACHardwareAddress));

		syslog(
			LOG_INFO,
			"NCP Status: MACHardwareAddr:   %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
			mMACHardwareAddress[0],mMACHardwareAddress[1],mMACHardwareAddress[2],mMACHardwareAddress[3],
			mMACHardwareAddress[4],mMACHardwareAddress[5],mMACHardwareAddress[6],mMACHardwareAddress[7]
		);

		if ((x[0] & 1) == 1) {
			syslog(LOG_WARNING,"HARDWARE ADDRESS IS INVALID, MULTICAST BIT IS SET!");
		}

		signal_property_changed(kWPANTUNDProperty_NCPHardwareAddress, Data(mMACHardwareAddress, sizeof(mMACHardwareAddress)));

	}
}

void
NCPInstanceBase::reset_interface(void)
{
	syslog(LOG_NOTICE, "Resetting interface(s). . .");

	mCurrentNetworkInstance.joinable = false;

	set_commissioniner(0, 0, 0);

	mPrimaryInterface->reset();

	// The global address table must be cleared upon reset.
	mGlobalAddresses.clear();

	if (static_cast<bool>(mLegacyInterface)) {
		mLegacyInterface->reset();
	}
}

void
NCPInstanceBase::enable_legacy_interface(void)
{
	if (!static_cast<bool>(mLegacyInterface)) {
		mLegacyInterface = boost::shared_ptr<TunnelIPv6Interface>(new TunnelIPv6Interface(mPrimaryInterface->get_interface_name()+"-L"));
		mLegacyInterface->mLinkStateChanged.connect(boost::bind(&NCPInstanceBase::legacy_link_state_changed, this, _1, _2));
	}
}

bool
NCPInstanceBase::is_legacy_interface_enabled(void)
{
	if (mNodeTypeSupportsLegacy) {
		return static_cast<bool>(mLegacyInterface);
	}
	return false;
}



int
NCPInstanceBase::join_multicast_group(const std::string &group_name)
{
	int ret = -1;
	struct ipv6_mreq imreq;
	unsigned int value = 1;
	struct hostent *tmp = gethostbyname2(group_name.c_str(), AF_INET6);
	memset(&imreq, 0, sizeof(imreq));

	if (mMCFD < 0) {
		mMCFD = socket(AF_INET6, SOCK_DGRAM, 0);
	}

	require(mMCFD >= 0, skip_mcast);

	require(!h_errno && tmp, skip_mcast);
	require(tmp->h_length > 1, skip_mcast);

	memcpy(&imreq.ipv6mr_multiaddr.s6_addr, tmp->h_addr_list[0], 16);

	value = 1;
	ret = setsockopt(
		mMCFD,
		IPPROTO_IPV6,
		IPV6_MULTICAST_LOOP,
		&value,
		sizeof(value)
	);
	require_noerr(ret, skip_mcast);

	imreq.ipv6mr_interface = if_nametoindex(mPrimaryInterface->get_interface_name().c_str());

	ret = setsockopt(
		mMCFD,
		IPPROTO_IPV6,
		IPV6_JOIN_GROUP,
		&imreq,
		sizeof(imreq)
	);
	require_noerr(ret, skip_mcast);

skip_mcast:

	if (ret) {
		syslog(LOG_WARNING, "Failed to join multicast group \"%s\"", group_name.c_str());
	}

	return ret;
}


int
NCPInstanceBase::set_commissioniner(
    int seconds, uint8_t traffic_type, in_port_t traffic_port
    )
{
	int ret = kWPANTUNDStatus_Ok;

	mCommissioningRule.clear();

	if ((seconds > 0) && (traffic_port == 0)) {
		ret = kWPANTUNDStatus_InvalidArgument;
		goto bail;
	}

	if (seconds > 0 && traffic_port) {
		mCommissioningExpiration = time_get_monotonic() + seconds;

		mCommissioningRule.type = traffic_type;
		mCommissioningRule.local_port = traffic_port;
		mCommissioningRule.local_port_match = true;
		mCommissioningRule.local_address.s6_addr[0] = 0xFE;
		mCommissioningRule.local_address.s6_addr[1] = 0x80;
		mCommissioningRule.local_match_mask = 10;
	} else {
		mCommissioningExpiration = 0;
		mInsecureFirewall.clear();
	}

bail:
	return ret;
}



void
NCPInstanceBase::handle_normal_ipv6_from_ncp(const uint8_t* ip_packet, size_t packet_length)
{
	ssize_t ret = mPrimaryInterface->write(ip_packet, packet_length);

	if (ret != packet_length) {
		syslog(LOG_INFO, "[NCP->] IPv6 packet refused by host stack! (ret = %ld)", (long)ret);
	}
}

void
NCPInstanceBase::handle_alt_ipv6_from_ncp(const uint8_t* ip_packet, size_t packet_length)
{
	ssize_t ret = mLegacyInterface->write(ip_packet, packet_length);

	if (ret != packet_length) {
		syslog(LOG_INFO, "[NCP->] IPv6 packet refused by host stack! (ret = %ld)", (long)ret);
	}
}

#define FRAME_TYPE_TO_CSTR(x) \
			       (((x) == FRAME_TYPE_INSECURE_DATA) \
					? "INSECURE" \
					: ((x) == FRAME_TYPE_LEGACY_DATA) \
						? "LEGACY" \
						: "SECURE")

// This function does a little more than it's name might imply.
// It will also mutate the NCP frame to reflect the appropriate
// interface if the firewall rules indicate that it should be
// handled differently (i.e., a insecure packet that matches
// the appropriate firewall rules will be re-tagged as a normal
// packet, etc.)
bool
NCPInstanceBase::should_forward_hostbound_frame(uint8_t* type, const uint8_t* ip_packet, size_t packet_length)
{
	bool packet_should_be_dropped = false;
	IPv6PacketMatcherRule rule;

	rule.update_from_inbound_packet(ip_packet);

	// Handle special considerations for packets received
	// from the insecure data channel.
	if (*type == FRAME_TYPE_INSECURE_DATA) {
		// If the packet is from the insecure channel, we
		// mark the packed as "to be dropped" by default.
		// We perform some additional checks below which
		// may switch this back to `false`.
		packet_should_be_dropped = true;

		if (ncp_state_is_joining(get_ncp_state())) {
			// Don't drop data from insecure channel if we
			// aren't joined yet.
			packet_should_be_dropped = false;

		} else if (mCommissioningExpiration != 0) {
			if (mCommissioningExpiration > time_get_monotonic()) {
				// We are in the middle of commissioning and we
				// haven't expired yet.

				if (mInsecureFirewall.count(rule)) {
					syslog(LOG_INFO,
						   "[NCP->] Routing insecure commissioning traffic.");
					packet_should_be_dropped = false;
				} else if (mCommissioningRule.match_inbound(ip_packet)) {
					rule.subtype = IPv6PacketMatcherRule::SUBTYPE_ALL;
					mInsecureFirewall.insert(rule);
					packet_should_be_dropped = false;
					syslog(LOG_INFO,
						   "[NCP->] Tracking *NEW* insecure commissioning connection.");
				} else if (rule.type == IPv6PacketMatcherRule::TYPE_ICMP) {
					mInsecureFirewall.insert(rule);
					packet_should_be_dropped = false;
					syslog(LOG_INFO,
						   "[NCP->] Tracking *NEW* ICMP ping during commissioning.");
				} else {
					syslog(LOG_INFO,
						   "[NCP->] Non-matching insecure traffic while joinable, ignoring");
				}
			} else {
				// Commissioning has ended. Clean up.
				syslog(LOG_NOTICE, "Commissioning period has ended");
				mCommissioningExpiration = 0;
				mInsecureFirewall.clear();
			}
		}
	} else if (
		(   (*type == FRAME_TYPE_DATA)
			|| (*type == FRAME_TYPE_LEGACY_DATA)
		)
		&&	(mInsecureFirewall.size()>0)
	) {
		// In this case we want to make sure that if we receive
		// a packet on a secure channel that we were previously
		// routing over the insecure channel that we remove the
		// matching entry so that we don't route those packets
		// over the insecure channel any more.

		if (mInsecureFirewall.count(rule)) {
			syslog(LOG_NOTICE, "Secure packet matched rule on insecure firewall, removing rule.");
			mInsecureFirewall.erase(rule);

			if (*type == FRAME_TYPE_LEGACY_DATA) {
				// The first packet to match the rule on the insecure firewall
				// was from the legacy interface. To ensure the continuity
				// of the connection, we need to ensure that the packets
				// for this session continue to come out of the non-legacy
				// interface. We do that by adding them to this packet matcher.
				mLegacyCommissioningMatcher.insert(rule);
			}
		}
	}

	// If our legacy interface isn't enabled, drop all legacy traffic.
	if (*type == FRAME_TYPE_LEGACY_DATA) {
		packet_should_be_dropped |= !is_legacy_interface_enabled();
	}

	// Debug logging.
	dump_inbound_ipv6_packet(
		ip_packet,
		packet_length,
		FRAME_TYPE_TO_CSTR(*type),
		packet_should_be_dropped
	);

	// Make sure the interface is up.
	if (!ncp_state_is_interface_up(get_ncp_state())) {
		// The interface is down, so we don't send the packet.
		packet_should_be_dropped = true;

		// Check to see if the NCP is supposed to be asleep:
		if (ncp_state_is_sleeping(get_ncp_state())) {
			syslog(LOG_ERR, "Got IPv6 traffic when we should be asleep! (%s)",ncp_state_to_string(get_ncp_state()).c_str());
			ncp_is_misbehaving();
		} else {
			syslog(LOG_WARNING, "Ignoring IPv6 traffic while in %s state.", ncp_state_to_string(get_ncp_state()).c_str());
		}
	}

	if ((*type == FRAME_TYPE_LEGACY_DATA) && mLegacyCommissioningMatcher.count(rule)) {
		// This is for ensuring that the commissioning TCP connection survives
		// the transition to joining the network. In order for this to occur,
		// we need to ensure that the packets for this particular connection
		// continue to flow to and from the normal IPv6 data interface.
		*type = FRAME_TYPE_DATA;
	}

	if (!packet_should_be_dropped) {
		// Inform the statistic collector about the inbound IP packet
		get_stat_collector().record_inbound_packet(ip_packet);
	} else {
		syslog(LOG_DEBUG, "Dropping host-bound IPv6 packet.");
	}

	return !packet_should_be_dropped;
}

bool
NCPInstanceBase::should_forward_ncpbound_frame(uint8_t* type, const uint8_t* ip_packet, size_t packet_length)
{
	bool should_forward = true;
	IPv6PacketMatcherRule rule;

	if (!ncp_state_is_interface_up(get_ncp_state())) {
		syslog(LOG_DEBUG, "Dropping IPv6 packet, NCP not ready yet!");
		should_forward = false;
		goto bail;
	}

	// Skip non-IPv6 packets
	if (!is_valid_ipv6_packet(ip_packet, packet_length)) {
		syslog(LOG_DEBUG,
			   "Dropping non-IPv6 outbound packet (first byte was 0x%02X)",
			   ip_packet[0]);
		should_forward = false;
		goto bail;
	}

	rule.update_from_outbound_packet(ip_packet);

	if (mDropFirewall.match_outbound(ip_packet) != mDropFirewall.end()) {
		syslog(LOG_INFO, "[->NCP] Dropping matched packet.");
		should_forward = false;
		goto bail;
	}

	if (mLegacyCommissioningMatcher.count(rule)) {
		if (*type == FRAME_TYPE_LEGACY_DATA) {
			// This is for ensuring that the commissioning TCP connection survives
			// the transition to joining the network. In order for this to occur,
			// we need to ensure that the packets for this particular connection
			// continue to flow to and from the normal IPv6 data interface.
			*type = FRAME_TYPE_DATA;
		} else {
			mLegacyCommissioningMatcher.erase(rule);
		}
	}

	rule.subtype = IPv6PacketMatcherRule::SUBTYPE_ALL;

	if (mInsecureFirewall.count(rule)) {
		// We use `count` instead of `match_outbound` in the
		// check above because exact matches are faster.
		syslog(LOG_INFO, "[->NCP] Routing insecure commissioning traffic.");
		*type = FRAME_TYPE_INSECURE_DATA;
	}

	if (ncp_state_is_joining(get_ncp_state())) {
		// When we are joining, all outbound traffic is insecure.
		*type = FRAME_TYPE_INSECURE_DATA;

	} else if ((mCommissioningExpiration != 0)
		&& (mCommissioningExpiration < time_get_monotonic())
	) {
		syslog(LOG_NOTICE, "Commissioning period has ended");
		mCommissioningExpiration = 0;
		mInsecureFirewall.clear();
	}

	// Inform the statistics collector about the outbound IPv6 packet
	if (should_forward) {
		get_stat_collector().record_outbound_packet(ip_packet);
	} else {
		syslog(LOG_DEBUG, "Dropping NCP-bound IPv6 packet.");
	}

	// Debug logging
	dump_outbound_ipv6_packet(
		ip_packet,
		packet_length,
		FRAME_TYPE_TO_CSTR(*type)
	);

bail:

	return should_forward;
}
