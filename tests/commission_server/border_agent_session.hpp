#ifndef OTBR_BORDER_AGENT_SESSION_H_
#define OTBR_BORDER_AGENT_SESSION_H_

#include <arpa/inet.h>
#include <errno.h>
#include <sys/socket.h>
#include "agent/coap.hpp"
#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif
#include "commission_common.hpp"
#include <mbedtls/certs.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/debug.h>
#include <mbedtls/ecjpake.h>
#include <mbedtls/entropy.h>
#include <mbedtls/error.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/ssl.h>
#include <mbedtls/timing.h>
#include <web/pskc-generator/pskc.hpp>
#include <set>

namespace ot {
namespace BorderRouter {

class BorderAgentDtlsSession
{
public:
    BorderAgentDtlsSession(const uint8_t *aXpanidBin, const char *aNetworkName, const char *aPassphrase);

    int Connect(const sockaddr_in &aAgentAddr);

    void Disconnect();

    int SetupProxyServer();

    void ShutDownPorxyServer();

    /**
     * This method updates the fd_set and timeout for mainloop. @p aTimeout should
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

private:
    BorderAgentDtlsSession(const BorderAgentDtlsSession &);
    BorderAgentDtlsSession &operator=(const BorderAgentDtlsSession &);

    int DtlsHandShake(const sockaddr_in &aAgentAddr);
    int BecomeCommissioner();
    int CommissionerPetition(Coap::Agent *aCoapAgent, int &aCoapToken);

    static ssize_t SendCoap(const uint8_t *aBuffer,
                            uint16_t       aLength,
                            const uint8_t *aIp6,
                            uint16_t       aPort,
                            void *         aContext);
    static void HandleCommissionerPetition(const Coap::Message &aMessage, void *aContext);

    uint8_t mPskcBin[OT_PSKC_LENGTH];
    uint16_t mCommissionerSessionId;

    mbedtls_net_context          mSslClientFd;
    mbedtls_ssl_context          mSsl;
    mbedtls_entropy_context      mEntropy;
    mbedtls_ctr_drbg_context     mDrbg;
    mbedtls_ssl_config           mSslConf;
    mbedtls_timing_delay_context mTimer;

    int mListenFd;
    std::set<int> mClientFds;

    uint8_t mIOBuffer[kSizeMaxPacket];

    static const uint8_t kSeed[];
    static const int     kCipherSuites[];
    static const char    kCommissionerId[];

    enum {
        kStateConnected,
        kStateAccepted,
        kStateRejected,
        kStateDone,
        kStateInvalid,
    } mCommissionState;
};

} // namespace BorderRouter
} // namespace ot
#endif
