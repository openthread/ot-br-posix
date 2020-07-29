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

#include "rest/response.hpp"

#include <stdio.h>

namespace otbr {
namespace rest {

Response::Response()
    : mCallback(false)
{
    mProtocol = "HTTP/1.1";
    mCode     = "200 OK";
    mHeaderField.push_back("Content-Type");
    mHeaderValue.push_back("application/json");

    mHeaderField.push_back("Access-Control-Allow-Origin");
    mHeaderValue.push_back("*");
}

void Response::SetResponsCode(std::string aCode)
{
    mCode = aCode;
}

void Response::SetCallback()
{
    mCallback = true;
}

void Response::SetBody(std::string aBody)
{
    mBody = aBody;
}

std::string Response::GetBody()
{
    return mBody;
}

bool Response::NeedCallback()
{
    return mCallback;
}

std::string Response::Serialize()
{
    char          contentLength[10];
    unsigned long index;
    std::string   spacer = "\r\n";
    std::string   ret(mProtocol + " " + mCode);
    for (index = 0; index < mHeaderField.size(); index++)
    {
        ret += (spacer + mHeaderField[index] + ": " + mHeaderValue[index]);
    }

    snprintf(contentLength, 9, "%d", static_cast<int>(mBody.size()));

    ret += spacer + "Content-Length: " + std::string(contentLength);

    ret += (spacer + spacer + mBody);

    return ret;
}

} // namespace rest
} // namespace otbr
