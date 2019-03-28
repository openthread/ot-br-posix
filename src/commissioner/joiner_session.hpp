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
 *   The file is the header for the dtls session class to communicate with the joiner
 */

#ifndef OTBR_JOINER_SESSION_HPP_
#define OTBR_JOINER_SESSION_HPP_

#include <sys/socket.h>

#include "commissioner_constants.hpp"
#include "common/coap.hpp"
#include "common/dtls.hpp"
#include "utils/pskc.hpp"

namespace ot {
namespace BorderRouter {

class JoinerSession
{
public:
    /**
     * The constructor to initialize JoinerSession
     *
     * @param[in]    aInternalServerPort    port for internal dtls server to listen to
     * @param[in]    aPskdAscii             ascii form of pskd
     *
     */
    JoinerSession(uint16_t aInternalServerPort, const char *aPskdAscii);

    /**
     * This method updates the fd_set and timeout @p aTimeout should
     * only be updated if session has pending process in less than its current value.
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
     * This method returns whether the underlying relay service should append kek after dtls encapsulation
     * @returns whether the underlying relay service should append kek after dtls encapsulation
     */
    bool NeedAppendKek(void);

    /**
     * This method is used by the underlying relay service to notify key has been appended
     */
    void MarkKekSent(void);

    /**
     * This method copies to the key encrypted key(KEK) to buffer provided
     * @param[out] aBuf         output buffer to copy kek to
     * @param[in]  aBufSize     size of output buffer
     */
    void GetKek(uint8_t *aBuf, size_t aBufSize);

    /**
     * This method writes data to joiner session, the data will be relayed to dtls server
     * @param[in]  aBuf         data buffer to write
     * @param[in]  aLength      data length to write
     *
     * @returns on success returns a non-negative number representing the number written,
     *          on failure returns a negative errorno
     */
    ssize_t Write(const uint8_t *aBuf, uint16_t aLength);

    ~JoinerSession();

private:
    JoinerSession(const JoinerSession &);
    JoinerSession &operator=(const JoinerSession &);

    static void    HandleSessionChange(Dtls::Session &aSession, Dtls::Session::State aState, void *aContext);
    static ssize_t SendCoap(const uint8_t *aBuffer,
                            uint16_t       aLength,
                            const uint8_t *aIp6,
                            uint16_t       aPort,
                            void *         aContext);
    static void    FeedCoap(const uint8_t *aBuffer, uint16_t aLength, void *aContext);
    static void    HandleJoinerFinalize(const Coap::Resource &aResource,
                                        const Coap::Message & aRequest,
                                        Coap::Message &       aResponse,
                                        const uint8_t *       aIp6,
                                        uint16_t              aPort,
                                        void *                aContext);

    uint8_t mKek[kKEKSize];

    Dtls::Server * mDtlsServer;
    Dtls::Session *mDtlsSession;
    Coap::Agent *  mCoapAgent;
    Coap::Resource mJoinerFinalizeHandler;
    bool           mNeedAppendKek;
};

} // namespace BorderRouter
} // namespace ot

#endif // OTBR_JOINER_SESSION_HPP_
