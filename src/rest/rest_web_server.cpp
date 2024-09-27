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

#define OTBR_LOG_TAG "REST"

#include "rest/rest_web_server.hpp"

#include <arpa/inet.h>
#include <cerrno>

#include <fcntl.h>

#include "utils/socket_utils.hpp"

using std::chrono::duration_cast;
using std::chrono::microseconds;
using std::chrono::steady_clock;

namespace otbr {
namespace rest {

// Maximum number of connection a server support at the same time.
static const uint32_t kMaxServeNum = 500;

RestWebServer::RestWebServer(RcpHost &aHost, const std::string &aRestListenAddress, int aRestListenPort)
    : mResource(Resource(&aHost))
    , mListenFd(-1)
{
    mAddress.sin6_family = AF_INET6;
    mAddress.sin6_addr   = in6addr_any;
    mAddress.sin6_port   = htons(aRestListenPort);

    if (!aRestListenAddress.empty())
    {
        if (!ParseListenAddress(aRestListenAddress, &mAddress.sin6_addr))
            otbrLogWarning("Failed to parse REST listen address %s, listening on any address.",
                           aRestListenAddress.c_str());
    }
}

RestWebServer::~RestWebServer(void)
{
    if (mListenFd != -1)
    {
        close(mListenFd);
    }
}

void RestWebServer::Init(void)
{
    mResource.Init();
    InitializeListenFd();
}

void RestWebServer::Update(MainloopContext &aMainloop)
{
    aMainloop.AddFdToReadSet(mListenFd);

    return;
}

void RestWebServer::Process(const MainloopContext &aMainloop)
{
    UpdateConnections(aMainloop.mReadFdSet);
}

void RestWebServer::UpdateConnections(const fd_set &aReadFdSet)
{
    otbrError error   = OTBR_ERROR_NONE;
    auto      eraseIt = mConnectionSet.begin();

    // Erase useless connections
    for (eraseIt = mConnectionSet.begin(); eraseIt != mConnectionSet.end();)
    {
        Connection *connection = eraseIt->second.get();

        if (connection->IsComplete())
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

    if (error != OTBR_ERROR_NONE)
    {
        otbrLogWarning("Failed to accept new connection: %s", otbrErrorString(error));
    }
}

bool RestWebServer::ParseListenAddress(const std::string listenAddress, struct in6_addr *sin6_addr)
{
    const std::string ipv4_prefix       = "::FFFF:";
    const std::string ipv4ListenAddress = ipv4_prefix + listenAddress;

    if (inet_pton(AF_INET6, listenAddress.c_str(), sin6_addr) == 1)
    {
        return true;
    }

    if (inet_pton(AF_INET6, ipv4ListenAddress.c_str(), sin6_addr) == 1)
    {
        return true;
    }

    return false;
}

void RestWebServer::InitializeListenFd(void)
{
    otbrError   error = OTBR_ERROR_NONE;
    std::string errorMessage;
    int32_t     ret;
    int32_t     err = errno;
    int32_t     yes = 1;
    int32_t     no  = 0;

    mListenFd = SocketWithCloseExec(AF_INET6, SOCK_STREAM, 0, kSocketNonBlock);
    VerifyOrExit(mListenFd != -1, err = errno, error = OTBR_ERROR_REST, errorMessage = "socket");

    ret = setsockopt(mListenFd, IPPROTO_IPV6, IPV6_V6ONLY, reinterpret_cast<char *>(&no), sizeof(no));
    VerifyOrExit(ret == 0, err = errno, error = OTBR_ERROR_REST, errorMessage = "sock opt v6only");

    ret = setsockopt(mListenFd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char *>(&yes), sizeof(yes));
    VerifyOrExit(ret == 0, err = errno, error = OTBR_ERROR_REST, errorMessage = "sock opt reuseaddr");

    ret = bind(mListenFd, reinterpret_cast<struct sockaddr *>(&mAddress), sizeof(mAddress));
    VerifyOrExit(ret == 0, err = errno, error = OTBR_ERROR_REST, errorMessage = "bind");

    ret = listen(mListenFd, 5);
    VerifyOrExit(ret >= 0, err = errno, error = OTBR_ERROR_REST, errorMessage = "listen");

exit:

    if (error != OTBR_ERROR_NONE)
    {
        otbrLogErr("InitializeListenFd error %s : %s", errorMessage.c_str(), strerror(err));
    }

    VerifyOrDie(error == OTBR_ERROR_NONE, "otbr rest server init error");
}

otbrError RestWebServer::Accept(int aListenFd)
{
    std::string errorMessage;
    otbrError   error = OTBR_ERROR_NONE;
    int32_t     err;
    int32_t     fd;
    sockaddr_in tmp;
    socklen_t   addrlen = sizeof(tmp);

    fd  = accept(aListenFd, reinterpret_cast<struct sockaddr *>(&mAddress), &addrlen);
    err = errno;

    VerifyOrExit(fd >= 0, err = errno, error = OTBR_ERROR_REST, errorMessage = "accept");

    VerifyOrExit(SetFdNonblocking(fd), err = errno, error = OTBR_ERROR_REST; errorMessage = "set nonblock");

    CreateNewConnection(fd);

exit:
    if (error != OTBR_ERROR_NONE)
    {
        if (fd != -1)
        {
            close(fd);
            fd = -1;
        }
        otbrLogErr("Rest server accept error: %s %s", errorMessage.c_str(), strerror(err));
    }

    return error;
}

void RestWebServer::CreateNewConnection(int &aFd)
{
    auto it =
        mConnectionSet.emplace(aFd, std::unique_ptr<Connection>(new Connection(steady_clock::now(), &mResource, aFd)));

    if (it.second == true)
    {
        Connection *connection = it.first->second.get();
        connection->Init();
    }
    else
    {
        // failure on inserting new connection
        close(aFd);
        aFd = -1;
    }
}

bool RestWebServer::SetFdNonblocking(int32_t fd)
{
    int32_t oldMode;
    bool    ret = true;

    oldMode = fcntl(fd, F_GETFL);

    VerifyOrExit(fcntl(fd, F_SETFL, oldMode | O_NONBLOCK) >= 0, ret = false);

exit:
    return ret;
}

} // namespace rest
} // namespace otbr
