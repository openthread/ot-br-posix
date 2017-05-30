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
 *   This file provides DBus operations APIs for other modules to control the WPAN interface.
 */

#ifndef WPAN_CONTROLLER_HPP
#define WPAN_CONTROLLER_HPP

// #define WPAN_TUNNEL_DBUS_PATH   "/com/nestlabs/WPANTunnelDriver"
// #define WPANTUND_DBUS_PATH      "/org/wpantund"
// #define WPANTUND_DBUS_APIv1_INTERFACE "org.wpantund.v1"
// #define WPAN_TUNNEL_DBUS_INTERFACE  "com.nestlabs.WPANTunnelDriver"

#define ERRORCODE_UNKNOWN       (4)
#define SCANNED_NET_BUFFER_SIZE 250
#define SET_MAX_DATA_SIZE 250
#define NETWORK_NAME_MAX_SIZE 17
#define HARDWARE_ADDRESS_SIZE 8
#define PREFIX_SIZE 8

// #define kWPANTUNDProperty_NetworkName            "Network:Name"
// #define kWPANTUNDProperty_NetworkXPANID          "Network:XPANID"
// #define kWPANTUNDProperty_NetworkPANID           "Network:PANID"
// #define kWPANTUNDProperty_NetworkNodeType        "Network:NodeType"
// #define kWPANTUNDProperty_NetworkKey             "Network:Key"
// #define kWPANTUNDProperty_NetworkKeyIndex        "Network:KeyIndex"
// #define kWPANTUNDProperty_NCPMACAddress          "NCP:MACAddress"
// #define kWPANTUNDProperty_NCPChannel             "NCP:Channel"
// #define kWPANTUNDProperty_NCPHardwareAddress     "NCP:HardwareAddress"
// #define kWPANTUNDProperty_NetworkPskc            "Network:PSKc"
// #define kWPANTUNDProperty_NestLabs_NetworkAllowingJoin         "com.nestlabs.internal:Network:AllowingJoin"
// #define WPANTUND_IF_SIGNAL_NET_SCAN_BEACON    "NetScanBeacon"
#define LEADER_ROLE 1
#define ROUTER_ROLE 2

#include <stdint.h>
#include <string.h>

#include <dbus/dbus.h>
extern "C" {
#include "wpanctl-utils.h"
#include "wpan-dbus-v0.h"
#include "wpan-dbus-v1.h"
}

#include "dbus_base.hpp"

namespace ot {
namespace Dbus {

enum
{
    kWpantundStatus_Ok                       = 0,
    kWpantundStatus_Failure                  = 1,

    kWpantundStatus_InvalidArgument          = 2,
    kWpantundStatus_InvalidWhenDisabled      = 3,
    kWpantundStatus_InvalidForCurrentState   = 4,
    kWpantundStatus_InvalidType              = 5,
    kWpantundStatus_InvalidRange             = 6,

    kWpantundStatus_Timeout                  = 7,
    kWpantundStatus_SocketReset              = 8,
    kWpantundStatus_Busy                     = 9,

    kWpantundStatus_Already                  = 10,
    kWpantundStatus_Canceled                 = 11,
    kWpantundStatus_InProgress               = 12,
    kWpantundStatus_TryAgainLater            = 13,

    kWpantundStatus_FeatureNotSupported      = 14,
    kWpantundStatus_FeatureNotImplemented    = 15,

    kWpantundStatus_PropertyNotFound         = 16,
    kWpantundStatus_PropertyEmpty            = 17,

    kWpantundStatus_JoinFailedUnknown        = 18,
    kWpantundStatus_JoinFailedAtScan         = 19,
    kWpantundStatus_JoinFailedAtAuthenticate = 20,
    kWpantundStatus_FormFailedAtScan         = 21,

    kWpantundStatus_NCP_Crashed              = 22,
    kWpantundStatus_NCP_Fatal                = 23,
    kWpantundStatus_NCP_InvalidArgument      = 24,
    kWpantundStatus_NCP_InvalidRange         = 25,

    kWpantundStatus_MissingXPANID            = 26,

    kWpantundStatus_NCP_Reset                = 27,

    kWpantundStatus_InterfaceNotFound        = 28,

    kWpantundStatus_InvalidConnection        = 29,
    kWpantundStatus_InvalidMessage           = 30,
    kWpantundStatus_InvalidReply             = 31,
    kWpantundStatus_NetworkNotFound          = 32,
    kWpantundStatus_LookUpFailed             = 33,
    kWpantundStatus_LeaveFailed              = 34,
    kWpantundStatus_ScanFailed               = 35,
    kWpantundStatus_SetFailed                = 36,
    kWpantundStatus_JoinFailed               = 37,
    kWpantundStatus_GatewayFailed            = 38,
    kWpantundStatus_FormFailed               = 39,
    kWpantundStatus_StartFailed              = 40,
    kWpantundStatus_GetNullMessage           = 41,
    kWpantundStatus_GetNullReply             = 42,
    kWpantundStatus_GetNullPending           = 43,
    kWpantundStatus_InvalidDBusName          = 44,
};

struct WpanNetworkInfo
{
    char        mNetworkName[NETWORK_NAME_MAX_SIZE];
    dbus_bool_t mAllowingJoin;
    uint16_t    mPanId;
    uint16_t    mChannel;
    uint64_t    mExtPanId;
    int8_t      mRssi;
    uint8_t     mHardwareAddress[HARDWARE_ADDRESS_SIZE];
    uint8_t     mPrefix[PREFIX_SIZE];
};

class WPANController
{
public:
    const struct WpanNetworkInfo *GetScanNetworksInfo(void);
    int GetScanNetworksInfoCount(void);
    const char *GetDBusInterfaceName(void);
    int Leave(void);
    int Form(const char *aNetworkname, int aChannel);
    int Scan(void);
    int Join(const char *aNetworkname, int16_t aChannel, uint64_t aExtPanId,
             uint16_t aPanId);
    const char *Get(const char *aPropertyName);
    int Set(uint8_t aType, const char *aPropertyName, const char *aPropertyValue);
    int Gateway(const char *aPrefix, uint8_t aLength, bool aIsDefaultRoute);
    int RemoveGateway(const char *aPrefix, uint8_t aLength);
    void SetInterfaceName(const char *aIfName);

private:

    char            mIfName[DBUS_MAXIMUM_NAME_LENGTH + 1];
    WpanNetworkInfo mScannedNetworks[SCANNED_NET_BUFFER_SIZE];
    int             mScannedNetworkCount = 0;

};

} //namespace Dbus
} //namespace ot
#endif  //WPAN_CONTROLLER_HPP
