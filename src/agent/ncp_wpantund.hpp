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

#include "common/types.hpp"
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
     * @param[in]   aPSKcHandler    A pointer to the function that receives the PSKc.
     * @param[in]   aPacketHandler  A pointer to the function that handles the packet.
     * @param[in]   aContext    A pointer to application-specific context.
     *
     */
    ControllerWpantund(const char *aInterfaceName, PSKcHandler aPSKcHandler, PacketHandler aPacketHandler,
                       void *aContext);
    ~ControllerWpantund(void);

    /**
     * This method request the Ncp to stop the border agent proxy service.
     *
     * @returns 0 on success, otherwise failure.
     *
     */
    virtual int BorderAgentProxyStart(void);

    /**
     * This method request the Ncp to stop the border agent proxy service.
     *
     * @returns 0 on success, otherwise failure.
     *
     */
    virtual int BorderAgentProxyStop(void);

    /**
     * This method sends a packet through border agent proxy service.
     *
     * @returns 0 on success, otherwise failure.
     *
     */
    virtual int BorderAgentProxySend(const uint8_t *aBuffer, uint16_t aLength, uint16_t aLocator, uint16_t aPort);

    /**
     * This method updates the fd_set to poll.
     *
     * @param[inout]    aReadFdSet      A reference to fd_set for polling read.
     * @param[inout]    aWriteFdSet     A reference to fd_set for polling read.
     * @param[inout]    aMaxFd          A reference to the current max fd in @p aReadFdSet and @p aWriteFdSet.
     *
     */
    virtual void UpdateFdSet(fd_set &aReadFdSet, fd_set &aWriteFdSet, fd_set &aErrorFdSet, int &aMaxFd);

    /**
     * This method performs the DTLS processing.
     *
     * @param[in]   aReadFdSet      A reference to fd_set ready for reading.
     * @param[in]   aWriteFdSet     A reference to fd_set ready for writing.
     */
    virtual void Process(const fd_set &aReadFdSet, const fd_set &aWriteFdSet, const fd_set &aErrorFdSet);

    /**
     * This method retrieves the current PSKc.
     *
     * @returns The current PSKc.
     *
     */
    virtual const uint8_t *GetPSKc(void);

    /**
     * This method retrieves the Eui64.
     *
     * @returns The hardware address.
     *
     */
    virtual const uint8_t *GetEui64(void);

private:
    /**
     * This map is used to track DBusWatch-es.
     *
     */
    typedef std::map<DBusWatch *, bool> WatchMap;

    static DBusHandlerResult HandleProperyChangedSignal(DBusConnection *aConnection, DBusMessage *aMessage,
                                                        void *aContext);
    DBusHandlerResult HandleProperyChangedSignal(DBusConnection &aConnection, DBusMessage &aMessage);
    DBusMessage *RequestProperty(const char *aKey);
    int      GetProperty(const char *aKey, uint8_t *aBuffer, size_t &aSize);

    static dbus_bool_t AddDBusWatch(struct DBusWatch *aWatch, void *aContext);
    static void RemoveDBusWatch(struct DBusWatch *aWatch, void *aContext);
    static void ToggleDBusWatch(struct DBusWatch *aWatch, void *aContext);

    int BorderAgentProxyEnable(dbus_bool_t aEnable);

    char            mInterfaceDBusName[DBUS_MAXIMUM_NAME_LENGTH + 1];
    char            mInterfaceDBusPath[DBUS_MAXIMUM_NAME_LENGTH + 1];
    uint8_t         mPSKc[kSizePSKc];
    uint8_t         mEui64[kSizeEui64];
    char            mInterfaceName[IFNAMSIZ];
    DBusConnection *mDBus;
    PacketHandler   mPacketHandler;
    PSKcHandler     mPSKcHandler;
    void           *mContext;
    WatchMap        mWatches;
};

} // Ncp

} // namespace BorderRouter

} // namespace ot

#endif  //  NCP_WPANTUND_HPP_
