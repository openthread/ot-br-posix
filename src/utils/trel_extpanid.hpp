/*
 *  Copyright (c) 2026, The OpenThread Authors.
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
 *   Utilities for TREL Extended PAN ID exclusion.
 */

#ifndef OTBR_UTILS_TREL_EXTPANID_HPP_
#define OTBR_UTILS_TREL_EXTPANID_HPP_

#include "openthread-br/config.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace otbr {
namespace Utils {

constexpr size_t kTrelExtPanIdLength = 8;

using TrelExtPanId = std::array<uint8_t, kTrelExtPanIdLength>;

/**
 * Parses a user-supplied Extended PAN ID hex string.
 *
 * Accepts compact, colon-separated, and hyphen-separated hex input. The value
 * must decode to exactly eight bytes, parsed left to right.
 *
 * @param[in]  aStr  The input string.
 * @param[out] aOut  The parsed Extended PAN ID.
 *
 * @returns `true` when parsing succeeds; otherwise `false`.
 */
bool ParseTrelExcludeExtPanId(const char *aStr, TrelExtPanId &aOut);

/**
 * Determines whether @p aPeer matches any entry in @p aExcluded.
 *
 * @param[in] aPeer      The peer Extended PAN ID.
 * @param[in] aExcluded  The exclusion list.
 *
 * @returns `true` when @p aPeer is excluded; otherwise `false`.
 */
bool IsTrelExcludedExtPanId(const TrelExtPanId &aPeer, const std::vector<TrelExtPanId> &aExcluded);

/**
 * Determines whether a discovered TREL peer should be excluded.
 *
 * Peers without a valid Extended PAN ID in TXT are never excluded.
 *
 * @param[in] aHasExtPanId     Whether the peer TXT contained a valid `xp` value.
 * @param[in] aPeerExtPanId    The peer Extended PAN ID from TXT.
 * @param[in] aExcluded        The exclusion list.
 *
 * @returns `true` when the peer should be excluded; otherwise `false`.
 */
bool ShouldExcludeTrelPeer(bool                             aHasExtPanId,
                           const TrelExtPanId              &aPeerExtPanId,
                           const std::vector<TrelExtPanId> &aExcluded);

/**
 * Reads the Extended PAN ID from a TREL DNS-SD TXT record (`xp` key).
 *
 * @param[in]  aTxtData       A pointer to the TXT data.
 * @param[in]  aTxtLength     The TXT data length in bytes.
 * @param[out] aOut           The parsed Extended PAN ID.
 * @param[out] aHasExtPanId   Set to `true` when a valid `xp` value is present.
 *
 * @returns `true` when parsing succeeds; otherwise `false`.
 */
bool ReadTrelExtPanIdFromTxtData(const uint8_t *aTxtData, uint16_t aTxtLength, TrelExtPanId &aOut, bool &aHasExtPanId);

} // namespace Utils
} // namespace otbr

#endif // OTBR_UTILS_TREL_EXTPANID_HPP_
