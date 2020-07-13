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

/**
 * @file
 *   This file includes definitions for RESTful HTTP server.
 */

#ifndef OTBR_AGENT_REST_WEB_SERVER_HPP_
#define OTBR_AGENT_REST_WEB_SERVER_HPP_

#include <unordered_map>
#include <unordered_set>

#include <chrono>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <cjson/cJSON.h>
#include <http_parser.h>

#include <openthread/netdiag.h>

#include "agent/ncp_openthread.hpp"
#include "agent/thread_helper.hpp"

using std::chrono::steady_clock;

namespace otbr {
namespace Ncp {
class ControllerOpenThread;
} // namespace Ncp
} // namespace otbr

namespace otbr {
namespace agent {

class Connection;
typedef struct DiagInfo
{
    steady_clock::time_point        mStartTime;
    cJSON *                         mDiagJson;
    std::unordered_set<std::string> mNodeSet;
    DiagInfo(steady_clock::time_point aStartTime)
        : mStartTime(aStartTime)
    {
    }
} DiagInfo;
typedef cJSON *(*requestHandler)(Connection *connection, otInstance *mInstance);
typedef std::unordered_map<std::string, requestHandler> HandlerMap;

class Connection
{
public:
    Connection(steady_clock::time_point aStartTime, HandlerMap *aHandlerMap, otInstance *aInstance);

    void NonBlockRead();

    void ServeRequest();

    void FreeConnection();

    void ParseUri();

    void SentResponse(cJSON *aData);

    cJSON *GetHandler();

    steady_clock::time_point mStartTime;
    DiagInfo *               mDiagInfo;
    HandlerMap *             mHandlerMap;
    otInstance *             mInstance;
    int                      mRequested;
    int                      mError;
    int                      mCallback;
    int                      mFd;
    int                      mCompleted;
    int                      mMethod;
    unsigned int             mContentLength;
    char *                   mPath;
    char *                   mStatus;
    char *                   mBody;
    char *                   mHeaderField;
    char *                   mHeaderValue;
    int                      mHeader_state;
    char *                   mReadBuf;
    char *                   mReadPointer;
    int                      mReadLength;
    int                      mBufRemain;
    http_parser *            mParser;
};

class JSONGenerator
{
public:
    static cJSON *CreateJsonMode(const otLinkModeConfig &aMode);
    static cJSON *CreateJsonConnectivity(const otNetworkDiagConnectivity &aConnectivity);
    static cJSON *CreateJsonRoute(const otNetworkDiagRoute &aRoute);
    static cJSON *CreateJsonRouteData(const otNetworkDiagRouteData &aRouteData);
    static cJSON *CreateJsonLeaderData(const otLeaderData &aLeaderData);
    static cJSON *CreateJsonIp6Address(const otIp6Address &aAddress);
    static cJSON *CreateJsonMacCounters(const otNetworkDiagMacCounters &aMacCounters);
    static cJSON *CreateJsonChildTableEntry(const otNetworkDiagChildEntry &aChildEntry);
};
class Handler
{
public:
    Handler(HandlerMap &aHandlerMap);

    static cJSON *GetJsonNodeInfo(Connection *aConnection, otInstance *aInstance);
    static cJSON *GetJsonExtendedAddr(Connection *aConnection, otInstance *aInstance);
    static cJSON *GetJsonState(Connection *aConnection, otInstance *aInstance);
    static cJSON *GetJsonNetworkName(Connection *aConnection, otInstance *aInstance);
    static cJSON *GetJsonLeaderData(Connection *aConnection, otInstance *aInstance);
    static cJSON *GetJsonNumOfRoute(Connection *aConnection, otInstance *aInstance);
    static cJSON *GetJsonRloc16(Connection *aConnection, otInstance *aInstance);
    static cJSON *GetJsonExtendedPanId(Connection *aConnection, otInstance *aInstance);
    static cJSON *GetJsonRloc(Connection *aConnection, otInstance *aInstance);
    static cJSON *DiagnosticRequestHandler(Connection *aConnection, otInstance *aInstance);

    // callback
};

class RestWebServer
{
public:
    RestWebServer(otbr::Ncp::ControllerOpenThread *aNcp);

    void Init();

    void UpdateFdSet(fd_set &aReadFdSet, int &aMaxFd, timeval &aTimeout);

    void Process(fd_set &aReadFdSet);

private:
    static void DiagnosticResponseHandler(otMessage *aMessage, const otMessageInfo *aMessageInfo, void *aContext);
    void        DiagnosticResponseHandler(otMessage *aMessage, const otMessageInfo);
    // for service
    otbr::Ncp::ControllerOpenThread *mNcp;
    otbr::agent::ThreadHelper *      mThreadHelper;
    otInstance *                     mInstance;

    // for server configure
    sockaddr_in *mAddress;
    int          mListenFd;

    // for Connection
    static const int                      sTimeout     = 1000000;
    int                                   mMaxServeNum = 100;
    std::unordered_map<int, Connection *> mConnectionSet;

    // Handler
    HandlerMap mHandlerMap;
    Handler *  mHandler;
};

} // namespace agent
} // namespace otbr

#endif // OTBR_AGENT_REST_WEB_SERVER_HPP_
