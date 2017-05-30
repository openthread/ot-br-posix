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
 *   This file includes definition for DTLS service.
 */

#ifndef DTLS_HPP_
#define DTLS_HPP_

#include <sys/select.h>

namespace ot {

namespace BorderAgent {

namespace Dtls {

/**
 * @addtogroup border-agent-dtls
 *
 * @brief
 *   This module includes definition for DTLS service.
 *
 * @{
 */

class Server;
class Session;

/**
 * This interface defines DTLS session functionality.
 *
 */
class Session
{
public:

    /**
     * State of a DTLS session.
     *
     */
    enum State
    {
        kStateHandshaking = 0, ///< The session is doing handshaking.
        kStateReady       = 1, ///< The session is established and ready for data transfering.
        kStateEnd         = 2, ///< The session successfully ended.
        kStateError       = 3, ///< The session corrupted.
        kStateExpired     = 4, ///< The session expired.
    };

    /**
     * This function pointer is called when decrypted data are ready for use.
     *
     * @param[in]   aBuffer     A pointer to decrypted data.
     * @param[in]   aLength     Number of bytes of @p aBuffer.
     * @param[in]   aContext    A pointer to application-specific context.
     *
     */
    typedef void (*DataHandler)(const uint8_t *aBuffer, uint16_t aLength, void *aContext);

    /**
     * This method sets the data handler for this session.
     *
     * @param[in]   aDataHandler    A pointer to the function to be called when decrypted data ready.
     * @param[in]   aContext        A pointer to application-specific context.
     *
     */
    virtual void SetDataHandler(DataHandler aDataHandler, void *aContext) = 0;

    /**
     * This method sends data through the session.
     *
     * @param[in]   aBuffer     A pointer to plain data.
     * @param[in]   aLength     Number of bytes of @p aBuffer.
     *
     * @returns number of bytes successfully sended, a negative value indicates failure.
     *
     */
    virtual ssize_t Write(const uint8_t *aBuffer, uint16_t aLength) = 0;

    virtual ~Session(void) {}
};

/**
 * This interface defines DTLS server functionality.
 *
 */
class Server
{
public:
    /**
     * This function pointer is called when the session state changed.
     *
     * @param[in]   aSession    The DTLS session whose state changed.
     * @param[in]   aState      The new state.
     * @param[in]   aContext    A pointer to application-specific context.
     *
     */
    typedef void (*StateHandler)(Session &aSession, Session::State aState, void *aContext);

    /**
     * This method creates a DTLS server.
     *
     * @param[in]   aStateHandler   A pointer to a function to be called when session state changed.
     * @param[in]   aContext        A pointer to application-specific context.
     *
     * @returns pointer to the created the DTLS server.
     */
    static Server *Create(StateHandler aStateHandler, void *aContext);

    /**
     * This method destroy a DTLS server.
     * @param[in]   aServer     A pointer to the DTLS server to be destroyed.
     *
     */
    static void Destroy(Server *aServer);

    /**
     * This method updates the PSK of TLS_ECJPAKE_WITH_AES_128_CCM_8 used by this server.
     *
     * @param[in]   aPSK    A pointer to the PSK buffer of 16 bytes length.
     *
     */
    virtual void SetPSK(const uint8_t *aPSK) = 0;

    /**
     * This method updates the seed for random generator.
     *
     * @param[in]   aSeed   A pointer to the buffer of seed.
     * @param[in]   aLength The length of the seed.
     *
     */
    virtual void SetSeed(const uint8_t *aSeed, uint16_t aLength) = 0;

    /**
     * This method updates the fd_set and timeout for mainloop.
     * @p aTimeout should only be updated if the DTLS service has pending process in less than its current value.
     *
     * @param[inout]    aReadFdSet      A reference to fd_set for polling read.
     * @param[inout]    aWriteFdSet     A reference to fd_set for polling read.
     * @param[inout]    aMaxFd          A reference to the current max fd in @p aReadFdSet and @p aWriteFdSet.
     * @param[inout]    aTimeout        A reference to the timeout.
     *
     */
    virtual void UpdateFdSet(fd_set &aReadFdSet, fd_set &aWriteFdSet, int &aMaxFd, timeval &aTimeout) = 0;

    /**
     * This method performs the DTLS processing.
     * @param[in]   aReadFdSet      A reference to fd_set ready for reading.
     * @param[in]   aWriteFdSet     A reference to fd_set ready for writing.
     */
    virtual void Process(const fd_set &aReadFdSet, const fd_set &aWriteFdSet) = 0;

    virtual ~Server(void) {}
};

/**
 * @}
 */

} // namespace Dtls

} // namespace BorderAgent

} // namespace ot

#endif  // DTLS_HPP_
