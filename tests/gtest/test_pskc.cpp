/*
 *    Copyright (c) 2017, The OpenThread Authors.
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

#include "utils/pskc.hpp"

TEST(Pskc, Test123456_0001020304050607_OpenThread)
{
    otbr::Psk::Pskc pskc;

    uint8_t extpanid[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
    uint8_t expected[] = {
        0xb7, 0x83, 0x81, 0x27, 0x89, 0x91, 0x1e, 0xb4, 0xea, 0x76, 0x59, 0x6c, 0x9c, 0xed, 0x2a, 0x69,
    };

    const uint8_t *actual = pskc.ComputePskc(extpanid, "OpenThread", "123456");
    EXPECT_THAT(std::vector<uint8_t>(actual, actual + OT_PSKC_LENGTH), ElementsAreArray(expected));
}

TEST(Pskc, Test_TruncatedNetworkNamePskc_OpenThread)
{
    otbr::Psk::Pskc pskc;
    uint8_t         extpanid[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
    uint8_t         expected[OT_PSKC_LENGTH];

    // First run with shorter network name (max)
    const uint8_t *actual = pskc.ComputePskc(extpanid, "OpenThread123456", "123456");
    memcpy(expected, actual, OT_PSKC_LENGTH);

    // Second run with longer network name that gets truncated
    actual = pskc.ComputePskc(extpanid, "OpenThread123456NetworkNameThatExceedsBuffer", "123456");

    EXPECT_THAT(std::vector<uint8_t>(actual, actual + OT_PSKC_LENGTH), ElementsAreArray(expected));
}
