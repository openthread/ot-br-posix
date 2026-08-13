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

#include <sys/socket.h>
#include <unistd.h>

#include <gtest/gtest.h>

#include "web/web-service/ot_client.hpp"
#include "web/web-service/wpan_service.hpp"

namespace otbr {
namespace Web {

class WebServiceTest : public testing::Test
{
protected:
    static void SetSocket(OpenThreadClient &aClient, int aSocket) { aClient.mSocket = aSocket; }

    static std::string EscapeOtCliEscapable(const std::string &aArg) { return WpanService::escapeOtCliEscapable(aArg); }
};

TEST_F(WebServiceTest, ExecuteRejectsLineBreaks)
{
    int sockets[2];

    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets), 0);

    {
        OpenThreadClient client("wpan0");

        SetSocket(client, sockets[0]);
        EXPECT_EQ(client.Execute("dataset networkname %s", "network\nreset"), nullptr);
        EXPECT_EQ(client.Execute("dataset networkname %s", "network\rreset"), nullptr);
        EXPECT_EQ(client.Execute("dataset networkname %s", "network\r\nreset"), nullptr);
        EXPECT_EQ(client.Execute("dataset networkname %s", "\nnetwork"), nullptr);
        EXPECT_EQ(client.Execute("dataset networkname %s", "network\n"), nullptr);
    }

    EXPECT_EQ(close(sockets[1]), 0);
}

TEST_F(WebServiceTest, EscapeOtCliEscapable)
{
    EXPECT_EQ(EscapeOtCliEscapable("alpha beta\tgamma\\delta\r\nepsilon"), "alpha\\ beta\\\tgamma\\\\delta\r\nepsilon");
}

} // namespace Web
} // namespace otbr
