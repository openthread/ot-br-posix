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

#include "ncp_wpantund.hpp"

#include <vector>

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

extern "C" {
#include "spinel.h"
#include "wpan-dbus-v1.h"
#include "wpanctl-utils.h"
}

#include "common/code_utils.hpp"
#include "common/logging.hpp"
#include "utils/strcpy_utils.hpp"

#if OTBR_ENABLE_NCP_WPANTUND

namespace ot {

namespace BorderRouter {

namespace Ncp {

/**
 * This string is used to filter the property changed signal from wpantund.
 */
const char *kDBusMatchPropChanged = "type='signal',interface='" WPANTUND_DBUS_APIv1_INTERFACE "',"
                                    "member='" WPANTUND_IF_SIGNAL_PROP_CHANGED "'";

#define OTBR_AGENT_DBUS_NAME_PREFIX "otbr.agent"

static void HandleDBusError(DBusError &aError)
{
    otbrLog(OTBR_LOG_ERR, "NCP DBus error %s: %s!", aError.name, aError.message);
    dbus_error_free(&aError);
}

DBusHandlerResult ControllerWpantund::HandlePropertyChangedSignal(DBusConnection *aConnection,
                                                                  DBusMessage *   aMessage,
                                                                  void *          aContext)
{
    (void)aConnection;
    return static_cast<ControllerWpantund *>(aContext)->HandlePropertyChangedSignal(*aMessage);
}

DBusHandlerResult ControllerWpantund::HandlePropertyChangedSignal(DBusMessage &aMessage)
{
    DBusHandlerResult result = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    DBusMessageIter   iter;
    const char *      key    = NULL;
    const char *      sender = dbus_message_get_sender(&aMessage);
    const char *      path   = dbus_message_get_path(&aMessage);

    if (sender && path && strcmp(sender, mInterfaceDBusName) && strstr(path, mInterfaceName))
    {
        // DBus name of the interface has changed, possibly caused by wpantund restarted,
        // We have to restart the border agent.
        otbrLog(OTBR_LOG_WARNING, "NCP DBus name changed.");
        SuccessOrExit(UpdateInterfaceDBusPath());
    }

    VerifyOrExit(dbus_message_is_signal(&aMessage, WPANTUND_DBUS_APIv1_INTERFACE, WPANTUND_IF_SIGNAL_PROP_CHANGED),
                 result = DBUS_HANDLER_RESULT_NOT_YET_HANDLED);

    VerifyOrExit(dbus_message_iter_init(&aMessage, &iter));
    dbus_message_iter_get_basic(&iter, &key);
    VerifyOrExit(key != NULL, result = DBUS_HANDLER_RESULT_NOT_YET_HANDLED);
    dbus_message_iter_next(&iter);

    otbrLog(OTBR_LOG_DEBUG, "NCP property %s changed.", key);
    SuccessOrExit(OTBR_ERROR_NONE == ParseEvent(key, &iter));

    result = DBUS_HANDLER_RESULT_HANDLED;

exit:
    return result;
}

otbrError ControllerWpantund::ParseEvent(const char *aKey, DBusMessageIter *aIter)
{
    otbrError ret = OTBR_ERROR_NONE;

    if (!strcmp(aKey, kWPANTUNDProperty_NetworkPSKc))
    {
        const uint8_t * pskc  = NULL;
        int             count = 0;
        DBusMessageIter subIter;

        dbus_message_iter_recurse(aIter, &subIter);
        dbus_message_iter_get_fixed_array(&subIter, &pskc, &count);
        VerifyOrExit(count == kSizePSKc, ret = OTBR_ERROR_DBUS);

        EventEmitter::Emit(kEventPSKc, pskc);
    }
    else if (!strcmp(aKey, kWPANTUNDProperty_UdpForwardStream))
    {
        const uint8_t *buf      = NULL;
        uint16_t       peerPort = 0;
        in6_addr       peerAddr;
        uint16_t       sockPort = 0;
        uint16_t       len      = 0;

        {
            DBusMessageIter sub_iter;
            int             nelements = 0;

            dbus_message_iter_recurse(aIter, &sub_iter);
            dbus_message_iter_get_fixed_array(&sub_iter, &buf, &nelements);
            len = static_cast<uint16_t>(nelements);
        }

        // both port and locator are encoded in network endian.
        sockPort = buf[--len];
        sockPort |= buf[--len] << 8;
        len -= sizeof(in6_addr);
        memcpy(peerAddr.s6_addr, &buf[len], sizeof(peerAddr));
        peerPort = buf[--len];
        peerPort |= buf[--len] << 8;

        EventEmitter::Emit(kEventUdpForwardStream, buf, len, peerPort, &peerAddr, sockPort);
    }
    else if (!strcmp(aKey, kWPANTUNDProperty_NCPState))
    {
        const char *state = NULL;
        dbus_message_iter_get_basic(aIter, &state);

        otbrLog(OTBR_LOG_INFO, "state %s", state);

        EventEmitter::Emit(kEventThreadState, 0 == strcmp(state, "associated"));
    }
    else if (!strcmp(aKey, kWPANTUNDProperty_NetworkName))
    {
        const char *networkName = NULL;
        dbus_message_iter_get_basic(aIter, &networkName);

        otbrLog(OTBR_LOG_INFO, "network name %s...", networkName);
        EventEmitter::Emit(kEventNetworkName, networkName);
    }
    else if (!strcmp(aKey, kWPANTUNDProperty_NetworkXPANID))
    {
        uint64_t xpanid = 0;

        if (DBUS_TYPE_UINT64 == dbus_message_iter_get_arg_type(aIter))
        {
            dbus_message_iter_get_basic(aIter, &xpanid);
#if __BYTE_ORDER == __LITTLE_ENDIAN
            // convert to network endian
            for (uint8_t *p = reinterpret_cast<uint8_t *>(&xpanid), *q = p + sizeof(xpanid) - 1; p < q; ++p, --q)
            {
                uint8_t tmp = *p;
                *p          = *q;
                *q          = tmp;
            }
#endif
        }
        else if (DBUS_TYPE_ARRAY == dbus_message_iter_get_arg_type(aIter))
        {
            int             count;
            DBusMessageIter subIter;

            dbus_message_iter_recurse(aIter, &subIter);
            dbus_message_iter_get_fixed_array(&subIter, &xpanid, &count);
            VerifyOrExit(count == sizeof(xpanid), ret = OTBR_ERROR_DBUS);
        }
        else
        {
            ExitNow(ret = OTBR_ERROR_DBUS);
        }

        EventEmitter::Emit(kEventExtPanId, reinterpret_cast<uint8_t *>(&xpanid));
    }

exit:
    return ret;
}

dbus_bool_t ControllerWpantund::AddDBusWatch(struct DBusWatch *aWatch, void *aContext)
{
    static_cast<ControllerWpantund *>(aContext)->mWatches[aWatch] = true;
    return TRUE;
}

void ControllerWpantund::RemoveDBusWatch(struct DBusWatch *aWatch, void *aContext)
{
    static_cast<ControllerWpantund *>(aContext)->mWatches.erase(aWatch);
}

void ControllerWpantund::ToggleDBusWatch(struct DBusWatch *aWatch, void *aContext)
{
    static_cast<ControllerWpantund *>(aContext)->mWatches[aWatch] = (dbus_watch_get_enabled(aWatch) ? true : false);
}

ControllerWpantund::ControllerWpantund(const char *aInterfaceName)
    : mDBus(NULL)
{
    mInterfaceDBusName[0] = '\0';
    strcpy_safe(mInterfaceName, sizeof(mInterfaceName), aInterfaceName);
}

otbrError ControllerWpantund::UpdateInterfaceDBusPath()
{
    otbrError ret = OTBR_ERROR_ERRNO;

    memset(mInterfaceDBusPath, 0, sizeof(mInterfaceDBusPath));
    memset(mInterfaceDBusName, 0, sizeof(mInterfaceDBusName));

    VerifyOrExit(lookup_dbus_name_from_interface(mInterfaceDBusName, mInterfaceName) == 0,
                 otbrLog(OTBR_LOG_ERR, "NCP failed to find the interface!"), errno = ENODEV);

    // Populate the path according to source code of wpanctl, better to export a function.
    snprintf(mInterfaceDBusPath, sizeof(mInterfaceDBusPath), "%s/%s", WPANTUND_DBUS_PATH, mInterfaceName);

    ret = OTBR_ERROR_NONE;

exit:
    return ret;
}

otbrError ControllerWpantund::Init(void)
{
    otbrError ret = OTBR_ERROR_DBUS;
    DBusError error;
    char      dbusName[DBUS_MAXIMUM_NAME_LENGTH];

    dbus_error_init(&error);
    mDBus = dbus_bus_get(DBUS_BUS_SYSTEM, &error);
    VerifyOrExit(mDBus != NULL);

    VerifyOrExit(dbus_bus_register(mDBus, &error));

    sprintf(dbusName, "%s.%s", OTBR_AGENT_DBUS_NAME_PREFIX, mInterfaceName);
    otbrLog(OTBR_LOG_INFO, "NCP request DBus name %s", dbusName);
    VerifyOrExit(dbus_bus_request_name(mDBus, dbusName, DBUS_NAME_FLAG_DO_NOT_QUEUE, &error) ==
                 DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER);

    VerifyOrExit(
        dbus_connection_set_watch_functions(mDBus, AddDBusWatch, RemoveDBusWatch, ToggleDBusWatch, this, NULL));

    dbus_bus_add_match(mDBus, kDBusMatchPropChanged, &error);
    VerifyOrExit(!dbus_error_is_set(&error));

    VerifyOrExit(dbus_connection_add_filter(mDBus, HandlePropertyChangedSignal, this, NULL));

    // Allow wpantund not started.
    ret = OTBR_ERROR_NONE;
    otbrLogResult("Get Thread interface d-bus path", UpdateInterfaceDBusPath());

exit:
    if (dbus_error_is_set(&error))
    {
        HandleDBusError(error);
    }

    if (ret)
    {
        if (mDBus)
        {
            dbus_connection_unref(mDBus);
            mDBus = NULL;
        }
    }

    otbrLogResult("NCP initialize", ret);
    return ret;
}

ControllerWpantund::~ControllerWpantund(void)
{
    if (mDBus)
    {
        dbus_connection_unref(mDBus);
        mDBus = NULL;
    }
}

otbrError ControllerWpantund::UdpForwardSend(const uint8_t * aBuffer,
                                             uint16_t        aLength,
                                             uint16_t        aPeerPort,
                                             const in6_addr &aPeerAddr,
                                             uint16_t        aSockPort)
{
    otbrError    ret     = OTBR_ERROR_ERRNO;
    DBusMessage *message = NULL;

    std::vector<uint8_t> data(aLength + sizeof(aPeerPort) + sizeof(aPeerAddr) + sizeof(aSockPort));
    const uint8_t *      value = data.data();
    const char *         key   = kWPANTUNDProperty_UdpForwardStream;
    size_t               index = aLength;

    VerifyOrExit(mInterfaceDBusPath[0] != '\0', errno = EADDRNOTAVAIL);

    memcpy(data.data(), aBuffer, aLength);
    data[index]     = (aPeerPort >> 8);
    data[index + 1] = (aPeerPort & 0xff);
    index += sizeof(aPeerPort);

    memcpy(&data[index], aPeerAddr.s6_addr, sizeof(aPeerAddr));
    index += sizeof(aPeerAddr);

    data[index]     = (aSockPort >> 8);
    data[index + 1] = (aSockPort & 0xff);

    message = dbus_message_new_method_call(mInterfaceDBusName, mInterfaceDBusPath, WPANTUND_DBUS_APIv1_INTERFACE,
                                           WPANTUND_IF_CMD_PROP_SET);

    VerifyOrExit(message != NULL, errno = ENOMEM);

    VerifyOrExit(dbus_message_append_args(message, DBUS_TYPE_STRING, &key, DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE, &value,
                                          data.size(), DBUS_TYPE_INVALID),
                 errno = EINVAL);

    VerifyOrExit(dbus_connection_send(mDBus, message, NULL), errno = ENOMEM);

    ret = OTBR_ERROR_NONE;
    otbrDump(OTBR_LOG_INFO, "UdpForwardSend success", value, data.size());

exit:

    if (message)
    {
        dbus_message_unref(message);
    }

    if (ret != OTBR_ERROR_NONE)
    {
        otbrLog(OTBR_LOG_WARNING, "UdpForwardSend failed: ", otbrErrorString(ret));
    }

    return ret;
}

void ControllerWpantund::UpdateFdSet(otSysMainloopContext &aMainloop)
{
    DBusWatch *  watch = NULL;
    unsigned int flags;
    int          fd;

    for (WatchMap::iterator it = mWatches.begin(); it != mWatches.end(); ++it)
    {
        if (!it->second)
        {
            continue;
        }

        watch = it->first;
        flags = dbus_watch_get_flags(watch);
        fd    = dbus_watch_get_unix_fd(watch);

        if (fd < 0)
        {
            continue;
        }

        if (flags & DBUS_WATCH_READABLE)
        {
            FD_SET(fd, &aMainloop.mReadFdSet);
        }

        if ((flags & DBUS_WATCH_WRITABLE) && dbus_connection_has_messages_to_send(mDBus))
        {
            FD_SET(fd, &aMainloop.mWriteFdSet);
        }

        FD_SET(fd, &aMainloop.mErrorFdSet);

        if (fd > aMainloop.mMaxFd)
        {
            aMainloop.mMaxFd = fd;
        }
    }
}

void ControllerWpantund::Process(const otSysMainloopContext &aMainloop)
{
    DBusWatch *  watch = NULL;
    unsigned int flags;
    int          fd;

    for (WatchMap::iterator it = mWatches.begin(); it != mWatches.end(); ++it)
    {
        if (!it->second)
        {
            continue;
        }

        watch = it->first;
        flags = dbus_watch_get_flags(watch);
        fd    = dbus_watch_get_unix_fd(watch);

        if (fd < 0)
        {
            continue;
        }

        if ((flags & DBUS_WATCH_READABLE) && !FD_ISSET(fd, &aMainloop.mReadFdSet))
        {
            flags &= static_cast<unsigned int>(~DBUS_WATCH_READABLE);
        }

        if ((flags & DBUS_WATCH_WRITABLE) && !FD_ISSET(fd, &aMainloop.mWriteFdSet))
        {
            flags &= static_cast<unsigned int>(~DBUS_WATCH_WRITABLE);
        }

        if (FD_ISSET(fd, &aMainloop.mErrorFdSet))
        {
            flags |= DBUS_WATCH_ERROR;
        }

        dbus_watch_handle(watch, flags);
    }

    while (DBUS_DISPATCH_DATA_REMAINS == dbus_connection_get_dispatch_status(mDBus) &&
           dbus_connection_read_write_dispatch(mDBus, 0))
        ;
}

otbrError ControllerWpantund::RequestEvent(int aEvent)
{
    otbrError       ret     = OTBR_ERROR_ERRNO;
    DBusMessage *   message = NULL;
    DBusMessage *   reply   = NULL;
    DBusMessageIter iter;
    const char *    key     = NULL;
    const int       timeout = DEFAULT_TIMEOUT_IN_SECONDS * 1000;
    DBusError       error;

    switch (aEvent)
    {
    case kEventExtPanId:
        key = kWPANTUNDProperty_NetworkXPANID;
        break;
    case kEventThreadState:
        key = kWPANTUNDProperty_NCPState;
        break;
    case kEventNetworkName:
        key = kWPANTUNDProperty_NetworkName;
        break;
    case kEventPSKc:
        key = kWPANTUNDProperty_NetworkPSKc;
        break;
    default:
        assert(false);
        break;
    }

    VerifyOrExit(key != NULL && mInterfaceDBusPath[0] != '\0', errno = EINVAL);

    otbrLog(OTBR_LOG_DEBUG, "Request event %s", key);
    VerifyOrExit((message = dbus_message_new_method_call(mInterfaceDBusName, mInterfaceDBusPath,
                                                         WPANTUND_DBUS_APIv1_INTERFACE, WPANTUND_IF_CMD_PROP_GET)) !=
                     NULL,
                 errno = ENOMEM);

    VerifyOrExit(dbus_message_append_args(message, DBUS_TYPE_STRING, &key, DBUS_TYPE_INVALID), errno = EINVAL);

    dbus_error_init(&error);
    reply = dbus_connection_send_with_reply_and_block(mDBus, message, timeout, &error);
    VerifyOrExit(reply != NULL, HandleDBusError(error), ret = OTBR_ERROR_DBUS);
    VerifyOrExit(dbus_message_iter_init(reply, &iter), errno = ENOENT);

    {
        uint32_t status = 0;
        dbus_message_iter_get_basic(&iter, &status);
        VerifyOrExit(status == SPINEL_STATUS_OK, errno = EREMOTEIO);
    }

    dbus_message_iter_next(&iter);
    ret = ParseEvent(key, &iter);

exit:

    if (reply)
    {
        dbus_message_unref(reply);
    }

    if (message)
    {
        dbus_message_unref(message);
    }

    if (ret != OTBR_ERROR_NONE)
    {
        otbrLog(OTBR_LOG_WARNING, "Error requesting %s: %s", key, otbrErrorString(ret));
    }
    return ret;
}

Controller *Controller::Create(const char *aInterfaceName, char *aRadioFile, char *aRadioConfig)
{
    (void)aRadioFile;
    (void)aRadioConfig;

    return new ControllerWpantund(aInterfaceName);
}

} // namespace Ncp

} // namespace BorderRouter

} // namespace ot

#endif // OTBR_ENABLE_NCP_WPANTUND
