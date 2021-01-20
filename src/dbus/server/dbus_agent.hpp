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

#include <functional>
#include <set>
#include <string>
#include <sys/select.h>

#include "dbus/common/dbus_message_helper.hpp"
#include "dbus/common/dbus_resources.hpp"
#include "dbus/server/dbus_object.hpp"
#include "dbus/server/dbus_thread_object.hpp"

#include "agent/ncp_openthread.hpp"

namespace otbr {
namespace DBus {

class DBusAgent
{
public:
    /**
     * The constructor of dbus agent.
     *
     * @param[in]       aInterfaceName  The interface name.
     * @param[in]       aNcp            The ncp controller.
     *
     */
    DBusAgent(const std::string &aInterfaceName, otbr::Ncp::ControllerOpenThread *aNcp);

    /**
     * This method initializes the dbus agent.
     *
     * @returns The intialization error.
     *
     */
    otbrError Init(void);

    /**
     * This method performs the dbus select update.
     *
     * @param[out]      aReadFdSet   The read file descriptors.
     * @param[out]      aWriteFdSet  The write file descriptors.
     * @param[out]      aErorFdSet   The error file descriptors.
     * @param[inout]    aMaxFd       The max file descriptor.
     * @param[inout]    aTimeOut     The select timeout.
     *
     */
    void UpdateFdSet(fd_set &        aReadFdSet,
                     fd_set &        aWriteFdSet,
                     fd_set &        aErrorFdSet,
                     int &           aMaxFd,
                     struct timeval &aTimeOut);

    /**
     * This method processes the dbus I/O.
     *
     * @param[in]       aReadFdSet   The read file descriptors.
     * @param[in]       aWriteFdSet  The write file descriptors.
     * @param[in]       aErorFdSet   The error file descriptors.
     *
     */
    void Process(const fd_set &aReadFdSet, const fd_set &aWriteFdSet, const fd_set &aErrorFdSet);

private:
    static dbus_bool_t AddDBusWatch(struct DBusWatch *aWatch, void *aContext);
    static void        RemoveDBusWatch(struct DBusWatch *aWatch, void *aContext);

    static const struct timeval kPollTimeout;

    std::string                       mInterfaceName;
    std::unique_ptr<DBusThreadObject> mThreadObject;
    using UniqueDBusConnection = std::unique_ptr<DBusConnection, std::function<void(DBusConnection *)>>;
    UniqueDBusConnection             mConnection;
    otbr::Ncp::ControllerOpenThread *mNcp;

    /**
     * This map is used to track DBusWatch-es.
     *
     */
    std::set<DBusWatch *> mWatches;
};

} // namespace DBus
} // namespace otbr

#endif // OTBR_DBUS_AGENT_HPP_
