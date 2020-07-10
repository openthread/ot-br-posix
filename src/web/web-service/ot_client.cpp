/*
 *  Copyright (c) 2020, The OpenThread Authors.
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

#include "web/web-service/ot_client.hpp"

#include <openthread/platform/toolchain.h>

#include <errno.h>
#include <inttypes.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "common/code_utils.hpp"
#include "common/logging.hpp"
#include "utils/strcpy_utils.hpp"

// Temporary solution before posix platform header files are cleaned up.
#ifndef OPENTHREAD_POSIX_APP_SOCKET_NAME
#define OPENTHREAD_POSIX_APP_SOCKET_NAME "/tmp/openthread.sock"
#endif

namespace otbr {
namespace Web {

OpenThreadClient::OpenThreadClient(void)
    : mTimeout(kDefaultTimeout)
    , mSocket(-1)
{
}

OpenThreadClient::~OpenThreadClient(void)
{
    Disconnect();
}

void OpenThreadClient::Disconnect(void)
{
    if (mSocket != -1)
    {
        close(mSocket);
        mSocket = -1;
    }
}

bool OpenThreadClient::Connect(void)
{
    struct sockaddr_un sockname;
    int                ret;

    mSocket = socket(AF_UNIX, SOCK_STREAM, 0);
    VerifyOrExit(mSocket != -1, perror("socket"); ret = EXIT_FAILURE);

    memset(&sockname, 0, sizeof(struct sockaddr_un));
    sockname.sun_family = AF_UNIX;
    strcpy_safe(sockname.sun_path, sizeof(sockname.sun_path), OPENTHREAD_POSIX_APP_SOCKET_NAME);

    ret = connect(mSocket, reinterpret_cast<const struct sockaddr *>(&sockname), sizeof(struct sockaddr_un));

    if (ret == -1)
    {
        otbrLog(OTBR_LOG_ERR, "OpenThread daemon is not running.");
    }

exit:
    return ret == 0;
}

char *OpenThreadClient::Execute(const char *aFormat, ...)
{
    va_list args;
    int     ret;
    char *  rval = nullptr;
    ssize_t count;
    size_t  rxLength = 0;

    va_start(args, aFormat);
    ret = vsnprintf(&mBuffer[1], sizeof(mBuffer) - 1, aFormat, args);
    va_end(args);

    if (ret < 0)
    {
        otbrLog(OTBR_LOG_ERR, "Failed to generate command: %s", strerror(errno));
    }

    mBuffer[0] = '\n';
    ret++;

    if (ret == sizeof(mBuffer))
    {
        otbrLog(OTBR_LOG_ERR, "Command exceeds maximum limit: %d", kBufferSize);
    }

    mBuffer[ret] = '\n';
    ret++;

    count = write(mSocket, mBuffer, ret);

    if (count < ret)
    {
        mBuffer[ret] = '\0';
        otbrLog(OTBR_LOG_ERR, "Failed to send command: %s", mBuffer);
    }

    for (int i = 0; i < mTimeout; ++i)
    {
        fd_set  readFdSet;
        timeval timeout = {0, 1000};
        char *  done;

        FD_ZERO(&readFdSet);
        FD_SET(mSocket, &readFdSet);

        ret = select(mSocket + 1, &readFdSet, nullptr, nullptr, &timeout);
        VerifyOrExit(ret != -1 || errno == EINTR);
        if (ret <= 0)
        {
            continue;
        }

        count = read(mSocket, &mBuffer[rxLength], sizeof(mBuffer) - rxLength);
        VerifyOrExit(count > 0);
        rxLength += count;

        mBuffer[rxLength] = '\0';
        done              = strstr(mBuffer, "Done\r\n");

        if (done != nullptr)
        {
            // remove trailing \r\n
            if (done - rval > 2)
            {
                done[-2] = '\0';
            }

            rval = mBuffer;
            break;
        }
    }

exit:
    return rval;
}

int OpenThreadClient::Scan(WpanNetworkInfo *aNetworks, int aLength)
{
    char *result;
    int   rval = 0;

    mTimeout = 5000;
    result   = Execute("scan");
    VerifyOrExit(result != nullptr);

    for (result = strtok(result, "\r\n"); result != nullptr && rval < aLength; result = strtok(nullptr, "\r\n"))
    {
        static const char kCliPrompt[] = "> ";
        char *            cliPrompt;
        int               matched;
        int               joinable;
        int               lqi;

        // remove prompt
        if ((cliPrompt = strstr(result, kCliPrompt)) != nullptr)
        {
            if (cliPrompt == result)
            {
                result += sizeof(kCliPrompt) - 1;
            }
            else
            {
                memmove(cliPrompt, cliPrompt + sizeof(kCliPrompt) - 1, strlen(cliPrompt) - sizeof(kCliPrompt) - 1);
            }
        }

        matched = sscanf(result,
                         "| %d | %s | %" PRIx64
                         " | %hx | %02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx | %hu | %hhd | %d |",
                         &joinable, aNetworks[rval].mNetworkName, &aNetworks[rval].mExtPanId, &aNetworks[rval].mPanId,
                         &aNetworks[rval].mHardwareAddress[0], &aNetworks[rval].mHardwareAddress[1],
                         &aNetworks[rval].mHardwareAddress[2], &aNetworks[rval].mHardwareAddress[3],
                         &aNetworks[rval].mHardwareAddress[4], &aNetworks[rval].mHardwareAddress[5],
                         &aNetworks[rval].mHardwareAddress[6], &aNetworks[rval].mHardwareAddress[7],
                         &aNetworks[rval].mChannel, &aNetworks[rval].mRssi, &lqi);

        // 15 is the number of output arguments of the last sscanf()
        if (matched != 15)
        {
            continue;
        }

        aNetworks[rval].mAllowingJoin = joinable != 0;
        ++rval;
    }

    mTimeout = kDefaultTimeout;

exit:
    return rval;
}

bool OpenThreadClient::FactoryReset(void)
{
    const char *result;
    bool        rval = false;
#if __APPLE__
    typedef sig_t sighandler_t;
#endif
    sighandler_t handler;

    // Ignore the expected SIGPIPE signal during daemon reset.
    handler = signal(SIGPIPE, SIG_IGN);
    Execute("factoryreset");
    signal(SIGPIPE, handler);
    Disconnect();
    sleep(1);
    VerifyOrExit(rval = Connect());

    result = Execute("version");
    VerifyOrExit(result != nullptr);

    rval = strstr(result, "OPENTHREAD") != nullptr;

exit:
    return rval;
}

} // namespace Web
} // namespace otbr
