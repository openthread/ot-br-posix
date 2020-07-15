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
 *   This file includes definitions for RESTful HTTP server.
 */

#ifndef OTBR_REST_REST_WEB_SERVER_HPP_
#define OTBR_REST_REST_WEB_SERVER_HPP_

#include <chrono>
#include <unordered_map>
#include <unordered_set>

#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <openthread/netdiag.h>

#include "agent/ncp_openthread.hpp"
#include "agent/thread_helper.hpp"
#include "rest/connection.hpp"
#include "rest/handler.hpp"
#include "rest/json.hpp"

using std::chrono::steady_clock;

using otbr::Ncp::ControllerOpenThread;

namespace otbr {
namespace rest {

class Connection;
class Handler;

class RestWebServer
{
public:
    RestWebServer(ControllerOpenThread *aNcp);

    void Init();

    void UpdateFdSet(fd_set &aReadFdSet, int &aMaxFd, timeval &aTimeout);

    void Process(fd_set &aReadFdSet);

    void AddDiag(std::string aRloc16, std::string &aDiag);

private:
    // for service
    otbr::Ncp::ControllerOpenThread *mNcp;
    otbr::agent::ThreadHelper *      mThreadHelper;
    otInstance *                     mInstance;

    // for server configure
    sockaddr_in *mAddress;
    int          mListenFd;

    // for Connection
    int mMaxServeNum = 100;

    std::unordered_map<int, std::unique_ptr<Connection>> mConnectionSet;
    static const int                                     sTimeout = 1000000;
};

} // namespace rest
} // namespace otbr

#endif // OTBR_REST_REST_WEB_SERVER_HPP_
