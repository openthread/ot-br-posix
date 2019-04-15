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
 *   The file implements the dtls session class to communicate with the joiner
 */

#include "joiner_session.hpp"
#include "commissioner_utils.hpp"
#include "agent/uris.hpp"
#include "common/logging.hpp"
#include "common/tlv.hpp"

namespace ot {
namespace BorderRouter {

JoinerSession::JoinerSession(uint16_t aInternalServerPort, const char *aPskdAscii)
    : mDtlsServer(Dtls::Server::Create(aInternalServerPort, JoinerSession::HandleSessionChange, this))
    , mCoapAgent(Coap::Agent::Create(JoinerSession::SendCoap, this))
    , mJoinerFinalizeHandler(OT_URI_PATH_JOINER_FINALIZE, HandleJoinerFinalize, this)
    , mNeedAppendKek(false)
{
    mDtlsServer->SetPSK(reinterpret_cast<const uint8_t *>(aPskdAscii), static_cast<uint8_t>(strlen(aPskdAscii)));
    mDtlsServer->Start();
    mCoapAgent->AddResource(mJoinerFinalizeHandler);
}

void JoinerSession::HandleSessionChange(Dtls::Session &aSession, Dtls::Session::State aState, void *aContext)
{
    JoinerSession *joinerSession = static_cast<JoinerSession *>(aContext);

    switch (aState)
    {
    case Dtls::Session::kStateReady:
        memcpy(joinerSession->mKek, aSession.GetKek(), sizeof(joinerSession->mKek));
        aSession.SetDataHandler(JoinerSession::FeedCoap, joinerSession);
        joinerSession->mDtlsSession = &aSession;
        break;

    case Dtls::Session::kStateClose:
        break;

    case Dtls::Session::kStateError:
    case Dtls::Session::kStateEnd:
        joinerSession->mDtlsSession = NULL;
        break;
    default:
        break;
    }
}

ssize_t JoinerSession::SendCoap(const uint8_t *aBuffer,
                                uint16_t       aLength,
                                const uint8_t *aIp6,
                                uint16_t       aPort,
                                void *         aContext)
{
    ssize_t        ret           = 0;
    JoinerSession *joinerSession = static_cast<JoinerSession *>(aContext);

    if (joinerSession->mDtlsSession)
    {
        ret = joinerSession->mDtlsSession->Write(aBuffer, aLength);
    }
    else
    {
        otbrLog(OTBR_LOG_INFO, "SendCoap: error NO SESSION");
        ret = -1;
    }

    (void)aIp6;
    (void)aPort;

    return ret;
}

void JoinerSession::FeedCoap(const uint8_t *aBuffer, uint16_t aLength, void *aContext)
{
    JoinerSession *joinerSession = static_cast<JoinerSession *>(aContext);
    joinerSession->mCoapAgent->Input(aBuffer, aLength, NULL, 0);
}

void JoinerSession::HandleJoinerFinalize(const Coap::Resource &aResource,
                                         const Coap::Message & aRequest,
                                         Coap::Message &       aResponse,
                                         const uint8_t *       aIp6,
                                         uint16_t              aPort,
                                         void *                aContext)
{
    JoinerSession *joinerSession = static_cast<JoinerSession *>(aContext);
    uint8_t        payload[10];
    Tlv *          responseTlv = reinterpret_cast<Tlv *>(payload);

    joinerSession->mNeedAppendKek = true;

    responseTlv->SetType(Meshcop::kState);
    responseTlv->SetValue(static_cast<uint8_t>(Meshcop::kStateAccepted));
    responseTlv = responseTlv->GetNext();

    // Piggyback response
    aResponse.SetCode(Coap::kCodeChanged);
    aResponse.SetPayload(payload, Utils::LengthOf(payload, responseTlv));

    (void)aResource;
    (void)aRequest;
    (void)aIp6;
    (void)aPort;
}

void JoinerSession::Process(const fd_set &aReadFdSet, const fd_set &aWriteFdSet, const fd_set &aErrorFdSet)
{
    mDtlsServer->Process(aReadFdSet, aWriteFdSet, aErrorFdSet);
}

void JoinerSession::UpdateFdSet(fd_set & aReadFdSet,
                                fd_set & aWriteFdSet,
                                fd_set & aErrorFdSet,
                                int &    aMaxFd,
                                timeval &aTimeout)
{
    mDtlsServer->UpdateFdSet(aReadFdSet, aWriteFdSet, aErrorFdSet, aMaxFd, aTimeout);
}

bool JoinerSession::NeedAppendKek(void)
{
    return mNeedAppendKek;
}

void JoinerSession::MarkKekSent(void)
{
    mNeedAppendKek = false;
}

void JoinerSession::GetKek(uint8_t *aBuf, size_t aBufSize)
{
    memcpy(aBuf, mKek, Utils::Min(sizeof(mKek), aBufSize));
}

JoinerSession::~JoinerSession()
{
    Dtls::Server::Destroy(mDtlsServer);
    Coap::Agent::Destroy(mCoapAgent);
}

} // namespace BorderRouter
} // namespace ot
