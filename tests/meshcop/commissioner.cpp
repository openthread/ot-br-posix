/*
 *    Copyright (c) 2018, The OpenThread Authors.
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
 *   The file implements the commissioner class
 */

#include "addr_utils.hpp"
#include "commissioner.hpp"
#include "stdlib.h"
#include "string.h"
#include "utils.hpp"
#include <vector>
#include "agent/uris.hpp"
#include "common/code_utils.hpp"
#include "common/logging.hpp"
#include "common/tlv.hpp"
#include "utils/hex.hpp"
#include "web/pskc-generator/pskc.hpp"

namespace ot {
namespace BorderRouter {

const uint16_t Commissioner::kPortJoinerSession = 49192;
const uint8_t  Commissioner::kSeed[]            = "Commissioner";
const int      Commissioner::kCipherSuites[]    = {MBEDTLS_TLS_ECJPAKE_WITH_AES_128_CCM_8, 0};
const char     Commissioner::kCommissionerId[]  = "OpenThread";

static void my_debug(void *ctx, int level, const char *file, int line, const char *str)
{
    ((void)level);
    (void)(ctx);
#if MBED_LOG_TO_STDOUT
    fprintf((FILE *)ctx, "%s:%04d: %s", file, line, str);
    fflush((FILE *)ctx);
#endif
    char buf[100];
    strncpy(buf, str, sizeof(buf));

    /* mbed inserts EOL and so does otbrLog()
     * Thus we remove the one from MBED
     */
    char *cp;
    cp = strchr(buf, '\n');
    if (cp)
    {
        *cp = 0;
    }
    cp = strchr(buf, '\r');
    if (cp)
    {
        *cp = 0;
    }
    otbrLog(OTBR_LOG_INFO, "%s:%d: %s", file, line, buf);
}

/** dummy function for mbed */
static int export_keys(void *               aContext,
                       const unsigned char *aMasterSecret,
                       const unsigned char *aKeyBlock,
                       size_t               aMacLength,
                       size_t               aKeyLength,
                       size_t               aIvLength)
{
    (void)aContext;
    (void)aMasterSecret;
    (void)aKeyBlock;
    (void)aMacLength;
    (void)aKeyLength;
    (void)aIvLength;
    return 0;
}

Commissioner::Commissioner(const uint8_t *     aPskcBin,
                           const char *        aPskdAscii,
                           const SteeringData &aSteeringData,
                           int                 keepAliveRate)
    : mRelayReceiveHandler(OT_URI_PATH_RELAY_RX, Commissioner::HandleRelayReceive, this)
    , mJoinerSession(kPortJoinerSession, aPskdAscii)
    , mSteeringData(aSteeringData)
    , mKeepAliveRate(keepAliveRate)
{
    // Psk::Pskc      pskc;
    // const uint8_t *pskcBin = pskc.ComputePskc(aXpanidBin, aNetworkName, aPassphrase);
    memcpy(mPskcBin, aPskcBin, sizeof(mPskcBin));
    mJoinerSessionClientFd = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in addr;
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port        = htons(kPortJoinerSession);
    connect(mJoinerSessionClientFd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr));
}

int Commissioner::Connect(const sockaddr_in &aAgentAddr)
{
    int ret;
    SuccessOrExit(ret = DtlsHandShake(aAgentAddr));
    SuccessOrExit(ret = BecomeCommissioner());
exit:
    return ret;
}

void Commissioner::UpdateFdSet(fd_set & aReadFdSet,
                               fd_set & aWriteFdSet,
                               fd_set & aErrorFdSet,
                               int &    aMaxFd,
                               timeval &aTimeout)
{
    FD_SET(mSslClientFd.fd, &aReadFdSet);
    aMaxFd = utils::max(mSslClientFd.fd, aMaxFd);
    FD_SET(mJoinerSessionClientFd, &aReadFdSet);
    aMaxFd = utils::max(mJoinerSessionClientFd, aMaxFd);
    mJoinerSession.UpdateFdSet(aReadFdSet, aWriteFdSet, aErrorFdSet, aMaxFd, aTimeout);
    (void)aTimeout;
    (void)aWriteFdSet;
    (void)aErrorFdSet;
}

void Commissioner::Process(const fd_set &aReadFdSet, const fd_set &aWriteFdSet, const fd_set &aErrorFdSet)
{
    std::vector<int> closedClientFds;
    timeval          nowTime;
    mJoinerSession.Process(aReadFdSet, aWriteFdSet, aErrorFdSet);

    if (FD_ISSET(mSslClientFd.fd, &aReadFdSet))
    {
        int n = mbedtls_ssl_read(&mSsl, mIOBuffer, sizeof(mIOBuffer));
        if (n > 0)
        {
            printf("Get dtls input\n");
            mCoapAgent->Input(mIOBuffer, n, NULL, 0);
        }
    }

    if (FD_ISSET(mJoinerSessionClientFd, &aReadFdSet))
    {
        struct sockaddr_in from_addr;
        socklen_t          addrlen;
        ssize_t n = recvfrom(mJoinerSessionClientFd, mIOBuffer, sizeof(mIOBuffer), 0, (struct sockaddr *)(&from_addr),
                             &addrlen);
        if (n > 0)
        {
            {
                char buf[100];
                get_ip_str((struct sockaddr *)(&from_addr), buf, sizeof(buf));
                otbrLog(OTBR_LOG_INFO, "relay from: %s\n", buf);
            }
            SendRelayTransmit(mIOBuffer, n);
        }
    }
    gettimeofday(&nowTime, NULL);
    if (mKeepAliveRate > 0 && nowTime.tv_sec - mLastKeepAliveTime.tv_sec > mKeepAliveRate)
    {
        CommissionerKeepAlive();
    }
    (void)aWriteFdSet;
    (void)aErrorFdSet;
}

bool Commissioner::IsCommissioner()
{
    return mCommissionState == kStateReady;
}

int Commissioner::DtlsHandShake(const sockaddr_in &aAgentAddr)
{
    int  ret;
    char addressAscii[100];
    char portAscii[10];

    mbedtls_debug_set_threshold(4);
    get_ip_str(reinterpret_cast<const sockaddr *>(&aAgentAddr), addressAscii, sizeof(addressAscii));
    sprintf(portAscii, "%d", ntohs(aAgentAddr.sin_port));

    mbedtls_net_init(&mSslClientFd);
    mbedtls_ssl_init(&mSsl);
    mbedtls_ssl_config_init(&mSslConf);
    mbedtls_ctr_drbg_init(&mDrbg);
    mbedtls_entropy_init(&mEntropy);
    SuccessOrExit(ret = mbedtls_ctr_drbg_seed(&mDrbg, mbedtls_entropy_func, &mEntropy, kSeed, sizeof(kSeed)));
    SuccessOrExit(ret = mbedtls_net_connect(&mSslClientFd, addressAscii, portAscii, MBEDTLS_NET_PROTO_UDP));
    SuccessOrExit(ret = mbedtls_ssl_config_defaults(&mSslConf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_DATAGRAM,
                                                    MBEDTLS_SSL_PRESET_DEFAULT));
    mbedtls_ssl_conf_rng(&mSslConf, mbedtls_ctr_drbg_random, &mDrbg);
    mbedtls_ssl_conf_min_version(&mSslConf, MBEDTLS_SSL_MAJOR_VERSION_3, MBEDTLS_SSL_MINOR_VERSION_3);
    mbedtls_ssl_conf_max_version(&mSslConf, MBEDTLS_SSL_MAJOR_VERSION_3, MBEDTLS_SSL_MINOR_VERSION_3);
    mbedtls_ssl_conf_authmode(&mSslConf, MBEDTLS_SSL_VERIFY_NONE);
    mbedtls_ssl_conf_dbg(&mSslConf, my_debug, stdout);
    mbedtls_ssl_conf_ciphersuites(&mSslConf, kCipherSuites);
    mbedtls_ssl_conf_export_keys_cb(&mSslConf, export_keys, NULL);
    mbedtls_ssl_conf_handshake_timeout(&mSslConf, 8000, 60000);

    otbrLog(OTBR_LOG_INFO, "connecting: ssl-setup");
    SuccessOrExit(ret = mbedtls_ssl_setup(&mSsl, &mSslConf));
    mbedtls_ssl_set_bio(&mSsl, &mSslClientFd, mbedtls_net_send, mbedtls_net_recv, mbedtls_net_recv_timeout);
    mbedtls_ssl_set_timer_cb(&mSsl, &mTimer, mbedtls_timing_set_delay, mbedtls_timing_get_delay);

    for (size_t i = 0; i < OT_PSKC_LENGTH; i++)
    {
        printf("%02x ", mPskcBin[i]);
    }
    printf("\n");
    mbedtls_ssl_set_hs_ecjpake_password(&mSsl, mPskcBin, OT_PSKC_LENGTH);

    otbrLog(OTBR_LOG_INFO, "connect: perform handshake");
    do
    {
        ret = mbedtls_ssl_handshake(&mSsl);
    } while (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE);

    if (ret != 0)
    {
        otbrLog(OTBR_LOG_ERR, "Handshake fails");
        goto exit;
    }
exit:
    return ret;
}

int Commissioner::BecomeCommissioner()
{
    int ret;
    mCoapAgent = Coap::Agent::Create(SendCoap, this);
    mCoapToken = rand();
    SuccessOrExit(ret = mCoapAgent->AddResource(mRelayReceiveHandler));
    mCommissionState = kStateConnected;
    SuccessOrExit(ret = CommissionerPetition());
    SuccessOrExit(ret = CommissionerSet());
exit:
    return ret;
}

ssize_t Commissioner::SendCoap(const uint8_t *aBuffer,
                               uint16_t       aLength,
                               const uint8_t *aIp6,
                               uint16_t       aPort,
                               void *         aContext)
{
    (void)aIp6;
    (void)aPort;
    Commissioner *commissioner = reinterpret_cast<Commissioner *>(aContext);
    return mbedtls_ssl_write(&commissioner->mSsl, aBuffer, aLength);
}

int Commissioner::CommissionerPetition()
{
    int     ret;
    int     retryCount = 0;
    uint8_t buffer[kSizeMaxPacket];
    Tlv *   tlv = reinterpret_cast<Tlv *>(buffer);

    Coap::Message *message;
    uint16_t       token = ++mCoapToken;

    otbrLog(OTBR_LOG_INFO, "COMM_PET.req: start");
    while ((mCommissionState == kStateConnected || mCommissionState == kStateRejected) &&
           retryCount < kPetitionMaxRetry)
    {
        if (mCommissionState == kStateRejected)
        {
            sleep(kPetitionAttemptDelay);
            retryCount++;
        }
        token   = htons(token);
        message = mCoapAgent->NewMessage(Coap::kTypeConfirmable, Coap::kCodePost,
                                         reinterpret_cast<const uint8_t *>(&token), sizeof(token));
        tlv->SetType(Meshcop::kCommissionerId);
        tlv->SetValue(kCommissionerId, sizeof(kCommissionerId));
        tlv = tlv->GetNext();

        message->SetPath("c/cp");
        message->SetPayload(buffer, utils::LengthOf(buffer, tlv));
        otbrLog(OTBR_LOG_INFO, "COMM_PET.req: send");
        mCoapAgent->Send(*message, NULL, 0, HandleCommissionerPetition, this);
        mCoapAgent->FreeMessage(message);

        do
        {
            ret = mbedtls_ssl_read(&mSsl, buffer, sizeof(buffer));
            if (ret > 0)
            {
                mCoapAgent->Input(buffer, static_cast<uint16_t>(ret), NULL, 0);
                switch (mCommissionState)
                {
                case kStateConnected:
                    ret = MBEDTLS_ERR_SSL_WANT_READ;
                default:
                    break;
                }
            }
        } while (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE);
    }

    otbrLog(OTBR_LOG_INFO, "COMM_PET.req: complete");

    ret = mCommissionState == kStateAccepted ? 0 : -1;
    return ret;
}

void Commissioner::HandleRelayReceive(const Coap::Resource &aResource,
                                      const Coap::Message & aMessage,
                                      Coap::Message &       aResponse,
                                      const uint8_t *       aIp6,
                                      uint16_t              aPort,
                                      void *                aContext)
{
    int            ret = 0;
    int            tlvType;
    uint16_t       length;
    Commissioner * commissioner = reinterpret_cast<Commissioner *>(aContext);
    const uint8_t *payload      = aMessage.GetPayload(length);

    for (const Tlv *requestTlv = reinterpret_cast<const Tlv *>(payload); utils::LengthOf(payload, requestTlv) < length;
         requestTlv            = requestTlv->GetNext())
    {
        tlvType = requestTlv->GetType();
        switch (tlvType)
        {
        case Meshcop::kJoinerDtlsEncapsulation:
            struct sockaddr_in addr;
            memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_port   = htons(kPortJoinerSession);

            printf("Encapsulation: %d bytes for port: %d\n", requestTlv->GetLength(), kPortJoinerSession);
            otbrLog(OTBR_LOG_INFO, "Encapsulation: %d bytes for port: %d", requestTlv->GetLength(), kPortJoinerSession);
            {
                char buf[100];
                get_ip_str((struct sockaddr *)&addr, buf, sizeof(buf));
                otbrLog(OTBR_LOG_INFO, "DEST: %s", buf);
            }
            ret = send(commissioner->mJoinerSessionClientFd, requestTlv->GetValue(), requestTlv->GetLength(), 0);
            if (ret < 0)
            {
                otbrLog(OTBR_LOG_ERR, "relay receive, sendto() fails with %d", errno);
            }
            VerifyOrExit(ret != -1, ret = errno);
            break;

        case Meshcop::kJoinerUdpPort:
            commissioner->mJoinerUdpPort = requestTlv->GetValueUInt16();
            otbrLog(OTBR_LOG_INFO, "JoinerPort: %d", commissioner->mJoinerUdpPort);
            break;

        case Meshcop::kJoinerIid:
            memcpy(commissioner->mJoinerIid, requestTlv->GetValue(), sizeof(commissioner->mJoinerIid));
            break;

        case Meshcop::kJoinerRouterLocator:
            commissioner->mJoinerRouterLocator = requestTlv->GetValueUInt16();
            otbrLog(OTBR_LOG_INFO, "Router locator: %d", commissioner->mJoinerRouterLocator);
            break;

        default:
            otbrLog(OTBR_LOG_INFO, "skip tlv type: %d", tlvType);
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

int Commissioner::SendRelayTransmit(uint8_t *aBuf, size_t aLength)
{
    uint8_t payload[kSizeMaxPacket];
    Tlv *   responseTlv = reinterpret_cast<Tlv *>(payload);

    responseTlv->SetType(Meshcop::kJoinerDtlsEncapsulation);
    responseTlv->SetValue(aBuf, static_cast<uint16_t>(aLength));
    responseTlv = responseTlv->GetNext();

    responseTlv->SetType(Meshcop::kJoinerUdpPort);
    responseTlv->SetValue(mJoinerUdpPort);
    responseTlv = responseTlv->GetNext();

    responseTlv->SetType(Meshcop::kJoinerIid);
    responseTlv->SetValue(mJoinerIid, sizeof(mJoinerIid));
    responseTlv = responseTlv->GetNext();

    responseTlv->SetType(Meshcop::kJoinerRouterLocator);
    responseTlv->SetValue(mJoinerRouterLocator);
    responseTlv = responseTlv->GetNext();

    if (mJoinerSession.NeedAppendKek())
    {
        uint8_t kek[JoinerSession::kKekSize];
        mJoinerSession.GetKek(kek, sizeof(kek));
        mJoinerSession.MarkKekSent();
        otbrLog(OTBR_LOG_INFO, "realy: KEK state");
        responseTlv->SetType(Meshcop::kJoinerRouterKek);
        responseTlv->SetValue(kek, sizeof(kek));
        responseTlv = responseTlv->GetNext();
    }

    {
        Coap::Message *message;
        uint16_t       token = mCoapToken;

        message = mCoapAgent->NewMessage(Coap::kTypeNonConfirmable, Coap::kCodePost,
                                         reinterpret_cast<const uint8_t *>(&token), sizeof(token));
        message->SetPath("c/tx");
        message->SetPayload(payload, utils::LengthOf(payload, responseTlv));
        otbrLog(OTBR_LOG_INFO, "RELAY_tx.req: send");
        mCoapAgent->Send(*message, NULL, 0, NULL, this);
        mCoapAgent->FreeMessage(message);
    }

    return aLength;
}

/** handle c/cp response */
void Commissioner::HandleCommissionerPetition(const Coap::Message &aMessage, void *aContext)
{
    uint16_t       length;
    int            tlvType;
    const Tlv *    tlv;
    const uint8_t *payload;
    Commissioner * commissioner = reinterpret_cast<Commissioner *>(aContext);

    otbrLog(OTBR_LOG_INFO, "COMM_PET.rsp: start");
    payload = aMessage.GetPayload(length);
    tlv     = reinterpret_cast<const Tlv *>(payload);

    while (utils::LengthOf(payload, tlv) < length)
    {
        int8_t state = static_cast<int8_t>(tlv->GetValueUInt8());

        tlvType = tlv->GetType();
        switch (tlvType)
        {
        case Meshcop::kState:
            switch (state)
            {
            case Meshcop::kStateAccepted:
                otbrLog(OTBR_LOG_INFO, "COMM_PET.rsp: state=accepted");
                commissioner->mCommissionState = kStateAccepted;
                break;
            case Meshcop::kStateRejected:
                otbrLog(OTBR_LOG_INFO, "COMM_PET.rsp: state=rejected");
                commissioner->mCommissionState = kStateRejected;
                break;
            default:
                otbrLog(OTBR_LOG_INFO, "COMM_PET.rsp: state=%d", state);
                commissioner->mCommissionState = kStateInvalid;
                break;
            }
            break;
        case Meshcop::kCommissionerSessionId:
            commissioner->mCommissionerSessionId = tlv->GetValueUInt16();
            printf("COMM_PET.rsp: session-id=%d\n", commissioner->mCommissionerSessionId);
            otbrLog(OTBR_LOG_INFO, "COMM_PET.rsp: session-id=%d", commissioner->mCommissionerSessionId);
            break;

        default:
            otbrLog(OTBR_LOG_INFO, "COMM_PET.rsp: ignore-tlv: %d", tlvType);
            break;
        }
        tlv = tlv->GetNext();
    }

    gettimeofday(&commissioner->mLastKeepAliveTime, NULL);
    otbrLog(OTBR_LOG_INFO, "COMM_PET.rsp: complete");
}

int Commissioner::CommissionerSet()
{
    int      ret   = 0;
    uint16_t token = ++mCoapToken;
    uint8_t  buffer[kSizeMaxPacket];
    Tlv *    tlv = reinterpret_cast<Tlv *>(buffer);

    Coap::Message *message;

    otbrLog(OTBR_LOG_INFO, "COMMISSIONER_SET.req: start");
    printf("COMMISSIONER_SET.req: start\n");
    token   = htons(token);
    message = mCoapAgent->NewMessage(Coap::kTypeConfirmable, Coap::kCodePost, reinterpret_cast<const uint8_t *>(&token),
                                     sizeof(token));

    tlv->SetType(Meshcop::kCommissionerSessionId);
    tlv->SetValue(mCommissionerSessionId);
    otbrLog(OTBR_LOG_INFO, "COMMISSIONER_SET.req: session-id=%d", mCommissionerSessionId);
    printf("COMMISSIONER_SET.req: session-id=%d\n", mCommissionerSessionId);
    tlv = tlv->GetNext();

    tlv->SetType(Meshcop::kSteeringData);
    tlv->SetValue(mSteeringData.GetDataPointer(), mSteeringData.GetLength());
    printf("Steering data size %u\n", mSteeringData.GetLength());
    for (size_t i = 0; i < mSteeringData.GetLength(); i++)
    {
        printf("%02x ", mSteeringData.GetDataPointer()[i]);
    }
    printf("\n");
    tlv = tlv->GetNext();

    message->SetPath("c/cs");
    otbrLog(OTBR_LOG_INFO, "COMMISSIONER_SET.req: coap-uri: %s", "c/cs");
    message->SetPayload(buffer, utils::LengthOf(buffer, tlv));
    otbrLog(OTBR_LOG_INFO, "COMMISSIONER_SET.req: sent");
    printf("COMMISSIONER_SET.req: sent\n");
    mCoapAgent->Send(*message, NULL, 0, HandleCommissionerSet, this);
    mCoapAgent->FreeMessage(message);

    do
    {
        ret = mbedtls_ssl_read(&mSsl, buffer, sizeof(buffer));
        if (ret > 0)
        {
            mCoapAgent->Input(buffer, static_cast<uint16_t>(ret), NULL, 0);
            switch (mCommissionState)
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
    } while (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE);

    printf("COMMISSIONER_SET.req: done\n");
    return ret;
}

void Commissioner::HandleCommissionerSet(const Coap::Message &aMessage, void *aContext)
{
    uint16_t       length;
    int            tlvType;
    const Tlv *    tlv;
    const uint8_t *payload;
    Commissioner * commissioner = reinterpret_cast<Commissioner *>(aContext);

    otbrLog(OTBR_LOG_INFO, "COMMISSIONER_SET.rsp: start");
    payload = (aMessage.GetPayload(length));
    tlv     = reinterpret_cast<const Tlv *>(payload);

    while (utils::LengthOf(payload, tlv) < length)
    {
        tlvType      = tlv->GetType();
        int8_t state = static_cast<int8_t>(tlv->GetValueUInt8());

        tlvType = tlv->GetType();
        switch (tlvType)
        {
        case Meshcop::kState:
            switch (state)
            {
            case Meshcop::kStateAccepted:
                otbrLog(OTBR_LOG_INFO, "COMM_SET.rsp: state=accepted");
                commissioner->mCommissionState = kStateReady;
                break;
            case Meshcop::kStateRejected:
                otbrLog(OTBR_LOG_INFO, "COMM_SET.rsp: state=rejected");
                commissioner->mCommissionState = kStateRejected;
                break;
            default:
                otbrLog(OTBR_LOG_INFO, "COMM_SET.rsp: state=%d", state);
                commissioner->mCommissionState = kStateInvalid;
                break;
            }
            break;
        case Meshcop::kCommissionerSessionId:
            commissioner->mCommissionerSessionId = tlv->GetValueUInt16();
            printf("COMMISSIONER_SET.rsp: session-id=%d\n", commissioner->mCommissionerSessionId);
            otbrLog(OTBR_LOG_INFO, "COMMISSIONER_SET.rsp: session-id=%d", commissioner->mCommissionerSessionId);
            break;

        default:
            otbrLog(OTBR_LOG_INFO, "COMMISSIONER_SET.rsp: ignore-tlv=%d", tlvType);
            break;
        }
        tlv = tlv->GetNext();
    }
    printf("COMMISSIONER_SET.rsp: complete\n");
    otbrLog(OTBR_LOG_INFO, "COMMISSIONER_SET.rsp: complete");
}

void Commissioner::CommissionerKeepAlive()
{
    uint8_t buffer[kSizeMaxPacket];
    Tlv *   tlv = reinterpret_cast<Tlv *>(buffer);

    Coap::Message *message;

    mCoapToken++;
    message = mCoapAgent->NewMessage(Coap::kTypeConfirmable, Coap::kCodePost,
                                     reinterpret_cast<const uint8_t *>(&mCoapToken), sizeof(mCoapToken));

    tlv->SetType(Meshcop::kState);
    tlv->SetValue(static_cast<uint8_t>(1));
    tlv = tlv->GetNext();

    tlv->SetType(Meshcop::kCommissionerSessionId);
    tlv->SetValue(mCommissionerSessionId);
    tlv = tlv->GetNext();

    message->SetPath("c/ca");
    message->SetPayload(buffer, utils::LengthOf(buffer, tlv));

    otbrLog(OTBR_LOG_INFO, "COMM_KA.req: send");
    gettimeofday(&mLastKeepAliveTime, NULL);
    keepAliveTxCount += 1;
    mCoapAgent->Send(*message, NULL, 0, HandleCommissionerKeepAlive, this);
    mCoapAgent->FreeMessage(message);
}

/** Handle a COMM_KA response */
void Commissioner::HandleCommissionerKeepAlive(const Coap::Message &aMessage, void *aContext)
{
    uint16_t       length;
    int            tlvType;
    const Tlv *    tlv;
    const uint8_t *payload;
    Commissioner * commissioner = static_cast<Commissioner *>(aContext);

    otbrLog(OTBR_LOG_INFO, "COMM_KA.rsp: start");

    /* record stats */
    gettimeofday(&commissioner->mLastKeepAliveTime, NULL);
    commissioner->keepAliveRxCount += 1;

    payload = (aMessage.GetPayload(length));
    tlv     = reinterpret_cast<const Tlv *>(payload);

    while (utils::LengthOf(payload, tlv) < length)
    {
        int8_t state = static_cast<int8_t>(tlv->GetValueUInt8());
        tlvType      = tlv->GetType();

        tlvType = tlv->GetType();
        switch (tlvType)
        {
        case Meshcop::kState:
            switch (state)
            {
            case Meshcop::kStateAccepted:
                otbrLog(OTBR_LOG_INFO, "COMM_KA.rsp: state=accepted");
                commissioner->mCommissionState = kStateReady;
                break;
            case Meshcop::kStateRejected:
                otbrLog(OTBR_LOG_INFO, "COMM_KA.rsp: state=accepted");
                commissioner->mCommissionState = kStateRejected;
                break;
            default:
                otbrLog(OTBR_LOG_INFO, "COMM_KA.rsp: state=%d", state);
                commissioner->mCommissionState = kStateInvalid;
                break;
            }
            break;
        default:
            otbrLog(OTBR_LOG_INFO, "COMM_KA.rsp: ignore-tlv=%d", tlvType);
            break;
        }
        tlv = tlv->GetNext();
    }
    otbrLog(OTBR_LOG_INFO, "COMM_KA.rsp: complete");
}

void Commissioner::Commissioner::Disconnect()
{
    int ret;
    do
    {
        ret = mbedtls_ssl_close_notify(&mSsl);
    } while (ret == MBEDTLS_ERR_SSL_WANT_WRITE);

    mbedtls_net_free(&mSslClientFd);

    mbedtls_ssl_free(&mSsl);
    mbedtls_ssl_config_free(&mSslConf);
    mbedtls_ctr_drbg_free(&mDrbg);
    mbedtls_entropy_free(&mEntropy);

    close(mJoinerSessionClientFd);
    Coap::Agent::Destroy(mCoapAgent);
}

} // namespace BorderRouter
} // namespace ot
