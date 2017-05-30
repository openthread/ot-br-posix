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

#ifndef __wpantund__NCPInstanceBase__
#define __wpantund__NCPInstanceBase__

#include "NCPInstance.h"
#include <set>
#include <map>
#include <string>
#include "FirmwareUpgrade.h"
#include "EventHandler.h"
#include "NCPTypes.h"
#include "StatCollector.h"
#include "NetworkRetain.h"
#include "RunawayResetBackoffManager.h"
#include "Pcap.h"

namespace nl {
namespace wpantund {

class NCPInstanceBase : public NCPInstance, public EventHandler{
public:

	enum {
		FRAME_TYPE_DATA = 2,
		FRAME_TYPE_INSECURE_DATA = 3,
		FRAME_TYPE_LEGACY_DATA = 4
	};

protected:
	NCPInstanceBase(const Settings& settings = Settings());

public:
	virtual ~NCPInstanceBase();

	virtual const std::string &get_name();

	virtual void set_socket_adapter(const boost::shared_ptr<SocketAdapter> &adapter);

public:
	// ========================================================================
	// Static Functions

	static bool setup_property_supported_by_class(const std::string& prop_name);


public:
	// ========================================================================
	// MARK: ASync I/O

	virtual cms_t get_ms_to_next_event(void);

	virtual int update_fd_set(
		fd_set *read_fd_set,
		fd_set *write_fd_set,
		fd_set *error_fd_set,
		int *max_fd,
		cms_t *timeout
	);

	virtual void process(void);

	// Helpful for use with callbacks.
	int process_event_helper(int event);

	virtual StatCollector& get_stat_collector(void);

protected:
	virtual char ncp_to_driver_pump() = 0;
	virtual char driver_to_ncp_pump() = 0;

public:
	// ========================================================================
	// MARK: NCP Behavior

	virtual void hard_reset_ncp(void);

	virtual int set_ncp_power(bool power);

	virtual bool can_set_ncp_power(void);

public:
	// ========================================================================
	// MARK: Other

	virtual void reinitialize_ncp(void);

	virtual void reset_tasks(wpantund_status_t status = kWPANTUNDStatus_Canceled);

	NCPState get_ncp_state()const;

	bool is_state_change_valid(NCPState new_ncp_state)const;

	//! Handles transitioning from state-to-state.
	/*! This is the ONLY WAY to change mNCPState. */
	void change_ncp_state(NCPState new_ncp_state);

	virtual void handle_ncp_state_change(NCPState new_ncp_state, NCPState old_ncp_state);

	virtual void ncp_is_misbehaving();

	virtual void set_initializing_ncp(bool x);

	virtual bool is_initializing_ncp()const;

public:
	// ========================================================================
	// MARK: Network Interface Methods

	int set_online(bool x);

	void set_mac_address(const uint8_t addr[8]);

	void set_mac_hardware_address(const uint8_t addr[8]);

	void reset_interface(void);

	const WPAN::NetworkInstance& get_current_network_instance(void)const;

public:
	// ========================================================================
	// MARK: Global Address Management


	void add_address(const struct in6_addr &address, uint8_t prefix = 64, uint32_t valid_lifetime = UINT32_MAX, uint32_t preferred_lifetime = UINT32_MAX);
	void remove_address(const struct in6_addr &address);

	void refresh_global_addresses();

	//! Removes all nonpermanent global address entries
	void clear_nonpermanent_global_addresses();

	void restore_global_addresses();

	bool is_address_known(const struct in6_addr &address);

	bool lookup_address_for_prefix(struct in6_addr *address, const struct in6_addr &prefix, int prefix_len_in_bits = 64);

	int join_multicast_group(const std::string &group_name);

public:
	// ========================================================================
	// MARK: Subclass Hooks

	virtual void address_was_added(const struct in6_addr& addr, int prefix_len);

	virtual void address_was_removed(const struct in6_addr& addr, int prefix_len);

	virtual void link_state_changed(bool is_up, bool is_running);

	virtual void legacy_link_state_changed(bool is_up, bool is_running);

public:
	// ========================================================================
	// MARK: Firmware Upgrade

	virtual bool is_firmware_upgrade_required(const std::string& version);

	virtual void upgrade_firmware(void);

	virtual int get_upgrade_status(void);

	virtual bool can_upgrade_firmware(void);

public:
	// ========================================================================
	// MARK: Busy/OkToSleep

	virtual bool is_busy(void);

	virtual void update_busy_indication(void);

public:
	// ========================================================================
	// MARK: IPv6 data path helpers

	bool should_forward_hostbound_frame(uint8_t* type, const uint8_t* packet, size_t packet_length);

	bool should_forward_ncpbound_frame(uint8_t* type, const uint8_t* packet, size_t packet_length);

	void handle_normal_ipv6_from_ncp(const uint8_t* packet, size_t packet_length);

	int set_commissioniner(int seconds, uint8_t traffic_type, in_port_t traffic_port);

public:
	// ========================================================================
	// MARK: Legacy Interface Methods

	void enable_legacy_interface(void);

	bool is_legacy_interface_enabled(void);

	void handle_alt_ipv6_from_ncp(const uint8_t* packet, size_t packet_length);

public:

	virtual std::set<std::string> get_supported_property_keys()const;

	virtual void get_property(
	    const std::string& key,
	    CallbackWithStatusArg1 cb
	);

	virtual void set_property(
	    const std::string& key,
	    const boost::any& value,
		CallbackWithStatus cb = NilReturn()
	);

	virtual void signal_property_changed(
	    const std::string& key,
	    const boost::any& value = boost::any()
	);

	wpantund_status_t set_ncp_version_string(const std::string& version_string);

protected:
	// ========================================================================
	// MARK: Protected Data

	boost::shared_ptr<TunnelIPv6Interface> mPrimaryInterface;

	boost::shared_ptr<SocketWrapper> mRawSerialAdapter;
	boost::shared_ptr<SocketWrapper> mSerialAdapter;

	struct nlpt mNCPToDriverPumpPT;
	struct nlpt mDriverToNCPPumpPT;

	std::map<struct in6_addr, GlobalAddressEntry> mGlobalAddresses;

	IPv6PacketMatcherRule mCommissioningRule;
	IPv6PacketMatcher mInsecureFirewall;
	IPv6PacketMatcher mDropFirewall;

	time_t mCommissioningExpiration;

	std::string mNCPVersionString;

	bool mEnabled;
	bool mTerminateOnFault;
	bool mAutoUpdateFirmware;
	bool mAutoResume;
	bool mAutoDeepSleep;
	int mAutoDeepSleepTimeout; // In seconds
	uint16_t mCommissionerPort;

private:
	NCPState mNCPState;
	bool mIsInitializingNCP;

protected:
	//! This is set to the currently used MAC address (EUI64).
	uint8_t mMACAddress[8];

	//! This is set to the manufacturer-assigned permanent EUI64 address.
	uint8_t mMACHardwareAddress[8];
	union {
		uint8_t mNCPV6Prefix[8];
		struct in6_addr mNCPMeshLocalAddress;
	};
	struct in6_addr mNCPLinkLocalAddress;

	WPAN::NetworkInstance mCurrentNetworkInstance;

	std::set<unsigned int> mSupprotedChannels;

	NodeType mNodeType;

	int mFailureCount;
	int mFailureThreshold;

	RunawayResetBackoffManager mRunawayResetBackoffManager;

protected:
	// ========================================================================
	// MARK: Legacy Interface Support

	boost::shared_ptr<TunnelIPv6Interface> mLegacyInterface;
	IPv6PacketMatcher mLegacyCommissioningMatcher;
	uint8_t mNCPV6LegacyPrefix[8];
	bool mLegacyInterfaceEnabled;
	bool mNodeTypeSupportsLegacy;

	PcapManager mPcapManager;

private:
	// ========================================================================
	// MARK: Private Data

	int mResetFD; //!^ File descriptor for resetting NCP.
	char mResetFD_BeginReset; //!^ Value for entering reset
	char mResetFD_EndReset; //!^ Value for leaving reset

	int mPowerFD; //!^ File descriptor for controlling NCP power.
	char mPowerFD_PowerOn; //!^ Value for the power being on.
	char mPowerFD_PowerOff; //!^ Value for the power being off.

	int mMCFD; //!^ File descriptor for multicast stuff.

	bool mWasBusy;
	cms_t mLastChangedBusy;

	FirmwareUpgrade mFirmwareUpgrade;

	NetworkRetain mNetworkRetain;

	StatCollector mStatCollector;  // Statistic collector
}; // class NCPInstance

}; // namespace wpantund

}; // namespace nl

// This callback is not sent from the NCP. It is a fake NCP
// callback sent from the processing thread to indicate that
// the NCP is in deep sleep.
#define EVENT_NCP_DISABLED                 0x78C9

#define EVENT_NCP_CONN_RESET               0x78CB

// Extracts a pointer and length from argument list and
// returns a `nl::Data` object.
static inline nl::Data
va_arg_as_Data(va_list args)
{
	const uint8_t* data = NULL;
	size_t data_len = 0;

	data = va_arg(args, const uint8_t*);
	data_len = va_arg(args, size_t);

	// Sanity check
	assert(data_len < 1024*1024);

	return nl::Data(data, data_len);
}

#define va_arg_small(args, type)		static_cast<type>(va_arg(args, int))

#endif /* defined(__wpantund__NCPInstanceBase__) */
