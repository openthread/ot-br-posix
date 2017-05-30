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
 *      This file contains the interface for a C++ wrapper around the
 *      `tunnel.c`/`tunnel.h` interface.
 *
 */

#ifndef __wpantund__TunnelInterface__
#define __wpantund__TunnelInterface__

#include "tunnel.h"
#include "netif-mgmt.h"
#include <cstdio>
#include <string>
#include <errno.h>
#include <netinet/in.h>
#include <net/if.h>
#include "UnixSocket.h"
#include <set>
#include "IPv6Helpers.h"
#include <boost/signals2/signal.hpp>

class TunnelIPv6Interface : public nl::UnixSocket
{

public:
	TunnelIPv6Interface(const std::string& interface_name = "", int mtu = 1280);

	virtual ~TunnelIPv6Interface();

	const std::string& get_interface_name(void);

	int get_last_error(void);

	bool is_up(void);
	int set_up(bool isUp);

	bool is_running(void);
	int set_running(bool isRunning);

	// This "online" is a bit of a mashup of "up" and "running".
	// This is going to be phased out.
	bool is_online(void);
	int set_online(bool isOnline);

	const struct in6_addr& get_realm_local_address()const;

	bool add_address(const struct in6_addr *addr, int prefixlen = 64);
	bool remove_address(const struct in6_addr *addr, int prefixlen = 64);

	bool add_route(const struct in6_addr *route, int prefixlen = 64);
	bool remove_route(const struct in6_addr *route, int prefixlen = 64);

	virtual void reset();
	virtual ssize_t write(const void* data, size_t len);
	virtual ssize_t read(void* data, size_t len);

	virtual int process(void);
	virtual int update_fd_set(fd_set *read_fd_set, fd_set *write_fd_set, fd_set *error_fd_set, int *max_fd, cms_t *timeout);

public: // Signals

	boost::signals2::signal<void(const struct in6_addr&, int)> mAddressWasAdded;
	boost::signals2::signal<void(const struct in6_addr&, int)> mAddressWasRemoved;

	// void linkStateChanged(isUp, isRunning);
	boost::signals2::signal<void(bool, bool)> mLinkStateChanged;

private:
	void setup_signals();

	void on_link_state_changed(bool isUp, bool isRunning);
private:
	std::string mInterfaceName;
	int mLastError;

	int mNetlinkFD;
	int mNetifMgmtFD;

	bool mIsRunning;
	bool mIsUp;

	std::set<struct in6_addr> mAddresses;
};
#endif /* defined(__wpantund__TunnelInterface__) */
