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
#include <functional>
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

static constexpr size_t kCliMaxLineLength = OTBR_CONFIG_CLI_MAX_LINE_LENGTH;
static const char      *kTestOutput       = "sample output";
static const char       kTruncatedMsg[]   = "(truncated ...)";

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

class CliDaemonTestInput : public otbr::CliDaemon::Dependencies
{
public:
    CliDaemonTestInput(bool &aReceived, std::string &aReceivedCommand)
        : mReceived(aReceived)
        , mReceivedCommand(aReceivedCommand)
    {
    }

    otbrError InputCommandLine(const char *aLine) override
    {
        mReceivedCommand = std::string(aLine);
        mReceived        = true;
        return OTBR_ERROR_NONE;
    }

    bool        &mReceived;
    std::string &mReceivedCommand;
};

TEST(CliDaemon, InputCommandLineCorrectly_AfterReveivingOnSessionSocket)
{
    bool               received = false;
    std::string        receivedCommand;
    CliDaemonTestInput cliDependency(received, receivedCommand);

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

        cliDaemon.Update(context);
        int rval = select(context.mMaxFd + 1, &context.mReadFdSet, &context.mWriteFdSet, &context.mErrorFdSet,
                          &context.mTimeout);
        ASSERT_GE(rval, 0) << "select failed, error: " << strerror(errno);

        cliDaemon.Process(context);
    }

    EXPECT_STREQ(receivedCommand.c_str(), command);

    cliDaemon.Deinit();
}

class CliDaemonTestOutput : public otbr::CliDaemon::Dependencies
{
public:
    // Store a pointer to the CliDaemon to call HandleCommandOutput
    CliDaemon  *mCliDaemonInstance = nullptr;
    const char *mOutputToSend      = kTestOutput;

    otbrError InputCommandLine(const char *aLine) override
    {
        OTBR_UNUSED_VARIABLE(aLine);

        if (mCliDaemonInstance != nullptr)
        {
            mCliDaemonInstance->OutputFormat("%s", mOutputToSend);
        }
        return OTBR_ERROR_NONE;
    }
};

TEST(CliDaemon, HandleCommandOutputCorrectly_AfterReveivingOnSessionSocket)
{
    const char *command    = "test command\n";
    const char *netIfName  = "tun0";
    const char *socketFile = "/run/openthread-tun0.sock";

    CliDaemonTestOutput cliDependency;
    CliDaemon           cliDaemon(cliDependency);
    cliDependency.mCliDaemonInstance = &cliDaemon;

    otbr::MainloopContext context;

    EXPECT_EQ(cliDaemon.Init(netIfName), OT_ERROR_NONE);

    int clientSocket = -1;
    {
        struct sockaddr_un serverAddr;

        clientSocket = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
        ASSERT_GE(clientSocket, 0) << "socket creation failed: " << strerror(errno);

        memset(&serverAddr, 0, sizeof(serverAddr));
        serverAddr.sun_family = AF_UNIX;
        strncpy(serverAddr.sun_path, socketFile, sizeof(serverAddr.sun_path) - 1);
        ASSERT_EQ(connect(clientSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)), 0);

        int rval = send(clientSocket, command, strlen(command), 0);
        ASSERT_GE(rval, 0) << "Error sending command: " << strerror(errno);
    }

    char recvBuf[kCliMaxLineLength];
    bool outputReceived = false;

    while (!outputReceived)
    {
        context.mMaxFd   = -1;
        context.mTimeout = {100, 0};
        FD_ZERO(&context.mReadFdSet);
        FD_ZERO(&context.mWriteFdSet);
        FD_ZERO(&context.mErrorFdSet);

        cliDaemon.Update(context);

        context.AddFdToReadSet(clientSocket);

        int rval = select(context.mMaxFd + 1, &context.mReadFdSet, &context.mWriteFdSet, &context.mErrorFdSet,
                          &context.mTimeout);
        ASSERT_GE(rval, 0) << "select failed, error: " << strerror(errno);

        cliDaemon.Process(context);

        if (FD_ISSET(clientSocket, &context.mReadFdSet))
        {
            int rval = read(clientSocket, recvBuf, kCliMaxLineLength - 1);
            ASSERT_GE(rval, 0) << "Error receiving cli output: " << strerror(errno);

            recvBuf[rval]  = '\0';
            outputReceived = true;
        }
    }

    EXPECT_STREQ(recvBuf, kTestOutput);

    cliDaemon.Deinit();
}

TEST(CliDaemon, HandleCommandOutputTruncatedCorrectly_AfterReceivingOnSessionSocket)
{
    const char *command    = "test command\n";
    const char *netIfName  = "tun0";
    const char *socketFile = "/run/openthread-tun0.sock";

    std::string longTestOutput(kCliMaxLineLength + 50, 'A');

    CliDaemonTestOutput cliDependency;
    CliDaemon           cliDaemon(cliDependency);
    cliDependency.mCliDaemonInstance = &cliDaemon;
    cliDependency.mOutputToSend      = longTestOutput.c_str();

    otbr::MainloopContext context;

    EXPECT_EQ(cliDaemon.Init(netIfName), OT_ERROR_NONE);

    int clientSocket = -1;
    {
        struct sockaddr_un serverAddr;

        clientSocket = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
        ASSERT_GE(clientSocket, 0) << "socket creation failed: " << strerror(errno);

        memset(&serverAddr, 0, sizeof(serverAddr));
        serverAddr.sun_family = AF_UNIX;
        strncpy(serverAddr.sun_path, socketFile, sizeof(serverAddr.sun_path) - 1);
        ASSERT_EQ(connect(clientSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)), 0);

        int rval = send(clientSocket, command, strlen(command), 0);
        ASSERT_GE(rval, 0) << "Error sending command: " << strerror(errno);
    }

    char recvBuf[kCliMaxLineLength];
    bool outputReceived = false;

    while (!outputReceived)
    {
        context.mMaxFd   = -1;
        context.mTimeout = {100, 0};
        FD_ZERO(&context.mReadFdSet);
        FD_ZERO(&context.mWriteFdSet);
        FD_ZERO(&context.mErrorFdSet);

        cliDaemon.Update(context);

        context.AddFdToReadSet(clientSocket);

        int rval = select(context.mMaxFd + 1, &context.mReadFdSet, &context.mWriteFdSet, &context.mErrorFdSet,
                          &context.mTimeout);
        ASSERT_GE(rval, 0) << "select failed, error: " << strerror(errno);

        cliDaemon.Process(context);

        if (FD_ISSET(clientSocket, &context.mReadFdSet))
        {
            int rval = read(clientSocket, recvBuf, kCliMaxLineLength - 1);
            ASSERT_GE(rval, 0) << "Error receiving cli output: " << strerror(errno);

            recvBuf[rval]  = '\0';
            outputReceived = true;
        }
    }

    EXPECT_EQ(strncmp(recvBuf, longTestOutput.c_str(), kCliMaxLineLength - sizeof(kTruncatedMsg)), 0);
    EXPECT_STREQ(recvBuf + kCliMaxLineLength - sizeof(kTruncatedMsg), kTruncatedMsg);

    cliDaemon.Deinit();
}

class CliDaemonTestMultiLineInput : public otbr::CliDaemon::Dependencies
{
public:
    otbrError InputCommandLine(const char *aLine) override
    {
        mCommands.emplace_back(aLine);
        return OTBR_ERROR_NONE;
    }

    std::vector<std::string> mCommands;
};

static int ConnectCliDaemonClient(const char *aSocketFile)
{
    int                clientSocket;
    struct sockaddr_un serverAddr;

    clientSocket = socket(AF_UNIX, SOCK_STREAM, 0);
    VerifyOrExit(clientSocket >= 0);

    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sun_family = AF_UNIX;
    strncpy(serverAddr.sun_path, aSocketFile, sizeof(serverAddr.sun_path) - 1);

    if (connect(clientSocket, reinterpret_cast<struct sockaddr *>(&serverAddr), sizeof(serverAddr)) != 0)
    {
        close(clientSocket);
        clientSocket = -1;
    }

exit:
    return clientSocket;
}

static void ProcessCliDaemonUntil(CliDaemon &aCliDaemon, std::function<bool(void)> aCondition)
{
    otbr::MainloopContext context;

    for (int i = 0; i < 100 && !aCondition(); i++)
    {
        context.mMaxFd   = -1;
        context.mTimeout = {0, 100000};
        FD_ZERO(&context.mReadFdSet);
        FD_ZERO(&context.mWriteFdSet);
        FD_ZERO(&context.mErrorFdSet);

        aCliDaemon.Update(context);
        ASSERT_GE(select(context.mMaxFd + 1, &context.mReadFdSet, &context.mWriteFdSet, &context.mErrorFdSet,
                         &context.mTimeout),
                  0)
            << "select failed, error: " << strerror(errno);
        aCliDaemon.Process(context);
    }
}

TEST(CliDaemon, InputCommandLineSplitsLines_AfterReceivingOnSessionSocket)
{
    CliDaemonTestMultiLineInput cliDependency;
    CliDaemon                   cliDaemon(cliDependency);
    int                         clientSocket;

    ASSERT_EQ(cliDaemon.Init("tun0"), OTBR_ERROR_NONE);
    clientSocket = ConnectCliDaemonClient("/run/openthread-tun0.sock");
    ASSERT_GE(clientSocket, 0) << "connect failed: " << strerror(errno);

    // Two complete lines and a partial one in a single write, with an empty line before and between them (such as the
    // leading '\n' sent by OpenThreadClient::Execute()).
    ASSERT_GE(send(clientSocket, "\nstate\r\n  \r\nversion\nifcon", 25, 0), 0);
    ProcessCliDaemonUntil(cliDaemon, [&]() { return cliDependency.mCommands.size() >= 2; });
    ASSERT_EQ(cliDependency.mCommands, (std::vector<std::string>{"state", "version"}));

    // The partial line is completed by the next write.
    ASSERT_GE(send(clientSocket, "fig\n", 4, 0), 0);
    ProcessCliDaemonUntil(cliDaemon, [&]() { return cliDependency.mCommands.size() >= 3; });
    EXPECT_EQ(cliDependency.mCommands, (std::vector<std::string>{"state", "version", "ifconfig"}));

    close(clientSocket);
    cliDaemon.Deinit();
}

TEST(CliDaemon, InputCommandLineDropsTooLongLine_AfterReceivingOnSessionSocket)
{
    CliDaemonTestMultiLineInput cliDependency;
    CliDaemon                   cliDaemon(cliDependency);
    int                         clientSocket;
    std::string                 input(kCliMaxLineLength * 2, 'a');
    char                        recvBuf[kCliMaxLineLength];
    ssize_t                     received;

    ASSERT_EQ(cliDaemon.Init("tun0"), OTBR_ERROR_NONE);
    clientSocket = ConnectCliDaemonClient("/run/openthread-tun0.sock");
    ASSERT_GE(clientSocket, 0) << "connect failed: " << strerror(errno);

    input += "\nstate\n";
    ASSERT_EQ(send(clientSocket, input.data(), input.size(), 0), static_cast<ssize_t>(input.size()));
    ProcessCliDaemonUntil(cliDaemon, [&]() { return !cliDependency.mCommands.empty(); });

    // The too long line is rejected as a whole, and the next line is still processed.
    EXPECT_EQ(cliDependency.mCommands, (std::vector<std::string>{"state"}));

    received = recv(clientSocket, recvBuf, sizeof(recvBuf) - 1, MSG_DONTWAIT);
    ASSERT_GT(received, 0);
    recvBuf[received] = '\0';
    EXPECT_STREQ(recvBuf, "Error 7: InvalidArgs\r\n");

    close(clientSocket);
    cliDaemon.Deinit();
}

#endif // __linux__
