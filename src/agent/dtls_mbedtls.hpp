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
 *   This file includes definition for mbedTLS-based DTLS implementation.
 */

#ifndef DTLS_MBEDTLS_HPP_
#define DTLS_MBEDTLS_HPP_

#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <vector>

#include <boost/shared_ptr.hpp>

extern "C" {

#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif

#if defined(MBEDTLS_PLATFORM_C)
#include <mbedtls/platform.h>
#else
#include <stdio.h>
#define mbedtls_printf     printf
#define mbedtls_fprintf    fprintf
#define mbedtls_time_t     time_t
#endif

#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/ecjpake.h>
#include <mbedtls/ssl.h>
#include <mbedtls/ssl_cookie.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/error.h>
#include <mbedtls/debug.h>
#include <mbedtls/timing.h>

#if defined(MBEDTLS_SSL_CACHE_C)
#include <mbedtls/ssl_cache.h>
#endif

} // extern "C"

#include "common/types.hpp"
#include "dtls.hpp"

namespace ot {

namespace BorderRouter {

namespace Dtls {

/**
 * @addtogroup border-agent-dtls
 *
 * @brief
 *   This module includes definition for mbedTLS-based DTLS service.
 *
 * @{
 */

class MbedtlsServer;

/**
 * This class implements the DTLS Session functionality based on mbedTLS.
 *
 */
class MbedtlsSession : public Session
{
    friend class MbedtlsServer;

public:
    /**
     * The constructor to initialize a DTLS session.
     *
     * @param[in]   aServer     A reference to the DTLS server.
     * @param[in]   aNet        A reference to the mbedtls_net_context.
     *
     */
    MbedtlsSession(MbedtlsServer &aServer, mbedtls_net_context &aNet, const uint8_t *aIp, size_t aIpLength);

    ~MbedtlsSession(void);

    ssize_t Write(const uint8_t *aBuffer, uint16_t aLength);
    void SetDataHandler(DataHandler aDataHandler, void *aContext);

    /**
     * This method returns the current state of this session.
     *
     * @returns The current state.
     *
     */
    State GetState(void) const { return mState; }

    /**
     * This method returns the underlying unix fd of this session.
     *
     * @returns Unix file descriptor.
     *
     */
    int GetFd(void) const { return mNet.fd; }

    /**
     * This method returns the expiration of this session.
     *
     * @returns Expiration in miniseconds.
     *
     */
    uint64_t GetExpiration() const { return mExpiration; }

    /**
     * This method returns the exported KEK of this session.
     *
     * @returns A pointer to the KEK data.
     */
    const uint8_t *GetKek(void) { return mKek; }

    /**
     * This method performs the session processing.
     *
     */
    void Process(void);

    /**
     * This method closes the DTLS session.
     *
     */
    void Close(void);

private:
    enum
    {
        kMaxPacketSize  = 1500,  ///< Max size of DTLS UDP packet.
        kSessionTimeout = 60000, ///< Default DTLS session timeout in miniseconds.
        kKekSize        = 32,    ///< Size of KEK.
    };

    static int ExportKeys(void *aContext, const unsigned char *aMasterSecret, const unsigned char *aKeyBlock,
                          size_t aMacLength, size_t aKeyLength, size_t aIvLength);
    int Handshake(void);
    int Read(void);
    void SetState(State aState);

    mbedtls_net_context          mNet;
    mbedtls_timing_delay_context mTimer;
    mbedtls_ssl_context          mSsl;

    DataHandler                  mDataHandler;
    void                        *mContext;
    State                        mState;
    MbedtlsServer               &mServer;
    uint64_t                     mExpiration;
    uint8_t                      mKek[kKekSize];
};

/**
 * This class implements DTLS server functionality based on mbedTLS.
 *
 */
class MbedtlsServer : public Server
{
    friend class MbedtlsSession;

public:
    /**
     * The constructor to initialize a DTLS server.
     *
     * @param[in]   aPort           The listening port of this DTLS server.
     * @param[in]   aStateHandler   A pointer to the function to be called when an session's state changed.
     * @param[in]   aContext        A pointer to application-specific context.
     *
     */
    MbedtlsServer(uint16_t aPort, StateHandler aStateHandler, void *aContext) :
        mPort(aPort),
        mStateHandler(aStateHandler),
        mContext(aContext) {}
    ~MbedtlsServer(void);


    /**
     * This method starts the DTLS service.
     *
     */
    virtual void Start(void);

    void UpdateFdSet(fd_set &aReadFdSet, fd_set &aWriteFdSet, int &aMaxFd, timeval &aTimeout);

    void Process(const fd_set &aReadFdSet, const fd_set &aWriteFdSet);

    /**
     * This method updates the PSK of TLS_ECJPAKE_WITH_AES_128_CCM_8 used by this server.
     *
     * @param[in]   aPSK    A pointer to the PSK buffer.
     * @param[in]   aLength Length of PSK.
     *
     */
    void SetPSK(const uint8_t *aPSK, uint8_t aLength);

    /**
     * This method updates the seed for random generator.
     *
     * @param[in]   aSeed   A pointer to the buffer of seed.
     * @param[in]   aLength The length of the seed.
     *
     */
    void SetSeed(const uint8_t *aSeed, uint16_t aLength);

private:
    typedef std::vector< boost::shared_ptr<MbedtlsSession> > SessionSet;
    enum
    {
        kMaxSizeOfPSK = 32, ///< Max size of PSK in bytes.
    };

    void HandleSessionState(Session &aSession, Session::State aState);
    void ProcessServer(const fd_set &aReadFdSet, const fd_set &aWriteFdSet);

    SessionSet                mSessions;
    uint16_t                  mPort;
    StateHandler              mStateHandler;
    void                     *mContext;
    uint8_t                   mSeed[MBEDTLS_CTR_DRBG_MAX_SEED_INPUT];
    uint16_t                  mSeedLength;
    uint8_t                   mPSK[kMaxSizeOfPSK];
    uint8_t                   mPSKLength;

    mbedtls_net_context       mNet;
    mbedtls_ssl_cookie_ctx    mCookie;
    mbedtls_entropy_context   mEntropy;
    mbedtls_ctr_drbg_context  mCtrDrbg;
    mbedtls_ssl_config        mConf;
#if defined(MBEDTLS_SSL_CACHE_C)
    mbedtls_ssl_cache_context mCache;
#endif
};

/**
 * @}
 */

} // namespace Dtls

} // namespace BorderRouter

} // namespace ot

#endif  //DTLS_MBEDTLS_HPP_
