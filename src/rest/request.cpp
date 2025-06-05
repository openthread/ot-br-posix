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

#include "rest/request.hpp"
#include "utils/string_utils.hpp"

namespace otbr {
namespace rest {

Request::Request(void)
    : mComplete(false)
{
}

void Request::SetUrlPath(std::string aPath)
{
    mUrlPath = aPath;
}

void Request::SetBody(const char *aString, size_t aLength)
{
    mBody += std::string(aString, aLength);
}

void Request::SetContentLength(size_t aContentLength)
{
    mContentLength = aContentLength;
}

void Request::SetMethod(int32_t aMethod)
{
    mMethod = aMethod;
}

void Request::AddHeaderField(std::string aField, std::string aValue)
{
    mHeaders[aField] = aValue;
}

void Request::AddQueryField(std::string aField, std::string aValue)
{
    mQueryParameters[aField] = aValue;
}

HttpMethod Request::GetMethod() const
{
    return static_cast<HttpMethod>(mMethod);
}

std::string Request::GetBody() const
{
    return mBody;
}

std::string Request::GetUrlPath(void) const
{
    return mUrlPath;
}

std::string Request::GetHeaderValue(const std::string aHeaderField) const
{
    auto it = mHeaders.find(StringUtils::ToLowercase(aHeaderField));

    return (it == mHeaders.end()) ? "" : it->second;
}

std::string Request::GetQueryParameter(const std::string aQueryName) const
{
    auto it = mQueryParameters.find(aQueryName);

    return (it == mQueryParameters.end()) ? "" : it->second;
}

bool Request::HasQuery(const std::string aQueryName) const
{
    auto it = mQueryParameters.find(aQueryName);

    return (it == mQueryParameters.end()) ? false : true;
}

void Request::SetReadComplete(void)
{
    mComplete = true;
}

void Request::ResetReadComplete(void)
{
    mComplete = false;
}

bool Request::IsComplete(void) const
{
    return mComplete;
}

} // namespace rest
} // namespace otbr
