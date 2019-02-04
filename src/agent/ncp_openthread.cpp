/*
 *    Copyright (c) 2019, The OpenThread Authors.
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

#include "ncp_openthread.hpp"

#include <assert.h>
#include <stdio.h>

#include <openthread/cli.h>
#include <openthread/dataset.h>
#include <openthread/tasklet.h>
#include <openthread/thread.h>
#include <openthread/thread_ftd.h>
#include <openthread/platform/logging.h>

#include "common/types.hpp"

#if OTBR_ENABLE_NCP_OPENTHREAD
namespace ot {
namespace BorderRouter {
namespace Ncp {

ControllerOpenThread::ControllerOpenThread(const char *aInterfaceName, char *aRadioFile, char *aRadioConfig)
{
    (void)aInterfaceName;

    char *argv[] = {
        NULL,
        aRadioFile,
        aRadioConfig,
    };

    mInstance = otSysInit(sizeof(argv) / sizeof(argv[0]), argv);
}

ControllerOpenThread::~ControllerOpenThread(void)
{
    otInstanceFinalize(mInstance);
}

otbrError ControllerOpenThread::Init(void)
{
    otSysInitNetif(mInstance);
    otCliUartInit(mInstance);
    otSetStateChangedCallback(mInstance, &ControllerOpenThread::HandleStateChanged, this);

    return OTBR_ERROR_NONE;
}

void ControllerOpenThread::HandleStateChanged(otChangedFlags aFlags)
{
    if (aFlags | OT_CHANGED_THREAD_NETWORK_NAME)
    {
        EventEmitter::Emit(kEventNetworkName, otThreadGetNetworkName(mInstance));
    }

    if (aFlags | OT_CHANGED_THREAD_EXT_PANID)
    {
        EventEmitter::Emit(kEventExtPanId, otThreadGetExtendedPanId(mInstance));
    }

    if (aFlags | OT_CHANGED_THREAD_ROLE)
    {
        bool attached = false;

        switch (otThreadGetDeviceRole(mInstance))
        {
        case OT_DEVICE_ROLE_CHILD:
        case OT_DEVICE_ROLE_ROUTER:
        case OT_DEVICE_ROLE_LEADER:
            attached = true;
            break;
        default:
            break;
        }

        EventEmitter::Emit(kEventThreadState, attached);
    }
}

void ControllerOpenThread::UpdateFdSet(otSysMainloopContext &aMainloop)
{
    if (otTaskletsArePending(mInstance))
    {
        aMainloop.mTimeout.tv_sec  = 0;
        aMainloop.mTimeout.tv_usec = 0;
    }

    otSysMainloopUpdate(mInstance, &aMainloop);
}

void ControllerOpenThread::Process(const otSysMainloopContext &aMainloop)
{
    otTaskletsProcess(mInstance);

    otSysMainloopProcess(mInstance, &aMainloop);
}

otbrError ControllerOpenThread::RequestEvent(int aEvent)
{
    otbrError ret = OTBR_ERROR_ERRNO;

    switch (aEvent)
    {
    case kEventExtPanId:
    {
        EventEmitter::Emit(kEventExtPanId, otThreadGetExtendedPanId(mInstance));
        break;
    }
    case kEventThreadState:
    {
        bool attached = false;

        switch (otThreadGetDeviceRole(mInstance))
        {
        case OT_DEVICE_ROLE_CHILD:
        case OT_DEVICE_ROLE_ROUTER:
        case OT_DEVICE_ROLE_LEADER:
            attached = true;
            break;
        default:
            break;
        }

        EventEmitter::Emit(kEventThreadState, attached);
        break;
    }
    case kEventNetworkName:
    {
        EventEmitter::Emit(kEventNetworkName, otThreadGetNetworkName(mInstance));
        break;
    }
    case kEventPSKc:
    {
        EventEmitter::Emit(kEventPSKc, otThreadGetPSKc(mInstance));
        break;
    }
    default:
        assert(false);
        break;
    }

    return ret;
}

Controller *Controller::Create(const char *aInterfaceName, char *aRadioFile, char *aRadioConfig)
{
    return new ControllerOpenThread(aInterfaceName, aRadioFile, aRadioConfig);
}

/*
 * Provide, if required an "otPlatLog()" function
 */
extern "C" void otPlatLog(otLogLevel aLogLevel, otLogRegion aLogRegion, const char *aFormat, ...)
{
    OT_UNUSED_VARIABLE(aLogLevel);
    OT_UNUSED_VARIABLE(aLogRegion);
    OT_UNUSED_VARIABLE(aFormat);

    va_list ap;
    va_start(ap, aFormat);
    otCliPlatLogv(aLogLevel, aLogRegion, aFormat, ap);
    va_end(ap);
}

} // namespace Ncp
} // namespace BorderRouter
} // namespace ot

#endif // OTBR_ENABLE_NCP_OPENTHREAD
