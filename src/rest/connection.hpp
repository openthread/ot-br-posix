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
 *   This file includes connection definition for RESTful HTTP server.
 */

#ifndef OTBR_REST_CONNECTION_HPP_
#define OTBR_REST_CONNECTION_HPP_

#include <string.h>
#include <unistd.h>

#include "rest/parser.hpp"
#include "rest/resource.hpp"

using std::chrono::steady_clock;

namespace otbr {
namespace rest {

/**
 * This class implements a Connection class of each socket connection .
 *
 */
class Connection
{
public:
    /**
     * The constructor to initialize a socket connection instance.
     *
     * @param[in]   aStartTime  The reference time for each check point of a conneciton.
     * @param[in]   aResource   A pointer a resource handler.
     * @param[in]   aFd         The file descriptor for this conneciton.
     *
     */
    Connection(steady_clock::time_point aStartTime, Resource *aResource, int aFd);

     /**
     * This method initialize the connection.
     *
     *
     */
    void Init(void);

    /**
     * This method performs processing.
     *
     * @param[in]       aReadFdSet    The read file descriptors.
     * @param[in]       aWriteFdSet   The Write file descriptors.
     *
     */
    otbrError Process(fd_set &aReadFdSet, fd_set &aWriteFdSet);

    /**
     * This method updates the file descriptor sets and timeout for mainloop.
     *
     * @param[inout]    aMainloop   A reference to OpenThread mainloop context.
     *
     */
    void UpdateFdSet(otSysMainloopContext &aMainloop);

    /**
     * This method indicates whether this connection no longer need process.
     *
     * @retval  true     This connection could be released in next loop.
     * @retval  false    This connection still need to be processed in next loop.
     *
     */
    bool IsComplete(void);

private:
   
    void UpdateReadFdSet(fd_set &aReadFdSet, int &aMaxFd);
    void UpdateWriteFdSet(fd_set &aWriteFdSet, int &aMaxFd);
    void UpdateTimeout(timeval &aTimeout);
    otbrError ProcessWaitRead(fd_set &aReadFdSet);
    otbrError ProcessWaitCallback(void);
    otbrError ProcessWaitWrite(fd_set &aWriteFdSet);
    otbrError Write(void);
    otbrError Handle(void);
    void      Disconnect(void);
    // Timestamp used for each check point of a connection
    steady_clock::time_point mStartTime;
    int                      mFd;
    ConnectionState          mState;
    // Write buffer in case write multiple times
    std::string              mWriteContent;

    Response mResponse;
    Request  mRequest;
    // HTTP parser instance
    Parser    mParser;
    // Resource handler instance
    Resource *mResource;
};

} // namespace rest
} // namespace otbr

#endif // OTBR_REST_CONNECTION_HPP_
