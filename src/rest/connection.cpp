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

#include <sys/time.h>

using std::chrono::duration_cast;
using std::chrono::microseconds;
using std::chrono::seconds;
using std::chrono::steady_clock;

namespace otbr {
namespace rest {

// Timeout settings in microseconds
static const uint32_t kCallbackTimeout = 2000000;
static const uint32_t kWriteTimeout    = 10000000;
static const uint32_t kReadTimeout     = 1000000;

Connection::Connection(steady_clock::time_point aStartTime, Resource *aResource, int aFd)
    : mStartTime(aStartTime)
    , mFd(aFd)
    , mState(OTBR_REST_CONNECTION_INIT)
    , mParser(&mRequest)
    , mResource(aResource)
{
}

void Connection::Init()
{
    mParser.Init();
}

void Connection::Disconnect()
{
    mState = OTBR_REST_CONNECTION_COMPLETE;

    if (mFd != -1)
    {
        close(mFd);
        mFd = -1;
    }
}

otbrError Connection::UpdateReadFdSet(fd_set &aReadFdSet, int &aMaxFd)
{
    otbrError error = OTBR_ERROR_NONE;

    if (mState == OTBR_REST_CONNECTION_READWAIT || mState == OTBR_REST_CONNECTION_INIT)
    {
        FD_SET(mFd, &aReadFdSet);
        aMaxFd = aMaxFd < mFd ? mFd : aMaxFd;
    }

    return error;
}

otbrError Connection::UpdateWriteFdSet(fd_set &aWriteFdSet, int &aMaxFd)
{
    otbrError error = OTBR_ERROR_NONE;

    if (mState == OTBR_REST_CONNECTION_WRITEWAIT)
    {
        FD_SET(mFd, &aWriteFdSet);
        aMaxFd = aMaxFd < mFd ? mFd : aMaxFd;
    }

    return error;
}

otbrError Connection::UpdateTimeout(timeval &aTimeout)
{
    otbrError      error = OTBR_ERROR_NONE;
    struct timeval timeout;
    uint32_t       timeoutLen = kReadTimeout;
    auto           duration   = duration_cast<microseconds>(steady_clock::now() - mStartTime).count();

    switch (mState)
    {
    case OTBR_REST_CONNECTION_READWAIT:
        timeoutLen = kReadTimeout;
        break;
    case OTBR_REST_CONNECTION_CALLBACKWAIT:
        timeoutLen = kCallbackTimeout;
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

    return error;
}

otbrError Connection::UpdateFdSet(otSysMainloopContext &aMainloop)
{
    otbrError error = OTBR_ERROR_NONE;

    error = UpdateTimeout(aMainloop.mTimeout);
    error = UpdateReadFdSet(aMainloop.mReadFdSet, aMainloop.mMaxFd);
    error = UpdateWriteFdSet(aMainloop.mWriteFdSet, aMainloop.mMaxFd);

    return error;
}

otbrError Connection::ProcessWaitRead(fd_set &aReadFdSet)
{
    otbrError error = OTBR_ERROR_NONE;
    int       received;
    char      buf[2048];
    int       err;
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

    if (received == 0)
    {
        Disconnect();
    }

    // 1. Catch the error
    // 2. Client side has close this connection(received == 0).
    VerifyOrExit(received >= 0 || (received == -1 && (err == EAGAIN || err == EWOULDBLOCK)),
                 mState = OTBR_REST_CONNECTION_INTERNALERROR, error = OTBR_ERROR_REST);

exit:

    switch (mState)
    {
    case OTBR_REST_CONNECTION_READTIMEOUT:

        mResource->ErrorHandler(mRequest, mResponse, 408);
        error = Write();
        break;

    case OTBR_REST_CONNECTION_INTERNALERROR:

        mResource->ErrorHandler(mRequest, mResponse, 500);
        error = Write();
        break;

    default:
        break;
    }
    if (error == OTBR_ERROR_REST)
    {
        Disconnect();
    }

    return error;
}

otbrError Connection::Handle()
{
    otbrError error = OTBR_ERROR_NONE;

    shutdown(mFd, SHUT_RD);

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

    return error;
}

otbrError Connection::ProcessWaitCallback()
{
    otbrError error    = OTBR_ERROR_NONE;
    auto      duration = duration_cast<microseconds>(steady_clock::now() - mStartTime).count();

    if (duration >= kCallbackTimeout)
    {
        // Handle Callback, then Write back response
        mResource->HandleCallback(mRequest, mResponse);
        error = Write();
    }

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

otbrError Connection::Process(fd_set &aReadFdSet, fd_set &aWriteFdSet)
{
    otbrError error = OTBR_ERROR_NONE;

    switch (mState)
    {
    // Initial state, directly read for the first time.
    case OTBR_REST_CONNECTION_INIT:
    {
        error = ProcessWaitRead(aReadFdSet);
        break;
    }
    case OTBR_REST_CONNECTION_READWAIT:
    {
        error = ProcessWaitRead(aReadFdSet);
        break;
    }
    case OTBR_REST_CONNECTION_CALLBACKWAIT:
    {
        //  Wait for Callback process.
        error = ProcessWaitCallback();
        break;
    }
    case OTBR_REST_CONNECTION_WRITEWAIT:
    {
        error = ProcessWaitWrite(aWriteFdSet);
        break;
    }
    default:
        break;
    }

    return error;
}

otbrError Connection::Write()
{
    otbrError   error = OTBR_ERROR_NONE;
    std::string errorCode;
    int32_t     sendLength;
    int         err;

    if (mState != OTBR_REST_CONNECTION_WRITEWAIT)
    {
        // Change its state when try write for the first time.
        mState        = OTBR_REST_CONNECTION_WRITEWAIT;
        mStartTime    = steady_clock::now();
        mWriteContent = mResponse.Serialize();
    }

    if (mWriteContent.size() == 0)
    {
        Disconnect();
    }

    sendLength = write(mFd, mWriteContent.c_str(), mWriteContent.size());
    err        = errno;

    // Write successfully
    if (sendLength == static_cast<int32_t>(mWriteContent.size()))
    {
        Disconnect();
    }
    else if (sendLength >= 0)
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

    if (error != OTBR_ERROR_NONE)
    {
        Disconnect();
    }
    return error;
}

bool Connection::WaitRelease()
{
    return mState == OTBR_REST_CONNECTION_COMPLETE;
}

} // namespace rest
} // namespace otbr
