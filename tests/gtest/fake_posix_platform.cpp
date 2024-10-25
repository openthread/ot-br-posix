/*
 *  Copyright (c) 2024, The OpenThread Authors.
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

#include "fake_platform.hpp"

#include <assert.h>

#include "openthread/openthread-system.h"

otPlatResetReason        gPlatResetReason = OT_PLAT_RESET_REASON_POWER_ON;
static ot::FakePlatform *sFakePlatform;

const otRadioSpinelMetrics *otSysGetRadioSpinelMetrics(void)
{
    return nullptr;
}
const otRcpInterfaceMetrics *otSysGetRcpInterfaceMetrics(void)
{
    return nullptr;
}

uint32_t otSysGetInfraNetifFlags(void)
{
    return 0;
}

void otSysCountInfraNetifAddresses(otSysInfraNetIfAddressCounters *)
{
}

const char *otSysGetInfraNetifName(void)
{
    return nullptr;
}

otInstance *otSysInit(otPlatformConfig *aPlatformConfig)
{
    OT_UNUSED_VARIABLE(aPlatformConfig);

    assert(sFakePlatform == nullptr);
    sFakePlatform = new ot::FakePlatform();
    return sFakePlatform->CurrentInstance();
}

void otSysDeinit(void)
{
    if (sFakePlatform != nullptr)
    {
        delete sFakePlatform;
        sFakePlatform = nullptr;
    }
}

void otSysMainloopUpdate(otInstance *, otSysMainloopContext *)
{
}

void otSysMainloopProcess(otInstance *, const otSysMainloopContext *)
{
    sFakePlatform->Run(/* microseconds */ 1000);
}
