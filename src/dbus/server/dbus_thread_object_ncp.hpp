/*
 *    Copyright (c) 2024, The OpenThread Authors.
 *    All rights reserved.
 *
 *    Redistribution and use in source and binary forms, with or without
 *    modification, are permitted provided that the following conditions are met:
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *    3. Neither the name of the copyright holder nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 *    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *    ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *    LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *    CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *    ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *    POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 * This file includes definitions for the d-bus object of Thread service when
 * the co-processor is an NCP.
 */

#ifndef OTBR_DBUS_THREAD_OBJECT_NCP_HPP_
#define OTBR_DBUS_THREAD_OBJECT_NCP_HPP_

#include "openthread-br/config.h"

#include <string>

#include <openthread/link.h>

#include "dbus/server/dbus_object.hpp"
#include "mdns/mdns.hpp"
#include "ncp/ncp_host.hpp"

namespace otbr {
namespace DBus {

/**
 * @addtogroup border-router-dbus-server
 *
 * @brief
 *   This module includes the <a href="dbus-api.html">dbus server api</a>.
 *
 * @{
 */

class DBusThreadObjectNcp : public DBusObject
{
public:
    /**
     * This constructor of dbus thread object.
     *
     * @param[in] aConnection     The dbus connection.
     * @param[in] aInterfaceName  The dbus interface name.
     * @param[in] aHost           The Thread controller.
     */
    DBusThreadObjectNcp(DBusConnection &aConnection, const std::string &aInterfaceName, otbr::Ncp::NcpHost &aHost);

    /**
     * This method initializes the dbus thread object.
     *
     * @retval OTBR_ERROR_NONE  The initialization succeeded.
     * @retval OTBR_ERROR_DBUS  The initialization failed due to dbus connection.
     */
    otbrError Init(void) override;

private:
    void AsyncGetDeviceRoleHandler(DBusRequest &aRequest);
    void ReplyAsyncGetProperty(DBusRequest &aRequest, const std::string &aContent);

    void JoinHandler(DBusRequest &aRequest);
    void LeaveHandler(DBusRequest &aRequest);
    void ScheduleMigrationHandler(DBusRequest &aRequest);

    otbr::Ncp::NcpHost &mHost;
};

/**
 * @}
 */

} // namespace DBus
} // namespace otbr

#endif // OTBR_DBUS_THREAD_OBJECT_NCP_HPP_
