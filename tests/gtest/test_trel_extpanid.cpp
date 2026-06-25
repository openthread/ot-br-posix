/*
 *    Copyright (c) 2026, The OpenThread Authors.
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

#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::ElementsAreArray;

#include "utils/trel_extpanid.hpp"

using otbr::Utils::IsTrelExcludedExtPanId;
using otbr::Utils::kTrelExtPanIdLength;
using otbr::Utils::ParseTrelExcludeExtPanId;
using otbr::Utils::ReadTrelExtPanIdFromTxtData;
using otbr::Utils::ShouldExcludeTrelPeer;
using otbr::Utils::TrelExtPanId;

namespace {

const TrelExtPanId kAppleExtPanId = {0xdb, 0x80, 0x67, 0x60, 0x15, 0xdb, 0x4b, 0x6e};
const TrelExtPanId kOtherExtPanId = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77};

TEST(TrelExtPanId, ParseCompactLowercase)
{
    TrelExtPanId extPanId{};

    EXPECT_TRUE(ParseTrelExcludeExtPanId("db80676015db4b6e", extPanId));
    EXPECT_THAT(extPanId, ElementsAreArray(kAppleExtPanId));
}

TEST(TrelExtPanId, ParseCompactUppercase)
{
    TrelExtPanId extPanId{};

    EXPECT_TRUE(ParseTrelExcludeExtPanId("DB80676015DB4B6E", extPanId));
    EXPECT_THAT(extPanId, ElementsAreArray(kAppleExtPanId));
}

TEST(TrelExtPanId, ParseColonSeparated)
{
    TrelExtPanId extPanId{};

    EXPECT_TRUE(ParseTrelExcludeExtPanId("db:80:67:60:15:db:4b:6e", extPanId));
    EXPECT_THAT(extPanId, ElementsAreArray(kAppleExtPanId));
}

TEST(TrelExtPanId, ParseHyphenSeparated)
{
    TrelExtPanId extPanId{};

    EXPECT_TRUE(ParseTrelExcludeExtPanId("db-80-67-60-15-db-4b-6e", extPanId));
    EXPECT_THAT(extPanId, ElementsAreArray(kAppleExtPanId));
}

TEST(TrelExtPanId, RejectInvalidHex)
{
    TrelExtPanId extPanId{};

    EXPECT_FALSE(ParseTrelExcludeExtPanId("nothex", extPanId));
}

TEST(TrelExtPanId, RejectWrongLength)
{
    TrelExtPanId extPanId{};

    EXPECT_FALSE(ParseTrelExcludeExtPanId("001122334455667", extPanId));
    EXPECT_FALSE(ParseTrelExcludeExtPanId("001122334455667788", extPanId));
}

TEST(TrelExtPanId, MatchExcludedPeer)
{
    const std::vector<TrelExtPanId> excluded = {kAppleExtPanId};

    EXPECT_TRUE(IsTrelExcludedExtPanId(kAppleExtPanId, excluded));
    EXPECT_FALSE(IsTrelExcludedExtPanId(kOtherExtPanId, excluded));
}

TEST(TrelExtPanId, MatchMultipleExcludedPeers)
{
    const std::vector<TrelExtPanId> excluded = {kAppleExtPanId, kOtherExtPanId};

    EXPECT_TRUE(IsTrelExcludedExtPanId(kOtherExtPanId, excluded));
    EXPECT_FALSE(IsTrelExcludedExtPanId(TrelExtPanId{}, excluded));
}

TEST(TrelExtPanId, ShouldExcludePeerWithoutExtPanId)
{
    const std::vector<TrelExtPanId> excluded = {kAppleExtPanId};

    EXPECT_FALSE(ShouldExcludeTrelPeer(false, kAppleExtPanId, excluded));
}

TEST(TrelExtPanId, ShouldExcludePeerWithMatchingExtPanId)
{
    const std::vector<TrelExtPanId> excluded = {kAppleExtPanId};

    EXPECT_TRUE(ShouldExcludeTrelPeer(true, kAppleExtPanId, excluded));
    EXPECT_FALSE(ShouldExcludeTrelPeer(true, kOtherExtPanId, excluded));
}

TEST(TrelExtPanId, ShouldExcludePeerWithEmptyExclusionList)
{
    const std::vector<TrelExtPanId> excluded = {};

    EXPECT_FALSE(ShouldExcludeTrelPeer(true, kAppleExtPanId, excluded));
}

static std::vector<uint8_t> BuildTrelTxtData(const TrelExtPanId *aExtPanId)
{
    std::vector<uint8_t> txtData;
    const uint8_t        xa[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};

    txtData.push_back(static_cast<uint8_t>(2 + 1 + sizeof(xa)));
    txtData.insert(txtData.end(), {'x', 'a', '='});
    txtData.insert(txtData.end(), xa, xa + sizeof(xa));

    if (aExtPanId != nullptr)
    {
        txtData.push_back(static_cast<uint8_t>(2 + 1 + kTrelExtPanIdLength));
        txtData.insert(txtData.end(), {'x', 'p', '='});
        txtData.insert(txtData.end(), aExtPanId->begin(), aExtPanId->end());
    }

    return txtData;
}

TEST(TrelExtPanId, ReadExtPanIdFromTxtData)
{
    const std::vector<uint8_t> txtData = BuildTrelTxtData(&kAppleExtPanId);
    TrelExtPanId               extPanId{};
    bool                       hasExtPanId = false;

    EXPECT_TRUE(
        ReadTrelExtPanIdFromTxtData(txtData.data(), static_cast<uint16_t>(txtData.size()), extPanId, hasExtPanId));
    EXPECT_TRUE(hasExtPanId);
    EXPECT_THAT(extPanId, ElementsAreArray(kAppleExtPanId));
}

TEST(TrelExtPanId, ReadExtPanIdFromTxtDataWithoutXp)
{
    const std::vector<uint8_t> txtData = BuildTrelTxtData(nullptr);
    TrelExtPanId               extPanId{};
    bool                       hasExtPanId = false;

    EXPECT_TRUE(
        ReadTrelExtPanIdFromTxtData(txtData.data(), static_cast<uint16_t>(txtData.size()), extPanId, hasExtPanId));
    EXPECT_FALSE(hasExtPanId);
}

TEST(TrelExtPanId, ReadExtPanIdFromTxtDataRejectsInvalidXpLength)
{
    std::vector<uint8_t> txtData = BuildTrelTxtData(nullptr);

    txtData.push_back(static_cast<uint8_t>(2 + 1 + 4));
    txtData.insert(txtData.end(), {'x', 'p', '='});
    txtData.insert(txtData.end(), {0x01, 0x02, 0x03, 0x04});

    TrelExtPanId extPanId{};
    bool         hasExtPanId = false;

    EXPECT_FALSE(
        ReadTrelExtPanIdFromTxtData(txtData.data(), static_cast<uint16_t>(txtData.size()), extPanId, hasExtPanId));
    EXPECT_FALSE(hasExtPanId);
}

} // namespace
