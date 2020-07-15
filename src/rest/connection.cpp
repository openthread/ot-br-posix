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


#include "rest/connection.hpp"

#include <chrono>

using std::chrono::duration_cast;
using std::chrono::microseconds;
using std::chrono::seconds;
using std::chrono::steady_clock;

namespace otbr {
namespace rest {

Connection::Connection(steady_clock::time_point aStartTime, otInstance *aInstance, int aFd)
    : mStartTime(aStartTime)
     , mInstance(aInstance)
    , mReadFlag(1)
    , mFd(aFd)
    
    , mBufLength(2048)
    , mReadLength(0)
{   mReadBuf = std::unique_ptr<char> (new char(mBufLength));
    mParser = std::unique_ptr<http_parser>(new http_parser());
    http_parser_init(mParser.get(), HTTP_REQUEST);
    
} 
otInstance * Connection::GetInstance(){
    return this->mInstance;
}

int Connection::GetCallbackFlag(){
     return this->mCallbackFlag;
}

std::string Connection::GetErrorCode(){
    return this->mErrorCode;
}

int Connection::GetStatus(){
    return this->mEndFlag;
}
void Connection::Check(){
    Request   req;
    Response  res;
    auto duration = duration_cast<microseconds>(steady_clock::now() - this->mStartTime).count();

    // If need to read.
    if (this->mReadFlag)this->NonBlockRead();

    
    // normal entry, create a request and process it  
    if (duration > sTimeout)
    {       
            // parse buf data into request 
            this->CreateRequest(req);
            // find the Handler according to path
            requestHandler Handler = this->GetHandler(req);
            // process the request
            Handler(*this, res);

            
            if(!this->mCallbackFlag && !this->mErrorFlag){
               this->SentResponse(res);
               this->SetEndFlag(1);
            }
    }
    
    // when error occurs
    if (this->mErrorFlag) {
        this->ErrorFormResponse(res);
        this->SentResponse(res); 
        this->SetEndFlag(1);
    }
    else{
        
        if ( this->mCallbackFlag )
        {
            duration = duration_cast<microseconds>(steady_clock::now() - this->mDiagInfo->mStartTime).count();
            if (duration > 4 * sTimeout)
            {     
                this->CallbackFormResponse(res);
                this->SentResponse(res);
                this->SetEndFlag(1);
            }
        }
    }
}

void Connection::NonBlockRead()
{
    
    int received = 0;
   
    while ((received = read(this->mFd, this->mReadBuf.get(), this->mBufLength)) > 0)
    {   
        this->mReadContent += std::string(this->mReadBuf.get(),received);
        this->mReadLength += received;
    }
    auto err = errno;
    if (received == -1 && err != EAGAIN && err!= EWOULDBLOCK  ){
        this->mErrorFlag = 1;
        this->mErrorCode = "error while read";
    }
    
}



void  Connection::CreateRequest(Request & aRequest)
{
   
    mParser->data = &aRequest;
    http_parser_execute(mParser.get(), &mSettings, this->mReadContent.c_str(), this->mReadLength);
}

void Connection::SetEndFlag(int aFlag)
{
    this->mEndFlag = aFlag;
    close(this->mFd);
}
bool Connection::CheckDiag(std::string& aRloc){
    if(!this->mCallbackFlag)return false;
    auto it = this->mDiagInfo->mNodeSet.find(aRloc);
    if (it == this->mDiagInfo->mNodeSet.end()) return false;
    else return true;
}

void  Connection::AddDiag(std::string aRloc , std::string aDiag){
    this->mDiagInfo->mDiagContent.push_back(aDiag);
    this->mDiagInfo->mNodeSet.insert(aRloc);
}

void Connection::SetCallbackFlag(int aFlag){
    this->mCallbackFlag = aFlag;
}
void Connection::SetErrorFlag(int aFlag){
    this->mEndFlag = aFlag;
}

void Connection::ResetDiagInfo(){
    this->mDiagInfo.reset();
    mDiagInfo = std::unique_ptr<DiagInfo>(new DiagInfo(steady_clock::now()));
    
}

void Connection::ErrorFormResponse(Response &aResponse){
    aResponse.SetBody(this->mErrorCode);
}

void Connection::CallbackFormResponse(Response &aResponse){
    std::string str = mJsonFormater.VectorToJson(this->mDiagInfo->mDiagContent);
    aResponse.SetBody(str);
}

void Connection::SetErrorCode(std::string aErrorCode){
    this->mErrorCode = aErrorCode;
}


requestHandler Connection::GetHandler(Request& aRequest)
{
    

    std::string url     = aRequest.getPath();
    auto        Handler = this->mHander.GetHandler(url);
    return Handler;
}

steady_clock::time_point Connection::GetConnectionStartTime(){
    return this->mStartTime;
}

steady_clock::time_point Connection::GetDiagStartTime(){
    return this->mDiagInfo->mStartTime;
}

void Connection::SentResponse(Response& aResponse)
{
    std::string data = aResponse.SerializeResponse();
    write(this->mFd, data.c_str(), data.size());
}

int Connection::OnMessageBegin(http_parser *parser)
{
    Request *request = (Request *)parser->data;
    request->SetComplete(0);
    return 0;
}

int Connection::OnStatus(http_parser *parser, const char *at, size_t len)
{
    Request *request = (Request *)parser->data;
    request->SetStatus(at,len);    
    return 0;
}

int Connection::OnUrl(http_parser *parser, const char *at, size_t len)
{
    Request *request = (Request *)parser->data;
    request->SetPath(at,len);    
    return 0;
}

int Connection::OnHeaderField(http_parser *parser, const char *at, size_t len)
{
    Request *request = (Request *)parser->data;
    request->AddHeaderField (at,len);
    return 0;
}

int Connection::OnHeaderValue(http_parser *parser, const char *at, size_t len)
{
    Request *request = (Request *)parser->data;
    request->AddHeaderValue(at,len);
    return 0;
}
int Connection::OnBody(http_parser *parser, const char *at, size_t len)
{   
    Request *request = (Request *)parser->data;
    request->SetBody(at,len);
    return 0;
}

int Connection::OnHeadersComplete(http_parser *parser)
{
     
    Request *request = (Request *)parser->data;
    request->SetContentLength(parser->content_length);
    request->SetMethod(parser->method);
    return 0;
}

int Connection::OnMessageComplete(http_parser *parser)
{
    Request *request = (Request *)parser->data;
    request->SetComplete(0);
    return 0;
}

int Connection::OnChunkHeader(http_parser *parser)
{
    Request *request = (Request *)parser->data;
    request->SetComplete(0);
    return 0;
}

int Connection::OnChunkComplete(http_parser *parser)
{
    Request *request = (Request *)parser->data;
    request->SetComplete(0);
    return 0;
}

void Connection::SetReadFlag(int aFlag){
    this->mReadFlag = aFlag;
}

}
}