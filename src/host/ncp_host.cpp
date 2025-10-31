/*
 *  Copyright (c) 2024, The OpenThread Authors.
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

#define OTBR_LOG_TAG "NCP_HOST"

#include "ncp_host.hpp"

#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <memory>

#include <openthread/error.h>
#include <openthread/thread.h>

#include <openthread/openthread-system.h>

#include "common/code_utils.hpp"

#if OTBR_ENABLE_MDNS
#include "mdns/mdns.hpp"
#endif

#include "host/async_task.hpp"
#include "lib/spinel/spinel_driver.hpp"

namespace otbr {
namespace Host {

// =============================== NcpNetworkProperties ===============================

constexpr otMeshLocalPrefix kMeshLocalPrefixInit = {
    {0xfd, 0xde, 0xad, 0x00, 0xbe, 0xef, 0x00, 0x00},
};

NcpNetworkProperties::NcpNetworkProperties(void)
    : mDeviceRole(OT_DEVICE_ROLE_DISABLED)
{
    memset(&mDatasetActiveTlvs, 0, sizeof(mDatasetActiveTlvs));
    SetMeshLocalPrefix(kMeshLocalPrefixInit);
}

otDeviceRole NcpNetworkProperties::GetDeviceRole(void) const
{
    return mDeviceRole;
}

void NcpNetworkProperties::SetDeviceRole(otDeviceRole aRole)
{
    mDeviceRole = aRole;
}

bool NcpNetworkProperties::Ip6IsEnabled(void) const
{
    // TODO: Implement the method under NCP mode.
    return false;
}

uint32_t NcpNetworkProperties::GetPartitionId(void) const
{
    // TODO: Implement the method under NCP mode.
    return 0;
}

void NcpNetworkProperties::SetDatasetActiveTlvs(const otOperationalDatasetTlvs &aActiveOpDatasetTlvs)
{
    mDatasetActiveTlvs.mLength = aActiveOpDatasetTlvs.mLength;
    memcpy(mDatasetActiveTlvs.mTlvs, aActiveOpDatasetTlvs.mTlvs, aActiveOpDatasetTlvs.mLength);
}

void NcpNetworkProperties::GetDatasetActiveTlvs(otOperationalDatasetTlvs &aDatasetTlvs) const
{
    aDatasetTlvs.mLength = mDatasetActiveTlvs.mLength;
    memcpy(aDatasetTlvs.mTlvs, mDatasetActiveTlvs.mTlvs, mDatasetActiveTlvs.mLength);
}

void NcpNetworkProperties::GetDatasetPendingTlvs(otOperationalDatasetTlvs &aDatasetTlvs) const
{
    // TODO: Implement the method under NCP mode.
    OTBR_UNUSED_VARIABLE(aDatasetTlvs);
}

void NcpNetworkProperties::SetMeshLocalPrefix(const otMeshLocalPrefix &aMeshLocalPrefix)
{
    memcpy(mMeshLocalPrefix.m8, aMeshLocalPrefix.m8, sizeof(mMeshLocalPrefix.m8));
}

const otMeshLocalPrefix *NcpNetworkProperties::GetMeshLocalPrefix(void) const
{
    return &mMeshLocalPrefix;
}

// ===================================== NcpHost ======================================

NcpHost::NcpHost(const char *aInterfaceName, const char *aBackboneInterfaceName, bool aDryRun)
    : mIsInitialized(false)
    , mSpinelDriver(*static_cast<ot::Spinel::SpinelDriver *>(otSysGetSpinelDriver()))
    , mCliDaemon(mNcpSpinel)
{
    memset(&mConfig, 0, sizeof(mConfig));
    mConfig.mInterfaceName         = aInterfaceName;
    mConfig.mBackboneInterfaceName = aBackboneInterfaceName;
    mConfig.mDryRun                = aDryRun;
    mConfig.mSpeedUpFactor         = 1;
}

const char *NcpHost::GetCoprocessorVersion(void)
{
    return mSpinelDriver.GetVersion();
}

void NcpHost::Init(void)
{
    otSysInit(&mConfig);
    mNcpSpinel.Init(mSpinelDriver, *this);
    mCliDaemon.Init(mConfig.mInterfaceName);

    mNcpSpinel.CliDaemonSetOutputCallback([this](const char *aOutput) { mCliDaemon.HandleCommandOutput(aOutput); });

#if OTBR_ENABLE_SRP_ADVERTISING_PROXY
#if OTBR_ENABLE_SRP_SERVER_AUTO_ENABLE_MODE
    // Let SRP server use auto-enable mode. The auto-enable mode delegates the control of SRP server to the Border
    // Routing Manager. SRP server automatically starts when bi-directional connectivity is ready.
    mNcpSpinel.SrpServerSetAutoEnableMode(/* aEnabled */ true);
#else
    mNcpSpinel.SrpServerSetEnabled(/* aEnabled */ true);
#endif
#endif
    mIsInitialized = true;

#if OTBR_ENABLE_MDNS
    OpenMdnsSocket();
#endif

#if OTBR_ENABLE_TREL
    mNcpSpinel.SetTrelStateChangedCallback(
        [this](bool aEnabled, uint16_t aPort) { HandleTrelStateChanged(aEnabled, aPort); });
#endif // OTBR_ENABLE_TREL

    mNcpSpinel.SetUdpForwardSendCallback(
        [this](const uint8_t *aUdpPayload, uint16_t aLength, const otIp6Address &aPeerAddr, uint16_t aPeerPort,
               uint16_t aLocalPort) { HandleUdpForwardSend(aUdpPayload, aLength, aPeerAddr, aPeerPort, aLocalPort); });
}

void NcpHost::Deinit(void)
{
    mIsInitialized = false;
#if OTBR_ENABLE_MDNS
    CloseMdnsSocket();
#endif
#if OTBR_ENABLE_TREL
    CloseTrelSocket();
#endif
    mNcpSpinel.Deinit();
    otSysDeinit();
}

void NcpHost::Join(const otOperationalDatasetTlvs &aActiveOpDatasetTlvs, const AsyncResultReceiver &aReceiver)
{
    AsyncTaskPtr task;
    auto errorHandler = [aReceiver](otError aError, const std::string &aErrorInfo) { aReceiver(aError, aErrorInfo); };

    task = std::make_shared<AsyncTask>(errorHandler);
    task->First([this, aActiveOpDatasetTlvs](AsyncTaskPtr aNext) {
            mNcpSpinel.DatasetSetActiveTlvs(aActiveOpDatasetTlvs, std::move(aNext));
        })
        ->Then([this](AsyncTaskPtr aNext) { mNcpSpinel.Ip6SetEnabled(true, std::move(aNext)); })
        ->Then([this](AsyncTaskPtr aNext) { mNcpSpinel.ThreadSetEnabled(true, std::move(aNext)); });
    task->Run();
}

void NcpHost::Leave(bool aEraseDataset, const AsyncResultReceiver &aReceiver)
{
    AsyncTaskPtr task;
    auto errorHandler = [aReceiver](otError aError, const std::string &aErrorInfo) { aReceiver(aError, aErrorInfo); };

    task = std::make_shared<AsyncTask>(errorHandler);
    task->First([this](AsyncTaskPtr aNext) { mNcpSpinel.ThreadDetachGracefully(std::move(aNext)); })
        ->Then([this, aEraseDataset](AsyncTaskPtr aNext) {
            if (aEraseDataset)
            {
                mNcpSpinel.ThreadErasePersistentInfo(std::move(aNext));
            }
            else
            {
                aNext->SetResult(OT_ERROR_NONE, "");
            }
        });
    task->Run();
}

void NcpHost::ScheduleMigration(const otOperationalDatasetTlvs &aPendingOpDatasetTlvs,
                                const AsyncResultReceiver       aReceiver)
{
    otDeviceRole role  = GetDeviceRole();
    otError      error = OT_ERROR_NONE;
    auto errorHandler  = [aReceiver](otError aError, const std::string &aErrorInfo) { aReceiver(aError, aErrorInfo); };

    VerifyOrExit(role != OT_DEVICE_ROLE_DISABLED && role != OT_DEVICE_ROLE_DETACHED, error = OT_ERROR_INVALID_STATE);

    mNcpSpinel.DatasetMgmtSetPending(std::make_shared<otOperationalDatasetTlvs>(aPendingOpDatasetTlvs),
                                     std::make_shared<AsyncTask>(errorHandler));

exit:
    if (error != OT_ERROR_NONE)
    {
        mTaskRunner.Post(
            [aReceiver, error](void) { aReceiver(error, "Cannot schedule migration when this device is detached"); });
    }
}

void NcpHost::SetThreadEnabled(bool aEnabled, const AsyncResultReceiver aReceiver)
{
    OT_UNUSED_VARIABLE(aEnabled);

    // TODO: Implement SetThreadEnabled under NCP mode.
    mTaskRunner.Post([aReceiver](void) { aReceiver(OT_ERROR_NOT_IMPLEMENTED, "Not implemented!"); });
}

void NcpHost::SetCountryCode(const std::string &aCountryCode, const AsyncResultReceiver &aReceiver)
{
    OT_UNUSED_VARIABLE(aCountryCode);

    // TODO: Implement SetCountryCode under NCP mode.
    mTaskRunner.Post([aReceiver](void) { aReceiver(OT_ERROR_NOT_IMPLEMENTED, "Not implemented!"); });
}

void NcpHost::GetChannelMasks(const ChannelMasksReceiver &aReceiver, const AsyncResultReceiver &aErrReceiver)
{
    OT_UNUSED_VARIABLE(aReceiver);

    // TODO: Implement GetChannelMasks under NCP mode.
    mTaskRunner.Post([aErrReceiver](void) { aErrReceiver(OT_ERROR_NOT_IMPLEMENTED, "Not implemented!"); });
}

#if OTBR_ENABLE_POWER_CALIBRATION
void NcpHost::SetChannelMaxPowers(const std::vector<ChannelMaxPower> &aChannelMaxPowers,
                                  const AsyncResultReceiver          &aReceiver)
{
    OT_UNUSED_VARIABLE(aChannelMaxPowers);

    // TODO: Implement SetChannelMaxPowers under NCP mode.
    mTaskRunner.Post([aReceiver](void) { aReceiver(OT_ERROR_NOT_IMPLEMENTED, "Not implemented!"); });
}
#endif

void NcpHost::AddThreadStateChangedCallback(ThreadStateChangedCallback aCallback)
{
    // TODO: Implement AddThreadStateChangedCallback under NCP mode.
    OT_UNUSED_VARIABLE(aCallback);
}

void NcpHost::AddThreadEnabledStateChangedCallback(ThreadEnabledStateCallback aCallback)
{
    // TODO: Implement AddThreadEnabledStateChangedCallback under NCP mode.
    OT_UNUSED_VARIABLE(aCallback);
}

void NcpHost::SetHostPowerState(uint8_t aState, const AsyncResultReceiver &aReceiver)
{
    AsyncTaskPtr task;
    auto errorHandler = [aReceiver](otError aError, const std::string &aErrorInfo) { aReceiver(aError, aErrorInfo); };

    task = std::make_shared<AsyncTask>(errorHandler);
    task->First([this, aState](AsyncTaskPtr aNext) { mNcpSpinel.SetHostPowerState(aState, std::move(aNext)); });
    task->Run();
}

#if OTBR_ENABLE_EPSKC
void NcpHost::EnableEphemeralKey(bool aEnable, const AsyncResultReceiver &aReceiver)
{
    AsyncTaskPtr task;
    auto errorHandler = [aReceiver](otError aError, const std::string &aErrorInfo) { aReceiver(aError, aErrorInfo); };

    task = std::make_shared<AsyncTask>(errorHandler);
    task->First([this, aEnable](AsyncTaskPtr aNext) { mNcpSpinel.EnableEphemeralKey(aEnable, std::move(aNext)); });
    task->Run();
}

void NcpHost::ActivateEphemeralKey(const char                *aPskc,
                                   uint32_t                   aDuration,
                                   uint16_t                   aPort,
                                   const AsyncResultReceiver &aReceiver)
{
    AsyncTaskPtr task;
    auto errorHandler = [aReceiver](otError aError, const std::string &aErrorInfo) { aReceiver(aError, aErrorInfo); };

    task = std::make_shared<AsyncTask>(errorHandler);
    task->First([this, aPskc, aDuration, aPort](AsyncTaskPtr aNext) {
        mNcpSpinel.ActivateEphemeralKey(aPskc, aDuration, aPort, std::move(aNext));
    });
    task->Run();
}

void NcpHost::DeactivateEphemeralKey(bool aRetainActiveSession, const AsyncResultReceiver &aReceiver)
{
    AsyncTaskPtr task;
    auto errorHandler = [aReceiver](otError aError, const std::string &aErrorInfo) { aReceiver(aError, aErrorInfo); };

    task = std::make_shared<AsyncTask>(errorHandler);
    task->First([this, aRetainActiveSession](AsyncTaskPtr aNext) {
        mNcpSpinel.DeactivateEphemeralKey(aRetainActiveSession, std::move(aNext));
    });
    task->Run();
}
#endif // OTBR_ENABLE_EPSKC

#if OTBR_ENABLE_BACKBONE_ROUTER
void NcpHost::SetBackboneRouterEnabled(bool aEnabled)
{
    mNcpSpinel.SetBackboneRouterEnabled(aEnabled);
}

void NcpHost::SetBackboneRouterMulticastListenerCallback(BackboneRouterMulticastListenerCallback aCallback)
{
    mNcpSpinel.SetBackboneRouterMulticastListenerCallback(std::move(aCallback));
}

void NcpHost::SetBackboneRouterStateChangedCallback(BackboneRouterStateChangedCallback aCallback)
{
    mNcpSpinel.SetBackboneRouterStateChangedCallback(std::move(aCallback));
}
#endif

void NcpHost::SetBorderAgentMeshCoPServiceChangedCallback(BorderAgentMeshCoPServiceChangedCallback aCallback)
{
    mNcpSpinel.SetBorderAgentMeshCoPServiceChangedCallback(aCallback);
}

void NcpHost::AddEphemeralKeyStateChangedCallback(EphemeralKeyStateChangedCallback aCallback)
{
    mNcpSpinel.AddEphemeralKeyStateChangedCallback(aCallback);
}

#if OTBR_ENABLE_BORDER_AGENT && !OTBR_ENABLE_BORDER_AGENT_MESHCOP_SERVICE
void NcpHost::SetBorderAgentVendorTxtData(const std::vector<uint8_t> &aVendorTxtData)
{
    // To be implemented
    OTBR_UNUSED_VARIABLE(aVendorTxtData);
}
#endif

void NcpHost::Process(const MainloopContext &aMainloop)
{
    mSpinelDriver.Process(&aMainloop);
    mCliDaemon.Process(aMainloop);
#if OTBR_ENABLE_MDNS
    ProcessMdnsSocket(aMainloop);
#endif
#if OTBR_ENABLE_TREL
    ProcessTrelSocket(aMainloop);
#endif
}

void NcpHost::Update(MainloopContext &aMainloop)
{
    mSpinelDriver.GetSpinelInterface()->UpdateFdSet(&aMainloop);

    if (mSpinelDriver.HasPendingFrame())
    {
        aMainloop.mTimeout.tv_sec  = 0;
        aMainloop.mTimeout.tv_usec = 0;
    }

    mCliDaemon.UpdateFdSet(aMainloop);
#if OTBR_ENABLE_MDNS
    UpdateMdnsSocketFdSet(aMainloop);
#endif
#if OTBR_ENABLE_TREL
    UpdateTrelSocketFdSet(aMainloop);
#endif
}

#if OTBR_ENABLE_MDNS
void NcpHost::SetMdnsPublisher(Mdns::Publisher *aPublisher)
{
    mNcpSpinel.SetMdnsPublisher(aPublisher);
}
#endif

#if OTBR_ENABLE_SRP_ADVERTISING_PROXY
void NcpHost::HandleMdnsState(Mdns::Publisher::State aState)
{
    mNcpSpinel.DnssdSetState(aState);
}
#endif

otbrError NcpHost::UdpForward(const uint8_t      *aUdpPayload,
                              uint16_t            aLength,
                              const otIp6Address &aRemoteAddr,
                              uint16_t            aRemotePort,
                              const UdpProxy     &aUdpProxy)
{
    return mNcpSpinel.UdpForward(aUdpPayload, aLength, aRemoteAddr, aRemotePort, aUdpProxy.GetThreadPort());
}

void NcpHost::SetUdpForwardToHostCallback(UdpForwardToHostCallback aCallback)
{
    mNcpSpinel.SetUdpForwardSendCallback(aCallback);
}

const otMeshLocalPrefix *NcpHost::GetMeshLocalPrefix(void) const
{
    return NcpNetworkProperties::GetMeshLocalPrefix();
}

void NcpHost::InitNetifCallbacks(Netif &aNetif)
{
    mNcpSpinel.Ip6SetAddressCallback(
        [&aNetif](const std::vector<Ip6AddressInfo> &aAddrInfos) { aNetif.UpdateIp6UnicastAddresses(aAddrInfos); });
    mNcpSpinel.Ip6SetAddressMulticastCallback(
        [&aNetif](const std::vector<Ip6Address> &aAddrs) { aNetif.UpdateIp6MulticastAddresses(aAddrs); });
    mNcpSpinel.NetifSetStateChangedCallback([&aNetif](bool aState) { aNetif.SetNetifState(aState); });
    mNcpSpinel.Ip6SetReceiveCallback(
        [&aNetif](const uint8_t *aData, uint16_t aLength) { aNetif.Ip6Receive(aData, aLength); });
}

#if OTBR_ENABLE_MDNS
void NcpHost::OpenMdnsSocket(void)
{
    int                 fd;
    int                 on = 1;
    int                 flags;
    struct sockaddr_in6 addr;
    struct ipv6_mreq    mreq;

    if (mMdnsSocket.mActive)
    {
        return;
    }

    fd = ::socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    if (fd < 0)
    {
        otbrLogWarning("mDNS socket create failed: %s", strerror(errno));
        return;
    }

    (void)setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    flags = fcntl(fd, F_GETFL, 0);
    if (flags != -1)
    {
        (void)fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin6_family = AF_INET6;
    addr.sin6_port   = htons(Mdns::kMdnsPort);
    addr.sin6_addr   = in6addr_any;

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
    {
        otbrLogWarning("mDNS socket bind(%u) failed: %s", Mdns::kMdnsPort, strerror(errno));
        close(fd);
        return;
    }

    memset(&mreq, 0, sizeof(mreq));
    mreq.ipv6mr_multiaddr.s6_addr[0]  = 0xff;
    mreq.ipv6mr_multiaddr.s6_addr[1]  = 0x02;
    mreq.ipv6mr_multiaddr.s6_addr[15] = 0xfb;
    mreq.ipv6mr_interface             = 0;

    if (setsockopt(fd, IPPROTO_IPV6, IPV6_JOIN_GROUP, &mreq, sizeof(mreq)) < 0)
    {
        otbrLogWarning("mDNS socket join multicast group failed: %s", strerror(errno));
        close(fd);
        return;
    }

    mMdnsSocket.mFd     = fd;
    mMdnsSocket.mActive = true;
    otbrLogInfo("mDNS socket opened on port %u", Mdns::kMdnsPort);
}

void NcpHost::CloseMdnsSocket(void)
{
    if (mMdnsSocket.mActive)
    {
        close(mMdnsSocket.mFd);
        mMdnsSocket.mFd     = -1;
        mMdnsSocket.mActive = false;
        otbrLogInfo("mDNS socket closed");
    }
}

static bool IsTrelMdnsPacket(const uint8_t *aData, uint16_t aLength)
{
    // DNS label encoding: each label is length-prefixed
    // "_trel._udp" encodes as: 0x05 't' 'r' 'e' 'l' 0x04 '_' 'u' 'd' 'p'
    static constexpr uint8_t  kTrelServicePattern[] = {0x05, 't', 'r', 'e', 'l', 0x04, '_', 'u', 'd', 'p'};
    static constexpr uint16_t kDnsHeaderSize        = 12;

    if (aLength < kDnsHeaderSize + sizeof(kTrelServicePattern))
    {
        return false;
    }

    for (uint16_t offset = kDnsHeaderSize; offset <= aLength - sizeof(kTrelServicePattern); offset++)
    {
        if (memcmp(&aData[offset], kTrelServicePattern, sizeof(kTrelServicePattern)) == 0)
        {
            return true;
        }
    }

    return false;
}

void NcpHost::ProcessMdnsSocket(const MainloopContext &aMainloop)
{
    if (!mMdnsSocket.mActive)
    {
        return;
    }

    if (FD_ISSET(mMdnsSocket.mFd, &aMainloop.mReadFdSet))
    {
        uint8_t             buffer[1280];
        struct sockaddr_in6 srcAddr;
        socklen_t           addrLen = sizeof(srcAddr);
        ssize_t             len;

        while ((len = recvfrom(mMdnsSocket.mFd, buffer, sizeof(buffer), 0, (struct sockaddr *)&srcAddr, &addrLen)) > 0)
        {
            otIp6Address peer;
            uint16_t     remotePort;

            // Only forward TREL-related mDNS packets to reduce NCP overhead
            if (!IsTrelMdnsPacket(buffer, static_cast<uint16_t>(len)))
            {
                continue;
            }

            memcpy(peer.mFields.m8, &srcAddr.sin6_addr, sizeof(peer.mFields.m8));
            remotePort = ntohs(srcAddr.sin6_port);

            mNcpSpinel.UdpForward(buffer, static_cast<uint16_t>(len), peer, remotePort, Mdns::kMdnsPort);
        }
    }
}

void NcpHost::UpdateMdnsSocketFdSet(MainloopContext &aMainloop)
{
    if (!mMdnsSocket.mActive)
    {
        return;
    }

    FD_SET(mMdnsSocket.mFd, &aMainloop.mReadFdSet);

    if (mMdnsSocket.mFd > aMainloop.mMaxFd)
    {
        aMainloop.mMaxFd = mMdnsSocket.mFd;
    }
}

#endif // OTBR_ENABLE_MDNS

#if OTBR_ENABLE_TREL
void NcpHost::HandleTrelStateChanged(bool aEnabled, uint16_t aPort)
{
    if (!aEnabled)
    {
        CloseTrelSocket();
    }
    else
    {
        // If enabled=true and port=0, OS will choose a random port
        OpenTrelSocket(aPort);
    }
}

void NcpHost::OpenTrelSocket(uint16_t aPort)
{
    if (mTrelSocket.mActive && mTrelSocket.mPort == aPort)
    {
        return;
    }
    CloseTrelSocket();

    int fd = ::socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    if (fd < 0)
    {
        otbrLogWarning("TREL socket create failed: %s", strerror(errno));
        return;
    }

    int on = 1;
    (void)setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    // Non-blocking
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags != -1)
    {
        (void)fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }

    struct sockaddr_in6 addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin6_family = AF_INET6;
    addr.sin6_port   = htons(aPort);
    addr.sin6_addr   = in6addr_any;
    if (bind(fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) < 0)
    {
        otbrLogWarning("TREL socket bind(%u) failed: %s", aPort, strerror(errno));
        close(fd);
        return;
    }

    // Get actual bound port (in case aPort was 0)
    socklen_t addrLen = sizeof(addr);
    if (getsockname(fd, reinterpret_cast<struct sockaddr *>(&addr), &addrLen) == 0)
    {
        uint16_t actualPort = ntohs(addr.sin6_port);
        mTrelSocket.mPort   = actualPort;

        // If actual port differs from requested, update NCP
        if (actualPort != aPort)
        {
            mNcpSpinel.UpdateTrelState(true, actualPort);
        }
    }
    else
    {
        mTrelSocket.mPort = aPort;
    }

    mTrelSocket.mFd     = fd;
    mTrelSocket.mActive = true;
    otbrLogInfo("TREL socket bound on UDP port %u", mTrelSocket.mPort);
}

void NcpHost::CloseTrelSocket(void)
{
    if (mTrelSocket.mActive)
    {
        close(mTrelSocket.mFd);
        mTrelSocket.mFd     = -1;
        mTrelSocket.mPort   = 0;
        mTrelSocket.mActive = false;
        otbrLogInfo("TREL socket closed");
    }
}

void NcpHost::ProcessTrelSocket(const MainloopContext &aMainloop)
{
    if (!mTrelSocket.mActive)
    {
        return;
    }

    if (FD_ISSET(mTrelSocket.mFd, &aMainloop.mReadFdSet))
    {
        uint8_t             buffer[1280];
        struct sockaddr_in6 srcAddr;
        socklen_t           addrLen = sizeof(srcAddr);
        ssize_t             len;

        while ((len = recvfrom(mTrelSocket.mFd, buffer, sizeof(buffer), 0, (struct sockaddr *)&srcAddr, &addrLen)) > 0)
        {
            otIp6Address peer;
            uint16_t     remotePort;

            memcpy(peer.mFields.m8, &srcAddr.sin6_addr, sizeof(peer.mFields.m8));
            remotePort = ntohs(srcAddr.sin6_port);

            mNcpSpinel.UdpForward(buffer, static_cast<uint16_t>(len), peer, remotePort, mTrelSocket.mPort);
        }
    }
}

void NcpHost::UpdateTrelSocketFdSet(MainloopContext &aMainloop)
{
    if (!mTrelSocket.mActive)
    {
        return;
    }

    FD_SET(mTrelSocket.mFd, &aMainloop.mReadFdSet);

    if (mTrelSocket.mFd > aMainloop.mMaxFd)
    {
        aMainloop.mMaxFd = mTrelSocket.mFd;
    }
}

#endif // OTBR_ENABLE_TREL

void NcpHost::HandleUdpForwardSend(const uint8_t      *aUdpPayload,
                                   uint16_t            aLength,
                                   const otIp6Address &aPeerAddr,
                                   uint16_t            aPeerPort,
                                   uint16_t            aLocalPort)
{
#if OTBR_ENABLE_MDNS
    if (aLocalPort == Mdns::kMdnsPort && mMdnsSocket.mActive)
    {
        struct sockaddr_in6 dest;
        memset(&dest, 0, sizeof(dest));
        dest.sin6_family = AF_INET6;
        memcpy(&dest.sin6_addr, &aPeerAddr, sizeof(dest.sin6_addr));
        dest.sin6_port = htons(aPeerPort);

        sendto(mMdnsSocket.mFd, aUdpPayload, aLength, 0, (struct sockaddr *)&dest, sizeof(dest));
    }
#endif

#if OTBR_ENABLE_TREL
    if (aLocalPort == mTrelSocket.mPort && mTrelSocket.mActive)
    {
        struct sockaddr_in6 dest;
        memset(&dest, 0, sizeof(dest));
        dest.sin6_family = AF_INET6;
        memcpy(&dest.sin6_addr, &aPeerAddr, sizeof(dest.sin6_addr));
        dest.sin6_port = htons(aPeerPort);

        sendto(mTrelSocket.mFd, aUdpPayload, aLength, 0, (struct sockaddr *)&dest, sizeof(dest));
    }
#endif
}

void NcpHost::InitInfraIfCallbacks(InfraIf &aInfraIf)
{
    mNcpSpinel.InfraIfSetIcmp6NdSendCallback(
        [&aInfraIf](uint32_t aInfraIfIndex, const otIp6Address &aAddr, const uint8_t *aData, uint16_t aDataLen) {
            OTBR_UNUSED_VARIABLE(aInfraIf.SendIcmp6Nd(aInfraIfIndex, aAddr, aData, aDataLen));
        });
}

otbrError NcpHost::Ip6Send(const uint8_t *aData, uint16_t aLength)
{
    return mNcpSpinel.Ip6Send(aData, aLength);
}

otbrError NcpHost::Ip6MulAddrUpdateSubscription(const otIp6Address &aAddress, bool aIsAdded)
{
    return mNcpSpinel.Ip6MulAddrUpdateSubscription(aAddress, aIsAdded);
}

otbrError NcpHost::SetInfraIf(uint32_t aInfraIfIndex, bool aIsRunning, const std::vector<Ip6Address> &aIp6Addresses)
{
    return mNcpSpinel.SetInfraIf(aInfraIfIndex, aIsRunning, aIp6Addresses);
}

otbrError NcpHost::HandleIcmp6Nd(uint32_t          aInfraIfIndex,
                                 const Ip6Address &aIp6Address,
                                 const uint8_t    *aData,
                                 uint16_t          aDataLen)
{
    return mNcpSpinel.HandleIcmp6Nd(aInfraIfIndex, aIp6Address, aData, aDataLen);
}

} // namespace Host
} // namespace otbr
