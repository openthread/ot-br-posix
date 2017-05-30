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
 *		This file implements the main program entry point for the
 *		WPAN Tunnel Driver, masterfuly named `wpantund`.
 *
 */

#define DEBUG 1

#if HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef ASSERT_MACROS_USE_SYSLOG
#define ASSERT_MACROS_USE_SYSLOG 1
#endif

#ifndef ASSERT_MACROS_SQUELCH
#define ASSERT_MACROS_SQUELCH 0
#endif

#include "assert-macros.h"

#include "wpantund.h"

#include "config-file.h"
#include "args.h"
#include "pt.h"
#include "tunnel.h"
#include "socket-utils.h"
#include "string-utils.h"
#include "version.h"
#include "SuperSocket.h"
#include "Timer.h"

#include "DBUSIPCServer.h"
#include "NCPControlInterface.h"
#include "NCPInstance.h"

#include "nlpt.h"

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <string.h>
#include <ctype.h>

#include <unistd.h>
#include <signal.h>

#include <poll.h>
#include <sys/select.h>

#include <syslog.h>
#include <errno.h>
#include <libgen.h>

#include <exception>
#include <algorithm>
#include <memory>

#include "any-to.h"
#include "sec-random.h"

#include <boost/shared_ptr.hpp>
using ::boost::shared_ptr;

#if HAVE_PWD_H
#include <pwd.h>
#endif

#ifndef PREFIX
#define PREFIX "/usr/local"
#endif

#ifndef SYSCONFDIR
#define SYSCONFDIR PREFIX "/etc"
#endif

#ifndef DEFAULT_MAX_LOG_LEVEL
#if DEBUG
#define DEFAULT_MAX_LOG_LEVEL	LOG_INFO
#else
#define DEFAULT_MAX_LOG_LEVEL	LOG_NOTICE
#endif
#endif

#ifndef WPANTUND_DEFAULT_PRIV_DROP_USER
#define WPANTUND_DEFAULT_PRIV_DROP_USER     NULL
#endif

#ifndef WPANTUND_DEFAULT_CHROOT_PATH
#define WPANTUND_DEFAULT_CHROOT_PATH        NULL
#endif

#ifndef WPANTUND_BACKTRACE
#define WPANTUND_BACKTRACE	(HAVE_EXECINFO_H || __APPLE__)
#endif

#if WPANTUND_BACKTRACE
#include <execinfo.h>
#if HAVE_ASM_SIGCONTEXT
#include <asm/sigcontext.h>
#endif
#ifndef FAULT_BACKTRACE_STACK_DEPTH
#define FAULT_BACKTRACE_STACK_DEPTH		20
#endif
#endif

#ifndef SOURCE_VERSION
#define SOURCE_VERSION		PACKAGE_VERSION
__BEGIN_DECLS
#include "version.c.in"
__END_DECLS
#endif

using namespace nl;
using namespace wpantund;

static arg_list_item_t option_list[] = {
	{ 'h', "help",   NULL,	"Print Help"},
	{ 'd', "debug",  "<level>", "Enable debugging mode"},
	{ 'c', "config", "<filename>", "Config File"},
	{ 'o', "option", "<option-string>", "Config option"},
	{ 'I', "interface", "<iface>", "Network interface name"},
	{ 's', "socket", "<socket>", "Socket file"},
	{ 'b', "baudrate", "<integer>", "Baudrate"},
	{ 'v', "version", NULL, "Print version" },
#if HAVE_PWD_H
	{ 'u', "user", NULL, "Username for dropping privileges" },
#endif
	{ 0 }
};

static int gRet;

static const char* gProcessName = "wpantund";
static const char* gPIDFilename = NULL;
static const char* gChroot = WPANTUND_DEFAULT_CHROOT_PATH;

#if HAVE_PWD_H
static const char* gPrivDropToUser = WPANTUND_DEFAULT_PRIV_DROP_USER;
#endif

/* ------------------------------------------------------------------------- */
/* MARK: Signal Handlers */

static sig_t gPreviousHandlerForSIGINT;
static sig_t gPreviousHandlerForSIGTERM;

static void
signal_SIGINT(int sig)
{
	static const char message[] = "\nCaught SIGINT!\n";

	gRet = ERRORCODE_INTERRUPT;

	// Can't use syslog() because it isn't async signal safe.
	// So we write to stderr
	(void)write(STDERR_FILENO, message, sizeof(message)-1);

	// Restore the previous handler so that if we end up getting
	// this signal again we peform the system default action.
	signal(SIGINT, gPreviousHandlerForSIGINT);
	gPreviousHandlerForSIGINT = NULL;
}

static void
signal_SIGTERM(int sig)
{
	static const char message[] = "\nCaught SIGTERM!\n";

	gRet = ERRORCODE_QUIT;

	// Can't use syslog() because it isn't async signal safe.
	// So we write to stderr
	IGNORE_RETURN_VALUE(write(STDERR_FILENO, message, sizeof(message)-1));

	// Restore the previous handler so that if we end up getting
	// this signal again we peform the system default action.
	signal(SIGTERM, gPreviousHandlerForSIGTERM);
	gPreviousHandlerForSIGTERM = NULL;
}

static void
signal_SIGHUP(int sig)
{
	static const char message[] = "\nCaught SIGHUP!\n";

	gRet = ERRORCODE_SIGHUP;

	// Can't use syslog() because it isn't async signal safe.
	// So we write to stderr
	IGNORE_RETURN_VALUE(write(STDERR_FILENO, message, sizeof(message)-1));

	// We don't restore the "previous handler"
	// because we always want to let the main
	// loop decide what to do for hangups.
}

static void
signal_critical(int sig, siginfo_t * info, void * ucontext)
{
	// This is the last hurah for this process.
	// We dump the stack, because that's all we can do.

	// We call some functions here which aren't async-signal-safe,
	// but this function isn't really useful without those calls.
	// Since we are making a gamble (and we deadlock if we loose),
	// we are going to set up a two-second watchdog to make sure
	// we end up terminating like we should. The choice of a two
	// second timeout is entirely arbitrary, and may be changed
	// if needs warrant.
	alarm(2);
	signal(SIGALRM, SIG_DFL);

#if WPANTUND_BACKTRACE
	void *stack_mem[FAULT_BACKTRACE_STACK_DEPTH];
	void **stack = stack_mem;
	char **stack_symbols;
	int stack_depth, i;
	ucontext_t *uc = (ucontext_t*)ucontext;

	// Shut up compiler warning.
	(void)uc;

#endif // WPANTUND_BACKTRACE

	fprintf(stderr, " *** FATAL ERROR: Caught signal %d (%s):\n", sig, strsignal(sig));

#if WPANTUND_BACKTRACE
	stack_depth = backtrace(stack, FAULT_BACKTRACE_STACK_DEPTH);

#if __APPLE__
	// OS X adds an extra call onto the stack that
	// we can leave out for clarity sake.
	stack[1] = stack[0];
	stack++;
	stack_depth--;
#endif

	// Here are are trying to update the pointer in the backtrace
	// to be the actual location of the fault.
#if __linux__
#if defined(__x86_64__)
	stack[1] = (void *) uc->uc_mcontext.gregs[REG_RIP];
#elif defined(__arm__)
	stack[1] = (void *) uc->uc_mcontext.arm_ip;
#else
#warning TODO: Add this arch to signal_critical
#endif

#elif __APPLE__ // #if __linux__
#if defined(__x86_64__)
	stack[1] = (void *) uc->uc_mcontext->__ss.__rip;
#endif

#else //#elif __APPLE__
#warning TODO: Add this OS to signal_critical
#endif

	// Now dump the symbols to stderr, in case syslog barfs.
	backtrace_symbols_fd(stack, stack_depth, STDERR_FILENO);

	// Load up the symbols individually, so we can output to syslog, too.
	stack_symbols = backtrace_symbols(stack, stack_depth);
#endif // WPANTUND_BACKTRACE

	syslog(LOG_CRIT, " *** FATAL ERROR: Caught signal %d (%s):", sig, strsignal(sig));

#if WPANTUND_BACKTRACE
	for(i = 0; i != stack_depth; i++) {
#if __APPLE__
		syslog(LOG_CRIT, "[BT] %s", stack_symbols[i]);
#else
		syslog(LOG_CRIT, "[BT] %2d: %s", i, stack_symbols[i]);
#endif
	}

	free(stack_symbols);
#endif // WPANTUND_BACKTRACE

	_exit(EXIT_FAILURE);
}

/* ------------------------------------------------------------------------- */
/* MARK: Misc. */

// TODO: Refactor and clean up.
static int
set_config_param(
    void* ignored, const char* key, const char* value
    )
{
	int ret = -1;

	syslog(LOG_INFO, "set-config-param: \"%s\" = \"%s\"", key, value);

	if (strcaseequal(key, kWPANTUNDProperty_ConfigNCPSocketBaud)) {
		int baud = atoi(value);
		ret = 0;
		require(9600 <= baud, bail);
		gSocketWrapperBaud = baud;
#if HAVE_PWD_H
	} else if (strcaseequal(key, kWPANTUNDProperty_ConfigDaemonPrivDropToUser)) {
		if (value[0] == 0) {
			gPrivDropToUser = NULL;
		} else {
			gPrivDropToUser = strdup(value);
		}
		ret = 0;
#endif
	} else if (strcaseequal(key, kWPANTUNDProperty_ConfigDaemonChroot)) {
		if (value[0] == 0) {
			gChroot = NULL;
		} else {
			gChroot = strdup(value);
		}
		ret = 0;
	} else if (strcaseequal(key, kWPANTUNDProperty_DaemonSyslogMask)) {
		setlogmask(strtologmask(value, setlogmask(0)));
		ret = 0;
	} else if (strcaseequal(key, kWPANTUNDProperty_ConfigDaemonPIDFile)) {
		if (gPIDFilename)
			goto bail;
		gPIDFilename = strdup(value);
		unlink(gPIDFilename);
		FILE* pidfile = fopen(gPIDFilename, "w");
		if (!pidfile) {
			syslog(LOG_ERR, "Unable to open PID file \"%s\".", gPIDFilename);
			goto bail;
		}
		fclose(pidfile);
		ret = 0;
	}

bail:
	if (ret == 0) {
		syslog(LOG_INFO, "set-config-param: \"%s\" set succeded", key);
	}
	return ret;
}

// Used with `read_config()` to collect configuration settings into a map.
static int
add_to_map(
    void* context,
	const char* key,
	const char* value
) {
	std::map<std::string, std::string> *map = static_cast<std::map<std::string, std::string>*>(context);
	(*map)[key] = value;
	return 0;
}

static void
handle_error(int err)
{
	gRet = err;
}

std::string
nl::wpantund::get_wpantund_version_string(void)
{
	std::string version = PACKAGE_VERSION;

	if ((internal_build_source_version[0] == 0) || strequal(SOURCE_VERSION, internal_build_source_version)) {
		// Ignore any coverty warnings about the following line being pointless.
		// I assure you, it is not pointless.
		if (strequal(PACKAGE_VERSION, SOURCE_VERSION)) {
			version += " (";
			version += internal_build_date;
			version += ")";
		} else {
			version += " (";
			version += SOURCE_VERSION;
			version += "; ";
			version += internal_build_date;
			version += ")";
		}
	} else {
		// Ignore any coverty warnings about the following line being pointless.
		// I assure you, it is not pointless.
		if (strequal(SOURCE_VERSION, PACKAGE_VERSION) || strequal(PACKAGE_VERSION, internal_build_source_version)) {
			version += " (";
			version += internal_build_source_version;
			version += "; ";
			version += internal_build_date;
			version += ")";
		} else {
			version += " (";
			version += SOURCE_VERSION;
			version += "/";
			version += internal_build_source_version;
			version += "; ";
			version += internal_build_date;
			version += ")";
		}
	}
	return version;
}

static void
print_version()
{
	printf("wpantund %s\n", get_wpantund_version_string().c_str() );
}

/* ------------------------------------------------------------------------- */
/* MARK: NLPT Hooks */

static fd_set gReadableFDs;
static fd_set gWritableFDs;
static fd_set gErrorableFDs;

bool
nlpt_hook_check_read_fd_source(struct nlpt* nlpt, int fd)
{

	bool ret = false;
	if (fd >= 0) {
		ret = FD_ISSET(fd, &gReadableFDs) || FD_ISSET(fd, &gErrorableFDs);
		FD_CLR(fd, &gReadableFDs);
		FD_CLR(fd, &gErrorableFDs);
	}
	return ret;
}

bool
nlpt_hook_check_write_fd_source(struct nlpt* nlpt, int fd)
{
	bool ret = false;
	if (fd >= 0) {
		ret = FD_ISSET(fd, &gWritableFDs) || FD_ISSET(fd, &gErrorableFDs);
		FD_CLR(fd, &gWritableFDs);
		FD_CLR(fd, &gErrorableFDs);
	}
	return ret;
}

static void
syslog_dump_select_info(int loglevel, fd_set *read_fd_set, fd_set *write_fd_set, fd_set *error_fd_set, int fd_count, cms_t timeout)
{
#define DUMP_FD_SET(l, x) do {\
		int i; \
		std::string buffer; \
		for (i = 0; i < fd_count; i++) { \
			if (!FD_ISSET(i, x)) { \
				continue; \
			} \
			if (buffer.size() != 0) { \
				buffer += ", "; \
			} \
			char str[10]; \
			snprintf(str, sizeof(str), "%d", i); \
			buffer += str; \
		} \
		syslog(l, "SELECT:     %s: %s", #x, buffer.c_str()); \
	} while (0)

	// Check the log level preemptively to avoid wasted CPU.
	if((setlogmask(0)&LOG_MASK(loglevel))) {
		syslog(loglevel, "SELECT: fd_count=%d cms_timeout=%d", fd_count, timeout);

		DUMP_FD_SET(loglevel, read_fd_set);
		DUMP_FD_SET(loglevel, write_fd_set);
		//DUMP_FD_SET(loglevel, error_fd_set); // Commented out to reduce log volume
	}
}

/* ------------------------------------------------------------------------- */
/* MARK: Main Function */

int
main(int argc, char * argv[])
{
	int c;
	int fds_ready = 0;
	bool interface_added = false;
	int zero_cms_in_a_row_count = 0;
	const char* config_file = SYSCONFDIR "/wpantund.conf";
	const char* alt_config_file = SYSCONFDIR "/wpan-tunnel-driver.conf";
	std::list<shared_ptr<nl::wpantund::IPCServer> > ipc_server_list;

	nl::wpantund::NCPInstance *ncp_instance = NULL;
	std::map<std::string, std::string> cmd_line_settings;

	// ========================================================================
	// INITIALIZATION and ARGUMENT PARSING

	gPreviousHandlerForSIGINT = signal(SIGINT, &signal_SIGINT);
	gPreviousHandlerForSIGTERM = signal(SIGTERM, &signal_SIGTERM);
	signal(SIGHUP, &signal_SIGHUP);

	// Always ignore SIGPIPE.
	signal(SIGPIPE, SIG_IGN);

	{
		struct sigaction sigact = { };
		sigact.sa_sigaction = &signal_critical;
		sigact.sa_flags = SA_RESTART | SA_SIGINFO | SA_NOCLDWAIT;

		sigaction(SIGSEGV, &sigact, (struct sigaction *)NULL);
		sigaction(SIGBUS, &sigact, (struct sigaction *)NULL);
		sigaction(SIGILL, &sigact, (struct sigaction *)NULL);
		sigaction(SIGABRT, &sigact, (struct sigaction *)NULL);
	}

	openlog(basename(argv[0]), LOG_PERROR | LOG_PID | LOG_CONS, LOG_DAEMON);

	// Temper the amount of logging.
	setlogmask(setlogmask(0) & LOG_UPTO(DEFAULT_MAX_LOG_LEVEL));

	gRet = ERRORCODE_UNKNOWN;

	if (argc && argv[0][0]) {
		gProcessName = basename(argv[0]);
	}
	optind = 0;
	while(1) {
		static struct option long_options[] =
		{
			{"help",	no_argument,		0,	'h'},
			{"version",	no_argument,		0,	'v'},
			{"debug",	no_argument,		0,	'd'},
			{"config",	required_argument,	0,	'c'},
			{"option",	required_argument,	0,	'o'},
			{"interface",	required_argument,	0,	'I'},
			{"socket",	required_argument,	0,	's'},
			{"baudrate",	required_argument,	0,	'b'},
			{"user",	required_argument,	0,	'u'},
			{0,		0,			0,	0}
		};

		int option_index = 0;
		c = getopt_long(argc, argv, "hvd:c:o:I:s:b:u:", long_options,
			&option_index);

		if (c == -1)
			break;

		switch(c) {
		case 'h':
			print_arg_list_help(option_list, argv[0], "[options]");
			gRet = ERRORCODE_HELP;
			goto bail;

		case 'v':
			print_version();
			gRet = 0;
			goto bail;

		case 'd':
			setlogmask(~0);
			break;

		case 'c':
			config_file = optarg;
			break;

		case 'I':
			cmd_line_settings[kWPANTUNDProperty_ConfigTUNInterfaceName] = optarg;
			break;

		case 's':
			cmd_line_settings[kWPANTUNDProperty_ConfigNCPSocketPath] = optarg;
			break;

		case 'b':
			cmd_line_settings[kWPANTUNDProperty_ConfigNCPSocketBaud] = optarg;
			break;

		case 'u':
			cmd_line_settings[kWPANTUNDProperty_ConfigDaemonPrivDropToUser] = optarg;
			break;

		case 'o':
			if ((optind >= argc) || (strncmp(argv[optind], "-", 1) == 0)) {
				syslog(LOG_ERR, "Missing argument to '-o'.");
				gRet = ERRORCODE_BADARG;
				goto bail;
			}
			char *key = optarg;
			char *value = argv[optind];
			optind++;

			// We handle this option after we try loading the configuration
			// file, so that command-line specified settings can override
			// settings read from the configuration file.
			cmd_line_settings[key] = value;
			break;
		}
	}

	if (optind < argc) {
		fprintf(stderr,
		        "%s: error: Unexpected extra argument: \"%s\"\n",
		        argv[0],
		        argv[optind]);
		gRet = ERRORCODE_BADARG;
		goto bail;
	}

	// ========================================================================
	// STARTUP

	syslog(LOG_NOTICE, "Starting %s " PACKAGE_VERSION " (%s) . . .", gProcessName, internal_build_date);

	if (("" SOURCE_VERSION)[0] != 0) {
		syslog(LOG_NOTICE, "\tSOURCE_VERSION = " SOURCE_VERSION);
	}

	if (internal_build_source_version[0] != 0) {
		syslog(LOG_NOTICE, "\tBUILD_VERSION = %s", internal_build_source_version);
	}

	if (getuid() != 0) {
		// Warn people if we aren't root.
		syslog(LOG_WARNING, "wpantund was not started as 'root'! If wpantund fails immediately, this is probably why.");
	}

	try {
		std::map<std::string, std::string> settings;

		// Read the configuration file into the settings map.
		if (0 == read_config(config_file, &add_to_map, &settings)) {
			syslog(LOG_NOTICE, "Configuration file \"%s\" read.", config_file);
		} else if (0 == read_config(alt_config_file, &add_to_map, &settings)) {
			syslog(LOG_NOTICE, "Configuration file \"%s\" read.", alt_config_file);
		} else {
			syslog(LOG_WARNING, "Configuration file \"%s\" not found, will use defaults.", config_file);
		}

		// Command line settings override configuration file settings.
		// This is weird because the `insert()` method doesn't replace
		// a key value pair if it already exists, so to get the desired
		// behavior, we insert into `cmd_line_settings`, not `settings`.
		cmd_line_settings.insert(settings.begin(), settings.end());

		// Perform depricated property translation
		{
			std::map<std::string, std::string>::const_iterator iter;
			settings.clear();

			for (iter = cmd_line_settings.begin(); iter != cmd_line_settings.end(); iter++) {
				std::string key(iter->first);
				boost::any value(iter->second);
				if (NCPControlInterface::translate_deprecated_property(key, value)) {
					if (key.empty()) {
						syslog(LOG_WARNING, "Configuration property \"%s\" is no longer supported. Please remove it from your configuration.", iter->first.c_str());
					} else {
						syslog(LOG_WARNING, "CONFIGURATION PROPERTY \"%s\" IS DEPRECATED. Please use \"%s\" instead.", iter->first.c_str(), key.c_str());
					}
				}
				if (!key.empty()) {
					settings[key] = any_to_string(value);
				}
			}
		}

		// Handle all of the options/settings.
		if (!settings.empty()) {
			std::map<std::string, std::string>::const_iterator iter;
			std::map<std::string, std::string> settings_for_ncp_control_interface;

			for(iter = settings.begin(); iter != settings.end(); iter++) {
				if (set_config_param(NULL, iter->first.c_str(), iter->second.c_str())) {
					// If set_config_param() doesn't like the argument,
					// we hold onto it for now so we can try passing it as
					// a property to the NCP instance.
					settings_for_ncp_control_interface[iter->first] = iter->second;
				}
			}
			settings = settings_for_ncp_control_interface;
		}

		// Set up DBUSIPCServer
		try {
			ipc_server_list.push_back(shared_ptr<nl::wpantund::IPCServer>(new DBUSIPCServer()));
		} catch(std::exception x) {
			syslog(LOG_ERR, "Unable to start DBUSIPCServer \"%s\"",x.what());
		}

		/*** Add other IPCServers here! ***/

		// Always fail if we have no IPCServers.
		if (ipc_server_list.empty()) {
			syslog(LOG_ERR, "No viable IPC server.");
			goto bail;
		}

		ncp_instance = NCPInstance::alloc(settings);

		require_string(ncp_instance != NULL, bail, "Unable to create NCPInstance");

		ncp_instance->mOnFatalError.connect(&handle_error);

		ncp_instance->get_stat_collector().set_ncp_control_interface(&ncp_instance->get_control_interface());

	} catch(std::runtime_error x) {
		syslog(LOG_ERR, "Runtime error thrown while starting up, \"%s\"",x.what());
		ncp_instance = NULL;
		goto bail;

	} catch(std::exception x) {
		syslog(LOG_ERR, "Exception thrown while starting up, \"%s\"",x.what());
		ncp_instance = NULL;
		goto bail;
	}

	if (sec_random_init() < 0) {
		syslog(LOG_ERR, "Call to sec_random_init() failed, errno=%d \"%s\"", errno, strerror(errno));
		goto bail;
	}

	// ========================================================================
	// Dropping Privileges

	if (gChroot != NULL) {
		if (getuid() == 0) {
			if (chdir(gChroot) != 0) {
				syslog(LOG_CRIT, "chdir: %s", strerror(errno));
				gRet = ERRORCODE_ERRNO;
				goto bail;
			}

			if (chroot(gChroot) != 0) {
				syslog(LOG_CRIT, "chroot: %s", strerror(errno));
				gRet = ERRORCODE_ERRNO;
				goto bail;
			} else {
				if (chdir("/") != 0) {
					syslog(LOG_INFO, "Failed to `chdir` after `chroot` to \"%s\"", gChroot);
					gRet = ERRORCODE_ERRNO;
					goto bail;
				} else {
					syslog(LOG_INFO, "Successfully changed root directory to \"%s\".", gChroot);
				}
			}
		} else {
			syslog(LOG_WARNING, "Not running as root, cannot chroot");
		}
	}

#if HAVE_PWD_H
	if (getuid() == 0) {
		uid_t target_uid = 0;
		gid_t target_gid = 0;

		if (gPrivDropToUser != NULL) {
			struct passwd *passwd = getpwnam(gPrivDropToUser);

			if (passwd == NULL) {
				syslog(LOG_CRIT, "getpwnam: Unable to lookup user \"%s\", cannot drop privileges.", gPrivDropToUser);
				gRet = ERRORCODE_ERRNO;
				goto bail;
			}

			target_uid = passwd->pw_uid;
			target_gid = passwd->pw_gid;
		}

		if (target_gid != 0) {
			if (setgid(target_gid) != 0) {
				syslog(LOG_CRIT, "setgid: Unable to drop group privileges: %s", strerror(errno));
				gRet = ERRORCODE_ERRNO;
				goto bail;
			} else {
				syslog(LOG_INFO, "Group privileges dropped to GID:%d", (int)target_gid);
			}
		}

		if (target_uid != 0) {
			if (setuid(target_uid) != 0) {
				syslog(LOG_CRIT, "setuid: Unable to drop user privileges: %s", strerror(errno));
				gRet = ERRORCODE_ERRNO;
				goto bail;
			} else {
				syslog(LOG_INFO, "User privileges dropped to UID:%d", (int)target_uid);
			}
		}

		if ((target_gid == 0) || (target_uid == 0)) {
			syslog(LOG_NOTICE, "Running as root without dropping privileges!");
		}
	} else if (gPrivDropToUser != NULL) {
		syslog(LOG_NOTICE, "Not running as root, skipping dropping privileges");
	}
#endif // #if HAVE_PWD_H

	// ========================================================================
	// MAIN LOOP

	gRet = 0;

	while (!gRet) {
		const cms_t max_main_loop_timeout(CMS_DISTANT_FUTURE);
		cms_t cms_timeout(max_main_loop_timeout);
		int max_fd(-1);
		struct timeval timeout;
		std::list<shared_ptr<nl::wpantund::IPCServer> >::iterator ipc_iter;

		FD_ZERO(&gReadableFDs);
		FD_ZERO(&gWritableFDs);
		FD_ZERO(&gErrorableFDs);

		// Update the FD masks and timeouts
		ncp_instance->update_fd_set(&gReadableFDs, &gWritableFDs, &gErrorableFDs, &max_fd, &cms_timeout);
		Timer::update_timeout(&cms_timeout);

		for (ipc_iter = ipc_server_list.begin(); ipc_iter != ipc_server_list.end(); ++ipc_iter) {
			(*ipc_iter)->update_fd_set(&gReadableFDs, &gWritableFDs, &gErrorableFDs, &max_fd, &cms_timeout);
		}

		require_string(max_fd < FD_SETSIZE, bail, "Too many file descriptors");

		// Negative CMS timeout values are not valid.
		if (cms_timeout < 0) {
			syslog(LOG_DEBUG, "Negative CMS value: %d", cms_timeout);
			cms_timeout = 0;
		}

		// Identify conditions where we are burning too much CPU.
		if (cms_timeout == 0) {
			double loadavg[3] = {-1.0, -1.0, -1.0};

#if HAVE_GETLOADAVG
			getloadavg(loadavg, 3);
#endif

			switch (++zero_cms_in_a_row_count) {
			case 20:
				syslog(LOG_INFO, "BUG: Main loop is thrashing! (%f %f %f)", loadavg[0], loadavg[1], loadavg[2]);
				break;

			case 200:
				syslog(LOG_WARNING, "BUG: Main loop is still thrashing! Slowing things down. (%f %f %f)", loadavg[0], loadavg[1], loadavg[2]);
				break;

			case 1000:
				syslog(LOG_CRIT, "BUG: Main loop had over 1000 iterations in a row with a zero timeout! Terminating. (%f %f %f)", loadavg[0], loadavg[1], loadavg[2]);
				gRet = ERRORCODE_UNKNOWN;
				break;
			}
			if (zero_cms_in_a_row_count > 200) {
				// If the past 200 iterations have had a zero timeout,
				// start using a minimum timeout of 10ms, so that we
				// don't bring the rest of the system to a grinding halt.
				cms_timeout = 10;
			}
		} else {
			zero_cms_in_a_row_count = 0;
		}

		// Convert our `cms` value into timeval compatible with select().
		timeout.tv_sec = cms_timeout / MSEC_PER_SEC;
		timeout.tv_usec = (cms_timeout % MSEC_PER_SEC) * USEC_PER_MSEC;

#if DEBUG
		syslog_dump_select_info(
			LOG_DEBUG,
			&gReadableFDs,
			&gWritableFDs,
			&gErrorableFDs,
			max_fd + 1,
			cms_timeout
		);
#endif

		// Block until we timeout or there is FD activity.
		fds_ready = select(
			max_fd + 1,
			&gReadableFDs,
			&gWritableFDs,
			&gErrorableFDs,
			&timeout
		);

		if (fds_ready < 0) {
			syslog(LOG_ERR, "select() errno=\"%s\" (%d)", strerror(errno),
			       errno);

			if (errno == EINTR) {
				// EINTR isn't necessarily bad. If it was something bad,
				// we would either already be terminated or gRet will be
				// set and we will break out of the main loop in a moment.
				continue;
			}
			gRet = ERRORCODE_ERRNO;
			break;
		}

		// Process callback timers.
		Timer::process();

		// Process any necessary IPC actions.
		for (ipc_iter = ipc_server_list.begin(); ipc_iter != ipc_server_list.end(); ++ipc_iter) {
			(*ipc_iter)->process();
		}

		// Process the NCP instance.
		ncp_instance->process();

		// We only expose the interface via IPC after it is
		// successfully initialized for the first time.
		if (!interface_added) {
			const boost::any value = ncp_instance->get_control_interface().get_property(kWPANTUNDProperty_NCPState);
			if ((value.type() == boost::any(std::string()).type())
			 && (boost::any_cast<std::string>(value) != kWPANTUNDStateUninitialized)
			) {
				for (ipc_iter = ipc_server_list.begin(); ipc_iter != ipc_server_list.end(); ++ipc_iter) {
					(*ipc_iter)->add_interface(&ncp_instance->get_control_interface());
				}
				interface_added = true;
			}
		}
	} // while (!gRet)

bail:
	syslog(LOG_NOTICE, "Cleaning up. (gRet = %d)", gRet);

	if (gRet == ERRORCODE_QUIT) {
		gRet = 0;
	}

	if (gPIDFilename) {
		unlink(gPIDFilename);
	}

	syslog(LOG_NOTICE, "Stopped.");
	return gRet;
}
