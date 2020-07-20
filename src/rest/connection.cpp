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

#include <chrono>

#include <sys/time.h>

using std::chrono::duration_cast;
using std::chrono::microseconds;
using std::chrono::seconds;
using std::chrono::steady_clock;

namespace otbr {
namespace rest {

const uint32_t Connection::kCallbackTimeout = 4000000;
const uint32_t Connection::kWriteTimeout    = 10000000;
const uint32_t Connection::kReadTimeout     = 1000000;

Connection::Connection(steady_clock::time_point aStartTime, Resource *aResource, int aFd)

    : mStartTime(aStartTime)
    , mFd(aFd)
    , mState(OTBR_REST_CONNECTION_INIT)
    , mWritePointer(0)
    , mResource(aResource)
{
}

void Connection::Init()
{
    mSettings.on_message_begin = OnMessageBegin;
    mSettings.on_url           = OnUrl;
    mSettings.on_status        = OnStatus;
    mSettings.on_body = OnBody, mSettings.on_message_complete = OnMessageComplete,
    http_parser_init(&mParser, HTTP_REQUEST);
    mParser.data = &mRequest;
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
        timeoutLen = kWriteTimeout;
        break;
    case OTBR_REST_CONNECTION_WRITEWAIT:
        timeoutLen = kCallbackTimeout;
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
    error           = UpdateTimeout(aMainloop.mTimeout);
    error           = UpdateReadFdSet(aMainloop.mReadFdSet, aMainloop.mMaxFd);
    error           = UpdateWriteFdSet(aMainloop.mWriteFdSet, aMainloop.mMaxFd);
    return error;
}

otbrError Connection::ProcessWaitRead()
{
    otbrError error    = OTBR_ERROR_NONE;
    auto      duration = duration_cast<microseconds>(steady_clock::now() - mStartTime).count();
    if (mState == OTBR_REST_CONNECTION_INIT)
    {
        mState = OTBR_REST_CONNECTION_READWAIT;
    }
    if (duration <= kReadTimeout)
    {
        int  received;
        char buf[4096];
        auto err = errno;
        while (true)
        {
            received = read(mFd, buf, sizeof(buf));
            err      = errno;
            if (received <= 0)
                break;
            http_parser_execute(&mParser, &mSettings, buf, received);
            if (mRequest.IsComplete())
            {
                mResource->Handle(mRequest, mResponse);
                ProcessResponse();
                break;
            }
        }

        if (received == 0)
        {
            Disconnect();
        }
        else if (err == EINTR)
        {
            ProcessWaitRead();
        }
        else
        {
            VerifyOrExit(err == EAGAIN || err == EWOULDBLOCK, error = OTBR_ERROR_REST);
        }
    }
    else
    {
        Disconnect();
    }

exit:
    if (error == OTBR_ERROR_REST)
    {
        otbrLog(OTBR_LOG_ERR, "otbr rest read error");
        Disconnect();
    }
    return error;
}

otbrError Connection::ProcessWaitCallback()
{
    otbrError error    = OTBR_ERROR_NONE;
    auto      duration = duration_cast<microseconds>(steady_clock::now() - mStartTime).count();
    if (duration >= kCallbackTimeout)
    {
        mResource->HandleCallback(mRequest, mResponse);
        Write();
    }
    return error;
}

otbrError Connection::ProcessWaitWrite()
{
    otbrError error    = OTBR_ERROR_NONE;
    auto      duration = duration_cast<microseconds>(steady_clock::now() - mStartTime).count();
    if (duration <= kWriteTimeout)
    {
        Write();
    }
    else
    {
        Disconnect();
    }
    return error;
}

otbrError Connection::Process(fd_set &aReadFdSet, fd_set &aWriteFdSet)
{
    otbrError error = OTBR_ERROR_NONE;
    switch (mState)
    {
    case OTBR_REST_CONNECTION_INIT:
    {
        error = ProcessWaitRead();
        break;
    }
    case OTBR_REST_CONNECTION_READWAIT:
    {
        if (FD_ISSET(mFd, &aReadFdSet))
        {
            error = ProcessWaitRead();
        }
        break;
    }
    case OTBR_REST_CONNECTION_CALLBACKWAIT:
    {
        error = ProcessWaitCallback();
        break;
    }
    case OTBR_REST_CONNECTION_WRITEWAIT:
    {
        if (FD_ISSET(mFd, &aWriteFdSet))
        {
            error = ProcessWaitWrite();
        }

        break;
    }
    default:
        break;
    }

    return error;
}

otbrError Connection::ProcessResponse()
{
    otbrError error = OTBR_ERROR_NONE;

    if (mResponse.NeedCallback() && mState == OTBR_REST_CONNECTION_READWAIT)
    {
        mState     = OTBR_REST_CONNECTION_CALLBACKWAIT;
        mStartTime = steady_clock::now();
    }
    else
    {
        error = Write();
    }
    return error;
}

otbrError Connection::Write()
{
    otbrError   error = OTBR_ERROR_NONE;
    std::string errorCode;
    int         send;
    auto        err = errno;
    if (mState == OTBR_REST_CONNECTION_READWAIT || mState == OTBR_REST_CONNECTION_CALLBACKWAIT)
    {
        mWriteContent = mResponse.Serialize();
        mState        = OTBR_REST_CONNECTION_WRITEWAIT;
        mStartTime    = steady_clock::now();
    }

    if (mWriteContent.size() == mWritePointer)
    {
        Disconnect();
    }

    send = write(mFd, mWriteContent.substr(mWritePointer).c_str(), mWriteContent.size() - mWritePointer);

    err = errno;

    if (send >= 0)
    {
        mWritePointer += send;
        if (mWriteContent.size() == mWritePointer)
        {
            Disconnect();
        }
    }
    else
    {
        VerifyOrExit(err == EAGAIN || err == EWOULDBLOCK, error = OTBR_ERROR_REST; errorCode = "write error");
    }

exit:

    if (error == OTBR_ERROR_REST)
    {
        otbrLog(OTBR_LOG_ERR, "rest server error: %s", errorCode.c_str());
        Disconnect();
    }
    return error;
}

bool Connection::IsComplete()
{
    return mState == OTBR_REST_CONNECTION_COMPLETE;
}

int Connection::OnMessageBegin(http_parser *parser)
{
    Request *request = reinterpret_cast<Request *>(parser->data);
    request->ResetReadComplete();
    return 0;
}

int Connection::OnStatus(http_parser *parser, const char *at, size_t len)
{
    Request *request = reinterpret_cast<Request *>(parser->data);
    request->SetStatus(at, len);
    return 0;
}

int Connection::OnUrl(http_parser *parser, const char *at, size_t len)
{
    Request *request = reinterpret_cast<Request *>(parser->data);
    request->SetUrl(at, len);
    return 0;
}

int Connection::OnBody(http_parser *parser, const char *at, size_t len)
{
    Request *request = reinterpret_cast<Request *>(parser->data);
    request->SetBody(at, len);
    return 0;
}

int Connection::OnMessageComplete(http_parser *parser)
{
    Request *request = reinterpret_cast<Request *>(parser->data);
    request->SetReadComplete();

    return 0;
}

} // namespace rest
} // namespace otbr
