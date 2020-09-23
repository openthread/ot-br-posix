/*
 *    Copyright (c) 2020, The OpenThread Authors.
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

#include "region_code.hpp"

#include <string.h>
#include <utility>

#include "common/logging.hpp"

namespace {

enum : uint32_t
{
    kChannelMask11To24 = 0x1fff800,
    kChannelMask11To25 = 0x3fff800,
    kChannelMask11To26 = 0x7fff800,
};
}

namespace otbr {

static const std::pair<RegionCode, const char *> sRegionCodeNames[] = {
    {kRegionWW, "WW"},
    {kRegionCA, "CA"},
    {kRegionUS, "US"},
};

RegionCode StringToRegionCode(const char *aRegionString)
{
    RegionCode code  = kRegionUnknown;
    bool       found = false;

    for (const auto &p : sRegionCodeNames)
    {
        if (!strcmp(aRegionString, p.second))
        {
            code  = p.first;
            found = true;
        }
    }

    if (!found)
    {
        otbrLog(OTBR_LOG_WARNING, "Unknown region %s", aRegionString);
    }
    return code;
}

const char *RegionCodeToString(RegionCode aRegionCode)
{
    const char *name  = "Unknown";
    bool        found = false;

    for (const auto &p : sRegionCodeNames)
    {
        if (aRegionCode == p.first)
        {
            name  = p.second;
            found = true;
        }
    }

    if (!found)
    {
        otbrLog(OTBR_LOG_WARNING, "Unknown region code %d", static_cast<int>(aRegionCode));
    }
    return name;
}

uint32_t GetSupportedChannelMaskForRegion(RegionCode aRegionCode)
{
    uint32_t mask;

    switch (aRegionCode)
    {
    case kRegionCA:
    case kRegionUS:
        mask = kChannelMask11To25;
        break;
    default:
        mask = kChannelMask11To26;
        break;
    }

    return mask;
}

uint32_t GetPreferredChannelMaskForRegion(RegionCode aRegionCode)
{
    uint32_t mask;

    switch (aRegionCode)
    {
    case kRegionCA:
    case kRegionUS:
        mask = kChannelMask11To24;
        break;
    default:
        mask = kChannelMask11To26;
        break;
    }

    return mask;
}

} // namespace otbr
