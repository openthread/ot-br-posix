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

/**
 * This class implements a REST server.
 *
 */
class RestWebServer
{
public:
    /**
     * a static function which call the constructor of REST server and return a pointer to the server instance.
     *
     * @param[in]   aNcp  A pointer to the NCP controller.
     *
     * @returns A pointer pointing to the static rest server instance.
     *
     */
    static RestWebServer *GetRestWebServer(ControllerOpenThread *aNcp);

    /**
     * This method initializes the REST server.
     *
     * @retval  OTBR_ERROR_NONE     REST server initialized successfully.
     * @retval  OTBR_ERROR_REST     Failed due to rest error .
     *
     */
    otbrError Init(void);

    /**
     * This method updates the file descriptor sets and timeout for mainloop.
     *
     * @param[inout]    aMainloop   A reference to OpenThread mainloop context.
     *
     */
    void UpdateFdSet(otSysMainloopContext &aMainloop);

    /**
     * This method performs processing.
     *
     * @param[in]       aMainloop   A reference to OpenThread mainloop context.
     *
     */
    otbrError Process(otSysMainloopContext &aMainloop);

private:
    RestWebServer(ControllerOpenThread *aNcp);
    otbrError UpdateConnections(fd_set &aReadFdSet);
    void      CreateNewConnection(int32_t &aFd);
    otbrError Accept(int32_t aListenFd);
    otbrError InitializeListenFd(void);
    bool      SetFdNonblocking(int32_t fd);

    // Resource handler
    Resource mResource;
    // Struct for server configuration
    sockaddr_in mAddress;
    // File descriptor for listening
    int32_t mListenFd;
    // Connection List
    std::unordered_map<int32_t, std::unique_ptr<Connection>> mConnectionSet;
};

} // namespace rest
} // namespace otbr

#endif // OTBR_REST_REST_WEB_SERVER_HPP_
