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
typedef struct DiagInfo
{
    std::chrono::steady_clock::time_point start;
    cJSON *                               cDiag;
    std::unordered_set<std::string>       nodeSet;
    DiagInfo(std::chrono::steady_clock::time_point astart)
        : start(astart)
    {
    }
} DiagInfo;

typedef struct Connection
{
    std::chrono::steady_clock::time_point start;
    DiagInfo *                            diagInfo;

    int          diagisRequest;
    int          isError;
    int          hasCallback;
    int          fd;
    int          complete;
    int          method;
    unsigned int content_length;
    char *       path;
    char *       status;
    char *       body;
    char *       header_f;
    char *       header_v;
    int          header_state;
    char *       buf;
    char *       b;
    int          len;
    int          remain;
    http_parser *mParser;
    Connection(std::chrono::steady_clock::time_point astart)
        : start(astart)
        , diagisRequest(0)
        , isError(0)
        , hasCallback(0)
        , complete(0)
        , path(NULL)
        , status(NULL)
        , body(NULL)
        , header_f(NULL)
        , header_v(NULL)
        , buf(NULL)
        , len(0)
        , remain(8192)
    {
    }
} Connection;

typedef cJSON *(*requestHandler)(Connection *connection, otInstance *mInstance);
typedef std::unordered_map<std::string, requestHandler> handlerMap;

class RestWebServer
{
public:
    RestWebServer(otbr::Ncp::ControllerOpenThread *aNcp);

    void init();

    void UpdateFdSet(fd_set &aReadFdSet, int &aMaxFd, timeval &aTimeout);

    void Process(fd_set &aReadFdSet);

private:
    /* data */

    void nonBlockRead(int fd);

    void serveRequest(int fd);

    cJSON *getJSON(int fd);

    void FreeConnection(int fd);

    void parseUri(int fd);

    // for service
    otbr::Ncp::ControllerOpenThread *mNcp;
    otbr::agent::ThreadHelper *      mThreadHelper;
    otInstance *                     mInstance;

    // for server configure
    sockaddr_in *mAddress;
    int          mListenFd;

    // for Connection
    static const int                      mTimeout     = 1000000;
    int                                   mMaxServeNum = 100;
    std::unordered_map<int, Connection *> mConnectionSet;

    // static DiagInfo* mDiagInfo;

    // Handler
    handlerMap mHandlerMap;
    void       handlerInit();

    static cJSON *diagnosticRequestHandler(Connection *connection, otInstance *mInstance);
    static void   diagnosticResponseHandler(otMessage *aMessage, const otMessageInfo *aMessageInfo, void *aContext);
    void          diagnosticResponseHandler(otMessage *aMessage, const otMessageInfo );

    static cJSON *getJsonNodeInfo(Connection *connection, otInstance *mInstance);
    static cJSON *getJsonExtendedAddr(Connection *connection, otInstance *mInstance);
    static cJSON *getJsonState(Connection *connection, otInstance *mInstance);
    static cJSON *getJsonNetworkName(Connection *connection, otInstance *mInstance);
    static cJSON *getJsonLeaderData(Connection *connection, otInstance *mInstance);
    static cJSON *getJsonNumOfRoute(Connection *connection, otInstance *mInstance);
    static cJSON *getJsonRloc16(Connection *connection, otInstance *mInstance);
    static cJSON *getJsonExtendedPanId(Connection *connection, otInstance *mInstance);
    static cJSON *getJsonRloc(Connection *connection, otInstance *mInstance);

    static void sentResponse(cJSON *data, Connection *connection);

    static cJSON *CreateJsonMode(const otLinkModeConfig &aMode);
    static cJSON *CreateJsonConnectivity(const otNetworkDiagConnectivity &aConnectivity);
    static cJSON *CreateJsonRoute(const otNetworkDiagRoute &aRoute);
    static cJSON *CreateJsonRouteData(const otNetworkDiagRouteData &aRouteData);
    static cJSON *CreateJsonLeaderData(const otLeaderData &aLeaderData);
    static cJSON *CreateJsonIp6Address(const otIp6Address &aAddress);
    static cJSON *CreateJsonMacCounters(const otNetworkDiagMacCounters &aMacCounters);
    static cJSON *CreateJsonChildTableEntry(const otNetworkDiagChildEntry &aChildEntry);

    static uint16_t HostSwap16(uint16_t v);
    static char *   formatBytes(const uint8_t *aBytes, uint8_t aLength);
};

} // namespace agent
} // namespace otbr

#endif // OTBR_AGENT_REST_WEB_SERVER_HPP_
