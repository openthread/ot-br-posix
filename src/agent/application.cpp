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

#ifndef OTBR_MAINLOOP_POLL_TIMEOUT_SEC
#define OTBR_MAINLOOP_POLL_TIMEOUT_SEC 10
#endif

std::atomic_bool     Application::sShouldTerminate(false);
const struct timeval Application::kPollTimeout = {OTBR_MAINLOOP_POLL_TIMEOUT_SEC, 0};

Application::Application(Ncp::ThreadHost   &aHost,
                         const std::string &aInterfaceName,
                         const std::string &aBackboneInterfaceName,
                         const std::string &aRestListenAddress,
                         int                aRestListenPort)
    : mInterfaceName(aInterfaceName)
    , mBackboneInterfaceName(aBackboneInterfaceName.c_str())
    , mHost(aHost)
#if OTBR_ENABLE_MDNS
    , mPublisher(
          Mdns::Publisher::Create([this](Mdns::Publisher::State aState) { mMdnsStateSubject.UpdateState(aState); }))
#endif
#if OTBR_ENABLE_DBUS_SERVER && OTBR_ENABLE_BORDER_AGENT
    , mDBusAgent(MakeUnique<DBus::DBusAgent>(mHost, *mPublisher))
#endif
{
    if (mHost.GetCoprocessorType() == OT_COPROCESSOR_RCP)
    {
        CreateRcpMode(aRestListenAddress, aRestListenPort);
    }
}

void Application::Init(void)
{
    mHost.Init();

    switch (mHost.GetCoprocessorType())
    {
    case OT_COPROCESSOR_RCP:
        InitRcpMode();
        break;
    case OT_COPROCESSOR_NCP:
        InitNcpMode();
        break;
    default:
        DieNow("Unknown coprocessor type!");
        break;
    }

    otbrLogInfo("Co-processor version: %s", mHost.GetCoprocessorVersion());
}

void Application::Deinit(void)
{
    switch (mHost.GetCoprocessorType())
    {
    case OT_COPROCESSOR_RCP:
        DeinitRcpMode();
        break;
    case OT_COPROCESSOR_NCP:
        DeinitNcpMode();
        break;
    default:
        DieNow("Unknown coprocessor type!");
        break;
    }

    mHost.Deinit();
}

otbrError Application::Run(void)
{
    otbrError error = OTBR_ERROR_NONE;

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

            if (mErrorCondition)
            {
                error = mErrorCondition();
                if (error != OTBR_ERROR_NONE)
                {
                    break;
                }
            }
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

void Application::CreateRcpMode(const std::string &aRestListenAddress, int aRestListenPort)
{
    otbr::Ncp::RcpHost &rcpHost = static_cast<otbr::Ncp::RcpHost &>(mHost);
#if OTBR_ENABLE_BORDER_AGENT
    mBorderAgent = MakeUnique<BorderAgent>(rcpHost, *mPublisher);
#endif
#if OTBR_ENABLE_BACKBONE_ROUTER
    mBackboneAgent = MakeUnique<BackboneRouter::BackboneAgent>(rcpHost, mInterfaceName, mBackboneInterfaceName);
#endif
#if OTBR_ENABLE_SRP_ADVERTISING_PROXY
    mAdvertisingProxy = MakeUnique<AdvertisingProxy>(rcpHost, *mPublisher);
#endif
#if OTBR_ENABLE_DNSSD_DISCOVERY_PROXY
    mDiscoveryProxy = MakeUnique<Dnssd::DiscoveryProxy>(rcpHost, *mPublisher);
#endif
#if OTBR_ENABLE_TREL
    mTrelDnssd = MakeUnique<TrelDnssd::TrelDnssd>(rcpHost, *mPublisher);
#endif
#if OTBR_ENABLE_OPENWRT
    mUbusAgent = MakeUnique<ubus::UBusAgent>(rcpHost);
#endif
#if OTBR_ENABLE_REST_SERVER
    mRestWebServer = MakeUnique<rest::RestWebServer>(rcpHost, aRestListenAddress, aRestListenPort);
#endif
#if OTBR_ENABLE_VENDOR_SERVER
    mVendorServer = vendor::VendorServer::newInstance(*this);
#endif

    OT_UNUSED_VARIABLE(aRestListenAddress);
    OT_UNUSED_VARIABLE(aRestListenPort);
}

void Application::InitRcpMode(void)
{
#if OTBR_ENABLE_BORDER_AGENT
    mMdnsStateSubject.AddObserver(*mBorderAgent);
#endif
#if OTBR_ENABLE_SRP_ADVERTISING_PROXY
    mMdnsStateSubject.AddObserver(*mAdvertisingProxy);
#endif
#if OTBR_ENABLE_DNSSD_DISCOVERY_PROXY
    mMdnsStateSubject.AddObserver(*mDiscoveryProxy);
#endif
#if OTBR_ENABLE_TREL
    mMdnsStateSubject.AddObserver(*mTrelDnssd);
#endif

#if OTBR_ENABLE_MDNS
    mPublisher->Start();
#endif
#if OTBR_ENABLE_BORDER_AGENT
// This is for delaying publishing the MeshCoP service until the correct
// vendor name and OUI etc. are correctly set by BorderAgent::SetMeshCopServiceValues()
#if OTBR_STOP_BORDER_AGENT_ON_INIT
    mBorderAgent->SetEnabled(false);
#else
    mBorderAgent->SetEnabled(true);
#endif
#endif
#if OTBR_ENABLE_BACKBONE_ROUTER
    mBackboneAgent->Init();
#endif
#if OTBR_ENABLE_SRP_ADVERTISING_PROXY
    mAdvertisingProxy->SetEnabled(true);
#endif
#if OTBR_ENABLE_DNSSD_DISCOVERY_PROXY
    mDiscoveryProxy->SetEnabled(true);
#endif
#if OTBR_ENABLE_OPENWRT
    mUbusAgent->Init();
#endif
#if OTBR_ENABLE_REST_SERVER
    mRestWebServer->Init();
#endif
#if OTBR_ENABLE_DBUS_SERVER
    mDBusAgent->Init(*mBorderAgent);
#endif
#if OTBR_ENABLE_VENDOR_SERVER
    mVendorServer->Init();
#endif
}

void Application::DeinitRcpMode(void)
{
#if OTBR_ENABLE_SRP_ADVERTISING_PROXY
    mAdvertisingProxy->SetEnabled(false);
#endif
#if OTBR_ENABLE_DNSSD_DISCOVERY_PROXY
    mDiscoveryProxy->SetEnabled(false);
#endif
#if OTBR_ENABLE_BORDER_AGENT
    mBorderAgent->SetEnabled(false);
#endif
#if OTBR_ENABLE_MDNS
    mMdnsStateSubject.Clear();
    mPublisher->Stop();
#endif
}

void Application::InitNcpMode(void)
{
#if OTBR_ENABLE_DBUS_SERVER
    mDBusAgent->Init(*mBorderAgent);
#endif
}

void Application::DeinitNcpMode(void)
{
    /* empty */
}

} // namespace otbr
