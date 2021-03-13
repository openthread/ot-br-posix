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

#include "agent/thread_helper.hpp"

#include <assert.h>
#include <limits.h>
#include <string.h>

#include <string>

#include <openthread/border_router.h>
#include <openthread/channel_manager.h>
#include <openthread/jam_detection.h>
#include <openthread/joiner.h>
#include <openthread/thread_ftd.h>
#include <openthread/platform/radio.h>

#include "agent/ncp_openthread.hpp"
#include "common/byteswap.hpp"
#include "common/code_utils.hpp"
#include "common/logging.hpp"

namespace otbr {
namespace agent {

ThreadHelper::ThreadHelper(otInstance *aInstance, otbr::Ncp::ControllerOpenThread *aNcp)
    : mInstance(aInstance)
    , mNcp(aNcp)
{
}

void ThreadHelper::StateChangedCallback(otChangedFlags aFlags)
{
    if (aFlags & OT_CHANGED_THREAD_ROLE)
    {
        otDeviceRole role = otThreadGetDeviceRole(mInstance);

        for (const auto &handler : mDeviceRoleHandlers)
        {
            handler(role);
        }

        if (role != OT_DEVICE_ROLE_DISABLED && role != OT_DEVICE_ROLE_DETACHED)
        {
            if (mAttachHandler != nullptr)
            {
                mAttachHandler(OT_ERROR_NONE);
                mAttachHandler = nullptr;
            }
            else if (mJoinerHandler != nullptr)
            {
                mJoinerHandler(OT_ERROR_NONE);
                mJoinerHandler = nullptr;
            }
        }
    }
}

void ThreadHelper::AddDeviceRoleHandler(DeviceRoleHandler aHandler)
{
    mDeviceRoleHandlers.emplace_back(aHandler);
}

void ThreadHelper::Scan(ScanHandler aHandler)
{
    otError error = OT_ERROR_NONE;

    VerifyOrExit(aHandler != nullptr);
    mScanHandler = aHandler;
    mScanResults.clear();

    error =
        otLinkActiveScan(mInstance, /*scanChannels =*/0, /*scanDuration=*/0, &ThreadHelper::sActiveScanHandler, this);

exit:
    if (error != OT_ERROR_NONE)
    {
        if (aHandler)
        {
            mScanHandler(error, {});
        }
        mScanHandler = nullptr;
    }
}

void ThreadHelper::RandomFill(void *aBuf, size_t size)
{
    std::uniform_int_distribution<> dist(0, UINT8_MAX);
    uint8_t *                       buf = static_cast<uint8_t *>(aBuf);

    for (size_t i = 0; i < size; i++)
    {
        buf[i] = static_cast<uint8_t>(dist(mRandomDevice));
    }
}

void ThreadHelper::sActiveScanHandler(otActiveScanResult *aResult, void *aThreadHelper)
{
    ThreadHelper *helper = static_cast<ThreadHelper *>(aThreadHelper);

    helper->ActiveScanHandler(aResult);
}

void ThreadHelper::ActiveScanHandler(otActiveScanResult *aResult)
{
    if (aResult == nullptr)
    {
        if (mScanHandler != nullptr)
        {
            mScanHandler(OT_ERROR_NONE, mScanResults);
        }
    }
    else
    {
        mScanResults.push_back(*aResult);
    }
}

uint8_t ThreadHelper::RandomChannelFromChannelMask(uint32_t aChannelMask)
{
    // 8 bit per byte
    constexpr uint8_t kNumChannels = sizeof(aChannelMask) * 8;
    uint8_t           channels[kNumChannels];
    uint8_t           numValidChannels = 0;

    for (uint8_t i = 0; i < kNumChannels; i++)
    {
        if (aChannelMask & (1 << i))
        {
            channels[numValidChannels++] = i;
        }
    }

    return channels[std::uniform_int_distribution<unsigned int>(0, numValidChannels - 1)(mRandomDevice)];
}

static otExtendedPanId ToOtExtendedPanId(uint64_t aExtPanId)
{
    otExtendedPanId extPanId;
    uint64_t        mask = UINT8_MAX;

    for (size_t i = 0; i < sizeof(uint64_t); i++)
    {
        extPanId.m8[i] = static_cast<uint8_t>((aExtPanId >> ((sizeof(uint64_t) - i - 1) * 8)) & mask);
    }

    return extPanId;
}

void ThreadHelper::Attach(const std::string &         aNetworkName,
                          uint16_t                    aPanId,
                          uint64_t                    aExtPanId,
                          const std::vector<uint8_t> &aMasterKey,
                          const std::vector<uint8_t> &aPSKc,
                          uint32_t                    aChannelMask,
                          ResultHandler               aHandler)

{
    otError         error = OT_ERROR_NONE;
    otExtendedPanId extPanId;
    otMasterKey     masterKey;
    otPskc          pskc;
    uint32_t        channelMask;
    uint8_t         channel;

    VerifyOrExit(aHandler != nullptr, error = OT_ERROR_INVALID_ARGS);
    VerifyOrExit(mAttachHandler == nullptr && mJoinerHandler == nullptr, error = OT_ERROR_INVALID_STATE);
    mAttachHandler = aHandler;
    VerifyOrExit(aMasterKey.empty() || aMasterKey.size() == sizeof(masterKey.m8), error = OT_ERROR_INVALID_ARGS);
    VerifyOrExit(aPSKc.empty() || aPSKc.size() == sizeof(pskc.m8), error = OT_ERROR_INVALID_ARGS);
    VerifyOrExit(aChannelMask != 0, error = OT_ERROR_INVALID_ARGS);

    while (aPanId == UINT16_MAX)
    {
        RandomFill(&aPanId, sizeof(aPanId));
    }

    if (aExtPanId != UINT64_MAX)
    {
        extPanId = ToOtExtendedPanId(aExtPanId);
    }
    else
    {
        *reinterpret_cast<uint64_t *>(&extPanId) = UINT64_MAX;

        while (*reinterpret_cast<uint64_t *>(&extPanId) == UINT64_MAX)
        {
            RandomFill(extPanId.m8, sizeof(extPanId.m8));
        }
    }

    if (!aMasterKey.empty())
    {
        memcpy(masterKey.m8, &aMasterKey[0], sizeof(masterKey.m8));
    }
    else
    {
        RandomFill(masterKey.m8, sizeof(masterKey.m8));
    }

    if (!aPSKc.empty())
    {
        memcpy(pskc.m8, &aPSKc[0], sizeof(pskc.m8));
    }
    else
    {
        RandomFill(pskc.m8, sizeof(pskc.m8));
    }

    if (!otIp6IsEnabled(mInstance))
    {
        SuccessOrExit(error = otIp6SetEnabled(mInstance, true));
    }

    SuccessOrExit(error = otThreadSetNetworkName(mInstance, aNetworkName.c_str()));
    SuccessOrExit(error = otLinkSetPanId(mInstance, aPanId));
    SuccessOrExit(error = otThreadSetExtendedPanId(mInstance, &extPanId));
    SuccessOrExit(error = otThreadSetMasterKey(mInstance, &masterKey));

    channelMask = otPlatRadioGetPreferredChannelMask(mInstance) & aChannelMask;

    if (channelMask == 0)
    {
        channelMask = otLinkGetSupportedChannelMask(mInstance) & aChannelMask;
    }
    VerifyOrExit(channelMask != 0, otbrLog(OTBR_LOG_WARNING, "Invalid channel mask"), error = OT_ERROR_INVALID_ARGS);

    channel = RandomChannelFromChannelMask(channelMask);
    SuccessOrExit(otLinkSetChannel(mInstance, channel));

    SuccessOrExit(error = otThreadSetPskc(mInstance, &pskc));

    SuccessOrExit(error = otThreadSetEnabled(mInstance, true));
exit:
    if (error != OT_ERROR_NONE)
    {
        if (aHandler)
        {
            aHandler(error);
        }
        mAttachHandler = nullptr;
    }
}

void ThreadHelper::Attach(ResultHandler aHandler)
{
    otError error = OT_ERROR_NONE;

    VerifyOrExit(mAttachHandler == nullptr && mJoinerHandler == nullptr, error = OT_ERROR_INVALID_STATE);
    mAttachHandler = aHandler;

    if (!otIp6IsEnabled(mInstance))
    {
        SuccessOrExit(error = otIp6SetEnabled(mInstance, true));
    }
    SuccessOrExit(error = otThreadSetEnabled(mInstance, true));

exit:
    if (error != OT_ERROR_NONE)
    {
        if (aHandler)
        {
            aHandler(error);
        }
        mAttachHandler = nullptr;
    }
}

otError ThreadHelper::Reset(void)
{
    mDeviceRoleHandlers.clear();
    otInstanceReset(mInstance);

    return OT_ERROR_NONE;
}

void ThreadHelper::JoinerStart(const std::string &aPskd,
                               const std::string &aProvisioningUrl,
                               const std::string &aVendorName,
                               const std::string &aVendorModel,
                               const std::string &aVendorSwVersion,
                               const std::string &aVendorData,
                               ResultHandler      aHandler)
{
    otError error = OT_ERROR_NONE;

    VerifyOrExit(aHandler != nullptr, error = OT_ERROR_INVALID_ARGS);
    VerifyOrExit(mAttachHandler == nullptr && mJoinerHandler == nullptr, error = OT_ERROR_INVALID_STATE);
    mJoinerHandler = aHandler;

    if (!otIp6IsEnabled(mInstance))
    {
        SuccessOrExit(error = otIp6SetEnabled(mInstance, true));
    }
    error = otJoinerStart(mInstance, aPskd.c_str(), aProvisioningUrl.c_str(), aVendorName.c_str(), aVendorModel.c_str(),
                          aVendorSwVersion.c_str(), aVendorData.c_str(), sJoinerCallback, this);
exit:
    if (error != OT_ERROR_NONE)
    {
        if (aHandler)
        {
            aHandler(error);
        }
        mJoinerHandler = nullptr;
    }
}

void ThreadHelper::sJoinerCallback(otError aError, void *aThreadHelper)
{
    ThreadHelper *helper = static_cast<ThreadHelper *>(aThreadHelper);

    helper->JoinerCallback(aError);
}

void ThreadHelper::JoinerCallback(otError aError)
{
    if (aError != OT_ERROR_NONE)
    {
        otbrLog(OTBR_LOG_WARNING, "Failed to join Thread network: %s", otThreadErrorToString(aError));
        mJoinerHandler(aError);
        mJoinerHandler = nullptr;
    }
    else
    {
        LogOpenThreadResult("Start Thread network", otThreadSetEnabled(mInstance, true));
    }
}

otError ThreadHelper::TryResumeNetwork(void)
{
    otError error = OT_ERROR_NONE;

    if (otLinkGetPanId(mInstance) != UINT16_MAX && otThreadGetDeviceRole(mInstance) == OT_DEVICE_ROLE_DISABLED)
    {
        if (!otIp6IsEnabled(mInstance))
        {
            SuccessOrExit(error = otIp6SetEnabled(mInstance, true));
            SuccessOrExit(error = otThreadSetEnabled(mInstance, true));
        }
    }

exit:
    if (error != OT_ERROR_NONE)
    {
        (void)otIp6SetEnabled(mInstance, false);
    }

    return error;
}

#if OTBR_ENABLE_UNSECURE_JOIN
otError ThreadHelper::PermitUnsecureJoin(uint16_t aPort, uint32_t aSeconds)
{
    otError      error = OT_ERROR_NONE;
    otExtAddress steeringData;

    // 0xff to allow all devices to join
    memset(&steeringData.m8, 0xff, sizeof(steeringData.m8));
    SuccessOrExit(error = otIp6AddUnsecurePort(mInstance, aPort));
    otThreadSetSteeringData(mInstance, &steeringData);

    if (aSeconds > 0)
    {
        auto delay = Milliseconds(aSeconds * 1000);

        ++mUnsecurePortRefCounter[aPort];

        mNcp->PostTimerTask(delay, [this, aPort]() {
            assert(mUnsecurePortRefCounter.find(aPort) != mUnsecurePortRefCounter.end());
            assert(mUnsecurePortRefCounter[aPort] > 0);

            if (--mUnsecurePortRefCounter[aPort] == 0)
            {
                otExtAddress noneAddress;

                // 0 to clean steering data
                memset(&noneAddress.m8, 0, sizeof(noneAddress.m8));
                (void)otIp6RemoveUnsecurePort(mInstance, aPort);
                otThreadSetSteeringData(mInstance, &noneAddress);
                mUnsecurePortRefCounter.erase(aPort);
            }
        });
    }
    else
    {
        otExtAddress noneAddress;

        memset(&noneAddress.m8, 0, sizeof(noneAddress.m8));
        (void)otIp6RemoveUnsecurePort(mInstance, aPort);
        otThreadSetSteeringData(mInstance, &noneAddress);
    }

exit:
    return error;
}
#endif

} // namespace agent
} // namespace otbr
