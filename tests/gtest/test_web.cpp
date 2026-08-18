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
    static std::string ToLower(const std::string &aStr) { return WpanService::toLower(aStr); }

    static int StatusUninitialized(void) { return WpanService::kWpanStatus_Uninitialized; }
    static int StatusParseRequestFailed(void) { return WpanService::kWpanStatus_ParseRequestFailed; }
};

namespace {

// Parses a handler's JSON response and hands back the root, so each test can assert on whatever
// fields matter to it.
Json::Value ParseJson(const std::string &aJson)
{
    Json::Value                       root;
    Json::CharReaderBuilder           builder;
    std::string                       errs;
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());

    EXPECT_TRUE(reader->parse(aJson.c_str(), aJson.c_str() + aJson.size(), &root, &errs)) << errs;
    return root;
}

} // namespace

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

TEST_F(WebServiceTest, ToLower)
{
    EXPECT_EQ(ToLower("STARTED"), "started");
    EXPECT_EQ(ToLower("Connected"), "connected");
    EXPECT_EQ(ToLower(""), "");
    EXPECT_EQ(ToLower("already-lower"), "already-lower");
    // Guards the `static_cast<unsigned char>` in the std::transform lambda: a signed `char` with the
    // high bit set is UB for plain `std::tolower()`, so this must not crash under ASan/UBSan.
    EXPECT_NO_THROW(ToLower("\xff\x80mix3D"));
}

// HandleGetEpskcStatusRequest / HandleDeactivateEpskcRequest take no body, so with no daemon
// listening on the (bogus) interface's socket, Connect() deterministically fails and we can assert
// on the resulting error JSON without touching a real socket.

TEST_F(WebServiceTest, HandleGetEpskcStatusRequestNoDaemon)
{
    WpanService service;

    service.SetInterfaceName("otbr-test-no-daemon");

    Json::Value root = ParseJson(service.HandleGetEpskcStatusRequest());

    EXPECT_EQ(root["result"].asString(), "failed");
    EXPECT_EQ(root["error"].asInt(), StatusUninitialized());
    EXPECT_FALSE(root.isMember("enabled"));
    EXPECT_FALSE(root.isMember("state"));
}

TEST_F(WebServiceTest, HandleDeactivateEpskcRequestNoDaemon)
{
    WpanService service;

    service.SetInterfaceName("otbr-test-no-daemon");

    Json::Value root = ParseJson(service.HandleDeactivateEpskcRequest());

    EXPECT_EQ(root["result"].asString(), "failed");
    EXPECT_EQ(root["error"].asInt(), StatusUninitialized());
}

// HandleSetEpskcEnabledRequest validates its body *before* connecting, so these never touch a
// socket -- StatusParseRequestFailed (rather than just "some failure") proves that.

TEST_F(WebServiceTest, HandleSetEpskcEnabledRequestMalformedJson)
{
    WpanService service;

    service.SetInterfaceName("otbr-test-no-daemon");

    Json::Value root = ParseJson(service.HandleSetEpskcEnabledRequest("not json"));
    EXPECT_EQ(root["error"].asInt(), StatusParseRequestFailed());
}

TEST_F(WebServiceTest, HandleSetEpskcEnabledRequestMissingField)
{
    WpanService service;

    service.SetInterfaceName("otbr-test-no-daemon");

    Json::Value root = ParseJson(service.HandleSetEpskcEnabledRequest("{}"));
    EXPECT_EQ(root["error"].asInt(), StatusParseRequestFailed());
}

TEST_F(WebServiceTest, HandleSetEpskcEnabledRequestWrongType)
{
    WpanService service;

    service.SetInterfaceName("otbr-test-no-daemon");

    // Regression test for the maintainer-requested fix: a string "true" must be rejected, not
    // silently truthy-coerced.
    Json::Value root = ParseJson(service.HandleSetEpskcEnabledRequest(R"({"enabled":"true"})"));
    EXPECT_EQ(root["error"].asInt(), StatusParseRequestFailed());
}

TEST_F(WebServiceTest, HandleSetEpskcEnabledRequestValidBodyNoDaemon)
{
    WpanService service;

    service.SetInterfaceName("otbr-test-no-daemon");

    // Body parses fine; the only failure left is Connect().
    Json::Value root = ParseJson(service.HandleSetEpskcEnabledRequest(R"({"enabled":true})"));
    EXPECT_EQ(root["error"].asInt(), StatusUninitialized());
}

// HandleActivateEpskcRequest: "lifetime"/"port" are optional and validated before connecting.

TEST_F(WebServiceTest, HandleActivateEpskcRequestEmptyBodyNoDaemon)
{
    WpanService service;

    service.SetInterfaceName("otbr-test-no-daemon");

    // Empty body must be accepted (both fields optional) and fall through to Connect(), not to a
    // parse error -- this was the specific fix requested in review.
    Json::Value root = ParseJson(service.HandleActivateEpskcRequest(""));
    EXPECT_EQ(root["error"].asInt(), StatusUninitialized());
}

TEST_F(WebServiceTest, HandleActivateEpskcRequestMalformedJson)
{
    WpanService service;

    service.SetInterfaceName("otbr-test-no-daemon");

    Json::Value root = ParseJson(service.HandleActivateEpskcRequest("{not json"));
    EXPECT_EQ(root["error"].asInt(), StatusParseRequestFailed());
}

TEST_F(WebServiceTest, HandleActivateEpskcRequestLifetimeWrongType)
{
    WpanService service;

    service.SetInterfaceName("otbr-test-no-daemon");

    Json::Value root = ParseJson(service.HandleActivateEpskcRequest(R"({"lifetime":"soon"})"));
    EXPECT_EQ(root["error"].asInt(), StatusParseRequestFailed());
}

TEST_F(WebServiceTest, HandleActivateEpskcRequestPortWrongType)
{
    WpanService service;

    service.SetInterfaceName("otbr-test-no-daemon");

    Json::Value root = ParseJson(service.HandleActivateEpskcRequest(R"({"port":-1})"));
    EXPECT_EQ(root["error"].asInt(), StatusParseRequestFailed());
}

TEST_F(WebServiceTest, HandleActivateEpskcRequestValidFieldsNoDaemon)
{
    WpanService service;

    service.SetInterfaceName("otbr-test-no-daemon");

    Json::Value root = ParseJson(service.HandleActivateEpskcRequest(R"({"lifetime":30000,"port":49155})"));
    EXPECT_EQ(root["error"].asInt(), StatusUninitialized());
}

} // namespace Web
} // namespace otbr
