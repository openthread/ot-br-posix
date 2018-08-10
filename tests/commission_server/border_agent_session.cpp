#include "addr_utils.hpp"
#include "border_agent_session.hpp"
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

const uint16_t BorderAgentDtlsSession::kPortJoinerSession = 49192;
const uint8_t  BorderAgentDtlsSession::kSeed[]            = "Commissioner";
const int      BorderAgentDtlsSession::kCipherSuites[]    = {MBEDTLS_TLS_ECJPAKE_WITH_AES_128_CCM_8, 0};
const char     BorderAgentDtlsSession::kCommissionerId[]  = "OpenThread";

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

BorderAgentDtlsSession::BorderAgentDtlsSession(const uint8_t *aXpanidBin,
                                               const char *   aNetworkName,
                                               const char *   aPassphrase,
                                               const char *   aPskdAscii)
    : mRelayReceiveHandler(OT_URI_PATH_RELAY_RX, BorderAgentDtlsSession::HandleRelayReceive, this)
    , mJoinerSession(kPortJoinerSession, aPskdAscii)
{
    Psk::Pskc      pskc;
    const uint8_t *pskcBin = pskc.ComputePskc(aXpanidBin, aNetworkName, aPassphrase);
    memcpy(mPskcBin, pskcBin, sizeof(mPskcBin));
    mJoinerSessionClientFd = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in addr;
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port        = htons(kPortJoinerSession);
    connect(mJoinerSessionClientFd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr));
}

int BorderAgentDtlsSession::Connect(const sockaddr_in &aAgentAddr)
{
    int ret;
    SuccessOrExit(ret = DtlsHandShake(aAgentAddr));
    SuccessOrExit(ret = BecomeCommissioner());
exit:
    return ret;
}

void BorderAgentDtlsSession::UpdateFdSet(fd_set & aReadFdSet,
                                         fd_set & aWriteFdSet,
                                         fd_set & aErrorFdSet,
                                         int &    aMaxFd,
                                         timeval &aTimeout)
{
    FD_SET(mListenFd, &aReadFdSet);
    aMaxFd = utils::max(mListenFd, aMaxFd);
    for (std::set<int>::iterator iter = mClientFds.begin(); iter != mClientFds.end(); iter++)
    {
        FD_SET(*iter, &aReadFdSet);
        aMaxFd = utils::max(*iter, aMaxFd);
    }
    FD_SET(mSslClientFd.fd, &aReadFdSet);
    aMaxFd = utils::max(mSslClientFd.fd, aMaxFd);
    FD_SET(mJoinerSessionClientFd, &aReadFdSet);
    aMaxFd = utils::max(mJoinerSessionClientFd, aMaxFd);
    mJoinerSession.UpdateFdSet(aReadFdSet, aWriteFdSet, aErrorFdSet, aMaxFd, aTimeout);
    (void)aTimeout;
    (void)aWriteFdSet;
    (void)aErrorFdSet;
}

void BorderAgentDtlsSession::Process(const fd_set &aReadFdSet, const fd_set &aWriteFdSet, const fd_set &aErrorFdSet)
{
    mJoinerSession.Process(aReadFdSet, aWriteFdSet, aErrorFdSet);
    std::vector<int> closedClientFds;
    if (FD_ISSET(mListenFd, &aReadFdSet))
    {
        sockaddr  clientAddr;
        socklen_t clientAddrlen;
        int       newClientFd = accept(mListenFd, reinterpret_cast<struct sockaddr *>(&clientAddr), &clientAddrlen);
        if (newClientFd >= 0)
        {
            mClientFds.insert(newClientFd);
        }
        else
        {
            perror("accept");
        }
    }

    for (std::set<int>::iterator iter = mClientFds.begin(); iter != mClientFds.end(); iter++)
    {
        int clientFd = *iter;
        if (FD_ISSET(clientFd, &aReadFdSet))
        {
            ssize_t n = read(clientFd, mIOBuffer, sizeof(mIOBuffer));
            if (n > 0)
            {
                mbedtls_ssl_write(&mSsl, mIOBuffer, n);
            }
            else if (n == 0)
            {
                closedClientFds.push_back(clientFd);
            }
            else
            {
                otbrLog(OTBR_LOG_ERR, "read from client error", strerror(errno));
            }
        }
    }
    for (size_t i = 0; i < closedClientFds.size(); i++)
    {
        mClientFds.erase(closedClientFds[i]);
        close(closedClientFds[i]);
    }

    if (FD_ISSET(mSslClientFd.fd, &aReadFdSet))
    {
        int n = mbedtls_ssl_read(&mSsl, mIOBuffer, sizeof(mIOBuffer));
        if (n > 0)
        {
            printf("Get dtls input\n");
            mCoapAgent->Input(mIOBuffer, n, NULL, 0);
            for (std::set<int>::iterator iter = mClientFds.begin(); iter != mClientFds.end(); iter++)
            {
                int clientFd = *iter;
                write(clientFd, mIOBuffer, n);
            }
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
    (void)aWriteFdSet;
    (void)aErrorFdSet;
}

int BorderAgentDtlsSession::DtlsHandShake(const sockaddr_in &aAgentAddr)
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

int BorderAgentDtlsSession::BecomeCommissioner()
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

ssize_t BorderAgentDtlsSession::SendCoap(const uint8_t *aBuffer,
                                         uint16_t       aLength,
                                         const uint8_t *aIp6,
                                         uint16_t       aPort,
                                         void *         aContext)
{
    (void)aIp6;
    (void)aPort;
    BorderAgentDtlsSession *session = reinterpret_cast<BorderAgentDtlsSession *>(aContext);
    return mbedtls_ssl_write(&session->mSsl, aBuffer, aLength);
}

int BorderAgentDtlsSession::CommissionerPetition()
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

void BorderAgentDtlsSession::HandleRelayReceive(const Coap::Resource &aResource,
                                                const Coap::Message & aMessage,
                                                Coap::Message &       aResponse,
                                                const uint8_t *       aIp6,
                                                uint16_t              aPort,
                                                void *                aContext)
{
    int                     ret = 0;
    int                     tlvType;
    uint16_t                length;
    BorderAgentDtlsSession *session = reinterpret_cast<BorderAgentDtlsSession *>(aContext);
    const uint8_t *         payload = aMessage.GetPayload(length);

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
            ret = send(session->mJoinerSessionClientFd, requestTlv->GetValue(), requestTlv->GetLength(), 0);
            if (ret < 0)
            {
                otbrLog(OTBR_LOG_ERR, "relay receive, sendto() fails with %d", errno);
            }
            VerifyOrExit(ret != -1, ret = errno);
            break;

        case Meshcop::kJoinerUdpPort:
            session->mJoinerUdpPort = requestTlv->GetValueUInt16();
            otbrLog(OTBR_LOG_INFO, "JoinerPort: %d", session->mJoinerUdpPort);
            break;

        case Meshcop::kJoinerIid:
            memcpy(session->mJoinerIid, requestTlv->GetValue(), sizeof(session->mJoinerIid));
            break;

        case Meshcop::kJoinerRouterLocator:
            session->mJoinerRouterLocator = requestTlv->GetValueUInt16();
            otbrLog(OTBR_LOG_INFO, "Router locator: %d", session->mJoinerRouterLocator);
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

int BorderAgentDtlsSession::SendRelayTransmit(uint8_t *aBuf, size_t aLength)
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
void BorderAgentDtlsSession::HandleCommissionerPetition(const Coap::Message &aMessage, void *aContext)
{
    uint16_t                length;
    int                     tlvType;
    const Tlv *             tlv;
    const uint8_t *         payload;
    BorderAgentDtlsSession *session = reinterpret_cast<BorderAgentDtlsSession *>(aContext);

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
            case kStateAccepted:
                otbrLog(OTBR_LOG_INFO, "COMM_PET.rsp: state=accepted");
                session->mCommissionState = kStateAccepted;
                break;
            case kStateRejected:
                otbrLog(OTBR_LOG_INFO, "COMM_PET.rsp: state=accepted");
                session->mCommissionState = kStateRejected;
                break;
            default:
                otbrLog(OTBR_LOG_INFO, "COMM_PET.rsp: state=%d", state);
                session->mCommissionState = kStateInvalid;
                break;
            }
            break;
        case Meshcop::kCommissionerSessionId:
            session->mCommissionerSessionId = tlv->GetValueUInt16();
            printf("COMM_PET.rsp: session-id=%d\n", session->mCommissionerSessionId);
            otbrLog(OTBR_LOG_INFO, "COMM_PET.rsp: session-id=%d", session->mCommissionerSessionId);
            break;

        default:
            otbrLog(OTBR_LOG_INFO, "COMM_PET.rsp: ignore-tlv: %d", tlvType);
            break;
        }
        tlv = tlv->GetNext();
    }

    otbrLog(OTBR_LOG_INFO, "COMM_PET.rsp: complete");
}

int BorderAgentDtlsSession::CommissionerSet()
{
    int          ret   = 0;
    uint16_t     token = ++mCoapToken;
    uint8_t      buffer[kSizeMaxPacket];
    SteeringData steeringData;
    Tlv *        tlv = reinterpret_cast<Tlv *>(buffer);

    Coap::Message *message;

    otbrLog(OTBR_LOG_INFO, "COMMISSIONER_SET.req: start");
    token   = htons(token);
    message = mCoapAgent->NewMessage(Coap::kTypeConfirmable, Coap::kCodePost, reinterpret_cast<const uint8_t *>(&token),
                                     sizeof(token));

    tlv->SetType(Meshcop::kCommissionerSessionId);
    tlv->SetValue(mCommissionerSessionId);
    otbrLog(OTBR_LOG_INFO, "COMMISSIONER_SET.req: session-id=%d", mCommissionerSessionId);
    printf("COMMISSIONER_SET.req: session-id=%d\n", mCommissionerSessionId);
    tlv = tlv->GetNext();

    steeringData.Init();
    steeringData.Set();

    tlv->SetType(Meshcop::kSteeringData);
    tlv->SetValue(steeringData.GetDataPointer(), steeringData.GetLength());
    tlv = tlv->GetNext();

    message->SetPath("c/cs");
    otbrLog(OTBR_LOG_INFO, "COMMISSIONER_SET.req: coap-uri: %s", "c/cs");
    message->SetPayload(buffer, utils::LengthOf(buffer, tlv));
    otbrLog(OTBR_LOG_INFO, "COMMISSIONER_SET.req: sent");
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

    return ret;
}

void BorderAgentDtlsSession::HandleCommissionerSet(const Coap::Message &aMessage, void *aContext)
{
    uint16_t                length;
    int                     tlvType;
    const Tlv *             tlv;
    const uint8_t *         payload;
    BorderAgentDtlsSession *session = reinterpret_cast<BorderAgentDtlsSession *>(aContext);

    otbrLog(OTBR_LOG_INFO, "COMMISSIONER_SET.rsp: start");
    payload = (aMessage.GetPayload(length));
    tlv     = reinterpret_cast<const Tlv *>(payload);

    while (utils::LengthOf(payload, tlv) < length)
    {
        tlvType = tlv->GetType();
        switch (tlvType)
        {
        case Meshcop::kState:
            if (tlv->GetValueUInt8())
            {
                session->mCommissionState = kStateReady;
                otbrLog(OTBR_LOG_INFO, "COMMISSIONER_SET.rsp: state=ready");
            }
            else
            {
                otbrLog(OTBR_LOG_INFO, "COMMISSIONER_SET.rsp: state=NOT-ready");
            }
            break;

        case Meshcop::kCommissionerSessionId:
            session->mCommissionerSessionId = tlv->GetValueUInt16();
            printf("COMMISSIONER_SET.rsp: session-id=%d\n", session->mCommissionerSessionId);
            otbrLog(OTBR_LOG_INFO, "COMMISSIONER_SET.rsp: session-id=%d", session->mCommissionerSessionId);
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

int BorderAgentDtlsSession::SetupProxyServer()
{
    int         ret;
    int         optval = 1;
    sockaddr_in addr;
    mListenFd            = -1;
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(FORWARD_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    mListenFd            = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(mListenFd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    SuccessOrExit(ret = bind(mListenFd, (struct sockaddr *)&addr, sizeof(addr)));
    SuccessOrExit(ret = listen(mListenFd, 10)); // backlog = 10
exit:
    return ret;
}

void BorderAgentDtlsSession::ShutDownPorxyServer()
{
    close(mListenFd);
    close(mJoinerSessionClientFd);
    Coap::Agent::Destroy(mCoapAgent);
}

void BorderAgentDtlsSession::BorderAgentDtlsSession::Disconnect()
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

} // namespace BorderRouter
} // namespace ot
