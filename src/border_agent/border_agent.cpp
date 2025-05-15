/*
 *    Copyright (c) 2017, The OpenThread Authors.
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
 *   The file implements the Thread border agent.
 */

#define OTBR_LOG_TAG "BA"

#include "border_agent/border_agent.hpp"

#if OTBR_ENABLE_BORDER_AGENT

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <iomanip>
#include <random>
#include <sstream>

#include <openthread/border_agent.h>
#include <openthread/border_routing.h>
#include <openthread/random_crypto.h>
#include <openthread/random_noncrypto.h>
#include <openthread/thread.h>
#include <openthread/thread_ftd.h>
#include <openthread/verhoeff_checksum.h>
#include <openthread/platform/settings.h>
#include <openthread/platform/toolchain.h>

#include "agent/uris.hpp"
#include "host/rcp_host.hpp"
#if OTBR_ENABLE_BACKBONE_ROUTER
#include "backbone_router/backbone_agent.hpp"
#endif
#include "common/byteswap.hpp"
#include "common/code_utils.hpp"
#include "common/logging.hpp"
#include "common/tlv.hpp"
#include "common/types.hpp"
#include "utils/hex.hpp"

#if OTBR_ENABLE_BORDER_AGENT_MESHCOP_SERVICE
#if !(OTBR_ENABLE_MDNS_AVAHI || OTBR_ENABLE_MDNS_MDNSSD || OTBR_ENABLE_MDNS_MOJO)
#error "Border Agent meshcop service feature requires at least one `OTBR_MDNS` implementation"
#endif
#endif

namespace otbr {

#if OTBR_ENABLE_BORDER_AGENT_MESHCOP_SERVICE
static const char kBorderAgentServiceType[]      = "_meshcop._udp";   ///< Border agent service type of mDNS
static const char kBorderAgentEpskcServiceType[] = "_meshcop-e._udp"; ///< Border agent ePSKc service
#endif

static constexpr int kBorderAgentServiceDummyPort = 49152;
static constexpr int kEpskcRandomGenLen           = 8;

BorderAgent::BorderAgent(Mdns::Publisher &aPublisher)
    : mPublisher(aPublisher)
{
    ClearState();
}

void BorderAgent::Deinit(void)
{
    ClearState();
}

otbrError BorderAgent::CreateEphemeralKey(std::string &aEphemeralKey)
{
    std::string digitString;
    char        checksum;
    uint8_t     candidateBuffer[1];
    otbrError   error = OTBR_ERROR_NONE;

    for (uint8_t i = 0; i < kEpskcRandomGenLen; ++i)
    {
        while (true)
        {
            SuccessOrExit(otRandomCryptoFillBuffer(candidateBuffer, 1), error = OTBR_ERROR_ABORTED);
            // Generates a random number in the range [0, 9] with equal probability.
            if (candidateBuffer[0] < 250)
            {
                digitString += static_cast<char>('0' + candidateBuffer[0] % 10);
                break;
            }
        }
    }
    SuccessOrExit(otVerhoeffChecksumCalculate(digitString.c_str(), &checksum), error = OTBR_ERROR_INVALID_ARGS);
    aEphemeralKey = digitString + checksum;

exit:
    return error;
}

otbrError BorderAgent::SetMeshCoPServiceValues(const std::string              &aServiceInstanceName,
                                               const std::string              &aProductName,
                                               const std::string              &aVendorName,
                                               const std::vector<uint8_t>     &aVendorOui,
                                               const Mdns::Publisher::TxtList &aNonStandardTxtEntries)
{
    otbrError        error = OTBR_ERROR_NONE;
    VendorTxtEntries vendorEntries;

    VerifyOrExit(aProductName.size() <= kMaxProductNameLength, error = OTBR_ERROR_INVALID_ARGS);
    VerifyOrExit(aVendorName.size() <= kMaxVendorNameLength, error = OTBR_ERROR_INVALID_ARGS);
    VerifyOrExit(aVendorOui.empty() || aVendorOui.size() == kVendorOuiLength, error = OTBR_ERROR_INVALID_ARGS);

    for (const auto &txtEntry : aNonStandardTxtEntries)
    {
        VerifyOrExit(!txtEntry.mKey.empty() && txtEntry.mKey.front() == 'v', error = OTBR_ERROR_INVALID_ARGS);
        vendorEntries[txtEntry.mKey] = txtEntry.mValue;
    }

    mProductName = aProductName;
    mVendorName  = aVendorName;
    mVendorOui   = aVendorOui;
    EncodeVendorTxtData(vendorEntries);

    mBaseServiceInstanceName = aServiceInstanceName;

exit:
    return error;
}

void BorderAgent::SetEnabled(bool aIsEnabled)
{
    VerifyOrExit(IsEnabled() != aIsEnabled);
    mIsEnabled = aIsEnabled;
    if (mIsEnabled)
    {
        Start();
    }
    else
    {
        Stop();
    }
exit:
    return;
}

void BorderAgent::ClearState(void)
{
    VendorTxtEntries emptyTxtEntries;

    mIsEnabled = false;
    mVendorOui.clear();
    mVendorName  = OTBR_VENDOR_NAME;
    mProductName = OTBR_PRODUCT_NAME;
    EncodeVendorTxtData(emptyTxtEntries);
    mBaseServiceInstanceName = OTBR_MESHCOP_SERVICE_INSTANCE_NAME;
#if OTBR_ENABLE_BORDER_AGENT_MESHCOP_SERVICE
    mServiceInstanceName.clear();
#endif
}

void BorderAgent::Start(void)
{
    otbrLogInfo("Start Thread Border Agent");

#if OTBR_ENABLE_BORDER_AGENT_MESHCOP_SERVICE
    mServiceInstanceName = GetServiceInstanceName();
    UpdateMeshCoPService();
#endif
}

void BorderAgent::Stop(void)
{
#if OTBR_ENABLE_BORDER_AGENT_MESHCOP_SERVICE
    otbrLogInfo("Stop Thread Border Agent");
    UnpublishMeshCoPService();
#endif
}

#if OTBR_ENABLE_BORDER_AGENT_MESHCOP_SERVICE

void BorderAgent::HandleEpskcStateChanged(otBorderAgentEphemeralKeyState aEpskcState, uint16_t aPort)
{
    switch (aEpskcState)
    {
    case OT_BORDER_AGENT_STATE_STARTED:
    case OT_BORDER_AGENT_STATE_CONNECTED:
    case OT_BORDER_AGENT_STATE_ACCEPTED:
        PublishEpskcService(aPort);
        break;
    case OT_BORDER_AGENT_STATE_DISABLED:
    case OT_BORDER_AGENT_STATE_STOPPED:
        UnpublishEpskcService();
        break;
    }
}

void BorderAgent::PublishEpskcService(uint16_t aPort)
{
    otbrLogInfo("Publish meshcop-e service %s.%s.local. port %d", mServiceInstanceName.c_str(),
                kBorderAgentEpskcServiceType, aPort);

    mPublisher.PublishService(/* aHostName */ "", mServiceInstanceName, kBorderAgentEpskcServiceType,
                              Mdns::Publisher::SubTypeList{}, aPort, /* aTxtData */ {},
                              [this, aPort](otbrError aError) {
                                  if (aError == OTBR_ERROR_ABORTED)
                                  {
                                      // OTBR_ERROR_ABORTED is thrown when an ongoing service registration is
                                      // cancelled. This can happen when the meshcop-e service is being updated
                                      // frequently. To avoid false alarms, it should not be logged like a real error.
                                      otbrLogInfo("Cancelled previous publishing meshcop-e service %s.%s.local",
                                                  mServiceInstanceName.c_str(), kBorderAgentEpskcServiceType);
                                  }
                                  else
                                  {
                                      otbrLogResult(aError, "Result of publish meshcop-e service %s.%s.local",
                                                    mServiceInstanceName.c_str(), kBorderAgentEpskcServiceType);
                                  }

                                  if (aError == OTBR_ERROR_DUPLICATED)
                                  {
                                      // Try to unpublish current service in case we are trying to register
                                      // multiple new services simultaneously when the original service name
                                      // is conflicted.
                                      // Potential risk that instance name is not the same with meshcop service.
                                      UnpublishEpskcService();
                                      mServiceInstanceName = GetAlternativeServiceInstanceName();
                                      PublishEpskcService(aPort);
                                  }
                              });
}

void BorderAgent::UnpublishEpskcService()
{
    otbrLogInfo("Unpublish meshcop-e service %s.%s.local", mServiceInstanceName.c_str(), kBorderAgentEpskcServiceType);

    mPublisher.UnpublishService(mServiceInstanceName, kBorderAgentEpskcServiceType, [this](otbrError aError) {
        otbrLogResult(aError, "Result of unpublish meshcop-e service %s.%s.local", mServiceInstanceName.c_str(),
                      kBorderAgentEpskcServiceType);
    });
}

void BorderAgent::HandleMdnsState(Mdns::Publisher::State aState)
{
    VerifyOrExit(IsEnabled());

    switch (aState)
    {
    case Mdns::Publisher::State::kReady:
        UpdateMeshCoPService();
        break;
    default:
        otbrLogWarning("mDNS publisher not available!");
        break;
    }
exit:
    return;
}

void BorderAgent::HandleBorderAgentMeshCoPServiceChanged(bool                        aIsActive,
                                                         uint16_t                    aPort,
                                                         const std::vector<uint8_t> &aOtTxtData)
{
    if (aIsActive != mBaIsActive || aPort != mMeshCoPUdpPort)
    {
        mBaIsActive     = aIsActive;
        mMeshCoPUdpPort = aPort;
    }

    mOtTxtData.assign(aOtTxtData.begin(), aOtTxtData.end());

    // Parse extended address from the encoded data for the first time
    if (!mIsInitialized)
    {
        Mdns::Publisher::TxtList txtList;
        otbrError                error = Mdns::Publisher::DecodeTxtData(txtList, mOtTxtData.data(), mOtTxtData.size());

        otbrLogResult(error, "Result of decoding MeshCoP TXT data from OT");
        SuccessOrExit(error);

        for (auto &entry : txtList)
        {
            if (entry.mKey == "xa")
            {
                memcpy(mExtAddress.m8, entry.mValue.data(), sizeof(mExtAddress.m8));

                mServiceInstanceName = GetServiceInstanceName();
                mIsInitialized       = true;
                break;
            }
        }
    }

exit:
    UpdateMeshCoPService();
}

#endif // OTBR_ENABLE_BORDER_AGENT_MESHCOP_SERVICE

void BorderAgent::EncodeVendorTxtData(const VendorTxtEntries &aVendorEntries)
{
    Mdns::Publisher::TxtList txtList;

    if (!mVendorOui.empty())
    {
        txtList.emplace_back("vo", mVendorOui.data(), mVendorOui.size());
    }

    if (!mVendorName.empty())
    {
        txtList.emplace_back("vn", mVendorName.c_str());
    }

    if (!mProductName.empty())
    {
        txtList.emplace_back("mn", mProductName.c_str());
    }

    for (const auto &vendorEntry : aVendorEntries)
    {
        const std::string          &key   = vendorEntry.first;
        const std::vector<uint8_t> &value = vendorEntry.second;
        bool                        found = false;

        for (Mdns::Publisher::TxtEntry &txtEntry : txtList)
        {
            if (txtEntry.mKey == key)
            {
                txtEntry.mValue              = value;
                txtEntry.mIsBooleanAttribute = false;
                found                        = true;
                break;
            }
        }

        if (!found)
        {
            txtList.emplace_back(key.c_str(), value.data(), value.size());
        }
    }

    mVendorTxtData.clear();

    if (!txtList.empty())
    {
        otbrError error = Mdns::Publisher::EncodeTxtData(txtList, mVendorTxtData);

        assert(error == OTBR_ERROR_NONE);
        OTBR_UNUSED_VARIABLE(error);
    }
}

#if OTBR_ENABLE_BORDER_AGENT_MESHCOP_SERVICE

void BorderAgent::PublishMeshCoPService(void)
{
    Mdns::Publisher::TxtData txtData;
    int                      port;

    otbrLogInfo("Publish meshcop service %s.%s.local.", mServiceInstanceName.c_str(), kBorderAgentServiceType);

    // When thread interface is not active, the border agent is not started, thus it's not listening to any port and
    // not handling requests. In such situation, we use a dummy port number for publishing the MeshCoP service to
    // advertise the status of the border router. One can learn the thread interface status from `sb` entry so it
    // doesn't have to send requests to the dummy port when border agent is not running.
    port = mBaIsActive ? mMeshCoPUdpPort : kBorderAgentServiceDummyPort;

    txtData.insert(txtData.end(), mVendorTxtData.begin(), mVendorTxtData.end());
    txtData.insert(txtData.end(), mOtTxtData.begin(), mOtTxtData.end());

    mPublisher.PublishService(/* aHostName */ "", mServiceInstanceName, kBorderAgentServiceType,
                              Mdns::Publisher::SubTypeList{}, port, txtData, [this](otbrError aError) {
                                  if (aError == OTBR_ERROR_ABORTED)
                                  {
                                      // OTBR_ERROR_ABORTED is thrown when an ongoing service registration is
                                      // cancelled. This can happen when the meshcop service is being updated
                                      // frequently. To avoid false alarms, it should not be logged like a real error.
                                      otbrLogInfo("Cancelled previous publishing meshcop service %s.%s.local",
                                                  mServiceInstanceName.c_str(), kBorderAgentServiceType);
                                  }
                                  else
                                  {
                                      otbrLogResult(aError, "Result of publish meshcop service %s.%s.local",
                                                    mServiceInstanceName.c_str(), kBorderAgentServiceType);
                                  }
                                  if (aError == OTBR_ERROR_DUPLICATED)
                                  {
                                      // Try to unpublish current service in case we are trying to register
                                      // multiple new services simultaneously when the original service name
                                      // is conflicted.
                                      UnpublishMeshCoPService();
                                      mServiceInstanceName = GetAlternativeServiceInstanceName();
                                      PublishMeshCoPService();
                                  }
                              });
}

void BorderAgent::UnpublishMeshCoPService(void)
{
    otbrLogInfo("Unpublish meshcop service %s.%s.local", mServiceInstanceName.c_str(), kBorderAgentServiceType);

    mPublisher.UnpublishService(mServiceInstanceName, kBorderAgentServiceType, [this](otbrError aError) {
        otbrLogResult(aError, "Result of unpublish meshcop service %s.%s.local", mServiceInstanceName.c_str(),
                      kBorderAgentServiceType);
    });
}

void BorderAgent::UpdateMeshCoPService(void)
{
    VerifyOrExit(mIsInitialized);
    VerifyOrExit(IsEnabled());
    VerifyOrExit(mPublisher.IsStarted());
    PublishMeshCoPService();

exit:
    return;
}

#endif

#if OTBR_ENABLE_DBUS_SERVER
void BorderAgent::UpdateVendorMeshCoPTxtEntries(const VendorTxtEntries &aVendorEntries)
{
    EncodeVendorTxtData(aVendorEntries);
#if OTBR_ENABLE_BORDER_AGENT_MESHCOP_SERVICE
    UpdateMeshCoPService();
#endif
}
#endif

#if OTBR_ENABLE_BORDER_AGENT_MESHCOP_SERVICE

std::string BorderAgent::GetServiceInstanceName(void) const
{
    std::stringstream ss;

    ss << mBaseServiceInstanceName << " #";
    ss << std::uppercase << std::hex << std::setfill('0');
    ss << std::setw(2) << static_cast<int>(mExtAddress.m8[6]);
    ss << std::setw(2) << static_cast<int>(mExtAddress.m8[7]);
    return ss.str();
}

std::string BorderAgent::GetAlternativeServiceInstanceName(void) const
{
    std::random_device                      r;
    std::default_random_engine              engine(r());
    std::uniform_int_distribution<uint16_t> uniform_dist(1, 0xFFFF);
    uint16_t                                rand = uniform_dist(engine);
    std::stringstream                       ss;

    ss << GetServiceInstanceName() << " (" << rand << ")";
    return ss.str();
}

#endif // OTBR_ENABLE_BORDER_AGENT_MESHCOP_SERVICE

} // namespace otbr

#endif // OTBR_ENABLE_BORDER_AGENT
