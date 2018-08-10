#ifndef OTBR_JOINER_SESSION_H_
#define OTBR_JOINER_SESSION_H_

#include <arpa/inet.h>
#include <errno.h>
#include <sys/socket.h>
#include <web/pskc-generator/pskc.hpp>
#include "agent/coap.hpp"
#include "agent/dtls.hpp"

namespace ot {
namespace BorderRouter {

class JoinerSession
{
public:
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

    bool NeedAppendKek();

    void MarkKekSent();

    void GetKek(uint8_t *aBuf, size_t aBufSize);

    ssize_t Write(const uint8_t *aBuffer, uint16_t aLength);

    ssize_t RecvFrom(void *aBuf, size_t aSize, struct sockaddr *aFromAddr, size_t &aAddrlen);

    ~JoinerSession();

    static const size_t kKekSize = 32;
private:
    JoinerSession(const JoinerSession &);
    JoinerSession &operator=(const JoinerSession &);

    static void HandleSessionChange(Dtls::Session &aSession, Dtls::Session::State aState, void *aContext);
    static ssize_t SendCoap(const uint8_t *aBuffer,
                            uint16_t       aLength,
                            const uint8_t *aIp6,
                            uint16_t       aPort,
                            void *         aContext);
    static void FeedCoap(const uint8_t *aBuffer, uint16_t aLength, void *aContext);
    static void HandleJoinerFinalize(const Coap::Resource &aResource,
                          const Coap::Message & aRequest,
                          Coap::Message &       aResponse,
                          const uint8_t *       aIp6,
                          uint16_t              aPort,
                          void *                aContext);


    uint8_t mKek[32];

    Dtls::Server *mDtlsServer;
    Dtls::Session *mDtlsSession;
    Coap::Agent * mCoapAgent;
    Coap::Resource mJoinerFinalizeHandler;
    bool mNeedAppendKek;
};

} // namespace BorderRouter
} // namespace ot

#endif
