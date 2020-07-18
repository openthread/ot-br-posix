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

#include "rest/rest_web_server.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

using std::chrono::duration_cast;
using std::chrono::microseconds;
using std::chrono::steady_clock;

namespace otbr {
namespace rest {

const unsigned RestWebServer::kMaxServeNum             = 100;
const unsigned RestWebServer::kTimeout                 = 1000000;
const unsigned RestWebServer::kScaleForCallbackTimeout = 4;
const unsigned RestWebServer::kPortNumber              = 80;

RestWebServer::RestWebServer(ControllerOpenThread *aNcp)
    : mNcp(aNcp)
    , mAddress(new sockaddr_in())

{
}

otbrError RestWebServer::Init()
{
    mThreadHelper             = mNcp->GetThreadHelper();
    mInstance                 = mThreadHelper->GetInstance();
    mAddress->sin_family      = AF_INET;
    mAddress->sin_addr.s_addr = INADDR_ANY;
    mAddress->sin_port        = htons(kPortNumber);

    if ((mListenFd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        otbrLog(OTBR_LOG_ERR, "socket error");

    if ((bind(mListenFd, (struct sockaddr *)mAddress, sizeof(sockaddr))) != 0)
        otbrLog(OTBR_LOG_ERR, "bind error");

    if (listen(mListenFd, 5) < 0)
        otbrLog(OTBR_LOG_ERR, "listen error");

    SetNonBlocking(mListenFd);
}

void RestWebServer::UpdateFdSet(fd_set &aReadFdSet, int &aMaxFd, timeval &aTimeout)
{
    // For ReadRdSet
    UpdateReadFdSet(aReadFdSet, aMaxFd);
    // For timeout
    UpdateTimeout(aTimeout, kTimeout, kScaleForCallback);
}

void RestWebServer::Process(fd_set &aReadFdSet)
{
    Connection *connection;
    int         readFd;
    socklen_t   addrlen = sizeof(sockaddr);
    int         fd;

    // process each connection
    for (auto it = mConnectionSet.begin(); it != mConnectionSet.end(); ++it)
    {
        connection = it->second.get();
        readFd     = it->first;

        if (FD_ISSET(readFd, &aReadFdSet))
            connection->Read();

        if (connection->IsReadTimeout(kTimeout))

            connection->Process();

        if (connection->IsCallbackTimeout(kTimeout * kScaleForCallbackTimeout))

            connection->CallbackProcess();
    }

    UpdateConnections(aReadFdSet);
}

void RestWebServer::UpdateTimeout(timeval &aTimeout, int aTimeoutLen, int aScaleForCallback)
{
    Connection *   connection;
    int            timeoutLen;
    struct timeval timeout = {aTimeout.tv_sec, aTimeout.tv_usec};
    for (auto it = mConnectionSet.begin(); it != mConnectionSet.end(); ++it)
    {
        connection    = it->second.get();
        timeoutLen    = connection->HasCallback() ? aTimeoutLen * aScaleForCallback : aTimeoutLen;
        auto duration = duration_cast<microseconds>(steady_clock::now() - connection->GetStartTime()).count();
        if (duration <= timeoutLen)
        {
            timeout.tv_sec  = 0;
            timeout.tv_usec = timeout.tv_usec == 0 ? std::max(0, timeoutLen - reinterpret_cast<int> duration)
                                                   : std::min(reinterpret_cast<int>(timeout.tv_usec),
                                                              std::max(0, timeoutLen - reinterpret_cast<int> duration));
        }
    }
    if (timercmp(&timeout, &aTimeout, <))
    {
        aTimeout = timeout;
    }
}

void RestWebServer::UpdateConnections(fd_set &aReadFdSet)
{
    auto eraseIt = mConnectionSet.begin();
    for (eraseIt = mConnectionSet.begin(); eraseIt != mConnectionSet.end();)
    {
        Connection *connection = eraseIt->second.get();

        if (connection->IsComplete())
        {
            eraseIt = mConnectionSet.erase(eraseIt);
        }
        else
            eraseIt++;
    }

    if (FD_ISSET(mListenFd, &aReadFdSet))
    {
        while (true)
        {
            fd  = accept(mListenFd, (struct sockaddr *)mAddress, &addrlen);
            err = errno;
            if (fd < 0)
                break;

            if (mConnectionSet.size() < mMaxServeNum)
            {
                // set up new connection
                SetFdNonblocking(fd);
                mConnectionSet.insert(std::make_pair(
                    fd, std::unique_ptr<Connection>(new Connection(steady_clock::now(), mInstance, fd))));
            }
            else
            {
                otbrLog(OTBR_LOG_ERR, "server is busy");
            }
        }

        if (err == EWOULDBLOCK)
        {
            otbrLog(OTBR_LOG_ERR, "Having accept all connections");
        }
        else
        {
            otbrLog(OTBR_LOG_ERR, "Accept error, should handle it");
        }
    }
}

void UpdateReadFdSet(fd_set &aReadFdSet, int &aMaxFd)
{
    Connection *connection;
    int         maxFd;

    FD_SET(mListenFd, &aReadFdSet);
    aMaxFd = aMaxFd < mListenFd ? mListenFd : aMaxFd;

    for (auto it = mConnectionSet.begin(); it != mConnectionSet.end(); ++it)
    {
        connection = it->second.get();
        maxFD      = it->first;
        if (!connection->WaitRead())
        {
            FD_SET(maxFd, &aReadFdSet);
            aMaxFd = maxFd < mListenFd ? mListenFd : maxFd;
        }
    }
}

int RestWebServer::SetFdNonblocking(int fd)
{
    int oldflags;

    oldflags = fcntl(fd, F_GETFL);

    if (fcntl(fd, F_SETFL, oldflags | O_NONBLOCK) == -1)
    {
        return -1;
    }
    return 0;
}

} // namespace rest
} // namespace otbr
