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
 *      Statistics collector module.
 *
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <syslog.h>
#include <arpa/inet.h>
#include <iterator>
#include "StatCollector.h"
#include "any-to.h"
#include "wpan-error.h"

using namespace nl;
using namespace wpantund;

// Enable additional debug logs only in this module
#define ENABLE_MODULE_DEBUG  0
#if ENABLE_MODULE_DEBUG
#define DEBUG_LOG(msg, ...)  syslog(LOG_INFO, msg , ##__VA_ARGS__)
#else
#define DEBUG_LOG(msg, ...)  do { } while(false)
#endif


// Number of history items to show for short version of stat "Stat:Short"
#define STAT_COLLECTOR_SHORT_HISTORY_COUNT        10

// Number of history items to show for short version of stat "Stat:LinkQuality:Short"
#define STAT_COLLECTOR_LINK_STAT_HISTORY_SIZE     8

// Log level for adding logs to syslog when user/application requests it
#define STAT_COLLECTOR_LOG_LEVEL_USER_REQUEST     LOG_INFO

// Default log level for short auto logs (periodic logging)
#define STAT_COLLECTOR_AUTO_LOG_DEFAULT_LOG_LEVEL LOG_INFO

// Default period (in min) for automatically logging stat info
#define STAT_COLLECTOR_AUTO_LOG_PERIOD_IN_MIN     30     // 30 min

// Maximum allowed period for auto log (value is in min)
#define STAT_COLLECTOR_AUTO_LOG_MAX_PERIOD		  (60 * 24 * 7 * 2) // Two weeks

// Time stamp constants
#define TIMESTAMP_ONE_SEC_IN_MS           ((int32_t)1000)
#define TIMESTAMP_ONE_MIN_IN_MS           (TIMESTAMP_ONE_SEC_IN_MS * 60)
#define TIMESTAMP_ONE_HOUR_IN_MS          (TIMESTAMP_ONE_MIN_IN_MS * 60)
#define TIMESTAMP_ONE_DAY_IN_MS           (TIMESTAMP_ONE_HOUR_IN_MS * 24)
#define TIMESTAMP_UNINITIALIZED_VALUE     0

// IPv6 types
#define IPV6_TYPE_UDP                     0x11
#define IPV6_TYPE_TCP                     0x06
#define IPV6_TYPE_ICMP                    0x3A
#define IPV6_ICMP_TYPE_ECHO_REQUEST       128
#define IPV6_ICMP_TYPE_ECHO_REPLY         129

// IPv6 Header Offset
#define IPV6_HEADER_VERSION_OFFSET        0
#define IPV6_HEADER_PAYLOAD_LEN_OFFSET    4
#define IPV6_HEADER_TYPE_OFFSET           6
#define IPV6_HEADER_SRC_ADDRESS_OFFSET    8
#define IPV6_HEADER_DST_ADDRESS_OFFSET    24
#define IPV6_UDP_HEADER_SRC_PORT_OFFSET   40
#define IPV6_UDP_HEADER_DST_PORT_OFFSET   42
#define IPV6_ICMP_HEADER_CODE_OFFSET      40

#define IPV6_GET_UINT16(pkt,idx)   ( (static_cast<uint16_t>(pkt[idx]) << 8) + static_cast<uint16_t>(pkt[idx + 1] << 0) )

//===================================================================

static std::string
string_printf(const char *fmt, ...)
{
	va_list args;
	char c_str_buf[512];
	va_start(args, fmt);
	vsnprintf(c_str_buf, sizeof(c_str_buf), fmt, args);
	va_end(args);
	return std::string(c_str_buf);
}

static std::string
log_level_to_string(int log_level)
{
	switch(log_level) {
		case LOG_EMERG:   return "emerg";
		case LOG_ALERT:   return "alert";
		case LOG_CRIT:    return "crit";
		case LOG_ERR:     return "err";
		case LOG_WARNING: return "warning";
		case LOG_NOTICE:  return "notice";
		case LOG_INFO:    return "info";
		case LOG_DEBUG:   return "debug";
	}
	return string_printf("unknown(%d)", log_level);
}

// Converts a string to a log level, returns -1 if not a valid log level
static int
log_level_from_string(const char *log_string)
{
	int log_level = -1;

	if (strcaseequal(log_string, "emerg")) {
		log_level = LOG_EMERG;
	} else 	if (strcaseequal(log_string, "alert")) {
		log_level = LOG_ALERT;
	} else 	if (strcaseequal(log_string, "crit")) {
		log_level = LOG_CRIT;
	} else 	if (strcaseequal(log_string, "err") || (strcaseequal(log_string, "error"))) {
		log_level = LOG_ERR;
	} else 	if (strcaseequal(log_string, "warning")) {
		log_level = LOG_WARNING;
	} else 	if (strcaseequal(log_string, "notice")) {
		log_level = LOG_NOTICE;
	} else 	if (strcaseequal(log_string, "info")) {
		log_level = LOG_INFO;
	} else 	if (strcaseequal(log_string, "debug")) {
		log_level = LOG_DEBUG;
	}

	return log_level;
}

//-------------------------------------------------------------------
// IPAddress

void
StatCollector::IPAddress::read_from(const uint8_t *arr)
{
	memcpy(static_cast<void *>(mAddressBuffer), arr, sizeof(mAddressBuffer));
}

std::string
StatCollector::IPAddress::to_string(void) const
{
	char address_string[INET6_ADDRSTRLEN] = "::";
	inet_ntop(AF_INET6, static_cast<const void *>(mAddressBuffer), address_string, sizeof(address_string));
	return std::string(address_string);
}

bool
StatCollector::IPAddress::operator==(const IPAddress& lhs) const
{
	// Since IPv6 addresses typically start with same prefix, we intentionally
	// start the comparison from the end of address buffer
	for(int indx = sizeof(mAddressBuffer)/sizeof(mAddressBuffer[0]) - 1; indx ; indx--) {
		if (mAddressBuffer[indx] != lhs.mAddressBuffer[indx]) {
			return false;
		}
	}

	return true;
}

bool
StatCollector::IPAddress::operator<(const IPAddress& lhs) const
{
	// Since IPv6 addresses typically start with same prefix, we intentionally
	// start the comparison from the end of address buffer

	for(int indx = sizeof(mAddressBuffer)/sizeof(mAddressBuffer[0]) - 1; indx ; indx--) {
		if (mAddressBuffer[indx] < lhs.mAddressBuffer[indx]) {
			return true;
		}

		if (mAddressBuffer[indx] > lhs.mAddressBuffer[indx]) {
			return false;
		}
	}

	// If all is equal, then return false.
	return false;
}

//-------------------------------------------------------------------
// EUI64Address

void
StatCollector::EUI64Address::read_from(const uint8_t *arr)
{
	mAddress[0] = arr[3] + (arr[2] << 8) + (arr[1] << 16) + (arr[0] << 24);
	mAddress[1] = arr[7] + (arr[6] << 8) + (arr[5] << 16) + (arr[4] << 24);
}

std::string
StatCollector::EUI64Address::to_string(void) const
{
	return string_printf("%08X%08X", mAddress[0], mAddress[1]);
}

bool
StatCollector::EUI64Address::operator==(const EUI64Address& lhs) const
{
	return (mAddress[1] == lhs.mAddress[1]) && (mAddress[0] == lhs.mAddress[0]);
}

bool
StatCollector::EUI64Address::operator<(const EUI64Address& lhs) const
{
	if (mAddress[1] < lhs.mAddress[1]) {
		return true;
	}

	if (mAddress[1] > lhs.mAddress[1]) {
		return false;
	}

	if (mAddress[0] < lhs.mAddress[0]) {
		return true;
	}

	return false;
}

//-------------------------------------------------------------------
// TimeStamp

StatCollector::TimeStamp::TimeStamp()
{
	mTime = TIMESTAMP_UNINITIALIZED_VALUE;
}

void
StatCollector::TimeStamp::set_to_now(void)
{
	mTime = time_ms();

	if (mTime == TIMESTAMP_UNINITIALIZED_VALUE) {
		mTime--;
	}
}

void
StatCollector::TimeStamp::clear(void)
{
	mTime = TIMESTAMP_UNINITIALIZED_VALUE;
}

cms_t
StatCollector::TimeStamp::get_ms_till_now(void) const
{
	return CMS_SINCE(mTime);
}

bool
StatCollector::TimeStamp::is_expired(void) const
{
	if (mTime == TIMESTAMP_UNINITIALIZED_VALUE) {
		return true;
	}

	return (get_ms_till_now() < 0);
}

bool
StatCollector::TimeStamp::is_uninitialized(void) const
{
	return (mTime == TIMESTAMP_UNINITIALIZED_VALUE);
}

bool
StatCollector::TimeStamp::operator==(const TimeStamp &lhs)
{
	return mTime == lhs.mTime;
}

bool
StatCollector::TimeStamp::operator<(const TimeStamp &lhs)
{
	return (mTime - lhs.mTime) < 0;
}

std::string
StatCollector::TimeStamp::to_string(void) const
{
	int32_t days, hours, minutes, seconds, milliseconds;
	cms_t ms_till_now = get_ms_till_now();

	if (mTime == TIMESTAMP_UNINITIALIZED_VALUE)
		return std::string("never");

	if (ms_till_now < 0)
		return std::string("long time (>24.86 days) ago");

	days = ms_till_now / TIMESTAMP_ONE_DAY_IN_MS;
	ms_till_now %= TIMESTAMP_ONE_DAY_IN_MS;
	hours = ms_till_now / TIMESTAMP_ONE_HOUR_IN_MS;
	ms_till_now %= TIMESTAMP_ONE_HOUR_IN_MS;
	minutes = ms_till_now / TIMESTAMP_ONE_MIN_IN_MS;
	ms_till_now %= TIMESTAMP_ONE_MIN_IN_MS;
	seconds = ms_till_now / TIMESTAMP_ONE_SEC_IN_MS;
	milliseconds = ms_till_now % TIMESTAMP_ONE_SEC_IN_MS;

	if (days != 0) {
		return string_printf("%2d day%s %02d:%02d:%02d.%03d ago", days, (days > 1)? "s" : "",
		         hours, minutes, seconds, milliseconds);
	}

	return string_printf("%02d:%02d:%02d.%03d ago", hours, minutes, seconds, milliseconds);
}

int32_t
StatCollector::TimeStamp::time_difference_in_ms(TimeStamp t1, TimeStamp t2)
{
	return (t2.mTime - t1.mTime);
}

//-------------------------------------------------------------------
// BytesTotal

StatCollector::BytesTotal::BytesTotal()
{
	clear();
}

void
StatCollector::BytesTotal::clear()
{
	mBytes = 0;
	mKiloBytes = 0;
}

void
StatCollector::BytesTotal::add(uint16_t count)
{
	uint32_t num_bytes;

	num_bytes = count;
	num_bytes += mBytes;

	mKiloBytes += (num_bytes >> 10);  // divide by 1024.
	num_bytes &= 1023;                // reminder
	mBytes = static_cast<uint16_t>(num_bytes);
}

std::string
StatCollector::BytesTotal::to_string() const
{
	if (mKiloBytes == 0) {
		return string_printf("%d bytes", mBytes);
	}

	if (mBytes == 0) {
		return string_printf("%d Kbytes", mKiloBytes);
	}

	return string_printf("%d Kbytes & %d bytes", mKiloBytes, mBytes);
}

//-------------------------------------------------------------------
// PacketInfo

bool
StatCollector::PacketInfo::update_from_packet(const uint8_t *packet)
{
	bool ret = false;

	// Check the version in IPv6 header
	if ((packet[IPV6_HEADER_VERSION_OFFSET] & 0xF0) == 0x60)  {

		mTimeStamp.set_to_now();

		mPayloadLen = IPV6_GET_UINT16(packet, IPV6_HEADER_PAYLOAD_LEN_OFFSET);

		mType = packet[IPV6_HEADER_TYPE_OFFSET];

		mSrcAddress.read_from(&packet[IPV6_HEADER_SRC_ADDRESS_OFFSET]);
		mDstAddress.read_from(&packet[IPV6_HEADER_DST_ADDRESS_OFFSET]);

		if (mType == IPV6_TYPE_ICMP) {
			mSubtype = packet[IPV6_ICMP_HEADER_CODE_OFFSET];
		} else {
			mSubtype = 0;
		}

		if ((mType == IPV6_TYPE_UDP) || (mType == IPV6_TYPE_TCP)) {
			mSrcPort = IPV6_GET_UINT16(packet, IPV6_UDP_HEADER_SRC_PORT_OFFSET);
			mDstPort = IPV6_GET_UINT16(packet, IPV6_UDP_HEADER_DST_PORT_OFFSET);
		} else {
			mSrcPort = 0;
			mDstPort = 0;
		}

		ret = true;
	}

	return ret;
}

std::string
StatCollector::PacketInfo::to_string(void) const
{
	std::string type_str;
	bool has_port;

	has_port = false;
	switch(mType) {
		case IPV6_TYPE_TCP:
			type_str = "TCP";
			has_port = true;
			break;

		case IPV6_TYPE_UDP:
			type_str = "UDP";
			has_port = true;
			break;

		case IPV6_TYPE_ICMP:
			switch(mSubtype) {
				case IPV6_ICMP_TYPE_ECHO_REPLY:
					type_str = "ICMP6(echo reply)";
					break;
				case IPV6_ICMP_TYPE_ECHO_REQUEST:
					type_str = "ICMP6(echo request)";
					break;
				default:
					type_str = string_printf("ICMP6(code:%d)", mSubtype);
					break;
			}
			break;

		default:
			type_str = string_printf("0x%02x", mType);
			break;
	}

	if (has_port) {
		return string_printf(
		        "%s -> type:%s len:%d from:[%s]:%d to:[%s]:%d",
		        mTimeStamp.to_string().c_str(),
		        type_str.c_str(),
		        mPayloadLen,
		        mSrcAddress.to_string().c_str(), mSrcPort,
		        mDstAddress.to_string().c_str(), mDstPort
		);
	}

	return string_printf(
	        "%s -> type:%s len:%d from:[%s] to:[%s]",
	        mTimeStamp.to_string().c_str(),
	        type_str.c_str(),
	        mPayloadLen,
	        mSrcAddress.to_string().c_str(),
	        mDstAddress.to_string().c_str()
	);
}

//-------------------------------------------------------------------
// NcpStateInfo

void
StatCollector::NcpStateInfo::update(NCPState new_state)
{
	mTimeStamp.set_to_now();
	mNcpState = new_state;
}

std::string
StatCollector::NcpStateInfo::to_string(void) const
{
	return
		string_printf("%s -> %s",
			mTimeStamp.to_string().c_str(),
			ncp_state_to_string(mNcpState).c_str()
		);
}

bool
StatCollector::NcpStateInfo::is_expired(void) const
{
	return mTimeStamp.is_expired();
}

//------------------------------------------------------------------
// ReadyForHostSleepState

void
StatCollector::ReadyForHostSleepState::update_with_blocking_sleep_time(TimeStamp blocking_sleep_time)
{
	mStartBlockingHostSleepTime = blocking_sleep_time;
	mReadyForHostSleepTime.set_to_now();
}

std::string
StatCollector::ReadyForHostSleepState::to_string(void) const
{
	if (mReadyForHostSleepTime.is_uninitialized() || mStartBlockingHostSleepTime.is_uninitialized()) {
		return "Uninitialized";
	}

	return mStartBlockingHostSleepTime.to_string() + string_printf(" host sleep was blocked for %d ms",
		TimeStamp::time_difference_in_ms(mStartBlockingHostSleepTime, mReadyForHostSleepTime));
}

//-------------------------------------------------------------------
// Node Stat

StatCollector::NodeStat::NodeStat():
	mNodeInfoPool(), mNodeInfoMap()
{
	return;
}

void
StatCollector::NodeStat::clear()
{
	mNodeInfoMap.clear();
	mNodeInfoPool.free_all();
}

StatCollector::NodeStat::NodeInfo *
StatCollector::NodeStat::find_node_info(const IPAddress& address)
{
	std::map<IPAddress, NodeInfo*>::iterator it = mNodeInfoMap.find(address);

	if (it == mNodeInfoMap.end())
		return NULL;

	return it->second;
}

StatCollector::NodeStat::NodeInfo *
StatCollector::NodeStat::create_new_node_info(const IPAddress& address)
{
	NodeInfo *node_info_ptr = NULL;

	do {
		node_info_ptr = mNodeInfoPool.alloc();

		// If we can not allocate a new node info (all objects in the pool are used),
		// we will remove oldest node info and try again.
		if (!node_info_ptr) {
			remove_oldest_node_info();
		}
	} while (!node_info_ptr);

	node_info_ptr->clear();

	mNodeInfoMap.insert(std::pair<IPAddress, NodeInfo*>(address, node_info_ptr));

	return node_info_ptr;
}

void
StatCollector::NodeStat::remove_oldest_node_info(void)
{
	std::map<IPAddress, NodeInfo*>::iterator cur_iter, oldest_iter;
	TimeStamp ts, oldest_ts;

	for (cur_iter = oldest_iter = mNodeInfoMap.begin(); cur_iter != mNodeInfoMap.end(); cur_iter++) {

		ts = cur_iter->second->get_last_rx_or_tx_time();

		if (oldest_ts.is_uninitialized()) {
			oldest_ts = ts;
			oldest_iter = cur_iter;
		}

		if (!ts.is_uninitialized()) {
			if (ts < oldest_ts) {
				oldest_ts = ts;
				oldest_iter = cur_iter;
			}
		}
	}

	if (oldest_iter != mNodeInfoMap.end()) {
		NodeInfo *node_info_ptr = oldest_iter->second;
		mNodeInfoMap.erase(oldest_iter);
		mNodeInfoPool.free(node_info_ptr);

		syslog(LOG_INFO, "StatCollector: Out of NodeInfo objects --> Deleted the oldest NodeInfo");
	}
}

void
StatCollector::NodeStat::update_from_inbound_packet(const PacketInfo& packet_info)
{
	NodeInfo *node_info_ptr;

	node_info_ptr = find_node_info(packet_info.mSrcAddress);
	if (!node_info_ptr) {
		node_info_ptr = create_new_node_info(packet_info.mSrcAddress);
	}

	if (node_info_ptr) {
		node_info_ptr->mRxPacketsTotal++;
		switch(packet_info.mType) {
			case IPV6_TYPE_UDP: node_info_ptr->mRxPacketsUDP++; break;
			case IPV6_TYPE_TCP: node_info_ptr->mRxPacketsTCP++; break;
		}
		node_info_ptr->mRxHistory.force_write(packet_info);
	}
}

void
StatCollector::NodeStat::update_from_outbound_packet(const PacketInfo& packet_info)
{
	NodeInfo *node_info_ptr;

	node_info_ptr = find_node_info(packet_info.mDstAddress);
	if (!node_info_ptr) {
		node_info_ptr = create_new_node_info(packet_info.mDstAddress);
	}

	if (node_info_ptr) {
		node_info_ptr->mTxPacketsTotal++;
		switch(packet_info.mType) {
			case IPV6_TYPE_UDP:  node_info_ptr->mTxPacketsUDP++;  break;
			case IPV6_TYPE_TCP:  node_info_ptr->mTxPacketsTCP++;  break;
		}
		node_info_ptr->mTxHistory.force_write(packet_info);
	}
}

void
StatCollector::NodeStat::add_node_info_map_iter(StringList &output, const std::map<IPAddress, NodeInfo*>::const_iterator& it) const
{
	output.push_back("========================================================");
	output.push_back("Address: " + it->first.to_string());
	it->second->add_node_info(output);
	output.push_back("");
}

void
StatCollector::NodeStat::add_node_stat_history(StringList& output, std::string node_indicator) const
{
	std::map<IPAddress, NodeInfo*>::const_iterator it;

	if (node_indicator.empty()) {
		for (it = mNodeInfoMap.begin(); it != mNodeInfoMap.end(); it++) {
			add_node_info_map_iter(output, it);
		}
	} else {
		char c = node_indicator[0];
		if (c == '@' || c == '[') { // Ip address mode
			std::string ip_addr_str;
			uint8_t ip_addr_buf[16];

			if (c == '@') {
				ip_addr_str = node_indicator.substr(1);
			} else {
				if (node_indicator[node_indicator.length() - 1] == ']') {
					ip_addr_str = node_indicator.substr(1, node_indicator.length() - 2);
				} else {
					output.push_back(string_printf("Error : Missing \']\' in address format (\'%s\')", node_indicator.c_str()));
					return;
				}
			}

			if (inet_pton(AF_INET6, ip_addr_str.c_str(), ip_addr_buf) > 0) {
				IPAddress ip_address;
				ip_address.read_from(ip_addr_buf);
				it = mNodeInfoMap.find(ip_address);
				if (it != mNodeInfoMap.end()) {
					add_node_info_map_iter(output, it);
				} else {
					output.push_back(string_printf("Error : Address does not exist (\'%s\')", node_indicator.c_str()));
				}
			} else {
				output.push_back(string_printf("Error : Improper address format (\'%s\')", node_indicator.c_str()));
			}
		} else { // Index mode:
			int index;
			index = static_cast<int>(strtol(node_indicator.c_str(), NULL, 0));
			if (index < mNodeInfoMap.size()) {
				it = mNodeInfoMap.begin();
				std::advance(it, index);
				add_node_info_map_iter(output, it);
			} else {
				output.push_back(string_printf("Error: Out of bound index %d (\'%s\')", index, node_indicator.c_str()));
			}
		}
	}
}

void
StatCollector::NodeStat::add_node_stat(StringList& output) const
{
	std::map<IPAddress, NodeInfo*>::const_iterator it;

	for (it = mNodeInfoMap.begin(); it != mNodeInfoMap.end(); it++) {
		output.push_back("========================================================");
		output.push_back("Address: " + it->first.to_string());
		it->second->add_tx_stat(output);
		it->second->add_rx_stat(output);
		output.push_back("");
	}
}

//-------------------------------------------------------------------
// NodeStat::NodeInfo

StatCollector::NodeStat::NodeInfo::NodeInfo()
{
	clear();
}

void
StatCollector::NodeStat::NodeInfo::clear(void)
{
	mTxPacketsTotal = 0;
	mTxPacketsUDP = 0;
	mTxPacketsTCP = 0;

	mRxPacketsTotal = 0;
	mRxPacketsUDP = 0;
	mRxPacketsTCP = 0;

	mRxHistory.clear();
	mTxHistory.clear();
}

StatCollector::TimeStamp
StatCollector::NodeStat::NodeInfo::get_last_rx_time(void) const
{
	TimeStamp ts;
	const PacketInfo *pkt_info_ptr;

	pkt_info_ptr = mRxHistory.back();
	if (pkt_info_ptr) {
		ts = pkt_info_ptr->mTimeStamp;
	}
	return ts;
}

StatCollector::TimeStamp
StatCollector::NodeStat::NodeInfo::get_last_tx_time(void) const
{
	TimeStamp ts;
	const PacketInfo *pkt_info_ptr;

	pkt_info_ptr = mTxHistory.back();
	if (pkt_info_ptr) {
		ts = pkt_info_ptr->mTimeStamp;
	}
	return ts;
}

StatCollector::TimeStamp
StatCollector::NodeStat::NodeInfo::get_last_rx_or_tx_time(void) const
{
	TimeStamp rx_time = get_last_rx_time();
	TimeStamp tx_time = get_last_tx_time();

	// If either one is uninitialized, return the other one.
	if (rx_time.is_uninitialized()) {
		return tx_time;
	}

	if (tx_time.is_uninitialized()) {
		return rx_time;
	}

	if (rx_time < tx_time) {
		return tx_time;
	}

	return rx_time;
}

void
StatCollector::NodeStat::NodeInfo::add_tx_stat(StringList& output, bool add_last_tx_time) const
{
	std::string str;

	str = string_printf("%d packet%s (%d udp, %d tcp, %d other) %s sent to this address",
		mTxPacketsTotal,
		(mTxPacketsTotal == 1)? "" : "s",
		mTxPacketsUDP,
		mTxPacketsTCP,
		mTxPacketsTotal - mTxPacketsUDP - mTxPacketsTCP,
		(mTxPacketsTotal == 1)? "was" : "were"
	);

	if (add_last_tx_time) {
		TimeStamp last_tx_time = get_last_tx_time();
		if (!last_tx_time.is_uninitialized()) {
			str += " - last tx happened " + last_tx_time.to_string();
		}
	}

	output.push_back(str);
}

void
StatCollector::NodeStat::NodeInfo::add_rx_stat(StringList& output, bool add_last_rx_time) const
{
	std::string str;

	str = string_printf("%d packet%s (%d udp, %d tcp, %d other) %s received from this address",
		mRxPacketsTotal,
		(mRxPacketsTotal == 1)? "" : "s",
		mRxPacketsUDP,
		mRxPacketsTCP,
		mRxPacketsTotal - mRxPacketsUDP - mRxPacketsTCP,
		(mRxPacketsTotal == 1)? "was" : "were"
	);

	if (add_last_rx_time) {
		TimeStamp last_rx_time = get_last_rx_time();
		if (!last_rx_time.is_uninitialized()) {
			str += " - last rx happened " + last_rx_time.to_string();
		}
	}

	output.push_back(str);
}

void
StatCollector::NodeStat::NodeInfo::add_node_info(StringList& output) const
{
	add_tx_stat(output, false);

	if (!mTxHistory.empty()) {
		RingBuffer<PacketInfo, STAT_COLLECTOR_PER_NODE_TX_HISTORY_SIZE>::ReverseIterator iter;

		output.push_back(string_printf("\tLast %d tx packets", mTxHistory.size()));
		for (iter = mTxHistory.rbegin(); iter != mTxHistory.rend(); ++iter) {
			output.push_back("\t" + iter->to_string());
		}
	}
	output.push_back("");

	add_rx_stat(output, false);

	if (!mRxHistory.empty()) {
		RingBuffer<PacketInfo, STAT_COLLECTOR_PER_NODE_RX_HISTORY_SIZE>::ReverseIterator iter;

		output.push_back(string_printf("\tLast %d rx packets", mRxHistory.size()));
		for (iter = mRxHistory.rbegin(); iter != mRxHistory.rend(); ++iter) {
			output.push_back("\t" + iter->to_string());
		}
	}
}

//-------------------------------------------------------------------
// LinkStat:LinkQuality

StatCollector::LinkStat::LinkQuality::LinkQuality()
		: mTimeStamp()
{
	mRssi = 0;
	mLinkQualityIncomingOutgoing = 0xff;
}

void
StatCollector::LinkStat::LinkQuality::set(int8_t rssi, uint8_t incoming_link_quality, uint8_t outgoing_link_quality)
{
	mRssi = rssi;
	mLinkQualityIncomingOutgoing = ((incoming_link_quality & 0x0f) << 4) + (outgoing_link_quality & 0x0f);
	mTimeStamp.set_to_now();
}

StatCollector::TimeStamp
StatCollector::LinkStat::LinkQuality::get_time_stamp(void) const
{
	return mTimeStamp;
}

uint8_t
StatCollector::LinkStat::LinkQuality::get_incoming_link_quality(void) const
{
	return (mLinkQualityIncomingOutgoing >> 4);
}

uint8_t
StatCollector::LinkStat::LinkQuality::get_outgoing_link_quality(void) const
{
	return (mLinkQualityIncomingOutgoing & 0x0f);
}

std::string
StatCollector::LinkStat::LinkQuality::to_string(void) const
{
	if (mTimeStamp.is_uninitialized()) {
		return "Uninitialized";
	}

	return mTimeStamp.to_string() + string_printf("-> RSSI: %-6d  LinkQuality(Incoming/Outgoing): %d/%d", mRssi,
			get_incoming_link_quality(), get_outgoing_link_quality());
}

//-------------------------------------------------------------------
// LinkStat::LinkInfo

StatCollector::LinkStat::LinkInfo::LinkInfo() :
		mNodeType(), mLinkQualityHistory()
{
	clear();
}

void
StatCollector::LinkStat::LinkInfo::clear(void)
{
	mLinkQualityHistory.clear();
}

bool
StatCollector::LinkStat::LinkInfo::empty(void) const
{
	return mLinkQualityHistory.empty();
}

void
StatCollector::LinkStat::LinkInfo::add_link_info(StringList& output, int count) const
{
	RingBuffer<LinkQuality, STAT_COLLECTOR_LINK_QUALITY_HISTORY_SIZE>::ReverseIterator iter;

	if (count == 0) {
		count = mLinkQualityHistory.size();
	}

	for (iter = mLinkQualityHistory.rbegin(); (iter != mLinkQualityHistory.rend()) && (count != 0); ++iter, --count) {
		output.push_back("\t" + iter->to_string());
	}
}

StatCollector::TimeStamp
StatCollector::LinkStat::LinkInfo::get_last_update_time(void) const
{
	TimeStamp ts;
	const LinkQuality *link_quality_ptr;

	link_quality_ptr = mLinkQualityHistory.back();
	if (link_quality_ptr) {
		ts = link_quality_ptr->get_time_stamp();
	}
	return ts;
}

//-------------------------------------------------------------------
// LinkStat

StatCollector::LinkStat::LinkStat()
		: mLinkInfoPool(), mLinkInfoMap()
{
}

void
StatCollector::LinkStat::clear(void)
{
	mLinkInfoMap.clear();
	mLinkInfoPool.free_all();
}


StatCollector::LinkStat::LinkInfo *
StatCollector::LinkStat::find_link_info(const EUI64Address& address)
{
	std::map<EUI64Address, LinkInfo *>::iterator it = mLinkInfoMap.find(address);

	if (it == mLinkInfoMap.end())
		return NULL;

	return it->second;
}

StatCollector::LinkStat::LinkInfo *
StatCollector::LinkStat::create_new_link_info(const EUI64Address& address)
{
	LinkInfo *link_info_ptr = NULL;

	do {
		link_info_ptr = mLinkInfoPool.alloc();

		// If we can not allocate a new link info (all objects in the pool are used),
		// we will remove oldest one in the map and try again.
		if (!link_info_ptr) {
			remove_oldest_link_info();
		}
	} while (!link_info_ptr);

	link_info_ptr->clear();

	mLinkInfoMap.insert(std::pair<EUI64Address, LinkInfo*>(address, link_info_ptr));

	return link_info_ptr;
}

void
StatCollector::LinkStat::remove_oldest_link_info(void)
{
	std::map<EUI64Address, LinkInfo*>::iterator cur_iter, oldest_iter;
	TimeStamp ts, oldest_ts;

	for (cur_iter = oldest_iter = mLinkInfoMap.begin(); cur_iter != mLinkInfoMap.end(); cur_iter++) {

		ts = cur_iter->second->get_last_update_time();

		if (oldest_ts.is_uninitialized()) {
			oldest_ts = ts;
			oldest_iter = cur_iter;
		}

		if (!ts.is_uninitialized()) {
			if (ts < oldest_ts) {
				oldest_ts = ts;
				oldest_iter = cur_iter;
			}
		}
	}

	if (oldest_iter != mLinkInfoMap.end()) {
		LinkInfo *link_info_ptr = oldest_iter->second;
		mLinkInfoMap.erase(oldest_iter);
		mLinkInfoPool.free(link_info_ptr);

		syslog(LOG_INFO, "StatCollector: Out of LinkInfo objects --> Deleted the oldest LinkInfo");
	}
}

void
StatCollector::LinkStat::update(const uint8_t *eui64_address_arr, int8_t rssi,
		uint8_t incoming_link_quality, uint8_t outgoing_link_quality,
		NodeType node_type)
{
	LinkQuality link_quality;
	EUI64Address address;
	LinkInfo *link_info_ptr;

	if (eui64_address_arr) {

		address.read_from(eui64_address_arr);

		link_quality.set(rssi, incoming_link_quality, outgoing_link_quality);

		link_info_ptr = find_link_info(address);
		if (!link_info_ptr) {
			link_info_ptr = create_new_link_info(address);
		}

		if (link_info_ptr) {
			link_info_ptr->mLinkQualityHistory.force_write(link_quality);
			link_info_ptr->mNodeType = node_type;
		}
	}
}

void
StatCollector::LinkStat::add_link_stat(StringList& output, int count) const
{
	std::map<EUI64Address, LinkInfo*>::const_iterator it;

	for (it = mLinkInfoMap.begin(); it != mLinkInfoMap.end(); ++it) {
		output.push_back("========================================================");
		output.push_back("EUI64 address: " + it->first.to_string() + " -  Node type: " +
			node_type_to_string(it->second->mNodeType));
		it->second->add_link_info(output, count);
		output.push_back("");
	}
}

//-------------------------------------------------------------------
// StatCollector

StatCollector::StatCollector() :
		mTxBytesTotal(), mRxBytesTotal(),
		mRxHistory(), mTxHistory(),
		mLastBlockingHostSleepTime(),
		mNodeStat(), mLinkStat(),
		mAutoLogTimer(), mLinkStatTimer()
{
	mControlInterface = NULL;

	mTxPacketsTotal = 0;
	mRxPacketsTotal = 0;
	mRxPacketsUDP = 0;
	mRxPacketsTCP = 0;
	mRxPacketsICMP = 0;
	mTxPacketsUDP = 0;
	mTxPacketsTCP = 0;
	mTxPacketsICMP = 0;

	mLastReadyForHostSleepState = true;

	mUserRequestLogLevel = STAT_COLLECTOR_LOG_LEVEL_USER_REQUEST;
	mAutoLogLevel = STAT_COLLECTOR_AUTO_LOG_DEFAULT_LOG_LEVEL;

	mAutoLogState = kAutoLogShort;
	mAutoLogPeriod = STAT_COLLECTOR_AUTO_LOG_PERIOD_IN_MIN * Timer::kOneMinute;
	update_auto_log_timer();
}

StatCollector::~StatCollector()
{
	mAutoLogTimer.cancel();
}

void
StatCollector::set_ncp_control_interface(NCPControlInterface *ncp_ctrl_interface)
{
	if (mControlInterface == ncp_ctrl_interface) {
		return;
	}

	if (mControlInterface) {
		mControlInterface->mOnPropertyChanged.disconnect(
			boost::bind(&StatCollector::property_changed, this, _1, _2)
		);

		mControlInterface->mOnNetScanBeacon.disconnect(
			boost::bind(&StatCollector::did_rx_net_scan_beacon, this, _1)
		);
	}

	mControlInterface = ncp_ctrl_interface;

	if (mControlInterface) {
		mControlInterface->mOnPropertyChanged.connect(
			boost::bind(&StatCollector::property_changed, this, _1, _2)
		);

		mControlInterface->mOnNetScanBeacon.connect(
			boost::bind(&StatCollector::did_rx_net_scan_beacon, this, _1)
		);
	}
}

void
StatCollector::record_inbound_packet(const uint8_t *packet)
{
	PacketInfo packet_info;

	if (packet_info.update_from_packet(packet)) {
		mRxPacketsTotal++;
		switch (packet_info.mType) {
			case IPV6_TYPE_UDP:  mRxPacketsUDP++;  break;
			case IPV6_TYPE_TCP:  mRxPacketsTCP++;  break;
			case IPV6_TYPE_ICMP: mRxPacketsICMP++; break;
		}
		mRxBytesTotal.add(packet_info.mPayloadLen);
		mRxHistory.force_write(packet_info);

		mNodeStat.update_from_inbound_packet(packet_info);
	}
}

void
StatCollector::record_outbound_packet(const uint8_t *packet)
{
	PacketInfo packet_info;

	if (packet_info.update_from_packet(packet)) {
		mTxPacketsTotal++;
		switch (packet_info.mType) {
			case IPV6_TYPE_UDP:  mTxPacketsUDP++;  break;
			case IPV6_TYPE_TCP:  mTxPacketsTCP++;  break;
			case IPV6_TYPE_ICMP: mTxPacketsICMP++; break;
		}
		mTxBytesTotal.add(packet_info.mPayloadLen);
		mTxHistory.force_write(packet_info);

		mNodeStat.update_from_outbound_packet(packet_info);
	}
}

void
StatCollector::record_ncp_state_change(NCPState new_ncp_state)
{
	NcpStateInfo ncp_state_info;
	ncp_state_info.update(new_ncp_state);
	mNCPStateHistory.force_write(ncp_state_info);
}

void
StatCollector::record_ncp_ready_for_host_sleep_state(bool ready_for_sleep_state)
{
	ReadyForHostSleepState new_state;

	if (mLastReadyForHostSleepState == ready_for_sleep_state) {
		return;
	}

	if (ready_for_sleep_state) {
		new_state.update_with_blocking_sleep_time(mLastBlockingHostSleepTime);

		mReadyForSleepHistory.force_write(new_state);
	} else {
		mLastBlockingHostSleepTime.set_to_now();
	}

	mLastReadyForHostSleepState = ready_for_sleep_state;
}

void
StatCollector::add_tx_history(StringList& output, int count) const
{
	if (count == 0) {
		count = mTxHistory.size();
	}

	if (!mTxHistory.empty()) {
		RingBuffer<PacketInfo, STAT_COLLECTOR_TX_HISTORY_SIZE>::ReverseIterator iter;

		output.push_back("Tx History");
		output.push_back("-------------------------");
		for (iter = mTxHistory.rbegin(); (iter != mTxHistory.rend()) && (count != 0); ++iter, --count) {
			output.push_back(iter->to_string());
		}
	} else  {
		output.push_back("Tx history is empty");
	}
}

void
StatCollector::add_rx_history(StringList& output, int count) const
{
	if (count == 0) {
		count = mRxHistory.size();
	}

	if (!mRxHistory.empty()) {
		RingBuffer<PacketInfo, STAT_COLLECTOR_RX_HISTORY_SIZE>::ReverseIterator iter;

		output.push_back("Rx History");
		output.push_back("-------------------------");
		for (iter = mRxHistory.rbegin(); (iter != mRxHistory.rend()) && (count != 0); ++iter, --count) {
			output.push_back(iter->to_string());
		}
	} else  {
		output.push_back("Rx history is empty");
	}
}

void
StatCollector::add_ncp_state_history(StringList& output, int count) const
{
	if (count == 0) {
		count = mNCPStateHistory.size();
	}

	if(!mNCPStateHistory.empty()) {
		RingBuffer<NcpStateInfo, STAT_COLLECTOR_NCP_STATE_HISTORY_SIZE>::ReverseIterator iter;

		output.push_back("NCP State History");
		output.push_back("-------------------------");
		for (iter = mNCPStateHistory.rbegin(); (iter != mNCPStateHistory.rend()) && (count != 0); ++iter, --count) {
			output.push_back(iter->to_string());
		}
	} else {
		output.push_back("NCP state history is empty.");
	}
}

void
StatCollector::add_ncp_ready_for_host_sleep_state_history(StringList& output, int count) const
{
	if (count == 0) {
		count = mReadyForSleepHistory.size();
	}

	if (!mReadyForSleepHistory.empty() || (mLastReadyForHostSleepState == false)) {
		RingBuffer<ReadyForHostSleepState, STAT_COLLECTOR_NCP_READY_FOR_HOST_SLEEP_STATE_HISTORY_SIZE>::ReverseIterator iter;

		output.push_back("\'NCP Ready For Host Sleep State\' History");
		output.push_back("-------------------------");

		if (mLastReadyForHostSleepState == false) {
			output.push_back( mLastBlockingHostSleepTime.to_string() + " host sleep was blocked till now");
		}

		for (iter = mReadyForSleepHistory.rbegin(); (iter != mReadyForSleepHistory.rend()) && (count != 0); ++iter, --count) {
			output.push_back(iter->to_string());
		}
	} else {
		output.push_back("\'NCP Ready For Host Sleep State\' history is empty.");
	}
}

void
StatCollector::add_tx_stat(StringList& output) const
{
	output.push_back (
		string_printf("Tx: %d packet%s (%d udp, %d tcp, %d icmp6) -- ",
			mTxPacketsTotal, (mTxPacketsTotal == 1)? "" : "s",
			mTxPacketsUDP,
			mTxPacketsTCP,
			mTxPacketsICMP
		) +
		mTxBytesTotal.to_string()
	);
}

void
StatCollector::add_rx_stat(StringList& output) const
{
	output.push_back (
		string_printf("Rx: %d packet%s (%d udp, %d tcp, %d icmp6) -- ",
			mRxPacketsTotal, (mRxPacketsTotal == 1)? "" : "s",
			mRxPacketsUDP,
			mRxPacketsTCP,
			mRxPacketsICMP
		) +
		mRxBytesTotal.to_string()
	);
}

void
StatCollector::add_all_info(StringList& output, int count) const
{
	add_tx_stat(output);
	add_tx_history(output, count);

	output.push_back("");

	add_rx_stat(output);
	add_rx_history(output, count);

	output.push_back("");

	add_ncp_state_history(output, count);

	output.push_back("");

	if (count == 0) {
		mNodeStat.add_node_stat_history(output);
	} else {
		mNodeStat.add_node_stat(output);
	}

	output.push_back("");
	if (count == 0) {
		mLinkStat.add_link_stat(output);
	} else {
		mLinkStat.add_link_stat(output, STAT_COLLECTOR_LINK_STAT_HISTORY_SIZE);
	}
}

bool
StatCollector::is_a_stat_property(const std::string& key)
{
	// Check for the prefix to match
	return strncaseequal(key.c_str(), kWPANTUNDProperty_Stat_Prefix , sizeof(kWPANTUNDProperty_Stat_Prefix) - 1);
}

void
StatCollector::add_help(StringList& output) const
{
	output.push_back("List of statistics properties");
	output.push_back(string_printf("\t %-26s - RX statistics (all nodes)", kWPANTUNDProperty_StatRX));
	output.push_back(string_printf("\t %-26s - TX statistics (all nodes)", kWPANTUNDProperty_StatTX));
	output.push_back(string_printf("\t %-26s - RX packet info history (all nodes)", kWPANTUNDProperty_StatRXHistory));
	output.push_back(string_printf("\t %-26s - TX packet info history (all nodes)", kWPANTUNDProperty_StatTXHistory));
	output.push_back(string_printf("\t %-26s - Both RX & TX packet info history (all nodes)", kWPANTUNDProperty_StatHistory));
	output.push_back(string_printf("\t %-26s - NCP state change history", kWPANTUNDProperty_StatNCP));
	output.push_back(string_printf("\t %-26s - \'Blocking Host Sleep\' state change history", kWPANTUNDProperty_StatBlockingHostSleep));
	output.push_back(string_printf("\t %-26s - List of nodes + RX/TX statistics per node", kWPANTUNDProperty_StatNode));
	output.push_back(string_printf("\t %-26s - List of nodes + RX/TX statistics and packet history per node", kWPANTUNDProperty_StatNodeHistory));
	output.push_back(string_printf("\t %-26s - List of nodes + RX/TX statistics and packet history for a specific node with given IP address", kWPANTUNDProperty_StatNodeHistoryID "[<ipv6>]"));
	output.push_back(string_printf("\t %-26s - List of nodes + RX/TX statistics and packet history for a specific node with given index", kWPANTUNDProperty_StatNodeHistoryID "<index>"));
	output.push_back(string_printf("\t %-26s - Peer link quality history - short version", kWPANTUNDProperty_StatLinkQualityShort));
	output.push_back(string_printf("\t %-26s - Peer link quality history - long version", kWPANTUNDProperty_StatLinkQualityLong));
	output.push_back(string_printf("\t %-26s - All info - short version", kWPANTUNDProperty_StatShort));
	output.push_back(string_printf("\t %-26s - All info - long version", kWPANTUNDProperty_StatLong));
	output.push_back(string_printf("\t "));
	output.push_back(string_printf("\t %-26s - Peer link quality information - get only", kWPANTUNDProperty_StatLinkQuality));
	output.push_back(string_printf("\t %-26s - Period interval (in seconds) for collecting peer link quality - get/set - zero to disable", kWPANTUNDProperty_StatLinkQualityPeriod));
	output.push_back(string_printf("\t %-26s - AutoLog information - get only", kWPANTUNDProperty_StatAutoLog));
	output.push_back(string_printf("\t %-26s - AutoLog state (\'disabled\',\'long\',\'short\'') - get/set", kWPANTUNDProperty_StatAutoLogState));
	output.push_back(string_printf("\t %-26s - AutoLog period in minutes - get/set", kWPANTUNDProperty_StatAutoLogPeriod));
	output.push_back(string_printf("\t %-26s - AutoLog log level - get/set", kWPANTUNDProperty_StatAutoLogLogLevel));
	output.push_back(string_printf("\t %-26s - Log level for user requested logs - get/set", kWPANTUNDProperty_StatUserLogRequestLogLevel));
    output.push_back(string_printf("\t %-26s : \'emerg\', \'alert\', \'crit\', \'err\', \'warning\', \'notice\', \'info\', \'debug\'","Valid log levels"));
    output.push_back(string_printf("\t "));
	output.push_back(string_printf("\t %-26s - Print this help", kWPANTUNDProperty_StatHelp));
}

int
StatCollector::get_stat_property(const std::string& key, StringList& output) const
{
	int return_status = kWPANTUNDStatus_Ok;

	if (strcaseequal(key.c_str(), kWPANTUNDProperty_StatShort)) {
		add_all_info(output, STAT_COLLECTOR_SHORT_HISTORY_COUNT);
	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_StatLong)) {
		add_all_info(output);
	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_StatRX)) {
		add_rx_stat(output);
	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_StatTX)) {
		add_tx_stat(output);
	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_StatRXHistory)) {
		add_rx_stat(output);
		add_rx_history(output);
	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_StatTXHistory)) {
		add_tx_stat(output);
		add_tx_history(output);
	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_StatHistory)) {
		add_rx_history(output);
		output.push_back("");
		add_tx_history(output);
	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_StatNCP)) {
		add_ncp_state_history(output);
	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_StatBlockingHostSleep)) {
		add_ncp_ready_for_host_sleep_state_history(output);
	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_StatNode)) {
		mNodeStat.add_node_stat(output);
	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_StatNodeHistory)) {
		mNodeStat.add_node_stat_history(output);
	} else if (strncaseequal(key.c_str(), kWPANTUNDProperty_StatNodeHistoryID, sizeof(kWPANTUNDProperty_StatNodeHistoryID) - 1)) {
		mNodeStat.add_node_stat_history(output, key.substr(sizeof(kWPANTUNDProperty_StatNodeHistoryID) - 1));
	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_StatLinkQualityLong)) {
		mLinkStat.add_link_stat(output);
	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_StatLinkQualityShort)) {
		mLinkStat.add_link_stat(output, STAT_COLLECTOR_LINK_STAT_HISTORY_SIZE);
	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_StatHelp)) {
		add_help(output);
	} else {
		output.push_back(std::string("Unknown/unsupported stat property. Please use \"get ") + kWPANTUNDProperty_StatHelp +
			"\" to get list of supported properties by statistics collector." );
		return_status = kWPANTUNDStatus_PropertyNotFound;
	}

	return return_status;
}

void
StatCollector::property_get_value(const std::string& key, CallbackWithStatusArg1 cb)
{
	// First check for AutoLog properties.
	if (strcaseequal(key.c_str(), kWPANTUNDProperty_StatAutoLog)) {
		std::string str;
		switch(mAutoLogState) {
			case kAutoLogDisabled:
				str = "Auto stat log is disabled.";
				break;

			case kAutoLogLong:
				str = string_printf("Auto stat log is enabled using long version every %d min at log level \'%s\'.",
					mAutoLogPeriod / Timer::kOneMinute, log_level_to_string(mAutoLogLevel).c_str());
				break;

			case kAutoLogShort:
				str = string_printf("Auto stat log is enabled using short version of stat every %d min at log level \'%s\'.",
					mAutoLogPeriod / Timer::kOneMinute, log_level_to_string(mAutoLogLevel).c_str());
				break;
		}
		cb(kWPANTUNDStatus_Ok, boost::any(str));

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_StatAutoLogState)) {
		std::string state;

		switch (mAutoLogState) {
			case kAutoLogDisabled:  state = kWPANTUNDStatAutoLogState_Disabled; break;
			case kAutoLogShort:     state = kWPANTUNDStatAutoLogState_Short;    break;
			case kAutoLogLong:      state = kWPANTUNDStatAutoLogState_Long;     break;
		}
		cb(kWPANTUNDStatus_Ok, boost::any(state));

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_StatAutoLogPeriod)) {
		int period_in_min = mAutoLogPeriod / Timer::kOneMinute;
		cb(kWPANTUNDStatus_Ok, boost::any(period_in_min));

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_StatAutoLogLogLevel)) {
		cb(kWPANTUNDStatus_Ok, boost::any(log_level_to_string(mAutoLogLevel)));

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_StatUserLogRequestLogLevel)) {
		cb(kWPANTUNDStatus_Ok, boost::any(log_level_to_string(mUserRequestLogLevel)));

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_StatLinkQuality)) {
		int period_in_sec = static_cast<int>(mLinkStatTimer.get_interval() / Timer::kOneSecond);
		std::string str;

		if (period_in_sec == 0) {
			str = "Periodic query of peer link quality is disabled";
		} else {
			str = string_printf("Peer link quality is collected every %d second%s",
				period_in_sec, (period_in_sec == 1)? "" : "s");
		}
		cb(kWPANTUNDStatus_Ok, boost::any(str));

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_StatLinkQualityPeriod)) {
		int period_in_sec = static_cast<int>(mLinkStatTimer.get_interval() / Timer::kOneSecond);
		cb(kWPANTUNDStatus_Ok, boost::any(period_in_sec));

	} else {
		// If not an AutoLog property, check for the stat properties.
		StringList output;
		int status = get_stat_property(key, output);

		if (status == kWPANTUNDStatus_Ok) {
			cb(status, boost::any(output));
		} else {
			std::string err_str;
			err_str = std::string("Unknown stat property. Please use \"get ") + kWPANTUNDProperty_StatHelp
				+ "\" to get help about StatCollector.";
			cb(status, boost::any(err_str));
		}
	}
}

void
StatCollector::property_set_value(const std::string& key, const boost::any& value, CallbackWithStatus cb)
{
	int status = kWPANTUNDStatus_Ok;

	// First check for AutoLog properties.
	if (strcaseequal(key.c_str(), kWPANTUNDProperty_StatAutoLogState)) {
		std::string new_state_str = any_to_string(value);
		AutoLogState new_state;

		if (strcaseequal(new_state_str.c_str(), kWPANTUNDStatAutoLogState_Disabled) ||
		    strcaseequal(new_state_str.c_str(), "off") ||
		    strcaseequal(new_state_str.c_str(), "no") ||
		    strcaseequal(new_state_str.c_str(), "0") )
		{
			new_state = kAutoLogDisabled;
		} else if (
		    strcaseequal(new_state_str.c_str(), kWPANTUNDStatAutoLogState_Short) ||
		    strcaseequal(new_state_str.c_str(), "on") ||
		    strcaseequal(new_state_str.c_str(), "yes") ||
		    strcaseequal(new_state_str.c_str(), "1") )
		{
			new_state = kAutoLogShort;
		} else if (strcaseequal(new_state_str.c_str(), kWPANTUNDStatAutoLogState_Long)) {
			new_state = kAutoLogLong;
		} else {
			status = kWPANTUNDStatus_InvalidArgument;
		}

		if (status == kWPANTUNDStatus_Ok) {
			if (new_state != mAutoLogState) {
				mAutoLogState = new_state;
				update_auto_log_timer();
			}
		}
	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_StatAutoLogPeriod)) {
		int period_in_min = any_to_int(value);
		if ((period_in_min > 0) && (period_in_min <= STAT_COLLECTOR_AUTO_LOG_MAX_PERIOD)) {
			mAutoLogPeriod = period_in_min * Timer::kOneMinute;
			update_auto_log_timer();
		} else {
			status = kWPANTUNDStatus_InvalidArgument;
		}
	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_StatAutoLogLogLevel)) {
		int log_level = log_level_from_string(any_to_string(value).c_str());
		if (log_level >= 0) {
			status = kWPANTUNDStatus_Ok;
			mAutoLogLevel = log_level;
		} else {
			status = kWPANTUNDStatus_InvalidArgument;
		}
	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_StatUserLogRequestLogLevel)) {
		int log_level = log_level_from_string(any_to_string(value).c_str());
		if (log_level >= 0) {
			status = kWPANTUNDStatus_Ok;
			mUserRequestLogLevel = log_level;
		} else {
			status = kWPANTUNDStatus_InvalidArgument;
		}
	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_StatLinkQualityPeriod)) {
		int period_in_sec = any_to_int(value);
		if (period_in_sec >= 0) {
			update_link_stat_timer(period_in_sec * Timer::kOneSecond);
		}
	} else {
		StringList output;

		status = get_stat_property(key, output);

		if (status == kWPANTUNDStatus_Ok) {
			for(StringList::iterator it = output.begin(); it != output.end(); ++it) {
				syslog(mUserRequestLogLevel, "Stat: %s", it->c_str());
			}
		}
	}

	cb(status);
}

void
StatCollector::update_auto_log_timer(void)
{
	if (mAutoLogState == kAutoLogDisabled) {
		mAutoLogTimer.cancel();
	} else {
		mAutoLogTimer.schedule(
			mAutoLogPeriod,
			boost::bind(&StatCollector::auto_log_timer_did_fire, this),
			Timer::kPeriodicFixedDelay
		);

		// Invoke the callback directly for the first iteration
		auto_log_timer_did_fire();
	}
}

void
StatCollector::auto_log_timer_did_fire(void)
{
	StringList output;
	int status = kWPANTUNDStatus_Canceled;

	switch (mAutoLogState) {
		case kAutoLogDisabled:
			mAutoLogTimer.cancel();
			break;

		case kAutoLogLong:
			status = get_stat_property(kWPANTUNDProperty_StatLong, output);
			break;

		case kAutoLogShort:
			status = get_stat_property(kWPANTUNDProperty_StatShort, output);
			break;
	}

	if (status == kWPANTUNDStatus_Ok) {
		for(StringList::iterator it = output.begin(); it != output.end(); ++it) {
			syslog(mAutoLogLevel, "Stat (autolog): %s", it->c_str());
		}
	}
}

void
StatCollector::update_link_stat_timer(Timer::Interval interval)
{
	if (interval == 0) {
		mLinkStatTimer.cancel();
	} else {
		mLinkStatTimer.schedule(
			interval,
			boost::bind(&StatCollector::link_stat_timer_did_fire, this),
			Timer::kPeriodicFixedDelay
		);

		// Invoke the callback directly for the first iteration
		link_stat_timer_did_fire();
	}
}

void
StatCollector::link_stat_timer_did_fire(void)
{
}

void
StatCollector::did_get_rip_entry_value_map(int status, const boost::any& value)
{
	if (status == kWPANTUNDStatus_Ok) {
		if (value.type() == typeid(std::list<ValueMap>)) {
			std::list<ValueMap> rip_entry_list;
			std::list<ValueMap>::const_iterator it;

			rip_entry_list = boost::any_cast< std::list<ValueMap> >(value);

			for (it = rip_entry_list.begin(); it != rip_entry_list.end(); ++it) {
				record_rip_entry(*it);
			}
		}
	}
}

int
StatCollector::record_rip_entry(const ValueMap& rip_entry)
{
	ValueMap::const_iterator it;
	Data eui64;
	int8_t rssi = 0;
	uint8_t in_lqi = 0;
	uint8_t out_lqi = 0;
	NodeType node_type = UNKNOWN;


	mLinkStat.update(eui64.data(), rssi, in_lqi, out_lqi, node_type);

	return kWPANTUNDStatus_Ok;
}

void
StatCollector::property_changed(const std::string& key, const boost::any& value)
{
	if (strcaseequal(key.c_str(), kWPANTUNDProperty_NCPState)) {
		record_ncp_state_change(string_to_ncp_state(any_to_string(value)));
	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_DaemonReadyForHostSleep)) {
		record_ncp_ready_for_host_sleep_state(any_to_bool(value));
	}
}

void
StatCollector::did_rx_net_scan_beacon(const WPAN::NetworkInstance& network)
{
	// Log the scan result
	syslog(LOG_NOTICE,
	    "Scan -> "
	    "Name:%-17s, "
		"PanId:0x%04X, "
		"Ch:%2d, "
		"Joinable:%-3s, "
		"XPanId:0x%02X%02X%02X%02X%02X%02X%02X%02X, "
		"HwAddr:0x%02X%02X%02X%02X%02X%02X%02X%02X, "
		"RSSI:%-4d, "
		"LQI:%-3d, "
		"ProtoId:%-3d, "
		"Version:%2d, "
		"ShortAddr:0x%04X ",

		network.name.c_str(),
		network.panid,
		network.channel,
		network.joinable? "YES" : "NO",
		network.xpanid[0], network.xpanid[1], network.xpanid[2], network.xpanid[3],
		network.xpanid[4], network.xpanid[5], network.xpanid[6], network.xpanid[7],
		network.hwaddr[0], network.hwaddr[1], network.hwaddr[2], network.hwaddr[3],
		network.hwaddr[4], network.hwaddr[5], network.hwaddr[6], network.xpanid[7],
		network.rssi,
		network.lqi,
		network.type,
		network.version,
		network.saddr
	);
}
