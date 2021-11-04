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
 *   This file includes definition for Thread border router agent instance.
 */

#ifndef OTBR_AGENT_AGENT_INSTANCE_HPP_
#define OTBR_AGENT_AGENT_INSTANCE_HPP_

#include "openthread-br/config.h"

#include <stdarg.h>
#include <stdint.h>
#include <sys/select.h>
#include <sys/types.h>

#include "agent/instance_params.hpp"
#include "border_agent/border_agent.hpp"
#include "ncp/ncp_openthread.hpp"

namespace otbr {

/**
 * This class implements an instance to host services used by border router.
 *
 */
class AgentInstance
{
public:
    /**
     * The constructor to initialize the Thread border router agent instance.
     *
     * @param[in] aNcp  A reference to the NCP controller.
     *
     */
    AgentInstance(Ncp::ControllerOpenThread &aNcp);

    /**
     * This method initialize the agent.
     *
     * @retval OTBR_ERROR_NONE   Agent initialized successfully.
     * @retval OTBR_ERROR_ERRNO  Failed due to error indicated in errno.
     *
     */
    otbrError Init(void);

    /**
     * This method returns the NCP controller.
     *
     * @returns The pointer of the NCP controller.
     *
     */
    otbr::Ncp::ControllerOpenThread &GetNcp(void) { return mNcp; }

private:
    otbr::Ncp::ControllerOpenThread &mNcp;
    BorderAgent                      mBorderAgent;
};

} // namespace otbr

#endif // OTBR_AGENT_AGENT_INSTANCE_HPP_
