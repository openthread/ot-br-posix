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

#include "otbr-config.h"

#include <errno.h>
#include <stdio.h>
#include <syslog.h>
#include <unistd.h>

#include "border_agent.hpp"
#include "common/code_utils.hpp"

static const char kSyslogIdent[]            = "otBorderAgent";
static const char kDefaultInterfaceName[]   = "wpan0";

// Default poll timeout.
static const struct timeval kPollTimeout = {10, 0};

int Mainloop(const char *aInterfaceName)
{
    int rval = 0;

    ot::BorderAgent::BorderAgent br(aInterfaceName);

    while (true)
    {
        fd_set         readFdSet;
        fd_set         writeFdSet;
        fd_set         errorFdSet;
        int            maxFd = -1;
        struct timeval timeout = kPollTimeout;

        FD_ZERO(&readFdSet);
        FD_ZERO(&writeFdSet);
        FD_ZERO(&errorFdSet);

        br.UpdateFdSet(readFdSet, writeFdSet, errorFdSet, maxFd, timeout);
        rval = select(maxFd + 1, &readFdSet, &writeFdSet, &errorFdSet, &timeout);

        if ((rval < 0) && (errno != EINTR))
        {
            rval = errno;
            perror("select");
            break;
        }

        br.Process(readFdSet, writeFdSet, errorFdSet);
    }

    return rval;
}

void PrintVersion(void)
{
    printf("%s\n", PACKAGE_VERSION);
}

int main(int argc, char *argv[])
{
    const char *interfaceName = NULL;
    int         ret = 0;
    int         opt;

    while ((opt = getopt(argc, argv, "vI:")) != -1)
    {
        switch (opt)
        {
        case 'I':
            interfaceName = optarg;
            break;

        case 'v':
            PrintVersion();
            ExitNow();
            break;

        default:
            fprintf(stderr, "Usage: %s [-I interfaceName] [-v]\n", argv[0]);
            ExitNow(ret = -1);
            break;
        }
    }

    if (interfaceName == NULL)
    {
        interfaceName = kDefaultInterfaceName;
        printf("interfaceName not specified, using default %s\n", interfaceName);
    }

    openlog(kSyslogIdent, LOG_CONS | LOG_PID, LOG_USER);
    syslog(LOG_INFO, "backbone router started on %s", interfaceName);

    ret = Mainloop(interfaceName);

    closelog();

exit:
    return ret;
}
