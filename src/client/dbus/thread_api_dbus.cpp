/*
 *    Copyright (c) 2019, The OpenThread Authors.
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

#include "client/dbus/thread_api_dbus.hpp"
#include "common/code_utils.hpp"
#include "dbus/constants.hpp"
#include "dbus/dbus_message_helper.hpp"

using namespace otbr::dbus;

namespace otbr {
namespace client {

ThreadApiDBus::ThreadApiDBus(DBusConnection *aConnection)
    : mInterfaceName("wpan0")
    , mConnection(aConnection)
{
}

ThreadApiDBus::ThreadApiDBus(DBusConnection *aConnection, const std::string &aInterfaceName)
    : mInterfaceName(aInterfaceName)
    , mConnection(aConnection)
{
}

otError ThreadApiDBus::CallDBusMethodAsync(const std::string &aMethodName, DBusPendingCallNotifyFunction aFunction)
{
    otError          ret     = OT_ERROR_NONE;
    DBusMessage *    message = nullptr;
    DBusPendingCall *pending = nullptr;

    message = dbus_message_new_method_call(dbus::kOtbrDbusPrefix.c_str(),
                                           (dbus::kOtbrDbusObjectPrefix + mInterfaceName).c_str(),
                                           dbus::kOtbrDbusPrefix.c_str(), aMethodName.c_str());
    VerifyOrExit(message != nullptr, ret = OT_ERROR_FAILED);
    VerifyOrExit(dbus_connection_send_with_reply(mConnection, message, &pending, DBUS_TIMEOUT_USE_DEFAULT) == true,
                 ret = OT_ERROR_FAILED);

    VerifyOrExit(dbus_pending_call_set_notify(pending, aFunction, this, &ThreadApiDBus::EmptyFree) == true,
                 ret = OT_ERROR_FAILED);
exit:
    return ret;
}

otError ThreadApiDBus::Scan(const ScanHandler &aHandler)
{
    otError err = OT_ERROR_NONE;

    VerifyOrExit(mScanHandler == nullptr, err = OT_ERROR_INVALID_STATE);
    mScanHandler = aHandler;

    err = CallDBusMethodAsync(kOtbrDbusObjectScanMethod, &ThreadApiDBus::sScanPendingCallHandler);
    if (err != OT_ERROR_NONE)
    {
        mScanHandler = nullptr;
    }
exit:
    return err;
}

void ThreadApiDBus::sScanPendingCallHandler(DBusPendingCall *aPending, void *aData)
{
    ThreadApiDBus *threadApiDBus = static_cast<ThreadApiDBus *>(aData);

    threadApiDBus->ScanPendingCallHandler(aPending);
}

void ThreadApiDBus::ScanPendingCallHandler(DBusPendingCall *aPending)
{
    std::vector<otActiveScanResult> scanResults;
    DBusMessage *                   message = dbus_pending_call_steal_reply(aPending);
    auto                            args    = std::tie(scanResults);

    if (message != nullptr)
    {
        DBusMessageToTuple(*message, args);
        dbus_message_unref(message);
    }

    mScanHandler(scanResults);
    mScanHandler = nullptr;
}

std::string ThreadApiDBus::GetInterfaceName(void)
{
    return mInterfaceName;
}

} // namespace client
} // namespace otbr
