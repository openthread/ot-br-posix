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

#include "agent/rest_web_server.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "common/logging.hpp"
#include "openthread/thread_ftd.h"

using std::chrono::duration_cast;
using std::chrono::microseconds;
using std::chrono::seconds;
using std::chrono::steady_clock;
namespace otbr {
namespace agent {

static int OnMessageBegin(http_parser *parser)
{
    Connection *connection = (struct Connection *)parser->data;
    connection->mCompleted = 0;
    return 0;
}

static int OnStatus(http_parser *parser, const char *at, size_t len)
{
    Connection *connection = (struct Connection *)parser->data;
    connection->mStatus    = new char[len + 1]();
    strncpy(connection->mStatus, at, len);
    return 0;
}

static int OnUrl(http_parser *parser, const char *at, size_t len)
{
    Connection *connection = (struct Connection *)parser->data;
    connection->mPath      = new char[len + 1]();
    strncpy(connection->mPath, at, len);
    return 0;
}

static int OnHeaderField(http_parser *parser, const char *at, size_t len)
{
    Connection *connection   = (struct Connection *)parser->data;
    connection->mHeaderField = new char[len + 1]();

    strncpy(connection->mHeaderField, at, len);

    return 0;
}

static int OnHeaderValue(http_parser *parser, const char *at, size_t len)
{
    Connection *connection   = (struct Connection *)parser->data;
    connection->mHeaderValue = new char[len + 1]();

    strncpy(connection->mHeaderValue, at, len);

    return 0;
}
static int OnBody(http_parser *parser, const char *at, size_t len)
{
    Connection *connection = (struct Connection *)parser->data;
    connection->mBody      = new char[len + 1]();
    strncpy(connection->mBody, at, len - 1);

    return 0;
}

static int OnHeadersComplete(http_parser *parser)
{
    Connection *connection     = (struct Connection *)parser->data;
    connection->mContentLength = parser->content_length;
    connection->mMethod        = parser->method;
    return 0;
}

static int OnMessageComplete(http_parser *parser)
{
    Connection *connection = (struct Connection *)parser->data;
    connection->mCompleted = 0;
    return 0;
}

static int OnChunkHeader(http_parser *parser)
{
    Connection *connection = (struct Connection *)parser->data;
    connection->mCompleted = 0;
    return 0;
}

static int OnChunkComplete(http_parser *parser)
{
    Connection *connection = (struct Connection *)parser->data;
    connection->mCompleted = 0;
    return 0;
}

static int SetNonBlocking(int fd)
{
    int oldflags;

    oldflags = fcntl(fd, F_GETFL);

    if (fcntl(fd, F_SETFL, oldflags | O_NONBLOCK) == -1)
    {
        return -1;
    }

    return 0;
}

static http_parser_settings sSettings{.on_message_begin    = OnMessageBegin,
                                      .on_url              = OnUrl,
                                      .on_status           = OnStatus,
                                      .on_header_field     = OnHeaderField,
                                      .on_header_value     = OnHeaderValue,
                                      .on_headers_complete = OnHeadersComplete,
                                      .on_body             = OnBody,
                                      .on_message_complete = OnMessageComplete,
                                      .on_chunk_header     = OnChunkHeader,
                                      .on_chunk_complete   = OnChunkComplete};

Connection::Connection(steady_clock::time_point aStartTime, HandlerMap *aHandlerMap, otInstance *aInstance)
    : mStartTime(aStartTime)
    , mHandlerMap(aHandlerMap)
    , mInstance(aInstance)
    , mRequested(0)
    , mError(0)
    , mCallback(0)
    , mCompleted(0)
    , mPath(NULL)
    , mStatus(NULL)
    , mBody(NULL)
    , mHeaderField(NULL)
    , mHeaderValue(NULL)
    , mReadBuf(NULL)
    , mReadLength(0)
    , mBufRemain(8192)
{
}

RestWebServer::RestWebServer(otbr::Ncp::ControllerOpenThread *aNcp)
    : mNcp(aNcp)
    , mAddress(new sockaddr_in())

{
}

void RestWebServer::Init()
{
    mThreadHelper = mNcp->GetThreadHelper();
    mInstance     = mThreadHelper->GetInstance();
    mHandler      = new Handler(mHandlerMap);

    mAddress->sin_family      = AF_INET;
    mAddress->sin_addr.s_addr = INADDR_ANY;
    mAddress->sin_port        = htons(80);

    if ((mListenFd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        otbrLog(OTBR_LOG_ERR, "socket error");
    }

    if ((bind(mListenFd, (struct sockaddr *)mAddress, sizeof(sockaddr))) != 0)
    {
        otbrLog(OTBR_LOG_ERR, "bind error");
    }

    if (listen(mListenFd, 5) < 0)
    {
        otbrLog(OTBR_LOG_ERR, "listen error");
    }

    SetNonBlocking(mListenFd);
}

Handler::Handler(HandlerMap &aHandlerMap)
{
    aHandlerMap.emplace("/diagnostics", &DiagnosticRequestHandler);
    aHandlerMap.emplace("/node", &GetJsonNodeInfo);
    aHandlerMap.emplace("/node/state", &GetJsonState);
    aHandlerMap.emplace("/node/ext-address", &GetJsonExtendedAddr);
    aHandlerMap.emplace("/node/network-name", &GetJsonNetworkName);
    aHandlerMap.emplace("/node/rloc16", &GetJsonRloc16);
    aHandlerMap.emplace("/node/leader-data", &GetJsonLeaderData);
    aHandlerMap.emplace("/node/num-of-route", &GetJsonNumOfRoute);
    aHandlerMap.emplace("/node/ext-panid", &GetJsonExtendedPanId);
    aHandlerMap.emplace("/node/rloc", &GetJsonRloc);
}

void RestWebServer::UpdateFdSet(fd_set &aReadFdSet, int &aMaxFd, timeval &aTimeout)
{
    Connection *   connection;
    struct timeval timeout = {aTimeout.tv_sec, aTimeout.tv_usec};
    otThreadSetReceiveDiagnosticGetCallback(mInstance, &RestWebServer::DiagnosticResponseHandler, this);

    FD_SET(mListenFd, &aReadFdSet);
    if (aMaxFd < mListenFd)
    {
        aMaxFd = mListenFd;
    }

    for (auto it = mConnectionSet.begin(); it != mConnectionSet.end(); ++it)
    {
        connection    = it->second;
        auto duration = duration_cast<microseconds>(steady_clock::now() - connection->mStartTime).count();

        if (duration <= sTimeout)
        {
            timeout.tv_sec = 0;
            if (timeout.tv_usec == 0)
            {
                timeout.tv_usec = std::max(0, sTimeout - (int)duration);
            }
            else
            {
                timeout.tv_usec = std::min(int(timeout.tv_usec), std::max(0, sTimeout - (int)duration));
            }

            FD_SET(it->first, &aReadFdSet);
            if (aMaxFd < it->first)
            {
                aMaxFd = it->first;
            }
        }
        else
        {
            timeout.tv_sec  = 0;
            timeout.tv_usec = 0;
        }
        if (connection->mRequested)
        {
            duration = duration_cast<microseconds>(steady_clock::now() - connection->mDiagInfo->mStartTime).count();

            if (duration <= sTimeout * 4)
            {
                timeout.tv_sec = 0;
                if (timeout.tv_usec == 0)
                {
                    timeout.tv_usec = std::max(0, 4 * sTimeout - (int)duration);
                }
                else
                {
                    timeout.tv_usec = std::min(int(timeout.tv_usec), std::max(0, 4 * sTimeout - (int)duration));
                }
            }
            else
            {
                timeout.tv_sec  = 0;
                timeout.tv_usec = 0;
            }
        }
    }

    if (timercmp(&timeout, &aTimeout, <))
    {
        aTimeout = timeout;
    }
}

void RestWebServer::Process(fd_set &aReadFdSet)
{
    Connection *connection;
    socklen_t   addrlen = sizeof(sockaddr);
    ;
    int  fd;
    auto err = errno;

    for (auto it = mConnectionSet.begin(); it != mConnectionSet.end(); ++it)
    {
        connection    = it->second;
        auto duration = duration_cast<microseconds>(steady_clock::now() - connection->mStartTime).count();

        if (duration > sTimeout && connection->mCallback == 0)
        {
            connection->ServeRequest();
        }

        else
        {
            if (FD_ISSET(it->first, &aReadFdSet))
                connection->NonBlockRead();
        }
        if (connection->mRequested)
        {
            duration = duration_cast<microseconds>(steady_clock::now() - connection->mDiagInfo->mStartTime).count();
            if (duration > 4 * sTimeout)
            {
                if (connection->mDiagInfo->mNodeSet.size() == 0)
                    connection->mError = 1;
                connection->SentResponse(connection->mDiagInfo->mDiagJson);
                connection->FreeConnection();
            }
        }
    }
    auto eraseIt = mConnectionSet.begin();
    for (eraseIt = mConnectionSet.begin(); eraseIt != mConnectionSet.end();)
    {
        connection = eraseIt->second;

        if (connection->mCompleted == 1)
        {
            delete (connection);
            eraseIt = mConnectionSet.erase(eraseIt);
        }
        else
            eraseIt++;
    }

    if (FD_ISSET(mListenFd, &aReadFdSet))
    {
        while (true)
        {
            fd  = accept(mListenFd, (struct sockaddr *)mAddress, &addrlen);
            err = errno;
            if (fd < 0)
                break;

            if (mConnectionSet.size() < (unsigned)mMaxServeNum)
            {
                // set up new connection
                SetNonBlocking(fd);
                mConnectionSet[fd]      = new Connection(steady_clock::now(), &mHandlerMap, mInstance);
                mConnectionSet[fd]->mFd = fd;
            }
            else
            {
                otbrLog(OTBR_LOG_ERR, "server is busy");
            }
        }

        if (err == EWOULDBLOCK)
        {
            otbrLog(OTBR_LOG_ERR, "Having accept all connections");
        }
        else
        {
            otbrLog(OTBR_LOG_ERR, "Accept error, should havndle it");
        }
    }
}

cJSON *Connection::GetHandler()
{
    cJSON *ret = NULL;

    std::string url     = this->mPath;
    auto        Handler = this->mHandlerMap->find(url);
    if (Handler == this->mHandlerMap->end())
    {
        this->mError = 1;
    }
    else
    {
        ret = (*Handler->second)(this, this->mInstance);
    }
    return ret;
}

void Connection::ParseUri()
{
    char *querymStartTime = NULL;
    int   pc              = 0;
    char *tok;
    char *otok;
    char *querystring;
    char *p;
    for (querymStartTime = this->mPath; querymStartTime < this->mPath + strlen(this->mPath); querymStartTime++)
        if (*querymStartTime == '?')
        {
            break;
        }

    if (querymStartTime != this->mPath + strlen(this->mPath))
    {
        p = new char[strlen(this->mPath) + 1]();
        strncpy(p, this->mPath, querymStartTime - this->mPath + 1);
        delete (this->mPath);
        this->mPath = p;
        querymStartTime++;
        querystring = new char[strlen(this->mPath) + 1]();
        strcpy(querystring, querymStartTime);
        for (tok = strtok(querystring, "&"); tok != NULL; tok = strtok(tok, "&"))
        {
            pc++;
            otok = tok + strlen(tok) + 1;
            tok  = strtok(tok, "=");
            tok  = strtok(NULL, "=");
            tok  = otok;
        };
        delete (p);
    }
}

void Connection::NonBlockRead()
{
    if (!this->mReadBuf)
    {
        this->mReadBuf     = new char[this->mBufRemain + 1]();
        this->mReadPointer = this->mReadBuf;
    }
    int received = 0;
    int count    = 0;
    while ((received = read(this->mFd, this->mReadPointer, this->mBufRemain - this->mReadLength)) > 0)
    {
        this->mReadLength += received;
        this->mBufRemain -= received;
        this->mReadPointer += received;
    }
    if (received == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
    {
        otbrLog(OTBR_LOG_ERR, "normal read exit, no data available now");
    }
    else
    {
        if (received == 0)
            count++;
        else
            otbrLog(OTBR_LOG_ERR, "error occur, should handle( to do)");
    }
}

void Connection::FreeConnection()
{
    this->mCompleted = 1;

    close(this->mFd);
}

void Connection::ServeRequest()
{
    cJSON *data = NULL;

    this->mParser       = new http_parser();
    this->mParser->data = this;
    http_parser_init(this->mParser, HTTP_REQUEST);
    if (this->mReadLength == 0)
    {
        this->mError = 1;
    }
    else
    {
        // parse
        http_parser_execute(this->mParser, &sSettings, this->mReadBuf, strlen(this->mReadBuf));
        // call handler
        data = this->GetHandler();
    }
    if (this->mCallback == 0)
    {
        this->SentResponse(data);
        this->FreeConnection();
    }
    else
    {
        otbrLog(OTBR_LOG_ERR, "wait for callback");
    }
}

void Connection::SentResponse(cJSON *aData)
{
    // need more definition
    otbrLog(OTBR_LOG_ERR, "mStartTime send");
    char *out;
    char *p;

    cJSON *root, *cars;
    root = cJSON_CreateObject();
    cars = cJSON_CreateArray();
    cJSON_AddItemToObject(root, "Error", cars);
    out = cJSON_Print(root);

    if (this->mError == 1)
    {
        p = out;
    }
    else
    {
        p = cJSON_Print(aData);
    }
    char h[]  = "HTTP/1.1 200 OK\r\n";
    char h1[] = "Content-Type: application/json\r\n";
    char h3[] = "Access-Control-Allow-Origin: *\r\n";

    write(this->mFd, h, strlen(h));
    write(this->mFd, h1, strlen(h1));
    write(this->mFd, h3, strlen(h3));
    write(this->mFd, "\r\n", 2);
    write(this->mFd, p, strlen(p));

    delete (p);

    cJSON_Delete(aData);
}

static uint16_t HostSwap16(uint16_t v)

{
    return (((v & 0x00ffU) << 8) & 0xff00) | (((v & 0xff00U) >> 8) & 0x00ff);
}

static char *FormatBytes(const uint8_t *aBytes, uint8_t aLength)
{
    char *p = new char[aLength + 1]();
    char *q = p;

    for (int i = 0; i < aLength; i++)
    {
        snprintf(q, 3, "%02x", aBytes[i]);
        q += 2;
    }
    return p;
}

cJSON *Handler::GetJsonNodeInfo(Connection *aConnection, otInstance *aInstance)
{
    aConnection->mCallback = 0;
    cJSON *ret             = cJSON_CreateObject();
    cJSON *cState          = GetJsonState(aConnection, aInstance);
    cJSON *cName           = GetJsonNetworkName(aConnection, aInstance);
    cJSON *cExtPanId       = GetJsonExtendedPanId(aConnection, aInstance);
    cJSON *cRloc16         = GetJsonRloc16(aConnection, aInstance);
    cJSON *cNumRoute       = GetJsonNumOfRoute(aConnection, aInstance);
    cJSON *cLeaderData     = GetJsonLeaderData(aConnection, aInstance);
    cJSON *cExtAddr        = GetJsonExtendedAddr(aConnection, aInstance);

    cJSON_AddItemToObject(ret, "networkName", cName);
    cJSON_AddItemToObject(ret, "state", cState);
    cJSON_AddItemToObject(ret, "extAddress", cExtAddr);
    cJSON_AddItemToObject(ret, "rloc16", cRloc16);
    cJSON_AddItemToObject(ret, "numOfRouter", cNumRoute);
    cJSON_AddItemToObject(ret, "leaderData", cLeaderData);
    cJSON_AddItemToObject(ret, "extPanId", cExtPanId);

    return ret;
}

cJSON *Handler::GetJsonExtendedAddr(Connection *aConnection, otInstance *aInstance)
{
    aConnection->mCallback = 0;
    cJSON *cExtAddr;

    const uint8_t *extAddress = reinterpret_cast<const uint8_t *>(otLinkGetExtendedAddress(aInstance));
    char *         p          = FormatBytes(extAddress, OT_EXT_ADDRESS_SIZE);
    cExtAddr                  = cJSON_CreateString(p);
    delete (p);

    return cExtAddr;
}

cJSON *Handler::GetJsonState(Connection *aConnection, otInstance *aInstance)
{
    aConnection->mCallback = 0;
    cJSON *cState;

    switch (otThreadGetDeviceRole(aInstance))
    {
    case OT_DEVICE_ROLE_DISABLED:
        cState = cJSON_CreateString("disabled");
        break;
    case OT_DEVICE_ROLE_DETACHED:
        cState = cJSON_CreateString("detached");
        break;
    case OT_DEVICE_ROLE_CHILD:
        cState = cJSON_CreateString("child");
        break;
    case OT_DEVICE_ROLE_ROUTER:
        cState = cJSON_CreateString("router");
        break;
    case OT_DEVICE_ROLE_LEADER:
        cState = cJSON_CreateString("leader");
        break;
    default:
        cState = cJSON_CreateString("invalid state");
        break;
    }

    return cState;
}

cJSON *Handler::GetJsonNetworkName(Connection *aConnection, otInstance *aInstance)
{
    aConnection->mCallback  = 0;
    const char *networkName = otThreadGetNetworkName(aInstance);
    char *      p           = new char[20]();
    sprintf(p, "%s", static_cast<const char *>(networkName));
    cJSON *ret = cJSON_CreateString(p);
    delete (p);
    return ret;
}

cJSON *Handler::GetJsonLeaderData(Connection *aConnection, otInstance *aInstance)
{
    aConnection->mCallback = 0;
    otLeaderData leaderData;
    otThreadGetLeaderData(aInstance, &leaderData);
    cJSON *ret = JSONGenerator::CreateJsonLeaderData(leaderData);
    return ret;
}

cJSON *Handler::GetJsonNumOfRoute(Connection *aConnection, otInstance *aInstance)
{
    aConnection->mCallback = 0;
    int          count     = 0;
    uint8_t      maxRouterId;
    otRouterInfo routerInfo;
    maxRouterId = otThreadGetMaxRouterId(aInstance);
    char *p     = new char[6]();

    for (uint8_t i = 0; i <= maxRouterId; i++)
    {
        if (otThreadGetRouterInfo(aInstance, i, &routerInfo) != OT_ERROR_NONE)
        {
            continue;
        }
        count++;
    }
    sprintf(p, "%d", count);
    cJSON *ret = cJSON_CreateString(p);
    delete (p);
    return ret;
}
cJSON *Handler::GetJsonRloc16(Connection *aConnection, otInstance *aInstance)
{
    aConnection->mCallback = 0;
    uint16_t rloc16        = otThreadGetRloc16(aInstance);
    char *   p             = new char[7]();
    sprintf(p, "0x%04x", rloc16);
    cJSON *ret = cJSON_CreateString(p);
    delete (p);
    return ret;
}

cJSON *Handler::GetJsonExtendedPanId(Connection *aConnection, otInstance *aInstance)
{
    aConnection->mCallback  = 0;
    const uint8_t *extPanId = reinterpret_cast<const uint8_t *>(otThreadGetExtendedPanId(aInstance));
    char *         p        = FormatBytes(extPanId, OT_EXT_PAN_ID_SIZE);

    cJSON *ret = cJSON_CreateString(p);
    delete (p);

    return ret;
}

cJSON *Handler::GetJsonRloc(Connection *aConnection, otInstance *aInstance)
{
    aConnection->mCallback            = 0;
    struct otIp6Address rloc16address = *otThreadGetRloc(aInstance);
    char *              p             = new char[65]();
    sprintf(p, "%x:%x:%x:%x:%x:%x:%x:%x", HostSwap16(rloc16address.mFields.m16[0]),
            HostSwap16(rloc16address.mFields.m16[1]), HostSwap16(rloc16address.mFields.m16[2]),
            HostSwap16(rloc16address.mFields.m16[3]), HostSwap16(rloc16address.mFields.m16[4]),
            HostSwap16(rloc16address.mFields.m16[5]), HostSwap16(rloc16address.mFields.m16[6]),
            HostSwap16(rloc16address.mFields.m16[7]));
    cJSON *ret = cJSON_CreateString(p);
    delete (p);

    return ret;
}

cJSON *Handler::DiagnosticRequestHandler(Connection *aConnection, otInstance *aInstance)
{
    aConnection->mCallback            = 1;
    aConnection->mRequested           = 1;
    aConnection->mDiagInfo            = new DiagInfo(steady_clock::now());
    aConnection->mDiagInfo->mDiagJson = cJSON_CreateArray();

    struct otIp6Address rloc16address     = *otThreadGetRloc(aInstance);
    char                multicastAddr[10] = "ff02::2";
    struct otIp6Address multicastAddress;
    otIp6AddressFromString(multicastAddr, &multicastAddress);
    cJSON *ret = NULL;

    char *p = new char[65]();
    sprintf(p, "%x:%x:%x:%x:%x:%x:%x:%x", HostSwap16(rloc16address.mFields.m16[0]),
            HostSwap16(rloc16address.mFields.m16[1]), HostSwap16(rloc16address.mFields.m16[2]),
            HostSwap16(rloc16address.mFields.m16[3]), HostSwap16(rloc16address.mFields.m16[4]),
            HostSwap16(rloc16address.mFields.m16[5]), HostSwap16(rloc16address.mFields.m16[6]),
            HostSwap16(rloc16address.mFields.m16[7]));
    uint8_t tlvTypes[OT_NETWORK_DIAGNOSTIC_TYPELIST_MAX_ENTRIES];
    uint8_t count = 0;

    for (long t = 0; t <= 9; t++)
    {
        tlvTypes[count++] = static_cast<uint8_t>(t);
    }
    for (long t = 14; t <= 19; t++)
    {
        tlvTypes[count++] = static_cast<uint8_t>(t);
    }

    otThreadSendDiagnosticGet(aInstance, &rloc16address, tlvTypes, count);
    otThreadSendDiagnosticGet(aInstance, &multicastAddress, tlvTypes, count);
    delete (p);
    return ret;
}

cJSON *JSONGenerator::CreateJsonConnectivity(const otNetworkDiagConnectivity &aConnectivity)
{
    cJSON *ret = cJSON_CreateObject();

    char *tmp = new char[11]();

    sprintf(tmp, "%d", aConnectivity.mParentPriority);
    cJSON_AddItemToObject(ret, "ParentPriority", cJSON_CreateString(tmp));

    sprintf(tmp, "%u", aConnectivity.mLinkQuality3);
    cJSON_AddItemToObject(ret, "LinkQuality3", cJSON_CreateString(tmp));

    sprintf(tmp, "%u", aConnectivity.mLinkQuality2);
    cJSON_AddItemToObject(ret, "LinkQuality2", cJSON_CreateString(tmp));

    sprintf(tmp, "%u", aConnectivity.mLinkQuality1);
    cJSON_AddItemToObject(ret, "LinkQuality1", cJSON_CreateString(tmp));

    sprintf(tmp, "%u", aConnectivity.mLeaderCost);
    cJSON_AddItemToObject(ret, "LeaderCost", cJSON_CreateString(tmp));

    sprintf(tmp, "%u", aConnectivity.mIdSequence);
    cJSON_AddItemToObject(ret, "IdSequence", cJSON_CreateString(tmp));

    sprintf(tmp, "%u", aConnectivity.mActiveRouters);
    cJSON_AddItemToObject(ret, "ActiveRouters", cJSON_CreateString(tmp));

    sprintf(tmp, "%u", aConnectivity.mSedBufferSize);
    cJSON_AddItemToObject(ret, "SedBufferSize", cJSON_CreateString(tmp));

    sprintf(tmp, "%u", aConnectivity.mSedDatagramCount);
    cJSON_AddItemToObject(ret, "SedDatagramCount", cJSON_CreateString(tmp));

    return ret;
}
cJSON *JSONGenerator::CreateJsonMode(const otLinkModeConfig &aMode)
{
    cJSON *ret = cJSON_CreateObject();

    char *tmp = new char[2]();

    sprintf(tmp, "%d", aMode.mRxOnWhenIdle);
    cJSON_AddItemToObject(ret, "RxOnWhenIdle", cJSON_CreateString(tmp));

    sprintf(tmp, "%d", aMode.mSecureDataRequests);
    cJSON_AddItemToObject(ret, "SecureDataRequests", cJSON_CreateString(tmp));

    sprintf(tmp, "%d", aMode.mDeviceType);
    cJSON_AddItemToObject(ret, "DeviceType", cJSON_CreateString(tmp));

    sprintf(tmp, "%d", aMode.mNetworkData);
    cJSON_AddItemToObject(ret, "NetworkData", cJSON_CreateString(tmp));
    delete (tmp);
    return ret;
}

cJSON *JSONGenerator::CreateJsonRoute(const otNetworkDiagRoute &aRoute)
{
    cJSON *ret = cJSON_CreateObject();

    char *tmp = new char[11]();

    sprintf(tmp, "%d", aRoute.mIdSequence);

    cJSON_AddItemToObject(ret, "IdSequence", cJSON_CreateString(tmp));

    cJSON *RouteData = cJSON_CreateArray();
    for (uint16_t i = 0; i < aRoute.mRouteCount; ++i)
    {
        cJSON *RouteDatatmp = CreateJsonRouteData(aRoute.mRouteData[i]);

        cJSON_AddItemToArray(RouteData, RouteDatatmp);
    }

    cJSON_AddItemToObject(ret, "RouteData", RouteData);
    delete (tmp);
    return ret;
}
cJSON *JSONGenerator::CreateJsonRouteData(const otNetworkDiagRouteData &aRouteData)
{
    char * tmp       = new char[5]();
    cJSON *RouteData = cJSON_CreateObject();

    sprintf(tmp, "0x%02x", aRouteData.mRouterId);
    cJSON_AddItemToObject(RouteData, "RouteId", cJSON_CreateString(tmp));

    sprintf(tmp, "%d", aRouteData.mLinkQualityOut);
    cJSON_AddItemToObject(RouteData, "LinkQualityOut", cJSON_CreateString(tmp));

    sprintf(tmp, "%d", aRouteData.mLinkQualityIn);
    cJSON_AddItemToObject(RouteData, "LinkQualityIn", cJSON_CreateString(tmp));

    sprintf(tmp, "%d", aRouteData.mRouteCost);
    cJSON_AddItemToObject(RouteData, "RouteCost", cJSON_CreateString(tmp));
    delete (tmp);
    return RouteData;
}

cJSON *JSONGenerator::CreateJsonLeaderData(const otLeaderData &aLeaderData)
{
    char * tmp        = new char[9]();
    cJSON *LeaderData = cJSON_CreateObject();

    sprintf(tmp, "0x%08x", aLeaderData.mPartitionId);
    cJSON_AddItemToObject(LeaderData, "PartitionId", cJSON_CreateString(tmp));

    sprintf(tmp, "%d", aLeaderData.mWeighting);
    cJSON_AddItemToObject(LeaderData, "Weighting", cJSON_CreateString(tmp));

    sprintf(tmp, "%d", aLeaderData.mDataVersion);
    cJSON_AddItemToObject(LeaderData, "DataVersion", cJSON_CreateString(tmp));

    sprintf(tmp, "%d", aLeaderData.mStableDataVersion);
    cJSON_AddItemToObject(LeaderData, "StableDataVersion", cJSON_CreateString(tmp));

    sprintf(tmp, "0x%02x", aLeaderData.mLeaderRouterId);
    cJSON_AddItemToObject(LeaderData, "LeaderRouterId", cJSON_CreateString(tmp));
    delete (tmp);
    return LeaderData;
}
cJSON *JSONGenerator::CreateJsonIp6Address(const otIp6Address &aAddress)
{
    char *tmp = new char[65]();

    sprintf(tmp, "%x:%x:%x:%x:%x:%x:%x:%x", HostSwap16(aAddress.mFields.m16[0]), HostSwap16(aAddress.mFields.m16[1]),
            HostSwap16(aAddress.mFields.m16[2]), HostSwap16(aAddress.mFields.m16[3]),
            HostSwap16(aAddress.mFields.m16[4]), HostSwap16(aAddress.mFields.m16[5]),
            HostSwap16(aAddress.mFields.m16[6]), HostSwap16(aAddress.mFields.m16[7]));

    cJSON *ret = cJSON_CreateString(tmp);
    delete (tmp);
    return ret;
}

cJSON *JSONGenerator::CreateJsonMacCounters(const otNetworkDiagMacCounters &aMacCounters)
{
    char * tmp          = new char[9]();
    cJSON *cMacCounters = cJSON_CreateObject();

    sprintf(tmp, "%d", aMacCounters.mIfInUnknownProtos);
    cJSON_AddItemToObject(cMacCounters, "IfInUnknownProtos", cJSON_CreateString(tmp));

    sprintf(tmp, "%d", aMacCounters.mIfInErrors);
    cJSON_AddItemToObject(cMacCounters, "IfInErrors", cJSON_CreateString(tmp));

    sprintf(tmp, "%d", aMacCounters.mIfOutErrors);
    cJSON_AddItemToObject(cMacCounters, "IfOutErrors", cJSON_CreateString(tmp));

    sprintf(tmp, "%d", aMacCounters.mIfInUcastPkts);
    cJSON_AddItemToObject(cMacCounters, "IfInUcastPkts", cJSON_CreateString(tmp));

    sprintf(tmp, "%d", aMacCounters.mIfInBroadcastPkts);
    cJSON_AddItemToObject(cMacCounters, "IfInBroadcastPkts", cJSON_CreateString(tmp));

    sprintf(tmp, "%d", aMacCounters.mIfInDiscards);
    cJSON_AddItemToObject(cMacCounters, "IfInDiscards", cJSON_CreateString(tmp));

    sprintf(tmp, "%d", aMacCounters.mIfOutUcastPkts);
    cJSON_AddItemToObject(cMacCounters, "IfOutUcastPkts", cJSON_CreateString(tmp));

    sprintf(tmp, "%d", aMacCounters.mIfOutBroadcastPkts);
    cJSON_AddItemToObject(cMacCounters, "IfOutBroadcastPkts", cJSON_CreateString(tmp));

    sprintf(tmp, "%d", aMacCounters.mIfOutDiscards);
    cJSON_AddItemToObject(cMacCounters, "IfOutDiscards", cJSON_CreateString(tmp));
    delete (tmp);
    return cMacCounters;
}

cJSON *JSONGenerator::CreateJsonChildTableEntry(const otNetworkDiagChildEntry &aChildEntry)
{
    char * tmp              = new char[7]();
    cJSON *cChildTableEntry = cJSON_CreateObject();

    sprintf(tmp, "0x%04x", aChildEntry.mChildId);
    cJSON_AddItemToObject(cChildTableEntry, "ChildId", cJSON_CreateString(tmp));

    sprintf(tmp, "%d", aChildEntry.mTimeout);
    cJSON_AddItemToObject(cChildTableEntry, "Timeout", cJSON_CreateString(tmp));

    cJSON *cMode = CreateJsonMode(aChildEntry.mMode);
    cJSON_AddItemToObject(cChildTableEntry, "Mode", cMode);
    delete (tmp);
    return cChildTableEntry;
}

void RestWebServer::DiagnosticResponseHandler(otMessage *aMessage, const otMessageInfo *aMessageInfo, void *aContext)
{
    static_cast<RestWebServer *>(aContext)->DiagnosticResponseHandler(
        aMessage, *static_cast<const otMessageInfo *>(aMessageInfo));
}

void RestWebServer::DiagnosticResponseHandler(otMessage *aMessage, const otMessageInfo)
{
    uint8_t               buf[16];
    uint16_t              bytesToPrint;
    uint16_t              bytesPrinted = 0;
    uint16_t              length       = otMessageGetLength(aMessage) - otMessageGetOffset(aMessage);
    otNetworkDiagTlv      diagTlv;
    otNetworkDiagIterator iterator = OT_NETWORK_DIAGNOSTIC_ITERATOR_INIT;
    otError               error    = OT_ERROR_NONE;
    cJSON *               ret      = cJSON_CreateObject();
    std::string           keyRloc;

    while (length > 0)
    {
        bytesToPrint = (length < sizeof(buf)) ? length : sizeof(buf);
        otMessageRead(aMessage, otMessageGetOffset(aMessage) + bytesPrinted, buf, bytesToPrint);
        length -= bytesToPrint;
        bytesPrinted += bytesToPrint;
    }

    while ((error = otThreadGetNextDiagnosticTlv(aMessage, &iterator, &diagTlv)) == OT_ERROR_NONE)
    {
        switch (diagTlv.mType)
        {
        case OT_NETWORK_DIAGNOSTIC_TLV_EXT_ADDRESS:
        {
            char * p          = FormatBytes(diagTlv.mData.mExtAddress.m8, OT_EXT_ADDRESS_SIZE);
            cJSON *extAddress = cJSON_CreateString(p);
            cJSON_AddItemToObject(ret, "Ext Address", extAddress);
        }
        break;
        case OT_NETWORK_DIAGNOSTIC_TLV_SHORT_ADDRESS:
        {
            char *rloc = new char[7]();
            sprintf(rloc, "0x%04x", diagTlv.mData.mAddr16);
            keyRloc        = std::string(rloc);
            cJSON *cRloc16 = cJSON_CreateString(rloc);
            cJSON_AddItemToObject(ret, "Rloc16", cRloc16);
        }
        break;
        case OT_NETWORK_DIAGNOSTIC_TLV_MODE:
        {
            cJSON *cMode = JSONGenerator::CreateJsonMode(diagTlv.mData.mMode);
            cJSON_AddItemToObject(ret, "Mode", cMode);
        }
        break;
        case OT_NETWORK_DIAGNOSTIC_TLV_TIMEOUT:
        {
            char *timeout = new char[11]();
            sprintf(timeout, "%d", diagTlv.mData.mTimeout);

            cJSON *cTimeout = cJSON_CreateString(timeout);
            cJSON_AddItemToObject(ret, "Timeout", cTimeout);
        }
        break;
        case OT_NETWORK_DIAGNOSTIC_TLV_CONNECTIVITY:
        {
            cJSON *cConnectivity = JSONGenerator::CreateJsonConnectivity(diagTlv.mData.mConnectivity);
            cJSON_AddItemToObject(ret, "Connectivity", cConnectivity);
        }
        break;
        case OT_NETWORK_DIAGNOSTIC_TLV_ROUTE:
        {
            cJSON *cRoute = JSONGenerator::CreateJsonRoute(diagTlv.mData.mRoute);
            cJSON_AddItemToObject(ret, "Route", cRoute);
        }
        break;
        case OT_NETWORK_DIAGNOSTIC_TLV_LEADER_DATA:
        {
            cJSON *cLeaderData = JSONGenerator::CreateJsonLeaderData(diagTlv.mData.mLeaderData);
            cJSON_AddItemToObject(ret, "Leader Data", cLeaderData);
        }
        break;
        case OT_NETWORK_DIAGNOSTIC_TLV_NETWORK_DATA:
        {
            char *p = FormatBytes(diagTlv.mData.mNetworkData.m8, diagTlv.mData.mNetworkData.mCount);

            cJSON *cNetworkData = cJSON_CreateString(p);
            cJSON_AddItemToObject(ret, "Network Data", cNetworkData);
        }

        break;
        case OT_NETWORK_DIAGNOSTIC_TLV_IP6_ADDR_LIST:
        {
            cJSON *cIP6List = cJSON_CreateArray();
            for (uint16_t i = 0; i < diagTlv.mData.mIp6AddrList.mCount; ++i)
            {
                cJSON *cIP6Address = JSONGenerator::CreateJsonIp6Address(diagTlv.mData.mIp6AddrList.mList[i]);

                cJSON_AddItemToArray(cIP6List, cIP6Address);
            }
            cJSON_AddItemToObject(ret, "IP6 Address List", cIP6List);
        }
        break;
        case OT_NETWORK_DIAGNOSTIC_TLV_MAC_COUNTERS:
        {
            cJSON *cMacCounters = JSONGenerator::CreateJsonMacCounters(diagTlv.mData.mMacCounters);
            cJSON_AddItemToObject(ret, "MAC Counters", cMacCounters);
        }
        break;
        case OT_NETWORK_DIAGNOSTIC_TLV_BATTERY_LEVEL:
        {
            char *batteryLevel = new char[9]();
            sprintf(batteryLevel, "%d", diagTlv.mData.mBatteryLevel);
            cJSON *cBatteryLevel = cJSON_CreateString(batteryLevel);
            cJSON_AddItemToObject(ret, "Battery Level", cBatteryLevel);
        }
        break;
        case OT_NETWORK_DIAGNOSTIC_TLV_SUPPLY_VOLTAGE:
        {
            char *supplyVoltage = new char[9]();
            sprintf(supplyVoltage, "%d", diagTlv.mData.mSupplyVoltage);
            cJSON *cSupplyVoltage = cJSON_CreateString(supplyVoltage);
            cJSON_AddItemToObject(ret, "Supply Voltage", cSupplyVoltage);
        }

        break;
        case OT_NETWORK_DIAGNOSTIC_TLV_CHILD_TABLE:
        {
            cJSON *cChildTable = cJSON_CreateArray();

            for (uint16_t i = 0; i < diagTlv.mData.mChildTable.mCount; ++i)
            {
                cJSON *cChildTableEntry = JSONGenerator::CreateJsonChildTableEntry(diagTlv.mData.mChildTable.mTable[i]);
                cJSON_AddItemToArray(cChildTable, cChildTableEntry);
            }
            cJSON_AddItemToObject(ret, "Child Table", cChildTable);
        }
        break;
        case OT_NETWORK_DIAGNOSTIC_TLV_CHANNEL_PAGES:
        {
            char *channelPages = FormatBytes(diagTlv.mData.mChannelPages.m8, diagTlv.mData.mChannelPages.mCount);

            cJSON *cChannelPages = cJSON_CreateString(channelPages);
            cJSON_AddItemToObject(ret, "Channel Pages", cChannelPages);
            delete (channelPages);
        }
        break;
        case OT_NETWORK_DIAGNOSTIC_TLV_MAX_CHILD_TIMEOUT:
        {
            char *childTimeout = new char[9]();
            sprintf(childTimeout, "%d", diagTlv.mData.mMaxChildTimeout);
            cJSON *cChildTimeout = cJSON_CreateString(childTimeout);
            cJSON_AddItemToObject(ret, "Max Child Timeout", cChildTimeout);
            delete (childTimeout);
        }

        break;
        }
    }
    for (auto it = mConnectionSet.begin(); it != mConnectionSet.end(); ++it)
    {
        Connection *connection = it->second;
        if (connection->mRequested)
        {
            auto duration =
                duration_cast<microseconds>(steady_clock::now() - connection->mDiagInfo->mStartTime).count();
            if (duration <= sTimeout * 4 &&
                connection->mDiagInfo->mNodeSet.find(keyRloc) == connection->mDiagInfo->mNodeSet.end())
            {
                connection->mDiagInfo->mNodeSet.insert(keyRloc);
                cJSON_AddItemToArray(connection->mDiagInfo->mDiagJson, ret);

                break;
            }
        }
    }
}

} // namespace agent
} // namespace otbr
