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
 *   The file implements the Thread commissioner for test.
 */

#include <stdio.h>
#include <string.h>

#include <arpa/inet.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <syslog.h>

#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif
#include <mbedtls/net_sockets.h>
#include <mbedtls/debug.h>
#include <mbedtls/ssl.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/ecjpake.h>
#include <mbedtls/error.h>
#include <mbedtls/certs.h>
#include <mbedtls/timing.h>

#include "agent/coap.hpp"
#include "agent/dtls.hpp"
#include "agent/uris.hpp"
#include "common/tlv.hpp"
#include "common/code_utils.hpp"
#include "utils/hex.hpp"

#define SERVER_PORT "49191"
#define SERVER_NAME "localhost"
#define SERVER_ADDR "127.0.0.1" /* forces IPv4 */

#define READ_TIMEOUT_MS 1000
#define MAX_RETRY       5

#define MBEDTLS_DEBUG_C
#define DEBUG_LEVEL 1

#define SYSLOG_IDENT "otbr-commissioner"

using namespace ot;
using namespace ot::BorderRouter;

const uint8_t kSeed[] = "Commissioner";
const uint8_t kPSKd[] = "123456";
const char    kCommissionerId[] = "OpenThread";
const int     kCipherSuites[] = {
    MBEDTLS_TLS_ECJPAKE_WITH_AES_128_CCM_8,
    0
};

const struct timeval kPollTimeout = {10, 0};

/**
 * Steering data for joiner 18b4300000000002 with password 123456
 */
const uint8_t kSteeringData[] = {
    0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/**
 * Constants
 */
enum
{
    kPortJoinerSession = 49192,
    kSizeMaxPacket     = 1500,
};

/**
 * Commissioner State
 */
enum
{
    kStateInvalid,
    kStateConnected,
    kStateAccepted,
    kStateReady,
    kStateAuthenticated,
    kStateFinalized,
    kStateDone,
    kStateError,
};

/**
 * Commissioner TLV types
 */
enum
{
    kJoinerDtlsEncapsulation
};

/**
 * Commissioner Context.
 */
struct Context
{
    Coap::Agent         *mCoap;
    Dtls::Server        *mDtlsServer;
    Dtls::Session       *mSession;

    mbedtls_ssl_context *mSsl;
    mbedtls_net_context *mNet;

    int                  mSocket;

    uint16_t             mToken;
    uint16_t             mCommissionerSessionId;
    uint8_t              mPSK[16];
    uint8_t              mKek[32];

    int                  mState;

    uint16_t             mJoinerUdpPort;
    uint8_t              mJoinerIid[8];
    uint16_t             mJoinerRouterLocator;
};

void HandleRelayReceive(const Coap::Resource &aResource, const Coap::Message &aMessage,
                        Coap::Message &aResponse,
                        const uint8_t *aIp6, uint16_t aPort, void *aContext);

void HandleJoinerFinalize(const Coap::Resource &aResource, const Coap::Message &aMessage,
                          Coap::Message &aResponse,
                          const uint8_t *aIp6, uint16_t aPort, void *aContext);

inline uint16_t LengthOf(const void *aStart, const void *aEnd)
{
    return static_cast<const uint8_t *>(aEnd) - static_cast<const uint8_t *>(aStart);
}

void HandleJoinerFinalize(const Coap::Resource &aResource, const Coap::Message &aRequest,
                          Coap::Message &aResponse,
                          const uint8_t *aIp6, uint16_t aPort, void *aContext)
{
    Context &context = *static_cast<Context *>(aContext);
    uint8_t  payload[10];
    Tlv     *responseTlv = reinterpret_cast<Tlv *>(payload);

    context.mState = kStateFinalized;
    responseTlv->SetType(Meshcop::kState);
    responseTlv->SetValue(static_cast<uint8_t>(1));
    responseTlv = responseTlv->GetNext();

    // Piggyback response
    aResponse.SetCode(Coap::kCodeChanged);
    aResponse.SetPayload(payload, LengthOf(payload, responseTlv));

    (void)aResource;
    (void)aRequest;
    (void)aIp6;
    (void)aPort;
}

void HandleRelayReceive(const Coap::Resource &aResource, const Coap::Message &aMessage,
                        Coap::Message &aResponse,
                        const uint8_t *aIp6, uint16_t aPort, void *aContext)
{
    int            ret = 0;
    uint16_t       length;
    Context       &context = *static_cast<Context *>(aContext);
    const uint8_t *payload = aMessage.GetPayload(length);

    for (const Tlv *requestTlv = reinterpret_cast<const Tlv *>(payload);
         LengthOf(payload, requestTlv) < length;
         requestTlv = requestTlv->GetNext())
    {
        switch (requestTlv->GetType())
        {
        case Meshcop::kJoinerDtlsEncapsulation:
            struct sockaddr_in addr;
            memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_port = htons(kPortJoinerSession);

            ret = sendto(context.mSocket, requestTlv->GetValue(), requestTlv->GetLength(),
                         0, reinterpret_cast<const struct sockaddr *>(&addr), sizeof(addr));
            VerifyOrExit(ret != -1, ret = errno);
            break;

        case Meshcop::kJoinerUdpPort:
            context.mJoinerUdpPort = requestTlv->GetValueUInt16();
            break;

        case Meshcop::kJoinerIid:
            memcpy(context.mJoinerIid, requestTlv->GetValue(), sizeof(context.mJoinerIid));
            break;

        case Meshcop::kJoinerRouterLocator:
            context.mJoinerRouterLocator = requestTlv->GetValueUInt16();
            break;

        default:
            break;
        }
    }

exit:

    (void)aResource;
    (void)aResponse;
    (void)aIp6;
    (void)aPort;

    return;
}

int SendRelayTransmit(Context &aContext)
{
    uint8_t payload[kSizeMaxPacket];
    uint8_t dtlsEncapsulation[kSizeMaxPacket];
    Tlv    *responseTlv = reinterpret_cast<Tlv *>(payload);

    ssize_t ret = recvfrom(aContext.mSocket, dtlsEncapsulation, sizeof(dtlsEncapsulation), 0, NULL, NULL);

    VerifyOrExit(ret > 0);

    responseTlv->SetType(Meshcop::kJoinerDtlsEncapsulation);
    responseTlv->SetValue(dtlsEncapsulation, static_cast<uint16_t>(ret));
    responseTlv = responseTlv->GetNext();

    responseTlv->SetType(Meshcop::kJoinerUdpPort);
    responseTlv->SetValue(aContext.mJoinerUdpPort);
    responseTlv = responseTlv->GetNext();

    responseTlv->SetType(Meshcop::kJoinerIid);
    responseTlv->SetValue(aContext.mJoinerIid, sizeof(aContext.mJoinerIid));
    responseTlv = responseTlv->GetNext();

    responseTlv->SetType(Meshcop::kJoinerRouterLocator);
    responseTlv->SetValue(aContext.mJoinerRouterLocator);
    responseTlv = responseTlv->GetNext();

    if (aContext.mState == kStateFinalized)
    {
        responseTlv->SetType(Meshcop::kJoinerRouterKek);
        responseTlv->SetValue(aContext.mKek, sizeof(aContext.mKek));
        responseTlv = responseTlv->GetNext();
    }

    {
        Coap::Message *message;
        uint16_t       token = ++aContext.mToken;

        message = aContext.mCoap->NewMessage(Coap::kTypeNonConfirmable, Coap::kCodePost,
                                             reinterpret_cast<const uint8_t *>(&token), sizeof(token));
        message->SetPath("c/tx");
        message->SetPayload(payload, LengthOf(payload, responseTlv));
        aContext.mCoap->Send(*message, NULL, 0, NULL, &aContext);
        aContext.mCoap->FreeMessage(message);
    }

exit:
    return ret;
}

static ssize_t SendCoap(const uint8_t *aBuffer, uint16_t aLength, const uint8_t *aIp6, uint16_t aPort,
                        void *aContext)
{
    ssize_t  ret = 0;
    Context &context = *static_cast<Context *>(aContext);

    if (aPort == 0)
    {
        ret = mbedtls_ssl_write(context.mSsl, aBuffer, aLength);
    }
    else
    {
        if (context.mSession)
        {
            ret = context.mSession->Write(aBuffer, aLength);
        }
        else
        {
            ret = -1;
        }
    }

    (void)aIp6;
    (void)aPort;

    return ret;
}

static void my_debug(void *ctx, int level,
                     const char *file, int line,
                     const char *str)
{
    ((void) level);

    fprintf((FILE *) ctx, "%s:%04d: %s", file, line, str);
    fflush((FILE *) ctx);
}

int export_keys(void *aContext, const unsigned char *aMasterSecret, const unsigned char *aKeyBlock,
                size_t aMacLength, size_t aKeyLength, size_t aIvLength)
{
    (void)aContext;
    (void)aMasterSecret;
    (void)aKeyBlock;
    (void)aMacLength;
    (void)aKeyLength;
    (void)aIvLength;
    return 0;
}

void HandleCommissionerPetition(const Coap::Message &aMessage, void *aContext)
{
    uint16_t       length;
    const Tlv     *tlv;
    const uint8_t *payload;
    Context       &context = *static_cast<Context *>(aContext);

    payload = aMessage.GetPayload(length);
    tlv = reinterpret_cast<const Tlv *>(payload);

    while (LengthOf(payload, tlv) < length)
    {
        switch (tlv->GetType())
        {
        case Meshcop::kState:
            if (tlv->GetValueUInt8())
            {
                context.mState = kStateAccepted;
            }
            break;

        case Meshcop::kCommissionerSessionId:
            context.mCommissionerSessionId = tlv->GetValueUInt16();
            break;

        default:
            break;
        }
        tlv = tlv->GetNext();
    }
}

int CommissionerPetition(Context &aContext)
{
    int     ret;
    uint8_t buffer[kSizeMaxPacket];
    Tlv    *tlv = reinterpret_cast<Tlv *>(buffer);

    Coap::Message *message;
    uint16_t       token = ++aContext.mToken;

    token = htons(token);
    message = aContext.mCoap->NewMessage(Coap::kTypeConfirmable, Coap::kCodePost,
                                         reinterpret_cast<const uint8_t *>(&token), sizeof(token));
    tlv->SetType(Meshcop::kCommissionerId);
    tlv->SetValue(kCommissionerId, sizeof(kCommissionerId) - 1);
    tlv = tlv->GetNext();

    message->SetPath("c/cp");
    message->SetPayload(buffer, LengthOf(buffer, tlv));
    aContext.mCoap->Send(*message, NULL, 0, HandleCommissionerPetition, &aContext);
    aContext.mCoap->FreeMessage(message);

    do
    {
        ret = mbedtls_ssl_read(aContext.mSsl, buffer, sizeof(buffer));
        if (ret > 0)
        {
            aContext.mCoap->Input(buffer, static_cast<uint16_t>(ret), NULL, 0);
            switch (aContext.mState)
            {
            case kStateAccepted:
                ret = 0;
                break;
            case kStateConnected:
                ret = MBEDTLS_ERR_SSL_WANT_READ;
                break;
            default:
                ret = -1;
                break;
            }
        }
    }
    while (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE);


    return ret;
}

void HandleCommissionerSetResponse(const Coap::Message &aMessage, void *aContext)
{
    uint16_t       length;
    const Tlv     *tlv;
    const uint8_t *payload;
    Context       &context = *static_cast<Context *>(aContext);

    payload = (aMessage.GetPayload(length));
    tlv = reinterpret_cast<const Tlv *>(payload);

    while (LengthOf(payload, tlv) < length)
    {
        switch (tlv->GetType())
        {
        case Meshcop::kState:
            if (tlv->GetValueUInt8())
            {
                context.mState = kStateReady;
            }
            break;

        case Meshcop::kCommissionerSessionId:
            context.mCommissionerSessionId = tlv->GetValueUInt16();
            break;

        default:
            break;
        }
        tlv = tlv->GetNext();
    }
}

int CommissionerSet(Context &aContext)
{
    int      ret = 0;
    uint16_t token = ++aContext.mToken;
    uint8_t  buffer[kSizeMaxPacket];
    Tlv     *tlv = reinterpret_cast<Tlv *>(buffer);

    Coap::Message *message;

    token = htons(token);
    message = aContext.mCoap->NewMessage(Coap::kTypeConfirmable, Coap::kCodePost,
                                         reinterpret_cast<const uint8_t *>(&token), sizeof(token));

    tlv->SetType(Meshcop::kCommissionerSessionId);
    tlv->SetValue(aContext.mCommissionerSessionId);
    tlv = tlv->GetNext();

    tlv->SetType(Meshcop::kSteeringData);
    tlv->SetValue(kSteeringData, sizeof(kSteeringData));
    tlv = tlv->GetNext();

    message->SetPath("c/cs");
    message->SetPayload(buffer, LengthOf(buffer, tlv));
    aContext.mCoap->Send(*message, NULL, 0, HandleCommissionerSetResponse, &aContext);
    aContext.mCoap->FreeMessage(message);

    do
    {
        ret = mbedtls_ssl_read(aContext.mSsl, buffer, sizeof(buffer));
        if (ret > 0)
        {
            aContext.mCoap->Input(buffer, static_cast<uint16_t>(ret), NULL, 0);
            switch (aContext.mState)
            {
            case kStateReady:
                ret = 0;
                break;

            case kStateAccepted:
                ret = MBEDTLS_ERR_SSL_WANT_READ;
                break;

            default:
                break;
            }
        }
    }
    while (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE);

    return ret;
}

void HandleCommissionerKeepAlive(const Coap::Message &aMessage, void *aContext)
{
    uint16_t       length;
    const Tlv     *tlv;
    const uint8_t *payload;
    Context       &context = *static_cast<Context *>(aContext);

    payload = (aMessage.GetPayload(length));
    tlv = reinterpret_cast<const Tlv *>(payload);

    while (LengthOf(payload, tlv) < length)
    {
        switch (tlv->GetType())
        {
        case Meshcop::kState:
            if (tlv->GetValueUInt8())
            {
                context.mState = kStateReady;
            }
            else
            {
                context.mState = kStateError;
            }
            break;

        default:
            break;
        }
        tlv = tlv->GetNext();
    }
}

int CommissionerKeepAlive(Context &aContext)
{
    int     ret = 0;
    uint8_t buffer[kSizeMaxPacket];
    Tlv    *tlv = reinterpret_cast<Tlv *>(buffer);

    Coap::Message *message;

    aContext.mToken++;
    message = aContext.mCoap->NewMessage(Coap::kTypeConfirmable, Coap::kCodePost,
                                         reinterpret_cast<const uint8_t *>(&aContext.mToken), sizeof(aContext.mToken));

    tlv->SetType(Meshcop::kState);
    tlv->SetValue(static_cast<uint8_t>(1));
    tlv = tlv->GetNext();

    tlv->SetType(Meshcop::kCommissionerSessionId);
    tlv->SetValue(aContext.mCommissionerSessionId);
    tlv = tlv->GetNext();

    message->SetPath("c/ca");
    message->SetPayload(buffer, LengthOf(buffer, tlv));
    aContext.mCoap->Send(*message, NULL, 0, HandleCommissionerKeepAlive, &aContext);
    aContext.mCoap->FreeMessage(message);

    do
    {
        ret = mbedtls_ssl_read(aContext.mSsl, buffer, sizeof(buffer));
    }
    while (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE);

    if (ret > 0)
    {
        aContext.mCoap->Input(buffer, static_cast<uint16_t>(ret), NULL, 0);
        if (aContext.mState == kStateAccepted)
        {
            ret = 0;
        }
        else
        {
            ret = -1;
        }
    }

    return ret;
}

int CommissionerSessionProcess(Context &context)
{
    uint8_t buf[kSizeMaxPacket];
    int     ret;

    ret = mbedtls_ssl_read(context.mSsl, buf, sizeof(buf));
    if (ret > 0)
    {
        context.mCoap->Input(buf, static_cast<uint16_t>(ret), NULL, 0);
    }

    return ret;
}

void FeedCoaps(const uint8_t *aBuffer, uint16_t aLength, void *aContext)
{
    Context &context = *static_cast<Context *>(aContext);

    context.mCoap->Input(aBuffer, aLength, NULL, 1);
}

void HandleSessionChange(Dtls::Session &aSession, Dtls::Session::State aState, void *aContext)
{
    Context &context = *static_cast<Context *>(aContext);

    switch (aState)
    {
    case Dtls::Session::kStateReady:
        context.mState = kStateAuthenticated;
        memcpy(context.mKek, aSession.GetKek(), sizeof(context.mKek));
        aSession.SetDataHandler(FeedCoaps, aContext);
        context.mSession = &aSession;
        break;

    case Dtls::Session::kStateClose:
        context.mState = kStateDone;
        break;

    case Dtls::Session::kStateError:
    case Dtls::Session::kStateEnd:
        context.mSession = NULL;
        break;
    default:
        break;
    }
}

int CommissionerServe(Context &aContext)
{
    fd_set readFdSet;
    fd_set writeFdSet;
    fd_set errorFdSet;
    int    ret = 0;

    aContext.mSocket = socket(AF_INET, SOCK_DGRAM, 0);
    VerifyOrExit(aContext.mSocket != -1, ret = errno);
    aContext.mDtlsServer = Dtls::Server::Create(kPortJoinerSession, HandleSessionChange, &aContext);
    aContext.mDtlsServer->SetPSK(kPSKd, strlen(reinterpret_cast<const char *>(kPSKd)));
    aContext.mDtlsServer->Start();

    while (aContext.mState != kStateDone && aContext.mState != kStateError)
    {
        struct timeval timeout = kPollTimeout;
        int            maxFd = -1;

        FD_ZERO(&readFdSet);
        FD_ZERO(&writeFdSet);
        FD_ZERO(&errorFdSet);

        FD_SET(aContext.mNet->fd, &readFdSet);
        FD_SET(aContext.mSocket, &readFdSet);
        aContext.mDtlsServer->UpdateFdSet(readFdSet, writeFdSet, errorFdSet, maxFd, timeout);
        ret = select(maxFd + 1, &readFdSet, &writeFdSet, &errorFdSet, &timeout);
        if ((ret < 0) && (errno != EINTR))
        {
            perror("select");
            break;
        }

        if (FD_ISSET(aContext.mSocket, &readFdSet))
        {
            SendRelayTransmit(aContext);
        }

        if (FD_ISSET(aContext.mNet->fd, &readFdSet))
        {
            ret = CommissionerSessionProcess(aContext);
            VerifyOrExit(ret > 0 || ret == MBEDTLS_ERR_SSL_TIMEOUT);
        }

        aContext.mDtlsServer->Process(readFdSet, writeFdSet, errorFdSet);
    }

    aContext.mSession->Close();
    Dtls::Server::Destroy(aContext.mDtlsServer);
    close(aContext.mSocket);
    if (aContext.mState == kStateDone)
    {
        ret = 0;
    }
    else
    {
        ret = -1;
    }

exit:

    return ret;
}

int run(Context &context)
{
    int                          ret;
    mbedtls_net_context          client_fd;
    mbedtls_entropy_context      entropy;
    mbedtls_ctr_drbg_context     ctr_drbg;
    mbedtls_ssl_context          ssl;
    mbedtls_ssl_config           conf;
    mbedtls_timing_delay_context timer;

    Coap::Resource relayReceiveHandler(OT_URI_PATH_RELAY_RX, HandleRelayReceive, &context);
    Coap::Resource joinerFinalizeHandler(OT_URI_PATH_JOINER_FINALIZE, HandleJoinerFinalize, &context);

    context.mSsl = &ssl;
    context.mNet = &client_fd;
#if defined(MBEDTLS_DEBUG_C)
    mbedtls_debug_set_threshold(DEBUG_LEVEL);
#endif

    mbedtls_net_init(&client_fd);
    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&conf);
    mbedtls_ctr_drbg_init(&ctr_drbg);

    mbedtls_entropy_init(&entropy);
    if ((ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                                     kSeed,
                                     sizeof(kSeed))) != 0)
    {
        goto exit;
    }

    if ((ret = mbedtls_net_connect(&client_fd, SERVER_ADDR,
                                   SERVER_PORT, MBEDTLS_NET_PROTO_UDP)) != 0)
    {
        goto exit;
    }

    if ((ret = mbedtls_ssl_config_defaults(&conf,
                                           MBEDTLS_SSL_IS_CLIENT,
                                           MBEDTLS_SSL_TRANSPORT_DATAGRAM,
                                           MBEDTLS_SSL_PRESET_DEFAULT)) != 0)
    {
        goto exit;
    }

    mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);
    mbedtls_ssl_conf_min_version(&conf, MBEDTLS_SSL_MAJOR_VERSION_3, MBEDTLS_SSL_MINOR_VERSION_3);
    mbedtls_ssl_conf_max_version(&conf, MBEDTLS_SSL_MAJOR_VERSION_3, MBEDTLS_SSL_MINOR_VERSION_3);
    mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_NONE);
    mbedtls_ssl_conf_dbg(&conf, my_debug, stdout);
    mbedtls_ssl_conf_ciphersuites(&conf, kCipherSuites);
    mbedtls_ssl_conf_export_keys_cb(&conf, export_keys, NULL);
    mbedtls_ssl_conf_handshake_timeout(&conf, 8000, 60000);

    if ((ret = mbedtls_ssl_setup(&ssl, &conf)) != 0)
    {
        goto exit;
    }

    mbedtls_ssl_set_bio(&ssl, &client_fd,
                        mbedtls_net_send, mbedtls_net_recv, mbedtls_net_recv_timeout);

    mbedtls_ssl_set_timer_cb(&ssl, &timer, mbedtls_timing_set_delay,
                             mbedtls_timing_get_delay);

    mbedtls_ssl_set_hs_ecjpake_password(&ssl, context.mPSK, sizeof(context.mPSK));

    do
    {
        ret = mbedtls_ssl_handshake(&ssl);
    }
    while (ret == MBEDTLS_ERR_SSL_WANT_READ ||
           ret == MBEDTLS_ERR_SSL_WANT_WRITE);

    if (ret != 0)
    {
        goto exit;
    }

    context.mState = kStateConnected;
    context.mCoap = Coap::Agent::Create(SendCoap, &context);

    SuccessOrExit(ret = context.mCoap->AddResource(relayReceiveHandler));
    SuccessOrExit(ret = context.mCoap->AddResource(joinerFinalizeHandler));

    SuccessOrExit(ret = CommissionerPetition(context));
    SuccessOrExit(ret = CommissionerSet(context));
    SuccessOrExit(ret = CommissionerServe(context));

    do
    {
        ret = mbedtls_ssl_close_notify(&ssl);
    }
    while (ret == MBEDTLS_ERR_SSL_WANT_WRITE);
    ret = 0;

exit:

#ifdef MBEDTLS_ERROR_C
    if (ret != 0)
    {
        char error_buf[100];
        mbedtls_strerror(ret, error_buf, 100);
        printf("Last error was: %d - %s\n\n", ret, error_buf);
    }
#endif

    mbedtls_net_free(&client_fd);

    mbedtls_ssl_free(&ssl);
    mbedtls_ssl_config_free(&conf);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);

    /* Shell can not handle large exit numbers -> 1 for errors */
    if (ret < 0)
    {
        ret = -1;
    }

    return ret;
}

int main(int argc, char *argv[])
{
    int     ret = 0;
    Context context;

    if (argc != 2)
    {
        ExitNow(ret = 1);
    }

    ot::Utils::Hex2Bytes(argv[1], context.mPSK, sizeof(context.mPSK));

    openlog(SYSLOG_IDENT, LOG_CONS | LOG_PID, LOG_USER);

    ret = run(context);

exit:
    closelog();
    return ret;
}
