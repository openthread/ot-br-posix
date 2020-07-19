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
 *   This file includes connection definition for RESTful HTTP server.
 */

#ifndef OTBR_REST_CONNECTION_HPP_
#define OTBR_REST_CONNECTION_HPP_

#include <chrono>
#include <unordered_map>
#include <unordered_set>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "http_parser.h"

#include "rest/request.hpp"
#include "rest/resource.hpp"
#include "rest/response.hpp"

using std::chrono::steady_clock;

namespace otbr {
namespace rest {

class Connection
{
public:
    Connection(steady_clock::time_point aStartTime, Resource *aResource, int aFd);

    void Process();

    void CallbackProcess();

    void Read();

    otInstance *GetInstance();

    steady_clock::time_point GetStartTime();

    bool IsComplete();

    bool IsReadTimeout(int aDuration);

    bool IsCallbackTimeout(int aDuration);

    bool HasCallback();

    bool Readable(int aDuration);

private:
    void Parse(Request &aRequest);

    void HandleRequest(Request &aRequest, Response &aResponse);

    void HandleCallback(Request &aRequest, Response &aResponse);

    void ProcessResponse(Response &aResponse);

    void Write(Response &aResponse);

    void SetComplete();

    steady_clock::time_point mStartTime;
    bool                     mComplete;
    bool                     mHasCallback;
    int                      mFd;

    // read parameter
    std::string mReadContent;
    char        mReadBuf[2048];

    int mReadLength;

    http_parser mParser;
    Resource *  mResource;

    static int OnMessageBegin(http_parser *parser);
    static int OnStatus(http_parser *parser, const char *at, size_t len);
    static int OnUrl(http_parser *parser, const char *at, size_t len);
    static int OnHeaderField(http_parser *parser, const char *at, size_t len);
    static int OnHeaderValue(http_parser *parser, const char *at, size_t len);
    static int OnBody(http_parser *parser, const char *at, size_t len);
    static int OnHeadersComplete(http_parser *parser);
    static int OnMessageComplete(http_parser *parser);
    static int OnChunkHeader(http_parser *parser);
    static int OnChunkComplete(http_parser *parser);

    http_parser_settings mSettings{.on_message_begin    = OnMessageBegin,
                                   .on_url              = OnUrl,
                                   .on_status           = OnStatus,
                                   .on_header_field     = OnHeaderField,
                                   .on_header_value     = OnHeaderValue,
                                   .on_headers_complete = OnHeadersComplete,
                                   .on_body             = OnBody,
                                   .on_message_complete = OnMessageComplete,
                                   .on_chunk_header     = OnChunkHeader,
                                   .on_chunk_complete   = OnChunkComplete};
};

} // namespace rest
} // namespace otbr

#endif
