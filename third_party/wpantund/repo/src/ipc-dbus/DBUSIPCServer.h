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
 *      Declaration of the DBus IPCServer subclass.
 *
 */

#ifndef wpantund_DBUSIPCServer_h
#define wpantund_DBUSIPCServer_h

#include "IPCServer.h"
#include <map>
#include <dbus/dbus.h>
#include <boost/signals2/signal.hpp>
#include <boost/bind.hpp>

#include "DBusIPCAPI_v0.h"
#include "DBusIPCAPI_v1.h"

namespace nl {
namespace wpantund {

class DBUSIPCServer : public IPCServer {
public:

	DBUSIPCServer();
	virtual ~DBUSIPCServer();

	virtual int add_interface(NCPControlInterface* instance);
	virtual cms_t get_ms_to_next_event(void );
	virtual void process(void);
	virtual int update_fd_set(fd_set *read_fd_set, fd_set *write_fd_set, fd_set *error_fd_set, int *max_fd, cms_t *timeout);

private:
	DBusHandlerResult message_handler(
	    DBusConnection *connection,
	    DBusMessage *   message
	    );

	static DBusHandlerResult dbus_message_handler(
	    DBusConnection *connection,
	    DBusMessage *   message,
	    void *                  user_data
	    );

	void interface_added(const std::string& interface_name);
	void interface_removed(const std::string& interface_name);

private:
	DBusConnection *mConnection;
	std::map<std::string, NCPControlInterface*> mInterfaceMap;
	std::map<std::string, std::string> mExternalInterfaceMap;
	DBusIPCAPI_v0 mAPI_v0;
	DBusIPCAPI_v1 mAPI_v1;
};

};
};


#endif
