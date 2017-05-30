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

#ifndef __wpantund__NCPInstance__
#define __wpantund__NCPInstance__

#define BOOST_SIGNALS_NO_DEPRECATION_WARNING 1

#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS 1
#endif

#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS 1
#endif

#include <stdint.h>
#include <string>
#include <vector>
#include <boost/signals2/signal.hpp>
#include <boost/any.hpp>
#include <list>
#include <arpa/inet.h>

#include "NetworkInstance.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <cstring>
#include "IPv6PacketMatcher.h"
#include "Data.h"
#include "NilReturn.h"
#include "NCPControlInterface.h"
#include "tunnel.h"
#include "SocketAdapter.h"
#include "TunnelIPv6Interface.h"
#include "NCPConstants.h"
#include "wpan-error.h"
#include "StatCollector.h"

#define ERRORCODE_OK            (0)
#define ERRORCODE_HELP          (1)
#define ERRORCODE_BADARG        (2)
#define ERRORCODE_NOCOMMAND     (3)
#define ERRORCODE_UNKNOWN       (4)
#define ERRORCODE_BADCOMMAND    (5)
#define ERRORCODE_NOREADLINE    (6)
#define ERRORCODE_QUIT          (7)
#define ERRORCODE_BADCONFIG     (8)
#define ERRORCODE_ERRNO         (9)

#define ERRORCODE_INTERRUPT     (128 + SIGINT)
#define ERRORCODE_SIGHUP        (128 + SIGHUP)

#define WTD_WEAK                __attribute__((weak))

#define WPANTUND_DECLARE_NCPINSTANCE_PLUGIN(short_name, class_name) \
	extern "C" nl::wpantund::NCPInstance* wpantund_ncpinstance_ ## short_name ## _alloc(const nl::wpantund::NCPInstance::Settings& settings)

#define WPANTUND_DEFINE_NCPINSTANCE_PLUGIN(short_name, class_name) \
	nl::wpantund::NCPInstance* \
	wpantund_ncpinstance_ ## short_name ## _alloc(const nl::wpantund::NCPInstance::Settings& settings) { \
		return new class_name(settings); \
	} \
	nl::wpantund::NCPInstance* \
	wpantund_ncpinstance_default_alloc(const nl::wpantund::NCPInstance::Settings& settings) { \
		return new class_name(settings); \
	}

namespace nl {

namespace wpantund {
class NCPControlInterface;

class NCPInstance {
public:
	typedef std::map<std::string, std::string> Settings;

public:
	static NCPInstance* alloc(
		const Settings& settings = Settings()
	);

	virtual ~NCPInstance();

	virtual const std::string &get_name(void) = 0;
	virtual NCPControlInterface& get_control_interface(void) = 0;
	virtual StatCollector& get_stat_collector(void) = 0;

	virtual void set_socket_adapter(const boost::shared_ptr<SocketAdapter> &adapter) = 0;

	virtual cms_t get_ms_to_next_event(void) = 0;
	virtual void process(void) = 0;
	virtual int update_fd_set(fd_set *read_fd_set, fd_set *write_fd_set, fd_set *error_fd_set, int *max_fd, cms_t *timeout) = 0;

public:
	void signal_fatal_error(int err);
	SignalWithStatus mOnFatalError;
}; // class NCPInstance

}; // namespace wpantund
}; // namespace nl

extern "C" nl::wpantund::NCPInstance* wpantund_ncpinstance_default_alloc(const nl::wpantund::NCPInstance::Settings& settings);

#endif /* defined(__wpantund__NCPInstance__) */
