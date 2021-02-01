/*
 *    Copyright (c) 2021, The OpenThread Authors.
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
 *   This file implements HIDL Settings interface.
 */

#include "hidl/1.0/hidl_settings.hpp"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <hidl/HidlTransportSupport.h>
#include <openthread/platform/secure_settings.h>

#include "common/code_utils.hpp"
#include "common/logging.hpp"

namespace otbr {
namespace Hidl {
using android::hardware::handleTransportPoll;
using android::hardware::setupTransportPolling;

HidlSettings::HidlSettings(void)
    : mSettingsCallback(nullptr)
{
    VerifyOrDie((mHidlFd = setupTransportPolling()) >= 0, "Setup HIDL transport for use with (e)poll failed");
}

void HidlSettings::Init(void)
{
    otbrLog(OTBR_LOG_INFO, "Register HIDL Settings service");
    VerifyOrDie(registerAsService() == android::NO_ERROR, "Register HIDL Settings service failed");

    mDeathRecipient = new ClientDeathRecipient(sClientDeathCallback, this);
    VerifyOrDie(mDeathRecipient != nullptr, "Create client death reciptient failed");
}

otbrError HidlSettings::WaitingForClientToStart(void)
{
    otbrError error = OTBR_ERROR_NONE;
    fd_set    readFdSet;
    fd_set    writeFdSet;

    while (true)
    {
        FD_ZERO(&readFdSet);
        FD_ZERO(&writeFdSet);
        FD_SET(mHidlFd, &readFdSet);
        FD_SET(mHidlFd, &writeFdSet);

        if (select(mHidlFd + 1, &readFdSet, &writeFdSet, nullptr, nullptr) >= 0)
        {
            if (FD_ISSET(mHidlFd, &readFdSet) || FD_ISSET(mHidlFd, &writeFdSet))
            {
                handleTransportPoll(mHidlFd);
            }

            if (mSettingsCallback != nullptr)
            {
                break;
            }
        }
        else
        {
            error = OTBR_ERROR_ERRNO;
            break;
        }
    }

    return error;
}

Return<void> HidlSettings::initialize(const sp<IThreadSettingsCallback> &aCallback)
{
    VerifyOrExit(aCallback != NULL);

    mSettingsCallback = aCallback;

    mDeathRecipient->SetClientHasDied(false);
    mSettingsCallback->linkToDeath(mDeathRecipient, 2);

    otbrLog(OTBR_LOG_INFO, "HIDL Settings interface initialized");

exit:
    return Void();
}

Return<void> HidlSettings::deinitialize(void)
{
    mSettingsCallback = nullptr;

    if ((!mDeathRecipient->GetClientHasDied()) && (mSettingsCallback != nullptr))
    {
        mSettingsCallback->unlinkToDeath(mDeathRecipient);
        mDeathRecipient->SetClientHasDied(true);
    }

    otbrLog(OTBR_LOG_INFO, "HIDL Settings interface deinitialized");

    return Void();
}

otError HidlSettings::Get(uint16_t aKey, int aIndex, uint8_t *aValue, uint16_t *aValueLength)
{
    otError error = OT_ERROR_NONE;

    VerifyOrExit(mSettingsCallback != nullptr, error = OT_ERROR_INVALID_STATE);

    mSettingsCallback->onSettingsGet(aKey, aIndex, [&](ThreadError aError, const hidl_vec<uint8_t> &aValueVec) {
        if ((error = static_cast<otError>(aError)) == OT_ERROR_NONE)
        {
            uint16_t length = aValueVec.size();

            if (aValueLength != nullptr)
            {
                length        = (*aValueLength < length) ? *aValueLength : length;
                *aValueLength = length;

                if (aValue != nullptr)
                {
                    std::copy_n(aValueVec.begin(), length, aValue);
                }
            }
        }
    });

exit:
    return error;
}

otError HidlSettings::Set(uint16_t aKey, const uint8_t *aValue, uint16_t aValueLength)
{
    ThreadError ret;
    otError     error = OT_ERROR_NONE;

    VerifyOrExit(mSettingsCallback != nullptr, error = OT_ERROR_INVALID_STATE);

    ret   = mSettingsCallback->onSettingsSet(aKey,
                                           hidl_vec<uint8_t>(reinterpret_cast<const uint8_t *>(aValue),
                                                             reinterpret_cast<const uint8_t *>(aValue) + aValueLength));
    error = static_cast<otError>(ret);

exit:
    return error;
}

otError HidlSettings::Add(uint16_t aKey, const uint8_t *aValue, uint16_t aValueLength)
{
    ThreadError ret;
    otError     error = OT_ERROR_NONE;

    VerifyOrExit(mSettingsCallback != nullptr, error = OT_ERROR_INVALID_STATE);

    ret   = mSettingsCallback->onSettingsAdd(aKey,
                                           hidl_vec<uint8_t>(reinterpret_cast<const uint8_t *>(aValue),
                                                             reinterpret_cast<const uint8_t *>(aValue) + aValueLength));
    error = static_cast<otError>(ret);

exit:
    return error;
}

otError HidlSettings::Delete(uint16_t aKey, int aIndex)
{
    ThreadError ret;
    otError     error = OT_ERROR_NONE;

    VerifyOrExit(mSettingsCallback != nullptr, error = OT_ERROR_INVALID_STATE);

    ret   = mSettingsCallback->onSettingsDelete(aKey, aIndex);
    error = static_cast<otError>(ret);

exit:
    return error;
}

void HidlSettings::Wipe(void)
{
    VerifyOrExit(mSettingsCallback != nullptr);

    mSettingsCallback->onSettingsWipe();

exit:
    return;
}

} // namespace Hidl
} // namespace otbr

#include "hidl/1.0/hidl_agent.hpp"

extern std::unique_ptr<otbr::Hidl::HidlAgent> gHidlAgent;

void otPlatSecureSettingsInit(otInstance *aInstance)
{
    OTBR_UNUSED_VARIABLE(aInstance);
}

void otPlatSecureSettingsDeinit(otInstance *aInstance)
{
    OTBR_UNUSED_VARIABLE(aInstance);
}

otError otPlatSecureSettingsGet(otInstance *aInstance,
                                uint16_t    aKey,
                                int         aIndex,
                                uint8_t *   aValue,
                                uint16_t *  aValueLength)
{
    OTBR_UNUSED_VARIABLE(aInstance);

    otError error;

    VerifyOrExit(gHidlAgent != nullptr, error = OT_ERROR_INVALID_STATE);
    error = gHidlAgent->GetSettings().Get(aKey, aIndex, aValue, aValueLength);

exit:
    return error;
}

otError otPlatSecureSettingsSet(otInstance *aInstance, uint16_t aKey, const uint8_t *aValue, uint16_t aValueLength)
{
    OTBR_UNUSED_VARIABLE(aInstance);

    otError error;

    VerifyOrExit(gHidlAgent != nullptr, error = OT_ERROR_INVALID_STATE);
    error = gHidlAgent->GetSettings().Set(aKey, aValue, aValueLength);

exit:
    return error;
}

otError otPlatSecureSettingsAdd(otInstance *aInstance, uint16_t aKey, const uint8_t *aValue, uint16_t aValueLength)
{
    OTBR_UNUSED_VARIABLE(aInstance);

    otError error;

    VerifyOrExit(gHidlAgent != nullptr, error = OT_ERROR_INVALID_STATE);
    error = gHidlAgent->GetSettings().Add(aKey, aValue, aValueLength);

exit:
    return error;
}

otError otPlatSecureSettingsDelete(otInstance *aInstance, uint16_t aKey, int aIndex)
{
    OTBR_UNUSED_VARIABLE(aInstance);

    otError error;

    VerifyOrExit(gHidlAgent != nullptr, error = OT_ERROR_INVALID_STATE);
    error = gHidlAgent->GetSettings().Delete(aKey, aIndex);

exit:
    return error;
}

void otPlatSecureSettingsWipe(otInstance *aInstance)
{
    OTBR_UNUSED_VARIABLE(aInstance);

    VerifyOrExit(gHidlAgent != nullptr);
    gHidlAgent->GetSettings().Wipe();

exit:
    return;
}
