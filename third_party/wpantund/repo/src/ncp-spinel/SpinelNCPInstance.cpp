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

#include "SpinelNCPInstance.h"
#include "time-utils.h"
#include "assert-macros.h"
#include <syslog.h>
#include <errno.h>
#include "socket-utils.h"
#include <stdexcept>
#include <sys/file.h>
#include "SuperSocket.h"
#include "SpinelNCPTask.h"
#include "SpinelNCPTaskWake.h"
#include "SpinelNCPTaskSendCommand.h"
#include "SpinelNCPTaskJoin.h"
#include "SpinelNCPTaskGetNetworkTopology.h"
#include "SpinelNCPTaskGetMsgBufferCounters.h"
#include "any-to.h"
#include "spinel-extra.h"

#define kWPANTUNDProperty_Spinel_CounterPrefix		"NCP:Counter:"

#define kWPANTUND_Whitelist_RssiOverrideDisabled    127

using namespace nl;
using namespace wpantund;

WPANTUND_DEFINE_NCPINSTANCE_PLUGIN(spinel, SpinelNCPInstance);

void
SpinelNCPInstance::handle_ncp_log(const uint8_t* data_ptr, int data_len)
{
	static char linebuffer[NCP_DEBUG_LINE_LENGTH_MAX + 1];
	static int linepos = 0;
	while (data_len--) {
		char nextchar = *data_ptr++;

		if ((nextchar == '\t') || (nextchar >= 32)) {
			linebuffer[linepos++] = nextchar;
		}

		if ( (linepos != 0)
		  && ( (nextchar == '\n')
			|| (nextchar == '\r')
			|| (linepos >= (sizeof(linebuffer) - 1))
		  )
		)
		{
			// flush.
			linebuffer[linepos] = 0;
			syslog(LOG_WARNING, "NCP => %s\n", linebuffer);
			linepos = 0;
		}
	}
}

void
SpinelNCPInstance::start_new_task(const boost::shared_ptr<SpinelNCPTask> &task)
{
	if (ncp_state_is_detached_from_ncp(get_ncp_state())) {
		task->finish(kWPANTUNDStatus_InvalidWhenDisabled);
	} else if (PT_SCHEDULE(task->process_event(EVENT_STARTING_TASK))) {

		if (ncp_state_is_sleeping(get_ncp_state())
			&& (dynamic_cast<const SpinelNCPTaskWake*>(task.get()) == NULL)
		) {
			start_new_task(boost::shared_ptr<SpinelNCPTask>(new SpinelNCPTaskWake(this, NilReturn())));
		}
		mTaskQueue.push_back(task);
	}
}

int
nl::wpantund::spinel_status_to_wpantund_status(int spinel_status)
{
	wpantund_status_t ret;
	switch (spinel_status) {
	case SPINEL_STATUS_ALREADY:
		ret = kWPANTUNDStatus_Already;
		break;
	case SPINEL_STATUS_BUSY:
		ret = kWPANTUNDStatus_Busy;
		break;
	case SPINEL_STATUS_IN_PROGRESS:
		ret = kWPANTUNDStatus_InProgress;
		break;
	case SPINEL_STATUS_JOIN_FAILURE:
		ret = kWPANTUNDStatus_JoinFailedUnknown;
		break;
	case SPINEL_STATUS_JOIN_INCOMPATIBLE:
		ret = kWPANTUNDStatus_JoinFailedAtScan;
		break;
	case SPINEL_STATUS_JOIN_SECURITY:
		ret = kWPANTUNDStatus_JoinFailedAtAuthenticate;
		break;
	case SPINEL_STATUS_OK:
		ret = kWPANTUNDStatus_Ok;
		break;
	case SPINEL_STATUS_PROP_NOT_FOUND:
		ret = kWPANTUNDStatus_PropertyNotFound;
		break;
	case SPINEL_STATUS_INVALID_ARGUMENT:
		ret = kWPANTUNDStatus_NCP_InvalidArgument;
		break;
	case SPINEL_STATUS_INVALID_STATE:
		ret = kWPANTUNDStatus_InvalidForCurrentState;
		break;

	default:
		ret = WPANTUND_NCPERROR_TO_STATUS(spinel_status);
		break;
	}

	return ret;
}

int
nl::wpantund::peek_ncp_callback_status(int event, va_list args)
{
	int ret = 0;

	if (EVENT_NCP_PROP_VALUE_IS == event) {
		va_list tmp;
		va_copy(tmp, args);
		unsigned int key = va_arg(tmp, unsigned int);
		if (SPINEL_PROP_LAST_STATUS == key) {
			const uint8_t* spinel_data_ptr = va_arg(tmp, const uint8_t*);
			spinel_size_t spinel_data_len = va_arg(tmp, spinel_size_t);

			if (spinel_datatype_unpack(spinel_data_ptr, spinel_data_len, "i", &ret) <= 0) {
				ret = SPINEL_STATUS_PARSE_ERROR;
			}
		}
		va_end(tmp);
	} else if (EVENT_NCP_RESET == event) {
		va_list tmp;
		va_copy(tmp, args);
		ret = va_arg(tmp, int);
		va_end(tmp);
	}

	return ret;
}

SpinelNCPInstance::SpinelNCPInstance(const Settings& settings) :
	NCPInstanceBase(settings), mControlInterface(this)
{
	mOutboundBufferLen = 0;
	mInboundHeader = 0;
	mSupprotedChannels.clear();

	mSetSteeringDataWhenJoinable = false;
	memset(mSteeringDataAddress, 0xff, sizeof(mSteeringDataAddress));

	mIsPcapInProgress = false;
	mSettings.clear();
	mXPANIDWasExplicitlySet = false;

	if (!settings.empty()) {
		int status;
		Settings::const_iterator iter;

		for(iter = settings.begin(); iter != settings.end(); iter++) {
			if (!NCPInstanceBase::setup_property_supported_by_class(iter->first)) {
				status = static_cast<NCPControlInterface&>(get_control_interface())
					.property_set_value(iter->first, iter->second);

				if (status != 0) {
					syslog(LOG_WARNING, "Attempt to set property \"%s\" failed with err %d", iter->first.c_str(), status);
				}
			}
		}
	}
}

SpinelNCPInstance::~SpinelNCPInstance()
{
}


bool
SpinelNCPInstance::setup_property_supported_by_class(const std::string& prop_name)
{
	return NCPInstanceBase::setup_property_supported_by_class(prop_name);
}

SpinelNCPControlInterface&
SpinelNCPInstance::get_control_interface()
{
	return mControlInterface;
}

uint32_t
SpinelNCPInstance::get_default_channel_mask(void)
{
	uint32_t channel_mask = 0;
	uint16_t i;

	for (i = 0; i < 32; i++) {
		if (mSupprotedChannels.find(i) != mSupprotedChannels.end()) {
			channel_mask |= (1 << i);
		}
	}

	return channel_mask;
}

std::set<std::string>
SpinelNCPInstance::get_supported_property_keys()const
{
	std::set<std::string> properties (NCPInstanceBase::get_supported_property_keys());

	properties.insert(kWPANTUNDProperty_ConfigNCPDriverName);
	properties.insert(kWPANTUNDProperty_NCPChannel);
	properties.insert(kWPANTUNDProperty_NCPChannelMask);
	properties.insert(kWPANTUNDProperty_NCPFrequency);
	properties.insert(kWPANTUNDProperty_NCPRSSI);
	properties.insert(kWPANTUNDProperty_NCPExtendedAddress);

	if (mCapabilities.count(SPINEL_CAP_NET_THREAD_1_0)) {
		properties.insert(kWPANTUNDProperty_ThreadRLOC16);
		properties.insert(kWPANTUNDProperty_ThreadRouterID);
		properties.insert(kWPANTUNDProperty_ThreadLeaderAddress);
		properties.insert(kWPANTUNDProperty_ThreadLeaderRouterID);
		properties.insert(kWPANTUNDProperty_ThreadLeaderWeight);
		properties.insert(kWPANTUNDProperty_ThreadLeaderLocalWeight);
		properties.insert(kWPANTUNDProperty_ThreadNetworkData);
		properties.insert(kWPANTUNDProperty_ThreadNetworkDataVersion);
		properties.insert(kWPANTUNDProperty_ThreadStableNetworkData);
		properties.insert(kWPANTUNDProperty_ThreadStableNetworkDataVersion);
		properties.insert(kWPANTUNDProperty_ThreadLeaderNetworkData);
		properties.insert(kWPANTUNDProperty_ThreadStableLeaderNetworkData);
		properties.insert(kWPANTUNDProperty_ThreadChildTable);
		properties.insert(kWPANTUNDProperty_ThreadNeighborTable);
		properties.insert(kWPANTUNDProperty_ThreadCommissionerEnabled);
		properties.insert(kWPANTUNDProperty_ThreadOffMeshRoutes);
	}

	if (mCapabilities.count(SPINEL_CAP_COUNTERS)) {
		properties.insert(kWPANTUNDProperty_Spinel_CounterPrefix "TX_PKT_TOTAL");
		properties.insert(kWPANTUNDProperty_Spinel_CounterPrefix "TX_PKT_UNICAST");
		properties.insert(kWPANTUNDProperty_Spinel_CounterPrefix "TX_PKT_BROADCAST");
		properties.insert(kWPANTUNDProperty_Spinel_CounterPrefix "TX_PKT_ACK_REQ");
		properties.insert(kWPANTUNDProperty_Spinel_CounterPrefix "TX_PKT_ACKED");
		properties.insert(kWPANTUNDProperty_Spinel_CounterPrefix "TX_PKT_NO_ACK_REQ");
		properties.insert(kWPANTUNDProperty_Spinel_CounterPrefix "TX_PKT_DATA");
		properties.insert(kWPANTUNDProperty_Spinel_CounterPrefix "TX_PKT_DATA_POLL");
		properties.insert(kWPANTUNDProperty_Spinel_CounterPrefix "TX_PKT_BEACON");
		properties.insert(kWPANTUNDProperty_Spinel_CounterPrefix "TX_PKT_BEACON_REQ");
		properties.insert(kWPANTUNDProperty_Spinel_CounterPrefix "TX_PKT_OTHER");
		properties.insert(kWPANTUNDProperty_Spinel_CounterPrefix "TX_PKT_RETRY");
		properties.insert(kWPANTUNDProperty_Spinel_CounterPrefix "TX_ERR_CCA");
		properties.insert(kWPANTUNDProperty_Spinel_CounterPrefix "TX_ERR_ABORT");
		properties.insert(kWPANTUNDProperty_Spinel_CounterPrefix "RX_PKT_TOTAL");
		properties.insert(kWPANTUNDProperty_Spinel_CounterPrefix "RX_PKT_UNICAST");
		properties.insert(kWPANTUNDProperty_Spinel_CounterPrefix "RX_PKT_BROADCAST");
		properties.insert(kWPANTUNDProperty_Spinel_CounterPrefix "RX_PKT_DATA");
		properties.insert(kWPANTUNDProperty_Spinel_CounterPrefix "RX_PKT_DATA_POLL");
		properties.insert(kWPANTUNDProperty_Spinel_CounterPrefix "RX_PKT_BEACON");
		properties.insert(kWPANTUNDProperty_Spinel_CounterPrefix "RX_PKT_BEACON_REQ");
		properties.insert(kWPANTUNDProperty_Spinel_CounterPrefix "RX_PKT_OTHER");
		properties.insert(kWPANTUNDProperty_Spinel_CounterPrefix "RX_PKT_FILT_WL");
		properties.insert(kWPANTUNDProperty_Spinel_CounterPrefix "RX_PKT_FILT_DA");
		properties.insert(kWPANTUNDProperty_Spinel_CounterPrefix "RX_ERR_EMPTY");
		properties.insert(kWPANTUNDProperty_Spinel_CounterPrefix "RX_ERR_UKWN_NBR");
		properties.insert(kWPANTUNDProperty_Spinel_CounterPrefix "RX_ERR_NVLD_SADDR");
		properties.insert(kWPANTUNDProperty_Spinel_CounterPrefix "RX_ERR_SECURITY");
		properties.insert(kWPANTUNDProperty_Spinel_CounterPrefix "RX_ERR_BAD_FCS");
		properties.insert(kWPANTUNDProperty_Spinel_CounterPrefix "RX_ERR_OTHER");
		properties.insert(kWPANTUNDProperty_Spinel_CounterPrefix "TX_IP_SEC_TOTAL");
		properties.insert(kWPANTUNDProperty_Spinel_CounterPrefix "TX_IP_INSEC_TOTAL");
		properties.insert(kWPANTUNDProperty_Spinel_CounterPrefix "TX_IP_DROPPED");
		properties.insert(kWPANTUNDProperty_Spinel_CounterPrefix "RX_IP_SEC_TOTAL");
		properties.insert(kWPANTUNDProperty_Spinel_CounterPrefix "RX_IP_INSEC_TOTAL");
		properties.insert(kWPANTUNDProperty_Spinel_CounterPrefix "RX_IP_DROPPED");
		properties.insert(kWPANTUNDProperty_Spinel_CounterPrefix "TX_SPINEL_TOTAL");
		properties.insert(kWPANTUNDProperty_Spinel_CounterPrefix "RX_SPINEL_TOTAL");
		properties.insert(kWPANTUNDProperty_Spinel_CounterPrefix "RX_SPINEL_ERR");
	}

	if (mCapabilities.count(SPINEL_CAP_MAC_WHITELIST)) {
		properties.insert(kWPANTUNDProperty_MACWhitelistEnabled);
		properties.insert(kWPANTUNDProperty_MACWhitelistEntries);
	}

	if (mCapabilities.count(SPINEL_CAP_JAM_DETECT)) {
		properties.insert(kWPANTUNDProperty_JamDetectionStatus);
		properties.insert(kWPANTUNDProperty_JamDetectionEnable);
		properties.insert(kWPANTUNDProperty_JamDetectionRssiThreshold);
		properties.insert(kWPANTUNDProperty_JamDetectionWindow);
		properties.insert(kWPANTUNDProperty_JamDetectionBusyPeriod);
		properties.insert(kWPANTUNDProperty_JamDetectionDebugHistoryBitmap);
	}

	if (mCapabilities.count(SPINEL_CAP_THREAD_TMF_PROXY)) {
		properties.insert(kWPANTUNDProperty_TmfProxyEnabled);
	}

	if (mCapabilities.count(SPINEL_CAP_NEST_LEGACY_INTERFACE))
	{
		properties.insert(kWPANTUNDProperty_NestLabs_LegacyMeshLocalPrefix);
	}

	return properties;
}

cms_t
SpinelNCPInstance::get_ms_to_next_event(void)
{
	cms_t cms = NCPInstanceBase::get_ms_to_next_event();

	if (ncp_state_is_detached_from_ncp(get_ncp_state())) {
		return CMS_DISTANT_FUTURE;
	}

	// If the control protothread hasn't even started, set cms to zero.
	if (0 == mControlPT.lc) {
		cms = 0;
	}

	if (!mTaskQueue.empty()) {
		int tmp_cms = mTaskQueue.front()->get_ms_to_next_event();
		if (tmp_cms < cms) {
			cms = tmp_cms;
		}
	}

	if (cms < 0) {
		cms = 0;
	}

	return cms;
}

static void
convert_rloc16_to_router_id(CallbackWithStatusArg1 cb, int status, const boost::any& value)
{
	uint8_t router_id = 0;

	if (status == kWPANTUNDStatus_Ok) {
		uint16_t rloc16 = any_to_int(value);
		router_id = rloc16 >> 10;
	}
	cb(status, router_id);
}

static int
unpack_mac_whitelist_entries(const uint8_t *data_in, spinel_size_t data_len, boost::any& value, bool as_val_map)
{
	spinel_ssize_t len;
	ValueMap entry;
	std::list<ValueMap> result_as_val_map;
	std::list<std::string> result_as_string;
	const spinel_eui64_t *eui64 = NULL;
	int8_t rssi = 0;

	int ret = kWPANTUNDStatus_Ok;

	while (data_len > 0)
	{
		len = spinel_datatype_unpack(
			data_in,
			data_len,
			SPINEL_DATATYPE_STRUCT_S(
				SPINEL_DATATYPE_EUI64_S   // Extended address
				SPINEL_DATATYPE_INT8_S    // Rssi
			),
			&eui64,
			&rssi
		);

		if (len <= 0)
		{
			ret = kWPANTUNDStatus_Failure;
			break;
		}

		if (as_val_map) {
			entry.clear();
			entry[kWPANTUNDValueMapKey_Whitelist_ExtAddress] = Data(eui64->bytes, sizeof(spinel_eui64_t));

			if (rssi != kWPANTUND_Whitelist_RssiOverrideDisabled) {
				entry[kWPANTUNDValueMapKey_Whitelist_Rssi] = rssi;
			}

			result_as_val_map.push_back(entry);

		} else {
			char c_string[500];
			int index;

			index = snprintf(c_string, sizeof(c_string), "%02X%02X%02X%02X%02X%02X%02X%02X",
							 eui64->bytes[0], eui64->bytes[1], eui64->bytes[2], eui64->bytes[3],
							 eui64->bytes[4], eui64->bytes[5], eui64->bytes[6], eui64->bytes[7]);

			if (rssi != kWPANTUND_Whitelist_RssiOverrideDisabled) {
				if (index >= 0 && index < sizeof(c_string)) {
					snprintf(c_string + index, sizeof(c_string) - index, "   fixed-rssi:%d", rssi);
				}
			}

			result_as_string.push_back(std::string(c_string));
		}

		data_len -= len;
		data_in += len;
	}

	if (ret == kWPANTUNDStatus_Ok) {

		if (as_val_map) {
			value = result_as_val_map;
		} else {
			value = result_as_string;
		}
	}

	return ret;
}

static int
unpack_jam_detect_history_bitmap(const uint8_t *data_in, spinel_size_t data_len, boost::any& value)
{
	spinel_ssize_t len;
	uint32_t lower, higher;
	uint64_t val;
	int ret = kWPANTUNDStatus_Failure;

	len = spinel_datatype_unpack(
		data_in,
		data_len,
		SPINEL_DATATYPE_UINT32_S SPINEL_DATATYPE_UINT32_S,
		&lower,
		&higher
	);

	if (len > 0)
	{
		ret = kWPANTUNDStatus_Ok;
		value = (static_cast<uint64_t>(higher) << 32) + static_cast<uint64_t>(lower);
	}

	return ret;
}

static int
unpack_thread_off_mesh_routes(const uint8_t *data_in, spinel_size_t data_len, boost::any& value)
{

	int ret = kWPANTUNDStatus_Ok;
	std::list<std::string> result;

	while (data_len > 0)
	{
		spinel_ssize_t len;
		struct in6_addr *route_prefix = NULL;
		uint8_t prefix_len;
		bool is_stable;
		uint8_t flags;
		bool is_local;
		bool next_hop_is_this_device;


		len = spinel_datatype_unpack(
			data_in,
			data_len,
			SPINEL_DATATYPE_STRUCT_S(
				SPINEL_DATATYPE_IPv6ADDR_S      // Route Prefix
				SPINEL_DATATYPE_UINT8_S         // Prefix Length (in bits)
				SPINEL_DATATYPE_BOOL_S          // isStable
				SPINEL_DATATYPE_UINT8_S         // Flags
				SPINEL_DATATYPE_BOOL_S          // IsLocal
				SPINEL_DATATYPE_BOOL_S          // NextHopIsThisDevice
			),
			&route_prefix,
			&prefix_len,
			&is_stable,
			&flags,
			&is_local,
			&next_hop_is_this_device
		);

		if (len <= 0) {
			ret = kWPANTUNDStatus_Failure;
			break;
		} else {
			char address_string[INET6_ADDRSTRLEN];
			char c_string[200];
			NCPControlInterface::ExternalRoutePriority priority;

			priority = SpinelNCPControlInterface::convert_flags_to_external_route_priority(flags);

			inet_ntop(AF_INET6,	route_prefix, address_string, sizeof(address_string));

			snprintf(c_string, sizeof(c_string),
				"%s/%d, stable:%s, local:%s, next_hop:%s, priority:%s (flags:0x%02x)",
				address_string,
				prefix_len,
				is_stable ? "yes" : "no",
				is_local ? "yes" : "no",
				next_hop_is_this_device ? "this_device" : "off-mesh",
				SpinelNCPControlInterface::external_route_priority_to_string(priority).c_str(),
				flags
			);

			result.push_back(c_string);
		}

		data_in += len;
		data_len -= len;
	}

	if (ret == kWPANTUNDStatus_Ok) {
		value = result;
	}

	return ret;
}

void
SpinelNCPInstance::property_get_value(
	const std::string& key,
	CallbackWithStatusArg1 cb
) {
	if (!is_initializing_ncp()) {
		syslog(LOG_INFO, "property_get_value: key: \"%s\"", key.c_str());
	}

#define SIMPLE_SPINEL_GET(prop__, type__)                                \
	start_new_task(SpinelNCPTaskSendCommand::Factory(this)               \
		.set_callback(cb)                                                \
		.add_command(                                                    \
			SpinelPackData(SPINEL_FRAME_PACK_CMD_PROP_VALUE_GET, prop__) \
		)                                                                \
		.set_reply_format(type__)                                        \
		.finish()                                                        \
	)

	if (strcaseequal(key.c_str(), kWPANTUNDProperty_ConfigNCPDriverName)) {
		cb(0, boost::any(std::string("spinel")));

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NCPChannelMask)) {
		cb(0, boost::any(get_default_channel_mask()));

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NCPCCAThreshold)) {
		SIMPLE_SPINEL_GET(SPINEL_PROP_PHY_CCA_THRESHOLD, SPINEL_DATATYPE_INT8_S);

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NCPTXPower)) {
		SIMPLE_SPINEL_GET(SPINEL_PROP_PHY_TX_POWER, SPINEL_DATATYPE_INT8_S);

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NCPFrequency)) {
		SIMPLE_SPINEL_GET(SPINEL_PROP_PHY_FREQ, SPINEL_DATATYPE_INT32_S);

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NetworkKey)) {
		SIMPLE_SPINEL_GET(SPINEL_PROP_NET_MASTER_KEY, SPINEL_DATATYPE_DATA_S);

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NetworkPSKc)) {
		SIMPLE_SPINEL_GET(SPINEL_PROP_NET_PSKC, SPINEL_DATATYPE_DATA_S);

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NCPExtendedAddress)) {
		SIMPLE_SPINEL_GET(SPINEL_PROP_MAC_EXTENDED_ADDR, SPINEL_DATATYPE_EUI64_S);

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NetworkKeyIndex)) {
		SIMPLE_SPINEL_GET(SPINEL_PROP_NET_KEY_SEQUENCE_COUNTER, SPINEL_DATATYPE_UINT32_S);

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NetworkIsCommissioned)) {
		SIMPLE_SPINEL_GET(SPINEL_PROP_NET_SAVED, SPINEL_DATATYPE_BOOL_S);

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NetworkRole)) {
		SIMPLE_SPINEL_GET(SPINEL_PROP_NET_ROLE, SPINEL_DATATYPE_UINT8_S);

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NetworkPartitionId)) {
		SIMPLE_SPINEL_GET(SPINEL_PROP_NET_PARTITION_ID, SPINEL_DATATYPE_UINT32_S);

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NCPRSSI)) {
		SIMPLE_SPINEL_GET(SPINEL_PROP_PHY_RSSI, SPINEL_DATATYPE_INT8_S);

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_ThreadRLOC16)) {
		SIMPLE_SPINEL_GET(SPINEL_PROP_THREAD_RLOC16, SPINEL_DATATYPE_UINT16_S);

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_ThreadRouterID)) {
		cb = boost::bind(convert_rloc16_to_router_id, cb, _1, _2);
		SIMPLE_SPINEL_GET(SPINEL_PROP_THREAD_RLOC16, SPINEL_DATATYPE_UINT16_S);

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_ThreadLeaderAddress)) {
		SIMPLE_SPINEL_GET(SPINEL_PROP_THREAD_LEADER_ADDR, SPINEL_DATATYPE_IPv6ADDR_S);

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_ThreadLeaderRouterID)) {
		SIMPLE_SPINEL_GET(SPINEL_PROP_THREAD_LEADER_RID, SPINEL_DATATYPE_UINT8_S);

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_ThreadLeaderWeight)) {
		SIMPLE_SPINEL_GET(SPINEL_PROP_THREAD_LEADER_WEIGHT, SPINEL_DATATYPE_UINT8_S);

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_ThreadLeaderLocalWeight)) {
		SIMPLE_SPINEL_GET(SPINEL_PROP_THREAD_LOCAL_LEADER_WEIGHT, SPINEL_DATATYPE_UINT8_S);

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_ThreadNetworkData)) {
		SIMPLE_SPINEL_GET(SPINEL_PROP_THREAD_NETWORK_DATA, SPINEL_DATATYPE_DATA_S);

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_ThreadNetworkDataVersion)) {
		SIMPLE_SPINEL_GET(SPINEL_PROP_THREAD_NETWORK_DATA_VERSION, SPINEL_DATATYPE_UINT8_S);

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_ThreadStableNetworkData)) {
		SIMPLE_SPINEL_GET(SPINEL_PROP_THREAD_STABLE_NETWORK_DATA, SPINEL_DATATYPE_DATA_S);

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_ThreadLeaderNetworkData)) {
		SIMPLE_SPINEL_GET(SPINEL_PROP_THREAD_LEADER_NETWORK_DATA, SPINEL_DATATYPE_DATA_S);

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_ThreadStableLeaderNetworkData)) {
		SIMPLE_SPINEL_GET(SPINEL_PROP_THREAD_STABLE_LEADER_NETWORK_DATA, SPINEL_DATATYPE_DATA_S);

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_ThreadStableNetworkDataVersion)) {
		SIMPLE_SPINEL_GET(SPINEL_PROP_THREAD_STABLE_NETWORK_DATA_VERSION, SPINEL_DATATYPE_UINT8_S);

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_ThreadOffMeshRoutes)) {
		start_new_task(SpinelNCPTaskSendCommand::Factory(this)
			.set_callback(cb)
			.add_command(
				SpinelPackData(SPINEL_FRAME_PACK_CMD_PROP_VALUE_GET, SPINEL_PROP_THREAD_OFF_MESH_ROUTES)
			)
			.set_reply_unpacker(unpack_thread_off_mesh_routes)
			.finish()
		);

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_ThreadCommissionerEnabled)) {
		SIMPLE_SPINEL_GET(SPINEL_PROP_THREAD_COMMISSIONER_ENABLED, SPINEL_DATATYPE_BOOL_S);
	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_ThreadDeviceMode)) {
		SIMPLE_SPINEL_GET(SPINEL_PROP_THREAD_MODE, SPINEL_DATATYPE_UINT8_S);

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_IPv6MeshLocalPrefix) && !buffer_is_nonzero(mNCPV6Prefix, sizeof(mNCPV6Prefix))) {
		SIMPLE_SPINEL_GET(SPINEL_PROP_IPV6_ML_PREFIX, SPINEL_DATATYPE_IPv6ADDR_S);

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_IPv6MeshLocalAddress) && !buffer_is_nonzero(mNCPV6Prefix, sizeof(mNCPV6Prefix))) {
		SIMPLE_SPINEL_GET(SPINEL_PROP_IPV6_ML_ADDR, SPINEL_DATATYPE_IPv6ADDR_S);

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_IPv6LinkLocalAddress) && !IN6_IS_ADDR_LINKLOCAL(&mNCPLinkLocalAddress)) {
		SIMPLE_SPINEL_GET(SPINEL_PROP_IPV6_LL_ADDR, SPINEL_DATATYPE_IPv6ADDR_S);

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_OpenThreadDebugTestAssert)) {
		SIMPLE_SPINEL_GET(SPINEL_PROP_DEBUG_TEST_ASSERT, SPINEL_DATATYPE_BOOL_S);

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_MACWhitelistEnabled)) {
		if (!mCapabilities.count(SPINEL_CAP_MAC_WHITELIST)) {
			cb(kWPANTUNDStatus_FeatureNotSupported, boost::any(std::string("MAC whitelist feature not supported by NCP")));
		} else {
			SIMPLE_SPINEL_GET(SPINEL_PROP_MAC_WHITELIST_ENABLED, SPINEL_DATATYPE_BOOL_S);
		}

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_MACWhitelistEntries)) {
		if (!mCapabilities.count(SPINEL_CAP_MAC_WHITELIST)) {
			cb(kWPANTUNDStatus_FeatureNotSupported, boost::any(std::string("MAC whitelist feature not supported by NCP")));
		} else {
			start_new_task(SpinelNCPTaskSendCommand::Factory(this)
				.set_callback(cb)
				.add_command(
					SpinelPackData(SPINEL_FRAME_PACK_CMD_PROP_VALUE_GET, SPINEL_PROP_MAC_WHITELIST)
				)
				.set_reply_unpacker(boost::bind(unpack_mac_whitelist_entries, _1, _2, _3, false))
				.finish()
			);
		}

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_MACWhitelistEntriesAsValMap)) {
		if (!mCapabilities.count(SPINEL_CAP_MAC_WHITELIST)) {
			cb(kWPANTUNDStatus_FeatureNotSupported, boost::any(std::string("MAC whitelist feature not supported by NCP")));
		} else {
			start_new_task(SpinelNCPTaskSendCommand::Factory(this)
				.set_callback(cb)
				.add_command(
					SpinelPackData(SPINEL_FRAME_PACK_CMD_PROP_VALUE_GET, SPINEL_PROP_MAC_WHITELIST)
				)
				.set_reply_unpacker(boost::bind(unpack_mac_whitelist_entries, _1, _2, _3, true))
				.finish()
			);
		}

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_JamDetectionStatus)) {
		if (!mCapabilities.count(SPINEL_CAP_JAM_DETECT)) {
			cb(kWPANTUNDStatus_FeatureNotSupported, boost::any(std::string("Jam Detection Feature Not Supported")));
		} else {
			SIMPLE_SPINEL_GET(SPINEL_PROP_JAM_DETECTED, SPINEL_DATATYPE_BOOL_S);
		}

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_TmfProxyEnabled)) {
		SIMPLE_SPINEL_GET(SPINEL_PROP_THREAD_TMF_PROXY_ENABLED, SPINEL_DATATYPE_BOOL_S);

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_JamDetectionEnable)) {
		if (!mCapabilities.count(SPINEL_CAP_JAM_DETECT)) {
			cb(kWPANTUNDStatus_FeatureNotSupported, boost::any(std::string("Jam Detection Feature Not Supported")));
		} else {
			SIMPLE_SPINEL_GET(SPINEL_PROP_JAM_DETECT_ENABLE, SPINEL_DATATYPE_BOOL_S);
		}

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_JamDetectionRssiThreshold)) {
		if (!mCapabilities.count(SPINEL_CAP_JAM_DETECT)) {
			cb(kWPANTUNDStatus_FeatureNotSupported, boost::any(std::string("Jam Detection Feature Not Supported")));
		} else {
			SIMPLE_SPINEL_GET(SPINEL_PROP_JAM_DETECT_RSSI_THRESHOLD, SPINEL_DATATYPE_INT8_S);
		}

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_JamDetectionWindow)) {
		if (!mCapabilities.count(SPINEL_CAP_JAM_DETECT)) {
			cb(kWPANTUNDStatus_FeatureNotSupported, boost::any(std::string("Jam Detection Feature Not Supported")));
		} else {
			SIMPLE_SPINEL_GET(SPINEL_PROP_JAM_DETECT_WINDOW, SPINEL_DATATYPE_UINT8_S);
		}

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_JamDetectionBusyPeriod)) {
		if (!mCapabilities.count(SPINEL_CAP_JAM_DETECT)) {
			cb(kWPANTUNDStatus_FeatureNotSupported, boost::any(std::string("Jam Detection Feature Not Supported")));
		} else {
			SIMPLE_SPINEL_GET(SPINEL_PROP_JAM_DETECT_BUSY, SPINEL_DATATYPE_UINT8_S);
		}

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_JamDetectionDebugHistoryBitmap)) {
		if (!mCapabilities.count(SPINEL_CAP_JAM_DETECT)) {
			cb(kWPANTUNDStatus_FeatureNotSupported, boost::any(std::string("Jam Detection Feature Not Supported")));
		} else {
			start_new_task(SpinelNCPTaskSendCommand::Factory(this)
				.set_callback(cb)
				.add_command(
					SpinelPackData(SPINEL_FRAME_PACK_CMD_PROP_VALUE_GET, SPINEL_PROP_JAM_DETECT_HISTORY_BITMAP)
				)
				.set_reply_unpacker(unpack_jam_detect_history_bitmap)
				.finish()
			);
		}

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NestLabs_LegacyMeshLocalPrefix)) {
		if (!mCapabilities.count(SPINEL_CAP_NEST_LEGACY_INTERFACE)) {
			cb(kWPANTUNDStatus_FeatureNotSupported, boost::any(std::string("Legacy Capability Not Supported by NCP")));
		} else {
			SIMPLE_SPINEL_GET(SPINEL_PROP_NEST_LEGACY_ULA_PREFIX, SPINEL_DATATYPE_DATA_S);
		}

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_ThreadChildTable)) {
		start_new_task(boost::shared_ptr<SpinelNCPTask>(
			new SpinelNCPTaskGetNetworkTopology(
				this,
				cb,
				SpinelNCPTaskGetNetworkTopology::kChildTable,
				SpinelNCPTaskGetNetworkTopology::kResultFormat_StringArray
			)
		));
	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_ThreadChildTableAsValMap)) {
		start_new_task(boost::shared_ptr<SpinelNCPTask>(
			new SpinelNCPTaskGetNetworkTopology(
				this,
				cb,
				SpinelNCPTaskGetNetworkTopology::kChildTable,
				SpinelNCPTaskGetNetworkTopology::kResultFormat_ValueMapArray
			)
		));

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_ThreadNeighborTable)) {
		start_new_task(boost::shared_ptr<SpinelNCPTask>(
			new SpinelNCPTaskGetNetworkTopology(
				this,
				cb,
				SpinelNCPTaskGetNetworkTopology::kNeighborTable,
				SpinelNCPTaskGetNetworkTopology::kResultFormat_StringArray
			)
		));

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_ThreadNeighborTableAsValMap)) {
		start_new_task(boost::shared_ptr<SpinelNCPTask>(
			new SpinelNCPTaskGetNetworkTopology(
				this,
				cb,
				SpinelNCPTaskGetNetworkTopology::kNeighborTable,
				SpinelNCPTaskGetNetworkTopology::kResultFormat_ValueMapArray
			)
		));

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_OpenThreadMsgBufferCounters)) {
		start_new_task(boost::shared_ptr<SpinelNCPTask>(
			new SpinelNCPTaskGetMsgBufferCounters(
				this,
				cb,
				SpinelNCPTaskGetMsgBufferCounters::kResultFormat_StringArray
			)
		));

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_OpenThreadMsgBufferCountersAsString)) {
		start_new_task(boost::shared_ptr<SpinelNCPTask>(
			new SpinelNCPTaskGetMsgBufferCounters(
				this,
				cb,
				SpinelNCPTaskGetMsgBufferCounters::kResultFormat_String
			)
		));

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_OpenThreadLogLevel)) {
		SIMPLE_SPINEL_GET(SPINEL_PROP_DEBUG_NCP_LOG_LEVEL, SPINEL_DATATYPE_UINT8_S);

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_OpenThreadSteeringDataSetWhenJoinable)) {
		cb(0, boost::any(mSetSteeringDataWhenJoinable));

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_OpenThreadSteeringDataAddress)) {
		cb(0, boost::any(nl::Data(mSteeringDataAddress, sizeof(mSteeringDataAddress))));

	} else if (strncaseequal(key.c_str(), kWPANTUNDProperty_Spinel_CounterPrefix, sizeof(kWPANTUNDProperty_Spinel_CounterPrefix)-1)) {
		int cntr_key = 0;

#define CNTR_KEY(x)	\
	else if (strcaseequal(key.c_str()+sizeof(kWPANTUNDProperty_Spinel_CounterPrefix)-1, # x)) { \
		cntr_key = SPINEL_PROP_CNTR_ ## x; \
	}

		// Check to see if the counter name is an integer.
		cntr_key = (int)strtol(key.c_str()+(int)sizeof(kWPANTUNDProperty_Spinel_CounterPrefix)-1, NULL, 0);

		if ( (cntr_key > 0)
		  && (cntr_key < SPINEL_PROP_CNTR__END-SPINEL_PROP_CNTR__BEGIN)
		) {
			// Counter name was a valid integer. Let's use it.
			cntr_key += SPINEL_PROP_CNTR__BEGIN;
		}

		CNTR_KEY(TX_PKT_TOTAL)
		CNTR_KEY(TX_PKT_UNICAST)
		CNTR_KEY(TX_PKT_BROADCAST)
		CNTR_KEY(TX_PKT_ACK_REQ)
		CNTR_KEY(TX_PKT_ACKED)
		CNTR_KEY(TX_PKT_NO_ACK_REQ)
		CNTR_KEY(TX_PKT_DATA)
		CNTR_KEY(TX_PKT_DATA_POLL)
		CNTR_KEY(TX_PKT_BEACON)
		CNTR_KEY(TX_PKT_BEACON_REQ)
		CNTR_KEY(TX_PKT_OTHER)
		CNTR_KEY(TX_PKT_RETRY)
		CNTR_KEY(TX_ERR_CCA)
		CNTR_KEY(TX_ERR_ABORT)
		CNTR_KEY(RX_PKT_TOTAL)
		CNTR_KEY(RX_PKT_UNICAST)
		CNTR_KEY(RX_PKT_BROADCAST)
		CNTR_KEY(RX_PKT_DATA)
		CNTR_KEY(RX_PKT_DATA_POLL)
		CNTR_KEY(RX_PKT_BEACON)
		CNTR_KEY(RX_PKT_BEACON_REQ)
		CNTR_KEY(RX_PKT_OTHER)
		CNTR_KEY(RX_PKT_FILT_WL)
		CNTR_KEY(RX_PKT_FILT_DA)
		CNTR_KEY(RX_ERR_EMPTY)
		CNTR_KEY(RX_ERR_UKWN_NBR)
		CNTR_KEY(RX_ERR_NVLD_SADDR)
		CNTR_KEY(RX_ERR_SECURITY)
		CNTR_KEY(RX_ERR_BAD_FCS)
		CNTR_KEY(RX_ERR_OTHER)
		CNTR_KEY(TX_IP_SEC_TOTAL)
		CNTR_KEY(TX_IP_INSEC_TOTAL)
		CNTR_KEY(TX_IP_DROPPED)
		CNTR_KEY(RX_IP_SEC_TOTAL)
		CNTR_KEY(RX_IP_INSEC_TOTAL)
		CNTR_KEY(RX_IP_DROPPED)
		CNTR_KEY(TX_SPINEL_TOTAL)
		CNTR_KEY(RX_SPINEL_TOTAL)
		CNTR_KEY(RX_SPINEL_ERR)
		CNTR_KEY(IP_TX_SUCCESS)
		CNTR_KEY(IP_RX_SUCCESS)
		CNTR_KEY(IP_TX_FAILURE)
		CNTR_KEY(IP_RX_FAILURE)

#undef CNTR_KEY

		if (cntr_key != 0) {
			SIMPLE_SPINEL_GET(cntr_key, SPINEL_DATATYPE_UINT32_S);
		} else {
			NCPInstanceBase::property_get_value(key, cb);
		}
	} else {
		NCPInstanceBase::property_get_value(key, cb);
	}
}

void
SpinelNCPInstance::property_set_value(
	const std::string& key,
	const boost::any& value,
	CallbackWithStatus cb
) {
	syslog(LOG_INFO, "property_set_value: key: \"%s\"", key.c_str());

	// If we are disabled, then the only property we
	// are allowed to set is kWPANTUNDProperty_DaemonEnabled.
	if (!mEnabled && !strcaseequal(key.c_str(), kWPANTUNDProperty_DaemonEnabled)) {
		cb(kWPANTUNDStatus_InvalidWhenDisabled);
		return;
	}

	try {
		if (strcaseequal(key.c_str(), kWPANTUNDProperty_NCPChannel)) {
			int channel = any_to_int(value);
			mCurrentNetworkInstance.channel = channel;

			start_new_task(SpinelNCPTaskSendCommand::Factory(this)
				.set_callback(cb)
				.add_command(
					SpinelPackData(SPINEL_FRAME_PACK_CMD_PROP_VALUE_SET(SPINEL_DATATYPE_UINT8_S), SPINEL_PROP_PHY_CHAN, channel)
				)
				.finish()
			);

		} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NCPCCAThreshold)) {
			int cca = any_to_int(value);
			Data command = SpinelPackData(SPINEL_FRAME_PACK_CMD_PROP_VALUE_SET(SPINEL_DATATYPE_INT8_S), SPINEL_PROP_PHY_CCA_THRESHOLD, cca);

			mSettings[kWPANTUNDProperty_NCPCCAThreshold] = SettingsEntry(command);

			start_new_task(SpinelNCPTaskSendCommand::Factory(this)
				.set_callback(cb)
				.add_command(command)
				.finish()
			);

		} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NCPTXPower)) {
			int tx_power = any_to_int(value);
			Data command = SpinelPackData(SPINEL_FRAME_PACK_CMD_PROP_VALUE_SET(SPINEL_DATATYPE_INT8_S), SPINEL_PROP_PHY_TX_POWER, tx_power);

			mSettings[kWPANTUNDProperty_NCPTXPower] = SettingsEntry(command);

			start_new_task(SpinelNCPTaskSendCommand::Factory(this)
				.set_callback(cb)
				.add_command(command)
				.finish()
			);

		} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NetworkPANID)) {
			uint16_t panid = any_to_int(value);

			start_new_task(SpinelNCPTaskSendCommand::Factory(this)
				.set_callback(cb)
				.add_command(
					SpinelPackData(SPINEL_FRAME_PACK_CMD_PROP_VALUE_SET(SPINEL_DATATYPE_UINT16_S), SPINEL_PROP_MAC_15_4_PANID, panid)
				)
				.finish()
			);

		} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NetworkPSKc)) {
			Data network_pskc = any_to_data(value);

			start_new_task(SpinelNCPTaskSendCommand::Factory(this)
				.set_callback(cb)
				.add_command(
					SpinelPackData(SPINEL_FRAME_PACK_CMD_PROP_VALUE_SET(SPINEL_DATATYPE_DATA_S), SPINEL_PROP_NET_PSKC, network_pskc.data(), network_pskc.size())
				)
				.finish()
			);

		} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NetworkKey)) {
			Data network_key = any_to_data(value);

			if (!ncp_state_is_joining_or_joined(get_ncp_state())) {
				mNetworkKey = network_key;
				if (mNetworkKeyIndex == 0) {
					mNetworkKeyIndex = 1;
				}
			}

			if (get_ncp_state() == CREDENTIALS_NEEDED) {
				ValueMap options;
				options[kWPANTUNDProperty_NetworkKey] = value;
				start_new_task(boost::shared_ptr<SpinelNCPTask>(
					new SpinelNCPTaskJoin(
						this,
						boost::bind(cb,_1),
						options
					)
				));
			} else {
				start_new_task(SpinelNCPTaskSendCommand::Factory(this)
					.set_callback(cb)
					.add_command(
						SpinelPackData(SPINEL_FRAME_PACK_CMD_PROP_VALUE_SET(SPINEL_DATATYPE_DATA_S), SPINEL_PROP_NET_MASTER_KEY, network_key.data(), network_key.size())
					)
					.finish()
				);
			}

		} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NCPMACAddress)) {
			Data eui64_value = any_to_data(value);

			if (eui64_value.size() == sizeof(spinel_eui64_t)) {
				start_new_task(SpinelNCPTaskSendCommand::Factory(this)
					.set_callback(cb)
					.add_command(
						SpinelPackData(
							SPINEL_FRAME_PACK_CMD_PROP_VALUE_SET(SPINEL_DATATYPE_EUI64_S),
							SPINEL_PROP_MAC_15_4_LADDR,
							eui64_value.data()
						)
					)
					.finish()
				);

			} else {
				cb(kWPANTUNDStatus_InvalidArgument);
			}

		} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_InterfaceUp)) {
			bool isup = any_to_bool(value);
			if (isup) {
				start_new_task(SpinelNCPTaskSendCommand::Factory(this)
					.set_callback(cb)
					.add_command(SpinelPackData(
						SPINEL_FRAME_PACK_CMD_PROP_VALUE_SET(SPINEL_DATATYPE_BOOL_S),
						SPINEL_PROP_NET_IF_UP,
						true
					))
					.add_command(SpinelPackData(
						SPINEL_FRAME_PACK_CMD_PROP_VALUE_SET(SPINEL_DATATYPE_BOOL_S),
						SPINEL_PROP_NET_STACK_UP,
						true
					))
					.finish()
				);
			} else {
				start_new_task(SpinelNCPTaskSendCommand::Factory(this)
					.set_callback(cb)
					.add_command(SpinelPackData(
						SPINEL_FRAME_PACK_CMD_PROP_VALUE_SET(SPINEL_DATATYPE_BOOL_S),
						SPINEL_PROP_NET_STACK_UP,
						false
					))
					.add_command(SpinelPackData(
						SPINEL_FRAME_PACK_CMD_PROP_VALUE_SET(SPINEL_DATATYPE_BOOL_S),
						SPINEL_PROP_NET_IF_UP,
						false
					))
					.finish()
				);
			}

		} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NCPExtendedAddress)) {
			Data eui64_value = any_to_data(value);

			if (eui64_value.size() == sizeof(spinel_eui64_t)) {
				start_new_task(SpinelNCPTaskSendCommand::Factory(this)
					.set_callback(cb)
					.add_command(
						SpinelPackData(
							SPINEL_FRAME_PACK_CMD_PROP_VALUE_SET(SPINEL_DATATYPE_EUI64_S),
							SPINEL_PROP_MAC_EXTENDED_ADDR,
							eui64_value.data()
						)
					)
					.finish()
				);

			} else {
				cb(kWPANTUNDStatus_InvalidArgument);
			}

		} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NetworkXPANID)) {
			Data xpanid = any_to_data(value);

			start_new_task(SpinelNCPTaskSendCommand::Factory(this)
				.set_callback(cb)
				.add_command(
					SpinelPackData(SPINEL_FRAME_PACK_CMD_PROP_VALUE_SET(SPINEL_DATATYPE_DATA_S), SPINEL_PROP_NET_XPANID, xpanid.data(), xpanid.size())
				)
				.finish()
			);

			mXPANIDWasExplicitlySet = true;

		} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NetworkKey)) {
			Data network_key = any_to_data(value);

			start_new_task(SpinelNCPTaskSendCommand::Factory(this)
				.set_callback(cb)
				.add_command(
					SpinelPackData(SPINEL_FRAME_PACK_CMD_PROP_VALUE_SET(SPINEL_DATATYPE_DATA_S), SPINEL_PROP_NET_MASTER_KEY, network_key.data(), network_key.size())
				)
				.finish()
			);

		} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NetworkKeyIndex)) {
			uint32_t key_index = any_to_int(value);

			start_new_task(SpinelNCPTaskSendCommand::Factory(this)
				.set_callback(cb)
				.add_command(
					SpinelPackData(SPINEL_FRAME_PACK_CMD_PROP_VALUE_SET(SPINEL_DATATYPE_UINT32_S), SPINEL_PROP_NET_KEY_SEQUENCE_COUNTER, key_index)
				)
				.finish()
			);

		} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NetworkName)) {
			std::string str = any_to_string(value);

			start_new_task(SpinelNCPTaskSendCommand::Factory(this)
				.set_callback(cb)
				.add_command(SpinelPackData(SPINEL_FRAME_PACK_CMD_PROP_VALUE_SET(SPINEL_DATATYPE_UTF8_S), SPINEL_PROP_NET_NETWORK_NAME, str.c_str()))
				.finish()
			);

		} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NetworkRole)) {
			uint8_t role = any_to_int(value);

			start_new_task(SpinelNCPTaskSendCommand::Factory(this)
				.set_callback(cb)
				.add_command(SpinelPackData(SPINEL_FRAME_PACK_CMD_PROP_VALUE_SET(SPINEL_DATATYPE_UINT8_S), SPINEL_PROP_NET_ROLE, role))
				.finish()
			);

		} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_ThreadPreferredRouterID)) {
			uint8_t routerId = any_to_int(value);

			start_new_task(SpinelNCPTaskSendCommand::Factory(this)
				.set_callback(cb)
				.add_command(
					SpinelPackData(SPINEL_FRAME_PACK_CMD_PROP_VALUE_SET(SPINEL_DATATYPE_UINT8_S), SPINEL_PROP_THREAD_PREFERRED_ROUTER_ID, routerId)
				)
				.finish()
			);

		} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_ThreadDeviceMode)) {
			uint8_t mode = any_to_int(value);

			start_new_task(SpinelNCPTaskSendCommand::Factory(this)
				.set_callback(cb)
				.add_command(SpinelPackData(SPINEL_FRAME_PACK_CMD_PROP_VALUE_SET(SPINEL_DATATYPE_UINT8_S), SPINEL_PROP_THREAD_MODE, mode))
				.finish()
			);
		} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_TmfProxyEnabled)) {
			bool isEnabled = any_to_bool(value);
			Data command = SpinelPackData(SPINEL_FRAME_PACK_CMD_PROP_VALUE_SET(SPINEL_DATATYPE_BOOL_S), SPINEL_PROP_THREAD_TMF_PROXY_ENABLED, isEnabled);

			mSettings[kWPANTUNDProperty_TmfProxyEnabled] = SettingsEntry(command, SPINEL_CAP_THREAD_TMF_PROXY);

			if (!mCapabilities.count(SPINEL_CAP_THREAD_TMF_PROXY))
			{
				cb(kWPANTUNDStatus_FeatureNotSupported);
			} else {
				start_new_task(SpinelNCPTaskSendCommand::Factory(this)
					.set_callback(cb)
					.add_command(command)
					.finish()
				);
			}

		} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_MACWhitelistEnabled)) {
			bool isEnabled = any_to_bool(value);

			if (!mCapabilities.count(SPINEL_CAP_MAC_WHITELIST)) {
				cb(kWPANTUNDStatus_FeatureNotSupported);
			} else {
				start_new_task(SpinelNCPTaskSendCommand::Factory(this)
					.set_callback(cb)
					.add_command(
						SpinelPackData(
							SPINEL_FRAME_PACK_CMD_PROP_VALUE_SET(SPINEL_DATATYPE_BOOL_S),
							SPINEL_PROP_MAC_WHITELIST_ENABLED,
							isEnabled
						)
					)
					.finish()
				);
			}

		} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_JamDetectionEnable)) {
			bool isEnabled = any_to_bool(value);
			Data command = SpinelPackData(SPINEL_FRAME_PACK_CMD_PROP_VALUE_SET(SPINEL_DATATYPE_BOOL_S), SPINEL_PROP_JAM_DETECT_ENABLE, isEnabled);

			mSettings[kWPANTUNDProperty_JamDetectionEnable] = SettingsEntry(command, SPINEL_CAP_JAM_DETECT);

			if (!mCapabilities.count(SPINEL_CAP_JAM_DETECT))
			{
				cb(kWPANTUNDStatus_FeatureNotSupported);
			} else {
				start_new_task(SpinelNCPTaskSendCommand::Factory(this)
					.set_callback(cb)
					.add_command(command)
					.finish()
				);
			}

		} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_JamDetectionRssiThreshold)) {
			int8_t rssiThreshold = static_cast<int8_t>(any_to_int(value));
			Data command = SpinelPackData(SPINEL_FRAME_PACK_CMD_PROP_VALUE_SET(SPINEL_DATATYPE_INT8_S), SPINEL_PROP_JAM_DETECT_RSSI_THRESHOLD, rssiThreshold);

			mSettings[kWPANTUNDProperty_JamDetectionRssiThreshold] = SettingsEntry(command, SPINEL_CAP_JAM_DETECT);

			if (!mCapabilities.count(SPINEL_CAP_JAM_DETECT))
			{
				cb(kWPANTUNDStatus_FeatureNotSupported);
			} else {
				start_new_task(SpinelNCPTaskSendCommand::Factory(this)
					.set_callback(cb)
					.add_command(command)
					.finish()
				);
			}

		} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_JamDetectionWindow)) {
			uint8_t window = static_cast<uint8_t>(any_to_int(value));
			Data command = SpinelPackData(SPINEL_FRAME_PACK_CMD_PROP_VALUE_SET(SPINEL_DATATYPE_UINT8_S), SPINEL_PROP_JAM_DETECT_WINDOW, window);

			mSettings[kWPANTUNDProperty_JamDetectionWindow] = SettingsEntry(command, SPINEL_CAP_JAM_DETECT);

			if (!mCapabilities.count(SPINEL_CAP_JAM_DETECT))
			{
				cb(kWPANTUNDStatus_FeatureNotSupported);
			} else {
				start_new_task(SpinelNCPTaskSendCommand::Factory(this)
					.set_callback(cb)
					.add_command(command)
					.finish()
				);
			}

		} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_JamDetectionBusyPeriod)) {
			uint8_t busyPeriod = static_cast<uint8_t>(any_to_int(value));
			Data command = SpinelPackData(SPINEL_FRAME_PACK_CMD_PROP_VALUE_SET(SPINEL_DATATYPE_UINT8_S), SPINEL_PROP_JAM_DETECT_BUSY, busyPeriod);

			mSettings[kWPANTUNDProperty_JamDetectionBusyPeriod] = SettingsEntry(command, SPINEL_CAP_JAM_DETECT);

			if (!mCapabilities.count(SPINEL_CAP_JAM_DETECT))
			{
				cb(kWPANTUNDStatus_FeatureNotSupported);
			} else {
				start_new_task(SpinelNCPTaskSendCommand::Factory(this)
					.set_callback(cb)
					.add_command(command)
					.finish()
				);
			}

		} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NestLabs_LegacyMeshLocalPrefix)) {
			Data legacy_prefix = any_to_data(value);
			Data command =
				SpinelPackData(
					SPINEL_FRAME_PACK_CMD_PROP_VALUE_SET(SPINEL_DATATYPE_DATA_S),
					SPINEL_PROP_NEST_LEGACY_ULA_PREFIX,
					legacy_prefix.data(),
					legacy_prefix.size()
				);

			mSettings[kWPANTUNDProperty_NestLabs_LegacyMeshLocalPrefix] = SettingsEntry(command, SPINEL_CAP_NEST_LEGACY_INTERFACE);

			if (!mCapabilities.count(SPINEL_CAP_NEST_LEGACY_INTERFACE))
			{
				cb(kWPANTUNDStatus_FeatureNotSupported);
			} else {
				start_new_task(SpinelNCPTaskSendCommand::Factory(this)
					.set_callback(cb)
					.add_command(command)
					.finish()
				);
			}

		} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_ThreadCommissionerEnabled)) {
			bool isEnabled = any_to_bool(value);

			start_new_task(SpinelNCPTaskSendCommand::Factory(this)
				.set_callback(cb)
				.add_command(SpinelPackData(
					SPINEL_FRAME_PACK_CMD_PROP_VALUE_SET(SPINEL_DATATYPE_BOOL_S),
					SPINEL_PROP_THREAD_COMMISSIONER_ENABLED,
					isEnabled
				))
				.finish()
			);
		} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_OpenThreadLogLevel)) {
			uint8_t logLevel = static_cast<uint8_t>(any_to_int(value));
			Data command = SpinelPackData(SPINEL_FRAME_PACK_CMD_PROP_VALUE_SET(SPINEL_DATATYPE_UINT8_S), SPINEL_PROP_DEBUG_NCP_LOG_LEVEL, logLevel);

			mSettings[kWPANTUNDProperty_OpenThreadLogLevel] = SettingsEntry(command);

			start_new_task(SpinelNCPTaskSendCommand::Factory(this)
				.set_callback(cb)
				.add_command(command)
				.finish()
			);

		} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_OpenThreadSteeringDataSetWhenJoinable)) {
			mSetSteeringDataWhenJoinable = any_to_bool(value);
			cb(kWPANTUNDStatus_Ok);

		} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_OpenThreadSteeringDataAddress)) {
			Data address = any_to_data(value);
			wpantund_status_t status = kWPANTUNDStatus_Ok;

			if (address.size() != sizeof(mSteeringDataAddress)) {
				status = kWPANTUNDStatus_InvalidArgument;
			} else {
				memcpy(mSteeringDataAddress, address.data(), sizeof(mSteeringDataAddress));
			}

			cb (status);

		} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_TmfProxyStream)) {
			Data packet = any_to_data(value);

			uint16_t port = (packet[packet.size() - sizeof(port)] << 8 | packet[packet.size() - sizeof(port) + 1]);
			uint16_t locator = (packet[packet.size() - sizeof(locator) - sizeof(port)] << 8 |
					packet[packet.size() - sizeof(locator) - sizeof(port) + 1]);

			packet.resize(packet.size() - sizeof(locator) - sizeof(port));

			Data command = SpinelPackData(SPINEL_FRAME_PACK_CMD_PROP_VALUE_SET(SPINEL_DATATYPE_DATA_WLEN_S SPINEL_DATATYPE_UINT16_S SPINEL_DATATYPE_UINT16_S),
					SPINEL_PROP_THREAD_TMF_PROXY_STREAM, packet.data(), packet.size(), locator, port);

			start_new_task(SpinelNCPTaskSendCommand::Factory(this)
					.set_callback(cb)
					.add_command(command)
					.finish()
					);

		} else {
			NCPInstanceBase::property_set_value(key, value, cb);
		}

	} catch (const boost::bad_any_cast &x) {
		// We will get a bad_any_cast exception if the property is of
		// the wrong type.
		syslog(LOG_ERR,"property_set_value: Bad type for property \"%s\" (%s)", key.c_str(), x.what());
		cb(kWPANTUNDStatus_InvalidArgument);
	} catch (const std::invalid_argument &x) {
		// We will get a bad_any_cast exception if the property is of
		// the wrong type.
		syslog(LOG_ERR,"property_set_value: Invalid argument for property \"%s\" (%s)", key.c_str(), x.what());
		cb(kWPANTUNDStatus_InvalidArgument);
	}
}

void
SpinelNCPInstance::property_insert_value(
	const std::string& key,
	const boost::any& value,
	CallbackWithStatus cb
) {
	syslog(LOG_INFO, "property_insert_value: key: \"%s\"", key.c_str());

	if (!mEnabled) {
		cb(kWPANTUNDStatus_InvalidWhenDisabled);
		return;
	}

	try {
		if (strcaseequal(key.c_str(), kWPANTUNDProperty_MACWhitelistEntries)) {
			Data ext_address = any_to_data(value);
			int8_t rssi = kWPANTUND_Whitelist_RssiOverrideDisabled;

			if (!mCapabilities.count(SPINEL_CAP_MAC_WHITELIST)) {
				cb(kWPANTUNDStatus_FeatureNotSupported);
			} else {
				if (ext_address.size() == sizeof(spinel_eui64_t)) {
					start_new_task(SpinelNCPTaskSendCommand::Factory(this)
						.set_callback(cb)
						.add_command(
							SpinelPackData(
								SPINEL_FRAME_PACK_CMD_PROP_VALUE_INSERT(SPINEL_DATATYPE_EUI64_S SPINEL_DATATYPE_INT8_S),
								SPINEL_PROP_MAC_WHITELIST,
								ext_address.data(),
								rssi
							)
						)
						.finish()
					);
				} else {
					cb(kWPANTUNDStatus_InvalidArgument);
				}
			}

		} else {
			NCPInstanceBase::property_insert_value(key, value, cb);
		}
	} catch (const boost::bad_any_cast &x) {
		// We will get a bad_any_cast exception if the property is of
		// the wrong type.
		syslog(LOG_ERR,"property_insert_value: Bad type for property \"%s\" (%s)", key.c_str(), x.what());
		cb(kWPANTUNDStatus_InvalidArgument);
	} catch (const std::invalid_argument &x) {
		// We will get a bad_any_cast exception if the property is of
		// the wrong type.
		syslog(LOG_ERR,"property_insert_value: Invalid argument for property \"%s\" (%s)", key.c_str(), x.what());
		cb(kWPANTUNDStatus_InvalidArgument);
	}
}

void
SpinelNCPInstance::property_remove_value(
	const std::string& key,
	const boost::any& value,
	CallbackWithStatus cb
) {
	syslog(LOG_INFO, "property_remove_value: key: \"%s\"", key.c_str());

	try {
		if (strcaseequal(key.c_str(), kWPANTUNDProperty_MACWhitelistEntries)) {
			Data ext_address = any_to_data(value);

			if (!mCapabilities.count(SPINEL_CAP_MAC_WHITELIST)) {
				cb(kWPANTUNDStatus_FeatureNotSupported);
			} else {
				if (ext_address.size() == sizeof(spinel_eui64_t)) {
					start_new_task(SpinelNCPTaskSendCommand::Factory(this)
						.set_callback(cb)
						.add_command(
							SpinelPackData(
								SPINEL_FRAME_PACK_CMD_PROP_VALUE_REMOVE(SPINEL_DATATYPE_EUI64_S),
								SPINEL_PROP_MAC_WHITELIST,
								ext_address.data()
							)
						)
						.finish()
					);
				} else {
					cb(kWPANTUNDStatus_InvalidArgument);
				}
			}

		} else {
			NCPInstanceBase::property_remove_value(key, value, cb);
		}

	} catch (const boost::bad_any_cast &x) {
		// We will get a bad_any_cast exception if the property is of
		// the wrong type.
		syslog(LOG_ERR,"property_remove_value: Bad type for property \"%s\" (%s)", key.c_str(), x.what());
		cb(kWPANTUNDStatus_InvalidArgument);
	} catch (const std::invalid_argument &x) {
		// We will get a bad_any_cast exception if the property is of
		// the wrong type.
		syslog(LOG_ERR,"property_remove_value: Invalid argument for property \"%s\" (%s)", key.c_str(), x.what());
		cb(kWPANTUNDStatus_InvalidArgument);
	}
}

void
SpinelNCPInstance::reset_tasks(wpantund_status_t status)
{
	NCPInstanceBase::reset_tasks(status);
	while(!mTaskQueue.empty()) {
		mTaskQueue.front()->finish(status);
		mTaskQueue.pop_front();
	}
}

void
SpinelNCPInstance::handle_ncp_spinel_value_is(spinel_prop_key_t key, const uint8_t* value_data_ptr, spinel_size_t value_data_len)
{
	const uint8_t *original_value_data_ptr = value_data_ptr;
	spinel_size_t original_value_data_len = value_data_len;

	if (key == SPINEL_PROP_LAST_STATUS) {
		spinel_status_t status = SPINEL_STATUS_OK;
		spinel_datatype_unpack(value_data_ptr, value_data_len, "i", &status);
		syslog(LOG_INFO,"[-NCP-]: Last status (%s, %d)", spinel_status_to_cstr(status), status);
		if ((status >= SPINEL_STATUS_RESET__BEGIN) && (status <= SPINEL_STATUS_RESET__END)) {
			syslog(LOG_NOTICE, "[-NCP-]: NCP was reset (%s, %d)", spinel_status_to_cstr(status), status);
			process_event(EVENT_NCP_RESET, status);
			if (!mResetIsExpected && (mDriverState == NORMAL_OPERATION)) {
				wpantund_status_t wstatus = kWPANTUNDStatus_NCP_Reset;
				switch(status) {
				case SPINEL_STATUS_RESET_CRASH:
				case SPINEL_STATUS_RESET_FAULT:
				case SPINEL_STATUS_RESET_ASSERT:
				case SPINEL_STATUS_RESET_WATCHDOG:
				case SPINEL_STATUS_RESET_OTHER:
					wstatus = kWPANTUNDStatus_NCP_Crashed;
					break;
				default:
					break;
				}
				reset_tasks(wstatus);
			}

			if (mDriverState == NORMAL_OPERATION) {
				reinitialize_ncp();
			}
			mResetIsExpected = false;
			return;
		} else if (status == SPINEL_STATUS_INVALID_COMMAND) {
			syslog(LOG_NOTICE, "[-NCP-]: COMMAND NOT RECOGNIZED");
		}
	} else if (key == SPINEL_PROP_NCP_VERSION) {
		const char* ncp_version = NULL;
		spinel_datatype_unpack(value_data_ptr, value_data_len, "U", &ncp_version);

		set_ncp_version_string(ncp_version);


	} else if (key == SPINEL_PROP_INTERFACE_TYPE) {
		unsigned int interface_type = 0;
		spinel_datatype_unpack(value_data_ptr, value_data_len, "i", &interface_type);

		if (interface_type != SPINEL_PROTOCOL_TYPE_THREAD) {
			syslog(LOG_CRIT, "[-NCP-]: NCP is using unsupported protocol type (%d)", interface_type);
			change_ncp_state(FAULT);
		}


	} else if (key == SPINEL_PROP_PROTOCOL_VERSION) {
		unsigned int protocol_version_major = 0;
		unsigned int protocol_version_minor = 0;
		spinel_datatype_unpack(value_data_ptr, value_data_len, "ii", &protocol_version_major, &protocol_version_minor);

		if (protocol_version_major != SPINEL_PROTOCOL_VERSION_THREAD_MAJOR) {
			syslog(LOG_CRIT, "[-NCP-]: NCP is using unsupported protocol version (NCP:%d, wpantund:%d)", protocol_version_major, SPINEL_PROTOCOL_VERSION_THREAD_MAJOR);
			change_ncp_state(FAULT);
		}

		if (protocol_version_minor != SPINEL_PROTOCOL_VERSION_THREAD_MINOR) {
			syslog(LOG_WARNING, "[-NCP-]: NCP is using different protocol minor version (NCP:%d, wpantund:%d)", protocol_version_minor, SPINEL_PROTOCOL_VERSION_THREAD_MINOR);
		}

	} else if (key == SPINEL_PROP_CAPS) {
		const uint8_t* data_ptr = value_data_ptr;
		spinel_size_t data_len = value_data_len;
		std::set<unsigned int> capabilities;

		while(data_len != 0) {
			unsigned int value = 0;
			spinel_ssize_t parse_len = spinel_datatype_unpack(data_ptr, data_len, SPINEL_DATATYPE_UINT_PACKED_S, &value);
			if (parse_len <= 0) {
				syslog(LOG_WARNING, "[-NCP-]: Capability Parse failure");
				break;
			}
			capabilities.insert(value);
			syslog(LOG_INFO, "[-NCP-]: Capability (%s, %d)", spinel_capability_to_cstr(value), value);

			data_ptr += parse_len;
			data_len -= parse_len;
		}

		if (capabilities != mCapabilities) {
			mCapabilities = capabilities;
		}

	} else if (key == SPINEL_PROP_NET_NETWORK_NAME) {
		const char* value = NULL;
		spinel_datatype_unpack(value_data_ptr, value_data_len, "U", &value);
		if (value && (mCurrentNetworkInstance.name != value)) {
			mCurrentNetworkInstance.name = value;
			signal_property_changed(kWPANTUNDProperty_NetworkName, mCurrentNetworkInstance.name);
		}

	} else if (key == SPINEL_PROP_IPV6_LL_ADDR) {
		struct in6_addr *addr = NULL;
		spinel_datatype_unpack(value_data_ptr, value_data_len, "6", &addr);
		if ( NULL != addr
		  && (0 != memcmp(mNCPLinkLocalAddress.s6_addr, addr->s6_addr, sizeof(mNCPLinkLocalAddress)))
		) {
			if (IN6_IS_ADDR_LINKLOCAL(&mNCPLinkLocalAddress)) {
				remove_address(mNCPLinkLocalAddress);
			}

			memcpy((void*)mNCPLinkLocalAddress.s6_addr, (void*)addr->s6_addr, sizeof(mNCPLinkLocalAddress));

			if (IN6_IS_ADDR_LINKLOCAL(&mNCPLinkLocalAddress)) {
				add_address(mNCPLinkLocalAddress);
			}

			signal_property_changed(kWPANTUNDProperty_IPv6LinkLocalAddress, in6_addr_to_string(*addr));
		}

	} else if (key == SPINEL_PROP_IPV6_ML_ADDR) {
		struct in6_addr *addr = NULL;
		spinel_datatype_unpack(value_data_ptr, value_data_len, "6", &addr);
		if (addr
		 && buffer_is_nonzero(addr->s6_addr, 8)
		 && (0 != memcmp(mNCPMeshLocalAddress.s6_addr, addr->s6_addr, sizeof(mNCPMeshLocalAddress)))
		) {
			if (buffer_is_nonzero(mNCPMeshLocalAddress.s6_addr, sizeof(mNCPMeshLocalAddress))) {
				remove_address(mNCPMeshLocalAddress);
			}
			memcpy((void*)mNCPMeshLocalAddress.s6_addr, (void*)addr->s6_addr, sizeof(mNCPMeshLocalAddress));
			signal_property_changed(kWPANTUNDProperty_IPv6MeshLocalAddress, in6_addr_to_string(*addr));
			add_address(mNCPMeshLocalAddress);
		}

	} else if (key == SPINEL_PROP_IPV6_ML_PREFIX) {
		struct in6_addr *addr = NULL;
		spinel_datatype_unpack(value_data_ptr, value_data_len, "6", &addr);
		if (addr
		 && buffer_is_nonzero(addr->s6_addr, 8)
		 && (0 != memcmp(mNCPV6Prefix, addr, sizeof(mNCPV6Prefix)))
		) {
			if (buffer_is_nonzero(mNCPMeshLocalAddress.s6_addr, sizeof(mNCPMeshLocalAddress))) {
				remove_address(mNCPMeshLocalAddress);
			}
			memcpy((void*)mNCPV6Prefix, (void*)addr, sizeof(mNCPV6Prefix));
			struct in6_addr prefix_addr (mNCPMeshLocalAddress);
			// Zero out the lower 64 bits.
			memset(prefix_addr.s6_addr+8, 0, 8);
			signal_property_changed(kWPANTUNDProperty_IPv6MeshLocalPrefix, in6_addr_to_string(prefix_addr) + "/64");
		}

	} else if (key == SPINEL_PROP_IPV6_ADDRESS_TABLE) {
		std::map<struct in6_addr, GlobalAddressEntry>::const_iterator iter;
		std::map<struct in6_addr, GlobalAddressEntry> global_addresses(mGlobalAddresses);

		while (value_data_len > 0) {
			const uint8_t *entry_ptr = NULL;
			spinel_size_t entry_len = 0;
			spinel_ssize_t len = 0;
			len = spinel_datatype_unpack(value_data_ptr, value_data_len, "D.", &entry_ptr, &entry_len);
			if (len < 1) {
				break;
			}
			global_addresses.erase(*reinterpret_cast<const struct in6_addr*>(entry_ptr));
			handle_ncp_spinel_value_inserted(key, entry_ptr, entry_len);

			value_data_ptr += len;
			value_data_len -= len;
		}

		// Since this was the whole list, we need
		// to remove the addresses that weren't in
		// the list.
		for (iter = global_addresses.begin(); iter!= global_addresses.end(); ++iter) {
			if (!iter->second.mUserAdded) {
				remove_address(iter->first);
			}
		}

	} else if (key == SPINEL_PROP_HWADDR) {
		nl::Data hwaddr(value_data_ptr, value_data_len);
		if (value_data_len == sizeof(mMACHardwareAddress)) {
			set_mac_hardware_address(value_data_ptr);
		}

	} else if (key == SPINEL_PROP_MAC_15_4_LADDR) {
		nl::Data hwaddr(value_data_ptr, value_data_len);
		if (value_data_len == sizeof(mMACAddress)) {
			set_mac_address(value_data_ptr);
		}

	} else if (key == SPINEL_PROP_MAC_15_4_PANID) {
		uint16_t panid;
		spinel_datatype_unpack(value_data_ptr, value_data_len, SPINEL_DATATYPE_UINT16_S, &panid);
		if (panid != mCurrentNetworkInstance.panid) {
			mCurrentNetworkInstance.panid = panid;
			signal_property_changed(kWPANTUNDProperty_NetworkPANID, panid);
		}

	} else if (key == SPINEL_PROP_NET_XPANID) {
		nl::Data xpanid(value_data_ptr, value_data_len);
		if ((value_data_len == 8) && 0 != memcmp(xpanid.data(), mCurrentNetworkInstance.xpanid, 8)) {
			memcpy(mCurrentNetworkInstance.xpanid, xpanid.data(), 8);
			signal_property_changed(kWPANTUNDProperty_NetworkXPANID, xpanid);
		}

	} else if (key == SPINEL_PROP_NET_PSKC) {
		nl::Data network_pskc(value_data_ptr, value_data_len);
		if (network_pskc != mNetworkPSKc) {
			mNetworkPSKc = network_pskc;
			signal_property_changed(kWPANTUNDProperty_NetworkPSKc, mNetworkPSKc);
		}

	} else if (key == SPINEL_PROP_NET_MASTER_KEY) {
		nl::Data network_key(value_data_ptr, value_data_len);
		if (ncp_state_is_joining_or_joined(get_ncp_state())) {
			if (network_key != mNetworkKey) {
				mNetworkKey = network_key;
				signal_property_changed(kWPANTUNDProperty_NetworkKey, mNetworkKey);
			}
		}

	} else if (key == SPINEL_PROP_NET_KEY_SEQUENCE_COUNTER) {
		uint32_t network_key_index;
		spinel_datatype_unpack(value_data_ptr, value_data_len, SPINEL_DATATYPE_UINT32_S, &network_key_index);
		if (network_key_index != mNetworkKeyIndex) {
			mNetworkKeyIndex = network_key_index;
			signal_property_changed(kWPANTUNDProperty_NetworkKeyIndex, mNetworkKeyIndex);
		}

	} else if (key == SPINEL_PROP_PHY_CHAN) {
		unsigned int value;
		spinel_datatype_unpack(value_data_ptr, value_data_len, SPINEL_DATATYPE_UINT_PACKED_S, &value);
		if (value != mCurrentNetworkInstance.channel) {
			mCurrentNetworkInstance.channel = value;
			signal_property_changed(kWPANTUNDProperty_NCPChannel, mCurrentNetworkInstance.channel);
		}

	} else if (key == SPINEL_PROP_PHY_CHAN_SUPPORTED) {

		uint8_t channel;
		spinel_ssize_t len = 0;

		mSupprotedChannels.clear();

		while (value_data_len > 0)
		{
			len = spinel_datatype_unpack(value_data_ptr, value_data_len, SPINEL_DATATYPE_UINT8_S, &channel);
			mSupprotedChannels.insert(channel);

			value_data_ptr += len;
			value_data_len -= len;
		}

	} else if (key == SPINEL_PROP_PHY_TX_POWER) {
		int8_t value;
		spinel_datatype_unpack(value_data_ptr, value_data_len, SPINEL_DATATYPE_INT8_S, &value);
		if (value != mTXPower) {
			mTXPower = value;
			signal_property_changed(kWPANTUNDProperty_NCPTXPower, mTXPower);
		}

	} else if (key == SPINEL_PROP_STREAM_DEBUG) {
		handle_ncp_log(value_data_ptr, value_data_len);

	} else if (key == SPINEL_PROP_NET_ROLE) {
		uint8_t value;
		spinel_datatype_unpack(value_data_ptr, value_data_len, SPINEL_DATATYPE_UINT8_S, &value);
		syslog(LOG_INFO,"[-NCP-]: Net Role \"%s\" (%d)", spinel_net_role_to_cstr(value), value);

		if ( ncp_state_is_joining_or_joined(get_ncp_state())
		  && (value != SPINEL_NET_ROLE_DETACHED)
		) {
			change_ncp_state(ASSOCIATED);
		}

		if (value == SPINEL_NET_ROLE_CHILD) {
			if (mNodeType != END_DEVICE) {
				mNodeType = END_DEVICE;
				signal_property_changed(kWPANTUNDProperty_NetworkNodeType, node_type_to_string(mNodeType));
			}

		} else if (value == SPINEL_NET_ROLE_ROUTER) {
			if (mNodeType != ROUTER) {
				mNodeType = ROUTER;
				signal_property_changed(kWPANTUNDProperty_NetworkNodeType, node_type_to_string(mNodeType));
			}

		} else if (value == SPINEL_NET_ROLE_LEADER) {
			if (mNodeType != LEADER) {
				mNodeType = LEADER;
				signal_property_changed(kWPANTUNDProperty_NetworkNodeType, node_type_to_string(mNodeType));
			}

		} else if (value == SPINEL_NET_ROLE_DETACHED) {
			if (ncp_state_is_associated(get_ncp_state())) {
				change_ncp_state(ISOLATED);
			}
		}

	} else if (key == SPINEL_PROP_NET_STACK_UP) {
		bool is_stack_up;
		spinel_datatype_unpack(value_data_ptr, value_data_len, SPINEL_DATATYPE_BOOL_S, &is_stack_up);

		if (is_stack_up) {
			if (!ncp_state_is_joining_or_joined(get_ncp_state())) {
				change_ncp_state(ASSOCIATING);
			}
		} else {
			if (!ncp_state_is_joining(get_ncp_state())) {
				change_ncp_state(OFFLINE);
			}
		}

	} else if (key == SPINEL_PROP_NET_IF_UP) {
		bool is_if_up;
		spinel_datatype_unpack(value_data_ptr, value_data_len, SPINEL_DATATYPE_BOOL_S, &is_if_up);

		if (ncp_state_is_interface_up(get_ncp_state()) && !is_if_up) {
			change_ncp_state(OFFLINE);
		}

	} else if (key == SPINEL_PROP_THREAD_ON_MESH_NETS) {
		mOnMeshPrefixes.clear();
		while (value_data_len > 0) {
			spinel_ssize_t len = 0;
			struct in6_addr *addr = NULL;
			uint8_t prefix_len = 0;
			bool stable;
			uint8_t flags = 0;
			bool isLocal;

			len = spinel_datatype_unpack(
				value_data_ptr,
				value_data_len,
				"t(6CbCb)",
				&addr,
				&prefix_len,
				&stable,
				&flags,
				&isLocal
			);

			if (len < 1) {
				break;
			}

			refresh_on_mesh_prefix(addr, prefix_len, stable, flags, isLocal);

			value_data_ptr += len;
			value_data_len -= len;
		}

	} else if (key == SPINEL_PROP_THREAD_ASSISTING_PORTS) {
		bool is_assisting = (value_data_len != 0);
		uint16_t assisting_port(0);

		if (is_assisting != get_current_network_instance().joinable) {
			mCurrentNetworkInstance.joinable = is_assisting;
			signal_property_changed(kWPANTUNDProperty_NestLabs_NetworkAllowingJoin, is_assisting);
		}

		if (is_assisting) {
			int i;
			syslog(LOG_NOTICE, "Network is joinable");
			while (value_data_len > 0) {
				i = spinel_datatype_unpack(value_data_ptr, value_data_len, SPINEL_DATATYPE_UINT16_S, &assisting_port);
				if (i <= 0) {
					break;
				}
				syslog(LOG_NOTICE, "Assisting on port %d", assisting_port);
				value_data_ptr += i;
				value_data_len -= i;
			}
		} else {
			syslog(LOG_NOTICE, "Network is not joinable");
		}

	} else if (key == SPINEL_PROP_JAM_DETECTED) {
		bool jamDetected = false;

		spinel_datatype_unpack(value_data_ptr, value_data_len, SPINEL_DATATYPE_BOOL_S, &jamDetected);
		signal_property_changed(kWPANTUNDProperty_JamDetectionStatus, jamDetected);

		if (jamDetected) {
			syslog(LOG_NOTICE, "Signal jamming is detected");
		} else {
			syslog(LOG_NOTICE, "Signal jamming cleared");
		}

	} else if (key == SPINEL_PROP_STREAM_RAW) {
		if (mPcapManager.is_enabled()) {
			const uint8_t* frame_ptr(NULL);
			unsigned int frame_len(0);
			const uint8_t* meta_ptr(NULL);
			unsigned int meta_len(0);
			spinel_ssize_t ret;
			PcapPacket packet;
			uint16_t flags = 0;

			packet.set_timestamp().set_dlt(PCAP_DLT_IEEE802_15_4);

			// Unpack the packet.
			ret = spinel_datatype_unpack(
				value_data_ptr,
				value_data_len,
				SPINEL_DATATYPE_DATA_WLEN_S SPINEL_DATATYPE_DATA_S,
				&frame_ptr,
				&frame_len,
				&meta_ptr,
				&meta_len
			);

			require(ret > 0, bail);

			// Unpack the metadata.
			ret = spinel_datatype_unpack(
				meta_ptr,
				meta_len,
				SPINEL_DATATYPE_INT8_S     // RSSI/TXPower
				SPINEL_DATATYPE_INT8_S     // Noise Floor
				SPINEL_DATATYPE_UINT16_S,  // Flags
				NULL,   // Ignore RSSI/TXPower
				NULL,	// Ignore Noise Floor
				&flags
			);

			__ASSERT_MACROS_check(ret > 0);

			if ((flags & SPINEL_MD_FLAG_TX) == SPINEL_MD_FLAG_TX)
			{
				// Ignore FCS for transmitted packets
				frame_len -= 2;
				packet.set_dlt(PCAP_DLT_IEEE802_15_4_NOFCS);
			}

			mPcapManager.push_packet(
				packet
					.append_ppi_field(PCAP_PPI_TYPE_SPINEL, meta_ptr, meta_len)
					.append_payload(frame_ptr, frame_len)
			);
		}

	} else if (key == SPINEL_PROP_THREAD_TMF_PROXY_STREAM) {
		const uint8_t* frame_ptr(NULL);
		unsigned int frame_len(0);
		uint16_t locator;
		uint16_t port;
		spinel_ssize_t ret;
		Data data;

		ret = spinel_datatype_unpack(
			value_data_ptr,
			value_data_len,
			SPINEL_DATATYPE_DATA_S SPINEL_DATATYPE_UINT16_S SPINEL_DATATYPE_UINT16_S,
			&frame_ptr,
			&frame_len,
			&locator,
			&port
		);

		__ASSERT_MACROS_check(ret > 0);

		// Analyze the packet to determine if it should be dropped.
		if ((ret > 0)) {
			// append frame
			data.append(frame_ptr, frame_len);
			// pack the locator in big endian.
			data.push_back(locator >> 8);
			data.push_back(locator & 0xff);
			// pack the port in big endian.
			data.push_back(port >> 8);
			data.push_back(port & 0xff);
			signal_property_changed(kWPANTUNDProperty_TmfProxyStream, data);
		}

	} else if ((key == SPINEL_PROP_STREAM_NET) || (key == SPINEL_PROP_STREAM_NET_INSECURE)) {
		const uint8_t* frame_ptr(NULL);
		unsigned int frame_len(0);
		spinel_ssize_t ret;
		uint8_t frame_data_type = FRAME_TYPE_DATA;

		if (SPINEL_PROP_STREAM_NET_INSECURE == key) {
			frame_data_type = FRAME_TYPE_INSECURE_DATA;
		}

		ret = spinel_datatype_unpack(
			value_data_ptr,
			value_data_len,
			SPINEL_DATATYPE_DATA_S SPINEL_DATATYPE_DATA_S,
			&frame_ptr,
			&frame_len,
			NULL,
			NULL
		);

		__ASSERT_MACROS_check(ret > 0);

		// Analyze the packet to determine if it should be dropped.
		if ((ret > 0) && should_forward_hostbound_frame(&frame_data_type, frame_ptr, frame_len)) {
			if (static_cast<bool>(mLegacyInterface) && (frame_data_type == FRAME_TYPE_LEGACY_DATA)) {
				handle_alt_ipv6_from_ncp(frame_ptr, frame_len);
			} else {
				handle_normal_ipv6_from_ncp(frame_ptr, frame_len);
			}
		}
	} else if (key == SPINEL_PROP_THREAD_CHILD_TABLE) {
		SpinelNCPTaskGetNetworkTopology::Table child_table;
		SpinelNCPTaskGetNetworkTopology::Table::iterator it;
		int num_children = 0;

		SpinelNCPTaskGetNetworkTopology::prase_child_table(value_data_ptr, value_data_len, child_table);

		for (it = child_table.begin(); it != child_table.end(); it++)
		{
			num_children++;
			syslog(LOG_INFO, "[-NCP-] Child: %02d %s", num_children, it->get_as_string().c_str());
		}
		syslog(LOG_INFO, "[-NCP-] Child: Total %d child%s", num_children, (num_children > 1) ? "ren" : "");

	} else if (key == SPINEL_PROP_THREAD_LEADER_NETWORK_DATA) {
		char net_data_cstr_buf[540];
		encode_data_into_string(value_data_ptr, value_data_len, net_data_cstr_buf, sizeof(net_data_cstr_buf), 0);
		syslog(LOG_INFO, "[-NCP-] Leader network data: %s", net_data_cstr_buf);
	}

bail:
	process_event(EVENT_NCP_PROP_VALUE_IS, key, original_value_data_ptr, original_value_data_len);
}

void
SpinelNCPInstance::refresh_on_mesh_prefix(struct in6_addr *prefix, uint8_t prefix_len, bool stable, uint8_t flags, bool isLocal)
{
	if (!isLocal) {
		struct in6_addr addr;
		memcpy(&addr, prefix, sizeof(in6_addr));
		add_prefix(addr, UINT32_MAX, UINT32_MAX, flags);
	}
	if ( ((flags & (SPINEL_NET_FLAG_ON_MESH | SPINEL_NET_FLAG_SLAAC)) == (SPINEL_NET_FLAG_ON_MESH | SPINEL_NET_FLAG_SLAAC))
	  && !lookup_address_for_prefix(NULL, *prefix, prefix_len)
	) {
		SpinelNCPTaskSendCommand::Factory factory(this);
		struct in6_addr addr = make_slaac_addr_from_eui64(prefix->s6_addr, mMACAddress);

		syslog(LOG_NOTICE, "Pushing a new address %s/%d to the NCP", in6_addr_to_string(addr).c_str(), prefix_len);

		factory.set_lock_property(SPINEL_PROP_THREAD_ALLOW_LOCAL_NET_DATA_CHANGE);

		factory.add_command(
			SpinelPackData(
				SPINEL_FRAME_PACK_CMD_PROP_VALUE_INSERT(
					SPINEL_DATATYPE_IPv6ADDR_S   // Address
					SPINEL_DATATYPE_UINT8_S      // Prefix Length
					SPINEL_DATATYPE_UINT32_S     // Valid Lifetime
					SPINEL_DATATYPE_UINT32_S     // Preferred Lifetime
				),
				SPINEL_PROP_IPV6_ADDRESS_TABLE,
				&addr,
				prefix_len,
				UINT32_MAX,
				UINT32_MAX
			)
		);

		start_new_task(factory.finish());
	}
}

void
SpinelNCPInstance::handle_ncp_spinel_value_inserted(spinel_prop_key_t key, const uint8_t* value_data_ptr, spinel_size_t value_data_len)
{
	if (key == SPINEL_PROP_IPV6_ADDRESS_TABLE) {
			struct in6_addr *addr = NULL;
			uint8_t prefix_len = 0;
			uint32_t valid_lifetime = 0xFFFFFFFF;
			uint32_t preferred_lifetime = 0xFFFFFFFF;

			spinel_datatype_unpack(value_data_ptr, value_data_len, "6CLL", &addr, &prefix_len, &valid_lifetime, &preferred_lifetime);

			if (addr != NULL
				&& buffer_is_nonzero(addr->s6_addr, 8)
				&& !IN6_IS_ADDR_UNSPECIFIED(addr)
			) {
				static const uint8_t rloc_bytes[] = {0x00,0x00,0x00,0xFF,0xFE,0x00};
				if (IN6_IS_ADDR_LINKLOCAL(addr)) {
					if (0 != memcmp(rloc_bytes, addr->s6_addr+8, sizeof(rloc_bytes))) {
						handle_ncp_spinel_value_is(SPINEL_PROP_IPV6_LL_ADDR, addr->s6_addr, sizeof(*addr));
					}
				} else if (0 == memcmp(mNCPV6Prefix, addr, sizeof(mNCPV6Prefix))) {
					if (0 != memcmp(rloc_bytes, addr->s6_addr+8, sizeof(rloc_bytes))) {
						handle_ncp_spinel_value_is(SPINEL_PROP_IPV6_ML_ADDR, addr->s6_addr, sizeof(*addr));
					}
				} else {
					add_address(*addr, 64, valid_lifetime, preferred_lifetime);
				}
			}
	} else if (key == SPINEL_PROP_THREAD_ON_MESH_NETS) {
		struct in6_addr *addr = NULL;
		uint8_t prefix_len = 0;
		bool stable;
		uint8_t flags = 0;
		bool isLocal;
		static const char flag_lookup[] = "ppPSDCRM";

		spinel_datatype_unpack(
			value_data_ptr,
			value_data_len,
			"6CbCb",
			&addr,
			&prefix_len,
			&stable,
			&flags,
			&isLocal
		);

		syslog(LOG_NOTICE, "On-Mesh Network Added: %s/%d flags:%s", in6_addr_to_string(*addr).c_str(), prefix_len, flags_to_string(flags, flag_lookup).c_str());

		refresh_on_mesh_prefix(addr, prefix_len, stable, flags, isLocal);
	}

	process_event(EVENT_NCP_PROP_VALUE_INSERTED, key, value_data_ptr, value_data_len);
}

void
SpinelNCPInstance::handle_ncp_state_change(NCPState new_ncp_state, NCPState old_ncp_state)
{
	NCPInstanceBase::handle_ncp_state_change(new_ncp_state, old_ncp_state);

	if ( ncp_state_is_joining_or_joined(old_ncp_state)
	  && (new_ncp_state == OFFLINE)
	) {
		// Mark this as false so that if we are actually doing
		// a pcap right now it will force the details to be updated
		// on the NCP at the next run through the main loop. This
		// allows us to go back to promiscuous-mode sniffing at
		// disconnect
		mIsPcapInProgress = false;
	}

	if (ncp_state_is_associated(new_ncp_state)
	 && !ncp_state_is_associated(old_ncp_state)
	) {
		start_new_task(SpinelNCPTaskSendCommand::Factory(this)
			.add_command(SpinelPackData(SPINEL_FRAME_PACK_CMD_PROP_VALUE_GET, SPINEL_PROP_MAC_15_4_LADDR))
			.add_command(SpinelPackData(SPINEL_FRAME_PACK_CMD_PROP_VALUE_GET, SPINEL_PROP_IPV6_ML_ADDR))
			.add_command(SpinelPackData(SPINEL_FRAME_PACK_CMD_PROP_VALUE_GET, SPINEL_PROP_NET_XPANID))
			.add_command(SpinelPackData(SPINEL_FRAME_PACK_CMD_PROP_VALUE_GET, SPINEL_PROP_MAC_15_4_PANID))
			.add_command(SpinelPackData(SPINEL_FRAME_PACK_CMD_PROP_VALUE_GET, SPINEL_PROP_PHY_CHAN))
			.finish()
		);
	} else if (ncp_state_is_joining(new_ncp_state)
	 && !ncp_state_is_joining(old_ncp_state)
	) {
		if (!buffer_is_nonzero(mNCPV6Prefix, 8)) {
			start_new_task(SpinelNCPTaskSendCommand::Factory(this)
				.add_command(SpinelPackData(SPINEL_FRAME_PACK_CMD_PROP_VALUE_GET, SPINEL_PROP_IPV6_ML_PREFIX))
				.finish()
			);
		}
	}
}

void
SpinelNCPInstance::handle_ncp_spinel_value_removed(spinel_prop_key_t key, const uint8_t* value_data_ptr, spinel_size_t value_data_len)
{
	process_event(EVENT_NCP_PROP_VALUE_REMOVED, key, value_data_ptr, value_data_len);
}

void
SpinelNCPInstance::handle_ncp_spinel_callback(unsigned int command, const uint8_t* cmd_data_ptr, spinel_size_t cmd_data_len)
{
	switch (command) {
	case SPINEL_CMD_PROP_VALUE_IS:
		{
			spinel_prop_key_t key;
			uint8_t* value_data_ptr = NULL;
			spinel_size_t value_data_len = 0;
			spinel_ssize_t ret;

			ret = spinel_datatype_unpack(cmd_data_ptr, cmd_data_len, "CiiD", NULL, NULL, &key, &value_data_ptr, &value_data_len);

			__ASSERT_MACROS_check(ret != -1);

			if (ret == -1) {
				return;
			}

			if (key != SPINEL_PROP_STREAM_DEBUG) {
				syslog(LOG_INFO, "[NCP->] CMD_PROP_VALUE_IS(%s) tid:%d", spinel_prop_key_to_cstr(key), SPINEL_HEADER_GET_TID(cmd_data_ptr[0]));
			}

			return handle_ncp_spinel_value_is(key, value_data_ptr, value_data_len);
		}
		break;

	case SPINEL_CMD_PROP_VALUE_INSERTED:
		{
			spinel_prop_key_t key;
			uint8_t* value_data_ptr;
			spinel_size_t value_data_len;
			spinel_ssize_t ret;

			ret = spinel_datatype_unpack(cmd_data_ptr, cmd_data_len, "CiiD", NULL, NULL, &key, &value_data_ptr, &value_data_len);

			__ASSERT_MACROS_check(ret != -1);

			if (ret == -1) {
				return;
			}

			syslog(LOG_INFO, "[NCP->] CMD_PROP_VALUE_INSERTED(%s) tid:%d", spinel_prop_key_to_cstr(key), SPINEL_HEADER_GET_TID(cmd_data_ptr[0]));

			return handle_ncp_spinel_value_inserted(key, value_data_ptr, value_data_len);
		}
		break;

	case SPINEL_CMD_PROP_VALUE_REMOVED:
		{
			spinel_prop_key_t key;
			uint8_t* value_data_ptr;
			spinel_size_t value_data_len;
			spinel_ssize_t ret;

			ret = spinel_datatype_unpack(cmd_data_ptr, cmd_data_len, "CiiD", NULL, NULL, &key, &value_data_ptr, &value_data_len);

			__ASSERT_MACROS_check(ret != -1);

			if (ret == -1) {
				return;
			}

			syslog(LOG_INFO, "[NCP->] CMD_PROP_VALUE_REMOVED(%s) tid:%d", spinel_prop_key_to_cstr(key), SPINEL_HEADER_GET_TID(cmd_data_ptr[0]));

			return handle_ncp_spinel_value_removed(key, value_data_ptr, value_data_len);
		}
		break;

	default:
		break;
	}

	process_event(EVENT_NCP(command), cmd_data_ptr[0], cmd_data_ptr, cmd_data_len);
}

void
SpinelNCPInstance::address_was_added(const struct in6_addr& addr, int prefix_len)
{
	if (!is_address_known(addr) && !IN6_IS_ADDR_LINKLOCAL(&addr)) {
		SpinelNCPTaskSendCommand::Factory factory(this);
		uint8_t flags = SPINEL_NET_FLAG_SLAAC
					  | SPINEL_NET_FLAG_ON_MESH
					  | SPINEL_NET_FLAG_PREFERRED;
		CallbackWithStatus callback;

		NCPInstanceBase::address_was_added(addr, prefix_len);

		factory.set_lock_property(SPINEL_PROP_THREAD_ALLOW_LOCAL_NET_DATA_CHANGE);

		callback = boost::bind(&SpinelNCPInstance::check_operation_status, this, "address_was_added()", _1);
		factory.set_callback(callback);

		factory.add_command(
			SpinelPackData(
				SPINEL_FRAME_PACK_CMD_PROP_VALUE_INSERT(
					SPINEL_DATATYPE_IPv6ADDR_S   // Address
					SPINEL_DATATYPE_UINT8_S      // Prefix Length
					SPINEL_DATATYPE_UINT32_S     // Valid Lifetime
					SPINEL_DATATYPE_UINT32_S     // Preferred Lifetime
				),
				SPINEL_PROP_IPV6_ADDRESS_TABLE,
				&addr,
				prefix_len,
				UINT32_MAX,
				UINT32_MAX
			)
		);

		factory.add_command(
			SpinelPackData(
				SPINEL_FRAME_PACK_CMD_PROP_VALUE_INSERT(
					SPINEL_DATATYPE_IPv6ADDR_S   // Address
					SPINEL_DATATYPE_UINT8_S      // Prefix Length
					SPINEL_DATATYPE_BOOL_S       // Stable?
					SPINEL_DATATYPE_UINT8_S      // Flags
				),
				SPINEL_PROP_THREAD_ON_MESH_NETS,
				&addr,
				prefix_len,
				true,
				flags
			)
		);

		start_new_task(factory.finish());
	}
}

void
SpinelNCPInstance::address_was_removed(const struct in6_addr& addr, int prefix_len)
{
	if (mPrimaryInterface->is_online() && is_address_known(addr)) {
		SpinelNCPTaskSendCommand::Factory factory(this);

		factory.set_lock_property(SPINEL_PROP_THREAD_ALLOW_LOCAL_NET_DATA_CHANGE);

		factory.add_command(
			SpinelPackData(
				SPINEL_FRAME_PACK_CMD_PROP_VALUE_REMOVE(
					SPINEL_DATATYPE_IPv6ADDR_S   // Address
					SPINEL_DATATYPE_UINT8_S      // Prefix
				),
				SPINEL_PROP_IPV6_ADDRESS_TABLE,
				&addr,
				prefix_len
			)
		);

		start_new_task(factory.finish());
	}

	NCPInstanceBase::address_was_removed(addr, prefix_len);
}

void
SpinelNCPInstance::check_operation_status(std::string operation, int status)
{
	if (status == kWPANTUNDStatus_Timeout)
	{
		syslog(LOG_ERR, "Timed out while performing \"%s\" - Resetting NCP.", operation.c_str());
		ncp_is_misbehaving();
	}
}

bool
SpinelNCPInstance::is_busy(void)
{
	return NCPInstanceBase::is_busy()
		|| !mTaskQueue.empty();
}


void
SpinelNCPInstance::process(void)
{
	NCPInstanceBase::process();

	if (!is_initializing_ncp() && mTaskQueue.empty()) {
		bool x = mPcapManager.is_enabled();

		if (mIsPcapInProgress != x) {
			SpinelNCPTaskSendCommand::Factory factory(this);

			mIsPcapInProgress = x;

			factory.add_command(SpinelPackData(
				SPINEL_FRAME_PACK_CMD_PROP_VALUE_SET(SPINEL_DATATYPE_BOOL_S),
				SPINEL_PROP_MAC_RAW_STREAM_ENABLED,
				mIsPcapInProgress
			));

			if (mIsPcapInProgress) {
				factory.add_command(SpinelPackData(
					SPINEL_FRAME_PACK_CMD_PROP_VALUE_SET(SPINEL_DATATYPE_BOOL_S),
					SPINEL_PROP_NET_IF_UP,
					true
				));
				if (!ncp_state_is_joining_or_joined(get_ncp_state())) {
					factory.add_command(SpinelPackData(
						SPINEL_FRAME_PACK_CMD_PROP_VALUE_SET(SPINEL_DATATYPE_UINT8_S),
						SPINEL_PROP_MAC_PROMISCUOUS_MODE,
						SPINEL_MAC_PROMISCUOUS_MODE_FULL
					));
				}
			} else {
				factory.add_command(SpinelPackData(
					SPINEL_FRAME_PACK_CMD_PROP_VALUE_SET(SPINEL_DATATYPE_UINT8_S),
					SPINEL_PROP_MAC_PROMISCUOUS_MODE,
					SPINEL_MAC_PROMISCUOUS_MODE_OFF
				));
			}

			start_new_task(factory.finish());
			NCPInstanceBase::process();
		}
	}
}
