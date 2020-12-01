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
#include <openthread/platform/radio.h>

#include "agent/agent_instance.hpp"
#include "agent/ncp.hpp"
#include "agent/ncp_openthread.hpp"
#include "common/code_utils.hpp"
#include "common/logging.hpp"
#include "common/region_code.hpp"
#include "common/types.hpp"
#if OTBR_ENABLE_REST_SERVER
#include "rest/rest_web_server.hpp"
using otbr::rest::RestWebServer;
#endif
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

enum
{
    OTBR_OPT_BACKBONE_INTERFACE_NAME = 'B',
    OTBR_OPT_DEBUG_LEVEL             = 'd',
    OTBR_OPT_HELP                    = 'h',
    OTBR_OPT_INTERFACE_NAME          = 'I',
    OTBR_OPT_VERBOSE                 = 'v',
    OTBR_OPT_VERSION                 = 'V',
    OTBR_OPT_SHORTMAX                = 128,
    OTBR_OPT_RADIO_VERSION,
    OTBR_OPT_REGION,
};

// Default poll timeout.
static const struct timeval kPollTimeout = {10, 0};
static const struct option  kOptions[]   = {
    {"backbone-ifname", required_argument, nullptr, OTBR_OPT_BACKBONE_INTERFACE_NAME},
    {"debug-level", required_argument, nullptr, OTBR_OPT_DEBUG_LEVEL},
    {"help", no_argument, nullptr, OTBR_OPT_HELP},
    {"thread-ifname", required_argument, nullptr, OTBR_OPT_INTERFACE_NAME},
    {"verbose", no_argument, nullptr, OTBR_OPT_VERBOSE},
    {"version", no_argument, nullptr, OTBR_OPT_VERSION},
    {"radio-version", no_argument, nullptr, OTBR_OPT_RADIO_VERSION},
    {"reg", required_argument, nullptr, OTBR_OPT_REGION},
    {0, 0, 0, 0}};

static void HandleSignal(int aSignal)
{
    signal(aSignal, SIG_DFL);
}

static int Mainloop(otbr::AgentInstance &aInstance, const char *aInterfaceName)
{
    int                   error         = EXIT_FAILURE;
    ControllerOpenThread &ncpOpenThread = static_cast<ControllerOpenThread &>(aInstance.GetNcp());

#if OTBR_ENABLE_DBUS_SERVER
    std::unique_ptr<DBusAgent> dbusAgent = std::unique_ptr<DBusAgent>(new DBusAgent(aInterfaceName, &ncpOpenThread));
    dbusAgent->Init();
#else
    (void)aInterfaceName;
#endif
#if OTBR_ENABLE_REST_SERVER
    RestWebServer *restServer = RestWebServer::GetRestWebServer(&ncpOpenThread);
    restServer->Init();
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

#if OTBR_ENABLE_REST_SERVER
        restServer->UpdateFdSet(mainloop);
#endif

#if OTBR_ENABLE_OPENWRT
        UbusUpdateFdSet(mainloop.mReadFdSet, mainloop.mMaxFd);
        sThreadMutex.unlock();
#endif

        rval = select(mainloop.mMaxFd + 1, &mainloop.mReadFdSet, &mainloop.mWriteFdSet, &mainloop.mErrorFdSet,
                      &mainloop.mTimeout);

        if (ncpOpenThread.IsResetRequested())
        {
            ncpOpenThread.Reset();
            continue;
        }

        if (rval >= 0)
        {
#if OTBR_ENABLE_OPENWRT
            sThreadMutex.lock();
            UbusProcess(mainloop.mReadFdSet);
#endif

#if OTBR_ENABLE_REST_SERVER
            restServer->Process(mainloop);
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
    fprintf(stderr, "Usage: %s [--reg region] [-I interfaceName] [-d DEBUG_LEVEL] [-v] RADIO_URL\n", aProgramName);
    fprintf(stderr, "%s", otSysGetRadioUrlHelpString());
}

static void PrintVersion(void)
{
    printf("%s\n", OTBR_PACKAGE_VERSION);
}

static void PrintRadioVersion(otInstance *aInstance)
{
    printf("%s\n", otPlatRadioGetVersionString(aInstance));
}

static void OnAllocateFailed(void)
{
    otbrLog(OTBR_LOG_CRIT, "Allocate failure, exiting...");
    exit(1);
}

int main(int argc, char *argv[])
{
    int                              logLevel = OTBR_LOG_INFO;
    int                              opt;
    int                              ret                   = EXIT_SUCCESS;
    const char *                     interfaceName         = kDefaultInterfaceName;
    const char *                     backboneInterfaceName = "";
    otbr::Ncp::Controller *          ncp                   = nullptr;
    otbr::Ncp::ControllerOpenThread *ncpOpenThread         = nullptr;
    bool                             verbose               = false;
    bool                             printRadioVersion     = false;
    std::string                      regionCode;

    std::set_new_handler(OnAllocateFailed);

    while ((opt = getopt_long(argc, argv, "B:d:hI:Vv", kOptions, nullptr)) != -1)
    {
        switch (opt)
        {
        case OTBR_OPT_BACKBONE_INTERFACE_NAME:
            backboneInterfaceName = optarg;
            break;

        case OTBR_OPT_DEBUG_LEVEL:
            logLevel = atoi(optarg);
            VerifyOrExit(logLevel >= OTBR_LOG_EMERG && logLevel <= OTBR_LOG_DEBUG, ret = EXIT_FAILURE);
            break;

        case OTBR_OPT_INTERFACE_NAME:
            interfaceName = optarg;
            break;

        case OTBR_OPT_VERBOSE:
            verbose = true;
            break;

        case OTBR_OPT_VERSION:
            PrintVersion();
            ExitNow();
            break;

        case OTBR_OPT_HELP:
            PrintHelp(argv[0]);
            ExitNow(ret = EXIT_SUCCESS);
            break;

        case OTBR_OPT_RADIO_VERSION:
            printRadioVersion = true;
            break;

        case OTBR_OPT_REGION:
            regionCode = optarg;
            break;

        default:
            PrintHelp(argv[0]);
            ExitNow(ret = EXIT_FAILURE);
            break;
        }
    }

    otbrLogInit(kSyslogIdent, logLevel, verbose);
    otbrLog(OTBR_LOG_INFO, "Running %s", OTBR_PACKAGE_VERSION);
    VerifyOrExit(optind < argc, ret = EXIT_FAILURE);

    ncp           = otbr::Ncp::Controller::Create(interfaceName, argv[optind], backboneInterfaceName);
    ncpOpenThread = static_cast<ControllerOpenThread *>(ncp);
    VerifyOrExit(ncp != nullptr, ret = EXIT_FAILURE);

    otbrLog(OTBR_LOG_INFO, "Thread interface %s", interfaceName);
    otbrLog(OTBR_LOG_INFO, "Backbone interface %s", backboneInterfaceName);

    if (!regionCode.empty())
    {
        otbrLog(OTBR_LOG_INFO, "Region %s", regionCode.c_str());
    }
    ncpOpenThread->SetRegionCode(regionCode);

    {
        otbr::AgentInstance instance(ncp);

        otbr::InstanceParams::Get().SetThreadIfName(interfaceName);
        otbr::InstanceParams::Get().SetBackboneIfName(backboneInterfaceName);

        SuccessOrExit(ret = instance.Init());

        if (printRadioVersion)
        {
            PrintRadioVersion(ncpOpenThread->GetInstance());
            ExitNow(ret = EXIT_SUCCESS);
        }

#if OTBR_ENABLE_OPENWRT
        UbusServerInit(ncpOpenThread, &sThreadMutex);
        std::thread(UbusServerRun).detach();
#endif
        SuccessOrExit(ret = Mainloop(instance, interfaceName));
    }

    otbrLogDeinit();

exit:
    return ret;
}
