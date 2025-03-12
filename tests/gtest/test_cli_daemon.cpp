/*
 *    Copyright (c) 2025, The OpenThread Authors.
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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstring>
#include <ifaddrs.h>
#include <iostream>
#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <vector>

#include "common/code_utils.hpp"
#include "common/mainloop.hpp"
#include "common/types.hpp"
#include "host/posix/cli_daemon.hpp"
#include "utils/socket_utils.hpp"

// Only Test on linux platform for now.
#ifdef __linux__

using otbr::CliDaemon;

TEST(CliDaemon, InitSocketCreationWithFullNetIfName)
{
    const char *netIfName  = "tun0";
    const char *socketFile = "/run/openthread-tun0.sock";
    const char *lockFile   = "/run/openthread-tun0.lock";

    CliDaemon::Dependencies sDefaultCliDaemonDependencies;
    CliDaemon               cliDaemon(sDefaultCliDaemonDependencies);

    cliDaemon.Init(netIfName);

    struct stat st;

    EXPECT_EQ(stat(socketFile, &st), 0);
    EXPECT_EQ(stat(lockFile, &st), 0);
}

TEST(CliDaemon, InitSocketCreationWithEmptyNetIfName)
{
    const char *socketFile = "/run/openthread-wpan0.sock";
    const char *lockFile   = "/run/openthread-wpan0.lock";

    CliDaemon::Dependencies sDefaultCliDaemonDependencies;
    CliDaemon               cliDaemon(sDefaultCliDaemonDependencies);
    cliDaemon.Init("");

    struct stat st;

    EXPECT_EQ(stat(socketFile, &st), 0);
    EXPECT_EQ(stat(lockFile, &st), 0);
}

class CliDaemonTest : public otbr::CliDaemon::Dependencies
{
public:
    CliDaemonTest(bool &aReceived, std::string &aReceivedCommand)
        : mReceived(aReceived)
        , mReceivedCommand(aReceivedCommand)
    {
    }

    otbrError InputCommandLine(const uint8_t *aData, uint16_t aLength) override
    {
        mReceivedCommand = std::string(reinterpret_cast<const char *>(aData), aLength);
        mReceived        = true;
        return OTBR_ERROR_NONE;
    }

    bool        &mReceived;
    std::string &mReceivedCommand;
};

TEST(CliDaemon, InputCommandLineCorrectly_AfterReveivingOnSessionSocket)
{
    bool          received = false;
    std::string   receivedCommand;
    CliDaemonTest cliDependency(received, receivedCommand);

    const char *command    = "test command";
    const char *netIfName  = "tun0";
    const char *socketFile = "/run/openthread-tun0.sock";

    CliDaemon cliDaemon(cliDependency);
    EXPECT_EQ(cliDaemon.Init(netIfName), OT_ERROR_NONE);

    {
        int                clientSocket;
        struct sockaddr_un serverAddr;

        clientSocket = socket(AF_UNIX, SOCK_STREAM, 0);
        ASSERT_GE(clientSocket, 0) << "socket creation failed: " << strerror(errno);

        memset(&serverAddr, 0, sizeof(serverAddr));
        serverAddr.sun_family = AF_UNIX;
        strncpy(serverAddr.sun_path, socketFile, sizeof(serverAddr.sun_path) - 1);
        ASSERT_EQ(connect(clientSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)), 0);

        int rval = send(clientSocket, command, strlen(command), 0);
        ASSERT_GE(rval, 0) << "Error sending command: " << strerror(errno);

        close(clientSocket);
    }

    otbr::MainloopContext context;
    while (!received)
    {
        context.mMaxFd   = -1;
        context.mTimeout = {100, 0};
        FD_ZERO(&context.mReadFdSet);
        FD_ZERO(&context.mWriteFdSet);
        FD_ZERO(&context.mErrorFdSet);

        cliDaemon.UpdateFdSet(context);
        int rval = select(context.mMaxFd + 1, &context.mReadFdSet, &context.mWriteFdSet, &context.mErrorFdSet,
                          &context.mTimeout);
        ASSERT_GE(rval, 0) << "select failed, error: " << strerror(errno);

        cliDaemon.Process(context);
    }

    EXPECT_STREQ(receivedCommand.c_str(), command);

    cliDaemon.Deinit();
}

#endif // __linux__
