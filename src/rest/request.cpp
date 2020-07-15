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

#include "rest/request.hpp"

namespace otbr {
namespace rest {

void Request::SetComplete(int aCompleted)
{
    mCompleted = aCompleted;
}
void Request::SetPath(const char *aString, int aLength)
{
    mPath.assign(aString, aLength);
}
void Request::SetStatus(const char *aString, int aLength)
{
    mStatus.assign(aString, aLength);
}
void Request::SetBody(const char *aString, int aLength)
{
    mBody.assign(aString, aLength);
}

void Request::AddHeaderField(const char *aString, int aLength)
{
    mHeaderField.push_back(std::string(aString, aLength));
}
void Request::AddHeaderValue(const char *aString, int aLength)
{
    mHeaderValue.push_back(std::string(aString, aLength));
}

void Request::SetContentLength(int aContentLength)
{
    mContentLength = aContentLength;
}
void Request::SetMethod(int aMethod)
{
    mMethod = aMethod;
}
std::string Request::getPath()
{
    return this->mPath;
}

} // namespace rest
} // namespace otbr
