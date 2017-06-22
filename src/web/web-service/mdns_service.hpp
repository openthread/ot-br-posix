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

#ifndef MNDS_SERVICE
#define MNDS_SERVICE

#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include <net/if.h>

#include <jsoncpp/json/json.h>
#include <jsoncpp/json/writer.h>

#include "../mdns-publisher/mdns_publisher.hpp"
#include "../wpan-controller/wpan_controller.hpp"
#include "common/logging.hpp"

#define OT_BORDER_ROUTER_PORT 49191

namespace ot {
namespace Web {

/**
 * This class provides web service to multicast DNS.
 *
 */
class MdnsService
{
public:

    /**
     * This method handles http request to advertise service.
     *
     * @param[in]  aMdnsRequest  The reference to http request of mDNS service.
     *
     * @returns The string to the http response of mdns service.
     *
     */
    std::string HandleMdnsRequest(const std::string &aMdnsRequest);

    /**
     * This method starts mdns service.
     *
     * @param[in]  aNetworkName  The reference to the network name.
     * @param[in]  aExtPanId     The reference to the extend PAN ID.
     *
     * @retval kMndsServiceStatus_OK Successfully started the mDNS service
     */
    int StartMdnsService(const std::string &aNetworkName, const std::string &aExtPanId);

    /**
     * This method updates mdns service.
     *
     * @param[in]  aNetworkName  The reference to the network name.
     * @param[in]  aExtPanId     The reference to the extend PAN ID.
     *
     * @retval kMndsServiceStatus_OK Successfully updated the mDNS service
     */
    int UpdateMdnsService(const std::string &aNetworkName, const std::string &aExtPanId);

    /**
     * This method indicates whether or not the mDNS service is running.
     *
     * @retval true    Successfully started the mdns service.
     * @retval false   Failed to start the mdns service.
     *
     */
    bool IsStartedService(void) { return ot::Mdns::Publisher::GetInstance().IsRunning(); };

private:

    char        mIfName[IFNAMSIZ];
    std::string mNetworkName, mExtPanId;
    const char *mResponseSuccess = "successful";
    const char *mResponseFail = "failed";

    enum
    {
        kMndsServiceStatus_OK = 0,
        kMndsServiceStatus_ParseRequestFailed,
    };
};

} //namespace Web
} //namespace ot

#endif
