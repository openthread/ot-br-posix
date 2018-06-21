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

#define OT_SCANNED_NET_BUFFER_SIZE 250
#define OT_SET_MAX_DATA_SIZE 250
#define OT_NETWORK_NAME_MAX_SIZE 17
#define OT_HARDWARE_ADDRESS_SIZE 8
#define OT_PREFIX_SIZE 8
#define OT_ROUTER_ROLE 2

#include <net/if.h>
#include <stdint.h>
#include <string.h>

#include <dbus/dbus.h>
extern "C" {
#include "wpan-dbus-v0.h"
#include "wpan-dbus-v1.h"
#include "wpanctl-utils.h"
}

#include "dbus_base.hpp"

namespace ot {
namespace Dbus {

enum
{
    kWpantundStatus_Ok                = 0,
    kWpantundStatus_Failure           = 1,
    kWpantundStatus_InvalidArgument   = 2,
    kWpantundStatus_NetworkNotFound   = 3,
    kWpantundStatus_LeaveFailed       = 4,
    kWpantundStatus_ScanFailed        = 5,
    kWpantundStatus_SetFailed         = 6,
    kWpantundStatus_JoinFailed        = 7,
    kWpantundStatus_SetGatewayFailed  = 8,
    kWpantundStatus_FormFailed        = 9,
    kWpantundStatus_InvalidConnection = 10,
    kWpantundStatus_InvalidMessage    = 11,
    kWpantundStatus_InvalidReply      = 12,
    kWpantundStatus_InvalidPending    = 13,
    kWpantundStatus_InvalidDBusName   = 14,
};

struct WpanNetworkInfo
{
    char        mNetworkName[OT_NETWORK_NAME_MAX_SIZE];
    dbus_bool_t mAllowingJoin;
    uint16_t    mPanId;
    uint16_t    mChannel;
    uint64_t    mExtPanId;
    int8_t      mRssi;
    uint8_t     mHardwareAddress[OT_HARDWARE_ADDRESS_SIZE];
    uint8_t     mPrefix[OT_PREFIX_SIZE];
};

class WPANController
{
public:
    /**
     * This method returns the pointer to the WpanNetworkInfo structure.
     *
     * @returns The pointer to the WpanNetworkInfo structure.
     *
     */
    const struct WpanNetworkInfo *GetScanNetworksInfo(void) const;

    /**
     * This method returns the length of WpanNetworkInfo structure.
     *
     * @returns The length of WpanNetworkInfo structure.
     *
     */
    int GetScanNetworksInfoCount(void) const;

    /**
     * This method returns the pointer to the DBus interface name.
     *
     * @returns The pointer to the DBus interface name.
     *
     */
    const char *GetDBusInterfaceName(void) const;

    /**
     * This method leaves the current Thread Network.
     *
     * @retval kWpantundStatus_Ok                 Successfully left the Thread Network.
     * @retval kWpantundStatus_InvalidConnection  The DBus connection is invalid.
     * @retval kWpantundStatus_InvalidMessage     The DBus message is invalid.
     * @retval kWpantundStatus_InvalidReply       The DBus reply message is invalid.
     *
     */
    int Leave(void);

    /**
     * This method forms a new Thread Network.
     *
     * @param[in]  aNetworkName      A pointer to the Network Name of the new Thread Network.
     * @param[in]  aChannel          The channel of the new Thread Network.
     *
     * @retval kWpantundStatus_Ok  Successfully formed a new Thread Network.
     * @retval kWpantundStatus_InvalidConnection  The DBus connection is invalid.
     * @retval kWpantundStatus_InvalidMessage     The DBus message is invalid.
     * @retval kWpantundStatus_InvalidReply       The DBus reply message is invalid.
     * @retval kWpantundStatus_InvalidArgument    The aNetworkName or aChannel is invalid.
     *
     */
    int Form(const char *aNetworkName, uint16_t aChannel);

    /**
     * This method scan all existing Thread Network..
     *
     * @retval kWpantundStatus_Ok               Successfully scanned all existing Thread Network.
     * @retval kWpantundStatus_NetworkNotFound  Existing Thread Networks are not found.
     *
     */
    int Scan(void);

    /**
     * This method joins an existing Thread Network.
     *
     * @param[in]  aNetworkname      A pointer to the Network Name of the joining Thread Network.
     * @param[in]  aChannel          The channel of the joining Thread Network.
     * @param[in]  aExtPanId         The Extended Pan ID of the joining Network.
     * @param[in]  aPanId            The Pan ID of the joining Network.
     *
     * @retval kWpantundStatus_Ok                 Successfully joined an existing Thread Network.
     * @retval kWpantundStatus_InvalidConnection  The DBus connection is invalid.
     * @retval kWpantundStatus_InvalidMessage     The DBus message is invalid.
     * @retval kWpantundStatus_InvalidReply       The DBus reply message is invalid.
     * @retval kWpantundStatus_InvalidArgument    The aNetworkName or aChannel or aExtPanId or aPanId is invalid.
     *
     */
    int Join(const char *aNetworkName, uint16_t aChannel, uint64_t aExtPanId, uint16_t aPanId);

    /**
     * This method gets the property of the Thread Network.
     *
     * @param[in]  aPropertyName     A pointer to the property name of the Thread Network.
     *
     * @returns The pointer to the property value.
     *
     */
    const char *Get(const char *aPropertyName) const;

    /**
     * This method sets the Thread Network property.
     *
     * @param[in]  aType            The type of the property.
     * @param[in]  aPropertyName    A pointer to the property name.
     * @param[in]  aPropertyValue   A pointer to the property value.
     *
     * @retval kWpantundStatus_Ok                 Successfully set the property of the Thread Network.
     * @retval kWpantundStatus_InvalidConnection  The DBus connection is invalid.
     * @retval kWpantundStatus_InvalidMessage     The DBus message is invalid.
     * @retval kWpantundStatus_InvalidReply       The DBus reply message is invalid.
     * @retval kWpantundStatus_InvalidArgument    The aPropertyName or aPropertyValue is invalid.
     *
     */
    int Set(uint8_t aType, const char *aPropertyName, const char *aPropertyValue);

    /**
     * This method adds the gateway of the Thread Network.
     *
     * @param[in]  aPrefix           A point to the prefix of the gateway.
     * @param[in]  aIsDefaultRoute   The name of the property.
     *
     * @retval kWpantundStatus_Ok  Successfully added the Thread Network gateway.
     * @retval kWpantundStatus_InvalidConnection  The DBus connection is invalid.
     * @retval kWpantundStatus_InvalidMessage     The DBus message is invalid.
     * @retval kWpantundStatus_InvalidReply       The DBus reply message is invalid.
     * @retval kWpantundStatus_InvalidArgument    The aPrefix is invalid.
     *
     */
    int AddGateway(const char *aPrefix, bool aIsDefaultRoute);

    /**
     * This method removes the gateway of the Thread Network.
     *
     * @param[in]  aPrefix           A point to the prefix of the gateway.
     *
     * @retval kWpantundStatus_Ok  Successfully removed the Thread Network gateway.
     * @retval kWpantundStatus_InvalidConnection  The DBus connection is invalid.
     * @retval kWpantundStatus_InvalidMessage     The DBus message is invalid.
     * @retval kWpantundStatus_InvalidReply       The DBus reply message is invalid.
     * @retval kWpantundStatus_InvalidArgument    The aPrefix is invalid.
     *
     */
    int RemoveGateway(const char *aPrefix);

    /**
     * This method sets the interface name of the wpantund.
     *
     * @param[in]  aIfName           A point to the interface name of the wpantund.
     *
     */
    void SetInterfaceName(const char *aIfName);

private:
    char            mIfName[IFNAMSIZ];
    WpanNetworkInfo mScannedNetworks[OT_SCANNED_NET_BUFFER_SIZE];
    int             mScannedNetworkCount = 0;
};

} // namespace Dbus
} // namespace ot
#endif // WPAN_CONTROLLER_HPP
