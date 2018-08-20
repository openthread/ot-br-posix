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
 *   The file is the header for the commissioner class
 */

#ifndef OTBR_COMMISSIONER_HPP_
#define OTBR_COMMISSIONER_HPP_

#include <arpa/inet.h>
#include <errno.h>
#include <sys/socket.h>

#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif
#include <mbedtls/certs.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/debug.h>
#include <mbedtls/ecjpake.h>
#include <mbedtls/entropy.h>
#include <mbedtls/error.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/ssl.h>
#include <mbedtls/timing.h>
#include <sys/time.h>

#include "commissioner_constants.hpp"
#include "joiner_session.hpp"
#include "agent/coap.hpp"
#include "utils/steeringdata.hpp"
#include "web/pskc-generator/pskc.hpp"

namespace ot {
namespace BorderRouter {

class Commissioner
{
public:
    /**
     * The constructor to initialize Commissioner
     *
     * @param[in]    aPskcBin           binary form of pskc
     * @param[in]    aPskdAscii         ascii form of pskd
     * @param[in]    aSteeringData      default steering data to filter joiner
     * @param[in]    aKeepAliveRate     send keep alive packet ever aKeepAliveRate seconds
     *
     */
    Commissioner(const uint8_t *     aPskcBin,
                 const char *        aPskdAscii,
                 const SteeringData &aSteeringData,
                 int                 aKeepAliveRate);

    /**
     * This method updates the fd_set and timeout for mainloop.
     * @p aTimeout should only be updated if session has pending process in less than its current value.
     *
     * @param[inout]    aReadFdSet      A reference to fd_set for polling read.
     * @param[inout]    aWriteFdSet     A reference to fd_set for polling write.
     * @param[inout]    aErrorFdSet     A reference to fd_set for polling error.
     * @param[inout]    aMaxFd          A reference to the current max fd in @p aReadFdSet and @p aWriteFdSet.
     * @param[inout]    aTimeout        A reference to the timeout.
     *
     */
    void UpdateFdSet(fd_set &aReadFdSet, fd_set &aWriteFdSet, fd_set &aErrorFdSet, int &aMaxFd, timeval &aTimeout);

    /**
     * This method performs the session processing.
     *
     * @param[in]   aReadFdSet          A reference to fd_set ready for reading.
     * @param[in]   aWriteFdSet         A reference to fd_set ready for writing.
     * @param[in]   aErrorFdSet         A reference to fd_set with error occurred.
     *
     */
    void Process(const fd_set &aReadFdSet, const fd_set &aWriteFdSet, const fd_set &aErrorFdSet);

    /**
     * This method returns whether the commissioner is valid
     *
     * @returns true when commissioner valid, false when uninitialized, rejected or unkown error
     *
     */
    bool Valid();

    /**
     * This method initialize the dtls session
     *
     * @param[in]   aAgentAddr      Address of border agent
     *
     * @returns 0 on success, MBEDTLS_ERR_XXX on failure
     *
     */
    int InitDtls(const sockaddr_in &aAgentAddr);

    /**
     * This method initialize the dtls session
     *
     * @returns 0 on success,
     *          MBEDTLS_ERR_SSL_WANT_READ or MBEDTLS_ERR_SSL_WANT_WRITE for pending, in which case
     *          you need to call this method again,
     *          or other SSL error code on failure
     */
    int TryDtlsHandshake();

    /**
     * This method sends commissioner petition coap request
     *
     */
    void CommissionerPetition();

    /**
     * This method sends commissioner set coap request
     *
     * @param[in]    aSteeringData      steering data to filter joiner, overrrides data from constructor
     *
     */
    void CommissionerSet(const SteeringData &aSteeringData);

    ~Commissioner();

private:
    enum CommissionState
    {
        kStateConnected,
        kStateAccepted,
        kStateRejected,
        kStateReady,
        kStateInvalid,
    } mCommissionState;

    Commissioner(const Commissioner &);
    Commissioner &operator=(const Commissioner &);

    int  DtlsHandShake(const sockaddr_in &aAgentAddr);
    void CommissionerKeepAlive();

    static ssize_t SendCoap(const uint8_t *aBuffer,
                            uint16_t       aLength,
                            const uint8_t *aIp6,
                            uint16_t       aPort,
                            void *         aContext);

    static void HandleCommissionerPetition(const Coap::Message &aMessage, void *aContext);
    static void HandleCommissionerSet(const Coap::Message &aMessage, void *aContext);
    static void HandleCommissionerKeepAlive(const Coap::Message &aMessage, void *aContext);
    void        CommissionerResponseNext();

    static void HandleRelayReceive(const Coap::Resource &aResource,
                                   const Coap::Message & aMessage,
                                   Coap::Message &       aResponse,
                                   const uint8_t *       aIp6,
                                   uint16_t              aPort,
                                   void *                aContext);
    int         SendRelayTransmit(uint8_t *aBuf, size_t aLength);

    mbedtls_net_context          mSslClientFd;
    mbedtls_ssl_context          mSsl;
    mbedtls_entropy_context      mEntropy;
    mbedtls_ctr_drbg_context     mDrbg;
    mbedtls_ssl_config           mSslConf;
    mbedtls_timing_delay_context mTimer;
    bool                         mDtlsInitDone;

    Coap::Agent *  mCoapAgent;
    int            mCoapToken;
    Coap::Resource mRelayReceiveHandler;

    uint8_t  mPskcBin[OT_PSKC_LENGTH];
    int      mPetitionRetryCount;
    uint16_t mCommissionerSessionId;

    JoinerSession mJoinerSession;
    int           mJoinerSessionClientFd;
    uint16_t      mJoinerUdpPort;
    uint8_t       mJoinerIid[8];
    uint16_t      mJoinerRouterLocator;
    SteeringData  mSteeringData;

    int     mKeepAliveRate;
    timeval mLastKeepAliveTime;
    int     mKeepAliveTxCount;
    int     mKeepAliveRxCount;

    static const uint16_t kPortJoinerSession;
    static const uint8_t  kSeed[];
    static const int      kCipherSuites[];
    static const char     kCommissionerId[];
    static const char     kCommPetURI[];
    static const char     kCommSetURI[];
    static const char     kCommKaURI[];
    static const char     kRelayRxURI[];
    static const char     kRelayTxURI[];
    static const int      kCoapResponseWaitSecond;
    static const int      kCoapResponseRetryTime;
};

} // namespace BorderRouter
} // namespace ot

#endif // OTBR_COMMISSIONER_HPP_
