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

#ifndef wpantund_DBusIPCAPI_v1_h
#define wpantund_DBusIPCAPI_v1_h

#include <map>
#include <list>

#include <dbus/dbus.h>

#include <boost/bind.hpp>
#include <boost/any.hpp>
#include <boost/function.hpp>

#include "NetworkInstance.h"
#include "NCPTypes.h"
#include "Data.h"
#include "time-utils.h"

namespace nl {
namespace wpantund {

class NCPControlInterface;

class DBusIPCAPI_v1 {
public:
	DBusIPCAPI_v1(DBusConnection *connection);
	~DBusIPCAPI_v1();

	int add_interface(NCPControlInterface* interface);

private:

	DBusHandlerResult message_handler(
		NCPControlInterface* interface,
		DBusConnection *connection,
		DBusMessage *message
    );

	static DBusHandlerResult dbus_message_handler(
		DBusConnection *connection,
		DBusMessage *message,
		void *user_data
	);

	void init_callback_tables(void);

	std::string path_for_iface(NCPControlInterface* interface);

	// ------------------------------------------------------------------------

	void CallbackWithStatus_Helper(int ret, DBusMessage *original_message);
	void CallbackWithStatusArg1_Helper(int ret, const boost::any& value, DBusMessage *original_message);

	void status_response_helper(int ret, NCPControlInterface* interface, DBusMessage *original_message);

	// TODO: Remove these...
	//void scan_response_helper(int ret, DBusMessage *original_message);
	//void energy_scan_response_helper(int ret, DBusMessage *original_message);

	// ------------------------------------------------------------------------

	void property_changed(NCPControlInterface* interface, const std::string& key, const boost::any& value);
	void received_beacon(NCPControlInterface* interface, const WPAN::NetworkInstance& network);
	void received_energy_scan_result(NCPControlInterface* interface, const EnergyScanResultEntry& energy_scan_result);

	// ------------------------------------------------------------------------

	DBusHandlerResult interface_route_add_handler(
		NCPControlInterface* interface,
		DBusMessage *        message
	);

	DBusHandlerResult interface_route_remove_handler(
		NCPControlInterface* interface,
		DBusMessage *        message
	);

	DBusHandlerResult interface_reset_handler(
		NCPControlInterface* interface,
		DBusMessage *        message
	);

	DBusHandlerResult interface_status_handler(
		NCPControlInterface* interface,
		DBusMessage *        message
	);

	DBusHandlerResult interface_join_handler(
		NCPControlInterface* interface,
		DBusMessage *        message
	);

	DBusHandlerResult interface_form_handler(
		NCPControlInterface* interface,
		DBusMessage *        message
	);

	DBusHandlerResult interface_leave_handler(
		NCPControlInterface* interface,
		DBusMessage *        message
	);

	DBusHandlerResult interface_attach_handler(
		NCPControlInterface* interface,
		DBusMessage *        message
	);

	DBusHandlerResult interface_begin_low_power_handler(
		NCPControlInterface* interface,
		DBusMessage *        message
	);

	DBusHandlerResult interface_host_did_wake_handler(
		NCPControlInterface* interface,
		DBusMessage *        message
	);

	DBusHandlerResult interface_pcap_to_fd_handler(
		NCPControlInterface* interface,
		DBusMessage *        message
	);

	DBusHandlerResult interface_pcap_terminate_handler(
		NCPControlInterface* interface,
		DBusMessage *        message
	);

	DBusHandlerResult interface_get_prop_handler(
		NCPControlInterface* interface,
		DBusMessage *        message
	);

	DBusHandlerResult interface_set_prop_handler(
		NCPControlInterface* interface,
		DBusMessage *        message
	);

	DBusHandlerResult interface_net_scan_start_handler(
		NCPControlInterface* interface,
		DBusMessage *        message
	);

	DBusHandlerResult interface_net_scan_stop_handler(
		NCPControlInterface* interface,
		DBusMessage *        message
	);

	DBusHandlerResult interface_energy_scan_start_handler(
		NCPControlInterface* interface,
		DBusMessage *        message
	);

	DBusHandlerResult interface_energy_scan_stop_handler(
		NCPControlInterface* interface,
		DBusMessage *        message
	);

	DBusHandlerResult interface_data_poll_handler(
		NCPControlInterface* interface,
		DBusMessage *        message
	);

	DBusHandlerResult interface_config_gateway_handler(
		NCPControlInterface* interface,
		DBusMessage *        message
	);

	DBusHandlerResult interface_mfg_handler(
		NCPControlInterface* interface,
		DBusMessage *        message
	);

private:
	typedef DBusHandlerResult (interface_handler_cb)(
		NCPControlInterface*,
		DBusMessage *
	);

	DBusConnection *mConnection;
	std::map<std::string, boost::function<interface_handler_cb> > mInterfaceCallbackTable;
}; // class DBusIPCAPI_v1

}; // namespace nl
}; // namespace wpantund


#endif
