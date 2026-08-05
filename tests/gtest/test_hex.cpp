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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "utils/hex.hpp"

TEST(Hex, Bytes2HexBuffer)
{
    const uint8_t bytes[] = {0x12, 0x34, 0xab, 0xcd, 0xef, 0x00, 0xff};
    char          hexBuf[2 * sizeof(bytes) + 1];

    EXPECT_EQ(otbr::Utils::Bytes2Hex(nullptr, sizeof(bytes), hexBuf), 0u);
    EXPECT_STREQ(hexBuf, "");

    EXPECT_EQ(otbr::Utils::Bytes2Hex(bytes, 0, hexBuf), 0u);
    EXPECT_STREQ(hexBuf, "");

    EXPECT_EQ(otbr::Utils::Bytes2Hex(bytes, sizeof(bytes), nullptr), 0u);

    size_t len = otbr::Utils::Bytes2Hex(bytes, sizeof(bytes), hexBuf);
    EXPECT_EQ(len, 14u);
    EXPECT_STREQ(hexBuf, "1234ABCDEF00FF");
}

TEST(Hex, Bytes2HexString)
{
    const uint8_t bytes[] = {0x12, 0x34, 0xab, 0xcd, 0xef, 0x00, 0xff};

    EXPECT_EQ(otbr::Utils::Bytes2Hex(nullptr, sizeof(bytes)), "");
    EXPECT_EQ(otbr::Utils::Bytes2Hex(bytes, 0), "");

    std::string hexStr = otbr::Utils::Bytes2Hex(bytes, sizeof(bytes));
    EXPECT_EQ(hexStr, "1234ABCDEF00FF");
}
