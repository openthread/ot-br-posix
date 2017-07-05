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
#include "NCPInstance.h"
#include "tunnel.h"
#include <syslog.h>
#include <errno.h>

#ifndef HAVE_DLFCN_H
#define HAVE_DLFCN_H	defined(__APPLE__)
#endif

#ifndef PKGLIBEXECDIR
#define PKGLIBEXECDIR	"/usr/local/libexec/wpantund"
#endif

#if HAVE_DLFCN_H
#include <dlfcn.h>
#ifndef RTLD_DEFAULT
#define RTLD_DEFAULT	((void*)0)
#endif
#ifndef RTLD_NEXT
#define RTLD_NEXT		((void*)-1l)
#endif
#endif

#if WPANTUND_PLUGIN_STATICLY_LINKED || FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
#undef HAVE_DLFCN_H
#endif

#ifndef WPANTUND_DEFAULT_NCP_PLUGIN
#if WPANTUND_PLUGIN_STATICLY_LINKED || FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
#define WPANTUND_DEFAULT_NCP_PLUGIN "default"
#else
#define WPANTUND_DEFAULT_NCP_PLUGIN "spinel"
#endif
#endif

using namespace nl;
using namespace wpantund;

typedef nl::wpantund::NCPInstance* (*NCPInstanceAllocatorType)(const nl::wpantund::NCPInstance::Settings& settings);

NCPInstance*
NCPInstance::alloc(const Settings& settings)
{
	NCPInstance* ret = NULL;
	std::string ncp_driver_name = WPANTUND_DEFAULT_NCP_PLUGIN;
	std::string symbol_name = "wpantund_ncpinstance_default_alloc";
	NCPInstanceAllocatorType allocator = NULL;

	if (settings.count(kWPANTUNDProperty_ConfigNCPDriverName)) {
		ncp_driver_name = settings.find(kWPANTUNDProperty_ConfigNCPDriverName)->second;
	}

	if (ncp_driver_name == "default") {
		ncp_driver_name = WPANTUND_DEFAULT_NCP_PLUGIN;
	}

#if WPANTUND_PLUGIN_STATICLY_LINKED || FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
	// Statically-linked plugin
	if ((allocator == NULL) && (ncp_driver_name == WPANTUND_DEFAULT_NCP_PLUGIN)) {
		allocator = &wpantund_ncpinstance_default_alloc;
	}
#else
	// Dynamically-loaded plugin
	std::string plugin_path;

	if ((ncp_driver_name.find('/') != std::string::npos)
	 || (ncp_driver_name.find('.') != std::string::npos)
	) {
		plugin_path = ncp_driver_name;
	} else {
		plugin_path = std::string("ncp-") + ncp_driver_name + ".so";
		symbol_name = std::string("wpantund_ncpinstance_") +ncp_driver_name + "_alloc";

		if( access( (std::string(PKGLIBEXECDIR "/") + plugin_path).c_str(), X_OK ) != -1 ) {
			plugin_path = std::string(PKGLIBEXECDIR "/") + plugin_path;
		}

		allocator = (NCPInstanceAllocatorType)dlsym(RTLD_DEFAULT, symbol_name.c_str());
	}

	if (allocator == NULL) {
		// We failed to immediately look up the allocator function,
		// so lets see if we can find a plug-in that we can open,
		// and then try again.

		void* dl_handle = NULL;

		dl_handle = dlopen(plugin_path.c_str(), RTLD_NOW|RTLD_LOCAL);

		if (dl_handle == NULL) {
			syslog(LOG_ERR, "Couldn't open plugin \"%s\", %s", plugin_path.c_str(), dlerror());
		} else {
			allocator = (NCPInstanceAllocatorType)dlsym(dl_handle, symbol_name.c_str());
		}
	}

#endif

	require_string(allocator != NULL, bail, "Unknown NCP Driver");

	ret = (*allocator)(settings);

bail:
	if (ret == NULL) {
		syslog(LOG_ERR, "Unable to load NCP driver \"%s\".", ncp_driver_name.c_str());
	}

	return ret;
}

NCPInstance::~NCPInstance()
{
}

void
NCPInstance::signal_fatal_error(int err)
{
	if (err == ERRORCODE_ERRNO) {
		syslog(LOG_CRIT, "NCPInstance: errno %d \"%s\"\n", errno, strerror(
				   errno));
	} else {
		syslog(LOG_CRIT, "NCPInstance: error %d\n", err);
	}
	mOnFatalError(err);
}
