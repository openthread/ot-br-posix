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
 *   This file implements the web service of border router
 */

#ifndef WEB_SERVICE
#define WEB_SERVICE

#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <net/if.h>
#include <syslog.h>

#include <boost/asio/ip/tcp.hpp>
#include <jsoncpp/json/json.h>
#include <jsoncpp/json/writer.h>

#include "../wpan-controller/wpan_controller.hpp"

namespace SimpleWeb {
template<class T> class Server;
typedef boost::asio::ip::tcp::socket HTTP;
}

namespace ot {
namespace Web {

typedef SimpleWeb::Server<SimpleWeb::HTTP> HttpServer;

class WebServer
{
public:
    WebServer(void);
    ~WebServer(void);
    void StartWebServer(const char *aIfName);

    enum
    {
        kPropertyType_String = 0,
        kPropertyType_Data
    };

private:
    typedef std::string (*HttpRequestCallback)(Json::Value &aJsonObject, const char *aIfName);

    void HandleHttpRequest(const char *aUrl, const char *aMethod, HttpRequestCallback aCallback, const char *aIfName);
    void JoinNetworkResponse(void);
    void FormNetworkResponse(void);
    void AddOnMeshPrefix(void);
    void DeleteOnMeshPrefix(void);
    void GetNetworkResponse(void);
    void AvailableNetworkResponse(void);
    void BootMdnsPublisher(void);
    void DefaultHttpResponse(void);

    HttpServer                      *mServer;
    static ot::Dbus::WpanNetworkInfo sNetworks[DBUS_MAXIMUM_NAME_LENGTH];
    static int                       sNetworksCount;
    static std::string               sNetowrkName, sExtPanId;
    static bool                      sIsStarted;
    char                             mIfName[IFNAMSIZ];
};

} //namespace Web
} //namespace ot

#endif
