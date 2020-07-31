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

/**
 * @file
 *   This file includes definitions for RESTful HTTP server.
 */

#ifndef OTBR_REST_REST_WEB_SERVER_HPP_
#define OTBR_REST_REST_WEB_SERVER_HPP_

#include "rest/connection.hpp"

using otbr::Ncp::ControllerOpenThread;
using std::chrono::steady_clock;

namespace otbr {
namespace rest {

class RestWebServer
{
public:
    static RestWebServer *GetRestWebServer(ControllerOpenThread *aNcp);

    otbrError Init(void);

    void UpdateFdSet(otSysMainloopContext &aMainloop);

    otbrError Process(otSysMainloopContext &aMainloop);

private:
    // For service
    RestWebServer(ControllerOpenThread *aNcp);

    otbrError UpdateConnections(fd_set &aReadFdSet);

    void CreateNewConnection(int32_t aFd);

    otbrError Accept(int32_t aListenFd);

    bool SetFdNonblocking(int32_t fd);

    otbrError InitializeListenFd(void);
    // Resource handler
    Resource mResource;
    // For server configure
    sockaddr_in mAddress;
    int32_t     mListenFd;
    // For Connection
    std::unordered_map<int32_t, std::unique_ptr<Connection>> mConnectionSet;
    // Server
    static std::unique_ptr<RestWebServer> sRestWebServer;
};

} // namespace rest
} // namespace otbr

#endif // OTBR_REST_REST_WEB_SERVER_HPP_
