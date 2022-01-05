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

/**
 * @file
 *   The file implements the OTBR Agent.
 */

#define OTBR_LOG_TAG "APP"

#include "agent/application.hpp"
#include "common/code_utils.hpp"
#include "common/mainloop_manager.hpp"

namespace otbr {

bool                 Application::sShouldTerminate = false;
const struct timeval Application::kPollTimeout     = {10, 0};

Application::Application(Ncp::ControllerOpenThread &aOpenThread)
    : mPlaceHolder(aOpenThread)
#if OTBR_ENABLE_BORDER_AGENT
    , mBorderAgent(aOpenThread)
#endif
#if OTBR_ENABLE_BACKBONE_ROUTER
    , mBackboneAgent(aOpenThread)
#endif
#if OTBR_ENABLE_OPENWRT
    , mUbusAgent(aOpenThread)
#endif
#if OTBR_ENABLE_REST_SERVER
    , mRestWebServer(aOpenThread)
#endif
#if OTBR_ENABLE_DBUS_SERVER
    , mDBusAgent(aOpenThread)
#endif
#if OTBR_ENABLE_VENDOR_SERVER
    , mVendorServer(aOpenThread)
#endif
{
    OTBR_UNUSED_VARIABLE(mPlaceHolder);
}

void Application::Init(void)
{
#if OTBR_ENABLE_BORDER_AGENT
    mBorderAgent.Init();
#endif
#if OTBR_ENABLE_BACKBONE_ROUTER
    mBackboneAgent.Init();
#endif
#if OTBR_ENABLE_OPENWRT
    mUbusAgent.Init();
#endif
#if OTBR_ENABLE_REST_SERVER
    mRestWebServer.Init();
#endif
#if OTBR_ENABLE_DBUS_SERVER
    SuccessOrDie(mDBusAgent.Init(), "Failed to initialize DBus Agent");
#endif
#if OTBR_ENABLE_VENDOR_SERVER
    mVendorServer.Init();
#endif
}

otbrError Application::Run(void)
{
    otbrError error = OTBR_ERROR_NONE;

    otbrLogInfo("Border router agent started.");
    // allow quitting elegantly
    signal(SIGTERM, HandleSignal);

    while (!sShouldTerminate)
    {
        otbr::MainloopContext mainloop;
        int                   rval;

        mainloop.mMaxFd   = -1;
        mainloop.mTimeout = kPollTimeout;

        FD_ZERO(&mainloop.mReadFdSet);
        FD_ZERO(&mainloop.mWriteFdSet);
        FD_ZERO(&mainloop.mErrorFdSet);

        MainloopManager::GetInstance().Update(mainloop);

        rval = select(mainloop.mMaxFd + 1, &mainloop.mReadFdSet, &mainloop.mWriteFdSet, &mainloop.mErrorFdSet,
                      &mainloop.mTimeout);

        if (rval >= 0)
        {
            MainloopManager::GetInstance().Process(mainloop);
        }
        else if (errno != EINTR)
        {
            error = OTBR_ERROR_ERRNO;
            otbrLogErr("select() failed: %s", strerror(errno));
            break;
        }
    }

    return error;
}

void Application::HandleSignal(int aSignal)
{
    sShouldTerminate = true;
    signal(aSignal, SIG_DFL);
}

} // namespace otbr
