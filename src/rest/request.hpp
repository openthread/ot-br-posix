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

#include "openthread-br/config.h"

#include <map>
#include <string>

#include "common/code_utils.hpp"
#include "rest/types.hpp"

namespace otbr {
namespace rest {

/**
 * This class implements an instance to host services used by border router.
 */
class Request
{
public:
    /**
     * The constructor is to initialize Request instance.
     */
    Request(void);

    /**
     * This method sets the Url Path field of a request.
     *
     * @param[in] aPath  The url path
     *
     */
    void SetUrlPath(std::string aPath);

    /**
     * This method sets the body field of a request.
     *
     * @param[in] aString  A pointer points to body string.
     * @param[in] aLength  Length of the body string
     */
    void SetBody(const char *aString, size_t aLength);

    /**
     * This method sets the content-length field of a request.
     *
     * @param[in] aContentLength  An unsigned integer representing content-length.
     */
    void SetContentLength(size_t aContentLength);

    /**
     * This method sets the method of the parsed request.
     *
     * @param[in] aMethod  An integer representing request method.
     */
    void SetMethod(int32_t aMethod);

    /**
     * This method sets the next header field of a request.
     *
     * @param[in] aField  The field name.
     * @param[in] aValue  The value of the field.
     *
     */
    void AddHeaderField(std::string aField, std::string aValue);

    /**
     * This method adds a query field to the request.
     *
     * @param[in] aField  The field name.
     * @param[in] aValue  The value of the field.
     */
    void AddQueryField(std::string aField, std::string aValue);

    /**
     * This method labels the request as complete which means it no longer need to be parsed one more time .
     */
    void SetReadComplete(void);

    /**
     * This method resets the request then it could be set by parser from start.
     */
    void ResetReadComplete(void);

    /**
     * This method returns the HTTP method of this request.
     *
     * @returns A integer representing HTTP method.
     */
    HttpMethod GetMethod() const;

    /**
     * This method returns the HTTP method of this request.
     *
     * @returns A HttpMethod enum representing HTTP method of this request.
     */
    std::string GetBody() const;

    /**
     * This method returns the url for this request.
     *
     * @returns A string contains the url of this request.
     */
    std::string GetUrlPath(void) const;

    /**
     * This method returns the specified header field for this request.
     *
     * @param[in] aHeaderField  A header field.
     * @returns A string contains the header value of this request.
     */
    std::string GetHeaderValue(const std::string aHeaderField) const;

    /**
     * This method returns a boolean describing the presence of the specified query name in this request.
     *
     * @param aQueryName  A query name.
     * @return True if the query name is found or False if the query name could not be found.
     */
    bool HasQuery(const std::string aQueryName) const;

    /**
     * This method returns the specified query parameter for this request.
     *
     * @param aQueryName  A query name.
     * @return A string containing the value of the query or an empty string if the query could not be found.
     */
    std::string GetQueryParameter(const std::string aQueryName) const;

    /**
     * This method indicates whether this request is parsed completely.
     */
    bool IsComplete(void) const;

private:
    int32_t                            mMethod;
    size_t                             mContentLength;
    std::string                        mUrlPath;
    std::string                        mBody;
    std::map<std::string, std::string> mHeaders;
    std::map<std::string, std::string> mQueryParameters;
    bool                               mComplete;
};

} // namespace rest
} // namespace otbr

#endif // OTBR_REST_REQUEST_HPP_
