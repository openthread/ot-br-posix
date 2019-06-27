/*
 *    Copyright (c) 2017-2018, The OpenThread Authors.
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

#include <vector>

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "addr_utils.hpp"
#include "commissioner.hpp"
#include "commissioner_utils.hpp"
#include "agent/uris.hpp"
#include "common/code_utils.hpp"
#include "common/logging.hpp"
#include "common/tlv.hpp"
#include "utils/hex.hpp"
#include "utils/pskc.hpp"
#include "utils/strcpy_utils.hpp"

namespace ot {
namespace BorderRouter {

const uint16_t Commissioner::kPortJoinerSession      = 49192;
const uint8_t  Commissioner::kSeed[]                 = "Commissioner";
const int      Commissioner::kCipherSuites[]         = {MBEDTLS_TLS_ECJPAKE_WITH_AES_128_CCM_8, 0};
const char     Commissioner::kCommissionerId[]       = "OpenThread";
const int      Commissioner::kCoapResponseWaitSecond = 10;
const int      Commissioner::kCoapResponseRetryTime  = 2;

static void MBedDebugPrint(void *aCtx, int aLevel, const char *aFile, int aLine, const char *aStr)
{
    char buf[100];
    /* mbed inserts EOL and so does otbrLog()
     * Thus we remove the one from MBED
     */
    char *cp;

    strcpy_safe(buf, sizeof(buf), aStr);
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
    otbrLog(OTBR_LOG_INFO, "%s:%d: %s", aFile, aLine, buf);

    (void)aLevel;
    (void)aCtx;
}

/** dummy function for mbed */
static int DummyKeyExport(void *               aContext,
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

Commissioner::Commissioner(const uint8_t *aPskcBin, int aKeepAliveRate)
    : mDtlsInitDone(false)
    , mRelayReceiveHandler(OT_URI_PATH_RELAY_RX, Commissioner::HandleRelayReceive, this)
    , mPetitionRetryCount(0)
    , mJoinerSession(NULL)
    , mKeepAliveRate(aKeepAliveRate)
    , mNumFinializeJoiners(0)
{
    sockaddr_in addr;

    memcpy(mPskcBin, aPskcBin, sizeof(mPskcBin));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port        = htons(kPortJoinerSession);
    mCoapAgent           = Coap::Agent::Create(SendCoap, this);
    mCoapToken           = static_cast<uint16_t>(rand());
    mCoapAgent->AddResource(mRelayReceiveHandler);
    mCommissionerState = CommissionerState::kStateInvalid;
    VerifyOrExit((mJoinerSessionClientFd = socket(AF_INET, SOCK_DGRAM, 0)) > 0);
    SuccessOrExit(connect(mJoinerSessionClientFd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)));
exit:
    return;
}

void Commissioner::SetJoiner(const char *aPskdAscii, const SteeringData &aSteeringData)
{
    if (mJoinerSession)
    {
        delete mJoinerSession;
    }
    mJoinerSession = new JoinerSession(kPortJoinerSession, aPskdAscii);
    CommissionerSet(aSteeringData);
}

ssize_t Commissioner::SendCoap(const uint8_t *aBuffer,
                               uint16_t       aLength,
                               const uint8_t *aIp6,
                               uint16_t       aPort,
                               void *         aContext)
{
    Commissioner *commissioner = static_cast<Commissioner *>(aContext);

    return mbedtls_ssl_write(&commissioner->mSsl, aBuffer, aLength);

    (void)aIp6;
    (void)aPort;
}

int Commissioner::InitDtls(const char *aHost, const char *aPort)
{
    int  ret;
    char addressAscii[kIPAddrNameBufSize];
    char portAscii[kPortNameBufSize];

    mbedtls_debug_set_threshold(kMBedDebugDefaultThreshold);

    strcpy(addressAscii, aHost);
    strcpy(portAscii, aPort);
    mbedtls_net_init(&mSslClientFd);
    mbedtls_ssl_init(&mSsl);
    mbedtls_ssl_config_init(&mSslConf);
    mbedtls_ctr_drbg_init(&mDrbg);
    mbedtls_entropy_init(&mEntropy);
    mDtlsInitDone = true;
    SuccessOrExit(ret = mbedtls_ctr_drbg_seed(&mDrbg, mbedtls_entropy_func, &mEntropy, kSeed, sizeof(kSeed)));
    SuccessOrExit(ret = mbedtls_net_connect(&mSslClientFd, addressAscii, portAscii, MBEDTLS_NET_PROTO_UDP));
    SuccessOrExit(ret = mbedtls_ssl_config_defaults(&mSslConf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_DATAGRAM,
                                                    MBEDTLS_SSL_PRESET_DEFAULT));
    mbedtls_ssl_conf_rng(&mSslConf, mbedtls_ctr_drbg_random, &mDrbg);
    mbedtls_ssl_conf_min_version(&mSslConf, MBEDTLS_SSL_MAJOR_VERSION_3, MBEDTLS_SSL_MINOR_VERSION_3);
    mbedtls_ssl_conf_max_version(&mSslConf, MBEDTLS_SSL_MAJOR_VERSION_3, MBEDTLS_SSL_MINOR_VERSION_3);
    mbedtls_ssl_conf_authmode(&mSslConf, MBEDTLS_SSL_VERIFY_NONE);
    mbedtls_ssl_conf_dbg(&mSslConf, MBedDebugPrint, stdout);
    mbedtls_ssl_conf_ciphersuites(&mSslConf, kCipherSuites);
    mbedtls_ssl_conf_export_keys_cb(&mSslConf, DummyKeyExport, NULL);
    mbedtls_ssl_conf_handshake_timeout(&mSslConf, kMbedDtlsHandshakeMinTimeout, kMbedDtlsHandshakeMaxTimeout);

    otbrLog(OTBR_LOG_INFO, "connecting: ssl-setup");
    SuccessOrExit(ret = mbedtls_ssl_setup(&mSsl, &mSslConf));
    mbedtls_ssl_set_bio(&mSsl, &mSslClientFd, mbedtls_net_send, mbedtls_net_recv, mbedtls_net_recv_timeout);
    mbedtls_ssl_set_timer_cb(&mSsl, &mTimer, mbedtls_timing_set_delay, mbedtls_timing_get_delay);

    mbedtls_ssl_set_hs_ecjpake_password(&mSsl, mPskcBin, OT_PSKC_LENGTH);

exit:
    return ret;
}

int Commissioner::TryDtlsHandshake(void)
{
    int ret = mbedtls_ssl_handshake(&mSsl);
    if (ret == 0)
    {
        mCommissionerState = CommissionerState::kStateConnected;
    }
    else if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE)
    {
        mCommissionerState = CommissionerState::kStateInvalid;
    }
    return ret;
}

void Commissioner::CommissionerPetition(void)
{
    int     retryCount = 0;
    uint8_t buffer[kSizeMaxPacket];
    Tlv *   tlv = reinterpret_cast<Tlv *>(buffer);

    Coap::Message *message;
    uint16_t       token = ++mCoapToken;

    otbrLog(OTBR_LOG_INFO, "COMM_PET.req: start");
    if (mCommissionerState == CommissionerState::kStateRejected)
    {
        sleep(kPetitionAttemptDelay);
        retryCount++;
    }
    token   = htons(token);
    message = mCoapAgent->NewMessage(Coap::kTypeConfirmable, Coap::kCodePost, reinterpret_cast<const uint8_t *>(&token),
                                     sizeof(token));
    tlv->SetType(Meshcop::kCommissionerId);
    tlv->SetValue(kCommissionerId, sizeof(kCommissionerId));
    tlv = tlv->GetNext();

    message->SetPath(OT_URI_PATH_COMMISSIONER_PETITION);
    message->SetPayload(buffer, Utils::LengthOf(buffer, tlv));
    otbrLog(OTBR_LOG_INFO, "COMM_PET.req: send");
    mCoapAgent->Send(*message, NULL, 0, HandleCommissionerPetition, this);
    mCoapAgent->FreeMessage(message);

    otbrLog(OTBR_LOG_INFO, "COMM_PET.req: complete");
}

void Commissioner::LogMeshcopState(const char *aPrefix, int8_t aState)
{
    switch (aState)
    {
    case Meshcop::kStateAccepted:
        otbrLog(OTBR_LOG_INFO, "%s: state=accepted", aPrefix);
        break;
    case Meshcop::kStateRejected:
        otbrLog(OTBR_LOG_INFO, "%s: state=rejected", aPrefix);
        break;
    default:
        otbrLog(OTBR_LOG_INFO, "%s: state=%d", aPrefix, aState);
        break;
    }
}

/** handle c/cp response */
void Commissioner::HandleCommissionerPetition(const Coap::Message &aMessage, void *aContext)
{
    uint16_t       length;
    int            tlvType;
    const Tlv *    tlv;
    const uint8_t *payload;
    Commissioner * commissioner = static_cast<Commissioner *>(aContext);

    otbrLog(OTBR_LOG_INFO, "COMM_PET.rsp: start");
    payload = aMessage.GetPayload(length);
    tlv     = reinterpret_cast<const Tlv *>(payload);

    while (Utils::LengthOf(payload, tlv) < length)
    {
        int8_t state = static_cast<int8_t>(tlv->GetValueUInt8());

        tlvType = tlv->GetType();
        switch (tlvType)
        {
        case Meshcop::kState:
            LogMeshcopState("COMM_PET.rsp", state);
            switch (state)
            {
            case Meshcop::kStateAccepted:
                commissioner->mCommissionerState = CommissionerState::kStateAccepted;
                break;
            case Meshcop::kStateRejected:
                commissioner->mCommissionerState = CommissionerState::kStateRejected;
                break;
            default:
                commissioner->mCommissionerState = CommissionerState::kStateInvalid;
                break;
            }
            break;
        case Meshcop::kCommissionerSessionId:
            commissioner->mCommissionerSessionId = tlv->GetValueUInt16();
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

    commissioner->CommissionerResponseNext();
}

void Commissioner::CommissionerSet(const SteeringData &aSteeringData)
{
    uint16_t       token = ++mCoapToken;
    uint8_t        buffer[kSizeMaxPacket];
    Tlv *          tlv = reinterpret_cast<Tlv *>(buffer);
    Coap::Message *message;

    otbrLog(OTBR_LOG_INFO, "COMMISSIONER_SET.req: start");
    token   = htons(token);
    message = mCoapAgent->NewMessage(Coap::kTypeConfirmable, Coap::kCodePost, reinterpret_cast<const uint8_t *>(&token),
                                     sizeof(token));

    tlv->SetType(Meshcop::kCommissionerSessionId);
    tlv->SetValue(mCommissionerSessionId);
    otbrLog(OTBR_LOG_INFO, "COMMISSIONER_SET.req: session-id=%d", mCommissionerSessionId);
    tlv = tlv->GetNext();

    tlv->SetType(Meshcop::kSteeringData);
    tlv->SetValue(aSteeringData.GetBloomFilter(), aSteeringData.GetLength());
    tlv = tlv->GetNext();

    message->SetPath(OT_URI_PATH_COMMISSIONER_SET);
    otbrLog(OTBR_LOG_INFO, "COMMISSIONER_SET.req: coap-uri: %s", "c/cs");
    message->SetPayload(buffer, Utils::LengthOf(buffer, tlv));
    otbrLog(OTBR_LOG_INFO, "COMMISSIONER_SET.req: sent");
    mCoapAgent->Send(*message, NULL, 0, HandleCommissionerSet, this);
    mCoapAgent->FreeMessage(message);
}

void Commissioner::HandleCommissionerSet(const Coap::Message &aMessage, void *aContext)
{
    uint16_t       length;
    int            tlvType;
    const Tlv *    tlv;
    const uint8_t *payload;
    Commissioner * commissioner = static_cast<Commissioner *>(aContext);

    otbrLog(OTBR_LOG_INFO, "COMMISSIONER_SET.rsp: start");
    payload = (aMessage.GetPayload(length));
    tlv     = reinterpret_cast<const Tlv *>(payload);

    while (Utils::LengthOf(payload, tlv) < length)
    {
        int8_t state = static_cast<int8_t>(tlv->GetValueUInt8());

        tlvType = tlv->GetType();
        switch (tlvType)
        {
        case Meshcop::kState:
            LogMeshcopState("COMM_SET.rsp", state);
            break;
        case Meshcop::kCommissionerSessionId:
            commissioner->mCommissionerSessionId = tlv->GetValueUInt16();
            otbrLog(OTBR_LOG_INFO, "COMMISSIONER_SET.rsp: session-id=%d", commissioner->mCommissionerSessionId);
            break;
        default:
            otbrLog(OTBR_LOG_INFO, "COMMISSIONER_SET.rsp: ignore-tlv=%d", tlvType);
            break;
        }
        tlv = tlv->GetNext();
    }
    otbrLog(OTBR_LOG_INFO, "COMMISSIONER_SET.rsp: complete");

    commissioner->CommissionerResponseNext();
}

void Commissioner::CommissionerResponseNext(void)
{
    if (mCommissionerState == CommissionerState::kStateConnected ||
        mCommissionerState == CommissionerState::kStateRejected)
    {
        if (mCommissionerState != CommissionerState::kStateInvalid && mPetitionRetryCount < kPetitionMaxRetry)
        {
            mPetitionRetryCount++;
            CommissionerPetition();
        }
        else
        {
            mCommissionerState  = CommissionerState::kStateInvalid;
            mPetitionRetryCount = 0;
        }
    }
}

void Commissioner::UpdateFdSet(fd_set & aReadFdSet,
                               fd_set & aWriteFdSet,
                               fd_set & aErrorFdSet,
                               int &    aMaxFd,
                               timeval &aTimeout)
{
    FD_SET(mSslClientFd.fd, &aReadFdSet);
    aMaxFd = Utils::Max(mSslClientFd.fd, aMaxFd);
    FD_SET(mJoinerSessionClientFd, &aReadFdSet);
    aMaxFd = Utils::Max(mJoinerSessionClientFd, aMaxFd);
    if (mJoinerSession)
    {
        mJoinerSession->UpdateFdSet(aReadFdSet, aWriteFdSet, aErrorFdSet, aMaxFd, aTimeout);
    }
}

void Commissioner::Process(const fd_set &aReadFdSet, const fd_set &aWriteFdSet, const fd_set &aErrorFdSet)
{
    uint8_t buffer[kSizeMaxPacket];
    timeval nowTime;

    if (mJoinerSession)
    {
        mJoinerSession->Process(aReadFdSet, aWriteFdSet, aErrorFdSet);
    }
    if (FD_ISSET(mSslClientFd.fd, &aReadFdSet))
    {
        int n = mbedtls_ssl_read(&mSsl, buffer, sizeof(buffer));

        if (n > 0)
        {
            mCoapAgent->Input(buffer, static_cast<uint16_t>(n), NULL, 0);
        }
    }

    if (FD_ISSET(mJoinerSessionClientFd, &aReadFdSet))
    {
        struct sockaddr_in from_addr;
        socklen_t          addrlen = sizeof(from_addr);

        ssize_t n =
            recvfrom(mJoinerSessionClientFd, buffer, sizeof(buffer), 0, (struct sockaddr *)(&from_addr), &addrlen);
        if (n > 0)
        {
            {
                char buf[kIPAddrNameBufSize];
                GetIPString((struct sockaddr *)(&from_addr), buf, sizeof(buf));
                otbrLog(OTBR_LOG_INFO, "relay from: %s\n", buf);
            }
            SendRelayTransmit(buffer, static_cast<uint16_t>(n));
        }
    }
    gettimeofday(&nowTime, NULL);
    if (mCommissionerState == CommissionerState::kStateAccepted && mKeepAliveRate > 0 &&
        nowTime.tv_sec - mLastKeepAliveTime.tv_sec > mKeepAliveRate)
    {
        SendCommissionerKeepAlive(static_cast<int8_t>(Meshcop::kStateAccepted));
    }
}

void Commissioner::Resign(void)
{
    if (mCommissionerState == CommissionerState::kStateAccepted)
    {
        SendCommissionerKeepAlive(static_cast<int8_t>(Meshcop::kStateRejected));
    }
}

void Commissioner::SendCommissionerKeepAlive(int8_t aState)
{
    uint8_t        buffer[kSizeMaxPacket];
    Tlv *          tlv = reinterpret_cast<Tlv *>(buffer);
    Coap::Message *message;

    mCoapToken++;
    message = mCoapAgent->NewMessage(Coap::kTypeConfirmable, Coap::kCodePost,
                                     reinterpret_cast<const uint8_t *>(&mCoapToken), sizeof(mCoapToken));

    tlv->SetType(Meshcop::kState);
    tlv->SetValue(aState);
    tlv = tlv->GetNext();

    tlv->SetType(Meshcop::kCommissionerSessionId);
    tlv->SetValue(mCommissionerSessionId);
    tlv = tlv->GetNext();

    message->SetPath(OT_URI_PATH_COMMISSIONER_KEEP_ALIVE);
    message->SetPayload(buffer, Utils::LengthOf(buffer, tlv));

    otbrLog(OTBR_LOG_INFO, "COMM_KA.req: send");
    gettimeofday(&mLastKeepAliveTime, NULL);
    mKeepAliveTxCount += 1;
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
    commissioner->mKeepAliveRxCount += 1;

    payload = (aMessage.GetPayload(length));
    tlv     = reinterpret_cast<const Tlv *>(payload);

    while (Utils::LengthOf(payload, tlv) < length)
    {
        int8_t state = static_cast<int8_t>(tlv->GetValueUInt8());

        tlvType = tlv->GetType();
        switch (tlvType)
        {
        case Meshcop::kState:
            LogMeshcopState("COMM_KA.rsp", state);
            switch (state)
            {
            case Meshcop::kStateAccepted:
                commissioner->mCommissionerState = CommissionerState::kStateAccepted;
                break;
            case Meshcop::kStateRejected:
                commissioner->mCommissionerState = CommissionerState::kStateRejected;
                break;
            default:
                commissioner->mCommissionerState = CommissionerState::kStateInvalid;
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

    commissioner->CommissionerResponseNext();
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
    Commissioner * commissioner = static_cast<Commissioner *>(aContext);
    const uint8_t *payload      = aMessage.GetPayload(length);

    for (const Tlv *requestTlv = reinterpret_cast<const Tlv *>(payload); Utils::LengthOf(payload, requestTlv) < length;
         requestTlv            = requestTlv->GetNext())
    {
        tlvType = requestTlv->GetType();
        switch (tlvType)
        {
        case Meshcop::kJoinerDtlsEncapsulation:
            ret = static_cast<int>(
                send(commissioner->mJoinerSessionClientFd, requestTlv->GetValue(), requestTlv->GetLength(), 0));
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

ssize_t Commissioner::SendRelayTransmit(uint8_t *aBuf, size_t aLength)
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

    assert(mJoinerSession != NULL);

    if (mJoinerSession->NeedAppendKek())
    {
        uint8_t kek[kKEKSize];

        mJoinerSession->GetKek(kek, sizeof(kek));
        mJoinerSession->MarkKekSent();
        otbrLog(OTBR_LOG_INFO, "relay: KEK state");
        responseTlv->SetType(Meshcop::kJoinerRouterKek);
        responseTlv->SetValue(kek, sizeof(kek));
        responseTlv = responseTlv->GetNext();
        mNumFinializeJoiners++;
    }

    {
        Coap::Message *message;
        uint16_t       token = ++mCoapToken;

        message = mCoapAgent->NewMessage(Coap::kTypeNonConfirmable, Coap::kCodePost,
                                         reinterpret_cast<const uint8_t *>(&token), sizeof(token));
        message->SetPath(OT_URI_PATH_RELAY_TX);
        message->SetPayload(payload, Utils::LengthOf(payload, responseTlv));
        otbrLog(OTBR_LOG_INFO, "RELAY_tx.req: send");
        mCoapAgent->Send(*message, NULL, 0, NULL, this);
        mCoapAgent->FreeMessage(message);
    }

    return static_cast<ssize_t>(aLength);
}

int Commissioner::GetNumFinalizedJoiners(void) const
{
    return mNumFinializeJoiners;
}

Commissioner::~Commissioner(void)
{
    Resign();
    if (mDtlsInitDone)
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
    }

    if (mJoinerSession)
    {
        delete mJoinerSession;
    }

    close(mJoinerSessionClientFd);
    Coap::Agent::Destroy(mCoapAgent);
}

} // namespace BorderRouter
} // namespace ot
