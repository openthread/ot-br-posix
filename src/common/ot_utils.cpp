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

#include "common/ot_utils.hpp"

namespace otbr {

std::string DeviceRoleToString(otDeviceRole aRole)
{
    std::string roleName;

    switch (aRole)
    {
    case OT_DEVICE_ROLE_DISABLED:
        roleName = OTBR_ROLE_NAME_DISABLED;
        break;
    case OT_DEVICE_ROLE_DETACHED:
        roleName = OTBR_ROLE_NAME_DETACHED;
        break;
    case OT_DEVICE_ROLE_CHILD:
        roleName = OTBR_ROLE_NAME_CHILD;
        break;
    case OT_DEVICE_ROLE_ROUTER:
        roleName = OTBR_ROLE_NAME_ROUTER;
        break;
    case OT_DEVICE_ROLE_LEADER:
        roleName = OTBR_ROLE_NAME_LEADER;
        break;
    }

    return roleName;
}

uint64_t ArrayToUint64(const uint8_t *aValue)
{
    uint64_t val = 0;

    for (size_t i = 0; i < sizeof(uint64_t); i++)
    {
        val = (val << 8) | aValue[i];
    }

    return val;
}

otExtendedPanId Uint64ToOtExtendedPanId(uint64_t aExtPanId)
{
    otExtendedPanId extPanId;
    uint64_t        mask = UINT8_MAX;

    for (size_t i = 0; i < sizeof(uint64_t); i++)
    {
        extPanId.m8[i] = static_cast<uint8_t>((aExtPanId >> ((sizeof(uint64_t) - i - 1) * 8)) & mask);
    }

    return extPanId;
}
} // namespace otbr
