#include "border_agent_session.hpp"
#include "stdlib.h"
#include "string.h"
#include "utils.hpp"
#include <vector>
#include "common/code_utils.hpp"
#include "common/logging.hpp"
#include "common/tlv.hpp"
#include "web/pskc-generator/pskc.hpp"

namespace ot {
namespace BorderRouter {

const uint8_t BorderAgentDtlsSession::kSeed[]           = "Commissioner";
const int     BorderAgentDtlsSession::kCipherSuites[]   = {MBEDTLS_TLS_ECJPAKE_WITH_AES_128_CCM_8, 0};
const char    BorderAgentDtlsSession::kCommissionerId[] = "OpenThread";

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
                                               const char *   aPassphrase)
{
    Psk::Pskc      pskc;
    const uint8_t *pskcBin = pskc.ComputePskc(aXpanidBin, aNetworkName, aPassphrase);
    memcpy(mPskcBin, pskcBin, sizeof(mPskcBin));
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
    (void)aTimeout;
    (void)aWriteFdSet;
    (void)aErrorFdSet;
}

void BorderAgentDtlsSession::Process(const fd_set &aReadFdSet, const fd_set &aWriteFdSet, const fd_set &aErrorFdSet)
{
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
            for (std::set<int>::iterator iter = mClientFds.begin(); iter != mClientFds.end(); iter++)
            {
                int clientFd = *iter;
                write(clientFd, mIOBuffer, n);
            }
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
    utils::get_ip_str(reinterpret_cast<const sockaddr *>(&aAgentAddr), addressAscii, sizeof(addressAscii));
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
    int          ret;
    int          coapToken = rand();
    Coap::Agent *coapAgent = Coap::Agent::Create(SendCoap, this);
    mCommissionState       = kStateConnected;
    SuccessOrExit(ret = CommissionerPetition(coapAgent, coapToken));
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

int BorderAgentDtlsSession::CommissionerPetition(Coap::Agent *aCoapAgent, int &aCoapToken)
{
    int     ret;
    int     retryCount = 0;
    uint8_t buffer[kSizeMaxPacket];
    Tlv *   tlv = reinterpret_cast<Tlv *>(buffer);

    Coap::Message *message;
    uint16_t       token = ++aCoapToken;

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
        message = aCoapAgent->NewMessage(Coap::kTypeConfirmable, Coap::kCodePost,
                                         reinterpret_cast<const uint8_t *>(&token), sizeof(token));
        tlv->SetType(Meshcop::kCommissionerId);
        tlv->SetValue(kCommissionerId, sizeof(kCommissionerId));
        tlv = tlv->GetNext();

        message->SetPath("c/cp");
        message->SetPayload(buffer, utils::LengthOf(buffer, tlv));
        otbrLog(OTBR_LOG_INFO, "COMM_PET.req: send");
        aCoapAgent->Send(*message, NULL, 0, HandleCommissionerPetition, this);
        aCoapAgent->FreeMessage(message);

        do
        {
            ret = mbedtls_ssl_read(&mSsl, buffer, sizeof(buffer));
            if (ret > 0)
            {
                aCoapAgent->Input(buffer, static_cast<uint16_t>(ret), NULL, 0);
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
            case Meshcop::kPetitionAccepted:
                otbrLog(OTBR_LOG_INFO, "COMM_PET.rsp: state=accepted");
                session->mCommissionState = kStateAccepted;
                break;
            case Meshcop::kPetitionRejected:
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
