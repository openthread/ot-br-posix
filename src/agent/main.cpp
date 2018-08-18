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
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "agent_instance.hpp"
#include "common/code_utils.hpp"
#include "common/logging.hpp"
#include "common/types.hpp"

static const char kSyslogIdent[]          = "otbr-agent";
static const char kDefaultInterfaceName[] = "wpan0";

// Default poll timeout.
static const struct timeval kPollTimeout = {10, 0};

static void HandleSignal(int aSignal)
{
    signal(aSignal, SIG_DFL);
}

int Mainloop(const char *aInterfaceName)
{
    int rval = EXIT_FAILURE;

    ot::BorderRouter::AgentInstance instance(aInterfaceName);
    SuccessOrExit(instance.Init());

    otbrLog(OTBR_LOG_INFO, "Border router agent started.");

    // allow quitting elegantly
    signal(SIGTERM, HandleSignal);

    while (true)
    {
        fd_set         readFdSet;
        fd_set         writeFdSet;
        fd_set         errorFdSet;
        int            maxFd   = -1;
        struct timeval timeout = kPollTimeout;

        FD_ZERO(&readFdSet);
        FD_ZERO(&writeFdSet);
        FD_ZERO(&errorFdSet);

        instance.UpdateFdSet(readFdSet, writeFdSet, errorFdSet, maxFd, timeout);
        rval = select(maxFd + 1, &readFdSet, &writeFdSet, &errorFdSet, &timeout);

        if (rval < 0)
        {
            rval = OTBR_ERROR_ERRNO;
            otbrLog(OTBR_LOG_ERR, "select() failed", strerror(errno));
            break;
        }

        instance.Process(readFdSet, writeFdSet, errorFdSet);
    }

exit:
    return rval;
}

void PrintVersion(void)
{
    printf("%s\n", PACKAGE_VERSION);
}

int main(int argc, char *argv[])
{
    const char *interfaceName = kDefaultInterfaceName;
    int         logLevel      = OTBR_LOG_INFO;
    int         opt;
    int         ret = 0;

    while ((opt = getopt(argc, argv, "d:I:v")) != -1)
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
            PrintVersion();
            ExitNow();
            break;

        default:
            fprintf(stderr, "Usage: %s [-I interfaceName] [-d DEBUG_LEVEL] [-v]\n", argv[0]);
            ExitNow(ret = -1);
            break;
        }
    }

    otbrLogInit(kSyslogIdent, logLevel);
    otbrLog(OTBR_LOG_INFO, "Starting border router agent on %s...", interfaceName);

    ret = Mainloop(interfaceName);

    otbrLogDeinit();

exit:
    return ret;
}
