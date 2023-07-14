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

#include "rest/parser.hpp"

#include <string>
#include <vector>

namespace otbr {
namespace rest {

Parser::Parser(Request *aRequest)
{
    mState.mRequest = aRequest;

    mParser.data = &mState;
}

void Parser::Init(void)
{
    mSettings.on_message_begin    = Parser::OnMessageBegin;
    mSettings.on_url              = Parser::OnUrl;
    mSettings.on_status           = Parser::OnHandlerData;
    mSettings.on_header_field     = Parser::OnHeaderField;
    mSettings.on_header_value     = Parser::OnHeaderData;
    mSettings.on_body             = Parser::OnBody;
    mSettings.on_headers_complete = Parser::OnHeaderComplete;
    mSettings.on_message_complete = Parser::OnMessageComplete;
    http_parser_init(&mParser, HTTP_REQUEST);
}

void Parser::Process(const char *aBuf, size_t aLength)
{
    http_parser_execute(&mParser, &mSettings, aBuf, aLength);
}

int Parser::OnUrl(http_parser *parser, const char *at, size_t len)
{
    State *state = reinterpret_cast<State *>(parser->data);

    if (len > 0)
    {
        state->mUrl.append(at, len);
    }

    return 0;
}

int Parser::OnBody(http_parser *parser, const char *at, size_t len)
{
    State *state = reinterpret_cast<State *>(parser->data);

    if (len > 0)
    {
        state->mRequest->SetBody(at, len);
    }

    return 0;
}

int Parser::OnMessageComplete(http_parser *parser)
{
    State *state = reinterpret_cast<State *>(parser->data);

    http_parser_url urlParser;
    http_parser_url_init(&urlParser);
    http_parser_parse_url(state->mUrl.c_str(), state->mUrl.length(), 1, &urlParser);

    if (urlParser.field_set & (1 << UF_PATH))
    {
        std::string path = state->mUrl.substr(urlParser.field_data[UF_PATH].off, urlParser.field_data[UF_PATH].len);
        state->mRequest->SetUrlPath(path);
    }

    if (urlParser.field_set & (1 << UF_QUERY))
    {
        uint16_t offset = urlParser.field_data[UF_QUERY].off;
        uint16_t end = offset + urlParser.field_data[UF_QUERY].len;

        while(offset < end) {
            std::string::size_type next = state->mUrl.find('&', offset);
            if (next == std::string::npos)
            {
                next = end;
            }

            std::string::size_type split = state->mUrl.find('=', offset);
            if (split != std::string::npos && static_cast<uint16_t>(split) < next)
            {
                std::string query = state->mUrl.substr(offset, split - offset);
                std::string value = state->mUrl.substr(split + 1, next - split - 1);

                state->mRequest->AddQueryField(query, value);
            }

            offset = static_cast<uint16_t>(next + 1);
        }
    }

    state->mRequest->SetReadComplete();

    return 0;
}

int Parser::OnMessageBegin(http_parser *parser)
{
    State *state = reinterpret_cast<State *>(parser->data);
    state->mRequest->ResetReadComplete();

    return 0;
}

int Parser::OnHeaderComplete(http_parser *parser)
{
    State *state = reinterpret_cast<State *>(parser->data);
    state->mRequest->SetMethod(parser->method);
    return 0;
}

int Parser::OnHandlerData(http_parser *, const char *, size_t)
{
    return 0;
}

int Parser::OnHeaderField(http_parser *parser, const char *at, size_t len)
{
    State *state = reinterpret_cast<State *>(parser->data);

    if (len > 0)
    {
        state->mNextHeaderField = std::string(at, len);
    }

    return 0;
}

int Parser::OnHeaderData(http_parser *parser, const char *at, size_t len)
{
    State *state = reinterpret_cast<State *>(parser->data);

    if (len > 0)
    {
        state->mRequest->AddHeaderField(state->mNextHeaderField, std::string(at, len));
    }

    return 0;
}

} // namespace rest
} // namespace otbr
