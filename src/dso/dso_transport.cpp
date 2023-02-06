/*
 *    Copyright (c) 2023, The OpenThread Authors.
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

#if OTBR_ENABLE_DNS_DSO

#define OTBR_LOG_TAG "DSO"

#include "dso_transport.hpp"

#include <cinttypes>
#include <csignal>
#include <memory>
#include <netdb.h>
#include <sys/types.h>

#include "mbedtls/net_sockets.h"
#include "openthread/openthread-system.h"
#include "openthread/platform/dso_transport.h"

static otbr::Dso::DsoAgent *sDsoAgent = nullptr;

extern "C" void otPlatDsoEnableListening(otInstance *aInstance, bool aEnabled)
{
    sDsoAgent->SetEnabled(aInstance, aEnabled);
}

extern "C" void otPlatDsoConnect(otPlatDsoConnection *aConnection, const otSockAddr *aPeerSockAddr)
{
    sDsoAgent->FindOrCreateConnection(aConnection)->Connect(aPeerSockAddr);
}

extern "C" void otPlatDsoSend(otPlatDsoConnection *aConnection, otMessage *aMessage)
{
    auto conn = sDsoAgent->FindConnection(aConnection);

    VerifyOrExit(conn != nullptr);
    conn->Send(aMessage);

exit:
    otMessageFree(aMessage);
}

extern "C" void otPlatDsoDisconnect(otPlatDsoConnection *aConnection, otPlatDsoDisconnectMode aMode)
{
    auto conn = sDsoAgent->FindConnection(aConnection);

    VerifyOrExit(conn != nullptr);
    conn->Disconnect(aMode);

    sDsoAgent->RemoveConnection(aConnection);

exit:
    return;
}

namespace otbr {
namespace Dso {

DsoAgent::DsoAgent(void)
{
    signal(SIGPIPE, SIG_IGN);
    mbedtls_net_init(&mListeningCtx);
    sDsoAgent = this;
}

DsoAgent::~DsoAgent(void)
{
    mbedtls_net_free(&mListeningCtx);
}

void DsoAgent::Init(otInstance *aInstance, const std::string &aInfraNetIfName)
{
    assert(mInstance == nullptr);

    mInstance       = aInstance;
    mInfraNetIfName = aInfraNetIfName;
}

DsoAgent::DsoConnection *DsoAgent::FindConnection(otPlatDsoConnection *aConnection)
{
    DsoConnection *ret = nullptr;
    auto           it  = mMap.find(aConnection);

    if (it != mMap.end())
    {
        ret = it->second.get();
    }

    return ret;
}

DsoAgent::DsoConnection *DsoAgent::FindOrCreateConnection(otPlatDsoConnection *aConnection)
{
    auto &ret = mMap[aConnection];

    if (!ret)
    {
        ret = MakeUnique<DsoConnection>(this, aConnection);
    }

    return ret.get();
}

DsoAgent::DsoConnection *DsoAgent::FindOrCreateConnection(otPlatDsoConnection *aConnection, mbedtls_net_context aCtx)
{
    auto &ret = mMap[aConnection];

    if (!ret)
    {
        ret = MakeUnique<DsoConnection>(this, aConnection, aCtx);
    }

    return ret.get();
}

void DsoAgent::Enable(otInstance *aInstance)
{
    OTBR_UNUSED_VARIABLE(aInstance);

    assert(aInstance == mInstance);

    constexpr int kOne = 1;
    sockaddr_in6  sockAddr;

    VerifyOrExit(!mListeningEnabled);

    mListeningCtx.fd = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);

    VerifyOrExit(
        !setsockopt(mListeningCtx.fd, SOL_SOCKET, SO_BINDTODEVICE, mInfraNetIfName.c_str(), mInfraNetIfName.size()));
    VerifyOrExit(!setsockopt(mListeningCtx.fd, SOL_SOCKET, SO_REUSEADDR, (const uint8_t *)&kOne, sizeof(kOne)));
    VerifyOrExit(!setsockopt(mListeningCtx.fd, SOL_SOCKET, SO_REUSEPORT, (const uint8_t *)&kOne, sizeof(kOne)));

    sockAddr.sin6_family = AF_INET6;
    sockAddr.sin6_addr   = in6addr_any;
    sockAddr.sin6_port   = htons(kListeningPort);
    VerifyOrExit(!bind(mListeningCtx.fd, reinterpret_cast<sockaddr *>(&sockAddr), sizeof(sockAddr)));
    VerifyOrExit(!mbedtls_net_set_nonblock(&mListeningCtx), otbrLogWarning("Failed to set non-blocking: %d", errno));
    VerifyOrExit(!listen(mListeningCtx.fd, kMaxQueuedConnections));

    mListeningEnabled = true;

    otbrLogInfo("DSO socket starts listening");

exit:
    return;
}

void DsoAgent::Disable(otInstance *aInstance)
{
    OTBR_UNUSED_VARIABLE(aInstance);

    assert(aInstance == mInstance);

    VerifyOrExit(mListeningEnabled);

    mbedtls_net_close(&mListeningCtx);
    mbedtls_net_free(&mListeningCtx);
    mMap.clear();
    mListeningEnabled = false;

exit:
    return;
}

void DsoAgent::SetEnabled(otInstance *aInstance, bool aEnabled)
{
    if (aEnabled)
    {
        Enable(aInstance);
    }
    else
    {
        Disable(aInstance);
    }
}

void DsoAgent::RemoveConnection(otPlatDsoConnection *aConnection)
{
    mMap.erase(aConnection);
}

void DsoAgent::SetHandlers(AcceptHandler       aAcceptHandler,
                           ConnectedHandler    aConnectedHandler,
                           DisconnectedHandler aDisconnectedHandler,
                           ReceiveHandler      aReceiveHandler)
{
    mAcceptHandler       = std::move(aAcceptHandler);
    mConnectedHandler    = std::move(aConnectedHandler);
    mDisconnectedHandler = std::move(aDisconnectedHandler);
    mReceiveHandler      = std::move(aReceiveHandler);
}

const char *DsoAgent::DsoConnection::StateToString(State aState)
{
    const char *ret = "";

    switch (aState)
    {
    case State::kDisabled:
        ret = "Disabled";
        break;
    case State::kConnecting:
        ret = "Connecting";
        break;
    case State::kConnected:
        ret = "Connected";
        break;
    }

    return ret;
}

void DsoAgent::DsoConnection::Connect(const otSockAddr *aPeerSockAddr)
{
    int                 ret;
    char                addrBuf[OT_IP6_ADDRESS_STRING_SIZE];
    std::string         portString;
    struct sockaddr_in6 sockAddrIn6;

    VerifyOrExit(mState == State::kDisabled);

    mbedtls_net_init(&mCtx);
    mPeerSockAddr = *aPeerSockAddr;
    portString    = std::to_string(aPeerSockAddr->mPort);
    otIp6AddressToString(&aPeerSockAddr->mAddress, addrBuf, sizeof(addrBuf));

    otbrLogInfo("Connecting to [%s]:%s", addrBuf, portString.c_str());

    mCtx.fd = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
    VerifyOrExit(mCtx.fd > 0, otbrLogWarning("Failed to create a socket: %d", errno));
    VerifyOrExit(!mbedtls_net_set_nonblock(&mCtx), otbrLogWarning("Failed to set non-blocking: %d", errno));

    memset(&sockAddrIn6, 0, sizeof(sockAddrIn6));
    memcpy(&sockAddrIn6.sin6_addr, aPeerSockAddr->mAddress.mFields.m8, sizeof(sockAddrIn6.sin6_addr));
    sockAddrIn6.sin6_family = AF_INET6;
    sockAddrIn6.sin6_port   = htons(aPeerSockAddr->mPort);
    ret                     = connect(mCtx.fd, reinterpret_cast<sockaddr *>(&sockAddrIn6), sizeof(sockAddrIn6));

    otbrLogInfo("Connecting to [%s]:%s fd=%d", addrBuf, portString.c_str(), mCtx.fd);

    if (!ret)
    {
        otbrLogInfo("Connected [%s]:%s", addrBuf, portString.c_str());
        MarkStateAs(State::kConnected);
        mAgent->HandleConnected(this);
    }
    else if (errno == EAGAIN || errno == EINPROGRESS)
    {
        MarkStateAs(State::kConnecting);
    }

exit:
    if (mState == State::kDisabled)
    {
        mAgent->HandleDisconnected(this, OT_PLAT_DSO_DISCONNECT_MODE_FORCIBLY_ABORT);
    }
}

void DsoAgent::DsoConnection::Disconnect(otPlatDsoDisconnectMode aMode)
{
    struct linger l;

    VerifyOrExit(mState != State::kDisabled);

    switch (aMode)
    {
    case OT_PLAT_DSO_DISCONNECT_MODE_FORCIBLY_ABORT:
        l.l_onoff  = 1;
        l.l_linger = 0;
        setsockopt(mCtx.fd, SOL_SOCKET, SO_LINGER, &l, sizeof(l));
        break;
    case OT_PLAT_DSO_DISCONNECT_MODE_GRACEFULLY_CLOSE:
        break;
    default:
        otbrLogWarning("Unknown disconnection mode: %d", aMode);
        break;
    }

    mbedtls_net_close(&mCtx);
    mbedtls_net_free(&mCtx);
    MarkStateAs(State::kDisabled);

exit:
    return;
}

void DsoAgent::DsoConnection::Send(otMessage *aMessage)
{
    uint16_t             length = otMessageGetLength(aMessage);
    std::vector<uint8_t> buf(length);

    VerifyOrExit(length == otMessageRead(aMessage, 0, buf.data(), length),
                 otbrLogWarning("Failed to read message data"));
    Send(buf);

exit:
    return;
}

void DsoAgent::DsoConnection::Send(const std::vector<uint8_t> &aData)
{
    uint16_t lengthInBigEndian = htons(aData.size());

    VerifyOrExit(mState == State::kConnected);

    otbrLogDebug("Sending a message with length %zu", aData.size());

    mSendMessageBuffer.resize(mSendMessageBuffer.size() + sizeof(lengthInBigEndian));
    *reinterpret_cast<uint16_t *>(&mSendMessageBuffer[mSendMessageBuffer.size() - sizeof(lengthInBigEndian)]) =
        lengthInBigEndian;
    std::copy(aData.begin(), aData.end(), std::back_inserter(mSendMessageBuffer));

exit:
    return;
}

void DsoAgent::DsoConnection::HandleReceive(void)
{
    int     readLen, totalReadLen = 0;
    uint8_t buf[kRxBufferSize];

    VerifyOrExit(mState == State::kConnected);

    while (true)
    {
        size_t wantReadLen = mNeedBytes ? mNeedBytes : (sizeof(uint16_t) - mReceiveLengthBuffer.size());

        readLen = mbedtls_net_recv(&mCtx, buf, std::min(sizeof(buf), wantReadLen));

        assert(readLen <= static_cast<int>(std::min(sizeof(buf), wantReadLen)));

        if (readLen <= 0)
        {
            HandleMbedTlsError(readLen);
            VerifyOrExit(readLen != 0);
            VerifyOrExit(readLen != MBEDTLS_ERR_SSL_WANT_READ && readLen != MBEDTLS_ERR_SSL_WANT_WRITE);
            otbrLogWarning("Failed to receive message: %d", readLen);
            ExitNow();
        }

        totalReadLen += readLen;

        if (mNeedBytes)
        {
            std::copy(buf, buf + readLen, std::back_inserter(mReceiveMessageBuffer));
            mNeedBytes -= readLen;
            if (!mNeedBytes)
            {
                mAgent->HandleReceive(this, mReceiveMessageBuffer);
                mReceiveMessageBuffer.clear();
            }
        }
        else
        {
            assert(mReceiveLengthBuffer.size() < sizeof(uint16_t));
            assert(mReceiveMessageBuffer.empty());

            std::copy(buf, buf + readLen, std::back_inserter(mReceiveLengthBuffer));
            if (mReceiveLengthBuffer.size() == sizeof(uint16_t))
            {
                mNeedBytes = mReceiveLengthBuffer[0] << 8 | mReceiveLengthBuffer[1];
                mReceiveLengthBuffer.clear();
            }
        }
    }

exit:
    if (!totalReadLen && mState != State::kDisabled)
    {
        MarkStateAs(State::kDisabled);
        mAgent->HandleDisconnected(this, OT_PLAT_DSO_DISCONNECT_MODE_GRACEFULLY_CLOSE);
    }

    return;
}

void DsoAgent::DsoConnection::HandleMbedTlsError(int aError)
{
    VerifyOrExit(aError < 0);

    switch (aError)
    {
    case MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY:
        MarkStateAs(State::kDisabled);
        mAgent->HandleDisconnected(this, OT_PLAT_DSO_DISCONNECT_MODE_GRACEFULLY_CLOSE);
        break;
    case MBEDTLS_ERR_NET_CONN_RESET:
        MarkStateAs(State::kDisabled);
        mAgent->HandleDisconnected(this, OT_PLAT_DSO_DISCONNECT_MODE_FORCIBLY_ABORT);
        break;
    case MBEDTLS_ERR_SSL_WANT_READ:
    case MBEDTLS_ERR_SSL_WANT_WRITE:
        break;
    default:
        MarkStateAs(State::kDisabled);
        mAgent->HandleDisconnected(this, OT_PLAT_DSO_DISCONNECT_MODE_FORCIBLY_ABORT);
        break;
    }

exit:
    return;
}

void DsoAgent::DsoConnection::UpdateStateBySocketState(void)
{
    int       optVal = 0;
    socklen_t optLen = sizeof(optVal);

    if (!getsockopt(GetFd(), SOL_SOCKET, SO_ERROR, &optVal, &optLen))
    {
        if (!optVal)
        {
            MarkStateAs(State::kConnected);
            mAgent->HandleConnected(this);
        }
        else
        {
            MarkStateAs(State::kDisabled);
            mAgent->HandleDisconnected(this, OT_PLAT_DSO_DISCONNECT_MODE_FORCIBLY_ABORT);
        }
    }
    else
    {
        otbrLogWarning("Failed to query socket status: %d Fd = %d", errno, GetFd());
        MarkStateAs(State::kDisabled);
        mAgent->HandleDisconnected(this, OT_PLAT_DSO_DISCONNECT_MODE_FORCIBLY_ABORT);
    }
}

void DsoAgent::DsoConnection::MarkStateAs(State aState)
{
    VerifyOrExit(mState != aState);

    otbrLogInfo("Connection state changed: %s -> %s", StateToString(mState), StateToString(aState));
    mState = aState;

exit:
    return;
}

void DsoAgent::DsoConnection::FlushSendBuffer(void)
{
    int writeLen = mbedtls_net_send(&mCtx, mSendMessageBuffer.data(), mSendMessageBuffer.size());

    VerifyOrExit(mState == State::kConnected);

    if (writeLen < 0)
    {
        otbrLogWarning("Failed to send DSO message: %d", writeLen);
        HandleMbedTlsError(writeLen);
    }
    else
    {
        mSendMessageBuffer.erase(mSendMessageBuffer.begin(), mSendMessageBuffer.begin() + writeLen);
    }

exit:
    return;
}

void DsoAgent::Update(MainloopContext &aMainloop)
{
    if (mListeningEnabled)
    {
        FD_SET(mListeningCtx.fd, &aMainloop.mReadFdSet);
        aMainloop.mMaxFd = std::max(aMainloop.mMaxFd, mListeningCtx.fd);
    }

    for (const auto &pair : mMap)
    {
        switch (pair.second->GetState())
        {
        case DsoConnection::State::kDisabled:
            break;
        case DsoConnection::State::kConnecting:
            FD_SET(pair.second->GetFd(), &aMainloop.mWriteFdSet);
            aMainloop.mMaxFd = std::max(aMainloop.mMaxFd, pair.second->GetFd());
            break;
        case DsoConnection::State::kConnected:
            FD_SET(pair.second->GetFd(), &aMainloop.mReadFdSet);
            if (!pair.second->mSendMessageBuffer.empty())
            {
                FD_SET(pair.second->GetFd(), &aMainloop.mWriteFdSet);
            }
            aMainloop.mMaxFd = std::max(aMainloop.mMaxFd, pair.second->GetFd());
            break;
        default:
            break;
        }
    }
}

void DsoAgent::Process(const MainloopContext &aMainloop)
{
    if (FD_ISSET(mListeningCtx.fd, &aMainloop.mReadFdSet))
    {
        ProcessIncomingConnections();
    }
    ProcessConnections(aMainloop);
}

void DsoAgent::ProcessIncomingConnections()
{
    mbedtls_net_context incomingCtx;
    uint8_t             address[sizeof(sockaddr_in6)];
    size_t              len;
    int                 ret = 0;

    VerifyOrExit(mListeningEnabled);

    while (!(ret = mbedtls_net_accept(&mListeningCtx, &incomingCtx, &address, sizeof(address), &len)))
    {
        ProcessIncomingConnection(incomingCtx, address, len);
    }

    if (ret != MBEDTLS_ERR_SSL_WANT_READ)
    {
        otbrLogWarning("Failed to accept incoming connection: %d", ret);
    }

exit:

    return;
}

void DsoAgent::ProcessIncomingConnection(mbedtls_net_context aCtx, uint8_t *aAddress, size_t aAddressLength)
{
    otSockAddr           sockAddr;
    otPlatDsoConnection *conn;
    in6_addr            *addrIn6    = nullptr;
    bool                 successful = false;

    VerifyOrExit(!mbedtls_net_set_nonblock(&aCtx),
                 otbrLogWarning("Failed to set the socket as non-blocking: %d", errno));

    // TODO: support IPv4
    if (aAddressLength == OT_IP6_ADDRESS_SIZE)
    {
        Ip6Address address;

        addrIn6 = reinterpret_cast<in6_addr *>(aAddress);
        memcpy(&sockAddr.mAddress.mFields.m8, addrIn6, aAddressLength);
        sockAddr.mPort = 0; // Mbed TLS doesn't provide the client's port number.

        address.CopyFrom(*addrIn6);
        otbrLogInfo("Receiving connection from %s", address.ToString().c_str());
    }
    else
    {
        otbrLogInfo("Unsupported address length: %zu", aAddressLength);
        ExitNow();
    }

    conn = HandleAccept(mInstance, &sockAddr);

    VerifyOrExit(conn != nullptr, otbrLogInfo("Failed to accept connection"));

    HandleConnected(FindOrCreateConnection(conn, aCtx));
    successful = true;

exit:
    if (!successful)
    {
        mbedtls_net_close(&aCtx);
    }
}

void DsoAgent::ProcessConnections(const MainloopContext &aMainloop)
{
    std::vector<DsoConnection *> connections;

    connections.reserve(mMap.size());
    for (auto &conn : mMap)
    {
        connections.push_back(conn.second.get());
    }
    for (const auto &conn : connections)
    {
        switch (conn->GetState())
        {
        case DsoConnection::State::kDisabled:
            RemoveConnection(conn->GetOtPlatDsoConnection());
            break;
        case DsoConnection::State::kConnecting:
            if (FD_ISSET(conn->GetFd(), &aMainloop.mWriteFdSet))
            {
                conn->UpdateStateBySocketState();
            }
            break;
        case DsoConnection::State::kConnected:
            if (FD_ISSET(conn->GetFd(), &aMainloop.mReadFdSet))
            {
                conn->HandleReceive();
            }
            if (FD_ISSET(conn->GetFd(), &aMainloop.mWriteFdSet))
            {
                conn->FlushSendBuffer();
            }
            break;
        default:
            break;
        }
    }
}

otPlatDsoConnection *DsoAgent::HandleAccept(otInstance *aInstance, otSockAddr *aSockAddr)
{
    assert(mAcceptHandler != nullptr);

    return mAcceptHandler(aInstance, aSockAddr);
}

void DsoAgent::HandleConnected(DsoConnection *aConnection)
{
    if (mConnectedHandler)
    {
        mConnectedHandler(aConnection);
    }
}

void DsoAgent::HandleDisconnected(DsoConnection *aConnection, otPlatDsoDisconnectMode aMode)
{
    if (mDisconnectedHandler)
    {
        mDisconnectedHandler(aConnection, aMode);
    }
}

void DsoAgent::HandleReceive(DsoConnection *aConnection, const std::vector<uint8_t> &aData)
{
    if (mReceiveHandler)
    {
        mReceiveHandler(aConnection, aData);
    }
}

otPlatDsoConnection *DsoAgent::DefaultAcceptHandler(otInstance *aInstance, otSockAddr *aSockAddr)
{
    return otPlatDsoAccept(aInstance, aSockAddr);
}

void DsoAgent::DefaultConnectedHandler(DsoConnection *aConnection)
{
    otPlatDsoHandleConnected(aConnection->GetOtPlatDsoConnection());
}

void DsoAgent::DefaultDisconnectedHandler(DsoConnection *aConnection, otPlatDsoDisconnectMode aMode)
{
    otPlatDsoHandleDisconnected(aConnection->GetOtPlatDsoConnection(), aMode);
}

void DsoAgent::DefaultReceiveHandler(DsoConnection *aConnection, const std::vector<uint8_t> &aData)
{
    otError    error;
    otMessage *message = otIp6NewMessage(aConnection->mAgent->mInstance, nullptr);

    VerifyOrExit(message != nullptr, otbrLogWarning("Failed to allocate message buffer"));
    error = otMessageAppend(message, aData.data(), aData.size());
    VerifyOrExit(error == OT_ERROR_NONE,
                 otbrLogWarning("Failed to construct message: %s", otThreadErrorToString(error)));

    otPlatDsoHandleReceive(aConnection->GetOtPlatDsoConnection(), message);

exit:
    return;
}

} // namespace Dso
} // namespace otbr

#endif
