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

#include <fstream>
#include <mutex>
#include <sstream>

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <assert.h>
#include <openthread/logging.h>
#include <openthread/platform/radio.h>

#if __ANDROID__ && OTBR_CONFIG_ANDROID_PROPERTY_ENABLE
#include <cutils/properties.h>
#endif

#include "agent/application.hpp"
#include "agent/instance_params.hpp"
#include "common/code_utils.hpp"
#include "common/logging.hpp"
#include "common/mainloop.hpp"
#include "common/types.hpp"
#include "ncp/ncp_openthread.hpp"

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
};

static jmp_buf sResetJump;

void __gcov_flush();

// Default poll timeout.
static const struct timeval kPollTimeout             = {10, 0};
static constexpr const char kArgNoAutoResumeThread[] = "no-auto-resume-thread";
static const struct option  kOptions[]               = {
    {kArgNoAutoResumeThread, no_argument, nullptr, 0},
    {"backbone-ifname", required_argument, nullptr, OTBR_OPT_BACKBONE_INTERFACE_NAME},
    {"debug-level", required_argument, nullptr, OTBR_OPT_DEBUG_LEVEL},
    {"help", no_argument, nullptr, OTBR_OPT_HELP},
    {"thread-ifname", required_argument, nullptr, OTBR_OPT_INTERFACE_NAME},
    {"verbose", no_argument, nullptr, OTBR_OPT_VERBOSE},
    {"version", no_argument, nullptr, OTBR_OPT_VERSION},
    {"radio-version", no_argument, nullptr, OTBR_OPT_RADIO_VERSION},
    {0, 0, 0, 0}};

static int Mainloop(otbr::Ncp::ControllerOpenThread &aOpenThread)
{
    otbr::Application app(aOpenThread);

    app.Init();

    return app.Run();
}

static void PrintHelp(const char *aProgramName)
{
    fprintf(stderr,
            "Usage: %s [-I interfaceName] [-B backboneIfName] [-d DEBUG_LEVEL] [-v] RADIO_URL [RADIO_URL]\n"
            "More options:\n"
            "    --no-auto-resume-thread        Do not automatically resume Thread stack.\n",
            aProgramName);
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
    otbrLogCrit("Allocate failure, exiting...");
    exit(1);
}

static otbrLogLevel GetDefaultLogLevel(void)
{
    otbrLogLevel level = OTBR_LOG_INFO;

#if __ANDROID__ && OTBR_CONFIG_ANDROID_PROPERTY_ENABLE
    char value[PROPERTY_VALUE_MAX];

    property_get("ro.build.type", value, "user");
    if (!strcmp(value, "user"))
    {
        level = OTBR_LOG_WARNING;
    }
#endif

    return level;
}

static int realmain(int argc, char *argv[])
{
    otbrLogLevel              logLevel = GetDefaultLogLevel();
    int                       opt;
    int                       ret                   = EXIT_SUCCESS;
    const char *              interfaceName         = kDefaultInterfaceName;
    const char *              backboneInterfaceName = "";
    bool                      verbose               = false;
    bool                      printRadioVersion     = false;
    bool                      autoResumeThread      = true;
    std::vector<const char *> radioUrls;
    int                       longind;

    std::set_new_handler(OnAllocateFailed);

    while ((opt = getopt_long(argc, argv, "B:d:hI:Vv", kOptions, &longind)) != -1)
    {
        switch (opt)
        {
        case OTBR_OPT_BACKBONE_INTERFACE_NAME:
            backboneInterfaceName = optarg;
            break;

        case OTBR_OPT_DEBUG_LEVEL:
            logLevel = static_cast<otbrLogLevel>(atoi(optarg));
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

        case 0:
            if (!strcmp(kOptions[longind].name, kArgNoAutoResumeThread))
            {
                autoResumeThread = false;
            }
            break;

        default:
            PrintHelp(argv[0]);
            ExitNow(ret = EXIT_FAILURE);
            break;
        }
    }

    otbrLogInit(kSyslogIdent, logLevel, verbose);
    otbrLogInfo("Running %s", OTBR_PACKAGE_VERSION);
    otbrLogInfo("Thread version: %s", otbr::Ncp::ControllerOpenThread::GetThreadVersion());
    otbrLogInfo("Thread interface: %s", interfaceName);
    otbrLogInfo("Backbone interface: %s", backboneInterfaceName);
    otbrLogInfo("Thread stack %s be automatically resumed.", autoResumeThread ? "will" : "won't");

    for (int i = optind; i < argc; i++)
    {
        otbrLogInfo("Radio URL: %s", argv[i]);
        radioUrls.push_back(argv[i]);
    }

    {
        otbr::Ncp::ControllerOpenThread ncpOpenThread{interfaceName, radioUrls, backboneInterfaceName,
                                                      /* aDryRun */ printRadioVersion,
                                                      /* aAutoResume */ autoResumeThread};

        otbr::InstanceParams::Get().SetThreadIfName(interfaceName);
        otbr::InstanceParams::Get().SetBackboneIfName(backboneInterfaceName);

        SuccessOrExit(ret = ncpOpenThread.Init());

        if (printRadioVersion)
        {
            PrintRadioVersion(ncpOpenThread.GetInstance());
            ExitNow(ret = EXIT_SUCCESS);
        }

        SuccessOrExit(ret = Mainloop(ncpOpenThread));
    }

    otbrLogDeinit();

exit:
    return ret;
}

void otPlatReset(otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);

    gPlatResetReason = OT_PLAT_RESET_REASON_SOFTWARE;

    otSysDeinit();

    longjmp(sResetJump, 1);
    assert(false);
}

static std::vector<char *> AppendNoAutoResumeThreadArg(char *aArguments[])
{
    std::vector<char *> newArguments;
    bool                appended = false;
    int                 i        = 0;

    OTBR_UNUSED_VARIABLE(aArguments);

    while (true)
    {
        char *arg = aArguments[i++];

        newArguments.push_back(arg);

        if (arg != nullptr && !strncmp(arg, "--", 2) && !strcmp(arg + 2, kArgNoAutoResumeThread))
        {
            appended = true;
        }

        if (arg == nullptr)
        {
            break;
        }
    }

    if (!appended)
    {
        static char sNoAutoResumeThreadArg[sizeof(kArgNoAutoResumeThread) + 2];

        sNoAutoResumeThreadArg[0] = '-';
        sNoAutoResumeThreadArg[1] = '-';
        strcpy(&sNoAutoResumeThreadArg[2], kArgNoAutoResumeThread);

        newArguments.insert(newArguments.begin() + 1, sNoAutoResumeThreadArg);
    }

    return newArguments;
}

int main(int argc, char *argv[])
{
    if (setjmp(sResetJump))
    {
        std::vector<char *> newArgs;

        alarm(0);
#if OPENTHREAD_ENABLE_COVERAGE
        __gcov_flush();
#endif
        newArgs = AppendNoAutoResumeThreadArg(argv);
        execvp(argv[0], newArgs.data());
    }

    return realmain(argc, argv);
}
