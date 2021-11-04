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
 *   This file includes implementation of mDNS publisher.
 */

#include "mdns/mdns.hpp"

#include "common/code_utils.hpp"

namespace otbr {

namespace Mdns {

bool Publisher::IsServiceTypeEqual(std::string aFirstType, std::string aSecondType)
{
    if (!aFirstType.empty() && aFirstType.back() == '.')
    {
        aFirstType.pop_back();
    }

    if (!aSecondType.empty() && aSecondType.back() == '.')
    {
        aSecondType.pop_back();
    }

    return aFirstType == aSecondType;
}

otbrError Publisher::EncodeTxtData(const TxtList &aTxtList, std::vector<uint8_t> &aTxtData)
{
    otbrError error = OTBR_ERROR_NONE;

    for (const auto &txtEntry : aTxtList)
    {
        const auto & name        = txtEntry.mName;
        const auto & value       = txtEntry.mValue;
        const size_t entryLength = name.length() + 1 + value.size();

        VerifyOrExit(entryLength <= kMaxTextEntrySize, error = OTBR_ERROR_INVALID_ARGS);

        aTxtData.push_back(static_cast<uint8_t>(entryLength));
        aTxtData.insert(aTxtData.end(), name.begin(), name.end());
        aTxtData.push_back('=');
        aTxtData.insert(aTxtData.end(), value.begin(), value.end());
    }

exit:
    return error;
}

} // namespace Mdns

} // namespace otbr
