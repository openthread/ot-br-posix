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

#include "SpinelNCPControlInterface.h"
#include "SpinelNCPInstance.h"

#include "wpantund.h"
#include "config-file.h"
#include "nlpt.h"
#include "string-utils.h"
#include "any-to.h"
#include "time-utils.h"
#include "commissioner-utils.h"

#include <cstring>
#include <algorithm>
#include <errno.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <sys/time.h>

#include <boost/bind.hpp>

#include "spinel-extra.h"

#include "SpinelNCPTask.h"
#include "SpinelNCPTaskWake.h"
#include "SpinelNCPTaskJoin.h"
#include "SpinelNCPTaskForm.h"
#include "SpinelNCPTaskLeave.h"
#include "SpinelNCPTaskScan.h"
#include "SpinelNCPTaskSendCommand.h"

using namespace nl;
using namespace nl::wpantund;

// ----------------------------------------------------------------------------
// MARK: -

SpinelNCPControlInterface::SpinelNCPControlInterface(SpinelNCPInstance* instance_pointer)
	:mNCPInstance(instance_pointer)
{
}

// ----------------------------------------------------------------------------
// MARK: -

void
SpinelNCPControlInterface::join(
	const ValueMap& options,
	CallbackWithStatus cb
) {
	mNCPInstance->start_new_task(boost::shared_ptr<SpinelNCPTask>(
		new SpinelNCPTaskJoin(
			mNCPInstance,
			boost::bind(cb,_1),
			options
		)
	));
}

void
SpinelNCPControlInterface::form(
	const ValueMap& options,
	CallbackWithStatus cb
) {
	mNCPInstance->start_new_task(boost::shared_ptr<SpinelNCPTask>(
		new SpinelNCPTaskForm(
			mNCPInstance,
			boost::bind(cb,_1),
			options
		)
	));
}

void
SpinelNCPControlInterface::leave(CallbackWithStatus cb)
{
	mNCPInstance->start_new_task(boost::shared_ptr<SpinelNCPTask>(
		new SpinelNCPTaskLeave(
			mNCPInstance,
			boost::bind(cb,_1)
		)
	));
}

void
SpinelNCPControlInterface::attach(CallbackWithStatus cb)
{
	mNCPInstance->start_new_task(
		SpinelNCPTaskSendCommand::Factory(mNCPInstance)
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
}

void
SpinelNCPControlInterface::reset(CallbackWithStatus cb)
{
	if (mNCPInstance->get_ncp_state() == FAULT) {
		mNCPInstance->change_ncp_state(UNINITIALIZED);
	}

	mNCPInstance->start_new_task(SpinelNCPTaskSendCommand::Factory(mNCPInstance)
		.set_callback(CallbackWithStatus(boost::bind(cb,kWPANTUNDStatus_Ok)))
		.add_command(SpinelPackData(SPINEL_FRAME_PACK_CMD_RESET))
		.finish()
	);
}

void
SpinelNCPControlInterface::begin_net_wake(uint8_t data, uint32_t flags, CallbackWithStatus cb)
{
	// TODO: Writeme!
	cb(kWPANTUNDStatus_FeatureNotImplemented);
}

void
SpinelNCPControlInterface::host_did_wake(CallbackWithStatus cb)
{
	// TODO: Writeme!
	cb(kWPANTUNDStatus_FeatureNotImplemented);
}

void
SpinelNCPControlInterface::begin_low_power(CallbackWithStatus cb)
{
	// TODO: Writeme!
	cb(kWPANTUNDStatus_FeatureNotImplemented);
}

void
SpinelNCPControlInterface::refresh_state(CallbackWithStatus cb)
{
	mNCPInstance->start_new_task(SpinelNCPTaskSendCommand::Factory(mNCPInstance)
		.set_callback(cb)
		.add_command(SpinelPackData(SPINEL_FRAME_PACK_CMD_NOOP))
		.finish()
	);
}

void
SpinelNCPControlInterface::data_poll(CallbackWithStatus cb)
{
	mNCPInstance->start_new_task(SpinelNCPTaskSendCommand::Factory(mNCPInstance)
		.set_callback(cb)
		.add_command(SpinelPackData(SPINEL_FRAME_PACK_CMD_PROP_VALUE_GET, SPINEL_PROP_STREAM_NET))
		.finish()
	);
}

void
SpinelNCPControlInterface::add_on_mesh_prefix(
	const struct in6_addr *prefix,
	bool defaultRoute,
	bool preferred,
	bool slaac,
	bool onMesh,
	OnMeshPrefixPriority priority,
	CallbackWithStatus cb
) {
	const static int kPreferenceOffset = 6;
	uint8_t flags = 0;
	SpinelNCPTaskSendCommand::Factory factory(mNCPInstance);

	require_action(prefix != NULL, bail, cb(kWPANTUNDStatus_InvalidArgument));
	require_action(mNCPInstance->mEnabled, bail, cb(kWPANTUNDStatus_InvalidWhenDisabled));

	switch (priority) {
	case ROUTE_HIGH_PREFERENCE:
		flags = (1 << kPreferenceOffset);
		break;

	case ROUTE_MEDIUM_PREFERENCE:
		flags = 0;
		break;

	case ROUTE_LOW_PREFRENCE:
		flags = (3 << kPreferenceOffset);
		break;
	}

	if (defaultRoute) {
		flags |= SPINEL_NET_FLAG_DEFAULT_ROUTE;
	}

	if (preferred) {
		flags |= SPINEL_NET_FLAG_PREFERRED;
	}

	if (slaac) {
		flags |= SPINEL_NET_FLAG_SLAAC;
	}

	if (onMesh) {
		flags |= SPINEL_NET_FLAG_ON_MESH;
	}

	factory.set_callback(cb);
	factory.set_lock_property(SPINEL_PROP_THREAD_ALLOW_LOCAL_NET_DATA_CHANGE);

	factory.add_command(SpinelPackData(
		SPINEL_FRAME_PACK_CMD_PROP_VALUE_INSERT(
			SPINEL_DATATYPE_IPv6ADDR_S
			SPINEL_DATATYPE_UINT8_S
			SPINEL_DATATYPE_BOOL_S
			SPINEL_DATATYPE_UINT8_S
		),
		SPINEL_PROP_THREAD_ON_MESH_NETS,
		prefix,
		IPV6_NETWORK_PREFIX_LENGTH,
		true,
		flags
	));

	mNCPInstance->start_new_task(factory.finish());

bail:
	return;
}

void
SpinelNCPControlInterface::remove_on_mesh_prefix(
	const struct in6_addr *prefix,
	CallbackWithStatus cb
) {
	uint8_t flags = 0;
	SpinelNCPTaskSendCommand::Factory factory(mNCPInstance);

	require_action(prefix != NULL, bail, cb(kWPANTUNDStatus_InvalidArgument));
	require_action(mNCPInstance->mEnabled, bail, cb(kWPANTUNDStatus_InvalidWhenDisabled));

	factory.set_callback(cb);
	factory.set_lock_property(SPINEL_PROP_THREAD_ALLOW_LOCAL_NET_DATA_CHANGE);

	factory.add_command(SpinelPackData(
		SPINEL_FRAME_PACK_CMD_PROP_VALUE_REMOVE(
			SPINEL_DATATYPE_IPv6ADDR_S
			SPINEL_DATATYPE_UINT8_S
			SPINEL_DATATYPE_BOOL_S
			SPINEL_DATATYPE_UINT8_S
		),
		SPINEL_PROP_THREAD_ON_MESH_NETS,
		prefix,
		IPV6_NETWORK_PREFIX_LENGTH,
		true,
		flags
	));

	mNCPInstance->start_new_task(factory.finish());

bail:
	return;
}

void
SpinelNCPControlInterface::add_external_route(
	const struct in6_addr *prefix,
	int prefix_len_in_bits,
	int domain_id,
	ExternalRoutePriority priority,
	CallbackWithStatus cb
) {
	require_action(prefix != NULL, bail, cb(kWPANTUNDStatus_InvalidArgument));
	require_action(prefix_len_in_bits >= 0, bail, cb(kWPANTUNDStatus_InvalidArgument));
	require_action(prefix_len_in_bits <= IPV6_MAX_PREFIX_LENGTH, bail, cb(kWPANTUNDStatus_InvalidArgument));
	require_action(mNCPInstance->mEnabled, bail, cb(kWPANTUNDStatus_InvalidWhenDisabled));

	mNCPInstance->start_new_task(SpinelNCPTaskSendCommand::Factory(mNCPInstance)
		.set_callback(cb)
		.add_command(SpinelPackData(
			SPINEL_FRAME_PACK_CMD_PROP_VALUE_INSERT(
				SPINEL_DATATYPE_IPv6ADDR_S
				SPINEL_DATATYPE_UINT8_S
				SPINEL_DATATYPE_BOOL_S
				SPINEL_DATATYPE_UINT8_S
			),
			SPINEL_PROP_THREAD_OFF_MESH_ROUTES,
			prefix,
			prefix_len_in_bits,
			true,
			convert_external_route_priority_to_flags(priority)
		))
		.set_lock_property(SPINEL_PROP_THREAD_ALLOW_LOCAL_NET_DATA_CHANGE)
		.finish()
	);

bail:
	return;
}

void
SpinelNCPControlInterface::joiner_add(
		const char *psk,
		uint32_t joiner_timeout,
		const uint8_t *addr,
		CallbackWithStatus cb
) {

	require_action(psk != NULL, bail, cb(kWPANTUNDStatus_InvalidArgument));
	require_action(mNCPInstance->mEnabled, bail, cb(kWPANTUNDStatus_InvalidWhenDisabled));

	if (addr) {
		mNCPInstance->start_new_task(SpinelNCPTaskSendCommand::Factory(mNCPInstance)
			.set_callback(cb)
			.add_command(SpinelPackData(
				SPINEL_FRAME_PACK_CMD_PROP_VALUE_INSERT(
					SPINEL_DATATYPE_UTF8_S
					SPINEL_DATATYPE_UINT32_S
					SPINEL_DATATYPE_EUI64_S
				),
				SPINEL_PROP_THREAD_JOINERS,
				psk,
				joiner_timeout,
				addr
			))
			.set_lock_property(SPINEL_PROP_THREAD_ALLOW_LOCAL_NET_DATA_CHANGE)
			.finish()
		);
	}
	else {
		mNCPInstance->start_new_task(SpinelNCPTaskSendCommand::Factory(mNCPInstance)
			.set_callback(cb)
			.add_command(SpinelPackData(
				SPINEL_FRAME_PACK_CMD_PROP_VALUE_INSERT(
					SPINEL_DATATYPE_UTF8_S
					SPINEL_DATATYPE_UINT32_S
				),
				SPINEL_PROP_THREAD_JOINERS,
				psk,
				joiner_timeout
			))
			.set_lock_property(SPINEL_PROP_THREAD_ALLOW_LOCAL_NET_DATA_CHANGE)
			.finish()
		);
	}

bail:
	return;
}

void
SpinelNCPControlInterface::remove_external_route(
	const struct in6_addr *prefix,
	int prefix_len_in_bits,
	int domain_id,
	CallbackWithStatus cb
) {
	require_action(prefix != NULL, bail, cb(kWPANTUNDStatus_InvalidArgument));
	require_action(prefix_len_in_bits >= 0, bail, cb(kWPANTUNDStatus_InvalidArgument));
	require_action(prefix_len_in_bits <= IPV6_MAX_PREFIX_LENGTH, bail, cb(kWPANTUNDStatus_InvalidArgument));
	require_action(mNCPInstance->mEnabled, bail, cb(kWPANTUNDStatus_InvalidWhenDisabled));

	mNCPInstance->start_new_task(SpinelNCPTaskSendCommand::Factory(mNCPInstance)
		.set_callback(cb)
		.add_command(SpinelPackData(
			SPINEL_FRAME_PACK_CMD_PROP_VALUE_REMOVE(
				SPINEL_DATATYPE_IPv6ADDR_S
				SPINEL_DATATYPE_UINT8_S
				SPINEL_DATATYPE_BOOL_S
				SPINEL_DATATYPE_UINT8_S
			),
			SPINEL_PROP_THREAD_OFF_MESH_ROUTES,
			prefix,
			prefix_len_in_bits,
			true,
			0
		))
		.set_lock_property(SPINEL_PROP_THREAD_ALLOW_LOCAL_NET_DATA_CHANGE)
		.finish()
	);

bail:
	return;
}

void
SpinelNCPControlInterface::permit_join(
	int seconds,
	uint8_t traffic_type,
	in_port_t traffic_port,
	bool network_wide,
	CallbackWithStatus cb
	)
{
	SpinelNCPTaskSendCommand::Factory factory(mNCPInstance);
	bool should_update_steering_data = false;
	uint8_t steering_data_addr[sizeof(mNCPInstance->mSteeringDataAddress)];

	int ret = kWPANTUNDStatus_Ok;

	if (!mNCPInstance->mEnabled) {
		ret = kWPANTUNDStatus_InvalidWhenDisabled;
		goto bail;
	}

	if (mNCPInstance->mCapabilities.count(SPINEL_CAP_OOB_STEERING_DATA)
		&& mNCPInstance->mSetSteeringDataWhenJoinable
	) {
		should_update_steering_data = true;
	}

	if (traffic_port == 0) {
		// If no port was explicitly set, default to the discovered
		// "Commissioner Port"  (“:MC”).
		traffic_port = htons(mNCPInstance->mCommissionerPort);
	}

	ret = mNCPInstance->set_commissioniner(seconds, traffic_type, traffic_port);

	require_noerr(ret, bail);

	factory.set_callback(cb);

	if (seconds > 0) {
		factory.add_command(SpinelPackData(
			SPINEL_FRAME_PACK_CMD_PROP_VALUE_SET(SPINEL_DATATYPE_UINT16_S),
			SPINEL_PROP_THREAD_ASSISTING_PORTS,
			ntohs(traffic_port)
		));

		memcpy(steering_data_addr, mNCPInstance->mSteeringDataAddress, sizeof(steering_data_addr));

	} else {

		factory.add_command(SpinelPackData(
			SPINEL_FRAME_PACK_CMD_PROP_VALUE_SET(SPINEL_DATATYPE_NULL_S),
			SPINEL_PROP_THREAD_ASSISTING_PORTS
		));

		memset(steering_data_addr, 0, sizeof(steering_data_addr));
	}

	if (should_update_steering_data) {
			factory.add_command(SpinelPackData(
				SPINEL_FRAME_PACK_CMD_PROP_VALUE_SET(SPINEL_DATATYPE_EUI64_S),
				SPINEL_PROP_THREAD_STEERING_DATA,
				steering_data_addr
			));
	}

	mNCPInstance->start_new_task(factory.finish());

bail:
	if (ret) {
		cb(ret);
	} else {
		if (!should_update_steering_data) {
			syslog(LOG_NOTICE, "PermitJoin: seconds=%d type=%d port=%d", seconds, traffic_type, ntohs(traffic_port));
		} else {
			syslog(
				LOG_NOTICE,
				"PermitJoin: seconds=%d type=%d port=%d, steering_data_addr=%02X%02X%02X%02X%02X%02X%02X%02X",
				seconds,
				traffic_type,
				ntohs(traffic_port),
				steering_data_addr[0], steering_data_addr[1], steering_data_addr[2], steering_data_addr[3],
				steering_data_addr[4], steering_data_addr[5], steering_data_addr[6], steering_data_addr[7]
			);
		}
	}
}

void
SpinelNCPControlInterface::netscan_start(
	const ValueMap& options,
	CallbackWithStatus cb
) {
	ChannelMask channel_mask(mNCPInstance->get_default_channel_mask());
	SpinelNCPTaskScan::ScanType scan_type;
	int scan_period = 0; 			   // per channel in ms
	bool joiner_flag = false;          // Scan for joiner only devices (used in discover scan).
	bool enable_filtering = false;     // Enable scan result filtering (used in discover scan).
	uint16_t pan_id_filter = 0xffff;   // PANID used for filtering, 0xFFFF to disable (used in discover scan.)

	// Channel mask
	if (options.count(kWPANTUNDValueMapKey_Scan_ChannelMask)) {
		channel_mask = any_to_int(options.at(kWPANTUNDValueMapKey_Scan_ChannelMask));
	}

	// Scan type
	if (options.count(kWPANTUNDValueMapKey_Scan_Discover)) {
		scan_type = SpinelNCPTaskScan::kScanTypeDiscover;

		if (options.count(kWPANTUNDValueMapKey_Scan_JoinerFalg)) {
			joiner_flag = any_to_bool(options.at(kWPANTUNDValueMapKey_Scan_JoinerFalg));
		}

		if (options.count(kWPANTUNDValueMapKey_Scan_EnableFiltering)) {
			enable_filtering = any_to_bool(options.at(kWPANTUNDValueMapKey_Scan_EnableFiltering));
		}

		if (options.count(kWPANTUNDValueMapKey_Scan_PANIDFilter)) {
			pan_id_filter = static_cast<uint16_t>(any_to_int(options.at(kWPANTUNDValueMapKey_Scan_PANIDFilter)));
		}

	} else {
		scan_type = SpinelNCPTaskScan::kScanTypeNet;
	}

	// Scan period
	if (options.count(kWPANTUNDValueMapKey_Scan_Period)) {
		scan_period = any_to_int(options.at(kWPANTUNDValueMapKey_Scan_Period));
	}

	if (scan_period <= 0) {
		scan_period = SpinelNCPTaskScan::kDefaultScanPeriod;
	}

	mNCPInstance->start_new_task(boost::shared_ptr<SpinelNCPTask>(
		new SpinelNCPTaskScan(
			mNCPInstance,
			boost::bind(cb,_1),
			channel_mask,
			scan_period,
			scan_type,
			joiner_flag,
			enable_filtering,
			pan_id_filter
		)
	));
}

void
SpinelNCPControlInterface::netscan_stop(CallbackWithStatus cb)
{
	cb(kWPANTUNDStatus_FeatureNotImplemented); // TODO: Start network scan
}

void
SpinelNCPControlInterface::energyscan_start(
	const ValueMap& options,
	CallbackWithStatus cb
) {
	ChannelMask channel_mask(mNCPInstance->get_default_channel_mask());

	if (options.count(kWPANTUNDProperty_NCPChannelMask)) {
		channel_mask = any_to_int(options.at(kWPANTUNDProperty_NCPChannelMask));
	}

	mNCPInstance->start_new_task(boost::shared_ptr<SpinelNCPTask>(
		new SpinelNCPTaskScan(
			mNCPInstance,
			boost::bind(cb,_1),
			channel_mask,
			SpinelNCPTaskScan::kDefaultScanPeriod,
			SpinelNCPTaskScan::kScanTypeEnergy
		)
	));
}

void
SpinelNCPControlInterface::energyscan_stop(CallbackWithStatus cb)
{
	cb(kWPANTUNDStatus_FeatureNotImplemented);
}

std::string
SpinelNCPControlInterface::get_name() {
	return mNCPInstance->get_name();
}

void
SpinelNCPControlInterface::mfg(
	const std::string& mfg_command,
	CallbackWithStatusArg1 cb
) {
	mNCPInstance->start_new_task(
		SpinelNCPTaskSendCommand::Factory(mNCPInstance)
			.set_callback(cb)
			.add_command(SpinelPackData(
				SPINEL_FRAME_PACK_CMD_PROP_VALUE_SET(SPINEL_DATATYPE_UTF8_S),
				SPINEL_PROP_NEST_STREAM_MFG,
				mfg_command.c_str()
			))
			.set_reply_format(SPINEL_DATATYPE_UTF8_S)
			.finish()
	);
}

const WPAN::NetworkInstance&
SpinelNCPControlInterface::get_current_network_instance()const
{
	return mNCPInstance->get_current_network_instance();
}


NCPInstance&
SpinelNCPControlInterface::get_ncp_instance()
{
	return (*mNCPInstance);
}

void
SpinelNCPControlInterface::pcap_to_fd(int fd, CallbackWithStatus cb)
{
	int ret = mNCPInstance->mPcapManager.insert_fd(fd);

	if (ret < 0) {
		syslog(LOG_ERR, "pcap_to_fd: Failed: \"%s\" (%d)", strerror(errno), errno);

		cb(kWPANTUNDStatus_Failure);

	} else {
		cb(kWPANTUNDStatus_Ok);
	}
}

void
SpinelNCPControlInterface::pcap_terminate(CallbackWithStatus cb)
{
	mNCPInstance->mPcapManager.close_fd_set(mNCPInstance->mPcapManager.get_fd_set());
	cb(kWPANTUNDStatus_Ok);
}

// ----------------------------------------------------------------------------
// MARK: -

void
SpinelNCPControlInterface::property_get_value(
	const std::string& in_key,
	CallbackWithStatusArg1 cb
) {
	mNCPInstance->property_get_value(in_key, cb);
}

void
SpinelNCPControlInterface::property_set_value(
	const std::string& key,
	const boost::any& value,
	CallbackWithStatus cb
) {
	mNCPInstance->property_set_value(key, value, cb);
}

void
SpinelNCPControlInterface::property_insert_value(
	const std::string& key,
	const boost::any& value,
	CallbackWithStatus cb
) {
	mNCPInstance->property_insert_value(key, value, cb);
}

void
SpinelNCPControlInterface::property_remove_value(
	const std::string& key,
	const boost::any& value,
	CallbackWithStatus cb
) {
	mNCPInstance->property_remove_value(key, value, cb);
}

// ----------------------------------------------------------------------------
// MARK: -

SpinelNCPControlInterface::ExternalRoutePriority
SpinelNCPControlInterface::convert_flags_to_external_route_priority(uint8_t flags)
{
	ExternalRoutePriority priority = ROUTE_MEDIUM_PREFERENCE;

	switch ((flags & SPINEL_NET_FLAG_PREFERENCE_MASK) >> SPINEL_NET_FLAG_PREFERENCE_OFFSET) {
		case 1:
			priority = ROUTE_HIGH_PREFERENCE;
			break;

		case 3:
			priority = ROUTE_LOW_PREFRENCE;
			break;

		case 0:
			priority = ROUTE_MEDIUM_PREFERENCE;
			break;
	}

	return priority;
}

uint8_t
SpinelNCPControlInterface::convert_external_route_priority_to_flags(ExternalRoutePriority priority)
{
	uint8_t flags;

	switch (priority) {
	case ROUTE_HIGH_PREFERENCE:
		flags = (1 << SPINEL_NET_FLAG_PREFERENCE_OFFSET);
		break;

	case ROUTE_MEDIUM_PREFERENCE:
		flags = (0 << SPINEL_NET_FLAG_PREFERENCE_OFFSET);
		break;

	case ROUTE_LOW_PREFRENCE:
		flags = (3 << SPINEL_NET_FLAG_PREFERENCE_OFFSET);
		break;
	}

	return flags;
}
