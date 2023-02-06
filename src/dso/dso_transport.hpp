/*
 *    Copyright (c) 2023, The OpenThread Authors.
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
 *   This file includes definitions for DSO agent.
 */

#ifndef OTBR_AGENT_DSO_TRANSPORT_HPP_
#define OTBR_AGENT_DSO_TRANSPORT_HPP_

#if OTBR_ENABLE_DNS_DSO

#include <arpa/inet.h>
#include <map>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <vector>

#include <cassert>
#include <list>

#include "common/code_utils.hpp"
#include "common/mainloop.hpp"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/debug.h"
#include "mbedtls/entropy.h"
#include "mbedtls/error.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "ncp/ncp_openthread.hpp"
#include "openthread/logging.h"
#include "openthread/message.h"
#include "openthread/platform/dso_transport.h"

class DsoTestFixture;

namespace otbr {
namespace Dso {

// TODO: Support DNS-over-TLS
/**
 * @addtogroup border-router-dso
 *
 * @brief
 *   This module includes definitions for DSO agent.
 *
 * @{
 */

class DsoAgent : public MainloopProcessor
{
private:
    class DsoConnection;

    friend class ::DsoTestFixture;

public:
    using AcceptHandler       = std::function<otPlatDsoConnection *(otInstance *, otSockAddr *)>;
    using ConnectedHandler    = std::function<void(DsoConnection *)>;
    using DisconnectedHandler = std::function<void(DsoConnection *, otPlatDsoDisconnectMode)>;
    using ReceiveHandler      = std::function<void(DsoConnection *, const std::vector<uint8_t> &)>;

    /**
     * This constructor initializes the DsoAgent instance.
     *
     */
    explicit DsoAgent(void);

    /**
     * This destructor destroys the DsoAgent instance.
     *
     */
    ~DsoAgent(void);

    /**
     * This method initializes the DsoAgent instance.
     *
     * @param[in] aInstance  The OT instance.
     *
     */
    void Init(otInstance *aInstance, const std::string &aInfraNetIfName);

    /**
     * This method enables listening for DsoAgent.
     *
     * @param[in] aInstance  The OT instance.
     *
     */
    void Enable(otInstance *aInstance);

    /**
     * This method disables listening for DsoAgent.
     *
     * @param[in] aInstance  The OT instance.
     *
     */
    void Disable(otInstance *aInstance);

    /**
     * This method enables/disables listening for DsoAgent.
     *
     * @param[in] aInstance  The OT instance.
     * @param[in] aEnabled   A boolean indicating whether to enable listening.
     *
     */
    void SetEnabled(otInstance *aInstance, bool aEnabled);

    /**
     * This method finds the DsoConnection corresponding to the given otPlatDsoConnection.
     *
     * @param[in] aConnection  A pointer to the otPlatDsoConnection.
     *
     * @returns  A pointer to the matching DsoConnection object.
     *
     */
    DsoConnection *FindConnection(otPlatDsoConnection *aConnection);

    /**
     * This method finds the DsoConnection corresponding to the given otPlatDsoConnection. If the DsoConnection doesn't
     * exist, it creates a new one.
     *
     * @param[in] aConnection  A pointer to the otPlatDsoConnection.
     *
     * @returns  A pointer to the matching DsoConnection object.
     *
     */
    DsoConnection *FindOrCreateConnection(otPlatDsoConnection *aConnection);

    /**
     * This method finds the DsoConnection corresponding to the given otPlatDsoConnection. If the DsoConnection doesn't
     * exist, it creates a new one.
     *
     * @param[in] aConnection  A pointer to the otPlatDsoConnection.
     * @param[in] aCtx         A mbedtls_net_context object representing the platform connection. This is used for
     *                         creating the DsoConnection.
     *
     * @returns  A pointer to the matching DsoConnection object.
     *
     */
    DsoConnection *FindOrCreateConnection(otPlatDsoConnection *aConnection, mbedtls_net_context aCtx);

    /**
     * This method removes and destroys the DsoConnection corresponding to the given otPlatDsoConnection.
     *
     * @param aConnection  A pointer to the otPlatDsoConnection.
     *
     */
    void RemoveConnection(otPlatDsoConnection *aConnection);

    /**
     * This method sets the handlers for each event. This method can decouple DsoAgent from OT API so that we can
     * implement unit tests for DsoAgent.
     *
     * @param aAcceptHandler        The handler is called when a DsoConnection gets accepted.
     * @param aConnectedHandler     The handler is called when a DsoConnection gets connected.
     * @param aDisconnectedHandler  The handler is called when a DsoConnection is disconnected.
     * @param aReceiveHandler       The handler is called when a DsoConnection receives a DSO message.
     *
     */
    void SetHandlers(AcceptHandler       aAcceptHandler,
                     ConnectedHandler    aConnectedHandler,
                     DisconnectedHandler aDisconnectedHandler,
                     ReceiveHandler      aReceiveHandler);

    /**
     * This function is the default AcceptHandler for DsoAgent. It basically calls otPlatDsoAccept().
     *
     * @param aInstance               A pointer to the OT instance.
     * @param aSockAddr               A pointer to the peer socket address.
     *
     * @returns otPlatDsoConnection*  A pointer to the otPlatDsoConnection.
     *
     */
    static otPlatDsoConnection *DefaultAcceptHandler(otInstance *aInstance, otSockAddr *aSockAddr);

    /**
     * This function is the default ConnectedHandler for DsoAgent. It basically calls otPlatDsoHandleConnected().
     *
     * @param aConnection  A pointer to the DsoConnection.
     *
     */
    static void DefaultConnectedHandler(DsoConnection *aConnection);

    /**
     * This function is the default DisconnectedHandler for DsoAgent. It basically calls otPlatDsoHandleDisconnected().
     *
     * @param aConnection  A pointer to the DsoConnection.
     * @param aMode        The disconnection mode.
     *
     */
    static void DefaultDisconnectedHandler(DsoConnection *aConnection, otPlatDsoDisconnectMode aMode);

    /**
     * This function is the default ReceiveHandler for DsoAgent. It basically calls otPlatHandleReceive().
     *
     * @param aConnection  A pointer to the DsoConnection.
     * @param aData        A vector of bytes representing the received data.
     *
     */
    static void DefaultReceiveHandler(DsoConnection *aConnection, const std::vector<uint8_t> &aData);

private:
    class DsoConnection : NonCopyable
    {
    public:
        enum class State
        {
            kDisabled,
            kConnecting,
            kConnected,
#if OTBR_ENABLE_DNS_DSO_TLS
            kTlsHandshaking,
#endif
        };

        DsoConnection(DsoAgent *aAgent, otPlatDsoConnection *aConnection);

        DsoConnection(DsoAgent *aAgent, otPlatDsoConnection *aConnection, mbedtls_net_context aCtx);

        ~DsoConnection(void);

        static const char *StateToString(State aState);

        State GetState(void) const
        {
            return mState;
        }

        int GetFd(void) const
        {
            return mCtx.fd;
        }

        otPlatDsoConnection *GetOtPlatDsoConnection(void) const
        {
            return mConnection;
        }

        void Connect(const otSockAddr *aPeerSockAddr);

        void Disconnect(otPlatDsoDisconnectMode aMode);

        void Send(const std::vector<uint8_t> &aData);

        void Send(otMessage *aMessage);

        void HandleReceive(void);

        void HandleMbedTlsError(int aError); // Returns true if the connection is still alive, false otherwise.

        void UpdateStateBySocketState(void); // Update the state according to socket's state.

        void MarkStateAs(State aState);

        void SetContext(mbedtls_net_context aCtx);

#if OTBR_ENABLE_DNS_DSO_TLS
        void TlsHandshake(void);
#endif

    private:
        static constexpr size_t kRxBufferSize = 512;

        friend class ::DsoTestFixture;
        friend class DsoAgent;

        void FlushSendBuffer(void);

        DsoAgent            *mAgent;
        otPlatDsoConnection *mConnection;
        otSockAddr           mPeerSockAddr{};
        size_t               mNeedBytes = 0;
        std::vector<uint8_t> mReceiveLengthBuffer;
        std::vector<uint8_t> mReceiveMessageBuffer;
        std::vector<uint8_t> mSendMessageBuffer;
        mbedtls_net_context  mCtx{};
        State                mState{State::kDisabled};

#if OTBR_ENABLE_DNS_DSO_TLS
        mbedtls_ssl_context mTlsCtx{};
#endif
    };

#if OTBR_ENABLE_DNS_DSO_TLS
    class TlsConfig
    {
    public:
        ~TlsConfig(void);

        void Init(bool aIsClient);

        mbedtls_ssl_config &GetConfig(void) { return mConfig; }

    private:
        friend class DsoAgent;

        // TODO: The below certificates are used as MbedTLS examples. We should use real certificates later.

        static constexpr const char  *kCasPem       = "-----BEGIN CERTIFICATE-----\r\n"
                                                      "MIIBtDCCATqgAwIBAgIBTTAKBggqhkjOPQQDAjBLMQswCQYDVQQGEwJOTDERMA8G\r\n"
                                                      "A1UEChMIUG9sYXJTU0wxKTAnBgNVBAMTIFBvbGFyU1NMIFRlc3QgSW50ZXJtZWRp\r\n"
                                                      "YXRlIEVDIENBMB4XDTE1MDkwMTE0MDg0M1oXDTI1MDgyOTE0MDg0M1owSjELMAkG\r\n"
                                                      "A1UEBhMCVUsxETAPBgNVBAoTCG1iZWQgVExTMSgwJgYDVQQDEx9tYmVkIFRMUyBU\r\n"
                                                      "ZXN0IGludGVybWVkaWF0ZSBDQSAzMFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAE\r\n"
                                                      "732fWHLNPMPsP1U1ibXvb55erlEVMlpXBGsj+KYwVqU1XCmW9Z9hhP7X/5js/DX9\r\n"
                                                      "2J/utoHyjUtVpQOzdTrbsaMQMA4wDAYDVR0TBAUwAwEB/zAKBggqhkjOPQQDAgNo\r\n"
                                                      "ADBlAjAJRxbGRas3NBmk9MnGWXg7PT1xnRELHRWWIvfLdVQt06l1/xFg3ZuPdQdt\r\n"
                                                      "Qh7CK80CMQD7wa1o1a8qyDKBfLN636uKmKGga0E+vYXBeFCy9oARBangGCB0B2vt\r\n"
                                                      "pz590JvGWfM=\r\n"
                                                      "-----END CERTIFICATE-----\r\n";
        static constexpr const size_t kCasPemLength = 665; // includes NUL byte

        static constexpr const char  *kSrvPem       = "-----BEGIN CERTIFICATE-----\r\n"
                                                      "MIICHzCCAaWgAwIBAgIBCTAKBggqhkjOPQQDAjA+MQswCQYDVQQGEwJOTDERMA8G\r\n"
                                                      "A1UEChMIUG9sYXJTU0wxHDAaBgNVBAMTE1BvbGFyc3NsIFRlc3QgRUMgQ0EwHhcN\r\n"
                                                      "MTMwOTI0MTU1MjA0WhcNMjMwOTIyMTU1MjA0WjA0MQswCQYDVQQGEwJOTDERMA8G\r\n"
                                                      "A1UEChMIUG9sYXJTU0wxEjAQBgNVBAMTCWxvY2FsaG9zdDBZMBMGByqGSM49AgEG\r\n"
                                                      "CCqGSM49AwEHA0IABDfMVtl2CR5acj7HWS3/IG7ufPkGkXTQrRS192giWWKSTuUA\r\n"
                                                      "2CMR/+ov0jRdXRa9iojCa3cNVc2KKg76Aci07f+jgZ0wgZowCQYDVR0TBAIwADAd\r\n"
                                                      "BgNVHQ4EFgQUUGGlj9QH2deCAQzlZX+MY0anE74wbgYDVR0jBGcwZYAUnW0gJEkB\r\n"
                                                      "PyvLeLUZvH4kydv7NnyhQqRAMD4xCzAJBgNVBAYTAk5MMREwDwYDVQQKEwhQb2xh\r\n"
                                                      "clNTTDEcMBoGA1UEAxMTUG9sYXJzc2wgVGVzdCBFQyBDQYIJAMFD4n5iQ8zoMAoG\r\n"
                                                      "CCqGSM49BAMCA2gAMGUCMQCaLFzXptui5WQN8LlO3ddh1hMxx6tzgLvT03MTVK2S\r\n"
                                                      "C12r0Lz3ri/moSEpNZWqPjkCMCE2f53GXcYLqyfyJR078c/xNSUU5+Xxl7VZ414V\r\n"
                                                      "fGa5kHvHARBPc8YAIVIqDvHH1Q==\r\n"
                                                      "-----END CERTIFICATE-----\r\n";
        static constexpr const size_t kSrvPemLength = 813; // includes NUL byte

        static constexpr const char  *kSrvKey       = "-----BEGIN EC PRIVATE KEY-----\r\n"
                                                      "MHcCAQEEIPEqEyB2AnCoPL/9U/YDHvdqXYbIogTywwyp6/UfDw6noAoGCCqGSM49\r\n"
                                                      "AwEHoUQDQgAEN8xW2XYJHlpyPsdZLf8gbu58+QaRdNCtFLX3aCJZYpJO5QDYIxH/\r\n"
                                                      "6i/SNF1dFr2KiMJrdw1VzYoqDvoByLTt/w==\r\n"
                                                      "-----END EC PRIVATE KEY-----\r\n";
        static constexpr const size_t kSrvKeyLength = 233; // includes NUL byte

        static const int kCipherSuites[3];

        void LoadCert(void);

        mbedtls_entropy_context  mEntropy;
        mbedtls_ctr_drbg_context mCtrDrbg;
        mbedtls_ssl_config       mConfig;
        mbedtls_x509_crt         mSrvCert;
        mbedtls_pk_context       mPKey;
    };
#endif

    static constexpr uint16_t kListeningPort        = 853;
    static constexpr int      kMaxQueuedConnections = 10;

    friend class DsoConnection;

    void Update(MainloopContext &aMainloop) override;
    void Process(const MainloopContext &aMainloop) override;

    void ProcessIncomingConnections(void);
    void ProcessIncomingConnection(mbedtls_net_context aCtx, uint8_t *aAddress, size_t aAddressLength);
    void ProcessConnections(const MainloopContext &aMainloop);

    otPlatDsoConnection *HandleAccept(otInstance *aInstance, otSockAddr *aSockAddr);
    void                 HandleConnected(DsoConnection *aConnection);
    void                 HandleDisconnected(DsoConnection *aConnection, otPlatDsoDisconnectMode aMode);
    void                 HandleReceive(DsoConnection *aConnection, const std::vector<uint8_t> &aData);

    otInstance         *mInstance = nullptr;
    std::string         mInfraNetIfName;
    bool                mListeningEnabled = false;
    mbedtls_net_context mListeningCtx;

    AcceptHandler       mAcceptHandler{&DsoAgent::DefaultAcceptHandler};
    ConnectedHandler    mConnectedHandler{&DsoAgent::DefaultConnectedHandler};
    DisconnectedHandler mDisconnectedHandler{&DsoAgent::DefaultDisconnectedHandler};
    ReceiveHandler      mReceiveHandler{&DsoAgent::DefaultReceiveHandler};

    std::map<otPlatDsoConnection *, std::unique_ptr<DsoConnection>> mMap;

#if OTBR_ENABLE_DNS_DSO_TLS
    TlsConfig mTlsClientConfig;
    TlsConfig mTlsServerConfig;
#endif
};

/**
 * @}
 */

} // namespace Dso
} // namespace otbr

#endif // OTBR_ENABLE_DNS_DSO

#endif // OTBR_AGENT_DSO_TRANSPORT_HPP_
