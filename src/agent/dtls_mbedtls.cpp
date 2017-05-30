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

#include <syslog.h>

#include "common/code_utils.hpp"
#include "common/time.hpp"

namespace ot {

namespace BorderAgent {

namespace Dtls {

static const char *kBorderAgentUdpPort = "49191";

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
        level = LOG_ERR;
        break;
    case 2:
        level = LOG_WARNING;
        break;
    case 3:
        level = LOG_INFO;
        break;
    case 4:
        level = LOG_DEBUG;
        break;
    default:
        level = LOG_DEBUG;
    }

    syslog(level, "%s:%04d: %s", file, line, str);
}

int export_keys(void *aContext, const unsigned char *aMasterSecret, const unsigned char *aKeyBlock,
                size_t aMacLength, size_t aKeyLength, size_t aIvLength)
{
    return 0;
}

Server *Server::Create(StateHandler aStateHandler, void *aContext)
{
    return new MbedtlsServer(aStateHandler, aContext);
}

void Server::Destroy(Server *aServer)
{
    delete static_cast<MbedtlsServer *>(aServer);
}

MbedtlsServer::MbedtlsServer(StateHandler aStateHandler, void *aContext) : mStateHandler(aStateHandler),
    mContext(aContext)
{
    int              ret = 0;
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

#if defined(MBEDTLS_DEBUG_C)
    mbedtls_debug_set_threshold(DEBUG_LEVEL);
#endif

    syslog(LOG_DEBUG, "Setting CTR_DRBG seed");
    SuccessOrExit(ret = mbedtls_ctr_drbg_seed(&mCtrDrbg, mbedtls_entropy_func, &mEntropy, mSeed,
                                              mSeedLength));

    syslog(LOG_DEBUG, "Configuring DTLS");
    SuccessOrExit(ret = mbedtls_ssl_config_defaults(&mConf,
                                                    MBEDTLS_SSL_IS_SERVER,
                                                    MBEDTLS_SSL_TRANSPORT_DATAGRAM,
                                                    MBEDTLS_SSL_PRESET_DEFAULT));

    mbedtls_ssl_conf_rng(&mConf, mbedtls_ctr_drbg_random, &mCtrDrbg);
    mbedtls_ssl_conf_min_version(&mConf, MBEDTLS_SSL_MAJOR_VERSION_3, MBEDTLS_SSL_MINOR_VERSION_3);
    mbedtls_ssl_conf_max_version(&mConf, MBEDTLS_SSL_MAJOR_VERSION_3, MBEDTLS_SSL_MINOR_VERSION_3);
    mbedtls_ssl_conf_dbg(&mConf, MbedtlsDebug, this);
    mbedtls_ssl_conf_ciphersuites(&mConf, ciphersuites);
    mbedtls_ssl_conf_export_keys_cb(&mConf, export_keys, NULL);

#if defined(MBEDTLS_SSL_CACHE_C)
    mbedtls_ssl_conf_session_cache(&mConf, &mCache, mbedtls_ssl_cache_get, mbedtls_ssl_cache_set);
#endif

    syslog(LOG_DEBUG, "Setting up cookie");
    SuccessOrExit(ret = mbedtls_ssl_cookie_setup(&mCookie, mbedtls_ctr_drbg_random, &mCtrDrbg));

    mbedtls_ssl_conf_dtls_cookies(&mConf, mbedtls_ssl_cookie_write, mbedtls_ssl_cookie_check,
                                  &mCookie);

    mbedtls_net_init(&mNet);

    syslog(LOG_DEBUG, "Binding to port %s", kBorderAgentUdpPort);
    SuccessOrExit(ret = mbedtls_net_bind(&mNet, NULL, kBorderAgentUdpPort, MBEDTLS_NET_PROTO_UDP));

exit:
    if (ret != 0)
    {
        syslog(LOG_ERR, "mbedtls error: %d", ret);
        throw std::runtime_error("Failed to create DTLS server");
    }
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

MbedtlsSession::~MbedtlsSession(void)
{
    if (mState == kStateEnd || mState == kStateExpired)
    {
        while (mbedtls_ssl_close_notify(&mSsl) == MBEDTLS_ERR_SSL_WANT_WRITE) ;
    }

    mbedtls_net_free(&mNet);
    mbedtls_ssl_free(&mSsl);
    syslog(LOG_INFO, "DTLS session destroyed");
}

void MbedtlsSession::Process(void)
{
    uint8_t buffer[kMaxPacketSize];
    int     ret = 0;

    mExpiration = GetNow() + kSessionTimeout;

    do
    {
        ret = mbedtls_ssl_read(&mSsl, buffer, sizeof(buffer));
        syslog(LOG_DEBUG, "mbedtls_ssl_read returned %d", ret);

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
            syslog(LOG_WARNING, "connection was closed gracefully");
            SetState(kStateEnd);
            break;

        case MBEDTLS_ERR_SSL_CLIENT_RECONNECT:
            syslog(LOG_WARNING, "reconnection");
            SetState(kStateHandshaking);
            break;

        case MBEDTLS_ERR_SSL_TIMEOUT:
            syslog(LOG_WARNING, "timeout");
            SetState(kStateError);
            break;

        default:
            syslog(LOG_ERR, "mbedtls_ssl_read returned -0x%x", -ret);
            SetState(kStateError);
            break;
        }
    }
}

MbedtlsSession::MbedtlsSession(MbedtlsServer &aServer, mbedtls_net_context &aNet) :
    mNet(aNet),
    mServer(aServer)
{
    int ret = 0;

    mbedtls_ssl_init(&mSsl);
    SuccessOrExit(ret = mbedtls_ssl_setup(&mSsl, &mServer.mConf));

    mbedtls_ssl_set_timer_cb(&mSsl, &mTimer, mbedtls_timing_set_delay, mbedtls_timing_get_delay);

    mState = kStateHandshaking;

exit:
    if (ret)
    {
        syslog(LOG_ERR, "Failed to create session: %d", ret);
        throw std::runtime_error("Failed to create session");
    }
}

bool MbedtlsSession::Handshake(const uint8_t *aIp, size_t aIpLength)
{
    int ret = 0;

    VerifyOrExit(mState == kStateHandshaking, syslog(LOG_ERR, "Invalid state"));

    SuccessOrExit(ret = mbedtls_ssl_session_reset(&mSsl));
    SuccessOrExit(ret = mbedtls_ssl_set_hs_ecjpake_password(&mSsl, mServer.mPSK, kSizePSKc));
    SuccessOrExit(ret = mbedtls_ssl_set_client_transport_id(&mSsl, aIp, aIpLength));

    mbedtls_ssl_set_bio(&mSsl, &mNet, mbedtls_net_send, mbedtls_net_recv, mbedtls_net_recv_timeout);

    syslog(LOG_INFO, "Performing DTLS handshake");

    do
    {
        ret = mbedtls_ssl_handshake(&mSsl);
    }
    while (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE);

    SuccessOrExit(ret);

    syslog(LOG_INFO, "DTLS session ready");
    SetState(kStateReady);
    ret = 0;

exit:
    if (ret)
    {
        syslog(LOG_ERR, "Handshake failed:-0x%x", -ret);
    }

    mExpiration = GetNow() + kSessionTimeout;

    return mState == kStateReady;
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
            syslog(LOG_INFO, "DTLS session timeout");
            HandleSessionState(*session, Session::kStateExpired);
            it = mSessions.erase(it);
        }
        else if (session->GetState() == Session::kStateReady)
        {
            int      fd = session->GetFd();
            uint64_t sessionTimeout = session->GetExpiration() - now;

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

    FD_SET(mNet.fd, &aReadFdSet);

    if (aMaxFd < mNet.fd)
    {
        aMaxFd = mNet.fd;
    }

    aTimeout.tv_sec = timeout / 1000;
    aTimeout.tv_usec = (timeout % 1000) * 1000;
}

void MbedtlsServer::HandleSessionState(Session &aSession, Session::State aState)
{
    syslog(LOG_INFO, "Session state changed to %d", aState);
    if (mStateHandler)
    {
        mStateHandler(aSession, aState, mContext);
    }
}

void MbedtlsServer::ProcessServer(const fd_set &aReadFdSet, const fd_set &aWriteFdSet)
{
    int                 ret = 0;
    size_t              addrLength;
    Ip6Address          addr;
    mbedtls_net_context net;


    VerifyOrExit(FD_ISSET(mNet.fd, &aReadFdSet));

    syslog(LOG_INFO, "Trying to accept connection");
    mbedtls_net_init(&net);
    SuccessOrExit(ret = mbedtls_net_accept(&mNet, &net, addr.m8, sizeof(addr), &addrLength));

    // TODO Should check if this client has an existing session.
    {
        boost::shared_ptr<MbedtlsSession> session(new MbedtlsSession(*this, net));
        if (session->Handshake(addr.m8, addrLength))
        {
            mSessions.push_back(session);
        }
    }

exit:
    if (ret)
    {
        syslog(LOG_ERR, "Failed to initiate new session: -0x%x", -ret);
    }
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
            session->Process();
        }
    }

    ProcessServer(aReadFdSet, aWriteFdSet);
}

void MbedtlsServer::SetPSK(const uint8_t *aPSK)
{
    memcpy(mPSK, aPSK, kSizePSKc);
}

MbedtlsServer::~MbedtlsServer(void)
{
    mbedtls_net_free(&mNet);
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
                 syslog(LOG_ERR, "Seed must be no more than %u bytes", sizeof(mSeed)));

    memcpy(mSeed, aSeed, aLength);
    mSeedLength = aLength;

exit:
    return;
}

} // namespace Dtls

} // namespace BorderAgent

} // namespace ot
