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
 *   This file includes request definition for RESTful HTTP server.
 */

#ifndef OTBR_REST_REQUEST_HPP_
#define OTBR_REST_REQUEST_HPP_

#include <string>
#include <vector>

#include "common/code_utils.hpp"

namespace otbr {
namespace rest {

enum HttpMethod
{
    OTBR_REST_METHOD_DELETE = 0, ///< DELETE
    OTBR_REST_METHOD_GET    = 1, ///< GET
    OTBR_REST_METHOD_HEAD   = 2, ///< HEAD
    OTBR_REST_METHOD_POST   = 3, ///< POST
    OTBR_REST_METHOD_PUT    = 4, ///< PUT

};

enum PostError
{

    OTBR_REST_POST_ERROR_NONE = 0,
    OTBR_REST_POST_BAD_REQUEST= 1, 
    OTBR_REST_POST_SET_FAIL   = 2, 
};



class Request
{
public:
    Request(void);

    void        SetUrl(const char *aString, size_t aLength);
    void        SetBody(const char *aString, size_t aLength);
    void        SetContentLength(size_t aContentLength);
    void        SetMethod(int32_t aMethod);
    int32_t     GetMethod() const;
    std::string  GetBody() const;
    void        SetReadComplete(void);
    void        ResetReadComplete(void);
    std::string GetUrl(void) const ;
    bool        IsComplete(void) const;

private:
    int32_t     mMethod;
    size_t      mContentLength;
    std::string mUrl;
    std::string mBody;
    bool        mComplete;
};

} // namespace rest
} // namespace otbr

#endif // OTBR_REST_REQUEST_HPP_
