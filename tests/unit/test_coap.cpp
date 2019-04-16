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

#include <CppUTest/TestHarness.h>

#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

#include "common/coap.hpp"

using namespace ot::BorderRouter;

struct TestContext
{
    Coap::Agent *mAgent;
    int          mSocket;
    sockaddr_in6 mSockName;
    bool         mRequestHandled;
    bool         mResponseHandled;
};

void TestRequestHandler(const Coap::Resource &aResource,
                        const Coap::Message & aRequest,
                        Coap::Message &       aResponse,
                        const uint8_t *       aIp6,
                        uint16_t              aPort,
                        void *                aContext)
{
    TestContext &context = *static_cast<TestContext *>(aContext);

    context.mRequestHandled = true;
    aResponse.SetCode(Coap::kCodeChanged);

    (void)aResource;
    (void)aRequest;
    (void)aIp6;
    (void)aPort;
    (void)aContext;
}

void TestResponseHandler(const Coap::Message &aMessage, void *aContext)
{
    TestContext &context = *static_cast<TestContext *>(aContext);

    context.mResponseHandled = true;
    (void)aMessage;
}

ssize_t TestNetworkSender(const uint8_t *aBuffer, uint16_t aLength, const uint8_t *aIp6, uint16_t aPort, void *aContext)
{
    TestContext &context = *static_cast<TestContext *>(aContext);

    CHECK_EQUAL(aLength, sendto(context.mSocket, aBuffer, aLength, 0,
                                reinterpret_cast<struct sockaddr *>(&context.mSockName), sizeof(context.mSockName)));

    (void)aIp6;
    (void)aPort;
    return static_cast<ssize_t>(aLength);
}

TEST_GROUP(Coap)
{
    Coap::Agent *agent;
};

TEST(Coap, TestAddRemoveResource)
{
    Coap::Resource resource("test/a", TestRequestHandler, NULL);
    agent = Coap::Agent::Create(NULL, NULL);

    CHECK_EQUAL(OTBR_ERROR_NONE, agent->AddResource(resource));

    // Adding the same resource twice is not allowed.
    CHECK_EQUAL(OTBR_ERROR_ERRNO, agent->AddResource(resource));
    CHECK_EQUAL(EEXIST, errno);

    CHECK_EQUAL(OTBR_ERROR_NONE, agent->RemoveResource(resource));

    // Removing non-exist resource should fail.
    CHECK_EQUAL(OTBR_ERROR_ERRNO, agent->RemoveResource(resource));
    CHECK_EQUAL(ENOENT, errno);

    Coap::Agent::Destroy(agent);
}

TEST(Coap, TestRequest)
{
    TestContext context;

    Coap::Resource resource("cool", TestRequestHandler, &context);
    uint16_t       token = htons(1);
    ot::Ip6Address addr(0);
    uint8_t        buffer[128];

    agent = Coap::Agent::Create(TestNetworkSender, &context);

    context.mSocket = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    CHECK(context.mSocket != -1);
    socklen_t sin6len = sizeof(context.mSockName);
    memset(&context.mSockName, 0, sizeof(context.mSockName));
    context.mSockName.sin6_family = AF_INET6;
    context.mSockName.sin6_addr   = in6addr_any;
    context.mSockName.sin6_port   = 0;
    CHECK_EQUAL(
        0, bind(context.mSocket, reinterpret_cast<struct sockaddr *>(&context.mSockName), sizeof(context.mSockName)));
    getsockname(context.mSocket, reinterpret_cast<struct sockaddr *>(&context.mSockName), &sin6len);

    context.mAgent           = agent;
    context.mRequestHandled  = false;
    context.mResponseHandled = false;

    CHECK_EQUAL(OTBR_ERROR_NONE, agent->AddResource(resource));
    Coap::Message *message = agent->NewMessage(Coap::kTypeConfirmable, Coap::kCodePost,
                                               reinterpret_cast<const uint8_t *>(&token), sizeof(token));
    message->SetPath("cool");

    agent->Send(*message, NULL, 0, TestResponseHandler, &context);

    agent->FreeMessage(message);

    // Process request
    {
        ssize_t count = recvfrom(context.mSocket, buffer, sizeof(buffer), 0, NULL, NULL);
        CHECK(count > 0 && count <= 0xffff);
        context.mAgent->Input(buffer, static_cast<uint16_t>(count), NULL, 0);
        CHECK_EQUAL(true, context.mRequestHandled);
        CHECK_EQUAL(false, context.mResponseHandled);
    }

    // Process reponse
    {
        ssize_t count = recvfrom(context.mSocket, buffer, sizeof(buffer), 0, NULL, NULL);
        CHECK(count > 0 && count <= 0xffff);
        context.mAgent->Input(buffer, static_cast<uint16_t>(count), NULL, 0);
        CHECK_EQUAL(true, context.mResponseHandled);
    }

    CHECK_EQUAL(0, close(context.mSocket));

    Coap::Agent::Destroy(agent);
}
