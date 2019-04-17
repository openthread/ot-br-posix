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

#if HAVE_CONFIG_H
#include "otbr-config.h"
#endif

#include <errno.h>
#include <getopt.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "agent_instance.hpp"
#include "ncp.hpp"
#include "common/code_utils.hpp"
#include "common/logging.hpp"
#include "common/types.hpp"

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

jmp_buf gResetJump;

using namespace ot::BorderRouter;

static void HandleSignal(int aSignal)
{
    signal(aSignal, SIG_DFL);
}

static int Mainloop(AgentInstance &aInstance)
{
    int rval = EXIT_FAILURE;

    otbrLog(OTBR_LOG_INFO, "Border router agent started.");

    // allow quitting elegantly
    signal(SIGTERM, HandleSignal);

    while (true)
    {
        otSysMainloopContext mainloop;

        mainloop.mMaxFd   = -1;
        mainloop.mTimeout = kPollTimeout;

        FD_ZERO(&mainloop.mReadFdSet);
        FD_ZERO(&mainloop.mWriteFdSet);
        FD_ZERO(&mainloop.mErrorFdSet);

        aInstance.UpdateFdSet(mainloop);

        if (select(mainloop.mMaxFd + 1, &mainloop.mReadFdSet, &mainloop.mWriteFdSet, &mainloop.mErrorFdSet,
                   &mainloop.mTimeout) >= 0)
        {
            aInstance.Process(mainloop);
        }
        else
        {
            rval = OTBR_ERROR_ERRNO;
            otbrLog(OTBR_LOG_ERR, "select() failed", strerror(errno));
            break;
        }
    }

    return rval;
}

static void PrintHelp(const char *aProgramName)
{
#if OTBR_ENABLE_NCP_WPANTUND
    fprintf(stderr, "Usage: %s [-I interfaceName] [-d DEBUG_LEVEL] [-v]\n", aProgramName);
#else
    fprintf(stderr, "Usage: %s [-I interfaceName] [-d DEBUG_LEVEL] [-v] [RADIO_DEVICE] [RADIO_CONFIG]\n", aProgramName);
#endif
}

static void PrintVersion(void)
{
    printf("%s\n", PACKAGE_VERSION);
}

int main(int argc, char *argv[])
{
    if (setjmp(gResetJump))
    {
        alarm(0);
        execvp(argv[0], argv);
    }

    int              logLevel = OTBR_LOG_INFO;
    int              opt;
    int              ret           = EXIT_SUCCESS;
    const char *     interfaceName = kDefaultInterfaceName;
    Ncp::Controller *ncp           = NULL;
    bool             verbose       = false;

    while ((opt = getopt_long(argc, argv, "d:hI:Vv", kOptions, NULL)) != -1)
    {
        switch (opt)
        {
        case 'd':
            logLevel = atoi(optarg);
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

#if OTBR_ENABLE_NCP_WPANTUND
    ncp = Ncp::Controller::Create(interfaceName);
#else
    VerifyOrExit(optind + 1 < argc, ret = EXIT_FAILURE);
    ncp = Ncp::Controller::Create(interfaceName, argv[optind], argv[optind + 1]);
#endif
    VerifyOrExit(ncp != NULL, ret = EXIT_FAILURE);

    otbrLogInit(kSyslogIdent, logLevel, verbose);

    otbrLog(OTBR_LOG_INFO, "Thread interface %s", interfaceName);

    {
        AgentInstance instance(ncp);

        SuccessOrExit(ret = instance.Init());
        SuccessOrExit(ret = Mainloop(instance));
    }

    otbrLogDeinit();

exit:
    return ret;
}
