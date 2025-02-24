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
 *   This file implements the web server of border router
 */

#define OTBR_LOG_TAG "WEB"

#include "web/web-service/web_server.hpp"

#include "common/code_utils.hpp"
#include "common/logging.hpp"

#define OT_ADD_PREFIX_PATH "^/add_prefix"
#define OT_AVAILABLE_NETWORK_PATH "^/available_network$"
#define OT_DELETE_PREFIX_PATH "^/delete_prefix"
#define OT_FORM_NETWORK_PATH "^/form_network$"
#define OT_GET_NETWORK_PATH "^/get_properties$"
#define OT_JOIN_NETWORK_PATH "^/join_network$"
#define OT_GET_QRCODE_PATH "^/get_qrcode$"
#define OT_SET_NETWORK_PATH "^/settings$"
#define OT_COMMISSIONER_START_PATH "^/commission$"
#define OT_REQUEST_METHOD_GET "GET"
#define OT_REQUEST_METHOD_POST "POST"
#define OT_RESPONSE_SUCCESS_STATUS "HTTP/1.1 200 OK\r\n"
#define OT_RESPONSE_HEADER_LENGTH "Content-Length: "
#define OT_RESPONSE_HEADER_CSS_TYPE "\r\nContent-Type: text/css"
#define OT_RESPONSE_HEADER_TEXT_HTML_TYPE "\r\nContent-Type: text/html; charset=utf-8"
#define OT_RESPONSE_HEADER_TYPE "Content-Type: application/json\r\n charset=utf-8"
#define OT_RESPONSE_PLACEHOLD "\r\n\r\n"
#define OT_RESPONSE_FAILURE_STATUS "HTTP/1.1 400 Bad Request\r\n"
#define OT_BUFFER_SIZE 1024

namespace otbr {
namespace Web {

using httplib::Request;
using httplib::Response;

WebServer::WebServer(void)
{
}

WebServer::~WebServer(void)
{
}

void WebServer::Init()
{
    std::string networkName, extPanId;

    if (mWpanService.GetWpanServiceStatus(networkName, extPanId) > 0)
    {
        return;
    }
}

void WebServer::StartWebServer(const char *aIfName, const char *aListenAddr, uint16_t aPort)
{
    mWpanService.SetInterfaceName(aIfName);
    Init();
    ResponseGetQRCode();
    ResponseJoinNetwork();
    ResponseFormNetwork();
    ResponseAddOnMeshPrefix();
    ResponseDeleteOnMeshPrefix();
    ResponseGetStatus();
    ResponseGetAvailableNetwork();
    ResponseCommission();
    mServer.set_mount_point("/", WEB_FILE_PATH);

    mServer.listen(aListenAddr, aPort);
}

void WebServer::StopWebServer(void)
{
    try
    {
        mServer.stop();
    } catch (const std::exception &e)
    {
        otbrLogCrit("failed to stop web server: %s", e.what());
    }
}

std::string WebServer::HandleJoinNetworkRequest(const std::string &aJoinRequest, void *aUserData)
{
    WebServer *webServer = static_cast<WebServer *>(aUserData);

    return webServer->HandleJoinNetworkRequest(aJoinRequest);
}

std::string WebServer::HandleGetQRCodeRequest(const std::string &aGetQRCodeRequest, void *aUserData)
{
    WebServer *webServer = static_cast<WebServer *>(aUserData);

    return webServer->HandleGetQRCodeRequest(aGetQRCodeRequest);
}

std::string WebServer::HandleFormNetworkRequest(const std::string &aFormRequest, void *aUserData)
{
    WebServer *webServer = static_cast<WebServer *>(aUserData);

    return webServer->HandleFormNetworkRequest(aFormRequest);
}

std::string WebServer::HandleAddPrefixRequest(const std::string &aAddPrefixRequest, void *aUserData)
{
    WebServer *webServer = static_cast<WebServer *>(aUserData);

    return webServer->HandleAddPrefixRequest(aAddPrefixRequest);
}

std::string WebServer::HandleDeletePrefixRequest(const std::string &aDeletePrefixRequest, void *aUserData)
{
    WebServer *webServer = static_cast<WebServer *>(aUserData);

    return webServer->HandleDeletePrefixRequest(aDeletePrefixRequest);
}

std::string WebServer::HandleGetStatusRequest(const std::string &aGetStatusRequest, void *aUserData)
{
    WebServer *webServer = static_cast<WebServer *>(aUserData);

    return webServer->HandleGetStatusRequest(aGetStatusRequest);
}

std::string WebServer::HandleGetAvailableNetworkResponse(const std::string &aGetAvailableNetworkRequest,
                                                         void              *aUserData)
{
    WebServer *webServer = static_cast<WebServer *>(aUserData);

    return webServer->HandleGetAvailableNetworkResponse(aGetAvailableNetworkRequest);
}

std::string WebServer::HandleCommission(const std::string &aCommissionRequest, void *aUserData)
{
    WebServer *webServer = static_cast<WebServer *>(aUserData);

    return webServer->HandleCommission(aCommissionRequest);
}

void WebServer::ResponseJoinNetwork(void)
{
    mServer.Post(OT_JOIN_NETWORK_PATH, [this](const Request &aRequest, Response &aResponse) {
        auto body = HandleJoinNetworkRequest(aRequest.body);
        aResponse.set_content(body, OT_RESPONSE_HEADER_TYPE);
    });
}

void WebServer::ResponseGetQRCode(void)
{
    mServer.Get(OT_GET_QRCODE_PATH, [this](const Request &aRequest, Response &aResponse) {
        auto body = HandleGetQRCodeRequest(aRequest.body);
        aResponse.set_content(body, OT_RESPONSE_HEADER_TYPE);
    });
}

void WebServer::ResponseFormNetwork(void)
{
    mServer.Post(OT_FORM_NETWORK_PATH, [this](const Request &aRequest, Response &aResponse) {
        auto body = HandleFormNetworkRequest(aRequest.body);
        aResponse.set_content(body, OT_RESPONSE_HEADER_TYPE);
    });
}

void WebServer::ResponseAddOnMeshPrefix(void)
{
    mServer.Post(OT_ADD_PREFIX_PATH, [this](const Request &aRequest, Response &aResponse) {
        auto body = HandleAddPrefixRequest(aRequest.body);
        aResponse.set_content(body, OT_RESPONSE_HEADER_TYPE);
    });
}

void WebServer::ResponseDeleteOnMeshPrefix(void)
{
    mServer.Post(OT_DELETE_PREFIX_PATH, [this](const Request &aRequest, Response &aResponse) {
        auto body = HandleDeletePrefixRequest(aRequest.body);
        aResponse.set_content(body, OT_RESPONSE_HEADER_TYPE);
    });
}

void WebServer::ResponseGetStatus(void)
{
    mServer.Get(OT_GET_NETWORK_PATH, [this](const Request &aRequest, Response &aResponse) {
        auto body = HandleGetStatusRequest(aRequest.body);
        aResponse.set_content(body, OT_RESPONSE_HEADER_TYPE);
    });
}

void WebServer::ResponseGetAvailableNetwork(void)
{
    mServer.Get(OT_AVAILABLE_NETWORK_PATH, [this](const Request &aRequest, Response &aResponse) {
        auto body = HandleGetAvailableNetworkResponse(aRequest.body);
        aResponse.set_content(body, OT_RESPONSE_HEADER_TYPE);
    });
}

void WebServer::ResponseCommission(void)
{
    mServer.Post(OT_COMMISSIONER_START_PATH, [this](const Request &aRequest, Response &aResponse) {
        auto body = HandleCommission(aRequest.body);
        aResponse.set_content(body, OT_RESPONSE_HEADER_TYPE);
    });
}

std::string WebServer::HandleJoinNetworkRequest(const std::string &aJoinRequest)
{
    return mWpanService.HandleJoinNetworkRequest(aJoinRequest);
}

std::string WebServer::HandleGetQRCodeRequest(const std::string &aGetQRCodeRequest)
{
    OTBR_UNUSED_VARIABLE(aGetQRCodeRequest);
    return mWpanService.HandleGetQRCodeRequest();
}

std::string WebServer::HandleFormNetworkRequest(const std::string &aFormRequest)
{
    return mWpanService.HandleFormNetworkRequest(aFormRequest);
}

std::string WebServer::HandleAddPrefixRequest(const std::string &aAddPrefixRequest)
{
    return mWpanService.HandleAddPrefixRequest(aAddPrefixRequest);
}

std::string WebServer::HandleDeletePrefixRequest(const std::string &aDeletePrefixRequest)
{
    return mWpanService.HandleDeletePrefixRequest(aDeletePrefixRequest);
}

std::string WebServer::HandleGetStatusRequest(const std::string &aGetStatusRequest)
{
    OTBR_UNUSED_VARIABLE(aGetStatusRequest);
    return mWpanService.HandleStatusRequest();
}

std::string WebServer::HandleGetAvailableNetworkResponse(const std::string &aGetAvailableNetworkRequest)
{
    OTBR_UNUSED_VARIABLE(aGetAvailableNetworkRequest);
    return mWpanService.HandleAvailableNetworkRequest();
}

std::string WebServer::HandleCommission(const std::string &aCommissionRequest)
{
    return mWpanService.HandleCommission(aCommissionRequest);
}

} // namespace Web
} // namespace otbr
