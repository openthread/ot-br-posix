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
 *   This file includes implementation for Thread border router agent instance.
 */

#include "agent_instance.hpp"

#include <assert.h>

#include "common/code_utils.hpp"
#include "common/logging.hpp"

namespace ot {

namespace BorderRouter {

AgentInstance::AgentInstance(const char *aIfName) :
    mNcp(Ncp::Controller::Create(aIfName)),
    mCoap(Coap::Agent::Create(SendCoap, this)),
    mBorderAgent(mNcp, mCoap) {}

otbrError AgentInstance::Init(void)
{
    otbrError error = OTBR_ERROR_NONE;

    SuccessOrExit(error = mNcp->Init());

    mNcp->On(Ncp::kEventTmfProxyStream, FeedCoap, this);

    SuccessOrExit(error = mNcp->TmfProxyStart());

    SuccessOrExit(error = mBorderAgent.Start());

exit:
    if (error != OTBR_ERROR_NONE)
    {
        otbrLog(OTBR_LOG_ERR, "Failed to create border route agent instance: %d!", error);
    }

    return error;
}

void AgentInstance::UpdateFdSet(fd_set &aReadFdSet, fd_set &aWriteFdSet, fd_set &aErrorFdSet, int &aMaxFd,
                                timeval &aTimeout)
{
    mNcp->UpdateFdSet(aReadFdSet, aWriteFdSet, aErrorFdSet, aMaxFd);
    mBorderAgent.UpdateFdSet(aReadFdSet, aWriteFdSet, aErrorFdSet, aMaxFd, aTimeout);
}

void AgentInstance::Process(const fd_set &aReadFdSet, const fd_set &aWriteFdSet, const fd_set &aErrorFdSet)
{
    mNcp->Process(aReadFdSet, aWriteFdSet, aErrorFdSet);
    mBorderAgent.Process(aReadFdSet, aWriteFdSet, aErrorFdSet);
}

void AgentInstance::FeedCoap(void *aContext, int aEvent, va_list aArguments)
{
    assert(aEvent == Ncp::kEventTmfProxyStream);

    AgentInstance *agentInstance = static_cast<AgentInstance *>(aContext);
    const uint8_t *buffer = va_arg(aArguments, const uint8_t *);
    uint16_t       length = va_arg(aArguments, int);
    uint16_t       locator = va_arg(aArguments, int);
    uint16_t       port = va_arg(aArguments, int);
    Ip6Address     addr(locator);

    agentInstance->mCoap->Input(buffer, length, addr.m8, port);
}

ssize_t AgentInstance::SendCoap(const uint8_t *aBuffer, uint16_t aLength, const uint8_t *aIp6, uint16_t aPort,
                                void *aContext)
{
    return static_cast<AgentInstance *>(aContext)->SendCoap(aBuffer, aLength, aIp6, aPort);
}

ssize_t AgentInstance::SendCoap(const uint8_t *aBuffer, uint16_t aLength, const uint8_t *aIp6, uint16_t aPort)
{
    const Ip6Address *addr = reinterpret_cast<const Ip6Address *>(aIp6);
    uint16_t          rloc = addr->ToLocator();

    mNcp->TmfProxySend(aBuffer, aLength, rloc, aPort);
    return aLength;
}

AgentInstance::~AgentInstance(void)
{
    otbrError error = OTBR_ERROR_NONE;

    Coap::Agent::Destroy(mCoap);

    if ((error = mNcp->TmfProxyStop()))
    {
        otbrLog(OTBR_LOG_ERR, "Failed to stop TMF proxy: %d!", error);
    }

    Ncp::Controller::Destroy(mNcp);
}

} // namespace BorderRouter

} // namespace ot
