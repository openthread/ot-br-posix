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
 *   This file implements the wpan controller service
 */

#include "web/web-service/wpan_service.hpp"

#include <inttypes.h>
#include <sstream>
#include <stdio.h>

#include "common/byteswap.hpp"
#include "common/code_utils.hpp"

namespace otbr {
namespace Web {

const char *WpanService::kBorderAgentHost = "127.0.0.1";
const char *WpanService::kBorderAgentPort = "49191";

#define WPAN_RESPONSE_SUCCESS "successful"
#define WPAN_RESPONSE_FAILURE "failed"

std::string WpanService::HandleJoinNetworkRequest(const std::string &aJoinRequest)
{
    Json::Value                 root;
    Json::Reader                reader;
    Json::FastWriter            jsonWriter;
    std::string                 response;
    int                         index;
    std::string                 masterKey;
    std::string                 prefix;
    bool                        defaultRoute;
    int                         ret = kWpanStatus_Ok;
    otbr::Web::OpenThreadClient client;

    VerifyOrExit(client.Connect(), ret = kWpanStatus_SetFailed);

    VerifyOrExit(reader.parse(aJoinRequest.c_str(), root) == true, ret = kWpanStatus_ParseRequestFailed);
    index        = root["index"].asUInt();
    masterKey    = root["masterKey"].asString();
    prefix       = root["prefix"].asString();
    defaultRoute = root["defaultRoute"].asBool();

    if (prefix.find('/') == std::string::npos)
    {
        prefix += "/64";
    }

    VerifyOrExit(client.FactoryReset(), ret = kWpanStatus_LeaveFailed);
    VerifyOrExit((ret = commitActiveDataset(client, masterKey, mNetworks[index].mNetworkName, mNetworks[index].mChannel,
                                            mNetworks[index].mExtPanId, mNetworks[index].mPanId)) == kWpanStatus_Ok);
    VerifyOrExit(client.Execute("ifconfig up") != nullptr, ret = kWpanStatus_JoinFailed);
    VerifyOrExit(client.Execute("thread start") != nullptr, ret = kWpanStatus_JoinFailed);
    VerifyOrExit(client.Execute("prefix add %s paso%s", prefix.c_str(), (defaultRoute ? "r" : "")) != nullptr,
                 ret = kWpanStatus_SetFailed);
exit:

    root.clear();
    root["result"] = WPAN_RESPONSE_SUCCESS;

    root["error"] = ret;
    if (ret != kWpanStatus_Ok)
    {
        otbrLog(OTBR_LOG_ERR, "wpan service error: %d", ret);
        root["result"] = WPAN_RESPONSE_FAILURE;
    }
    response = jsonWriter.write(root);
    return response;
}

std::string WpanService::HandleFormNetworkRequest(const std::string &aFormRequest)
{
    Json::Value                 root;
    Json::FastWriter            jsonWriter;
    Json::Reader                reader;
    std::string                 response;
    otbr::Psk::Pskc             psk;
    char                        pskcStr[OT_PSKC_MAX_LENGTH * 2 + 1];
    uint8_t                     extPanIdBytes[OT_EXTENDED_PANID_LENGTH];
    std::string                 masterKey;
    std::string                 prefix;
    uint16_t                    channel;
    std::string                 networkName;
    std::string                 passphrase;
    uint16_t                    panId;
    uint64_t                    extPanId;
    bool                        defaultRoute;
    int                         ret = kWpanStatus_Ok;
    otbr::Web::OpenThreadClient client;

    VerifyOrExit(client.Connect(), ret = kWpanStatus_SetFailed);

    pskcStr[OT_PSKC_MAX_LENGTH * 2] = '\0'; // for manipulating with strlen
    VerifyOrExit(reader.parse(aFormRequest.c_str(), root) == true, ret = kWpanStatus_ParseRequestFailed);
    masterKey   = root["masterKey"].asString();
    prefix      = root["prefix"].asString();
    channel     = root["channel"].asUInt();
    networkName = root["networkName"].asString();
    passphrase  = root["passphrase"].asString();
    VerifyOrExit(sscanf(root["panId"].asString().c_str(), "%hx", &panId) == 1, ret = kWpanStatus_ParseRequestFailed);
    VerifyOrExit(sscanf(root["extPanId"].asString().c_str(), "%" PRIx64, &extPanId) == 1,
                 ret = kWpanStatus_ParseRequestFailed);
    defaultRoute = root["defaultRoute"].asBool();

    otbr::Utils::Hex2Bytes(root["extPanId"].asString().c_str(), extPanIdBytes, OT_EXTENDED_PANID_LENGTH);
    otbr::Utils::Bytes2Hex(psk.ComputePskc(extPanIdBytes, networkName.c_str(), passphrase.c_str()), OT_PSKC_MAX_LENGTH,
                           pskcStr);

    if (prefix.find('/') == std::string::npos)
    {
        prefix += "/64";
    }

    VerifyOrExit(client.FactoryReset(), ret = kWpanStatus_LeaveFailed);
    VerifyOrExit((ret = commitActiveDataset(client, masterKey, networkName, channel, extPanId, panId)) ==
                 kWpanStatus_Ok);
    VerifyOrExit(client.Execute("pskc %s", pskcStr) != nullptr, ret = kWpanStatus_SetFailed);
    VerifyOrExit(client.Execute("ifconfig up") != nullptr, ret = kWpanStatus_FormFailed);
    VerifyOrExit(client.Execute("thread start") != nullptr, ret = kWpanStatus_FormFailed);
    VerifyOrExit(client.Execute("prefix add %s paso%s", prefix.c_str(), (defaultRoute ? "r" : "")) != nullptr,
                 ret = kWpanStatus_SetFailed);
exit:

    root.clear();

    root["result"] = WPAN_RESPONSE_SUCCESS;
    root["error"]  = ret;
    if (ret != kWpanStatus_Ok)
    {
        otbrLog(OTBR_LOG_ERR, "wpan service error: %d", ret);
        root["result"] = WPAN_RESPONSE_FAILURE;
    }
    response = jsonWriter.write(root);
    return response;
}

std::string WpanService::HandleAddPrefixRequest(const std::string &aAddPrefixRequest)
{
    Json::Value                 root;
    Json::FastWriter            jsonWriter;
    Json::Reader                reader;
    std::string                 response;
    std::string                 prefix;
    bool                        defaultRoute;
    int                         ret = kWpanStatus_Ok;
    otbr::Web::OpenThreadClient client;

    VerifyOrExit(client.Connect(), ret = kWpanStatus_SetFailed);

    VerifyOrExit(reader.parse(aAddPrefixRequest.c_str(), root) == true, ret = kWpanStatus_ParseRequestFailed);
    prefix       = root["prefix"].asString();
    defaultRoute = root["defaultRoute"].asBool();

    VerifyOrExit(client.Execute("prefix add %s paso%s", prefix.c_str(), (defaultRoute ? "r" : "")) != nullptr,
                 ret = kWpanStatus_SetGatewayFailed);
exit:

    root.clear();

    root["result"] = WPAN_RESPONSE_SUCCESS;
    root["error"]  = ret;
    if (ret != kWpanStatus_Ok)
    {
        otbrLog(OTBR_LOG_ERR, "wpan service error: %d", ret);
        root["result"] = WPAN_RESPONSE_FAILURE;
    }
    response = jsonWriter.write(root);
    return response;
}

std::string WpanService::HandleDeletePrefixRequest(const std::string &aDeleteRequest)
{
    Json::Value                 root;
    Json::FastWriter            jsonWriter;
    Json::Reader                reader;
    std::string                 response;
    std::string                 prefix;
    int                         ret = kWpanStatus_Ok;
    otbr::Web::OpenThreadClient client;

    VerifyOrExit(client.Connect(), ret = kWpanStatus_SetFailed);

    VerifyOrExit(reader.parse(aDeleteRequest.c_str(), root) == true, ret = kWpanStatus_ParseRequestFailed);
    prefix = root["prefix"].asString();

    VerifyOrExit(client.Execute("prefix remove %s", prefix.c_str()) != nullptr, ret = kWpanStatus_SetGatewayFailed);
exit:

    root.clear();
    root["result"] = WPAN_RESPONSE_SUCCESS;

    root["error"] = ret;
    if (ret != kWpanStatus_Ok)
    {
        otbrLog(OTBR_LOG_ERR, "wpan service error: %d", ret);
        root["result"] = WPAN_RESPONSE_FAILURE;
    }
    response = jsonWriter.write(root);
    return response;
}

std::string WpanService::HandleStatusRequest()
{
    Json::Value                 root, networkInfo;
    Json::FastWriter            jsonWriter;
    std::string                 response, networkName, extPanId, propertyValue;
    int                         ret = kWpanStatus_Ok;
    otbr::Web::OpenThreadClient client;
    char *                      rval;

    networkInfo["WPAN service"] = "uninitialized";
    VerifyOrExit(client.Connect(), ret = kWpanStatus_SetFailed);

    VerifyOrExit((rval = client.Execute("state")) != nullptr, ret = kWpanStatus_GetPropertyFailed);
    networkInfo["NCP:State"] = rval;

    if (!strcmp(rval, "disabled"))
    {
        networkInfo["WPAN service"] = "offline";
        ExitNow();
    }
    else if (!strcmp(rval, "detached"))
    {
        networkInfo["WPAN service"] = "associating";
        ExitNow();
    }
    else
    {
        networkInfo["WPAN service"] = "associated";
    }

    VerifyOrExit((rval = client.Execute("version")) != nullptr, ret = kWpanStatus_GetPropertyFailed);
    networkInfo["NCP:Version"] = rval;

    VerifyOrExit((rval = client.Execute("eui64")) != nullptr, ret = kWpanStatus_GetPropertyFailed);
    networkInfo["NCP:HardwareAddress"] = rval;

    VerifyOrExit((rval = client.Execute("channel")) != nullptr, ret = kWpanStatus_GetPropertyFailed);
    networkInfo["NCP:Channel"] = rval;

    VerifyOrExit((rval = client.Execute("state")) != nullptr, ret = kWpanStatus_GetPropertyFailed);
    networkInfo["Network:NodeType"] = rval;

    VerifyOrExit((rval = client.Execute("networkname")) != nullptr, ret = kWpanStatus_GetPropertyFailed);
    networkInfo["Network:Name"] = rval;

    VerifyOrExit((rval = client.Execute("extpanid")) != nullptr, ret = kWpanStatus_GetPropertyFailed);
    networkInfo["Network:XPANID"] = rval;

    VerifyOrExit((rval = client.Execute("panid")) != nullptr, ret = kWpanStatus_GetPropertyFailed);
    networkInfo["Network:PANID"] = rval;

    {
        static const char kMeshLocalPrefixLocator[]       = "Mesh Local Prefix: ";
        static const char kMeshLocalAddressTokenLocator[] = "0:ff:fe00:";
        std::string       meshLocalPrefix;

        VerifyOrExit((rval = client.Execute("dataset active")) != nullptr, ret = kWpanStatus_GetPropertyFailed);
        rval = strstr(rval, kMeshLocalPrefixLocator);
        rval += sizeof(kMeshLocalPrefixLocator) - 1;
        *strstr(rval, "\r\n") = '\0';

        networkInfo["IPv6:MeshLocalPrefix"] = rval;

        meshLocalPrefix = rval;
        meshLocalPrefix.resize(meshLocalPrefix.find(":/"));

        VerifyOrExit((rval = client.Execute("ipaddr")) != nullptr, ret = kWpanStatus_GetPropertyFailed);

        for (rval = strtok(rval, "\r\n"); rval != nullptr; rval = strtok(nullptr, "\r\n"))
        {
            char *meshLocalAddressToken = nullptr;

            if (strstr(rval, meshLocalPrefix.c_str()) != rval)
            {
                continue;
            }

            meshLocalAddressToken = strstr(rval, kMeshLocalAddressTokenLocator);

            if (meshLocalAddressToken == nullptr)
            {
                break;
            }

            // In case this address is not ends with 0:ff:fe00:xxxx
            if (strchr(meshLocalAddressToken + sizeof(kMeshLocalAddressTokenLocator) - 1, ':') != nullptr)
            {
                break;
            }
        }

        networkInfo["IPv6:MeshLocalAddress"] = rval;
    }

exit:
    root["result"] = networkInfo;

    if (ret != kWpanStatus_Ok)
    {
        root["result"] = WPAN_RESPONSE_FAILURE;
        otbrLog(OTBR_LOG_ERR, "wpan service error: %d", ret);
    }
    root["error"] = ret;
    response      = jsonWriter.write(root);
    return response;
}

std::string WpanService::HandleAvailableNetworkRequest()
{
    Json::Value                 root, networks, networkInfo;
    Json::FastWriter            jsonWriter;
    std::string                 response;
    int                         ret = kWpanStatus_Ok;
    otbr::Web::OpenThreadClient client;

    VerifyOrExit(client.Connect(), ret = kWpanStatus_ScanFailed);
    VerifyOrExit((mNetworksCount = client.Scan(mNetworks, sizeof(mNetworks) / sizeof(mNetworks[0]))) > 0,
                 ret = kWpanStatus_NetworkNotFound);

    for (int i = 0; i < mNetworksCount; i++)
    {
        char extPanId[OT_EXTENDED_PANID_LENGTH * 2 + 1], panId[OT_PANID_LENGTH * 2 + 3],
            hardwareAddress[OT_HARDWARE_ADDRESS_LENGTH * 2 + 1];
        otbr::Utils::Long2Hex(bswap_64(mNetworks[i].mExtPanId), extPanId);
        otbr::Utils::Bytes2Hex(mNetworks[i].mHardwareAddress, OT_HARDWARE_ADDRESS_LENGTH, hardwareAddress);
        sprintf(panId, "0x%X", mNetworks[i].mPanId);
        networkInfo[i]["nn"] = mNetworks[i].mNetworkName;
        networkInfo[i]["xp"] = extPanId;
        networkInfo[i]["pi"] = panId;
        networkInfo[i]["ch"] = mNetworks[i].mChannel;
        networkInfo[i]["ha"] = hardwareAddress;
    }

    root["result"] = networkInfo;

exit:
    if (ret != kWpanStatus_Ok)
    {
        root["result"] = WPAN_RESPONSE_FAILURE;
        otbrLog(OTBR_LOG_ERR, "Error is %d", ret);
    }
    root["error"] = ret;
    response      = jsonWriter.write(root);
    return response;
}

int WpanService::GetWpanServiceStatus(std::string &aNetworkName, std::string &aExtPanId) const
{
    int                         status = kWpanStatus_Ok;
    otbr::Web::OpenThreadClient client;
    const char *                rval;

    VerifyOrExit(client.Connect(), status = kWpanStatus_Uninitialized);
    rval = client.Execute("state");
    VerifyOrExit(rval != nullptr, status = kWpanStatus_Down);
    if (!strcmp(rval, "disabled"))
    {
        status = kWpanStatus_Offline;
    }
    else if (!strcmp(rval, "detached"))
    {
        status = kWpanStatus_Associating;
    }
    else
    {
        rval = client.Execute("networkname");
        VerifyOrExit(rval != nullptr, status = kWpanStatus_Down);
        aNetworkName = rval;

        rval = client.Execute("extpanid");
        VerifyOrExit(rval != nullptr, status = kWpanStatus_Down);
        aExtPanId = rval;
    }

exit:

    return status;
}

std::string WpanService::HandleCommission(const std::string &aCommissionRequest)
{
    Json::Value  root;
    Json::Reader reader;
    int          ret = kWpanStatus_Ok;
    std::string  pskd;
    std::string  response;

    VerifyOrExit(reader.parse(aCommissionRequest.c_str(), root) == true, ret = kWpanStatus_ParseRequestFailed);
    pskd = root["pskd"].asString();
    {
        otbr::Web::OpenThreadClient client;
        const char *                rval;

        VerifyOrExit(client.Connect(), ret = kWpanStatus_Uninitialized);
        rval = client.Execute("commissioner start");
        VerifyOrExit(rval != nullptr, ret = kWpanStatus_Down);
        rval = client.Execute("commissioner joiner add * %s", pskd.c_str());
        VerifyOrExit(rval != nullptr, ret = kWpanStatus_Down);
        root["error"] = ret;
    }
exit:
    if (ret != kWpanStatus_Ok)
    {
        root["result"] = WPAN_RESPONSE_FAILURE;
        otbrLog(OTBR_LOG_ERR, "error: %d", ret);
    }
    return response;
}

int WpanService::commitActiveDataset(otbr::Web::OpenThreadClient &aClient,
                                     const std::string &          aMasterKey,
                                     const std::string &          aNetworkName,
                                     uint16_t                     aChannel,
                                     uint64_t                     aExtPanId,
                                     uint16_t                     aPanId)
{
    int ret = kWpanStatus_Ok;

    VerifyOrExit(aClient.Execute("dataset init new") != nullptr, ret = kWpanStatus_SetFailed);
    VerifyOrExit(aClient.Execute("dataset masterkey %s", aMasterKey.c_str()) != nullptr, ret = kWpanStatus_SetFailed);
    VerifyOrExit(aClient.Execute("dataset networkname %s", escapeOtCliEscapable(aNetworkName).c_str()) != nullptr,
                 ret = kWpanStatus_SetFailed);
    VerifyOrExit(aClient.Execute("dataset channel %u", aChannel) != nullptr, ret = kWpanStatus_SetFailed);
    VerifyOrExit(aClient.Execute("dataset extpanid %016" PRIx64, aExtPanId) != nullptr, ret = kWpanStatus_SetFailed);
    VerifyOrExit(aClient.Execute("dataset panid %u", aPanId) != nullptr, ret = kWpanStatus_SetFailed);
    VerifyOrExit(aClient.Execute("dataset commit active") != nullptr, ret = kWpanStatus_SetFailed);

exit:
    return ret;
}

std::string WpanService::escapeOtCliEscapable(const std::string &aArg)
{
    std::stringbuf strbuf;

    for (char c : aArg)
    {
        switch (c)
        {
        case ' ':
        case '\t':
        case '\r':
        case '\n':
        case '\\':
            strbuf.sputc('\\');
            // Fallthrough
        default:
            strbuf.sputc(c);
        }
    }

    return strbuf.str();
}

} // namespace Web
} // namespace otbr
