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

#include "wpan_service.hpp"

#include <inttypes.h>

#include "ot_client.hpp"
#include "common/code_utils.hpp"

namespace ot {
namespace Web {

const char *WpanService::kBorderAgentHost = "127.0.0.1";
const char *WpanService::kBorderAgentPort = "49191";

std::string WpanService::HandleJoinNetworkRequest(const std::string &aJoinRequest)
{
    Json::Value      root;
    Json::Reader     reader;
    Json::FastWriter jsonWriter;
    std::string      response;
    int              index;
    std::string      networkKey;
    std::string      prefix;
    bool             defaultRoute;
    int              ret = ot::Dbus::kWpantundStatus_Ok;
#if OTBR_ENABLE_NCP_WPANTUND
    ot::Dbus::WPANController wpanController;
#else
    ot::Client client;

    VerifyOrExit(client.Connect(), ret = ot::Dbus::kWpantundStatus_SetFailed);
#endif

    VerifyOrExit(reader.parse(aJoinRequest.c_str(), root) == true, ret = kWpanStatus_ParseRequestFailed);
    index        = root["index"].asUInt();
    networkKey   = root["networkKey"].asString();
    prefix       = root["prefix"].asString();
    defaultRoute = root["defaultRoute"].asBool();

#if OTBR_ENABLE_NCP_WPANTUND
    wpanController.SetInterfaceName(mIfName);
    VerifyOrExit(wpanController.Leave() == ot::Dbus::kWpantundStatus_Ok, ret = ot::Dbus::kWpantundStatus_LeaveFailed);
    VerifyOrExit(wpanController.Set(kPropertyType_Data, "Network:Key", networkKey.c_str()) ==
                     ot::Dbus::kWpantundStatus_Ok,
                 ret = ot::Dbus::kWpantundStatus_SetFailed);
    VerifyOrExit(wpanController.Join(mNetworks[index].mNetworkName, mNetworks[index].mChannel,
                                     mNetworks[index].mExtPanId,
                                     mNetworks[index].mPanId) == ot::Dbus::kWpantundStatus_Ok,
                 ret = ot::Dbus::kWpantundStatus_JoinFailed);
    VerifyOrExit(wpanController.AddGateway(prefix.c_str(), defaultRoute) == ot::Dbus::kWpantundStatus_Ok,
                 ret = ot::Dbus::kWpantundStatus_SetGatewayFailed);
#else  // OTBR_ENABLE_NCP_WPANTUND
    if (prefix.find('/') == std::string::npos)
    {
        prefix += "/64";
    }

    VerifyOrExit(client.FactoryReset(), ret = ot::Dbus::kWpantundStatus_LeaveFailed);
    VerifyOrExit(client.Execute("masterkey %s", networkKey.c_str()) != NULL, ret = ot::Dbus::kWpantundStatus_SetFailed);
    VerifyOrExit(client.Execute("networkname %s", mNetworks[index].mNetworkName) != NULL,
                 ret = ot::Dbus::kWpantundStatus_SetFailed);
    VerifyOrExit(client.Execute("channel %u", mNetworks[index].mChannel) != NULL,
                 ret = ot::Dbus::kWpantundStatus_SetFailed);
    VerifyOrExit(client.Execute("extpanid %016" PRIx64, mNetworks[index].mExtPanId) != NULL,
                 ret = ot::Dbus::kWpantundStatus_SetFailed);
    VerifyOrExit(client.Execute("panid %u", mNetworks[index].mPanId) != NULL,
                 ret = ot::Dbus::kWpantundStatus_SetFailed);
    VerifyOrExit(client.Execute("ifconfig up") != NULL, ret = ot::Dbus::kWpantundStatus_JoinFailed);
    VerifyOrExit(client.Execute("thread start") != NULL, ret = ot::Dbus::kWpantundStatus_JoinFailed);
    VerifyOrExit(client.Execute("prefix add %s paso%s", prefix.c_str(), (defaultRoute ? "r" : "")) != NULL,
                 ret = ot::Dbus::kWpantundStatus_SetFailed);
#endif // OTBR_ENABLE_NCP_WPANTUND
exit:

    root.clear();
    root["result"] = mResponseSuccess;

    root["error"] = ret;
    if (ret != ot::Dbus::kWpantundStatus_Ok)
    {
        otbrLog(OTBR_LOG_ERR, "wpan service error: %d", ret);
        root["result"] = mResponseFail;
    }
    response = jsonWriter.write(root);
    return response;
}

std::string WpanService::HandleFormNetworkRequest(const std::string &aFormRequest)
{
    Json::Value      root;
    Json::FastWriter jsonWriter;
    Json::Reader     reader;
    std::string      response;
    ot::Psk::Pskc    psk;
    char             pskcStr[OT_PSKC_MAX_LENGTH * 2 + 1];
    uint8_t          extPanIdBytes[OT_EXTENDED_PANID_LENGTH];
    std::string      networkKey;
    std::string      prefix;
    uint16_t         channel;
    std::string      networkName;
    std::string      passphrase;
    std::string      panId;
    std::string      extPanId;
    bool             defaultRoute;
    int              ret = ot::Dbus::kWpantundStatus_Ok;
#if OTBR_ENABLE_NCP_WPANTUND
    ot::Dbus::WPANController wpanController;
#else
    ot::Client client;

    VerifyOrExit(client.Connect(), ret = ot::Dbus::kWpantundStatus_SetFailed);
#endif

    pskcStr[OT_PSKC_MAX_LENGTH * 2] = '\0'; // for manipulating with strlen
    VerifyOrExit(reader.parse(aFormRequest.c_str(), root) == true, ret = kWpanStatus_ParseRequestFailed);
    networkKey   = root["networkKey"].asString();
    prefix       = root["prefix"].asString();
    channel      = root["channel"].asUInt();
    networkName  = root["networkName"].asString();
    passphrase   = root["passphrase"].asString();
    panId        = root["panId"].asString();
    extPanId     = root["extPanId"].asString();
    defaultRoute = root["defaultRoute"].asBool();

    ot::Utils::Hex2Bytes(extPanId.c_str(), extPanIdBytes, OT_EXTENDED_PANID_LENGTH);
    ot::Utils::Bytes2Hex(psk.ComputePskc(extPanIdBytes, networkName.c_str(), passphrase.c_str()), OT_PSKC_MAX_LENGTH,
                         pskcStr);

#if OTBR_ENABLE_NCP_WPANTUND
    wpanController.SetInterfaceName(mIfName);
    VerifyOrExit(wpanController.Leave() == ot::Dbus::kWpantundStatus_Ok, ret = ot::Dbus::kWpantundStatus_LeaveFailed);

    VerifyOrExit(wpanController.Set(kPropertyType_Data, kWPANTUNDProperty_NetworkKey, networkKey.c_str()) ==
                     ot::Dbus::kWpantundStatus_Ok,
                 ret = ot::Dbus::kWpantundStatus_SetFailed);

    VerifyOrExit(wpanController.Set(kPropertyType_String, kWPANTUNDProperty_NetworkPANID, panId.c_str()) ==
                     ot::Dbus::kWpantundStatus_Ok,
                 ret = ot::Dbus::kWpantundStatus_SetFailed);
    VerifyOrExit(wpanController.Set(kPropertyType_Data, kWPANTUNDProperty_NetworkXPANID, extPanId.c_str()) ==
                     ot::Dbus::kWpantundStatus_Ok,
                 ret = ot::Dbus::kWpantundStatus_SetFailed);
    VerifyOrExit(wpanController.Set(kPropertyType_Data, kWPANTUNDProperty_NetworkPSKc, pskcStr) ==
                     ot::Dbus::kWpantundStatus_Ok,
                 ret = ot::Dbus::kWpantundStatus_SetFailed);

    VerifyOrExit(wpanController.Form(networkName.c_str(), channel) == ot::Dbus::kWpantundStatus_Ok,
                 ret = ot::Dbus::kWpantundStatus_FormFailed);

    VerifyOrExit(wpanController.AddGateway(prefix.c_str(), defaultRoute) == ot::Dbus::kWpantundStatus_Ok,
                 ret = ot::Dbus::kWpantundStatus_SetGatewayFailed);
#else  // OTBR_ENABLE_NCP_WPANTUND
    if (prefix.find('/') == std::string::npos)
    {
        prefix += "/64";
    }

    VerifyOrExit(client.FactoryReset(), ret = ot::Dbus::kWpantundStatus_LeaveFailed);
    VerifyOrExit(client.Execute("masterkey %s", networkKey.c_str()) != NULL, ret = ot::Dbus::kWpantundStatus_SetFailed);
    VerifyOrExit(client.Execute("networkname %s", networkName.c_str()) != NULL,
                 ret = ot::Dbus::kWpantundStatus_SetFailed);
    VerifyOrExit(client.Execute("channel %u", channel) != NULL, ret = ot::Dbus::kWpantundStatus_SetFailed);
    VerifyOrExit(client.Execute("extpanid %s", extPanId.c_str()) != NULL, ret = ot::Dbus::kWpantundStatus_SetFailed);
    VerifyOrExit(client.Execute("panid %s", panId.c_str()) != NULL, ret = ot::Dbus::kWpantundStatus_SetFailed);
    VerifyOrExit(client.Execute("pskc %s", pskcStr) != NULL, ret = ot::Dbus::kWpantundStatus_SetFailed);
    VerifyOrExit(client.Execute("ifconfig up") != NULL, ret = ot::Dbus::kWpantundStatus_FormFailed);
    VerifyOrExit(client.Execute("thread start") != NULL, ret = ot::Dbus::kWpantundStatus_FormFailed);
    VerifyOrExit(client.Execute("prefix add %s paso%s", prefix.c_str(), (defaultRoute ? "r" : "")) != NULL,
                 ret = ot::Dbus::kWpantundStatus_SetFailed);
#endif // OTBR_ENABLE_NCP_WPANTUND
exit:

    root.clear();

    root["result"] = mResponseSuccess;
    root["error"]  = ret;
    if (ret != ot::Dbus::kWpantundStatus_Ok)
    {
        otbrLog(OTBR_LOG_ERR, "wpan service error: %d", ret);
        root["result"] = mResponseFail;
    }
    response = jsonWriter.write(root);
    return response;
}

std::string WpanService::HandleAddPrefixRequest(const std::string &aAddPrefixRequest)
{
    Json::Value      root;
    Json::FastWriter jsonWriter;
    Json::Reader     reader;
    std::string      response;
    std::string      prefix;
    bool             defaultRoute;
    int              ret = ot::Dbus::kWpantundStatus_Ok;
#if OTBR_ENABLE_NCP_WPANTUND
    ot::Dbus::WPANController wpanController;
#else
    ot::Client client;

    VerifyOrExit(client.Connect(), ret = ot::Dbus::kWpantundStatus_SetFailed);
#endif

    VerifyOrExit(reader.parse(aAddPrefixRequest.c_str(), root) == true, ret = kWpanStatus_ParseRequestFailed);
    prefix       = root["prefix"].asString();
    defaultRoute = root["defaultRoute"].asBool();

#if OTBR_ENABLE_NCP_WPANTUND
    wpanController.SetInterfaceName(mIfName);
    VerifyOrExit(wpanController.AddGateway(prefix.c_str(), defaultRoute) == ot::Dbus::kWpantundStatus_Ok,
                 ret = ot::Dbus::kWpantundStatus_SetGatewayFailed);
#else
    VerifyOrExit(client.Execute("prefix add %s paso%s", prefix.c_str(), (defaultRoute ? "r" : "")) != NULL,
                 ret = ot::Dbus::kWpantundStatus_SetGatewayFailed);
#endif
exit:

    root.clear();

    root["result"] = mResponseSuccess;
    root["error"]  = ret;
    if (ret != ot::Dbus::kWpantundStatus_Ok)
    {
        otbrLog(OTBR_LOG_ERR, "wpan service error: %d", ret);
        root["result"] = mResponseFail;
    }
    response = jsonWriter.write(root);
    return response;
}

std::string WpanService::HandleDeletePrefixRequest(const std::string &aDeleteRequest)
{
    Json::Value      root;
    Json::FastWriter jsonWriter;
    Json::Reader     reader;
    std::string      response;
    std::string      prefix;
    int              ret = ot::Dbus::kWpantundStatus_Ok;
#if OTBR_ENABLE_NCP_WPANTUND
    ot::Dbus::WPANController wpanController;
#else
    ot::Client client;

    VerifyOrExit(client.Connect(), ret = ot::Dbus::kWpantundStatus_SetFailed);
#endif

    VerifyOrExit(reader.parse(aDeleteRequest.c_str(), root) == true, ret = kWpanStatus_ParseRequestFailed);
    prefix = root["prefix"].asString();

#if OTBR_ENABLE_NCP_WPANTUND
    wpanController.SetInterfaceName(mIfName);
    VerifyOrExit(wpanController.RemoveGateway(prefix.c_str()) == ot::Dbus::kWpantundStatus_Ok,
                 ret = ot::Dbus::kWpantundStatus_SetGatewayFailed);
#else
    VerifyOrExit(client.Execute("prefix remove %s", prefix.c_str()) != NULL,
                 ret = ot::Dbus::kWpantundStatus_SetGatewayFailed);
#endif
exit:

    root.clear();
    root["result"] = mResponseSuccess;

    root["error"] = ret;
    if (ret != ot::Dbus::kWpantundStatus_Ok)
    {
        otbrLog(OTBR_LOG_ERR, "wpan service error: %d", ret);
        root["result"] = mResponseFail;
    }
    response = jsonWriter.write(root);
    return response;
}

std::string WpanService::HandleStatusRequest()
{
    Json::Value      root, networkInfo;
    Json::FastWriter jsonWriter;
    std::string      response, networkName, extPanId, propertyValue;
    int              ret = kWpanStatus_OK;
#if OTBR_ENABLE_NCP_WPANTUND
    ot::Dbus::WPANController wpanController;

    switch (GetWpanServiceStatus(networkName, extPanId))
    {
    case kWpanStatus_OK:
        wpanController.SetInterfaceName(mIfName);
        propertyValue = wpanController.Get(kWPANTUNDProperty_NCPState);
        VerifyOrExit(propertyValue.length() > 0, ret = kWpanStatus_GetPropertyFailed);
        networkInfo[kWPANTUNDProperty_NCPState] = propertyValue;
        propertyValue                           = wpanController.Get(kWPANTUNDProperty_DaemonEnabled);
        VerifyOrExit(propertyValue.length() > 0, ret = kWpanStatus_GetPropertyFailed);
        networkInfo[kWPANTUNDProperty_DaemonEnabled] = propertyValue;
        propertyValue                                = wpanController.Get(kWPANTUNDProperty_NCPVersion);
        VerifyOrExit(propertyValue.length() > 0, ret = kWpanStatus_GetPropertyFailed);
        networkInfo[kWPANTUNDProperty_NCPVersion] = propertyValue;
        propertyValue                             = wpanController.Get(kWPANTUNDProperty_DaemonVersion);
        VerifyOrExit(propertyValue.length() > 0, ret = kWpanStatus_GetPropertyFailed);
        networkInfo[kWPANTUNDProperty_DaemonVersion] = propertyValue;
        propertyValue                                = wpanController.Get(kWPANTUNDProperty_ConfigNCPDriverName);
        VerifyOrExit(propertyValue.length() > 0, ret = kWpanStatus_GetPropertyFailed);
        networkInfo[kWPANTUNDProperty_ConfigNCPDriverName] = propertyValue;
        propertyValue                                      = wpanController.Get(kWPANTUNDProperty_NCPHardwareAddress);
        VerifyOrExit(propertyValue.length() > 0, ret = kWpanStatus_GetPropertyFailed);
        networkInfo[kWPANTUNDProperty_NCPHardwareAddress] = propertyValue;
        propertyValue                                     = wpanController.Get(kWPANTUNDProperty_NCPChannel);
        VerifyOrExit(propertyValue.length() > 0, ret = kWpanStatus_GetPropertyFailed);
        networkInfo[kWPANTUNDProperty_NCPChannel] = propertyValue;
        propertyValue                             = wpanController.Get(kWPANTUNDProperty_NetworkNodeType);
        VerifyOrExit(propertyValue.length() > 0, ret = kWpanStatus_GetPropertyFailed);
        networkInfo[kWPANTUNDProperty_NetworkNodeType] = propertyValue;
        propertyValue                                  = wpanController.Get(kWPANTUNDProperty_NetworkName);
        VerifyOrExit(propertyValue.length() > 0, ret = kWpanStatus_GetPropertyFailed);
        networkInfo[kWPANTUNDProperty_NetworkName] = propertyValue;
        propertyValue                              = wpanController.Get(kWPANTUNDProperty_NetworkXPANID);
        VerifyOrExit(propertyValue.length() > 0, ret = kWpanStatus_GetPropertyFailed);
        networkInfo[kWPANTUNDProperty_NetworkXPANID] = propertyValue;
        propertyValue                                = wpanController.Get(kWPANTUNDProperty_NetworkPANID);
        VerifyOrExit(propertyValue.length() > 0, ret = kWpanStatus_GetPropertyFailed);
        networkInfo[kWPANTUNDProperty_NetworkPANID] = propertyValue;
        propertyValue                               = wpanController.Get(kWPANTUNDProperty_IPv6LinkLocalAddress);
        VerifyOrExit(propertyValue.length() > 0, ret = kWpanStatus_GetPropertyFailed);
        networkInfo[kWPANTUNDProperty_IPv6LinkLocalAddress] = propertyValue;
        propertyValue = wpanController.Get(kWPANTUNDProperty_IPv6MeshLocalAddress);
        VerifyOrExit(propertyValue.length() > 0, ret = kWpanStatus_GetPropertyFailed);
        networkInfo[kWPANTUNDProperty_IPv6MeshLocalAddress] = propertyValue;
        propertyValue                                       = wpanController.Get(kWPANTUNDProperty_IPv6MeshLocalPrefix);
        VerifyOrExit(propertyValue.length() > 0, ret = kWpanStatus_GetPropertyFailed);
        networkInfo[kWPANTUNDProperty_IPv6MeshLocalPrefix] = propertyValue;
        break;

    case kWpanStatus_Offline:
        networkInfo["WPAN service"] = kWPANTUNDStateOffline;
        break;

    case kWpanStatus_Down:
    default:
        networkInfo["wpantund"]     = mServiceDown;
        networkInfo["WPAN service"] = kWPANTUNDStateUninitialized;
        break;
    }
#else
    ot::Client client;
    char *     rval;

    networkInfo["WPAN service"] = kWPANTUNDStateUninitialized;
    VerifyOrExit(client.Connect(), ret = ot::Dbus::kWpantundStatus_SetFailed);

    VerifyOrExit((rval = client.Execute("state")) != NULL, ret = kWpanStatus_GetPropertyFailed);
    networkInfo[kWPANTUNDProperty_NCPState] = rval;

    if (!strcmp(rval, "disabled"))
    {
        networkInfo["WPAN service"] = kWPANTUNDStateOffline;
        ExitNow();
    }
    else if (!strcmp(rval, "detached"))
    {
        networkInfo["WPAN service"] = kWPANTUNDStateAssociating;
        ExitNow();
    }
    else
    {
        networkInfo["WPAN service"] = kWPANTUNDStateAssociated;
    }

    VerifyOrExit((rval = client.Execute("version")) != NULL, ret = kWpanStatus_GetPropertyFailed);
    networkInfo[kWPANTUNDProperty_NCPVersion] = rval;

    VerifyOrExit((rval = client.Execute("eui64")) != NULL, ret = kWpanStatus_GetPropertyFailed);
    networkInfo[kWPANTUNDProperty_NCPHardwareAddress] = rval;

    VerifyOrExit((rval = client.Execute("channel")) != NULL, ret = kWpanStatus_GetPropertyFailed);
    networkInfo[kWPANTUNDProperty_NCPChannel] = rval;

    VerifyOrExit((rval = client.Execute("state")) != NULL, ret = kWpanStatus_GetPropertyFailed);
    networkInfo[kWPANTUNDProperty_NetworkNodeType] = rval;

    VerifyOrExit((rval = client.Execute("networkname")) != NULL, ret = kWpanStatus_GetPropertyFailed);
    networkInfo[kWPANTUNDProperty_NetworkName] = rval;

    VerifyOrExit((rval = client.Execute("extpanid")) != NULL, ret = kWpanStatus_GetPropertyFailed);
    networkInfo[kWPANTUNDProperty_NetworkXPANID] = rval;

    VerifyOrExit((rval = client.Execute("panid")) != NULL, ret = kWpanStatus_GetPropertyFailed);
    networkInfo[kWPANTUNDProperty_NetworkPANID] = rval;

    {
        static const char kMeshLocalPrefixLocator[]       = "Mesh Local Prefix: ";
        static const char kMeshLocalAddressTokenLocator[] = "0:ff:fe00:";
        std::string       meshLocalPrefix;

        VerifyOrExit((rval = client.Execute("dataset active")) != NULL, ret = kWpanStatus_GetPropertyFailed);
        rval = strstr(rval, kMeshLocalPrefixLocator);
        rval += sizeof(kMeshLocalPrefixLocator) - 1;
        *strstr(rval, "\r\n") = '\0';

        networkInfo[kWPANTUNDProperty_IPv6MeshLocalPrefix] = rval;

        meshLocalPrefix = rval;
        meshLocalPrefix.resize(meshLocalPrefix.find('/'));

        VerifyOrExit((rval = client.Execute("ipaddr")) != NULL, ret = kWpanStatus_GetPropertyFailed);

        for (rval = strtok(rval, "\r\n"); rval != NULL; rval = strtok(NULL, "\r\n"))
        {
            char *meshLocalAddressToken = NULL;

            if (strstr(rval, meshLocalPrefix.c_str()) != rval)
            {
                continue;
            }

            meshLocalAddressToken = strstr(rval, kMeshLocalAddressTokenLocator);

            if (meshLocalAddressToken == NULL)
            {
                break;
            }

            // In case this address is not ends with 0:ff:fe00:xxxx
            if (strchr(meshLocalAddressToken + sizeof(kMeshLocalAddressTokenLocator) - 1, ':') != NULL)
            {
                break;
            }
        }

        networkInfo[kWPANTUNDProperty_IPv6MeshLocalAddress] = rval;
    }

#endif

exit:
    root["result"] = networkInfo;

    if (ret != kWpanStatus_OK)
    {
        root["result"] = mResponseFail;
        otbrLog(OTBR_LOG_ERR, "wpan service error: %d", ret);
    }
    root["error"] = ret;
    response      = jsonWriter.write(root);
    return response;
}

std::string WpanService::HandleAvailableNetworkRequest()
{
    Json::Value      root, networks, networkInfo;
    Json::FastWriter jsonWriter;
    std::string      response;
    int              ret = ot::Dbus::kWpantundStatus_Ok;
#if OTBR_ENABLE_NCP_WPANTUND
    ot::Dbus::WPANController wpanController;

    wpanController.SetInterfaceName(mIfName);
    VerifyOrExit(wpanController.Scan() == ot::Dbus::kWpantundStatus_Ok, ret = ot::Dbus::kWpantundStatus_ScanFailed);
    mNetworksCount = wpanController.GetScanNetworksInfoCount();
    VerifyOrExit(mNetworksCount > 0, ret = ot::Dbus::kWpantundStatus_NetworkNotFound);
    memcpy(mNetworks, wpanController.GetScanNetworksInfo(), mNetworksCount * sizeof(ot::Dbus::WpanNetworkInfo));

#else
    ot::Client client;

    VerifyOrExit(client.Connect(), ret = ot::Dbus::kWpantundStatus_ScanFailed);
    VerifyOrExit((mNetworksCount = client.Scan(mNetworks, sizeof(mNetworks) / sizeof(mNetworks[0]))) > 0,
                 ret = ot::Dbus::kWpantundStatus_NetworkNotFound);
#endif

    for (int i = 0; i < mNetworksCount; i++)
    {
        char extPanId[OT_EXTENDED_PANID_LENGTH * 2 + 1], panId[OT_PANID_LENGTH * 2 + 3],
            hardwareAddress[OT_HARDWARE_ADDRESS_LENGTH * 2 + 1];
        ot::Utils::Long2Hex(Thread::Encoding::BigEndian::HostSwap64(mNetworks[i].mExtPanId), extPanId);
        ot::Utils::Bytes2Hex(mNetworks[i].mHardwareAddress, OT_HARDWARE_ADDRESS_LENGTH, hardwareAddress);
        sprintf(panId, "0x%X", mNetworks[i].mPanId);
        networkInfo[i]["nn"] = mNetworks[i].mNetworkName;
        networkInfo[i]["xp"] = extPanId;
        networkInfo[i]["pi"] = panId;
        networkInfo[i]["ch"] = mNetworks[i].mChannel;
        networkInfo[i]["ha"] = hardwareAddress;
    }

    root["result"] = networkInfo;

exit:
    if (ret != ot::Dbus::kWpantundStatus_Ok)
    {
        root["result"] = mResponseFail;
        otbrLog(OTBR_LOG_ERR, "Error is %d", ret);
    }
    root["error"] = ret;
    response      = jsonWriter.write(root);
    return response;
}

int WpanService::GetWpanServiceStatus(std::string &aNetworkName, std::string &aExtPanId) const
{
    std::string wpantundState = "";
    int         status        = kWpanStatus_OK;
#if OTBR_ENABLE_NCP_WPANTUND
    ot::Dbus::WPANController wpanController;

    wpanController.SetInterfaceName(mIfName);
    wpantundState = wpanController.Get(kWPANTUNDProperty_NCPState);
    if (wpantundState.length() == 0)
    {
        status = kWpanStatus_Down;
        ExitNow();
    }

    if (wpantundState == kWPANTUNDStateAssociated)
    {
        aNetworkName = wpanController.Get(kWPANTUNDProperty_NetworkName);
        aExtPanId    = wpanController.Get(kWPANTUNDProperty_NetworkXPANID);
        aExtPanId    = aExtPanId.substr(OT_HEX_PREFIX_LENGTH);
    }
    else if (wpantundState.find(kWPANTUNDStateOffline) != std::string::npos)
    {
        status = kWpanStatus_Offline;
    }
    else if (wpantundState.find(kWPANTUNDStateAssociating) != std::string::npos)
    {
        status = kWpanStatus_Associating;
    }
    else
    {
        status = kWpanStatus_Uninitialized;
    }
#else
    ot::Client  client;
    const char *rval;

    VerifyOrExit(client.Connect(), status = kWpanStatus_Uninitialized);
    rval = client.Execute("state");
    VerifyOrExit(rval != NULL, status = kWpanStatus_Down);
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
        VerifyOrExit(rval != NULL, status = kWpanStatus_Down);
        aNetworkName = rval;

        rval = client.Execute("extpanid");
        VerifyOrExit(rval != NULL, status = kWpanStatus_Down);
        aExtPanId = rval;
    }
#endif // OTBR_ENABLE_NCP_WPANTUND

exit:

    return status;
}

std::string WpanService::HandleCommission(const std::string &aCommissionRequest)
{
    Json::Value  root;
    Json::Reader reader;
    int          ret = ot::Dbus::kWpantundStatus_Ok;
    std::string  pskd;
    std::string  passphrase;
    std::string  response;

    VerifyOrExit(reader.parse(aCommissionRequest.c_str(), root) == true, ret = kWpanStatus_ParseRequestFailed);
    pskd       = root["pskd"].asString();
    passphrase = root["passphrase"].asString();

    response = CommissionDevice(pskd.c_str(), passphrase.c_str());
exit:
    if (ret != ot::Dbus::kWpantundStatus_Ok)
    {
        root["result"] = mResponseFail;
        otbrLog(OTBR_LOG_ERR, "error: %d", ret);
    }
    return response;
}

std::string WpanService::CommissionDevice(const char *aPskd, const char *aNetworkPassword)
{
    int                            ret = ot::Dbus::kWpantundStatus_Ok;
    Json::Value                    root, networkInfo;
    Json::FastWriter               jsonWriter;
    std::string                    response, networkName, extPanId, propertyValue;
    BorderRouter::CommissionerArgs args;
    int                            serviceStatus = GetWpanServiceStatus(networkName, extPanId);

    VerifyOrExit(serviceStatus == kWpanStatus_OK, ret = Dbus::kWpantundStatus_NetworkNotFound);
    args.mPSKd = aPskd;

    {
        ot::Psk::Pskc pskc;
        uint8_t       extPanIdHex[BorderRouter::kXPanIdLength];
        VerifyOrExit(sizeof(extPanIdHex) == Utils::Hex2Bytes(extPanId.c_str(), extPanIdHex, sizeof(extPanIdHex)),
                     ret = Dbus::kWpantundStatus_Failure);

        memcpy(args.mPSKc, pskc.ComputePskc(extPanIdHex, networkName.c_str(), aNetworkPassword), sizeof(args.mPSKc));
    }

    // We allow all joiners for simplicity
    {
        int steeringLength = 1;
        args.mSteeringData.Init(steeringLength);
        args.mSteeringData.Set();
    }

    args.mKeepAliveInterval = 15;
    args.mDebugLevel        = OTBR_LOG_EMERG;

    args.mAgentHost = kBorderAgentHost;
    args.mAgentPort = kBorderAgentPort;

    RunCommission(args);
exit:
    root["error"] = ret;
    response      = jsonWriter.write(root);

    return response;
}

int WpanService::RunCommission(BorderRouter::CommissionerArgs aArgs)
{
    BorderRouter::Commissioner commissioner(aArgs.mPSKc, aArgs.mKeepAliveInterval);
    bool                       joinerSetDone = false;
    int                        ret;

    commissioner.InitDtls(aArgs.mAgentHost, aArgs.mAgentPort);

    do
    {
        ret = commissioner.TryDtlsHandshake();
    } while (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE);

    if (commissioner.IsValid())
    {
        commissioner.CommissionerPetition();
    }

    while (commissioner.IsValid() && commissioner.GetNumFinalizedJoiners() == 0)
    {
        int            maxFd   = -1;
        struct timeval timeout = {10, 0};
        int            rval;

        fd_set readFdSet;
        fd_set writeFdSet;
        fd_set errorFdSet;

        FD_ZERO(&readFdSet);
        FD_ZERO(&writeFdSet);
        FD_ZERO(&errorFdSet);
        commissioner.UpdateFdSet(readFdSet, writeFdSet, errorFdSet, maxFd, timeout);
        rval = select(maxFd + 1, &readFdSet, &writeFdSet, &errorFdSet, &timeout);
        if (rval < 0)
        {
            otbrLog(OTBR_LOG_ERR, "select() failed", strerror(errno));
            break;
        }
        commissioner.Process(readFdSet, writeFdSet, errorFdSet);
        if (commissioner.IsCommissionerAccepted() && !joinerSetDone)
        {
            commissioner.SetJoiner(aArgs.mPSKd, aArgs.mSteeringData);
            joinerSetDone = true;
        }
    }

    return commissioner.GetNumFinalizedJoiners();
}

} // namespace Web
} // namespace ot
