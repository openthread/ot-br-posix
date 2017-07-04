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
#include "wpantund.h"
#include "any-to.h"

using namespace nl;
using namespace wpantund;

// ----------------------------------------------------------------------------
// MARK: -
// MARK: Constructors/Destructors

NCPInstanceBase::NCPInstanceBase(const Settings& settings):
	mCommissioningRule(),
	mCommissioningExpiration(0)
{
	std::string wpan_interface_name = "wpan0";

	mResetFD = -1;
	mResetFD_BeginReset = '0';
	mResetFD_EndReset = '1';

	mPowerFD = -1;
	mPowerFD_PowerOff = '0';
	mPowerFD_PowerOn = '1';

	mMCFD = -1;

	NLPT_INIT(&mNCPToDriverPumpPT);
	NLPT_INIT(&mDriverToNCPPumpPT);

	mCommissioningExpiration = 0;
	mEnabled = true;
	mTerminateOnFault = false;
	mAutoUpdateFirmware = false;
	mAutoResume = true;
	mAutoDeepSleep = false;
	mIsInitializingNCP = false;
	mNCPState = UNINITIALIZED;
	mNodeType = UNKNOWN;
	mFailureCount = 0;
	mFailureThreshold = 3;
	mAutoDeepSleepTimeout = 10;
	mCommissionerPort = 5684;

	memset(mNCPMeshLocalAddress.s6_addr, 0, sizeof(mNCPMeshLocalAddress));
	memset(mNCPLinkLocalAddress.s6_addr, 0, sizeof(mNCPLinkLocalAddress));
	memset(mNCPV6LegacyPrefix, 0, sizeof(mNCPV6LegacyPrefix));
	memset(mMACAddress, 0, sizeof(mMACAddress));
	memset(mMACHardwareAddress, 0, sizeof(mMACHardwareAddress));

	if (!settings.empty()) {
		Settings::const_iterator iter;

		for(iter = settings.begin(); iter != settings.end(); iter++) {
			if (strcaseequal(iter->first.c_str(), kWPANTUNDProperty_ConfigNCPHardResetPath)) {
				mResetFD = open_super_socket(iter->second.c_str());

			} else if (strcaseequal(iter->first.c_str(), kWPANTUNDProperty_ConfigNCPPowerPath)) {
				mPowerFD = open_super_socket(iter->second.c_str());

			} else if (strcaseequal(iter->first.c_str(), kWPANTUNDProperty_ConfigNCPSocketPath)) {
				mRawSerialAdapter = SuperSocket::create(iter->second);

			} else if (strcaseequal(iter->first.c_str(), kWPANTUNDProperty_ConfigTUNInterfaceName)) {
				wpan_interface_name = iter->second;

			} else if (strcaseequal(iter->first.c_str(), kWPANTUNDProperty_ConfigNCPFirmwareCheckCommand)) {
				mFirmwareUpgrade.set_firmware_check_command(iter->second);

			} else if (strcaseequal(iter->first.c_str(), kWPANTUNDProperty_ConfigNCPFirmwareUpgradeCommand)) {
				mFirmwareUpgrade.set_firmware_upgrade_command(iter->second);

			} else if (strcaseequal(iter->first.c_str(), kWPANTUNDProperty_DaemonAutoFirmwareUpdate)) {
				mAutoUpdateFirmware = any_to_bool(boost::any(iter->second));

			} else if (strcaseequal(iter->first.c_str(), kWPANTUNDProperty_ConfigDaemonNetworkRetainCommand)) {
				mNetworkRetain.set_network_retain_command(iter->second);
			}
		}
	}

	if (!mRawSerialAdapter) {
		syslog(LOG_WARNING, kWPANTUNDProperty_ConfigNCPSocketPath" was not specified. Using \"/dev/null\" instead.");
		mRawSerialAdapter = SuperSocket::create("/dev/null");
	}

	if (mRawSerialAdapter) {
		mRawSerialAdapter->set_log_level(LOG_DEBUG);
	}

	mSerialAdapter = mRawSerialAdapter;

	mPrimaryInterface = boost::shared_ptr<TunnelIPv6Interface>(new TunnelIPv6Interface(wpan_interface_name));
	mPrimaryInterface->mAddressWasAdded.connect(boost::bind(&NCPInstanceBase::address_was_added, this, _1, _2));
	mPrimaryInterface->mAddressWasRemoved.connect(boost::bind(&NCPInstanceBase::address_was_removed, this, _1, _2));
	mPrimaryInterface->mLinkStateChanged.connect(boost::bind(&NCPInstanceBase::link_state_changed, this, _1, _2));

	set_ncp_power(true);

	// Go ahead and start listening on ff03::1
	join_multicast_group("ff03::1");

	{
		IPv6PacketMatcherRule rule;

		// --------------------------------------------------------------------
		// Packet Drop rules

		rule.clear();
		// OS X seems to generate these packets when bringing up the interface.
		// Honey badger don't care.
		rule.type = IPv6PacketMatcherRule::TYPE_HOP_BY_HOP;
		rule.remote_address.s6_addr[0x0] = 0xFF;
		rule.remote_address.s6_addr[0x1] = 0x02;
		rule.remote_address.s6_addr[0xF] = 0x16;
		rule.remote_match_mask = 128;
		mDropFirewall.insert(rule);

		rule.clear();
		// Don't forward router advertisement or router solicitation
		// traffic.
		rule.type = IPv6PacketMatcherRule::TYPE_ICMP;
		rule.remote_address.s6_addr[0x0] = 0xFF;
		rule.remote_address.s6_addr[0x1] = 0x02;
		rule.remote_address.s6_addr[0xF] = 0x02;
		rule.remote_match_mask = 128;
		rule.subtype = IPv6PacketMatcherRule::SUBTYPE_ICMP_ROUTER_ADV;
		mDropFirewall.insert(rule);
		rule.subtype = IPv6PacketMatcherRule::SUBTYPE_ICMP_ROUTER_SOL;
		mDropFirewall.insert(rule);

		rule.clear();
		// Don't forward neighbor advertisement or neighbor solicitation
		// or redirect traffic.
		rule.type = IPv6PacketMatcherRule::TYPE_ICMP;
		rule.subtype = IPv6PacketMatcherRule::SUBTYPE_ICMP_NEIGHBOR_ADV;
		mDropFirewall.insert(rule);
		rule.subtype = IPv6PacketMatcherRule::SUBTYPE_ICMP_NEIGHBOR_SOL;
		mDropFirewall.insert(rule);
		rule.subtype = IPv6PacketMatcherRule::SUBTYPE_ICMP_REDIRECT;
		mDropFirewall.insert(rule);
	}
}

bool
NCPInstanceBase::setup_property_supported_by_class(const std::string& prop_name)
{
	return strcaseequal(prop_name.c_str(), kWPANTUNDProperty_ConfigNCPHardResetPath)
		|| strcaseequal(prop_name.c_str(), kWPANTUNDProperty_ConfigNCPPowerPath)
		|| strcaseequal(prop_name.c_str(), kWPANTUNDProperty_ConfigNCPSocketPath)
		|| strcaseequal(prop_name.c_str(), kWPANTUNDProperty_ConfigTUNInterfaceName)
		|| strcaseequal(prop_name.c_str(), kWPANTUNDProperty_ConfigNCPDriverName)
		|| strcaseequal(prop_name.c_str(), kWPANTUNDProperty_ConfigNCPFirmwareCheckCommand)
		|| strcaseequal(prop_name.c_str(), kWPANTUNDProperty_DaemonAutoFirmwareUpdate)
		|| strcaseequal(prop_name.c_str(), kWPANTUNDProperty_ConfigNCPFirmwareUpgradeCommand)
		|| strcaseequal(prop_name.c_str(), kWPANTUNDProperty_ConfigDaemonNetworkRetainCommand);
}

NCPInstanceBase::~NCPInstanceBase()
{
	close(mMCFD);
	close(mPowerFD);
	close(mResetFD);
}

const std::string &
NCPInstanceBase::get_name(void)
{
	return mPrimaryInterface->get_interface_name();
}

const WPAN::NetworkInstance&
NCPInstanceBase::get_current_network_instance(void) const
{
	return mCurrentNetworkInstance;
}

// Helpful for use with callbacks.
int
NCPInstanceBase::process_event_helper(int event)
{
	return EventHandler::process_event(event);
}

// ----------------------------------------------------------------------------
// MARK: -

wpantund_status_t
NCPInstanceBase::set_ncp_version_string(const std::string& version_string)
{
	wpantund_status_t status = kWPANTUNDStatus_Ok;

	if (version_string != mNCPVersionString) {
		if (!mNCPVersionString.empty()) {
			// The previous version string isn't empty!
			syslog(LOG_ERR, "Illegal NCP version change! (Previously \"%s\")", mNCPVersionString.c_str());
			ncp_is_misbehaving();
			status = kWPANTUNDStatus_InvalidArgument;
		} else {
			mNCPVersionString = version_string;

			syslog(LOG_NOTICE, "NCP is running \"%s\"", mNCPVersionString.c_str());
			syslog(LOG_NOTICE, "Driver is running \"%s\"", nl::wpantund::get_wpantund_version_string().c_str());

			if (mAutoUpdateFirmware && is_firmware_upgrade_required(version_string)) {
				syslog(LOG_NOTICE, "NCP FIRMWARE UPGRADE IS REQUIRED");
				upgrade_firmware();
			}
		}
	}
	return status;
}

std::set<std::string>
NCPInstanceBase::get_supported_property_keys(void) const
{
	std::set<std::string> properties;

	properties.insert(kWPANTUNDProperty_DaemonEnabled);
	properties.insert(kWPANTUNDProperty_NetworkIsCommissioned);
	properties.insert(kWPANTUNDProperty_InterfaceUp);
	properties.insert(kWPANTUNDProperty_NetworkName);
	properties.insert(kWPANTUNDProperty_NetworkPANID);
	properties.insert(kWPANTUNDProperty_NetworkXPANID);
	properties.insert(kWPANTUNDProperty_NetworkKey);
	properties.insert(kWPANTUNDProperty_NetworkPSKc);
	properties.insert(kWPANTUNDProperty_NetworkKeyIndex);
	properties.insert(kWPANTUNDProperty_NetworkNodeType);
	properties.insert(kWPANTUNDProperty_NCPState);
	properties.insert(kWPANTUNDProperty_NCPChannel);
	properties.insert(kWPANTUNDProperty_NCPTXPower);

	properties.insert(kWPANTUNDProperty_IPv6MeshLocalPrefix);
	properties.insert(kWPANTUNDProperty_IPv6MeshLocalAddress);
	properties.insert(kWPANTUNDProperty_IPv6LinkLocalAddress);
	properties.insert(kWPANTUNDProperty_IPv6AllAddresses);

	properties.insert(kWPANTUNDProperty_ThreadOnMeshPrefixes);

	properties.insert(kWPANTUNDProperty_DaemonAutoAssociateAfterReset);
	properties.insert(kWPANTUNDProperty_DaemonAutoDeepSleep);
	properties.insert(kWPANTUNDProperty_DaemonReadyForHostSleep);
	properties.insert(kWPANTUNDProperty_DaemonTerminateOnFault);

	properties.insert(kWPANTUNDProperty_NestLabs_NetworkAllowingJoin);

	properties.insert(kWPANTUNDProperty_DaemonVersion);
	properties.insert(kWPANTUNDProperty_DaemonTerminateOnFault);

	properties.insert(kWPANTUNDProperty_NCPVersion);
	properties.insert(kWPANTUNDProperty_NCPHardwareAddress);
	properties.insert(kWPANTUNDProperty_NCPCCAThreshold);

	properties.insert(kWPANTUNDProperty_NCPMACAddress);


	properties.insert(kWPANTUNDProperty_ConfigTUNInterfaceName);

	if (mLegacyInterfaceEnabled
		|| mNodeTypeSupportsLegacy
		|| buffer_is_nonzero(mNCPV6LegacyPrefix, sizeof(mNCPV6LegacyPrefix))
	) {
		properties.insert(kWPANTUNDProperty_NestLabs_LegacyMeshLocalAddress);
		properties.insert(kWPANTUNDProperty_NestLabs_LegacyMeshLocalPrefix);
	}

	properties.insert(kWPANTUNDProperty_NestLabs_NetworkPassthruPort);

	return properties;
}

void
NCPInstanceBase::property_get_value(
	const std::string& in_key,
	CallbackWithStatusArg1 cb
) {
	std::string key = in_key;

	if (key.empty()) {
		/* This key is used to get the list of available properties */
		cb(0, get_supported_property_keys());

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_ConfigTUNInterfaceName)) {
		cb(0, get_name());

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_DaemonEnabled)) {
		cb(0, boost::any(mEnabled));

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_InterfaceUp)) {
		cb(0, boost::any(mPrimaryInterface->is_online()));

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_DaemonReadyForHostSleep)) {
		cb(0, boost::any(!is_busy()));

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_ConfigTUNInterfaceName)) {
		cb(0, get_name());

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NCPVersion)) {
		cb(0, boost::any(mNCPVersionString));

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NetworkName)) {
		cb(0, boost::any(get_current_network_instance().name));

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NetworkIsCommissioned)) {
		NCPState ncp_state = get_ncp_state();
		if (ncp_state_is_commissioned(ncp_state)) {
			cb(0, boost::any(true));
		} else  if (ncp_state == OFFLINE || ncp_state == DEEP_SLEEP) {
			cb(0, boost::any(false));
		} else {
			cb(kWPANTUNDStatus_TryAgainLater,
			   boost::any(std::string("Unable to determine association state at this time"))
			);
		}

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NestLabs_LegacyEnabled)) {
		cb(0, boost::any(mLegacyInterfaceEnabled));

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NestLabs_NetworkAllowingJoin)) {
		cb(0, boost::any(get_current_network_instance().joinable));

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NetworkPANID)) {
		cb(0, boost::any(get_current_network_instance().panid));

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NetworkXPANID)) {
		cb(0, boost::any(get_current_network_instance().get_xpanid_as_uint64()));

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NCPChannel)) {
		cb(0, boost::any((int)get_current_network_instance().channel));

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_DaemonVersion)) {
		cb(0, boost::any(nl::wpantund::get_wpantund_version_string()));

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_DaemonAutoAssociateAfterReset)) {
		cb(0, boost::any(static_cast<bool>(mAutoResume)));

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_DaemonAutoDeepSleep)) {
		cb(0, boost::any(mAutoDeepSleep));

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_DaemonAutoFirmwareUpdate)) {
		cb(0, boost::any(mAutoUpdateFirmware));

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_DaemonTerminateOnFault)) {
		cb(0, boost::any(mTerminateOnFault));

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NestLabs_NetworkPassthruPort)) {
		cb(0, boost::any(mCommissionerPort));

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NCPMACAddress)) {
		cb(0, boost::any(nl::Data(mMACAddress, sizeof(mMACAddress))));

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NCPHardwareAddress)) {
		cb(0, boost::any(nl::Data(mMACHardwareAddress, sizeof(mMACHardwareAddress))));

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_IPv6MeshLocalPrefix)) {
		if (buffer_is_nonzero(mNCPV6Prefix, sizeof(mNCPV6Prefix))) {
			struct in6_addr addr (mNCPMeshLocalAddress);
			// Zero out the lower 64 bits.
			memset(addr.s6_addr+8, 0, 8);
			cb(0, boost::any(in6_addr_to_string(addr)+"/64"));
		} else {
			cb(kWPANTUNDStatus_FeatureNotSupported, std::string("Property is unavailable"));
		}
	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_IPv6MeshLocalAddress)) {
		if (buffer_is_nonzero(mNCPMeshLocalAddress.s6_addr, sizeof(mNCPMeshLocalAddress))) {
			cb(0, boost::any(in6_addr_to_string(mNCPMeshLocalAddress)));
		} else {
			cb(kWPANTUNDStatus_FeatureNotSupported, std::string("Property is unavailable"));
		}
	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_IPv6LinkLocalAddress)) {
		if (buffer_is_nonzero(mNCPLinkLocalAddress.s6_addr, sizeof(mNCPLinkLocalAddress))) {
			cb(0, boost::any(in6_addr_to_string(mNCPLinkLocalAddress)));
		} else {
			cb(kWPANTUNDStatus_FeatureNotSupported, std::string("Property is unavailable"));
		}

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NestLabs_LegacyMeshLocalPrefix)) {
		if (mLegacyInterfaceEnabled
			|| mNodeTypeSupportsLegacy
			|| buffer_is_nonzero(mNCPV6LegacyPrefix, sizeof(mNCPV6LegacyPrefix))
		) {
			cb(0, boost::any(nl::Data(mNCPV6LegacyPrefix, sizeof(mNCPV6LegacyPrefix))));
		} else {
			cb(kWPANTUNDStatus_FeatureNotSupported, std::string("Property is unavailable"));
		}

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NestLabs_LegacyMeshLocalAddress)) {
		struct in6_addr legacy_addr;

		if ( (mLegacyInterfaceEnabled || mNodeTypeSupportsLegacy)
		  && buffer_is_nonzero(mNCPV6LegacyPrefix, sizeof(mNCPV6LegacyPrefix))
		) {
			legacy_addr = make_slaac_addr_from_eui64(mNCPV6LegacyPrefix, mMACAddress);
			cb(0, boost::any(in6_addr_to_string(legacy_addr)));
		} else {
			cb(kWPANTUNDStatus_FeatureNotSupported, std::string("Property is unavailable"));
		}

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NCPState)) {
		if ( is_initializing_ncp()
		  && !ncp_state_is_detached_from_ncp(get_ncp_state())
		) {
			cb(0, boost::any(std::string(kWPANTUNDStateUninitialized)));
		} else {
			cb(0, boost::any(ncp_state_to_string(get_ncp_state())));
		}

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NetworkNodeType)) {
		cb(0, boost::any(node_type_to_string(mNodeType)));

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_ThreadOnMeshPrefixes)) {
		std::list<std::string> result;
		std::map<struct in6_addr, GlobalAddressEntry>::const_iterator it;
		static const char flag_lookup[] = "ppPSDCRM";
		char address_string[INET6_ADDRSTRLEN];

		for ( it = mOnMeshPrefixes.begin();
			  it != mOnMeshPrefixes.end();
			  it++ ) {
			inet_ntop(AF_INET6,	&it->first,	address_string, sizeof(address_string));
			result.push_back(std::string(address_string) + "  " + flags_to_string(it->second.mFlags, flag_lookup).c_str());
		}
		cb(0, boost::any(result));

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_IPv6AllAddresses)
		|| strcaseequal(key.c_str(), kWPANTUNDProperty_DebugIPv6GlobalIPAddressList)
	) {
		std::list<std::string> result;
		std::map<struct in6_addr, GlobalAddressEntry>::const_iterator it;
		char address_string[INET6_ADDRSTRLEN];
		for ( it = mGlobalAddresses.begin();
			  it != mGlobalAddresses.end();
			  it++ ) {
			inet_ntop(AF_INET6,	&it->first,	address_string, sizeof(address_string));
			result.push_back(std::string(address_string)+ "  " + it->second.get_description());
		}
		cb(0, boost::any(result));

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_DaemonSyslogMask)) {
		std::string mask_string;
		int logmask;

		setlogmask(logmask = setlogmask(0));

		if (LOG_FAC(logmask) == LOG_DAEMON) {
			mask_string += "daemon ";
		}
		if (LOG_FAC(logmask) == LOG_USER) {
			mask_string += "user ";
		}
		if (logmask & LOG_MASK(LOG_EMERG)) {
			mask_string += "emerg ";
		}
		if (logmask & LOG_MASK(LOG_ALERT)) {
			mask_string += "alert ";
		}
		if (logmask & LOG_MASK(LOG_CRIT)) {
			mask_string += "crit ";
		}
		if (logmask & LOG_MASK(LOG_ERR)) {
			mask_string += "err ";
		}
		if (logmask & LOG_MASK(LOG_WARNING)) {
			mask_string += "warning ";
		}
		if (logmask & LOG_MASK(LOG_NOTICE)) {
			mask_string += "notice ";
		}
		if (logmask & LOG_MASK(LOG_INFO)) {
			mask_string += "info ";
		}
		if (logmask & LOG_MASK(LOG_DEBUG)) {
			mask_string += "debug ";
		}

		cb(0, mask_string);

	} else if (StatCollector::is_a_stat_property(key)) {
		get_stat_collector().property_get_value(key, cb);

	} else {
		syslog(LOG_ERR, "property_get_value: Unsupported property \"%s\"", key.c_str());
		cb(kWPANTUNDStatus_PropertyNotFound, boost::any(std::string("Property Not Found")));
	}
}

void
NCPInstanceBase::property_set_value(
	const std::string& key,
	const boost::any& value,
	CallbackWithStatus cb
) {
	// If we are disabled, then the only property we
	// are allowed to set is kWPANTUNDProperty_DaemonEnabled.
	if (!mEnabled && !strcaseequal(key.c_str(), kWPANTUNDProperty_DaemonEnabled)) {
		cb(kWPANTUNDStatus_InvalidWhenDisabled);
		return;
	}

	try {
		if (strcaseequal(key.c_str(), kWPANTUNDProperty_DaemonEnabled)) {
			mEnabled = any_to_bool(value);
			cb(0);

		} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_InterfaceUp)) {
			bool isup = any_to_bool(value);
			if (isup != mPrimaryInterface->is_online()) {
				if (isup) {
					get_control_interface().attach(cb);
				} else {
					if (ncp_state_is_joining_or_joined(get_ncp_state())) {
						// This isn't quite what we want, but the subclass
						// should be overriding this anyway.
						get_control_interface().reset();
					}
				}
			} else {
				cb(0);
			}

		} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_DaemonAutoAssociateAfterReset)) {
			mAutoResume = any_to_bool(value);
			cb(0);

		} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NestLabs_NetworkPassthruPort)) {
			mCommissionerPort = static_cast<uint16_t>(any_to_int(value));
			cb(0);

		} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_DaemonAutoFirmwareUpdate)) {
			bool value_bool = any_to_bool(value);

			if (value_bool && !mAutoUpdateFirmware) {
				if (get_ncp_state() == FAULT) {
					syslog(LOG_ALERT, "The NCP is misbehaving: Attempting a firmware update");
					upgrade_firmware();
				} else if (get_ncp_state() != UNINITIALIZED) {
					if (is_firmware_upgrade_required(mNCPVersionString)) {
						syslog(LOG_NOTICE, "NCP FIRMWARE UPGRADE IS REQUIRED");
						upgrade_firmware();
					}
				}
			}

			mAutoUpdateFirmware = value_bool;

			cb(0);

		} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_DaemonTerminateOnFault)) {
			mTerminateOnFault = any_to_bool(value);
			cb(0);
			if (mTerminateOnFault && (get_ncp_state() == FAULT)) {
				reinitialize_ncp();
			}

		} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_IPv6MeshLocalPrefix)
			|| strcaseequal(key.c_str(), kWPANTUNDProperty_IPv6MeshLocalAddress)
		) {
			if (get_ncp_state() <= OFFLINE) {
				nl::Data prefix;

				if (value.type() == typeid(std::string)) {
					uint8_t ula_bytes[16] = {};
					const std::string ip_string(any_to_string(value));

					// Address-style
					int bits = inet_pton(AF_INET6,ip_string.c_str(),ula_bytes);
					if (bits <= 0) {
						// Prefix is the wrong length.
						cb(kWPANTUNDStatus_InvalidArgument);
						return;
					}

					prefix = nl::Data(ula_bytes, 8);
				} else {
					prefix = any_to_data(value);
				}

				if (prefix.size() < sizeof(mNCPV6Prefix)) {
					// Prefix is the wrong length.
					cb(kWPANTUNDStatus_InvalidArgument);
				}
				memcpy(mNCPV6Prefix, prefix.data(), sizeof(mNCPV6Prefix));
				cb(0);
			} else {
				cb(kWPANTUNDStatus_InvalidForCurrentState);
			}

		} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_DaemonAutoDeepSleep)) {
			mAutoDeepSleep = any_to_bool(value);

			if (mAutoDeepSleep == false
				&& mNCPState == DEEP_SLEEP
				&& mEnabled
			) {
				// Wake us up if we are asleep and deep sleep was turned off.
				get_control_interface().refresh_state(boost::bind(cb,0));
			} else {
				cb(0);
			}

		} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_DaemonSyslogMask)) {
			setlogmask(strtologmask(any_to_string(value).c_str(), setlogmask(0)));
			cb(0);

		} else if (StatCollector::is_a_stat_property(key)) {
			get_stat_collector().property_set_value(key, value, cb);

		} else {
			syslog(LOG_ERR, "property_set_value: Unsupported property \"%s\"", key.c_str());
			cb(kWPANTUNDStatus_PropertyNotFound);
		}

	} catch (const boost::bad_any_cast &x) {
		// We will get a bad_any_cast exception if the property is of
		// the wrong type.
		syslog(LOG_ERR, "property_set_value: Bad type for property \"%s\" (%s)", key.c_str(), x.what());
		cb(kWPANTUNDStatus_InvalidArgument);

	} catch (const std::invalid_argument &x) {
		// We will get a bad_any_cast exception if the property is of
		// the wrong type.
		syslog(LOG_ERR, "property_set_value: Invalid argument for property \"%s\" (%s)", key.c_str(), x.what());
		cb(kWPANTUNDStatus_InvalidArgument);
	}
}

void
NCPInstanceBase::property_insert_value(
	const std::string& key,
	const boost::any& value,
	CallbackWithStatus cb
) {
	syslog(LOG_ERR, "property_insert_value: Property not supported or not insert-value capable \"%s\"", key.c_str());
	cb(kWPANTUNDStatus_PropertyNotFound);
}

void
NCPInstanceBase::property_remove_value(
	const std::string& key,
	const boost::any& value,
	CallbackWithStatus cb
) {
	syslog(LOG_ERR, "property_remove_value: Property not supported or not remove-value capable \"%s\"", key.c_str());
	cb(kWPANTUNDStatus_PropertyNotFound);
}

void
NCPInstanceBase::signal_property_changed(
	const std::string& key,
	const boost::any& value
) {
	get_control_interface().mOnPropertyChanged(key, value);
}

// ----------------------------------------------------------------------------
// MARK: -

void
NCPInstanceBase::set_initializing_ncp(bool x)
{
	if (mIsInitializingNCP != x) {
		mIsInitializingNCP = x;

		if (mIsInitializingNCP) {
			change_ncp_state(UNINITIALIZED);
			set_ncp_power(true);
		} else if ( (get_ncp_state() != UNINITIALIZED)
				 && (get_ncp_state() != FAULT)
				 && (get_ncp_state() != UPGRADING)
		) {
			handle_ncp_state_change(get_ncp_state(), UNINITIALIZED);
		}
	}
}

bool
NCPInstanceBase::is_initializing_ncp(void) const
{
	return mIsInitializingNCP;
}

NCPState
NCPInstanceBase::get_ncp_state()const
{
	return mNCPState;
}

bool
NCPInstanceBase::is_state_change_valid(NCPState new_ncp_state) const
{
	// Add any invalid state transitions here so that
	// bugs can be more quickly identified and corrected.

	if (ncp_state_is_detached_from_ncp(get_ncp_state())) {
		return new_ncp_state == UNINITIALIZED;
	}

	return true;
}

void
NCPInstanceBase::change_ncp_state(NCPState new_ncp_state)
{
	NCPState old_ncp_state = mNCPState;

	if (old_ncp_state != new_ncp_state) {
		if (!is_state_change_valid(new_ncp_state)) {
			syslog(
				LOG_WARNING,
				"BUG: Invalid state change: \"%s\" -> \"%s\"",
				ncp_state_to_string(old_ncp_state).c_str(),
				ncp_state_to_string(new_ncp_state).c_str()
			);

			if (ncp_state_is_detached_from_ncp(get_ncp_state())) {
				// If the state was detached, do not allow the
				// state change to continue.
				return;
			}
		} else {
			syslog(
				LOG_NOTICE,
				"State change: \"%s\" -> \"%s\"",
				ncp_state_to_string(old_ncp_state).c_str(),
				ncp_state_to_string(new_ncp_state).c_str()
			);
		}

		mNCPState = new_ncp_state;

		if ( !mIsInitializingNCP
		  || (new_ncp_state == UNINITIALIZED)
		  || (new_ncp_state == FAULT)
		  || (new_ncp_state == UPGRADING)
		) {
			handle_ncp_state_change(new_ncp_state, old_ncp_state);
		}
	}
}

void
NCPInstanceBase::handle_ncp_state_change(NCPState new_ncp_state, NCPState old_ncp_state)
{
	// Detached NCP -> Online NCP
	if (ncp_state_is_detached_from_ncp(old_ncp_state)
	 && !ncp_state_is_detached_from_ncp(new_ncp_state)
	) {
		__ASSERT_MACROS_check(new_ncp_state == UNINITIALIZED);

		// We are transitioning out of a state where we are disconnected
		// from the NCP. This requires a hard reset.
		set_ncp_power(true);

		if (mResetFD >= 0) {
			// If we have a way to hard reset the NCP,
			// then do it. We do the check above to make
			// sure that we don't end up calling mSerialAdapter->reset()
			// twice.
			hard_reset_ncp();
		}

		mSerialAdapter->reset();

		PT_INIT(&mControlPT);
	}

	// Online NCP -> Detached NCP
	else if (!ncp_state_is_detached_from_ncp(old_ncp_state)
	 && ncp_state_is_detached_from_ncp(new_ncp_state)
	) {
		// We are transitioning into a state where we need to be disconnected
		// from the NCP. For this we use the hibernate command.
		mSerialAdapter->hibernate();
		PT_INIT(&mControlPT);
		NLPT_INIT(&mDriverToNCPPumpPT);
		NLPT_INIT(&mNCPToDriverPumpPT);
		mFailureCount = 0;

		if (new_ncp_state == FAULT) {
			// When we enter the fault state, attempt to
			// ensure that we are using as little power as
			// possible by physically turning off the NCP
			// (if a method of doing so has been specified
			// in our configuration)
			set_ncp_power(false);

			if (mTerminateOnFault) {
				signal_fatal_error(kWPANTUNDStatus_Failure);
			}
		}
		return;
	}

	// Interface Down -> Interface Up
	if (!ncp_state_is_interface_up(old_ncp_state)
	 && ncp_state_is_interface_up(new_ncp_state)
	) {
		set_online(true);


	// InterfaceUp -> COMMISSIONED
	// (Special case of InterfaceUp -> InterfaceDown)
	} else if (ncp_state_is_interface_up(old_ncp_state)
				&& (new_ncp_state == COMMISSIONED)
				&& mAutoResume
	) {
		// We don't bother going further if autoresume is on.
		return;


	// Commissioned -> InterfaceDown
	// (Special case of InterfaceUp -> InterfaceDown)
	} else if (ncp_state_is_commissioned(old_ncp_state)
		&& !ncp_state_is_commissioned(new_ncp_state)
		&& !ncp_state_is_sleeping(new_ncp_state)
		&& (new_ncp_state != UNINITIALIZED)
	) {
		reset_interface();


	// Uninitialized -> Offline
	// If we are transitioning from uninitialized to offline,
	// and we have global addresses, then need to clear them out.
	} else if (old_ncp_state == UNINITIALIZED
		&& new_ncp_state == OFFLINE
		&& !mGlobalAddresses.empty()
	) {
		reset_interface();


	// InterfaceUp -> InterfaceDown (General Case)
	} else if (ncp_state_is_interface_up(old_ncp_state)
		&& !ncp_state_is_interface_up(new_ncp_state)
		&& new_ncp_state != NET_WAKE_WAKING
	) {
		// Take the interface offline.
		syslog(LOG_NOTICE, "Taking interface(s) down. . .");

		mCurrentNetworkInstance.joinable = false;
		set_commissioniner(0, 0, 0);
		set_online(false);
	}

	// We don't announce transitions to the "UNITIALIZED" state.
	if (UNINITIALIZED != new_ncp_state) {
		signal_property_changed(kWPANTUNDProperty_NCPState, ncp_state_to_string(new_ncp_state));
	}

	mNetworkRetain.handle_ncp_state_change(new_ncp_state, old_ncp_state);
}

void
NCPInstanceBase::reinitialize_ncp(void)
{
	PT_INIT(&mControlPT);
	change_ncp_state(UNINITIALIZED);
}

void
NCPInstanceBase::reset_tasks(wpantund_status_t status)
{
}

// ----------------------------------------------------------------------------
// MARK: -

bool
NCPInstanceBase::is_busy(void)
{
	const NCPState ncp_state = get_ncp_state();
	bool is_busy = ncp_state_is_busy(ncp_state);

	if (is_initializing_ncp()) {
		return true;
	}

	if (ncp_state == FAULT) {
		return false;
	}

	if (get_upgrade_status() == EINPROGRESS) {
		is_busy = true;
	}

	return is_busy;
}

StatCollector&
NCPInstanceBase::get_stat_collector(void)
{
	return mStatCollector;
}

void
NCPInstanceBase::update_busy_indication(void)
{
	cms_t current_time = time_ms();

	if (mWasBusy != is_busy()) {
		if (!mWasBusy
			|| (mLastChangedBusy == 0)
			|| (current_time - mLastChangedBusy >= BUSY_DEBOUNCE_TIME_IN_MS)
			|| (current_time - mLastChangedBusy < 0)
		) {
			mWasBusy = !mWasBusy;
			if(!mWasBusy) {
				if (mLastChangedBusy == 0) {
					syslog(LOG_INFO, "NCP is no longer busy, host sleep is permitted.");
				} else {
					syslog(LOG_INFO, "NCP is no longer busy, host sleep is permitted. (Was busy for %dms)",(int)(current_time - mLastChangedBusy));
				}
				signal_property_changed(kWPANTUNDProperty_DaemonReadyForHostSleep, true);
			} else {
				syslog(LOG_INFO, "NCP is now BUSY.");
				signal_property_changed(kWPANTUNDProperty_DaemonReadyForHostSleep, false);
			}
			mLastChangedBusy = current_time;
		}
	} else if (mWasBusy
		&& (mLastChangedBusy != 0)
		&& (current_time - mLastChangedBusy > MAX_INSOMNIA_TIME_IN_MS)
	) {
		syslog(LOG_ERR, "Experiencing extended insomnia. Resetting internal state.");

		mLastChangedBusy = current_time;

		ncp_is_misbehaving();
	}
}

void
NCPInstanceBase::ncp_is_misbehaving(void)
{
	mFailureCount++;
	hard_reset_ncp();
	reset_tasks();
	reinitialize_ncp();

	if (mFailureCount >= mFailureThreshold) {
		change_ncp_state(FAULT);
	}
}

// ----------------------------------------------------------------------------
// MARK: -

bool
NCPInstanceBase::is_firmware_upgrade_required(const std::string& version)
{
	return mFirmwareUpgrade.is_firmware_upgrade_required(version);
}

void
NCPInstanceBase::upgrade_firmware(void)
{
	change_ncp_state(UPGRADING);

	set_ncp_power(true);

	return mFirmwareUpgrade.upgrade_firmware();
}

int
NCPInstanceBase::get_upgrade_status(void)
{
	return mFirmwareUpgrade.get_upgrade_status();
}

bool
NCPInstanceBase::can_upgrade_firmware(void)
{
	return mFirmwareUpgrade.can_upgrade_firmware();
}
