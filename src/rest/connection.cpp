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

#include "rest/connection.hpp"

#include <cerrno>

#include <assert.h>

#include <sys/time.h>

using std::chrono::duration_cast;
using std::chrono::microseconds;
using std::chrono::seconds;
using std::chrono::steady_clock;

namespace otbr {
namespace rest {

// Timeout settings in microseconds
static const uint32_t kCallbackTimeout       = 10000000;
static const uint32_t kCallbackCheckInterval = 500000;
static const uint32_t kWriteTimeout          = 10000000;
static const uint32_t kReadTimeout           = 1000000;

Connection::Connection(steady_clock::time_point aStartTime, Resource *aResource, int aFd)
    : mStartTime(aStartTime)
    , mFd(aFd)
    , mState(OTBR_REST_CONNECTION_INIT)
    , mParser(&mRequest)
    , mResource(aResource)
{
}

void Connection::Init(void)
{
    mParser.Init();
}

void Connection::UpdateReadFdSet(fd_set &aReadFdSet, int &aMaxFd)
{
    if (mState == OTBR_REST_CONNECTION_READWAIT || mState == OTBR_REST_CONNECTION_INIT)
    {
        FD_SET(mFd, &aReadFdSet);
        aMaxFd = aMaxFd < mFd ? mFd : aMaxFd;
    }
}

void Connection::UpdateWriteFdSet(fd_set &aWriteFdSet, int &aMaxFd)
{
    if (mState == OTBR_REST_CONNECTION_WRITEWAIT)
    {
        FD_SET(mFd, &aWriteFdSet);
        aMaxFd = aMaxFd < mFd ? mFd : aMaxFd;
    }
}

void Connection::UpdateTimeout(timeval &aTimeout)
{
    struct timeval timeout;
    uint32_t       timeoutLen = kReadTimeout;
    auto           duration   = duration_cast<microseconds>(steady_clock::now() - mStartTime).count();

    switch (mState)
    {
    case OTBR_REST_CONNECTION_READWAIT:
        timeoutLen = kReadTimeout;
        break;
    case OTBR_REST_CONNECTION_CALLBACKWAIT:
        timeoutLen = kCallbackCheckInterval;
        break;
    case OTBR_REST_CONNECTION_WRITEWAIT:
        timeoutLen = kWriteTimeout;
        break;
    case OTBR_REST_CONNECTION_COMPLETE:
        timeoutLen = 0;
        break;
    default:
        break;
    }

    if (duration <= timeoutLen)
    {
        timeout.tv_sec  = 0;
        timeout.tv_usec = timeoutLen - duration;
    }
    else
    {
        timeout.tv_sec  = 0;
        timeout.tv_usec = 0;
    }

    if (timercmp(&timeout, &aTimeout, <))
    {
        aTimeout = timeout;
    }
}

void Connection::UpdateFdSet(otSysMainloopContext &aMainloop)
{
    UpdateTimeout(aMainloop.mTimeout);
    UpdateReadFdSet(aMainloop.mReadFdSet, aMainloop.mMaxFd);
    UpdateWriteFdSet(aMainloop.mWriteFdSet, aMainloop.mMaxFd);
}

void Connection::Disconnect(void)
{
    mState = OTBR_REST_CONNECTION_COMPLETE;

    if (mFd != -1)
    {
        close(mFd);
        mFd = -1;
    }
}

otbrError Connection::Process(fd_set &aReadFdSet, fd_set &aWriteFdSet)
{
    otbrError error = OTBR_ERROR_NONE;

    switch (mState)
    {
    // Initial state, directly read for the first time.
    case OTBR_REST_CONNECTION_INIT:
    case OTBR_REST_CONNECTION_READWAIT:
        error = ProcessWaitRead(aReadFdSet);
        break;
    case OTBR_REST_CONNECTION_CALLBACKWAIT:
        //  Wait for Callback process.
        error = ProcessWaitCallback();
        break;
    case OTBR_REST_CONNECTION_WRITEWAIT:
        error = ProcessWaitWrite(aWriteFdSet);
        break;
    default:
        assert(false);
    }

    if (error != OTBR_ERROR_NONE)
    {
        Disconnect();
    }
    return error;
}

otbrError Connection::ProcessWaitRead(fd_set &aReadFdSet)
{
    otbrError error = OTBR_ERROR_NONE;
    int32_t   received;
    char      buf[2048];
    int32_t   err;
    auto      duration = duration_cast<microseconds>(steady_clock::now() - mStartTime).count();

    VerifyOrExit(duration <= kReadTimeout, mState = OTBR_REST_CONNECTION_READTIMEOUT);

    VerifyOrExit(FD_ISSET(mFd, &aReadFdSet) || mState == OTBR_REST_CONNECTION_INIT);

    do
    {
        mState   = OTBR_REST_CONNECTION_READWAIT;
        received = read(mFd, buf, sizeof(buf));
        err      = errno;
        if (received > 0)
        {
            mParser.Process(buf, received);
        }

    } while ((received > 0 && !mRequest.IsComplete()) || err == EINTR);

    if (mRequest.IsComplete())
    {
        Handle();
    }

    VerifyOrExit(received != 0, mState = OTBR_REST_CONNECTION_READTIMEOUT);

    // Catch ret = -1 error, then try to send back a response that there is an internal error.
    VerifyOrExit(received > 0 || (received == -1 && (err == EAGAIN || err == EWOULDBLOCK)),
                 mState = OTBR_REST_CONNECTION_INTERNALERROR);

exit:

    switch (mState)
    {
    case OTBR_REST_CONNECTION_READTIMEOUT:

        mResource->ErrorHandler(mResponse, 408);
        error = Write();
        break;

    case OTBR_REST_CONNECTION_INTERNALERROR:

        mResource->ErrorHandler(mResponse, 500);
        error = Write();
        break;

    default:
        break;
    }

    return error;
}

otbrError Connection::Handle(void)
{
    otbrError error = OTBR_ERROR_NONE;

    VerifyOrExit((shutdown(mFd, SHUT_RD) == 0), mState = OTBR_REST_CONNECTION_INTERNALERROR);

    mResource->Handle(mRequest, mResponse);

    if (mResponse.NeedCallback())
    {
        // Set its state as Waitforcallback and refresh its timer
        mState     = OTBR_REST_CONNECTION_CALLBACKWAIT;
        mStartTime = steady_clock::now();
    }
    else
    {
        // Normal Write back process.
        error = Write();
    }

exit:

    if (mState == OTBR_REST_CONNECTION_INTERNALERROR)
    {
        mResource->ErrorHandler(mResponse, 500);
        error = Write();
    }

    return error;
}

otbrError Connection::ProcessWaitCallback(void)
{
    otbrError error    = OTBR_ERROR_NONE;
    auto      duration = duration_cast<microseconds>(steady_clock::now() - mStartTime).count();

    mResource->HandleCallback(mRequest, mResponse);

    if (mResponse.IsComplete())
    {
        error = Write();
    }
    else
    {
        if (duration >= kCallbackTimeout)
        {
            mResource->ErrorHandler(mResponse, 404);
            error = Write();
        }

    }
    
    // if (duration >= kCallbackTimeout || mResponse.IsComplete())
    // {
    //     otbrLog(OTBR_LOG_ERR, "check callback here" );
    //     // Handle Callback, then Write back response
    //     mResource->HandleCallback(mRequest, mResponse);
    //     error = Write();
    // }

    return error;
}

otbrError Connection::ProcessWaitWrite(fd_set &aWriteFdSet)
{
    otbrError error    = OTBR_ERROR_NONE;
    auto      duration = duration_cast<microseconds>(steady_clock::now() - mStartTime).count();

    if (duration <= kWriteTimeout)
    {
        if (FD_ISSET(mFd, &aWriteFdSet))
        {
            // WriteFdSet is set, then try to write again
            error = Write();
        }
    }
    else
    { // Pass write timeout, just close the error
        Disconnect();
    }
    return error;
}

otbrError Connection::Write(void)
{
    otbrError   error = OTBR_ERROR_NONE;
    std::string errorCode;
    int32_t     sendLength;
    int32_t     err;
    
    if (mState != OTBR_REST_CONNECTION_WRITEWAIT)
    {
        // Change its state when try write for the first time.
        mState        = OTBR_REST_CONNECTION_WRITEWAIT;
        mStartTime    = steady_clock::now();
        mWriteContent = mResponse.Serialize();
    }
    otbrLog(OTBR_LOG_ERR, "request  method %d", mRequest.GetMethod() );
    otbrLog(OTBR_LOG_ERR, "request  %s", mRequest.GetUrl().c_str() );
    otbrLog(OTBR_LOG_ERR, "response %s", mWriteContent.c_str() );

    VerifyOrExit(mWriteContent.size() > 0, error = OTBR_ERROR_REST);

    sendLength = write(mFd, mWriteContent.c_str(), mWriteContent.size());
    err        = errno;

    // Write successfully
    if (sendLength == static_cast<int32_t>(mWriteContent.size()))
    {
        // Normal Exit
        Disconnect();
    }
    else if (sendLength > 0)
    {
        // Partly write
        mWriteContent = mWriteContent.substr(sendLength);
    }
    else
    {
        if (errno == EINTR)
        {
            error = Write();
        }
        else
        {
            VerifyOrExit(err == EAGAIN || err == EWOULDBLOCK, error = OTBR_ERROR_REST);
        }
    }

exit:

    return error;
}

bool Connection::IsComplete()
{
    return mState == OTBR_REST_CONNECTION_COMPLETE;
}

} // namespace rest
} // namespace otbr
