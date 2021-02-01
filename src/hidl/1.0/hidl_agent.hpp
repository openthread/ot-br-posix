/*
 *    Copyright (c) 2021, The OpenThread Authors.
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

#pragma once

#include <openthread-br/config.h>

#include "agent/ncp_openthread.hpp"
#include "hidl/1.0/hidl_settings.hpp"
#include "hidl/1.0/hidl_thread.hpp"

namespace otbr {
namespace Hidl {

/**
 * This class implements the HIDL agent.
 *
 */
struct HidlAgent
{
public:
    /**
     * The constructor of a HIDL object.
     *
     * Will use the default interfacename
     *
     * @param[in] aNcp  The ncp controller.
     *
     */
    explicit HidlAgent(otbr::Ncp::ControllerOpenThread *aNcp);

    /**
     * This method performs initialization for the HIDL services.
     *
     */
    void Init(void);

    /**
     * This method updates the file descriptor sets and timeout for mainloop.
     *
     * @param[inout] aMainloop  A reference to OpenThread mainloop context.
     *
     */
    void UpdateFdSet(otSysMainloopContext &aMainloop) const;

    /**
     * This method performs processing.
     *
     * @param[in] aMainloop  A reference to OpenThread mainloop context.
     *
     */
    void Process(const otSysMainloopContext &aMainloop);

    /**
     * This method returns a reference to HidlSettings object.
     *
     * @retval  A reference to HidlSettings object.
     *
     */
    otbr::Hidl::HidlSettings &GetSettings(void) { return mSettings; }

    /**
     * This method returns a reference to HidlMdns object.
     *
     * @retval  A reference to HidlSettings object.
     *
     */
    otbr::Hidl::HidlMdns &GetMdns(void) { return mMdns; }

private:
    int                      mHidlFd;
    otbr::Hidl::HidlThread   mThread;
    otbr::Hidl::HidlSettings mSettings;
};
} // namespace Hidl
} // namespace otbr
