/*
 *  Copyright (c) 2017, The OpenThread Authors.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 *   This file includes definitions for NCP service.
 */

#ifndef NCP_POSIX_HPP_
#define NCP_POSIX_HPP_

#include "ncp.hpp"

#if OTBR_ENABLE_NCP_OPENTHREAD

namespace ot {

namespace BorderRouter {

namespace Ncp {

/**
 * This interface defines NCP Controller functionality.
 *
 */
class ControllerOpenThread : public Controller
{
public:
    /**
     * This constructor initializes this object.
     *
     * @param[in]   aInterfaceName  A string of the NCP interface name.
     * @param[in]   aRadioFile      A string of the NCP device file, which can be serial device or executables.
     * @param[in]   aRadioConfig    A string of the NCP device parameters.
     *
     */
    ControllerOpenThread(const char *aInterfaceName, char *aRadioFile, char *aRadioConfig);

    /**
     * This method initalize the NCP controller.
     *
     * @retval  OTBR_ERROR_NONE     Successfully initialized NCP controller.
     *
     */
    virtual otbrError Init(void);

    /**
     * This method updates the fd_set to poll.
     *
     * @param[inout]    aMainloop   A reference to OpenThread mainloop context.
     *
     */
    virtual void UpdateFdSet(otSysMainloopContext &aMainloop);

    /**
     * This method performs the DTLS processing.
     *
     * @param[in]       aMainloop   A reference to OpenThread mainloop context.
     *
     */
    virtual void Process(const otSysMainloopContext &aMainloop);

    /**
     * This method request the event.
     *
     * @param[in]   aEvent  The event id to request.
     *
     * @retval  OTBR_ERROR_NONE         Successfully requested the event.
     * @retval  OTBR_ERROR_ERRNO        Failed to request the event.
     *
     */
    virtual otbrError RequestEvent(int aEvent);

    virtual ~ControllerOpenThread(void);

private:
    static void HandleStateChanged(otChangedFlags aFlags, void *aContext)
    {
        static_cast<ControllerOpenThread *>(aContext)->HandleStateChanged(aFlags);
    }
    void HandleStateChanged(otChangedFlags aFlags);

    otInstance *mInstance;
};

} // namespace Ncp

} // namespace BorderRouter

} // namespace ot

#endif // OTBR_ENABLE_NCP_OPENTHREAD

#endif // NCP_POSIX_HPP_
