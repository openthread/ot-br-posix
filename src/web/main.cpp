/*
 *  Copyright (c) 2017, The OpenThread Authors.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 *   This file is the entry of the program, it starts a Web service.
 */
#ifndef OT_WEB_FILE_PATH
#define OT_WEB_FILE_PATH "/usr/local/share/border-router/frontend"
#endif

#define OT_HTTP_PORT 80

#include "otbr-config.h"

#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include "common/code_utils.hpp"
#include "common/logging.hpp"
#include "web-service/web_server.hpp"

static const char kSyslogIdent[] = "otWeb";
static const char kDefaultInterfaceName[] = "wpan0";

void PrintVersion(void)
{
    printf("%s\n", PACKAGE_VERSION);
}

int main(int argc, char **argv)
{
    const char *interfaceName = NULL;
    const char *httpPort = NULL;
    int         logLevel = OTBR_LOG_INFO;
    int         ret = 0;
    int         opt;
    uint16_t    port = OT_HTTP_PORT;

    ot::Web::WebServer *server = NULL;

    while ((opt = getopt(argc, argv, "d:I:p:v")) != -1)
    {
        switch (opt)
        {
        case 'd':
            logLevel = atoi(optarg);
            break;

        case 'I':
            interfaceName = optarg;
            break;

        case 'p':
            httpPort = optarg;
            VerifyOrExit(httpPort != NULL);
            port = atoi(httpPort);
            break;

        case 'v':
            PrintVersion();
            ExitNow();
            break;

        default:
            fprintf(stderr, "Usage: %s [-d DEBUG_LEVEL] [-I interfaceName] [-p port] [-v]\n", argv[0]);
            ExitNow(ret = -1);
            break;
        }
    }

    if (interfaceName == NULL)
    {
        interfaceName = kDefaultInterfaceName;
        printf("interfaceName not specified, using default %s\n", interfaceName);
    }

    if (httpPort == NULL)
    {
        printf("http port not specified, using default %d\n", port);
    }

    otbrLogInit(kSyslogIdent, logLevel);
    otbrLog(OTBR_LOG_INFO, "border router web started on %s", interfaceName);

    server = new ot::Web::WebServer();
    server->StartWebServer(interfaceName, port);

    otbrLogDeinit();

exit:
    if (server != NULL)
    {
        delete server;
    }
    return ret;
}
