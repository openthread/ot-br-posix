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
 * This file implements the DTLS service.
 */

#include "dtls_mbedtls.hpp"

#include <stdexcept>
#include <algorithm>

#include <errno.h>
#include <sys/socket.h>

#include "common/code_utils.hpp"
#include "common/logging.hpp"
#include "common/time.hpp"

namespace ot {

namespace BorderRouter {

namespace Dtls {

enum
{
    kLogLevelNone,        ///< 0 No debug
    kLogLevelError,       ///< 1 Error
    kLogLevelStateChange, ///< 2 State change
    kLogLevelInfo,        ///< 3 Informational
    kLogLevelVerbose,     ///< 4 Verbose
};

static void MbedtlsDebug(void *ctx, int level,
                         const char *file, int line,
                         const char *str)
{
    // Debug levels of mbedtls from mbedtls documentation.
    //
    // 0 No debug
    // 1 Error
    // 2 State change
    // 3 Informational
    // 4 Verbose
    switch (level)
    {
    case 1:
        level = OTBR_LOG_ERR;
        break;
    case 2:
        level = OTBR_LOG_WARNING;
        break;
    case 3:
        level = OTBR_LOG_INFO;
        break;
    case 4:
        level = OTBR_LOG_DEBUG;
        break;
    default:
        level = OTBR_LOG_DEBUG;
    }

    otbrLog(level, "%s:%04d: %s", file, line, str);
    (void)ctx;
}

Server *Server::Create(uint16_t aPort, StateHandler aStateHandler, void *aContext)
{
    return new MbedtlsServer(aPort, aStateHandler, aContext);
}

void Server::Destroy(Server *aServer)
{
    delete static_cast<MbedtlsServer *>(aServer);
}

otbrError MbedtlsServer::Start(void)
{
    otbrError        ret = OTBR_ERROR_NONE;
    int              error = 0;
    static const int ciphersuites[] =
    {
        MBEDTLS_TLS_ECJPAKE_WITH_AES_128_CCM_8,
        0
    };

    mbedtls_ssl_config_init(&mConf);
    mbedtls_ssl_cookie_init(&mCookie);
#if defined(MBEDTLS_SSL_CACHE_C)
    mbedtls_ssl_cache_init(&mCache);
#endif
    mbedtls_entropy_init(&mEntropy);
    mbedtls_ctr_drbg_init(&mCtrDrbg);

    mbedtls_debug_set_threshold(kLogLevelError);

    otbrLog(OTBR_LOG_DEBUG, "Setting CTR_DRBG seed");
    SuccessOrExit(error = mbedtls_ctr_drbg_seed(&mCtrDrbg, mbedtls_entropy_func, &mEntropy, mSeed,
                                                mSeedLength));

    otbrLog(OTBR_LOG_DEBUG, "Configuring DTLS");
    SuccessOrExit(error = mbedtls_ssl_config_defaults(&mConf,
                                                      MBEDTLS_SSL_IS_SERVER,
                                                      MBEDTLS_SSL_TRANSPORT_DATAGRAM,
                                                      MBEDTLS_SSL_PRESET_DEFAULT));

    mbedtls_ssl_conf_rng(&mConf, mbedtls_ctr_drbg_random, &mCtrDrbg);
    mbedtls_ssl_conf_min_version(&mConf, MBEDTLS_SSL_MAJOR_VERSION_3, MBEDTLS_SSL_MINOR_VERSION_3);
    mbedtls_ssl_conf_max_version(&mConf, MBEDTLS_SSL_MAJOR_VERSION_3, MBEDTLS_SSL_MINOR_VERSION_3);
    mbedtls_ssl_conf_dbg(&mConf, MbedtlsDebug, this);
    mbedtls_ssl_conf_ciphersuites(&mConf, ciphersuites);
    mbedtls_ssl_conf_read_timeout(&mConf, 0);

#if defined(MBEDTLS_SSL_CACHE_C)
    mbedtls_ssl_conf_session_cache(&mConf, &mCache, mbedtls_ssl_cache_get, mbedtls_ssl_cache_set);
#endif

    otbrLog(OTBR_LOG_DEBUG, "Setting up cookie");
    SuccessOrExit(error = mbedtls_ssl_cookie_setup(&mCookie, mbedtls_ctr_drbg_random, &mCtrDrbg));

    mbedtls_ssl_conf_dtls_cookies(&mConf, mbedtls_ssl_cookie_write, mbedtls_ssl_cookie_check,
                                  &mCookie);

    SuccessOrExit(ret = Bind());

exit:

    if (error != 0)
    {
        otbrLog(OTBR_LOG_ERR, "mbedtls error: -0x%04x!", error);
        ret = OTBR_ERROR_DTLS;
    }

    return ret;
}

otbrError MbedtlsServer::Bind(void)
{
    otbrError           ret = OTBR_ERROR_ERRNO;
    int                 one = 1;
    struct sockaddr_in6 sin6;

    memset(&sin6, 0, sizeof(sin6));
    sin6.sin6_family = AF_INET6;
    sin6.sin6_port = htons(mPort);

    VerifyOrExit((mSocket = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP)) != -1);
    // This option enables retrieving the original destination IPv6 address.
    SuccessOrExit(setsockopt(mSocket, IPPROTO_IPV6, IPV6_RECVPKTINFO, &one, sizeof(one)));
    // This option allows binding to the same address.
    SuccessOrExit(setsockopt(mSocket, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)));
    SuccessOrExit(bind(mSocket, reinterpret_cast<struct sockaddr *>(&sin6), sizeof(sin6)));

    otbrLog(OTBR_LOG_INFO, "Bound to port %u", mPort);
    ret = OTBR_ERROR_NONE;

exit:
    if (ret)
    {
        otbrLog(OTBR_LOG_ERR, "Failed to bind to port %u", strerror(errno));
    }

    return ret;
}

void MbedtlsSession::SetState(State aState)
{
    mState = aState;
    mServer.HandleSessionState(*this, aState);
}

void MbedtlsSession::SetDataHandler(DataHandler aDataHandler, void *aContext)
{
    mContext = aContext;
    mDataHandler = aDataHandler;
}

ssize_t MbedtlsSession::Write(const uint8_t *aBuffer, uint16_t aLength)
{
    int ret;

    do
    {
        ret = mbedtls_ssl_write(&mSsl, aBuffer, aLength);
    }
    while (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE);

    if (ret < 0)
    {
        SetState(kStateError);
    }

    return ret;
}

void MbedtlsSession::Close(void)
{
    VerifyOrExit(mState != kStateError && mState != kStateEnd);

    while (mbedtls_ssl_close_notify(&mSsl) == MBEDTLS_ERR_SSL_WANT_WRITE) ;
    SetState(kStateEnd);

exit:
    return;
}

MbedtlsSession::~MbedtlsSession(void)
{
    Close();
    // In case session socket is not actually created.
    if (mNet.fd != mServer.mSocket)
    {
        mbedtls_net_free(&mNet);
    }
    mbedtls_ssl_free(&mSsl);
    otbrLog(OTBR_LOG_INFO, "DTLS session destroyed: %d", mState);
}

void MbedtlsSession::Process(void)
{
    mExpiration = GetNow() + kSessionTimeout;

    switch (mState)
    {
    case kStateHandshaking:
        Handshake();
        break;

    case kStateReady:
        Read();
        break;

    default:
        break;
    }
}

int MbedtlsSession::Read(void)
{
    uint8_t buffer[kMaxSizeOfPacket];
    int     ret = 0;

    do
    {
        ret = mbedtls_ssl_read(&mSsl, buffer, sizeof(buffer));
        otbrLog(OTBR_LOG_DEBUG, "mbedtls_ssl_read returned %d", ret);

        if (ret > 0)
        {
            mDataHandler(buffer, (uint16_t)ret, mContext);
        }
    }
    while (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE);

    if (ret <= 0)
    {
        switch (ret)
        {
        // 0 for EOF
        case 0:
            // fall through
            ;

        case MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY:
            otbrLog(OTBR_LOG_WARNING, "connection was closed gracefully");
            SetState(kStateClose);
            break;

        case MBEDTLS_ERR_SSL_CLIENT_RECONNECT:
            otbrLog(OTBR_LOG_WARNING, "reconnection");
            SetState(kStateHandshaking);
            break;

        case MBEDTLS_ERR_SSL_TIMEOUT:
            otbrLog(OTBR_LOG_WARNING, "read timeout");
            break;

        default:
            otbrLog(OTBR_LOG_ERR, "mbedtls_ssl_read returned -0x%x", -ret);
            SetState(kStateError);
            break;
        }
    }

    return ret;
}

int MbedtlsSession::ExportKeys(void *aContext, const unsigned char *aMasterSecret, const unsigned char *aKeyBlock,
                               size_t aMacLength, size_t aKeyLength, size_t aIvLength)
{
    mbedtls_sha256_context sha256;

    mbedtls_sha256_init(&sha256);
    mbedtls_sha256_starts(&sha256, 0);
    mbedtls_sha256_update(&sha256, aKeyBlock, 2 * static_cast<uint16_t>(aMacLength + aKeyLength + aIvLength));
    mbedtls_sha256_finish(&sha256, static_cast<MbedtlsSession *>(aContext)->mKek);

    (void)aMasterSecret;
    return 0;
}

MbedtlsSession::MbedtlsSession(MbedtlsServer &aServer, const mbedtls_net_context &aNet,
                               const struct sockaddr_in6 &aRemoteSock,
                               const sockaddr_in6 &aLocalSock) :
    mNet(aNet),
    mRemoteSock(aRemoteSock),
    mLocalSock(aLocalSock),
    mServer(aServer)
{
    int ret = 0;

    mbedtls_ssl_init(&mSsl);
    SuccessOrExit(ret = mbedtls_ssl_setup(&mSsl, &mServer.mConf));

    mbedtls_ssl_set_timer_cb(&mSsl, &mTimer, mbedtls_timing_set_delay, mbedtls_timing_get_delay);

    SuccessOrExit(ret = mbedtls_ssl_session_reset(&mSsl));
    SuccessOrExit(ret = mbedtls_ssl_set_hs_ecjpake_password(&mSsl, mServer.mPSK, mServer.mPSKLength));
    SuccessOrExit(ret = mbedtls_ssl_set_client_transport_id(&mSsl,
                                                            reinterpret_cast<const unsigned char *>(&mRemoteSock),
                                                            sizeof(mRemoteSock)));
    mbedtls_ssl_set_bio(&mSsl, this, SendMbedtls, ReadMbedtls, NULL);

    mState = kStateHandshaking;

exit:
    if (ret)
    {
        otbrLog(OTBR_LOG_ERR, "Failed to create session: %d", ret);
        throw std::runtime_error("Failed to create session");
    }
}

int MbedtlsSession::ReadMbedtls(unsigned char *aBuffer, size_t aLength)
{
    return mbedtls_net_recv(&mNet, aBuffer, aLength);
}

int MbedtlsSession::SendMbedtls(const unsigned char *aBuffer, size_t aLength)
{
    int  opt = 1;
    int &fd = mNet.fd;
    int  ret = 0;

    VerifyOrExit((fd = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP)) != -1, ret = -1);
    SuccessOrExit(ret = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)));
    SuccessOrExit(ret = bind(fd, reinterpret_cast<const struct sockaddr *>(&mLocalSock), sizeof(mLocalSock)));
    SuccessOrExit(ret = connect(fd, reinterpret_cast<const struct sockaddr *>(&mRemoteSock), sizeof(mRemoteSock)));
    SuccessOrExit(ret = mbedtls_net_set_nonblock(&mNet));
    mbedtls_ssl_set_bio(&mSsl, &mNet, mbedtls_net_send, mbedtls_net_recv, NULL);
    VerifyOrExit((ret = mbedtls_net_send(&mNet, aBuffer, aLength)) != -1);

exit:
    return ret;
}

int MbedtlsSession::Handshake(void)
{
    int ret = 0;

    VerifyOrExit(mState == kStateHandshaking, otbrLog(OTBR_LOG_ERR, "Invalid state"));

    otbrLog(OTBR_LOG_INFO, "Performing DTLS handshake");

    SuccessOrExit(ret = mbedtls_ssl_handshake(&mSsl));

    otbrLog(OTBR_LOG_INFO, "DTLS session ready");

    SetState(kStateReady);


exit:

    if (ret)
    {
        if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE)
        {
            otbrLog(OTBR_LOG_INFO, "Handshake pending:-0x%x", -ret);
        }
        else
        {
            otbrLog(OTBR_LOG_ERR, "Handshake failed:-0x%x", -ret);
            if (ret != MBEDTLS_ERR_SSL_HELLO_VERIFY_REQUIRED)
            {
                mbedtls_ssl_send_alert_message(&mSsl, MBEDTLS_SSL_ALERT_LEVEL_FATAL,
                                               MBEDTLS_SSL_ALERT_MSG_HANDSHAKE_FAILURE);
            }
            mState = kStateError;
        }
    }

    return ret;
}

void MbedtlsServer::UpdateFdSet(fd_set &aReadFdSet, fd_set &aWriteFdSet, int &aMaxFd, timeval &aTimeout)
{
    uint64_t now = GetNow();
    uint64_t timeout = aTimeout.tv_sec * 1000 + aTimeout.tv_sec / 1000;

    for (SessionSet::iterator it = mSessions.begin();
         it != mSessions.end(); )
    {
        boost::shared_ptr<MbedtlsSession> session = *it;

        if (session->GetExpiration() <= now)
        {
            otbrLog(OTBR_LOG_INFO, "DTLS session timeout");
            HandleSessionState(*session, Session::kStateExpired);
            it = mSessions.erase(it);
        }
        else if (session->GetState() == Session::kStateReady ||
                 session->GetState() == Session::kStateHandshaking)
        {
            int      fd = session->GetFd();
            uint64_t sessionTimeout = session->GetExpiration() - now;

            otbrLog(OTBR_LOG_INFO, "DTLS session[%d] alive", fd);
            FD_SET(fd, &aReadFdSet);

            if (aMaxFd < fd)
            {
                aMaxFd = fd;
            }

            if (sessionTimeout < timeout)
            {
                timeout = sessionTimeout;
            }

            // TODO error set
            ++it;
        }
        else
        {
            it = mSessions.erase(it);
        }
    }

    FD_SET(mSocket, &aReadFdSet);

    if (aMaxFd < mSocket)
    {
        aMaxFd = mSocket;
    }

    aTimeout.tv_sec = timeout / 1000;
    aTimeout.tv_usec = (timeout % 1000) * 1000;

    (void)aWriteFdSet;
}

void MbedtlsServer::HandleSessionState(Session &aSession, Session::State aState)
{
    otbrLog(OTBR_LOG_INFO, "Session state changed to %d", aState);
    if (mStateHandler)
    {
        mStateHandler(aSession, aState, mContext);
    }
}

void MbedtlsServer::ProcessServer(const fd_set &aReadFdSet, const fd_set &aWriteFdSet)
{
    uint8_t       packet[kMaxSizeOfPacket];
    uint8_t       control[kMaxSizeOfControl];
    ssize_t       ret = 0;
    sockaddr_in6  src;
    sockaddr_in6  dst;
    struct msghdr msghdr;
    struct iovec  iov[1];

    VerifyOrExit(FD_ISSET(mSocket, &aReadFdSet));

    otbrLog(OTBR_LOG_INFO, "Trying to accept connection");
    memset(&src, 0, sizeof(src));
    memset(&dst, 0, sizeof(dst));
    memset(&msghdr, 0, sizeof(msghdr));
    msghdr.msg_name = &src;
    msghdr.msg_namelen = sizeof(src);
    memset(&iov, 0, sizeof(iov));
    iov[0].iov_base = packet;
    iov[0].iov_len = kMaxSizeOfPacket;
    msghdr.msg_iov = iov;
    msghdr.msg_iovlen = 1;
    msghdr.msg_control = control;
    msghdr.msg_controllen = sizeof(control);

    VerifyOrExit(recvmsg(mSocket, &msghdr, MSG_PEEK) > 0, ret = -1);

    for (struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msghdr); cmsg != NULL; cmsg = CMSG_NXTHDR(&msghdr, cmsg))
    {
        if (cmsg->cmsg_level == IPPROTO_IPV6 && cmsg->cmsg_type == IPV6_PKTINFO)
        {
            const struct in6_pktinfo *pktinfo = reinterpret_cast<const struct in6_pktinfo *>(CMSG_DATA(cmsg));
            memcpy(dst.sin6_addr.s6_addr, pktinfo->ipi6_addr.s6_addr, sizeof(dst.sin6_addr));
            dst.sin6_family = AF_INET6;
            dst.sin6_port = ntohs(mPort);
            break;
        }
    }

    VerifyOrExit(memcmp(dst.sin6_addr.s6_addr, in6addr_any.s6_addr, sizeof(dst.sin6_addr)) != 0, ret = -1);
    // TODO Should check if this client has an existing session.
    {
        mbedtls_net_context               net = {
            mSocket
        };
        boost::shared_ptr<MbedtlsSession> session(new MbedtlsSession(*this, net, src, dst));
        mSessions.push_back(session);
        mbedtls_ssl_conf_export_keys_cb(&mConf, MbedtlsSession::ExportKeys, session.get());
        session->Process();
    }

exit:
    if (ret)
    {
        otbrLog(OTBR_LOG_ERR, "Failed to initiate new session: -0x%x", -ret);
    }

    (void)aWriteFdSet;
}

void MbedtlsServer::Process(const fd_set &aReadFdSet, const fd_set &aWriteFdSet)
{
    for (SessionSet::iterator it = mSessions.begin();
         it != mSessions.end();
         ++it)
    {
        boost::shared_ptr<MbedtlsSession> session = *it;
        int                               fd = session->GetFd();

        if (FD_ISSET(fd, &aReadFdSet))
        {
            otbrLog(OTBR_LOG_INFO, "DTLS session [%d] readable", fd);
            session->Process();
        }
    }

    ProcessServer(aReadFdSet, aWriteFdSet);
}

void MbedtlsServer::SetPSK(const uint8_t *aPSK, uint8_t aLength)
{
    otbrDump(OTBR_LOG_DEBUG, "DTLS PSK:", aPSK, aLength);

    mPSKLength = aLength;
    memcpy(mPSK, aPSK, aLength);
}

MbedtlsServer::~MbedtlsServer(void)
{
    close(mSocket);
    mbedtls_ssl_config_free(&mConf);
    mbedtls_ssl_cookie_free(&mCookie);
#if defined(MBEDTLS_SSL_CACHE_C)
    mbedtls_ssl_cache_free(&mCache);
#endif
    mbedtls_ctr_drbg_free(&mCtrDrbg);
    mbedtls_entropy_free(&mEntropy);
}

void MbedtlsServer::SetSeed(const uint8_t *aSeed, uint16_t aLength)
{
    VerifyOrExit(aLength <= sizeof(mSeed),
                 otbrLog(OTBR_LOG_ERR, "Seed must be no more than %zu bytes", sizeof(mSeed)));

    memcpy(mSeed, aSeed, aLength);
    mSeedLength = aLength;

exit:
    return;
}

} // namespace Dtls

} // namespace BorderRouter

} // namespace ot
