/*
 *    Copyright (c) 2017, The OpenThread Authors.
 *    All rights reserved.
 *
 *    Redistribution and use in source and binary forms, with or without
 *    modification, are permitted provided that the following conditions are met:
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *    3. Neither the name of the copyright holder nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 *    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *    ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *    LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *    CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *    ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *    POSSIBILITY OF SUCH DAMAGE.
 */

#include <openthread-br/config.h>

#include <mutex>
#include <thread>

#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <openthread/logging.h>

#include "agent/agent_instance.hpp"
#include "agent/ncp.hpp"
#include "common/code_utils.hpp"
#include "common/logging.hpp"
#include "common/types.hpp"

#include "agent/ncp_openthread.hpp"
#if OTBR_ENABLE_DBUS_SERVER
#include "dbus/server/dbus_agent.hpp"
using otbr::DBus::DBusAgent;
#endif
using otbr::Ncp::ControllerOpenThread;

#if OTBR_ENABLE_OPENWRT
extern void       UbusUpdateFdSet(fd_set &aReadFdSet, int &aMaxFd);
extern void       UbusProcess(const fd_set &aReadFdSet);
extern void       UbusServerRun(void);
extern void       UbusServerInit(otbr::Ncp::ControllerOpenThread *aController, std::mutex *aNcpThreadMutex);
static std::mutex sThreadMutex;
#endif

static const char kSyslogIdent[]          = "otbr-agent";
static const char kDefaultInterfaceName[] = "wpan0";

// Default poll timeout.
static const struct timeval kPollTimeout = {10, 0};
static const struct option  kOptions[]   = {{"debug-level", required_argument, NULL, 'd'},
                                         {"help", no_argument, NULL, 'h'},
                                         {"thread-ifname", required_argument, NULL, 'I'},
                                         {"verbose", no_argument, NULL, 'v'},
                                         {"version", no_argument, NULL, 'V'},
                                         {0, 0, 0, 0}};

static void HandleSignal(int aSignal)
{
    signal(aSignal, SIG_DFL);
}

static int Mainloop(otbr::AgentInstance &aInstance, const char *aInterfaceName)
{
    int error = EXIT_FAILURE;
#if OTBR_ENABLE_DBUS_SERVER
    ControllerOpenThread *     ncpOpenThread = reinterpret_cast<ControllerOpenThread *>(&aInstance.GetNcp());
    std::unique_ptr<DBusAgent> dbusAgent     = std::unique_ptr<DBusAgent>(new DBusAgent(aInterfaceName, ncpOpenThread));

    dbusAgent->Init();
#else
    (void)aInterfaceName;
#endif
    otbrLog(OTBR_LOG_INFO, "Border router agent started.");

    // allow quitting elegantly
    signal(SIGTERM, HandleSignal);

    while (true)
    {
        otSysMainloopContext mainloop;
        int                  rval;

        mainloop.mMaxFd   = -1;
        mainloop.mTimeout = kPollTimeout;

        FD_ZERO(&mainloop.mReadFdSet);
        FD_ZERO(&mainloop.mWriteFdSet);
        FD_ZERO(&mainloop.mErrorFdSet);

        aInstance.UpdateFdSet(mainloop);

#if OTBR_ENABLE_DBUS_SERVER
        dbusAgent->UpdateFdSet(mainloop.mReadFdSet, mainloop.mWriteFdSet, mainloop.mErrorFdSet, mainloop.mMaxFd,
                               mainloop.mTimeout);
#endif

#if OTBR_ENABLE_OPENWRT
        UbusUpdateFdSet(mainloop.mReadFdSet, mainloop.mMaxFd);
        sThreadMutex.unlock();
#endif

        rval = select(mainloop.mMaxFd + 1, &mainloop.mReadFdSet, &mainloop.mWriteFdSet, &mainloop.mErrorFdSet,
                      &mainloop.mTimeout);

#if OTBR_ENABLE_DBUS_SERVER
        if (ncpOpenThread->IsResetRequested())
        {
            ncpOpenThread->Reset();
            continue;
        }
#endif

        if (rval >= 0)
        {
#if OTBR_ENABLE_OPENWRT
            sThreadMutex.lock();
            UbusProcess(mainloop.mReadFdSet);
#endif
            aInstance.Process(mainloop);

#if OTBR_ENABLE_DBUS_SERVER
            dbusAgent->Process(mainloop.mReadFdSet, mainloop.mWriteFdSet, mainloop.mErrorFdSet);
#endif
        }
        else
        {
#if OTBR_ENABLE_OPENWRT
            sThreadMutex.lock();
#endif
            error = OTBR_ERROR_ERRNO;
            otbrLog(OTBR_LOG_ERR, "select() failed", strerror(errno));
            break;
        }
    }

    return error;
}

static void PrintHelp(const char *aProgramName)
{
    fprintf(stderr, "Usage: %s [-I interfaceName] [-d DEBUG_LEVEL] [-v] [RADIO_DEVICE] [RADIO_CONFIG]\n", aProgramName);
}

static void PrintVersion(void)
{
    printf("%s\n", PACKAGE_VERSION);
}

static void OnAllocateFailed(void)
{
    otbrLog(OTBR_LOG_CRIT, "Allocate failure, exiting...");
    exit(1);
}

int main(int argc, char *argv[])
{
    int                    logLevel = OTBR_LOG_INFO;
    int                    opt;
    int                    ret           = EXIT_SUCCESS;
    const char *           interfaceName = kDefaultInterfaceName;
    otbr::Ncp::Controller *ncp           = NULL;
    bool                   verbose       = false;

    std::set_new_handler(OnAllocateFailed);

    while ((opt = getopt_long(argc, argv, "d:hI:Vv", kOptions, NULL)) != -1)
    {
        switch (opt)
        {
        case 'd':
            logLevel = atoi(optarg);
            VerifyOrExit(logLevel >= OTBR_LOG_EMERG && logLevel <= OTBR_LOG_DEBUG, ret = EXIT_FAILURE);
            break;

        case 'I':
            interfaceName = optarg;
            break;

        case 'v':
            verbose = true;
            break;

        case 'V':
            PrintVersion();
            ExitNow();
            break;

        case 'h':
            PrintHelp(argv[0]);
            ExitNow(ret = EXIT_SUCCESS);
            break;
        default:
            PrintHelp(argv[0]);
            ExitNow(ret = EXIT_FAILURE);
            break;
        }
    }

    VerifyOrExit(optind + 1 < argc, ret = EXIT_FAILURE);
    ncp = otbr::Ncp::Controller::Create(interfaceName, argv[optind], argv[optind + 1]);
    VerifyOrExit(ncp != NULL, ret = EXIT_FAILURE);

    otbrLogInit(kSyslogIdent, logLevel, verbose);

    otbrLog(OTBR_LOG_INFO, "Thread interface %s", interfaceName);

    {
        otbr::AgentInstance instance(ncp);
        otLogLevel          level;

        SuccessOrExit(ret = instance.Init());

        switch (logLevel)
        {
        case OTBR_LOG_EMERG:
        case OTBR_LOG_ALERT:
        case OTBR_LOG_CRIT:
            level = OT_LOG_LEVEL_CRIT;
            break;
        case OTBR_LOG_ERR:
        case OTBR_LOG_WARNING:
            level = OT_LOG_LEVEL_WARN;
            break;
        case OTBR_LOG_NOTICE:
            level = OT_LOG_LEVEL_NOTE;
            break;
        case OTBR_LOG_INFO:
            level = OT_LOG_LEVEL_INFO;
            break;
        case OTBR_LOG_DEBUG:
            level = OT_LOG_LEVEL_DEBG;
            break;
        default:
            ExitNow(ret = EXIT_FAILURE);
            break;
        }

        VerifyOrExit(otLoggingSetLevel(level) == OT_ERROR_NONE, ret = EXIT_FAILURE);

#if OTBR_ENABLE_OPENWRT
        ControllerOpenThread *ncpThread = reinterpret_cast<ControllerOpenThread *>(ncp);

        UbusServerInit(ncpThread, &sThreadMutex);
        std::thread(UbusServerRun).detach();
#endif
        SuccessOrExit(ret = Mainloop(instance, interfaceName));
    }

    otbrLogDeinit();

exit:
    return ret;
}
