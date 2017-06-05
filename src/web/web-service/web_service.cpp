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

#include "web_service.hpp"

#define BOOST_NO_CXX11_SCOPED_ENUMS
#include <boost/filesystem.hpp>
#undef BOOST_NO_CXX11_SCOPED_ENUMS
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <server_http.hpp>

#include "common/code_utils.hpp"
#include "utils/hex.hpp"

#include "../mdns-publisher/mdns_publisher.hpp"
#include "../pskc-generator/pskc.hpp"
#include "../utils/encoding.hpp"

#define AVAILABLE_NETWORK_URL "^/available_network$"
#define JOIN_NETWORK_URL "^/join_network$"
#define FORM_NETWORK_URL "^/form_network$"
#define SET_NETWORK_URL "^/settings$"
#define GET_NETWORK_URL "^/get_properties$"
#define ADD_PREFIX_URL "^/add_prefix"
#define DELETE_PREFIX_URL "^/delete_prefix"
#define BOOT_MDNS_URL "^/boot_mdns$"
#define REQUEST_METHOD_GET "GET"
#define REQUEST_METHOD_POST "POST"
#define RESPONSE_SUCCESS_STATUS "HTTP/1.1 200 OK\r\n"
#define RESPONSE_HEADER_LENGTH "Content-Length: "
#define RESPONSE_HEADER_TYPE "Content-Type: application/json\r\n charset=utf-8"
#define RESPONSE_PLACEHOLD "\r\n\r\n"
#define RESPONSE_FAILURE_STATUS "HTTP/1.1 400 Bad Request\r\n"

#define NETWORK_NAME_LENGTH 16
#define EXTENED_PANID_LENGTH 8
#define HARDWARE_ADDRESS_LENGTH 8
#define PANID_LENGTH 4
#define BORDER_ROUTER_PORT 49191
#define PUBLISH_SERVICE_INTERVAL 20
#define PSKC_MAX_LENGTH 16

namespace ot {
namespace Web {

ot::Dbus::WpanNetworkInfo sNetworks[DBUS_MAXIMUM_NAME_LENGTH];
int                       sNetworksCount = 0;
std::string               sNetworkName = "";
std::string               sExtPanId = "";
bool                      sIsStarted = false;

void DefaultResourceSend(const HttpServer &aServer, const std::shared_ptr<HttpServer::Response> &aResponse,
                         const std::shared_ptr<std::ifstream> &aIfStream);

static std::string HttpReponse(uint8_t error)
{
    boost::property_tree::ptree root;
    std::stringstream           ss;

    root.put("error", error);
    if (error == 0)
        root.put("result", "successful");
    else
        root.put("result", "failed");
    write_json(ss, root, false);
    return ss.str();
}

static void SetNetworkInfo(const char *networkName, const char *extPanId)
{
    sNetworkName = networkName;
    sExtPanId = extPanId;
}

static std::string OnJoinNetworkRequest(boost::property_tree::ptree &aJoinRequest)
{
    char extPanId[EXTENED_PANID_LENGTH * 2 + 1];
    int  ret = ot::Dbus::kWpantundStatus_Ok;
    int  index = aJoinRequest.get<int>("index");

    std::string networkKey = aJoinRequest.get<std::string>("networkKey");
    std::string prefix = aJoinRequest.get<std::string>("prefix");
    bool        defaultRoute = aJoinRequest.get<bool>("defaultRoute");

    VerifyOrExit(ot::Dbus::WPANController::Start() == ot::Dbus::kWpantundStatus_Ok,
                 ret = ot::Dbus::kWpantundStatus_StartFailed);
    VerifyOrExit(ot::Dbus::WPANController::Leave() == ot::Dbus::kWpantundStatus_Ok,
                 ret = ot::Dbus::kWpantundStatus_LeaveFailed);
    VerifyOrExit(ot::Dbus::WPANController::Set(WebServer::kPropertyType_Data,
                                               "NetworkKey",
                                               networkKey.c_str()) == ot::Dbus::kWpantundStatus_Ok,
                 ret = ot::Dbus::kWpantundStatus_SetFailed);
    VerifyOrExit(ot::Dbus::WPANController::Join(sNetworks[index].mNetworkName,
                                                sNetworks[index].mChannel,
                                                sNetworks[index].mExtPanId,
                                                sNetworks[index].mPanId) == ot::Dbus::kWpantundStatus_Ok,
                 ret = ot::Dbus::kWpantundStatus_JoinFailed);
    VerifyOrExit(ot::Dbus::WPANController::Gateway(prefix.c_str(), 64, defaultRoute) == ot::Dbus::kWpantundStatus_Ok,
                 ret = ot::Dbus::kWpantundStatus_GatewayFailed);

    ot::Utils::Long2Hex(sNetworks[index].mExtPanId, extPanId);
    SetNetworkInfo(sNetworks[index].mNetworkName, extPanId);
exit:
    if (ret != ot::Dbus::kWpantundStatus_Ok)
    {
        syslog(LOG_ERR, "Error is %d\n", ret);
    }
    return HttpReponse(ret);
}

static std::string OnFormNetworkRequest(boost::property_tree::ptree &aFormRequest)
{
    int ret = ot::Dbus::kWpantundStatus_Ok;

    std::string   networkKey = aFormRequest.get<std::string>("networkKey");
    std::string   prefix = aFormRequest.get<std::string>("prefix");
    int           channel = aFormRequest.get<int>("channel");
    std::string   networkName = aFormRequest.get<std::string>("networkName");
    std::string   passphrase = aFormRequest.get<std::string>("passphrase");
    std::string   panId = aFormRequest.get<std::string>("panId");
    std::string   extPanId = aFormRequest.get<std::string>("extPanId");
    bool          defaultRoute = aFormRequest.get<bool>("defaultRoute");
    ot::Psk::Pskc psk;
    char          pskcStr[PSKC_MAX_LENGTH*2];
    uint8_t       extPanIdBytes[EXTENED_PANID_LENGTH];

    VerifyOrExit(ot::Dbus::WPANController::Start() == ot::Dbus::kWpantundStatus_Ok,
                 ret = ot::Dbus::kWpantundStatus_StartFailed);
    VerifyOrExit(ot::Dbus::WPANController::Leave() == ot::Dbus::kWpantundStatus_Ok,
                 ret = ot::Dbus::kWpantundStatus_LeaveFailed);

    VerifyOrExit(ot::Dbus::WPANController::Set(WebServer::kPropertyType_Data,
                                               kWPANTUNDProperty_NetworkKey,
                                               networkKey.c_str()) == ot::Dbus::kWpantundStatus_Ok,
                 ret = ot::Dbus::kWpantundStatus_SetFailed);

    VerifyOrExit(ot::Dbus::WPANController::Set(WebServer::kPropertyType_String,
                                               kWPANTUNDProperty_NetworkPANID,
                                               panId.c_str()) == ot::Dbus::kWpantundStatus_Ok,
                 ret = ot::Dbus::kWpantundStatus_SetFailed);
    VerifyOrExit(ot::Dbus::WPANController::Set(WebServer::kPropertyType_Data,
                                               kWPANTUNDProperty_NetworkXPANID,
                                               extPanId.c_str()) == ot::Dbus::kWpantundStatus_Ok,
                 ret = ot::Dbus::kWpantundStatus_SetFailed);
    ot::Utils::Hex2Bytes(extPanId.c_str(), extPanIdBytes, EXTENED_PANID_LENGTH);
    psk.SetSalt(extPanIdBytes, networkName.c_str());
    psk.SetPassphrase(passphrase.c_str());
    ot::Utils::Bytes2Hex(psk.GetPskc(), PSKC_MAX_LENGTH, pskcStr);
    VerifyOrExit(ot::Dbus::WPANController::Set(WebServer::kPropertyType_Data,
                                               kWPANTUNDProperty_NetworkPskc,
                                               pskcStr) == ot::Dbus::kWpantundStatus_Ok,
                 ret = ot::Dbus::kWpantundStatus_SetFailed);

    VerifyOrExit(ot::Dbus::WPANController::Form(networkName.c_str(), channel) == ot::Dbus::kWpantundStatus_Ok,
                 ret = ot::Dbus::kWpantundStatus_FormFailed);

    VerifyOrExit(ot::Dbus::WPANController::Gateway(prefix.c_str(), 64, defaultRoute) == ot::Dbus::kWpantundStatus_Ok,
                 ret = ot::Dbus::kWpantundStatus_GatewayFailed);
    SetNetworkInfo(networkName.c_str(), extPanId.c_str());
exit:
    if (ret != ot::Dbus::kWpantundStatus_Ok)
    {
        syslog(LOG_ERR, "Error is %d\n", ret);
    }
    return HttpReponse(ret);
}

static std::string OnAddPrefixRequest(boost::property_tree::ptree &aAddPrefixRequest)
{
    int ret = ot::Dbus::kWpantundStatus_Ok;

    std::string prefix = aAddPrefixRequest.get<std::string>("prefix");
    bool        defaultRoute = aAddPrefixRequest.get<bool>("defaultRoute");

    VerifyOrExit(ot::Dbus::WPANController::Start() == ot::Dbus::kWpantundStatus_Ok,
                 ret = ot::Dbus::kWpantundStatus_StartFailed);
    VerifyOrExit(ot::Dbus::WPANController::Gateway(prefix.c_str(), 64, defaultRoute) == ot::Dbus::kWpantundStatus_Ok,
                 ret = ot::Dbus::kWpantundStatus_GatewayFailed);
exit:
    if (ret != ot::Dbus::kWpantundStatus_Ok)
    {
        syslog(LOG_ERR, "Error is %d\n", ret);
    }
    return HttpReponse(ret);

}

static std::string OnDeletePrefixRequest(boost::property_tree::ptree &aDeleteRequest)
{
    int ret = ot::Dbus::kWpantundStatus_Ok;

    std::string prefix = aDeleteRequest.get<std::string>("prefix");

    VerifyOrExit(ot::Dbus::WPANController::Start() == ot::Dbus::kWpantundStatus_Ok,
                 ret = ot::Dbus::kWpantundStatus_StartFailed);
    VerifyOrExit(ot::Dbus::WPANController::RemoveGateway(prefix.c_str(), 64) == ot::Dbus::kWpantundStatus_Ok,
                 ret = ot::Dbus::kWpantundStatus_GatewayFailed);
exit:
    if (ret != ot::Dbus::kWpantundStatus_Ok)
    {
        syslog(LOG_ERR, "Error is %d\n", ret);
    }
    return HttpReponse(ret);
}

static std::string OnGetNetworkRequest(boost::property_tree::ptree &aGetNetworkRequest)
{
    boost::property_tree::ptree root, networkInfo;
    int                         ret = ot::Dbus::kWpantundStatus_Ok;

    VerifyOrExit(ot::Dbus::WPANController::Start() ==  ot::Dbus::kWpantundStatus_Ok,
                 ret = ot::Dbus::kWpantundStatus_StartFailed);
    networkInfo.put("NCP:State", ot::Dbus::WPANController::Get("NCP:State"));
    networkInfo.put("Daemon:Enabled", ot::Dbus::WPANController::Get("Daemon:Enabled"));
    networkInfo.put("NCP:Version", ot::Dbus::WPANController::Get("NCP:Version"));
    networkInfo.put("Daemon:Version", ot::Dbus::WPANController::Get("Daemon:Version"));
    networkInfo.put("Config:NCP:DriverName", ot::Dbus::WPANController::Get("Config:NCP:DriverName"));
    networkInfo.put("NCP:HardwareAddress", ot::Dbus::WPANController::Get("NCP:HardwareAddress"));
    networkInfo.put("NCP:Channel", ot::Dbus::WPANController::Get("NCP:Channel"));
    networkInfo.put("Network:NodeType", ot::Dbus::WPANController::Get("Network:NodeType"));
    networkInfo.put("Network:Name", ot::Dbus::WPANController::Get("Network:Name"));
    networkInfo.put("Network:XPANID", ot::Dbus::WPANController::Get("Network:XPANID"));
    networkInfo.put("Network:PANID", ot::Dbus::WPANController::Get("Network:PANID"));
    networkInfo.put("IPv6:LinkLocalAddress", ot::Dbus::WPANController::Get("IPv6:LinkLocalAddress"));
    networkInfo.put("IPv6:MeshLocalAddress", ot::Dbus::WPANController::Get("IPv6:MeshLocalAddress"));
    networkInfo.put("IPv6:MeshLocalPrefix", ot::Dbus::WPANController::Get("IPv6:MeshLocalPrefix"));
    root.add_child("result", networkInfo);
exit:
    if (ret != ot::Dbus::kWpantundStatus_Ok)
    {
        root.put("result", "failed");
        syslog(LOG_ERR, "Error is %d\n", ret);
    }
    root.put("error", ret);
    std::stringstream ss;
    write_json(ss, root, false);
    return ss.str();
}

static std::string OnGetAvailableNetworkResponse(boost::property_tree::ptree &aGetAvailableNetworkRequest)
{
    boost::property_tree::ptree root, networks, networkInfo;
    int                         ret = ot::Dbus::kWpantundStatus_Ok;

    VerifyOrExit(ot::Dbus::WPANController::Start() == ot::Dbus::kWpantundStatus_Ok,
                 ret = ot::Dbus::kWpantundStatus_LookUpFailed);
    VerifyOrExit(ot::Dbus::WPANController::Leave() == ot::Dbus::kWpantundStatus_Ok,
                 ret = ot::Dbus::kWpantundStatus_LeaveFailed);
    VerifyOrExit(ot::Dbus::WPANController::Scan() == ot::Dbus::kWpantundStatus_Ok,
                 ret = ot::Dbus::kWpantundStatus_ScanFailed);
    sNetworksCount = ot::Dbus::WPANController::GetScanNetworksInfoCount();
    VerifyOrExit(sNetworksCount > 0, ret = ot::Dbus::kWpantundStatus_NetworkNotFound);
    memcpy(sNetworks, ot::Dbus::WPANController::GetScanNetworksInfo(),
           sNetworksCount * sizeof(ot::Dbus::WpanNetworkInfo));

    for (int i = 0; i < sNetworksCount; i++)
    {
        char extPanId[EXTENED_PANID_LENGTH*2+1], panId[PANID_LENGTH+3], hardwareAddress[HARDWARE_ADDRESS_LENGTH*2+1];
        ot::Utils::Long2Hex(Thread::Encoding::BigEndian::HostSwap64(sNetworks[i].mExtPanId), extPanId);
        ot::Utils::Bytes2Hex(sNetworks[i].mHardwareAddress, HARDWARE_ADDRESS_LENGTH, hardwareAddress);
        sprintf(panId, "0x%X", sNetworks[i].mPanId);
        networkInfo.put("nn", sNetworks[i].mNetworkName);
        networkInfo.put("xp", extPanId);
        networkInfo.put("pi", panId);
        networkInfo.put("ch", sNetworks[i].mChannel);
        networkInfo.put("ha", hardwareAddress);
        networks.push_back(std::make_pair("", networkInfo));
    }
    root.add_child("result", networks);
exit:
    if (ret != ot::Dbus::kWpantundStatus_Ok)
    {
        root.put("result", "failed");
        syslog(LOG_ERR, "Error is %d\n", ret);
    }
    root.put("error", ret);
    std::stringstream ss;
    write_json(ss, root, false);
    return ss.str();
}

static std::string OnBootMdnsRequest(boost::property_tree::ptree &aBootMdnsRequest)
{
    std::thread mdnsPublisherThread([]() {
                ot::Mdns::Publisher::SetServiceName(sNetworkName.c_str());
                ot::Mdns::Publisher::SetType("_meshcop._udp");
                ot::Mdns::Publisher::SetPort(BORDER_ROUTER_PORT);
                sNetworkName = "nn=" + sNetworkName;
                sExtPanId = "xp=" + sExtPanId;
                ot::Mdns::Publisher::SetNetworkNameTxT(sNetworkName.c_str());
                ot::Mdns::Publisher::SetEPANIDTxT(sExtPanId.c_str());
                if (sIsStarted)
                    ot::Mdns::Publisher::UpdateService();
                else
                {
                    sIsStarted = true;
                    ot::Mdns::Publisher::StartServer();
                }
            });

    mdnsPublisherThread.detach();
    return HttpReponse(ot::Dbus::kWpantundStatus_Ok);
}

WebServer::WebServer(void) : mServer(new HttpServer()) {}

WebServer::~WebServer(void)
{
    delete &mServer;
}

void WebServer::StartWebServer(void)
{
    mServer->config.port = 80;
    JoinNetworkResponse();
    FormNetworkResponse();
    AddOnMeshPrefix();
    DeleteOnMeshPrefix();
    GetNetworkResponse();
    AvailableNetworkResponse();
    DefaultHttpResponse();
    BootMdnsPublisher();
    std::thread ServerThread([this]() {
                mServer->start();
            });
    ServerThread.join();
}

void WebServer::JoinNetworkResponse(void)
{
    HandleHttpRequest(JOIN_NETWORK_URL, REQUEST_METHOD_POST, OnJoinNetworkRequest);
}

void WebServer::FormNetworkResponse(void)
{
    HandleHttpRequest(FORM_NETWORK_URL, REQUEST_METHOD_POST, OnFormNetworkRequest);
}

void WebServer::AddOnMeshPrefix(void)
{
    HandleHttpRequest(ADD_PREFIX_URL, REQUEST_METHOD_POST, OnAddPrefixRequest);
}

void WebServer::DeleteOnMeshPrefix(void)
{
    HandleHttpRequest(DELETE_PREFIX_URL, REQUEST_METHOD_POST, OnDeletePrefixRequest);
}

void WebServer::GetNetworkResponse(void)
{
    HandleHttpRequest(GET_NETWORK_URL, REQUEST_METHOD_GET, OnGetNetworkRequest);
}

void WebServer::AvailableNetworkResponse(void)
{
    HandleHttpRequest(AVAILABLE_NETWORK_URL, REQUEST_METHOD_GET, OnGetAvailableNetworkResponse);
}

void WebServer::BootMdnsPublisher(void)
{
    HandleHttpRequest(BOOT_MDNS_URL, REQUEST_METHOD_GET, OnBootMdnsRequest);
}

void WebServer::HandleHttpRequest(const char *aUrl, const char *aMethod, HttpRequestCallback aCallback)
{
    mServer->resource[aUrl][aMethod] =
        [aCallback](std::shared_ptr<HttpServer::Response> response, std::shared_ptr<HttpServer::Request> request)
        {
            try
            {
                boost::property_tree::ptree pt;
                std::string                 httpResponse;
                if (aCallback != NULL)
                {
                    if (request->content.size() > 0)
                    {
                        read_json(request->content, pt);
                    }
                    httpResponse = aCallback(pt);
                }

                *response << RESPONSE_SUCCESS_STATUS
                          << RESPONSE_HEADER_LENGTH
                          << httpResponse.length()
                          << RESPONSE_PLACEHOLD
                          << httpResponse;
            }
            catch (std::exception & e)
            {

                *response << RESPONSE_FAILURE_STATUS
                          << RESPONSE_HEADER_LENGTH
                          << strlen(e.what())
                          << RESPONSE_PLACEHOLD
                          << e.what();
            }
        };
}

void WebServer::DefaultHttpResponse(void)
{
    int ret = ot::Dbus::kWpantundStatus_Ok;

    mServer->default_resource[REQUEST_METHOD_GET] =
        [this](std::shared_ptr<HttpServer::Response> response, std::shared_ptr<HttpServer::Request> request)
        {
            try
            {
                auto webRootPath =
                    boost::filesystem::canonical(WEB_FILE_PATH);
                auto path = boost::filesystem::canonical(
                    webRootPath/request->path);
                //Check if path is within webRootPath
                if (std::distance(webRootPath.begin(),
                                  webRootPath.end()) > std::distance(path.begin(), path.end()) ||
                    !std::equal(webRootPath.begin(), webRootPath.end(),
                                path.begin()))
                    throw std::invalid_argument("path must be within root path");
                if (boost::filesystem::is_directory(path))
                    path /= "index.html";
                if (!(boost::filesystem::exists(path) &&
                      boost::filesystem::is_regular_file(path)))
                    throw std::invalid_argument("file does not exist");

                std::string cacheControl, etag;

                auto ifs = std::make_shared<std::ifstream>();
                ifs->open(
                    path.string(), std::ifstream::in | std::ios::binary | std::ios::ate);

                if (*ifs)
                {
                    auto length = ifs->tellg();
                    ifs->seekg(0, std::ios::beg);

                    *response << RESPONSE_SUCCESS_STATUS
                              << cacheControl << etag
                              << RESPONSE_HEADER_LENGTH
                              << length
                              << RESPONSE_PLACEHOLD;

                    DefaultResourceSend(*mServer, response, ifs);
                }
                else
                    throw std::invalid_argument("could not read file");

            }
            catch (const std::exception &e)
            {
                std::string content = "Could not open path "+request->path+": "+
                                      e.what();
                *response << RESPONSE_FAILURE_STATUS
                          << RESPONSE_HEADER_LENGTH
                          << content.length()
                          << RESPONSE_PLACEHOLD
                          << content;
            }
        };

    VerifyOrExit(ot::Dbus::WPANController::Start() == ot::Dbus::kWpantundStatus_Ok,
                 ret = ot::Dbus::kWpantundStatus_StartFailed);
    VerifyOrExit(ot::Dbus::WPANController::Leave() == ot::Dbus::kWpantundStatus_Ok,
                 ret = ot::Dbus::kWpantundStatus_LeaveFailed);
exit:

    if (ret != ot::Dbus::kWpantundStatus_Ok)
    {
        syslog(LOG_ERR, "Error is %d\n", ret);
    }
}

void DefaultResourceSend(const HttpServer &aServer, const std::shared_ptr<HttpServer::Response> &aResponse,
                         const std::shared_ptr<std::ifstream> &aIfStream)
{
    static std::vector<char> buffer(131072); // Safe when server is running on one thread

    std::streamsize readLength;

    if ((readLength = aIfStream->read(&buffer[0], buffer.size()).gcount()) > 0)
    {
        aResponse->write(&buffer[0], readLength);
        if (readLength == static_cast<std::streamsize>(buffer.size()))
        {
            aServer.send(aResponse, [&aServer, aResponse, aIfStream](const boost::system::error_code &ec)
                    {
                        if (!ec)
                            DefaultResourceSend(aServer, aResponse, aIfStream);
                        else
                            std::cerr << "Connection interrupted" << std::endl;
                    });
        }
    }
}

} //namespace Web
} //namespace ot
