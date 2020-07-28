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

enum ConnectionState
{
    OTBR_REST_CONNECTION_INIT         = 0, ///< Init
    OTBR_REST_CONNECTION_READWAIT     = 1, ///< Wait to read
    OTBR_REST_CONNECTION_CALLBACKWAIT = 2, ///< Wait for callback
    OTBR_REST_CONNECTION_WRITEWAIT    = 3, ///< Wait for write
    OTBR_REST_CONNECTION_COMPLETE     = 4, ///< Have sent response and wait to be deleted

};
class Connection
{
public:
    Connection(steady_clock::time_point aStartTime, Resource *aResource, int aFd);

    void Init();

    otbrError Process(fd_set &aReadFdSet, fd_set &aWriteFdSet);

    otbrError UpdateFdSet(otSysMainloopContext &aMainloop);

    bool WaitRelease();

private:
    otbrError UpdateReadFdSet(fd_set &aReadFdSet, int &aMaxFd);
    otbrError UpdateWriteFdSet(fd_set &aWriteFdSet, int &aMaxFd);
    otbrError UpdateTimeout(timeval &aTimeout);
    otbrError ProcessWaitRead(fd_set &aReadFdSet);
    otbrError ProcessWaitCallback();
    otbrError ProcessWaitWrite(fd_set &aWriteFdSet);
    otbrError Write();
    void      Disconnect();

    steady_clock::time_point mStartTime;
    int                      mFd;
    ConnectionState          mState;
    std::string              mWriteContent;

    Response mResponse;
    Request  mRequest;

    std::unique_ptr<Parser> mParser;
    Resource *              mResource;
};

} // namespace rest
} // namespace otbr

#endif
