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

#ifndef __WPAN_DUMMY_NCP_H__
#define __WPAN_DUMMY_NCP_H__ 1

#include "NCPInstance.h"
#include "NCPControlInterface.h"
#include "NCPMfgInterface_v1.h"
#include "nlpt.h"
#include "Callbacks.h"
#include "EventHandler.h"

#include <queue>
#include <set>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>

namespace nl {
namespace wpantund {

class SpinelNCPInstance;

class SpinelNCPControlInterface : public NCPControlInterface, public NCPMfgInterface_v1 {
public:
	friend class SpinelNCPInstance;

	SpinelNCPControlInterface(SpinelNCPInstance* instance_pointer);
	virtual ~SpinelNCPControlInterface() { }

	virtual const WPAN::NetworkInstance& get_current_network_instance(void)const;

	virtual void join(
		const ValueMap& options,
		CallbackWithStatus cb = NilReturn()
	);

	virtual void form(
		const ValueMap& options,
		CallbackWithStatus cb = NilReturn()
	);

	virtual void leave(
		CallbackWithStatus cb = NilReturn()
	);

	virtual void attach(
		CallbackWithStatus cb = NilReturn()
	);

	virtual void begin_low_power(
		CallbackWithStatus cb = NilReturn()
	);

	virtual void netscan_start(
		const ValueMap& options,
		CallbackWithStatus cb = NilReturn()
	);

	virtual void netscan_stop(
		CallbackWithStatus cb = NilReturn()
	);

	virtual void energyscan_start(
		const ValueMap& options,
		CallbackWithStatus cb = NilReturn()
	);

	virtual void energyscan_stop(
		CallbackWithStatus cb = NilReturn()
	);

	virtual void begin_net_wake(
		uint8_t data,
		uint32_t flags,
		CallbackWithStatus cb = NilReturn()
	);

	virtual void reset(
		CallbackWithStatus cb = NilReturn()
	);

	virtual void permit_join(
		int seconds = 15 * 60,
		uint8_t commissioning_traffic_type = 0xFF,
		in_port_t commissioning_traffic_port = 0,
		bool network_wide = false,
		CallbackWithStatus cb = NilReturn()
	);

	virtual void refresh_state(
		CallbackWithStatus cb = NilReturn()
	);

	virtual void property_get_value(
		const std::string& key,
		CallbackWithStatusArg1 cb
	);

	virtual void property_set_value(
		const std::string& key,
		const boost::any& value,
		CallbackWithStatus cb
	);

	virtual void property_insert_value(
		const std::string& key,
		const boost::any& value,
		CallbackWithStatus cb
	);

	virtual void property_remove_value(
		const std::string& key,
		const boost::any& value,
		CallbackWithStatus cb
	);

	virtual void add_on_mesh_prefix(
		const struct in6_addr *prefix,
		bool defaultRoute,
		bool preferred,
		bool slaac,
		bool onMesh,
		OnMeshPrefixPriority priority,
		CallbackWithStatus cb = NilReturn()
	);

	virtual void remove_on_mesh_prefix(
		const struct in6_addr *prefix,
		CallbackWithStatus cb = NilReturn()
	);

	virtual void add_external_route(
		const struct in6_addr *prefix,
		int prefix_len_in_bits,
		int domain_id,
		ExternalRoutePriority priority,
		CallbackWithStatus cb = NilReturn()
	);

	virtual void remove_external_route(
		const struct in6_addr *prefix,
		int prefix_len_in_bits,
		int domain_id,
		CallbackWithStatus cb = NilReturn()
	);

	virtual void joiner_add(
		const char *psk,
		uint32_t joiner_timeout,
		const uint8_t *addr,
		CallbackWithStatus cb = NilReturn()
	);

	virtual void data_poll(
		CallbackWithStatus cb = NilReturn()
	);

	virtual void host_did_wake(
		CallbackWithStatus cb = NilReturn()
	);

	virtual std::string get_name(void);

	virtual NCPInstance& get_ncp_instance(void);

	virtual void pcap_to_fd(int fd,
		CallbackWithStatus cb = NilReturn()
	);

	virtual void pcap_terminate(
		CallbackWithStatus cb = NilReturn()
	);

	/******************* NCPMfgInterface_v1 ********************/
	virtual void mfg(
		const std::string& mfg_command,
		CallbackWithStatusArg1 cb = NilReturn()
	);

	static ExternalRoutePriority convert_flags_to_external_route_priority(uint8_t flags);
	static uint8_t convert_external_route_priority_to_flags(ExternalRoutePriority priority);

private:

	SpinelNCPInstance *mNCPInstance;

};

}; // namespace wpantund
}; // namespace nl


#endif
