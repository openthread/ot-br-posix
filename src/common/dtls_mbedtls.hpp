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

#include <vector>

#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" {

#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif

#if defined(MBEDTLS_PLATFORM_C)
#include <mbedtls/platform.h>
#endif

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/debug.h>
#include <mbedtls/ecjpake.h>
#include <mbedtls/entropy.h>
#include <mbedtls/error.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/ssl.h>
#include <mbedtls/ssl_cookie.h>
#include <mbedtls/timing.h>

#if defined(MBEDTLS_SSL_CACHE_C)
#include <mbedtls/ssl_cache.h>
#endif

} // extern "C"

#include "dtls.hpp"

namespace ot {

namespace BorderRouter {

namespace Dtls {

/**
 * @addtogroup border-router-dtls
 *
 * @brief
 *   This module includes definition for mbedTLS-based DTLS service.
 *
 * @{
 */

class MbedtlsServer;

enum
{
    kMaxSizeOfPacket  = 1500, ///< Max size of packet in bytes.
    kMaxSizeOfControl = 1500, ///< Max size of control message in bytes.
};

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
     * @param[in]   aRemoteSock A reference to the remote sockaddr of this session.
     * @param[in]   aLocalSock  A reference to the local sockaddr of this session.
     *
     */
    MbedtlsSession(MbedtlsServer &            aServer,
                   const mbedtls_net_context &aNet,
                   const struct sockaddr_in6 &aRemoteSock,
                   const struct sockaddr_in6 &aLocalSock);

    ~MbedtlsSession(void);

    /**
     * This method initialize the session.
     *
     * @retval  OTBR_ERROR_NONE     Initialized successfully.
     * @retval  OTBR_ERROR_DTLS     Failed to initialize.
     *
     */
    otbrError Init(void);

    ssize_t Write(const uint8_t *aBuffer, uint16_t aLength);
    void    SetDataHandler(DataHandler aDataHandler, void *aContext);

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
        kSessionTimeout = 60000, ///< Default DTLS session timeout in miniseconds.
        kKekSize        = 32,    ///< Size of KEK.
    };

    static int ExportKeys(void *               aContext,
                          const unsigned char *aMasterSecret,
                          const unsigned char *aKeyBlock,
                          size_t               aMacLength,
                          size_t               aKeyLength,
                          size_t               aIvLength);
    int        Handshake(void);
    int        Read(void);
    void       SetState(State aState);
    static int SendMbedtls(void *aContext, const unsigned char *aBuffer, size_t aLength)
    {
        return static_cast<MbedtlsSession *>(aContext)->SendMbedtls(aBuffer, aLength);
    }
    int SendMbedtls(const unsigned char *aBuffer, size_t aLength);

    static int ReadMbedtls(void *aContext, unsigned char *aBuffer, size_t aLength)
    {
        return static_cast<MbedtlsSession *>(aContext)->ReadMbedtls(aBuffer, aLength);
    }
    int ReadMbedtls(unsigned char *aBuffer, size_t aLength);

    static void SetDelay(void *aContext, uint32_t aIntermediate, uint32_t aFinal);
    void        SetDelay(uint32_t aIntermediate, uint32_t aFinal);
    static int  GetDelay(void *aContext);
    int         GetDelay(void) const;

    mbedtls_net_context mNet;
    mbedtls_ssl_context mSsl;

    DataHandler    mDataHandler;
    void *         mContext;
    State          mState;
    sockaddr_in6   mRemoteSock;
    sockaddr_in6   mLocalSock;
    MbedtlsServer &mServer;
    unsigned long  mExpiration;
    uint8_t        mKek[kKekSize];
    unsigned long  mIntermediate;
    unsigned long  mFinal;
    bool           mIsTimerSet;
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
     * @param[in]   aPort               The listening port of this DTLS server.
     * @param[in]   aStateHandler       A pointer to the function to be called when an session's state changed.
     * @param[in]   aContext            A pointer to application-specific context.
     *
     */
    MbedtlsServer(uint16_t aPort, StateHandler aStateHandler, void *aContext)
        : mSocket(-1)
        , mPort(aPort)
        , mStateHandler(aStateHandler)
        , mContext(aContext)
    {
    }

    ~MbedtlsServer(void);

    /**
     * This method starts the DTLS service.
     *
     * @retval      OTBR_ERROR_NONE     Successfully started.
     * @retval      OTBR_ERROR_ERRNO    Failed to start for system error.
     * @retval      OTBR_ERROR_DTLS     Failed to start for DTLS error.
     *
     */
    virtual otbrError Start(void);

    /**
     * This method updates the fd_set and timeout for mainloop. @p aTimeout should
     * only be updated if the DTLS service has pending process in less than its current value.
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
     * This method performs the DTLS processing.
     *
     * @param[in]   aReadFdSet          A reference to fd_set ready for reading.
     * @param[in]   aWriteFdSet         A reference to fd_set ready for writing.
     * @param[in]   aErrorFdSet         A reference to fd_set with error occurred.
     *
     */
    void Process(const fd_set &aReadFdSet, const fd_set &aWriteFdSet, const fd_set &aErrorFdSet);

    /**
     * This method updates the PSK of TLS_ECJPAKE_WITH_AES_128_CCM_8 used by this server.
     *
     * @param[in]   aPSK                A pointer to the PSK buffer.
     * @param[in]   aLength             The length of the PSK.
     *
     * @retval      OTBR_ERROR_NONE     Successfully set PSK.
     * @retval      OTBR_ERROR_ERRNO    Failed for the given PSK is too long.
     *
     */
    otbrError SetPSK(const uint8_t *aPSK, uint8_t aLength);

    /**
     * This method updates the seed for random generator.
     *
     * @param[in]   aSeed               A pointer to seed.
     * @param[in]   aLength             The length of the seed.
     *
     * @retval      OTBR_ERROR_NONE     Successfully set seed.
     * @retval      OTBR_ERROR_ERRNO    Failed for the given seed is too long.
     *
     */
    otbrError SetSeed(const uint8_t *aSeed, uint16_t aLength);

private:
    typedef std::vector<MbedtlsSession *> SessionSet;
    enum
    {
        kMaxSizeOfPSK = 32, ///< Max size of PSK in bytes.
    };

    void HandleSessionState(Session &aSession, Session::State aState);
    void ProcessServer(const fd_set &aReadFdSet, const fd_set &aWriteFdSet, const fd_set &aErrorFdSet);

    otbrError Bind(void);

    static void MbedtlsDebug(void *aContext, int aLevel, const char *aFile, int aLine, const char *aMessage);
    void        MbedtlsDebug(int aLevel, const char *aFile, int aLine, const char *aMessage);

    SessionSet   mSessions;
    int          mSocket;
    uint16_t     mPort;
    StateHandler mStateHandler;
    void *       mContext;
    uint8_t      mSeed[MBEDTLS_CTR_DRBG_MAX_SEED_INPUT];
    uint16_t     mSeedLength;
    uint8_t      mPSK[kMaxSizeOfPSK];
    uint8_t      mPSKLength;

    mbedtls_ssl_cookie_ctx   mCookie;
    mbedtls_entropy_context  mEntropy;
    mbedtls_ctr_drbg_context mCtrDrbg;
    mbedtls_ssl_config       mConf;
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

#endif // DTLS_MBEDTLS_HPP_
