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
 *   This file includes definitions for NCP service based on wpantund.
 */

#ifndef NCP_WPANTUND_HPP_
#define NCP_WPANTUND_HPP_

#include <map>

#include <arpa/inet.h>
#include <dbus/dbus.h>
#include <net/if.h>
#include <stdint.h>
#include <sys/select.h>

#include "ncp.hpp"

namespace ot {

namespace BorderRouter {

namespace Ncp {

/**
 * This class provides NCP service based on wpantund.
 *
 */
class ControllerWpantund : public Controller
{
public:
    /**
     * The contructor to initialize a Ncp Controller.
     *
     * @param[in]   aInterfaceName  A string of the NCP interface.
     *
     */
    ControllerWpantund(const char *aInterfaceName);
    ~ControllerWpantund(void);

    /*
     * This method initalize the NCP controller.
     *
     * @retval  OTBR_ERROR_NONE     Successfully initialized NCP controller.
     * @retval  OTBR_ERROR_NCP      Failed due to NCP's internal error.
     *
     */
    otbrError Init(void);

    /**
     * This method sends a packet through UDP forward service.
     *
     * @retval  OTBR_ERROR_NONE         Successfully sent the packet.
     * @retval  OTBR_ERROR_ERRNO        Failed to send the packet, erro info in errno.
     *
     */
    virtual otbrError UdpForwardSend(const uint8_t * aBuffer,
                                     uint16_t        aLength,
                                     uint16_t        aPeerPort,
                                     const in6_addr &aPeerAddr,
                                     uint16_t        aSockPort);

    /**
     * This method updates the fd_set to poll.
     *
     * @param[inout]    aReadFdSet      A reference to fd_set for polling read.
     * @param[inout]    aWriteFdSet     A reference to fd_set for polling read.
     * @param[inout]    aErrorFdSet     A reference to fd_set for polling error.
     * @param[inout]    aMaxFd          A reference to the current max fd in @p aReadFdSet and @p aWriteFdSet.
     *
     */
    virtual void UpdateFdSet(otSysMainloopContext &aMainloop);

    /**
     * This method performs the DTLS processing.
     *
     * @param[in]   aReadFdSet          A reference to fd_set ready for reading.
     * @param[in]   aWriteFdSet         A reference to fd_set ready for writing.
     * @param[in]   aErrorFdSet         A reference to fd_set with error occurred.
     *
     */
    virtual void Process(const otSysMainloopContext &aMainloop);

    /**
     * This method request the event.
     *
     * @param[in]   aEvent              The event id to request.
     *
     * @retval  OTBR_ERROR_NONE         Successfully requested the event.
     * @retval  OTBR_ERROR_ERRNO        Failed to request the event.
     *
     */
    virtual otbrError RequestEvent(int aEvent);

private:
    /**
     * This map is used to track DBusWatch-es.
     *
     */
    typedef std::map<DBusWatch *, bool> WatchMap;

    static DBusHandlerResult HandlePropertyChangedSignal(DBusConnection *aConnection,
                                                         DBusMessage *   aMessage,
                                                         void *          aContext);
    DBusHandlerResult        HandlePropertyChangedSignal(DBusMessage &aMessage);

    otbrError ParseEvent(const char *aKey, DBusMessageIter *aIter);

    otbrError UpdateInterfaceDBusPath();

    static dbus_bool_t AddDBusWatch(struct DBusWatch *aWatch, void *aContext);
    static void        RemoveDBusWatch(struct DBusWatch *aWatch, void *aContext);
    static void        ToggleDBusWatch(struct DBusWatch *aWatch, void *aContext);

    char            mInterfaceDBusName[DBUS_MAXIMUM_NAME_LENGTH + 1];
    char            mInterfaceDBusPath[DBUS_MAXIMUM_NAME_LENGTH + 1];
    char            mInterfaceName[IFNAMSIZ];
    DBusConnection *mDBus;
    WatchMap        mWatches;
};

} // namespace Ncp

} // namespace BorderRouter

} // namespace ot

#endif //  NCP_WPANTUND_HPP_
