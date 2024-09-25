/*
 *    Copyright (c) 2020, The OpenThread Authors.
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
 * This file includes definitions for d-bus agent.
 */

#ifndef OTBR_DBUS_AGENT_HPP_
#define OTBR_DBUS_AGENT_HPP_

#include "openthread-br/config.h"

#include <functional>
#include <set>
#include <string>
#include <sys/select.h>

#include "common/code_utils.hpp"
#include "common/mainloop.hpp"
#include "dbus/common/dbus_message_helper.hpp"
#include "dbus/common/dbus_resources.hpp"
#include "dbus/server/dbus_object.hpp"
#include "dbus/server/dbus_thread_object_ncp.hpp"
#include "dbus/server/dbus_thread_object_rcp.hpp"
#include "ncp/thread_host.hpp"

namespace otbr {
namespace DBus {

class DBusAgent : public MainloopProcessor, private NonCopyable
{
public:
    /**
     * The constructor of dbus agent.
     *
     * @param[in] aHost           A reference to the Thread host.
     * @param[in] aPublisher      A reference to the MDNS publisher.
     */
    DBusAgent(otbr::Ncp::ThreadHost &aHost, Mdns::Publisher &aPublisher);

    /**
     * This method initializes the dbus agent.
     */
    void Init(otbr::BorderAgent &aBorderAgent);

    void Update(MainloopContext &aMainloop) override;
    void Process(const MainloopContext &aMainloop) override;

private:
    using Clock                                              = std::chrono::steady_clock;
    constexpr static std::chrono::seconds kDBusWaitAllowance = std::chrono::seconds(30);

    using UniqueDBusConnection = std::unique_ptr<DBusConnection, std::function<void(DBusConnection *)>>;

    static dbus_bool_t   AddDBusWatch(struct DBusWatch *aWatch, void *aContext);
    static void          RemoveDBusWatch(struct DBusWatch *aWatch, void *aContext);
    UniqueDBusConnection PrepareDBusConnection(void);

    static const struct timeval kPollTimeout;

    std::string                 mInterfaceName;
    std::unique_ptr<DBusObject> mThreadObject;
    UniqueDBusConnection        mConnection;
    otbr::Ncp::ThreadHost      &mHost;
    Mdns::Publisher            &mPublisher;

    /**
     * This map is used to track DBusWatch-es.
     */
    std::set<DBusWatch *> mWatches;
};

} // namespace DBus
} // namespace otbr

#endif // OTBR_DBUS_AGENT_HPP_
