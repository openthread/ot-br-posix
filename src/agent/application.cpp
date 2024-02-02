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

#ifdef HAVE_LIBSYSTEMD
#include <systemd/sd-daemon.h>
#endif

#include "agent/application.hpp"
#include "common/code_utils.hpp"
#include "common/mainloop_manager.hpp"
#include "utils/infra_link_selector.hpp"

namespace otbr {

std::atomic_bool     Application::sShouldTerminate(false);
const struct timeval Application::kPollTimeout = {10, 0};

Application::Application(const std::string               &aInterfaceName,
                         const std::vector<const char *> &aBackboneInterfaceNames,
                         const std::vector<const char *> &aRadioUrls,
                         bool                             aEnableAutoAttach,
                         const std::string               &aRestListenAddress,
                         int                              aRestListenPort)
    : mInterfaceName(aInterfaceName)
#if __linux__
    , mInfraLinkSelector(aBackboneInterfaceNames)
    , mBackboneInterfaceName(mInfraLinkSelector.Select())
#else
    , mBackboneInterfaceName(aBackboneInterfaceNames.empty() ? "" : aBackboneInterfaceNames.front())
#endif
    , mNcp(mInterfaceName.c_str(), aRadioUrls, mBackboneInterfaceName, /* aDryRun */ false, aEnableAutoAttach)
#if OTBR_ENABLE_MDNS
    , mPublisher(Mdns::Publisher::Create([this](Mdns::Publisher::State aState) { this->HandleMdnsState(aState); }))
#endif
#if OTBR_ENABLE_BORDER_AGENT
    , mBorderAgent(mNcp, *mPublisher)
#endif
#if OTBR_ENABLE_BACKBONE_ROUTER
    , mBackboneAgent(mNcp, aInterfaceName, mBackboneInterfaceName)
#endif
#if OTBR_ENABLE_SRP_ADVERTISING_PROXY
    , mAdvertisingProxy(mNcp, *mPublisher)
#endif
#if OTBR_ENABLE_DNSSD_DISCOVERY_PROXY
    , mDiscoveryProxy(mNcp, *mPublisher)
#endif
#if OTBR_ENABLE_TREL
    , mTrelDnssd(mNcp, *mPublisher)
#endif
#if OTBR_ENABLE_OPENWRT
    , mUbusAgent(mNcp)
#endif
#if OTBR_ENABLE_REST_SERVER
    , mRestWebServer(mNcp, aRestListenAddress, aRestListenPort)
#endif
#if OTBR_ENABLE_DBUS_SERVER && OTBR_ENABLE_BORDER_AGENT
    , mDBusAgent(mNcp, *mPublisher)
#endif
#if OTBR_ENABLE_VENDOR_SERVER
    , mVendorServer(vendor::VendorServer::newInstance(*this))
#endif
{
    OTBR_UNUSED_VARIABLE(aRestListenAddress);
    OTBR_UNUSED_VARIABLE(aRestListenPort);
}

void Application::Init(void)
{
    mNcp.Init();

#if OTBR_ENABLE_MDNS
    mPublisher->Start();
#endif
#if OTBR_ENABLE_BORDER_AGENT
    mBorderAgent.SetEnabled(true);
#endif
#if OTBR_ENABLE_BACKBONE_ROUTER
    mBackboneAgent.Init();
#endif
#if OTBR_ENABLE_SRP_ADVERTISING_PROXY
    mAdvertisingProxy.SetEnabled(true);
#endif
#if OTBR_ENABLE_DNSSD_DISCOVERY_PROXY
    mDiscoveryProxy.SetEnabled(true);
#endif
#if OTBR_ENABLE_OPENWRT
    mUbusAgent.Init();
#endif
#if OTBR_ENABLE_REST_SERVER
    mRestWebServer.Init();
#endif
#if OTBR_ENABLE_DBUS_SERVER
    mDBusAgent.Init();
#endif
#if OTBR_ENABLE_VENDOR_SERVER
    mVendorServer->Init();
#endif
}

void Application::Deinit(void)
{
#if OTBR_ENABLE_SRP_ADVERTISING_PROXY
    mAdvertisingProxy.SetEnabled(false);
#endif
#if OTBR_ENABLE_DNSSD_DISCOVERY_PROXY
    mDiscoveryProxy.SetEnabled(false);
#endif
#if OTBR_ENABLE_BORDER_AGENT
    mBorderAgent.SetEnabled(false);
#endif
#if OTBR_ENABLE_MDNS
    mPublisher->Stop();
#endif

    mNcp.Deinit();
}

otbrError Application::Run(void)
{
    otbrError error = OTBR_ERROR_NONE;

    otbrLogInfo("Thread Border Router started on AIL %s.", mBackboneInterfaceName);

#ifdef HAVE_LIBSYSTEMD
    if (getenv("SYSTEMD_EXEC_PID") != nullptr)
    {
        otbrLogInfo("Notify systemd the service is ready.");

        // Ignored return value as systemd recommends.
        // See https://www.freedesktop.org/software/systemd/man/sd_notify.html
        sd_notify(0, "READY=1");
    }
#endif

#if OTBR_ENABLE_NOTIFY_UPSTART
    if (getenv("UPSTART_JOB") != nullptr)
    {
        otbrLogInfo("Notify Upstart the service is ready.");
        if (raise(SIGSTOP))
        {
            otbrLogWarning("Failed to notify Upstart.");
        }
    }
#endif

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

#if __linux__
            {
                const char *newInfraLink = mInfraLinkSelector.Select();

                if (mBackboneInterfaceName != newInfraLink)
                {
                    error = OTBR_ERROR_INFRA_LINK_CHANGED;
                    break;
                }
            }
#endif
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

void Application::HandleMdnsState(Mdns::Publisher::State aState)
{
    OTBR_UNUSED_VARIABLE(aState);

#if OTBR_ENABLE_BORDER_AGENT
    mBorderAgent.HandleMdnsState(aState);
#endif
#if OTBR_ENABLE_SRP_ADVERTISING_PROXY
    mAdvertisingProxy.HandleMdnsState(aState);
#endif
#if OTBR_ENABLE_DNSSD_DISCOVERY_PROXY
    mDiscoveryProxy.HandleMdnsState(aState);
#endif
#if OTBR_ENABLE_TREL
    mTrelDnssd.HandleMdnsState(aState);
#endif
}

void Application::HandleSignal(int aSignal)
{
    sShouldTerminate = true;
    signal(aSignal, SIG_DFL);
}

} // namespace otbr
