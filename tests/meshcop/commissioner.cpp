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
#include "commissioner.hpp"

const uint8_t kSeed[] = "Commissioner";
const char    kCommissionerId[] = "OpenThread";
const int     kCipherSuites[] = {
    MBEDTLS_TLS_ECJPAKE_WITH_AES_128_CCM_8,
    0
};

const struct timeval kPollTimeout = {1, 0};

struct Context gContext;


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

    otbrLog(OTBR_LOG_INFO, "HandleJoinerFinalize, STATE = 1\n");
    
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

void HandleRelayReceive(const Coap::Resource &aResource,
                        const Coap::Message &aMessage,
                        Coap::Message &aResponse,
                        const uint8_t *aIp6,
                        uint16_t aPort,
                        void *aContext)
{
    int            ret = 0;
    int            tlvtype;
    uint16_t       length;
    Context       &context = *static_cast<Context *>(aContext);
    const uint8_t *payload = aMessage.GetPayload(length);

    for (const Tlv *requestTlv = reinterpret_cast<const Tlv *>(payload);
         LengthOf(payload, requestTlv) < length;
         requestTlv = requestTlv->GetNext())
    {

        tlvtype = requestTlv->GetType();
        switch (tlvtype)
        {
        case Meshcop::kJoinerDtlsEncapsulation:
            struct sockaddr_in addr;
            memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_port = htons(kPortJoinerSession);

            otbrLog(OTBR_LOG_INFO, "Encapsulation: %d bytes for port: %d",
                    requestTlv->GetLength(), kPortJoinerSession);
            {
                char buf[50];
                inet_ntop(AF_INET, &addr.sin_addr, buf, sizeof(buf));
                otbrLog(OTBR_LOG_INFO, "DEST: %s", buf);
            }
            ret = sendto(context.mSocket,
                         requestTlv->GetValue(),
                         requestTlv->GetLength(),
                         0,
                         reinterpret_cast<const struct sockaddr *>(&addr),
                         sizeof(addr));
            if (ret < 0)
            {
                otbrLog(OTBR_LOG_ERR, "relay receive, sendto() fails with %d", ret);
            }
            VerifyOrExit(ret != -1, ret = errno);
            break;

        case Meshcop::kJoinerUdpPort:
            context.mJoiner.mUdpPort = requestTlv->GetValueUInt16();
            otbrLog(OTBR_LOG_INFO, "JoinerPort: %d", context.mJoiner.mUdpPort);
            break;

        case Meshcop::kJoinerIid:
            memcpy(context.mJoiner.mIid,
                   requestTlv->GetValue(),
                   sizeof(context.mJoiner.mIid));
            break;

        case Meshcop::kJoinerRouterLocator:
            context.mJoiner.mRouterLocator = requestTlv->GetValueUInt16();
            otbrLog(OTBR_LOG_INFO, "Router locator: %d", context.mJoiner.mRouterLocator);
            break;

        default:
            otbrLog(OTBR_LOG_INFO, "skip tlv type: %d", tlvtype);
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

/* from the example:
 * http://beej.us/guide/bgnet/output/html/multipage/inet_ntopman.html
 */
static char *get_ip_str(const struct sockaddr *sa, char *s, size_t maxlen)
{
    switch (sa->sa_family)
    {
    case AF_INET:
        inet_ntop(AF_INET, &(((struct sockaddr_in *)sa)->sin_addr),
                  s, maxlen);
        break;

    case AF_INET6:
        inet_ntop(AF_INET6, &(((struct sockaddr_in6 *)sa)->sin6_addr),
                  s, maxlen);
        break;

    default:
        strncpy(s, "Unknown AF", maxlen);
        return NULL;
    }

    return s;
}

int SendRelayTransmit(Context &aContext)
{
    uint8_t            payload[kSizeMaxPacket];
    uint8_t            dtlsEncapsulation[kSizeMaxPacket];
    Tlv               *responseTlv = reinterpret_cast<Tlv *>(payload);
    struct sockaddr_in from_addr;
    socklen_t          addrlen;

    addrlen = sizeof(from_addr);


    ssize_t ret =
        recvfrom(aContext.mSocket, dtlsEncapsulation, sizeof(dtlsEncapsulation), 0, (struct sockaddr *)(&from_addr),
                 &addrlen);

    VerifyOrExit(ret > 0);
    {
        // Print this, because in some environments...
        // other things on the network use the same port
        // as open thread is using here :-(
        char buf[50];
        get_ip_str((struct sockaddr *)(&from_addr), buf, sizeof(buf));
        otbrLog(OTBR_LOG_INFO, "relay from: %s\n", buf);
    }


    responseTlv->SetType(Meshcop::kJoinerDtlsEncapsulation);
    responseTlv->SetValue(dtlsEncapsulation, static_cast<uint16_t>(ret));
    responseTlv = responseTlv->GetNext();

    responseTlv->SetType(Meshcop::kJoinerUdpPort);
    responseTlv->SetValue(aContext.mJoiner.mUdpPort);
    responseTlv = responseTlv->GetNext();

    responseTlv->SetType(Meshcop::kJoinerIid);
    responseTlv->SetValue(aContext.mJoiner.mIid, sizeof(aContext.mJoiner.mIid));
    responseTlv = responseTlv->GetNext();

    responseTlv->SetType(Meshcop::kJoinerRouterLocator);
    responseTlv->SetValue(aContext.mJoiner.mRouterLocator);
    responseTlv = responseTlv->GetNext();

    if (aContext.mState == kStateFinalized)
    {
        otbrLog(OTBR_LOG_INFO, "realy: KEK state");
        responseTlv->SetType(Meshcop::kJoinerRouterKek);
        responseTlv->SetValue(aContext.mKek, sizeof(aContext.mKek));
        responseTlv = responseTlv->GetNext();
    }

    {
        Coap::Message *message;
        uint16_t       token = ++aContext.mCoapToken;

        message = aContext.mCoap->NewMessage(Coap::kTypeNonConfirmable, Coap::kCodePost,
                                             reinterpret_cast<const uint8_t *>(&token), sizeof(token));
        message->SetPath("c/tx");
        message->SetPayload(payload, LengthOf(payload, responseTlv));
        otbrLog(OTBR_LOG_INFO, "RELAY_tx.req: send");
        aContext.mCoap->Send(*message, NULL, 0, NULL, &aContext);
        aContext.mCoap->FreeMessage(message);
    }

exit:
    return ret;
}

/** Sends coap messages via one of the dtls interfaces (agent, or joiner) */
static ssize_t SendCoap(const uint8_t *aBuffer, uint16_t aLength, const uint8_t *aIp6, uint16_t aPort,
                        void *aContext)
{
    ssize_t  ret = 0;
    Context &context = *static_cast<Context *>(aContext);

    if (aPort == 0)
    {
        otbrLog(OTBR_LOG_INFO, "SendCoap: ssl-write-lenth: %d", aLength);
        ret = mbedtls_ssl_write(context.mSsl, aBuffer, aLength);
    }
    else
    {
        otbrLog(OTBR_LOG_INFO, "SendCoap: session-write-len: %d", aLength);
        if (context.mSession)
        {
            ret = context.mSession->Write(aBuffer, aLength);
        }
        else
        {
            otbrLog(OTBR_LOG_INFO, "SendCoap: error NO SESSION");
            ret = -1;
        }
    }

    (void)aIp6;
    (void)aPort;

    return ret;
}

/** callback to print debug from mbed */
static void my_debug(void *ctx, int level,
                     const char *file, int line,
                     const char *str)
{
    ((void) level);
    (void)(ctx);
#if 0
    fprintf((FILE *) ctx, "%s:%04d: %s", file, line, str);
    fflush((FILE *) ctx);
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
static int export_keys(void *aContext, const unsigned char *aMasterSecret, const unsigned char *aKeyBlock,
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

/** handle c/cp response */
static void HandleCommissionerPetition(const Coap::Message &aMessage, void *aContext)
{
    uint16_t       length;
    int            tlv_type;
    const Tlv     *tlv;
    const uint8_t *payload;
    Context       &context = *static_cast<Context *>(aContext);

    otbrLog(OTBR_LOG_INFO, "COMM_PET.rsp: start");
    payload = aMessage.GetPayload(length);
    tlv = reinterpret_cast<const Tlv *>(payload);

    while (LengthOf(payload, tlv) < length)
    {
        tlv_type = tlv->GetType();
        switch (tlv_type)
        {
        case Meshcop::kState:
            if (tlv->GetValueUInt8())
            {
                otbrLog(OTBR_LOG_INFO, "COMM_PET.rsp: state=accepted");
                context.mState = kStateAccepted;
            }
            else
            {
                otbrLog(OTBR_LOG_INFO, "COMM_PET.rsp: rejected");
            }
            break;

        case Meshcop::kCommissionerSessionId:
            context.mCommissionerSessionId = tlv->GetValueUInt16();
            otbrLog(OTBR_LOG_INFO, "COMM_PET.rsp: session-id=%d", context.mCommissionerSessionId);
            break;

        default:
            otbrLog(OTBR_LOG_INFO, "COMM_PET.rsp: ignore-tilv: %d", tlv_type);
            break;
        }
        tlv = tlv->GetNext();
    }
    otbrLog(OTBR_LOG_INFO, "COMM_PET.rsp: complete");
}

/** Send the c/cp request to the border router agent */
static int CommissionerPetition(Context &aContext)
{
    int     ret;
    uint8_t buffer[kSizeMaxPacket];
    Tlv    *tlv = reinterpret_cast<Tlv *>(buffer);

    Coap::Message *message;
    uint16_t       token = ++aContext.mCoapToken;

    otbrLog(OTBR_LOG_INFO, "COMM_PET.req: start");
    token = htons(token);
    message = aContext.mCoap->NewMessage(Coap::kTypeConfirmable, Coap::kCodePost,
                                         reinterpret_cast<const uint8_t *>(&token), sizeof(token));
    tlv->SetType(Meshcop::kCommissionerId);
    tlv->SetValue(kCommissionerId, sizeof(kCommissionerId) - 1);
    tlv = tlv->GetNext();

    message->SetPath("c/cp");
    message->SetPayload(buffer, LengthOf(buffer, tlv));
    otbrLog(OTBR_LOG_INFO, "COMM_PET.req: send");
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


    otbrLog(OTBR_LOG_INFO, "COMM_PET.req: complete");
    return ret;
}

/** This handles the commissioner data set response, ie: response to the steering data etc */
void HandleCommissionerSetResponse(const Coap::Message &aMessage, void *aContext)
{
    uint16_t       length;
    int            tlv_type;
    const Tlv     *tlv;
    const uint8_t *payload;
    Context       &context = *static_cast<Context *>(aContext);

    otbrLog(OTBR_LOG_INFO, "COMMISSIONER_SET.rsp: start");
    payload = (aMessage.GetPayload(length));
    tlv = reinterpret_cast<const Tlv *>(payload);

    while (LengthOf(payload, tlv) < length)
    {
        tlv_type = tlv->GetType();
        switch (tlv_type)
        {
        case Meshcop::kState:
            if (tlv->GetValueUInt8())
            {
                context.mState = kStateReady;
                otbrLog(OTBR_LOG_INFO, "COMMISSIONER_SET.rsp: state=ready");
            }
            else
            {
                otbrLog(OTBR_LOG_INFO, "COMMISSIONER_SET.rsp: state=NOT-ready");
            }
            break;

        case Meshcop::kCommissionerSessionId:
            context.mCommissionerSessionId = tlv->GetValueUInt16();
            otbrLog(OTBR_LOG_INFO, "COMMISSIONER_SET.rsp: session-id=%d", context.mCommissionerSessionId);
            break;

        default:
            otbrLog(OTBR_LOG_INFO, "COMMISSIONER_SET.rsp: ignore-tlv=%d", tlv_type);
            break;
        }
        tlv = tlv->GetNext();
    }
    otbrLog(OTBR_LOG_INFO, "COMMISSIONER_SET.rsp: complete");
}

/** Send the commissioner data (ie: Steering data, etc) to the leader/joining router */
static int CommissionerSet(Context &aContext)
{
    bool     ok;
    int      ret = 0;
    uint16_t token = ++aContext.mCoapToken;
    uint8_t  buffer[kSizeMaxPacket];
    Tlv     *tlv = reinterpret_cast<Tlv *>(buffer);

    Coap::Message *message;

    otbrLog(OTBR_LOG_INFO, "COMMISSIONER_SET.req: start");
    token = htons(token);
    message = aContext.mCoap->NewMessage(Coap::kTypeConfirmable,
                                         Coap::kCodePost,
                                         reinterpret_cast<const uint8_t *>(&token),
                                         sizeof(token));

    tlv->SetType(Meshcop::kCommissionerSessionId);
    tlv->SetValue(aContext.mCommissionerSessionId);
    otbrLog(OTBR_LOG_INFO, "COMMISSIONER_SET.req: session-id=%d", aContext.mCommissionerSessionId);
    tlv = tlv->GetNext();


    ok = compute_steering();
    /* Note: Steering computation will have logged the steering data */
    if (!ok)
    {
        fail("Cannot compute steering data\n");
    }

    tlv->SetType(Meshcop::kSteeringData);
    tlv->SetValue(aContext.mJoiner.mSteeringData.GetDataPointer(),
                  aContext.mJoiner.mSteeringData.GetLength());
    tlv = tlv->GetNext();

    message->SetPath("c/cs");
    otbrLog(OTBR_LOG_INFO, "COMMISSIONER_SET.req: coap-uri: %s", "c/cs");
    message->SetPayload(buffer, LengthOf(buffer, tlv));
    otbrLog(OTBR_LOG_INFO, "COMMISSIONER_SET.req: sent");
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

/** Handle a COMM_KA response */
static void HandleCommissionerKeepAlive(const Coap::Message &aMessage, void *aContext)
{
    uint16_t       length;
    int            tlv_type;
    const Tlv     *tlv;
    const uint8_t *payload;
    Context       &context = *static_cast<Context *>(aContext);

    otbrLog(OTBR_LOG_INFO, "COMM_KA.rsp: start");

    /* record stats */
    gettimeofday(&(context.mCOMM_KA.mLastRxTv), NULL);
    context.mCOMM_KA.mRxCnt += 1;

    payload = (aMessage.GetPayload(length));
    tlv = reinterpret_cast<const Tlv *>(payload);


    while (LengthOf(payload, tlv) < length)
    {
        tlv_type = tlv->GetType();
        switch (tlv_type)
        {
        case Meshcop::kState:
            if (tlv->GetValueUInt8())
            {
                context.mState = kStateReady;
                otbrLog(OTBR_LOG_INFO, "COMM_KA.rsp: state=ready");
            }
            else
            {
                otbrLog(OTBR_LOG_INFO, "COMM_KA.rsp: state=reject");
                context.mState = kStateError;
            }
            break;

        default:
            otbrLog(OTBR_LOG_INFO, "COMM_KA.rsp: ignore-tlv=%d", tlv_type);
            break;
        }
        tlv = tlv->GetNext();
    }
    otbrLog(OTBR_LOG_INFO, "COMM_KA.rsp: complete");
}

/** Send a COMM_KA to keep the session alive */
static int CommissionerKeepAlive(Context &aContext)
{
    int     ret = 0;
    uint8_t buffer[kSizeMaxPacket];
    Tlv    *tlv = reinterpret_cast<Tlv *>(buffer);

    Coap::Message *message;

    aContext.mCoapToken++;
    message = aContext.mCoap->NewMessage(Coap::kTypeConfirmable, Coap::kCodePost,
                                         reinterpret_cast<const uint8_t *>(&aContext.mCoapToken),
                                         sizeof(aContext.mCoapToken));

    tlv->SetType(Meshcop::kState);
    tlv->SetValue(static_cast<uint8_t>(1));
    tlv = tlv->GetNext();

    tlv->SetType(Meshcop::kCommissionerSessionId);
    tlv->SetValue(aContext.mCommissionerSessionId);
    tlv = tlv->GetNext();

    message->SetPath("c/ca");
    message->SetPayload(buffer, LengthOf(buffer, tlv));

    otbrLog(OTBR_LOG_INFO, "COMM_KA.req: send");
    gettimeofday(&(aContext.mCOMM_KA.mLastTxTv), NULL);
    aContext.mCOMM_KA.mTxCnt += 1;
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

/** This processes IO traffic during the commissioner session */
static int CommissionerSessionProcess(Context &context)
{
    uint8_t buf[kSizeMaxPacket];
    int     ret;

    ret = mbedtls_ssl_read(context.mSsl, buf, sizeof(buf));
    if (ret > 0)
    {
        context.mCoap->Input(buf, static_cast<uint16_t>(ret), NULL, 0);
    }
    else
    {
        otbrLog(OTBR_LOG_INFO, "CommissionerSessionProcess() ssl error -0x%04x", -ret);
    }

    return ret;
}

/** send data into the coap session */
static void FeedCoaps(const uint8_t *aBuffer, uint16_t aLength, void *aContext)
{
    Context &context = *static_cast<Context *>(aContext);

    context.mCoap->Input(aBuffer, aLength, NULL, 1);
}

/** DTLS session has changed */
static void HandleSessionChange(Dtls::Session &aSession, Dtls::Session::State aState, void *aContext)
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

/** Determine if it is time to send a COMM_KA or not */
static void CommissionerKeepAliveCheck(Context &aContext)
{
    struct timeval tv_now;
    int            nsecs;


    if (aContext.mCOMM_KA.mDisabled)
    {
        return;
    }

    /* check time */
    gettimeofday(&tv_now, NULL);


    nsecs = (int)(tv_now.tv_sec - aContext.mCOMM_KA.mLastTxTv.tv_sec);
    if (nsecs < aContext.mCOMM_KA.mTxRate)
    {
        /* not time yet */
        return;
    }

    /* send the COMM_KA */
    CommissionerKeepAlive(aContext);
}

/** Do we give up, did we never get a commissioned device? */
static bool IsEnvelopeTimeout(Context &aContext)
{
    struct timeval tv;
    int            nsecs;

    gettimeofday(&tv, NULL);

    nsecs = (int)(tv.tv_sec - aContext.mEnvelopeStartTv.tv_sec);

    if (nsecs > aContext.mEnvelopeTimeout)
    {
        otbrLog(OTBR_LOG_INFO, "ERROR: Envelope Timeout");
        return true;
    }
    else
    {
        return false;
    }
}


/** Once connected, we await here till a joiner appears... and handle requests */
int CommissionerServe(Context &aContext)
{
    fd_set readFdSet;
    fd_set writeFdSet;
    fd_set errorFdSet;
    int    ret = 0;

    otbrLog(OTBR_LOG_INFO, "CommissionerServe: start");
    aContext.mSocket = socket(AF_INET, SOCK_DGRAM, 0);
    VerifyOrExit(aContext.mSocket != -1, ret = errno);
    aContext.mDtlsServer = Dtls::Server::Create(kPortJoinerSession, HandleSessionChange, &aContext);

    otbrLog(OTBR_LOG_INFO, "commissioner-serve: device-pskd=%s", aContext.mJoiner.mPSKd_ascii);
    aContext.mDtlsServer->SetPSK((const uint8_t *)aContext.mJoiner.mPSKd_ascii, strlen(aContext.mJoiner.mPSKd_ascii));
    aContext.mDtlsServer->Start();

    if (aContext.mCOMM_KA.mDisabled)
    {
        /* log this once for 'record keeping' */
        /* and not in the polling loop */
        otbrLog(OTBR_LOG_INFO, "COMM_KA: disabled");
    }

    while (aContext.mState != kStateDone && aContext.mState != kStateError)
    {
        struct timeval timeout = kPollTimeout;
        int            maxFd = -1;


        otbrLog(OTBR_LOG_DEBUG, "CommissionerServe: Tick..");


        /* determine if it is time to send a COMM_KA */
        if (!aContext.mCOMM_KA.mDisabled)
        {
            CommissionerKeepAliveCheck(aContext);
        }

        if (IsEnvelopeTimeout(aContext))
        {
            break;
        }

        /* Monitor sockets */
        FD_ZERO(&readFdSet);
        FD_ZERO(&writeFdSet);
        FD_ZERO(&errorFdSet);
        FD_SET(aContext.mNet->fd, &readFdSet);
        if (maxFd < aContext.mNet->fd)
        {
            maxFd = aContext.mNet->fd;
        }
        FD_SET(aContext.mSocket, &readFdSet);
        if (maxFd < aContext.mSocket)
        {
            maxFd = aContext.mSocket;
        }
        aContext.mDtlsServer->UpdateFdSet(readFdSet, writeFdSet, errorFdSet, maxFd, timeout);
        ret = select(maxFd + 1, &readFdSet, &writeFdSet, &errorFdSet, &timeout);
        if ((ret < 0) && (errno != EINTR))
        {
            otbrLog(OTBR_LOG_ERR, "commissioner serve, select errno=%d", errno);
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

    /* the session with the joiner might not exist.
     * Example: If we never found the joiner..
     */
    if (aContext.mSession)
    {
        aContext.mSession->Close();
    }
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
    otbrLog(OTBR_LOG_INFO, "CommissionerServe: result=%d", ret);

    return ret;
}

/** this runs a commissioning session end to end */
static int commissioning_session(Context &context)
{
    int                          ret;
    bool                         ok;
    mbedtls_net_context          client_fd;
    mbedtls_entropy_context      entropy;
    mbedtls_ctr_drbg_context     ctr_drbg;
    mbedtls_ssl_context          ssl;
    mbedtls_ssl_config           conf;
    mbedtls_timing_delay_context timer;

    Coap::Resource relayReceiveHandler(OT_URI_PATH_RELAY_RX, HandleRelayReceive, &context);
    Coap::Resource joinerFinalizeHandler(OT_URI_PATH_JOINER_FINALIZE, HandleJoinerFinalize, &context);

    otbrLog(OTBR_LOG_INFO, "agent-address: %s", context.mAgent.mAddress_ascii);
    if (context.mAgent.mAddress_ascii[0] == 0)
    {
        fail("Missing AGENT ip address\n");
    }

    otbrLog(OTBR_LOG_INFO, "agent-port: %s", context.mAgent.mPort_ascii);
    if (context.mAgent.mPort_ascii[0] == 0)
    {
        fail("Missing AGENT ip port\n");
    }

    if( aContext.mJoiner.mPSKd_ascii[0] == 0 )
    {
	fail("Missing PSKd (joiner passphrase/password)\n");
    }
    
    context.mSsl = &ssl;
    context.mNet = &client_fd;
    {
        int n;
        n = otbrLogGetLevel();
        mbedtls_debug_set_threshold(n);
    }

    mbedtls_net_init(&client_fd);
    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&conf);
    mbedtls_ctr_drbg_init(&ctr_drbg);

    mbedtls_entropy_init(&entropy);
    if ((ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                                     kSeed,
                                     sizeof(kSeed))) != 0)
    {
        fail("mbed drgb seed fails?\n");
    }

    otbrLog(OTBR_LOG_INFO, "connecting...");
    if ((ret = mbedtls_net_connect(&client_fd,
                                   context.mAgent.mAddress_ascii,
                                   context.mAgent.mPort_ascii, MBEDTLS_NET_PROTO_UDP)) != 0)
    {
        fail("CONNECT: %s:%s failed\n",
             context.mAgent.mAddress_ascii,
             context.mAgent.mPort_ascii);
        goto exit;
    }

    if ((ret = mbedtls_ssl_config_defaults(&conf,
                                           MBEDTLS_SSL_IS_CLIENT,
                                           MBEDTLS_SSL_TRANSPORT_DATAGRAM,
                                           MBEDTLS_SSL_PRESET_DEFAULT)) != 0)
    {
        fail("Cannot configure mbed defaults\n");
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

    otbrLog(OTBR_LOG_INFO, "connecting: ssl-setup");
    if ((ret = mbedtls_ssl_setup(&ssl, &conf)) != 0)
    {
        fail("Cannot setup ssl\n");
        goto exit;
    }

    mbedtls_ssl_set_bio(&ssl, &client_fd,
                        mbedtls_net_send,
                        mbedtls_net_recv,
                        mbedtls_net_recv_timeout);

    mbedtls_ssl_set_timer_cb(&ssl,
                             &timer,
                             mbedtls_timing_set_delay,
                             mbedtls_timing_get_delay);


    ok = compute_pskc();
    /* note: compute_pskc will have logged details */
    if (!ok)
    {
        fail("Cannot compute PSKc (commissioning shared key)\n");
    }

    mbedtls_ssl_set_hs_ecjpake_password(&ssl,
                                        context.mAgent.mPSKc.bin,
                                        OT_PSKC_LENGTH);

    otbrLog(OTBR_LOG_INFO, "connect: perform handshake");
    do
    {
        ret = mbedtls_ssl_handshake(&ssl);
    }
    while (ret == MBEDTLS_ERR_SSL_WANT_READ ||
           ret == MBEDTLS_ERR_SSL_WANT_WRITE);

    if (ret != 0)
    {
        otbrLog(OTBR_LOG_ERR, "Handshake fails");
        goto exit;
    }

    otbrLog(OTBR_LOG_INFO, "connect: CONNECTED!");
    context.mState = kStateConnected;
    context.mCoap = Coap::Agent::Create(SendCoap, &context);

    SuccessOrExit(ret = context.mCoap->AddResource(relayReceiveHandler));
    SuccessOrExit(ret = context.mCoap->AddResource(joinerFinalizeHandler));

    /** Send the petitioning request */
    SuccessOrExit(ret = CommissionerPetition(context));

    /** Tell network who we want to commission */
    SuccessOrExit(ret = CommissionerSet(context));

    /* and wait for that joiner to appear */
    SuccessOrExit(ret = CommissionerServe(context));

    /* YEA! Success!!!! */
    otbrLog(OTBR_LOG_INFO, "Closing SSL connection");
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
        otbrLog(OTBR_LOG_ERR, "mbed error: %d - %s", ret, error_buf);
    }
#endif

    mbedtls_net_free(&client_fd);

    mbedtls_ssl_free(&ssl);
    mbedtls_ssl_config_free(&conf);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);

    /* Shell can not handle large exit numbers -> 1 for errors */
    if (ret == 0)
    {
        return EXIT_SUCCESS;
    }
    else
    {
        return EXIT_FAILURE;
    }

    return ret;
}


static void set_context_defaults(void)
{
    gContext.mJoiner.mSteeringData.SetLength(kSteeringDefaultLength);
    gContext.mJoiner.mSteeringData.Clear();

    /* 5minutes is a resonable period of time */
    gettimeofday(&(gContext.mEnvelopeStartTv), NULL);
    gContext.mEnvelopeTimeout = 5 * 60;

    /* Set the COMM_KA transmit rate to every 15 seconds */
    gContext.mCOMM_KA.mTxRate = 15;
}

int main(int argc, char *argv[])
{
    int ret = 0;

    /* Set the initial log */
    otbrLogInit(SYSLOG_IDENT, OTBR_LOG_ERR);

    /* set defaults that may be overridden by the cmdline params */
    set_context_defaults();

    /* parse command line params */
    commissioner_argcargv(argc, argv);

    if (!gContext.commission_device)
    {
        fprintf(stderr, "Nothing todo? Try --help\n");
        exit(EXIT_FAILURE);
    }


    /* be a commissioner */
    ret = commissioning_session(gContext);
    otbrLog(OTBR_LOG_INFO, "exit: %d\n", ret);
    return ret;
}
