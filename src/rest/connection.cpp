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

using std::chrono::duration_cast;
using std::chrono::microseconds;
using std::chrono::seconds;
using std::chrono::steady_clock;

namespace otbr {
namespace rest {

Connection::Connection(steady_clock::time_point aStartTime, int aFd)
    : mStartTime(aStartTime)
    , mComplete(false);
    , mReadFlag(1)
    , mFd(aFd)
    , mBufLength(2048)
    , mReadLength(0)
{
   http_parser_init(&mParser, HTTP_REQUEST);
}

void Connection::Process()
{
    Request  req;
    Response res;
    
    Parse(req);
    HandleRequest(req,res);
    ProcessResponse(res);

}

void Connection::CallBackProcess()
{   Response res;
    
    HandleCallback(res);
    ProcessResponse(res);
    
}

void Connection::Read()
{
    int received = 0;

    while ((received = read(mFd, mReadBuf, mBufLength)) > 0)
    {
        mReadContent += std::string(mReadBuf, received);
        mReadLength += received;
    }
    auto err = errno;

    // error handler
}

void Connection::Parse(Request &aRequest)
{
    mParser.data = &aRequest;
    http_parser_execute(&mParser, &mSettings, mReadContent.c_str(), mReadLength);

    //error handler
}

void  Connection::HandleRequest(Request &aRequest, Response &aResponse)
{
    mResource.Handle(aRequest, aResponse);
    //error handler
}

void  Connection::HandleCallback(Request &aRequest, Response &aResponse)
{
    mResource.HandleCallback(aRequest, aResponse);
    //error handler
}



void Connection::ProcessResponse(Response &aResponse)
{   
    if aResponse.NeedCallback(){
        mHasCallback = true;
        mStartTime = steady_clock::now();
    }
    else Write(aResponse);
}

void Connection::Write(Response &aResponse){
    
    mComplete = true;
    std::string data = aResponse.Serialize();
    write(mFd, data.c_str(), data.size());
    //error handler
    mComplete = true;
}


steady_clock::time_point Connection::GetStartTime()
{
    return mStartTime;
}

bool Connection::Iscomplete()
{   
    return mComplete;
}

bool Connection::IsCallbackTimeout(int aDuration)
{   
   if (!mHasCallback) return false;
   auto duration = duration_cast<microseconds>(steady_clock::now() - mStartTime).count();
   return duration >= aDuration ? true : false;
}

void Connection::SetComplete(int aFlag)
{
    mComplete = true;
    close(mFd);
}

bool Connection::HasCallback(){
    return mHasCallback;
}

bool Connection::WaitRead(){
    return (!mHasCallback && !IsReadTimeout());
}

bool Connection::IsReadTimeout(int aDuration)
{   if(mHasCallback) return false;
    auto duration = duration_cast<microseconds>(steady_clock::now() - mStartTime).count();
    return duration >= aDuration ? true : false;
}

int Connection::OnMessageBegin(http_parser *parser)
{
    OT_UNUSED_VARIABLE(parser);
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
    request->SetUrlPath(at, len);
    return 0;
}

int Connection::OnHeaderField(http_parser *parser, const char *at, size_t len)
{
    Request *request = reinterpret_cast<Request *>(parser->data);
    request->AddHeaderField(at, len);
    return 0;
}

int Connection::OnHeaderValue(http_parser *parser, const char *at, size_t len)
{
    Request *request = reinterpret_cast<Request *>(parser->data);
    request->AddHeaderValue(at, len);
    return 0;
}
int Connection::OnBody(http_parser *parser, const char *at, size_t len)
{
    Request *request = reinterpret_cast<Request *>(parser->data);
    request->SetBody(at, len);
    return 0;
}

int Connection::OnHeadersComplete(http_parser *parser)
{
    Request *request = reinterpret_cast<Request *>(parser->data);
    request->SetContentLength(parser->content_length);
    request->SetMethod(parser->method);
    return 0;
}

int Connection::OnMessageComplete(http_parser *parser)
{   
    OT_UNUSED_VARIABLE(parser);
    return 0;
}

int Connection::OnChunkHeader(http_parser *parser)
{
    OT_UNUSED_VARIABLE(parser);
    return 0;
}

int Connection::OnChunkComplete(http_parser *parser)
{
    OT_UNUSED_VARIABLE(parser);
    return 0;
}



} // namespace rest
} // namespace otbr
