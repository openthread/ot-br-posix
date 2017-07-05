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
 *      Declaration of Statistics collector module
 *
 */

#ifndef wpantund_StatCollector_h
#define wpantund_StatCollector_h

#include <stdint.h>
#include <string>
#include <list>
#include <map>
#include "time-utils.h"
#include "RingBuffer.h"
#include "ObjectPool.h"
#include "NCPControlInterface.h"
#include "NCPTypes.h"
#include "Timer.h"
#include "ValueMap.h"

namespace nl {
namespace wpantund {

// Size of the rx/tx history (for all nodes)
#define STAT_COLLECTOR_RX_HISTORY_SIZE        64
#define STAT_COLLECTOR_TX_HISTORY_SIZE        64

// Size of the NCP state history
#define STAT_COLLECTOR_NCP_STATE_HISTORY_SIZE 64

// Size of the NCP "ReadyForHostSleep" state history
#define STAT_COLLECTOR_NCP_READY_FOR_HOST_SLEEP_STATE_HISTORY_SIZE  64

// Max number of nodes to track at the same time (nodes are tracked by IP address)
#define STAT_COLLECTOR_MAX_NODES   64

// Size of rx/tx history per node
#define STAT_COLLECTOR_PER_NODE_RX_HISTORY_SIZE  5
#define STAT_COLLECTOR_PER_NODE_TX_HISTORY_SIZE  5

// Maximm number of peer nodes for which we store link quality
#define STAT_COLLECTOR_MAX_LINKS   64

// History length of link quality info per peer
#define STAT_COLLECTOR_LINK_QUALITY_HISTORY_SIZE 40

class StatCollector
{
public:
	StatCollector();
	virtual ~StatCollector();

	void set_ncp_control_interface(NCPControlInterface *ncp_ctrl_interface);

	// Static class methods

	static bool is_a_stat_property(const std::string& key);   // returns true if the property key is associated with stat module

	void property_get_value(const std::string& key, CallbackWithStatusArg1 cb);
	void property_set_value(const std::string& key, const boost::any& value, CallbackWithStatus cb);

	// Methods to inform StatCollector about received/sent packets and state changes
	void record_inbound_packet(const uint8_t *ipv6_packet);
	void record_outbound_packet(const uint8_t *ipv6_packet);

private:
	// Internal types and data structures

	typedef std::list<std::string> StringList;

	struct IPAddress
	{
		std::string to_string(void) const;
		void read_from(const uint8_t *arr);
		bool operator==(const IPAddress& lhs) const;
		bool operator<(const IPAddress& lhs) const;
	private:
		uint32_t mAddressBuffer[4];
	};

	struct EUI64Address
	{
		std::string to_string(void) const;
		void read_from(const uint8_t *arr);

		bool operator==(const EUI64Address& lhs) const;
		bool operator<(const EUI64Address& lhs) const;
	private:
		uint32_t mAddress[2];
	};

	struct TimeStamp
	{
		TimeStamp();
		void set_to_now(void);
		void clear(void);
		cms_t get_ms_till_now(void) const;
		bool is_expired(void) const;
		bool is_uninitialized(void) const;
		std::string to_string(void) const;
		bool operator==(const TimeStamp& lhs);
		bool operator<(const TimeStamp& lhs);

		static int32_t time_difference_in_ms(TimeStamp t1, TimeStamp t2);

	private:
		cms_t mTime;
	};

	struct PacketInfo
	{
		TimeStamp  mTimeStamp;
		uint16_t   mPayloadLen;
		uint8_t    mType;
		uint8_t    mSubtype;
		uint16_t   mSrcPort;
		uint16_t   mDstPort;
		IPAddress  mSrcAddress;
		IPAddress  mDstAddress;

		bool update_from_packet(const uint8_t *ipv6_packet);
		std::string to_string(void) const;
	};

	struct BytesTotal
	{
	public:
		BytesTotal();
		void add(uint16_t bytes);
		void clear(void);
		std::string to_string(void) const;
	private:
		uint16_t mBytes;      // Number of bytes remaining till next Kilo bytes (1024 bytes)
		uint32_t mKiloBytes;  // Can go up to 2^32 KB which is 4.3 terabytes (> 4 years of continuous exchange at 250 kbps)
	};

	struct NcpStateInfo
	{
	public:
		void update(NCPState new_state);
		std::string to_string(void) const;
		bool is_expired(void) const;
	private:
		NCPState mNcpState;
		TimeStamp mTimeStamp;
	};

	struct ReadyForHostSleepState
	{
	public:
		void update_with_blocking_sleep_time(TimeStamp blocking_sleep_time);
		std::string to_string(void) const;
	private:
		TimeStamp mStartBlockingHostSleepTime;
		TimeStamp mReadyForHostSleepTime;
	};

	class NodeStat
	{
	public:
		struct NodeInfo
		{
		public:
			uint32_t mTxPacketsTotal;
			uint32_t mTxPacketsUDP;
			uint32_t mTxPacketsTCP;

			uint32_t mRxPacketsTotal;
			uint32_t mRxPacketsUDP;
			uint32_t mRxPacketsTCP;

			RingBuffer<PacketInfo, STAT_COLLECTOR_PER_NODE_RX_HISTORY_SIZE> mRxHistory;
			RingBuffer<PacketInfo, STAT_COLLECTOR_PER_NODE_TX_HISTORY_SIZE> mTxHistory;

			NodeInfo();
			void clear();
			TimeStamp get_last_rx_time(void) const;
			TimeStamp get_last_tx_time(void) const;
			TimeStamp get_last_rx_or_tx_time(void) const;
			void add_rx_stat(StringList& output, bool add_last_rx_time = true) const;
			void add_tx_stat(StringList& output, bool add_last_tx_time = true) const;
			void add_node_info(StringList& output) const;
		};

		NodeStat();
		void clear(void);
		void update_from_inbound_packet(const PacketInfo& packet_info);
		void update_from_outbound_packet(const PacketInfo& packet_info);
		void add_node_stat(StringList& output) const;
		void add_node_stat_history(StringList& output, std::string node_indicator = "") const;

	private:
		NodeInfo *find_node_info(const IPAddress& address);
		NodeInfo *create_new_node_info(const IPAddress& address);
		void remove_oldest_node_info(void);
		void add_node_info_map_iter(StringList &output, const std::map<IPAddress, NodeInfo*>::const_iterator& it) const;

		ObjectPool<NodeInfo, STAT_COLLECTOR_MAX_NODES> mNodeInfoPool;
		std::map<IPAddress, NodeInfo*> mNodeInfoMap;
	};

	class LinkStat
	{
	public:
		struct LinkQuality
		{
			LinkQuality();
			void set(int8_t rssi, uint8_t incoming_link_quality, uint8_t outgoing_link_quality);
			std::string to_string(void) const;
			TimeStamp get_time_stamp(void) const;
		private:
			uint8_t get_incoming_link_quality(void) const;
			uint8_t get_outgoing_link_quality(void) const;

			int8_t mRssi;
			uint8_t mLinkQualityIncomingOutgoing;  // High 4 bits are for incoming, low 4 bits are for outgoing
			TimeStamp mTimeStamp;
		};

		struct LinkInfo
		{
			NodeType mNodeType;
			RingBuffer<LinkQuality, STAT_COLLECTOR_LINK_QUALITY_HISTORY_SIZE> mLinkQualityHistory;

			LinkInfo();
			void clear(void);
			bool empty(void) const;
			void add_link_info(StringList& output, int count = 0) const;
			TimeStamp get_last_update_time(void) const;
		};

		LinkStat();
		void clear();
		void update(const uint8_t *eui64_address, int8_t rssi, uint8_t incoming_link_quality, uint8_t outgoing_link_quality,
			NodeType node_type);
		void add_link_stat(StringList& output, int count = 0) const;

	private:
		LinkInfo *find_link_info(const EUI64Address& address);
		LinkInfo *create_new_link_info(const EUI64Address & address);
		void remove_oldest_link_info(void);

		ObjectPool<LinkInfo, STAT_COLLECTOR_MAX_LINKS> mLinkInfoPool;
		std::map<EUI64Address, LinkInfo *> mLinkInfoMap;
	};

	enum AutoLogState
	{
		kAutoLogDisabled,
		kAutoLogLong,
		kAutoLogShort
	};

private:
	void record_ncp_state_change(NCPState new_ncp_state);
	void record_ncp_ready_for_host_sleep_state(bool ready_to_sleep);
	void add_tx_stat(StringList& output) const;
	void add_rx_stat(StringList& output) const;
	void add_rx_history(StringList& output, int count = 0) const;
	void add_tx_history(StringList& output, int count = 0) const;
	void add_ncp_state_history(StringList& output, int count = 0) const;
	void add_ncp_ready_for_host_sleep_state_history(StringList& output, int count = 0) const;
	void add_help(StringList& output) const;
	void add_all_info(StringList& output, int count = 0) const;
	int  get_stat_property(const std::string& key, StringList& output) const;
	void update_auto_log_timer(void);
	void auto_log_timer_did_fire(void);
	void update_link_stat_timer(Timer::Interval interval);
	void link_stat_timer_did_fire(void);
	void did_get_rip_entry_value_map(int status, const boost::any& value);
	int  record_rip_entry(const ValueMap& rip_entry);
	void property_changed(const std::string& key, const boost::any& value);
	void did_rx_net_scan_beacon(const WPAN::NetworkInstance& network);

private:
	NCPControlInterface *mControlInterface;

	uint32_t mTxPacketsTotal;
	uint32_t mRxPacketsUDP;
	uint32_t mRxPacketsTCP;
	uint32_t mRxPacketsICMP;
	BytesTotal mTxBytesTotal;

	uint32_t mRxPacketsTotal;
	uint32_t mTxPacketsUDP;
	uint32_t mTxPacketsTCP;
	uint32_t mTxPacketsICMP;
	BytesTotal mRxBytesTotal;

	RingBuffer<PacketInfo, STAT_COLLECTOR_RX_HISTORY_SIZE> mRxHistory;
	RingBuffer<PacketInfo, STAT_COLLECTOR_TX_HISTORY_SIZE> mTxHistory;

	RingBuffer<NcpStateInfo, STAT_COLLECTOR_NCP_STATE_HISTORY_SIZE> mNCPStateHistory;

	RingBuffer<ReadyForHostSleepState, STAT_COLLECTOR_NCP_READY_FOR_HOST_SLEEP_STATE_HISTORY_SIZE> mReadyForSleepHistory;
	bool mLastReadyForHostSleepState;
	TimeStamp mLastBlockingHostSleepTime;

	NodeStat mNodeStat;
	LinkStat mLinkStat;

	Timer mAutoLogTimer;
	Timer mLinkStatTimer;

	Timer::Interval mAutoLogPeriod;
	enum AutoLogState mAutoLogState;

	int mAutoLogLevel;
	int mUserRequestLogLevel;
};

}; // namespace wpantund
}; // namespace nl

#endif  // defined(wpantund_StatCollector_h)
