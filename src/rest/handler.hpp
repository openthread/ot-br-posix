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
 *   This file includes Handler definition for RESTful HTTP server.
 */

#ifndef OTBR_REST_HANDLER_HPP_
#define OTBR_REST_HANDLER_HPP_

#include <openthread/netdiag.h>

#include "agent/ncp_openthread.hpp"
#include "agent/thread_helper.hpp"
#include "rest/response.hpp"
#include "rest/connection.hpp"
#include "rest/json.hpp"
#include "rest/rest_web_server.hpp"
#include "openthread/thread_ftd.h"

namespace otbr {
namespace rest {
class Connection;
class Response;
class RestWebServer;
class JSON;

typedef void (*requestHandler)(Connection& aConnection, Response &aResponse);
typedef std::unordered_map<std::string, requestHandler> HandlerMap;

class Handler{

    public:

    Handler();
    static requestHandler GetHandler(std::string aPath);
    static void GetNodeInfo(Connection &aConnection,  Response& aResponse);
    static void GetExtendedAddr(Connection &aConnection,  Response& aResponse);
    static void GetState(Connection &aConnection, Response& aResponse);
    static void GetNetworkName(Connection &aConnection,  Response& aResponse);
    static void GetLeaderData(Connection &aConnection,  Response& aResponse);
    static void GetNumOfRoute(Connection &aConnection, Response& aResponse );
    static void GetRloc16(Connection &aConnection, Response& aResponse);
    static void GetExtendedPanId(Connection &aConnection,  Response& aResponse);
    static void GetRloc(Connection &aConnection, Response& aResponse );
    static void GetDiagnostic(Connection &aConnection, Response& aResponse);
    static void ErrorHandler(Connection &aConnection, Response& aResponse );
    
    static void DiagnosticResponseHandler(otMessage *aMessage, const otMessageInfo *aMessageInfo, void *aContext);
    static void DiagnosticResponseHandler(otMessage *aMessage, const otMessageInfo, RestWebServer * aRestWebServer);
    
    


    private:
 
    static std::string GetDataNodeInfo(Connection &aConnection);
    static std::string GetDataExtendedAddr(Connection &aConnection);
    static std::string GetDataState(Connection &aConnection);
    static std::string GetDataNetworkName(Connection &aConnection);
    static std::string GetDataLeaderData(Connection &aConnection);
    static std::string GetDataNumOfRoute(Connection &aConnection);
    static std::string GetDataRloc16(Connection &aConnection);
    static std::string GetDataExtendedPanId(Connection &aConnection);
    static std::string GetDataRloc(Connection &aConnection );
 

    static JSON mJsonFormater;
    static HandlerMap mHandlerMap;
    
    

};




}
}


#endif
