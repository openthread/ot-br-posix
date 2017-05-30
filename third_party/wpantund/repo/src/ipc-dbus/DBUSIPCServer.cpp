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
 *      Implementation of the DBus IPCServer subclass.
 *
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef ASSERT_MACROS_USE_SYSLOG
#define ASSERT_MACROS_USE_SYSLOG 1
#endif

#include <stdio.h>
#include "DBUSIPCServer.h"
#include <dbus/dbus.h>
#if USING_GLIB
#include <glib.h>
#include <glib-object.h>
#endif
#include <string.h>
#include "NCPControlInterface.h"
#include "assert-macros.h"

#include <boost/bind.hpp>
#include "DBUSHelpers.h"
#include <errno.h>
#include <algorithm>
#include "any-to.h"
#include "wpan-dbus-v0.h"

using namespace DBUSHelpers;
using namespace nl;
using namespace nl::wpantund;

static const char gDBusObjectManagerMatchString[] =
	"type='signal'"
	",interface='" WPAN_TUNNEL_DBUS_INTERFACE "'"
	;

static DBusConnection *
get_dbus_connection()
{
	static DBusConnection* connection = NULL;
	DBusError error;
	dbus_error_init(&error);

	if (connection == NULL) {
		syslog(LOG_DEBUG, "Getting DBus connection");

		connection = dbus_bus_get(DBUS_BUS_STARTER, &error);

		if (!connection) {
			dbus_error_free(&error);
			dbus_error_init(&error);
			connection = dbus_bus_get(DBUS_BUS_SYSTEM, &error);
		}

		require_string(connection != NULL, bail, error.message);

		syslog(LOG_DEBUG, "Registering DBus connection");

		dbus_bus_register(connection, &error);

		require_string(error.name == NULL, bail, error.message);

		syslog(LOG_DEBUG, "Requesting DBus connection name %s", WPAN_TUNNEL_DBUS_NAME);

		dbus_bus_request_name(
			connection,
			WPAN_TUNNEL_DBUS_NAME,
			0,
			&error
		);

		require_string(error.name == NULL, bail, error.message);
	}

bail:
	if (error.message) {
		throw std::runtime_error(error.message);
	}
	dbus_error_free(&error);

	return connection;
}

DBUSIPCServer::DBUSIPCServer():
	mConnection(get_dbus_connection()),
	mAPI_v0(mConnection),
	mAPI_v1(mConnection)
{
	DBusError error;

	dbus_error_init(&error);

	static const DBusObjectPathVTable ipc_interface_vtable = {
		NULL,
		&DBUSIPCServer::dbus_message_handler,
	};

	require(
		dbus_connection_register_object_path(
			mConnection,
			WPAN_TUNNEL_DBUS_PATH,
			&ipc_interface_vtable,
			(void*)this
		),
		bail
	);

	dbus_bus_add_match(
		mConnection,
		gDBusObjectManagerMatchString,
		&error
	);
	require_string(error.name == NULL, bail, error.message);

	dbus_connection_add_filter(mConnection, &DBUSIPCServer::dbus_message_handler, (void*)this, NULL);

	syslog(LOG_NOTICE, "Ready. Using DBUS bus \"%s\"", dbus_bus_get_unique_name(mConnection));

bail:
	if (error.message) {
		throw std::runtime_error(error.message);
	}
	dbus_error_free(&error);
}

DBUSIPCServer::~DBUSIPCServer()
{
	dbus_bus_remove_match(
		mConnection,
		gDBusObjectManagerMatchString,
		NULL
	);
	dbus_connection_unref(mConnection);
}

cms_t
DBUSIPCServer::get_ms_to_next_event()
{
	cms_t ret = CMS_DISTANT_FUTURE;

	/* We could set up some sort of complicated mechanism
	 * using the dbus_timer objects to actually calculate this
	 * correctly, however we aren't really using any of the
	 * timer-dependent stuff in DBus. As such, the following
	 * seems to suffice. If we want wpantund to do things
	 * like handle response timeouts in a timely manner then
	 * we will need to go ahead and fully implement such a
	 * mechanism. */

	if (dbus_connection_get_dispatch_status(mConnection) == DBUS_DISPATCH_DATA_REMAINS) {
		ret = 0;
	}

	if (dbus_connection_has_messages_to_send(mConnection)) {
		ret = 0;
	}

	return ret;
}

void
DBUSIPCServer::process(void)
{
	dbus_connection_read_write_dispatch(mConnection, 0);
}

int
DBUSIPCServer::update_fd_set(fd_set *read_fd_set, fd_set *write_fd_set, fd_set *error_fd_set, int *max_fd, cms_t *timeout)
{
	int ret = -1;
	int unix_fd = -1;

	require(dbus_connection_get_unix_fd(mConnection, &unix_fd), bail);

	if (read_fd_set != NULL) {
		FD_SET(unix_fd, read_fd_set);
	}

	if (error_fd_set != NULL) {
		FD_SET(unix_fd, error_fd_set);
	}

	if ((write_fd_set != NULL) && dbus_connection_has_messages_to_send(mConnection)) {
		FD_SET(unix_fd, write_fd_set);
	}

	if ((max_fd != NULL)) {
		*max_fd = std::max(*max_fd, unix_fd);
	}

	if (timeout != NULL) {
		*timeout = std::min(*timeout, get_ms_to_next_event());
	}

	ret = 0;
bail:
	return ret;
}



void
DBUSIPCServer::interface_added(const std::string& interface_name)
{
	DBusMessage* signal;
	const char *interface_name_cstr = interface_name.c_str();

	syslog(LOG_DEBUG,
	       "DBus Sending Interface Added Signal for %s",
	       interface_name_cstr);
	signal = dbus_message_new_signal(
	    WPAN_TUNNEL_DBUS_PATH,
	    WPAN_TUNNEL_DBUS_INTERFACE,
	    WPAN_TUNNEL_SIGNAL_INTERFACE_ADDED
	    );
	dbus_message_append_args(
	    signal,
	    DBUS_TYPE_STRING, &interface_name_cstr,
	    DBUS_TYPE_INVALID
	    );

	dbus_connection_send(mConnection, signal, NULL);
	dbus_message_unref(signal);
}

void
DBUSIPCServer::interface_removed(const std::string& interface_name)
{
	DBusMessage* signal;
	const char *interface_name_cstr = interface_name.c_str();

	syslog(LOG_DEBUG,
	       "DBus Sending Interface Added Signal for %s",
	       interface_name_cstr);
	signal = dbus_message_new_signal(
	    WPAN_TUNNEL_DBUS_PATH,
	    WPAN_TUNNEL_DBUS_INTERFACE,
	    "InterfaceRemoved"
	    );
	dbus_message_append_args(
	    signal,
	    DBUS_TYPE_STRING, &interface_name_cstr,
	    DBUS_TYPE_INVALID
	    );
	dbus_connection_send(mConnection, signal, NULL);
	dbus_message_unref(signal);
}

int
DBUSIPCServer::add_interface(NCPControlInterface* instance)
{
	std::string name = instance->get_name();

	mInterfaceMap[name] = instance;

	mAPI_v0.add_interface(instance);
	mAPI_v1.add_interface(instance);

	interface_added(name);

	return 0;
}

DBusHandlerResult
DBUSIPCServer::message_handler(
    DBusConnection *connection,
    DBusMessage *message
    )
{
	DBusHandlerResult ret = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	if (dbus_message_is_method_call(message, WPAN_TUNNEL_DBUS_INTERFACE,
	                                WPAN_TUNNEL_CMD_GET_INTERFACES)) {
		DBusMessage *reply = dbus_message_new_method_return(message);
		DBusMessageIter iter, array_iter;
		dbus_message_iter_init_append(reply, &iter);

		dbus_message_iter_open_container(
		    &iter,
		    DBUS_TYPE_ARRAY,
			DBUS_TYPE_ARRAY_AS_STRING
		    DBUS_TYPE_STRING_AS_STRING,
		    &array_iter
		    );
		{
			std::map<std::string, NCPControlInterface*>::const_iterator i;
			std::map<std::string,
					 NCPControlInterface*>::const_iterator end =
				mInterfaceMap.end();
			for (i = mInterfaceMap.begin(); i != end; ++i) {
				DBusMessageIter item_iter;
				const char* name = i->first.c_str();
				const char* bus_name = dbus_bus_get_unique_name(connection);
				dbus_message_iter_open_container(
					&array_iter,
					DBUS_TYPE_ARRAY,
					DBUS_TYPE_STRING_AS_STRING,
					&item_iter
					);
				dbus_message_iter_append_basic(&item_iter, DBUS_TYPE_STRING,
											   &name);
				dbus_message_iter_append_basic(&item_iter, DBUS_TYPE_STRING,
											   &bus_name);
				dbus_message_iter_close_container(&array_iter, &item_iter);
			}
		}
		{
			std::map<std::string, std::string>::const_iterator i;
			std::map<std::string, std::string>::const_iterator end =
				mExternalInterfaceMap.end();
			for (i = mExternalInterfaceMap.begin(); i != end; ++i) {
				DBusMessageIter item_iter;
				const char* name = i->first.c_str();
				const char* bus_name = i->second.c_str();
				dbus_message_iter_open_container(
					&array_iter,
					DBUS_TYPE_ARRAY,
					DBUS_TYPE_STRING_AS_STRING,
					&item_iter
					);
				dbus_message_iter_append_basic(&item_iter, DBUS_TYPE_STRING,
											   &name);
				dbus_message_iter_append_basic(&item_iter, DBUS_TYPE_STRING,
											   &bus_name);
				dbus_message_iter_close_container(&array_iter, &item_iter);
			}
		}

		dbus_message_iter_close_container(&iter, &array_iter);

		dbus_connection_send(connection, reply, NULL);
		dbus_message_unref(reply);

		ret = DBUS_HANDLER_RESULT_HANDLED;
	} else if (dbus_message_is_signal(message, WPAN_TUNNEL_DBUS_INTERFACE, WPAN_TUNNEL_SIGNAL_INTERFACE_ADDED)) {
		ret = DBUS_HANDLER_RESULT_HANDLED;
		if (!strequal(dbus_message_get_sender(message), dbus_bus_get_unique_name(connection))) {
			const char* interface_name = NULL;
			dbus_message_get_args(
				message, NULL,
				DBUS_TYPE_STRING, &interface_name,
				DBUS_TYPE_INVALID
			);
			if (NULL != interface_name && (mInterfaceMap.count(interface_name) == 0)) {
				mExternalInterfaceMap[interface_name] = dbus_message_get_sender(message);
			}
		}
	} else if (dbus_message_is_signal(message, WPAN_TUNNEL_DBUS_INTERFACE, WPAN_TUNNEL_SIGNAL_INTERFACE_REMOVED)) {
		ret = DBUS_HANDLER_RESULT_HANDLED;
		if (!strequal(dbus_message_get_sender(message), dbus_bus_get_unique_name(connection))) {
			const char* interface_name = NULL;
			dbus_message_get_args(
				message, NULL,
				DBUS_TYPE_STRING, &interface_name,
				DBUS_TYPE_INVALID
			);
			if ((NULL != interface_name)
				&& (mInterfaceMap.count(interface_name) == 0)
				&& (mExternalInterfaceMap.count(interface_name) == 1)
				&& (mExternalInterfaceMap[interface_name] == dbus_message_get_sender(message))
			) {
				mExternalInterfaceMap.erase(interface_name);
			}
		}
	} else if (dbus_message_is_method_call(message, WPAN_TUNNEL_DBUS_INTERFACE,
	                                WPAN_TUNNEL_CMD_GET_VERSION)) {
		DBusMessage *reply = dbus_message_new_method_return(message);
		uint32_t version = WPAN_TUNNEL_DBUS_VERSION;
		dbus_message_append_args(
			reply,
			DBUS_TYPE_UINT32, &version,
			DBUS_TYPE_INVALID
			);

		dbus_connection_send(connection, reply, NULL);
		dbus_message_unref(reply);

		ret = DBUS_HANDLER_RESULT_HANDLED;
	}

	return ret;
}

DBusHandlerResult
DBUSIPCServer::dbus_message_handler(
    DBusConnection *connection,
    DBusMessage *   message,
    void *                  user_data
    )
{
	return ((DBUSIPCServer*)user_data)->message_handler(connection, message);
}
