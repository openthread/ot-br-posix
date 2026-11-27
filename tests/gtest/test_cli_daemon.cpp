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
#include <fcntl.h>
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
static const char       kTruncatedMsg[]   = "(truncated ...)\r\n";

// Runs a cleanup when leaving the scope, including on an early return from a failed ASSERT_*.
class ScopeGuard
{
public:
    explicit ScopeGuard(std::function<void(void)> aCleanup)
        : mCleanup(std::move(aCleanup))
    {
    }
    ~ScopeGuard(void) { mCleanup(); }

private:
    std::function<void(void)> mCleanup;
};

TEST(CliDaemon, InitSocketCreationWithFullNetIfName)
{
    const char *netIfName  = "tun0";
    const char *socketFile = "/run/openthread-tun0.sock";
    const char *lockFile   = "/run/openthread-tun0.lock";

    CliDaemon::Dependencies sDefaultCliDaemonDependencies;
    CliDaemon               cliDaemon(sDefaultCliDaemonDependencies);

    EXPECT_EQ(cliDaemon.Init(netIfName), OTBR_ERROR_NONE);

    struct stat st;

    EXPECT_EQ(stat(socketFile, &st), 0);
    EXPECT_EQ(stat(lockFile, &st), 0);

    cliDaemon.Deinit();

    // The socket file is removed on Deinit(), so that clients do not connect to a stale socket.
    EXPECT_NE(stat(socketFile, &st), 0);
}

TEST(CliDaemon, InitSocketCreationWithEmptyNetIfName)
{
    const char *socketFile = "/run/openthread-wpan0.sock";
    const char *lockFile   = "/run/openthread-wpan0.lock";

    CliDaemon::Dependencies sDefaultCliDaemonDependencies;
    CliDaemon               cliDaemon(sDefaultCliDaemonDependencies);
    EXPECT_EQ(cliDaemon.Init(""), OTBR_ERROR_NONE);

    struct stat st;

    EXPECT_EQ(stat(socketFile, &st), 0);
    EXPECT_EQ(stat(lockFile, &st), 0);

    cliDaemon.Deinit();
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

    int        clientSocket = -1;
    ScopeGuard clientSocketGuard([&clientSocket]() {
        if (clientSocket >= 0)
        {
            close(clientSocket);
        }
    });
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

    int        clientSocket = -1;
    ScopeGuard clientSocketGuard([&clientSocket]() {
        if (clientSocket >= 0)
        {
            close(clientSocket);
        }
    });
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
    // The truncated output still ends a line, so that line-based clients such as ot-ctl see the following output.
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

static void ProcessCliDaemonIterations(CliDaemon &aCliDaemon, int aIterations)
{
    int iterations = 0;

    ProcessCliDaemonUntil(aCliDaemon, [&]() { return iterations++ >= aIterations; });
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

class CliDaemonTestAsyncOutput : public otbr::CliDaemon::Dependencies
{
public:
    otbrError InputCommandLine(const char *aLine) override
    {
        mCommands.emplace_back(aLine);
        return OTBR_ERROR_NONE;
    }

    std::vector<std::string> mCommands;
};

TEST(CliDaemon, OutputFormatRoutesToConsole_WhenConsoleModeIsEnabled)
{
    CliDaemonTestAsyncOutput cliDependency;
    CliDaemon                cliDaemon(cliDependency, CliDaemon::OTBR_DAEMON_MODE_CONSOLE);
    int                      pipeFds[2];
    int                      savedStdin;
    int                      savedStdout;
    int                      nullFd;
    char                     recvBuf[kCliMaxLineLength];
    ssize_t                  received;
    char                     line[] = "state";

    // Keep readline away from the caller's terminal, and capture the console output in a pipe. The standard streams
    // are restored on any exit from this test so that a failure does not swallow the output of later tests.
    pipeFds[0] = pipeFds[1] = -1;
    savedStdin              = dup(STDIN_FILENO);
    ASSERT_GE(savedStdin, 0);
    savedStdout = dup(STDOUT_FILENO);
    ASSERT_GE(savedStdout, 0);

    {
        ScopeGuard stdioGuard([&]() {
            cliDaemon.Deinit();
            dup2(savedStdout, STDOUT_FILENO);
            dup2(savedStdin, STDIN_FILENO);
            close(savedStdout);
            close(savedStdin);
            if (pipeFds[1] >= 0)
            {
                close(pipeFds[1]);
            }
        });

        nullFd = open("/dev/null", O_RDONLY);
        ASSERT_GE(nullFd, 0);
        ASSERT_GE(dup2(nullFd, STDIN_FILENO), 0);
        close(nullFd);

        ASSERT_EQ(pipe(pipeFds), 0);
        ASSERT_GE(dup2(pipeFds[1], STDOUT_FILENO), 0);

        ASSERT_EQ(cliDaemon.Init("tun0"), OTBR_ERROR_NONE);

        // Output not triggered by a command (such as the initial prompt, or NCP output) goes to the console.
        EXPECT_GT(cliDaemon.OutputFormat("%s", "> "), 0);

        cliDaemon.ProcessLine(line, STDOUT_FILENO);
        EXPECT_EQ(cliDependency.mCommands, (std::vector<std::string>{"state"}));
        EXPECT_GT(cliDaemon.OutputFormat("%s", "disabled\r\nDone\r\n"), 0);
    }

    received = read(pipeFds[0], recvBuf, sizeof(recvBuf) - 1);
    ASSERT_GT(received, 0);
    recvBuf[received] = '\0';
    EXPECT_THAT(recvBuf, testing::HasSubstr("> disabled\r\nDone\r\n"));

    close(pipeFds[0]);
}

TEST(CliDaemon, OutputFormatIsNotSentToNextSession_AfterSessionCloses)
{
    CliDaemonTestAsyncOutput cliDependency;
    CliDaemon                cliDaemon(cliDependency);
    int                      clientSocket;
    char                     recvBuf[kCliMaxLineLength];
    ssize_t                  received;

    ASSERT_EQ(cliDaemon.Init("tun0"), OTBR_ERROR_NONE);

    clientSocket = ConnectCliDaemonClient("/run/openthread-tun0.sock");
    ASSERT_GE(clientSocket, 0) << "connect failed: " << strerror(errno);
    ASSERT_GE(send(clientSocket, "scan\n", 5, 0), 0);
    ProcessCliDaemonUntil(cliDaemon, [&]() { return !cliDependency.mCommands.empty(); });
    ASSERT_EQ(cliDependency.mCommands, (std::vector<std::string>{"scan"}));

    // The first client goes away while its command is still pending.
    close(clientSocket);
    ProcessCliDaemonIterations(cliDaemon, 3);

    clientSocket = ConnectCliDaemonClient("/run/openthread-tun0.sock");
    ASSERT_GE(clientSocket, 0) << "connect failed: " << strerror(errno);
    ProcessCliDaemonIterations(cliDaemon, 3);

    // The trailing output of the previous session's command is dropped instead of reaching the new session.
    EXPECT_EQ(cliDaemon.OutputFormat("%s", "Done\r\n"), 0);

    received = recv(clientSocket, recvBuf, sizeof(recvBuf) - 1, MSG_DONTWAIT);
    EXPECT_LT(received, 0);

    close(clientSocket);
    cliDaemon.Deinit();
}

TEST(CliDaemon, ProcessLineKeepsEscapedTrailingWhitespace)
{
    CliDaemonTestAsyncOutput cliDependency;
    CliDaemon                cliDaemon(cliDependency, /* aMode */ 0);
    char                     escapedSpace[]     = "networkname MyNet\\ \r";
    char                     escapedBackslash[] = "networkname MyNet\\\\ \r";

    // As sent by `ot-ctl networkname "MyNet "`.
    cliDaemon.ProcessLine(escapedSpace, -1);
    // An escaped backslash does not escape the following whitespace.
    cliDaemon.ProcessLine(escapedBackslash, -1);

    EXPECT_EQ(cliDependency.mCommands, (std::vector<std::string>{"networkname MyNet\\ ", "networkname MyNet\\\\"}));
}

TEST(CliDaemon, InitRebindsSocket_WhenInterfaceNameChanges)
{
    CliDaemon::Dependencies cliDependency;
    CliDaemon               cliDaemon(cliDependency);
    struct stat             st;

    ASSERT_EQ(cliDaemon.Init("tun0"), OTBR_ERROR_NONE);
    EXPECT_EQ(stat("/run/openthread-tun0.sock", &st), 0);

    // Init() again with the same name (a software reset of the host) keeps the socket.
    ASSERT_EQ(cliDaemon.Init("tun0"), OTBR_ERROR_NONE);
    EXPECT_EQ(stat("/run/openthread-tun0.sock", &st), 0);

    // The interface got another name, the daemon follows it.
    ASSERT_EQ(cliDaemon.Init("tun1"), OTBR_ERROR_NONE);
    EXPECT_NE(stat("/run/openthread-tun0.sock", &st), 0);
    EXPECT_EQ(stat("/run/openthread-tun1.sock", &st), 0);
    EXPECT_GE(ConnectCliDaemonClient("/run/openthread-tun1.sock"), 0);

    cliDaemon.Deinit();
}

#endif // __linux__
