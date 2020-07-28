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

#include <cerrno>

#include <fcntl.h>

using std::chrono::duration_cast;
using std::chrono::microseconds;
using std::chrono::steady_clock;

namespace otbr {
namespace rest {

const uint32_t RestWebServer::kMaxServeNum = 500;
const uint32_t RestWebServer::kPortNumber  = 81;

std::unordered_map<int, std::unique_ptr<Connection>> RestWebServer::mConnectionSet;
int                                                  RestWebServer::mListenFd;
std::unique_ptr<sockaddr_in>                         RestWebServer::mAddress;
std::unique_ptr<Resource>                            RestWebServer::mResource;

otbrError RestWebServer::Init(ControllerOpenThread *aNcp)
{
    otbrError error = OTBR_ERROR_NONE;

    mResource = std::unique_ptr<Resource>(new Resource(aNcp));

    mResource->Init();

    error = InitializeListenFd();

    return error;
}

otbrError RestWebServer::UpdateFdSet(otSysMainloopContext &aMainloop)
{
    otbrError error = OTBR_ERROR_NONE;

    FD_SET(mListenFd, &aMainloop.mReadFdSet);
    aMainloop.mMaxFd = aMainloop.mMaxFd < mListenFd ? mListenFd : aMainloop.mMaxFd;

    Connection *connection;
    for (auto it = mConnectionSet.begin(); it != mConnectionSet.end(); ++it)
    {
        connection = it->second.get();
        error      = connection->UpdateFdSet(aMainloop);
    }

    return error;
}

otbrError RestWebServer::Process(otSysMainloopContext &aMainloop)
{
    otbrError   error = OTBR_ERROR_NONE;
    Connection *connection;

    error = UpdateConnections(aMainloop.mReadFdSet);

    for (auto it = mConnectionSet.begin(); it != mConnectionSet.end(); ++it)
    {
        connection = it->second.get();
        error      = connection->Process(aMainloop.mReadFdSet, aMainloop.mWriteFdSet);
    }

    return error;
}

otbrError RestWebServer::UpdateConnections(fd_set &aReadFdSet)
{
    otbrError error   = OTBR_ERROR_NONE;
    auto      eraseIt = mConnectionSet.begin();

    // Erase useless connections
    for (eraseIt = mConnectionSet.begin(); eraseIt != mConnectionSet.end();)
    {
        Connection *connection = eraseIt->second.get();

        if (connection->WaitRelease())
        {
            eraseIt = mConnectionSet.erase(eraseIt);
        }
        else
        {
            eraseIt++;
        }
    }

    // Create new connection if listenfd is set
    if (FD_ISSET(mListenFd, &aReadFdSet) && mConnectionSet.size() < kMaxServeNum)
    {
        error = Accept(mListenFd);
    }

    return error;
}

otbrError RestWebServer::InitializeListenFd()
{
    otbrError   error = OTBR_ERROR_NONE;
    std::string errorMessage;
    int         ret;
    int         err    = errno;
    int         optval = 1;

    mAddress = std::unique_ptr<sockaddr_in>(new sockaddr_in());

    mAddress->sin_family      = AF_INET;
    mAddress->sin_addr.s_addr = INADDR_ANY;
    mAddress->sin_port        = htons(kPortNumber);

    mListenFd = socket(AF_INET, SOCK_STREAM, 0);

    VerifyOrExit(mListenFd != -1, err = errno, error = OTBR_ERROR_REST, errorMessage = "socket");

    ret = setsockopt(mListenFd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char *>(&optval), sizeof(optval));

    VerifyOrExit(ret == 0, err = errno, error = OTBR_ERROR_REST, errorMessage = "sock opt");

    ret = bind(mListenFd, reinterpret_cast<struct sockaddr *>(mAddress.get()), sizeof(sockaddr));

    VerifyOrExit(ret == 0, err = errno, error = OTBR_ERROR_REST, errorMessage = "bind");

    ret = listen(mListenFd, 5);

    VerifyOrExit(ret >= 0, err = errno, error = OTBR_ERROR_REST, errorMessage = "listen");

    ret = SetFdNonblocking(mListenFd);

    VerifyOrExit(ret, err = errno, error = OTBR_ERROR_REST, errorMessage = " set nonblock");

exit:
    if (error != OTBR_ERROR_NONE)
    {
        otbrLog(OTBR_LOG_ERR, "otbr rest server init error %s : %s", errorMessage.c_str(), strerror(err));
    }

    return error;
}

otbrError RestWebServer::Accept(int aListenFd)
{
    std::string errorMessage;
    otbrError   error = OTBR_ERROR_NONE;
    int         err;
    int         fd;
    sockaddr_in tmp;
    socklen_t   addrlen = sizeof(tmp);

    fd  = accept(aListenFd, reinterpret_cast<struct sockaddr *>(mAddress.get()), &addrlen);
    err = errno;

    VerifyOrExit(fd >= 0, err = errno, error = OTBR_ERROR_REST; errorMessage = "accept");
    if (fd >= 0)
    {
        // Set up new connection

        VerifyOrExit(SetFdNonblocking(fd), err = errno, error = OTBR_ERROR_REST; errorMessage = "set nonblock");

        auto it = mConnectionSet.insert(
            std::make_pair(fd, std::unique_ptr<Connection>(new Connection(steady_clock::now(), mResource.get(), fd))));
        if (it.second == true)
        {
            Connection *connection = it.first->second.get();
            connection->Init();
        }
        else
        {
            // Insert failed
            close(fd);
        }
    }

exit:
    if (error != OTBR_ERROR_NONE)
    {
        otbrLog(OTBR_LOG_ERR, "rest server accept error: %s %s", errorMessage.c_str(), strerror(err));
    }
    return error;
}

bool RestWebServer::SetFdNonblocking(int fd)
{
    int  oldflags;
    bool ret = true;

    oldflags = fcntl(fd, F_GETFL);

    VerifyOrExit(fcntl(fd, F_SETFL, oldflags | O_NONBLOCK) >= 0, ret = false);

exit:
    return ret;
}

} // namespace rest
} // namespace otbr

otbrError RestServerInit(otbr::Ncp::ControllerOpenThread *aNcp)
{
    otbrError error = OTBR_ERROR_NONE;

    error = otbr::rest::RestWebServer::Init(aNcp);

    return error;
}

otbrError RestServerUpdateFdSet(otSysMainloopContext &aMainloop)
{
    otbrError error = OTBR_ERROR_NONE;

    error = otbr::rest::RestWebServer::UpdateFdSet(aMainloop);

    return error;
}

otbrError RestServerProcess(otSysMainloopContext &aMainloop)
{
    otbrError error = OTBR_ERROR_NONE;

    error = otbr::rest::RestWebServer::Process(aMainloop);

    return error;
}
