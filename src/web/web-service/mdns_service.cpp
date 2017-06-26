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
 *   This file implements the mdns service
 */

#include "mdns_service.hpp"

#include "common/code_utils.hpp"

namespace ot {
namespace Web {

std::string MdnsService::HandleMdnsRequest(const std::string &aMdnsRequest)
{
    Json::Value      root;
    Json::FastWriter jsonWriter;
    Json::Reader     reader;
    std::string      response;
    std::string      networkName, extPanId;
    int              ret = kMndsServiceStatus_OK;

    VerifyOrExit(reader.parse(aMdnsRequest.c_str(), root) == true, ret = kMndsServiceStatus_ParseRequestFailed);
    networkName = root["networkName"].asString();
    extPanId = root["extPanId"].asString();
    if (ot::Mdns::Publisher::GetInstance().IsRunning())
    {
        ret = UpdateMdnsService(networkName, extPanId);
    }
    else
    {
        ret = StartMdnsService(networkName, extPanId);
    }
exit:

    root.clear();
    root["result"] = mResponseSuccess;
    root["error"] = ret;
    if (ret != kMndsServiceStatus_OK)
    {
        root["result"] = mResponseFail;
        otbrLog(OTBR_LOG_ERR, "mdns service error %d", ret);
    }

    response = jsonWriter.write(root);
    return response;
}

int MdnsService::StartMdnsService(const std::string &aNetworkName, const std::string &aExtPanId)
{
    int ret = kMndsServiceStatus_OK;

    std::thread mdnsPublisherThread([aNetworkName, aExtPanId, &ret]() {
                std::string networkNameTxt;
                std::string extPanIdTxt;
                ot::Mdns::Publisher::GetInstance().SetServiceName(aNetworkName.c_str());
                ot::Mdns::Publisher::GetInstance().SetType("_meshcop._udp");
                ot::Mdns::Publisher::GetInstance().SetPort(OT_BORDER_ROUTER_PORT);
                networkNameTxt = "nn=" + aNetworkName;
                extPanIdTxt = "xp=" + aExtPanId;
                ot::Mdns::Publisher::GetInstance().SetNetworkNameTxt(networkNameTxt.c_str());
                ot::Mdns::Publisher::GetInstance().SetExtPanIdTxt(extPanIdTxt.c_str());
                ret = ot::Mdns::Publisher::GetInstance().StartClient();
            });

    mdnsPublisherThread.detach();
    return ret;
}

int MdnsService::UpdateMdnsService(const std::string &aNetworkName, const std::string &aExtPanId)
{
    int ret = kMndsServiceStatus_OK;

    std::string networkNameTxt;
    std::string extPanIdTxt;

    ot::Mdns::Publisher::GetInstance().SetServiceName(aNetworkName.c_str());
    ot::Mdns::Publisher::GetInstance().SetType("_meshcop._udp");
    ot::Mdns::Publisher::GetInstance().SetPort(OT_BORDER_ROUTER_PORT);
    networkNameTxt = "nn=" + aNetworkName;
    extPanIdTxt = "xp=" + aExtPanId;
    ot::Mdns::Publisher::GetInstance().SetNetworkNameTxt(networkNameTxt.c_str());
    ot::Mdns::Publisher::GetInstance().SetExtPanIdTxt(extPanIdTxt.c_str());
    ret = ot::Mdns::Publisher::GetInstance().UpdateService();
    return ret;
}

} //namespace Web
} //namespace ot
