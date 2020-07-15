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

#include "common/logging.hpp"
#include "openthread/thread_ftd.h"

using std::chrono::duration_cast;
using std::chrono::microseconds;
using std::chrono::seconds;
using std::chrono::steady_clock;
namespace otbr {
namespace rest {

static int SetNonBlocking(int fd)
{
    int oldflags;

    oldflags = fcntl(fd, F_GETFL);

    if (fcntl(fd, F_SETFL, oldflags | O_NONBLOCK) == -1)
    {
        return -1;
    }

    return 0;
}



RestWebServer::RestWebServer(ControllerOpenThread *aNcp)
    : mNcp(aNcp)
    , mAddress(new sockaddr_in())

{
}

void RestWebServer::Init()
{    
    mThreadHelper = mNcp->GetThreadHelper();
    mInstance     = mThreadHelper->GetInstance();
    

    mAddress->sin_family      = AF_INET;
    mAddress->sin_addr.s_addr = INADDR_ANY;
    mAddress->sin_port        = htons(80);

    if ((mListenFd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        otbrLog(OTBR_LOG_ERR, "socket error");
    }

    if ((bind(mListenFd, (struct sockaddr *)mAddress, sizeof(sockaddr))) != 0)
    {
        otbrLog(OTBR_LOG_ERR, "bind error");
    }

    if (listen(mListenFd, 5) < 0)
    {
        otbrLog(OTBR_LOG_ERR, "listen error");
    }

    SetNonBlocking(mListenFd);
}


void RestWebServer::UpdateFdSet(fd_set &aReadFdSet, int &aMaxFd, timeval &aTimeout)
{
    
    int maxFd;
    struct timeval timeout = {aTimeout.tv_sec, aTimeout.tv_usec};

    //set each time need to handled 
    otThreadSetReceiveDiagnosticGetCallback(mInstance, &Handler::DiagnosticResponseHandler, this);

    FD_SET(mListenFd, &aReadFdSet);
    if (aMaxFd < mListenFd)
    {
        aMaxFd = mListenFd;
    }

    for (auto it = mConnectionSet.begin(); it != mConnectionSet.end(); ++it)
    {
        Connection *  connection = it->second.get();
        maxFd = it->first;
        auto duration = duration_cast<microseconds>(steady_clock::now() - connection->GetConnectionStartTime()).count();

        if (duration <= sTimeout)
        {
            timeout.tv_sec = 0;
            timeout.tv_usec = timeout.tv_usec == 0 ?  std::max(0, sTimeout - (int)duration): std::min(int(timeout.tv_usec), std::max(0, sTimeout - (int)duration));
            FD_SET(maxFd, &aReadFdSet);
            if (aMaxFd < maxFd) aMaxFd = maxFd;
        }
        else
        {
            timeout.tv_sec  = 0;
            timeout.tv_usec = 0;
        }
        if (connection->GetCallbackFlag())
        {
            duration = duration_cast<microseconds>(steady_clock::now() - connection->GetDiagStartTime()).count();

            if (duration <= sTimeout * 4)
            {
                timeout.tv_sec = 0;
                timeout.tv_usec = timeout.tv_usec == 0 ? std::max(0, 4 * sTimeout - (int)duration): std::min(int(timeout.tv_usec), std::max(0, 4 * sTimeout - (int)duration));;
            }
            else
            {
                timeout.tv_sec  = 0;
                timeout.tv_usec = 0;
            }
        }
    }

    if (timercmp(&timeout, &aTimeout, <))
    {
        aTimeout = timeout;
    }
}

void RestWebServer::Process(fd_set &aReadFdSet)
{
    
    int readFd;
    socklen_t   addrlen = sizeof(sockaddr);
    int  fd;
    auto err = errno;


    //check each connection
    for (auto it = mConnectionSet.begin(); it != mConnectionSet.end(); ++it)
    {
        
        Connection *connection    = it->second.get();
        readFd = it->first;
        
        if (FD_ISSET(readFd, &aReadFdSet))
                connection->SetReadFlag(1);
        
        connection->Check();
    }

    // erase connection if it is labeled
    auto eraseIt = mConnectionSet.begin();
    for (eraseIt = mConnectionSet.begin(); eraseIt != mConnectionSet.end();)
    {
        Connection * connection = eraseIt->second.get();

        if (connection->GetStatus() == 1)
        {
           eraseIt = mConnectionSet.erase(eraseIt);
        }
        else
            eraseIt++;
    }
    // create new connection if ListenFd is set
    if (FD_ISSET(mListenFd, &aReadFdSet))
    {
        while (true)
        {
            fd  = accept(mListenFd, (struct sockaddr *)mAddress, &addrlen);
            err = errno;
            if (fd < 0)
                break;

            if (mConnectionSet.size() < (unsigned)mMaxServeNum)
            {
                // set up new connection
                SetNonBlocking(fd);
                mConnectionSet.insert(std::make_pair(fd, std::unique_ptr<Connection>(new Connection(steady_clock::now(), mInstance, fd))));
               
               
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
            otbrLog(OTBR_LOG_ERR, "Accept error, should havndle it");
        }
    }
}

void RestWebServer::AddDiag(std::string aRloc16, std::string& aDiag){
    
    

    for (auto it = mConnectionSet.begin(); it != mConnectionSet.end(); ++it){
        Connection * connection = it->second.get();
        if (!connection->CheckDiag(aRloc16)){
            connection->AddDiag(aRloc16,aDiag);
        }
    }
}

} // namespace rest
} // namespace otbr
