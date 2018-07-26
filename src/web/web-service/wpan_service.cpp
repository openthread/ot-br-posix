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

#include "common/code_utils.hpp"

namespace ot {
namespace Web {

std::string WpanService::HandleJoinNetworkRequest(const std::string &aJoinRequest)
{
    char extPanId[OT_EXTENDED_PANID_LENGTH * 2 + 1];

    Json::Value              root;
    Json::Reader             reader;
    Json::FastWriter         jsonWriter;
    std::string              response;
    int                      index;
    std::string              networkKey;
    std::string              prefix;
    bool                     defaultRoute;
    int                      ret = ot::Dbus::kWpantundStatus_Ok;
    ot::Dbus::WPANController wpanController;

    VerifyOrExit(reader.parse(aJoinRequest.c_str(), root) == true, ret = kWpanStatus_ParseRequestFailed);
    index        = root["index"].asUInt();
    networkKey   = root["networkKey"].asString();
    prefix       = root["prefix"].asString();
    defaultRoute = root["defaultRoute"].asBool();

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

    ot::Utils::Long2Hex(mNetworks[index].mExtPanId, extPanId);
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
    Json::Value              root;
    Json::FastWriter         jsonWriter;
    Json::Reader             reader;
    std::string              response;
    ot::Psk::Pskc            psk;
    char                     pskcStr[OT_PSKC_MAX_LENGTH * 2 + 1];
    uint8_t                  extPanIdBytes[OT_EXTENDED_PANID_LENGTH];
    ot::Dbus::WPANController wpanController;
    std::string              networkKey;
    std::string              prefix;
    uint16_t                 channel;
    std::string              networkName;
    std::string              passphrase;
    std::string              panId;
    std::string              extPanId;
    bool                     defaultRoute;
    int                      ret = ot::Dbus::kWpantundStatus_Ok;

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
    ot::Utils::Hex2Bytes(extPanId.c_str(), extPanIdBytes, OT_EXTENDED_PANID_LENGTH);
    ot::Utils::Bytes2Hex(psk.ComputePskc(extPanIdBytes, networkName.c_str(), passphrase.c_str()), OT_PSKC_MAX_LENGTH,
                         pskcStr);
    VerifyOrExit(wpanController.Set(kPropertyType_Data, kWPANTUNDProperty_NetworkPSKc, pskcStr) ==
                     ot::Dbus::kWpantundStatus_Ok,
                 ret = ot::Dbus::kWpantundStatus_SetFailed);

    VerifyOrExit(wpanController.Form(networkName.c_str(), channel) == ot::Dbus::kWpantundStatus_Ok,
                 ret = ot::Dbus::kWpantundStatus_FormFailed);

    VerifyOrExit(wpanController.AddGateway(prefix.c_str(), defaultRoute) == ot::Dbus::kWpantundStatus_Ok,
                 ret = ot::Dbus::kWpantundStatus_SetGatewayFailed);
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
    Json::Value              root;
    Json::FastWriter         jsonWriter;
    Json::Reader             reader;
    std::string              response;
    std::string              prefix;
    bool                     defaultRoute;
    ot::Dbus::WPANController wpanController;
    int                      ret = ot::Dbus::kWpantundStatus_Ok;

    VerifyOrExit(reader.parse(aAddPrefixRequest.c_str(), root) == true, ret = kWpanStatus_ParseRequestFailed);
    prefix       = root["prefix"].asString();
    defaultRoute = root["defaultRoute"].asBool();

    wpanController.SetInterfaceName(mIfName);
    VerifyOrExit(wpanController.AddGateway(prefix.c_str(), defaultRoute) == ot::Dbus::kWpantundStatus_Ok,
                 ret = ot::Dbus::kWpantundStatus_SetGatewayFailed);
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
    Json::Value              root;
    Json::FastWriter         jsonWriter;
    Json::Reader             reader;
    std::string              response;
    std::string              prefix;
    ot::Dbus::WPANController wpanController;
    int                      ret = ot::Dbus::kWpantundStatus_Ok;

    VerifyOrExit(reader.parse(aDeleteRequest.c_str(), root) == true, ret = kWpanStatus_ParseRequestFailed);
    prefix = root["prefix"].asString();
    wpanController.SetInterfaceName(mIfName);
    VerifyOrExit(wpanController.RemoveGateway(prefix.c_str()) == ot::Dbus::kWpantundStatus_Ok,
                 ret = ot::Dbus::kWpantundStatus_SetGatewayFailed);
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
    Json::Value              root, networkInfo;
    Json::FastWriter         jsonWriter;
    ot::Dbus::WPANController wpanController;
    std::string              response, networkName, extPanId, propertyValue;
    int                      ret = ot::Dbus::kWpantundStatus_Ok;

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
        networkInfo["mDNS service"]                        = mServiceUp;
        break;

    case kWpanStatus_Offline:
        networkInfo["WPAN service"] = kWPANTUNDStateOffline;
        networkInfo["mDNS service"] = mServiceDown;
        break;

    case kWpanStatus_Down:
    default:
        networkInfo["wpantund"]     = mServiceDown;
        networkInfo["WPAN service"] = kWPANTUNDStateUninitialized;
        networkInfo["mDNS service"] = mServiceDown;
        break;
    }
    root["result"] = networkInfo;

exit:
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
    Json::Value              root, networks, networkInfo;
    ot::Dbus::WPANController wpanController;
    Json::FastWriter         jsonWriter;
    std::string              response;
    int                      ret = ot::Dbus::kWpantundStatus_Ok;

    wpanController.SetInterfaceName(mIfName);
    VerifyOrExit(wpanController.Scan() == ot::Dbus::kWpantundStatus_Ok, ret = ot::Dbus::kWpantundStatus_ScanFailed);
    mNetworksCount = wpanController.GetScanNetworksInfoCount();
    VerifyOrExit(mNetworksCount > 0, ret = ot::Dbus::kWpantundStatus_NetworkNotFound);
    memcpy(mNetworks, wpanController.GetScanNetworksInfo(), mNetworksCount * sizeof(ot::Dbus::WpanNetworkInfo));

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
    std::string              wpantundState = "";
    int                      status        = kWpanStatus_OK;
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
exit:

    return status;
}

} // namespace Web
} // namespace ot
