/*
 *  Copyright (c) 2025, The OpenThread Authors.
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

#define OTBR_LOG_TAG "CLI_DAEMON"

#include "cli_daemon.hpp"

#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <string.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include "utils/socket_utils.hpp"

namespace otbr {

otbrError CliDaemon::Dependencies::InputCommandLine(const char *aLine)
{
    OTBR_UNUSED_VARIABLE(aLine);

    return OTBR_ERROR_NONE;
}

static constexpr char kDefaultNetIfName[] = "wpan0";
static constexpr char kSocketBaseName[]   = "/run/openthread-";
static constexpr char kSocketSuffix[]     = ".sock";
static constexpr char kSocketLockSuffix[] = ".lock";
static constexpr char kTruncatedMsg[]     = "(truncated ...)";

static constexpr size_t kMaxSocketFilenameLength = sizeof(sockaddr_un::sun_path) - 1;

std::string CliDaemon::GetSocketFilename(const std::string &aNetIfName, const char *aSuffix) const
{
    std::string netIfName = aNetIfName.empty() ? kDefaultNetIfName : aNetIfName;

    std::string fileName = kSocketBaseName + netIfName + aSuffix;

    VerifyOrDie(fileName.size() <= kMaxSocketFilenameLength, otbrErrorString(OTBR_ERROR_INVALID_ARGS));

    return fileName;
}

void CliDaemon::HandleCommandOutput(const char *aOutput)
{
    int    ret;
    char   buf[kCliMaxLineLength];
    size_t length = strlen(aOutput);

    VerifyOrExit(mSessionSocket != -1);

    static_assert(sizeof(kTruncatedMsg) < kCliMaxLineLength, "OTBR_CONFIG_CLI_MAX_LINE_LENGTH is too short!");

    strncpy(buf, aOutput, kCliMaxLineLength);

    if (length >= kCliMaxLineLength)
    {
        length = kCliMaxLineLength - 1;
        memcpy(buf + kCliMaxLineLength - sizeof(kTruncatedMsg), kTruncatedMsg, sizeof(kTruncatedMsg));
    }

#ifdef __linux__
    // MSG_NOSIGNAL prevents read() from sending a SIGPIPE in the case of a broken pipe.
    ret = send(mSessionSocket, buf, length, MSG_NOSIGNAL);
#else
    ret = static_cast<int>(write(mSessionSocket, buf, length));
#endif

    if (ret < 0)
    {
        otbrLogWarning("Failed to write CLI output: %s", strerror(errno));
        Clear();
    }

exit:
    return;
}

CliDaemon::CliDaemon(Dependencies &aDependencies)
    : mListenSocket(-1)
    , mDaemonLock(-1)
    , mSessionSocket(-1)
    , mDeps(aDependencies)
{
}

otbrError CliDaemon::CreateListenSocket(const std::string &aNetIfName)
{
    otbrError error = OTBR_ERROR_NONE;

    std::string        lockFile;
    std::string        socketFile;
    struct sockaddr_un sockname;

    mListenSocket = SocketWithCloseExec(AF_UNIX, SOCK_STREAM, 0, kSocketNonBlock);
    VerifyOrExit(mListenSocket != -1, error = OTBR_ERROR_ERRNO);

    lockFile    = GetSocketFilename(aNetIfName, kSocketLockSuffix);
    mDaemonLock = open(lockFile.c_str(), O_CREAT | O_RDONLY | O_CLOEXEC, 0600);
    VerifyOrExit(mDaemonLock != -1, error = OTBR_ERROR_ERRNO);

    VerifyOrExit(flock(mDaemonLock, LOCK_EX | LOCK_NB) != -1, error = OTBR_ERROR_ERRNO);

    socketFile = GetSocketFilename(aNetIfName, kSocketSuffix);

    memset(&sockname, 0, sizeof(struct sockaddr_un));
    sockname.sun_family = AF_UNIX;
    strncpy(sockname.sun_path, socketFile.c_str(), sizeof(sockname.sun_path) - 1);
    OTBR_UNUSED_VARIABLE(unlink(sockname.sun_path));

    VerifyOrExit(
        bind(mListenSocket, reinterpret_cast<const struct sockaddr *>(&sockname), sizeof(struct sockaddr_un)) != -1,
        error = OTBR_ERROR_ERRNO);

exit:
    return error;
}

void CliDaemon::InitializeSessionSocket(void)
{
    int newSessionSocket = -1;
    int flag             = -1;

    // The `accept()` call uses `nullptr` for `addr` and `addrlen` arguments as we don't need the client address
    // information.
    VerifyOrExit((newSessionSocket = accept(mListenSocket, nullptr, nullptr)) != -1);
    VerifyOrExit((flag = fcntl(newSessionSocket, F_GETFD, 0)) != -1, close(newSessionSocket));

    flag |= FD_CLOEXEC;
    VerifyOrExit((flag = fcntl(newSessionSocket, F_SETFD, flag)) != -1, close(newSessionSocket));

#ifndef __linux__
    // some platforms (macOS, Solaris) don't have MSG_NOSIGNAL
    // SOME of those (macOS, but NOT Solaris) support SO_NOSIGPIPE
    // if we have SO_NOSIGPIPE, then set it. Otherwise, we're going
    // to simply ignore it.
#if defined(SO_NOSIGPIPE)
    VerifyOrExit((flag = setsockopt(newSessionSocket, SOL_SOCKET, SO_NOSIGPIPE, &flag, sizeof(flag))) != -1,
                 close(newSessionSocket));
#else
#warning "no support for MSG_NOSIGNAL or SO_NOSIGPIPE"
#endif
#endif // __linux__

    Clear();

    mSessionSocket = newSessionSocket;
    otbrLogInfo("Session socket is ready");

exit:
    if (flag == -1)
    {
        otbrLogWarning("Failed to initialize session socket: %s", strerror(errno));
        Clear();
    }
}

otbrError CliDaemon::Init(const std::string &aNetIfName)
{
    otbrError error = OTBR_ERROR_NONE;

    // This allows implementing pseudo reset.
    VerifyOrExit(mListenSocket == -1, error = OTBR_ERROR_INVALID_STATE);

    SuccessOrExit(error = CreateListenSocket(aNetIfName));

    //
    // only accept 1 connection.
    //
    VerifyOrExit(listen(mListenSocket, 1) != -1, error = OTBR_ERROR_ERRNO);

exit:
    return error;
}

void CliDaemon::Clear(void)
{
    if (mSessionSocket != -1)
    {
        close(mSessionSocket);
        mSessionSocket = -1;
    }
}

void CliDaemon::Deinit(void)
{
    Clear();
}

void CliDaemon::UpdateFdSet(MainloopContext &aContext)
{
    if (mListenSocket != -1)
    {
        aContext.AddFdToSet(mListenSocket, MainloopContext::kErrorFdSet | MainloopContext::kReadFdSet);
    }

    if (mSessionSocket != -1)
    {
        aContext.AddFdToSet(mSessionSocket, MainloopContext::kErrorFdSet | MainloopContext::kReadFdSet);
    }
}

void CliDaemon::Process(const MainloopContext &aContext)
{
    VerifyOrExit(mListenSocket != -1);

    if (FD_ISSET(mListenSocket, &aContext.mErrorFdSet))
    {
        DieNow("daemon socket error");
    }

    if (FD_ISSET(mListenSocket, &aContext.mReadFdSet))
    {
        InitializeSessionSocket();
    }

    VerifyOrExit(mSessionSocket != -1);

    if (FD_ISSET(mSessionSocket, &aContext.mErrorFdSet))
    {
        Clear();
    }
    else if (FD_ISSET(mSessionSocket, &aContext.mReadFdSet))
    {
        uint8_t   buffer[kCliMaxLineLength];
        otbrError error = OTBR_ERROR_NONE;
        ssize_t   received;

        // leave 1 byte for the null terminator
        received = read(mSessionSocket, buffer, sizeof(buffer) - 1);

        if (received > 0)
        {
            buffer[received] = '\0';
            error            = mDeps.InputCommandLine(reinterpret_cast<char *>(buffer));

            if (error != OTBR_ERROR_NONE)
            {
                otbrLogWarning("Failed to input command line, error:%s", otbrErrorString(error));
            }
        }
        else if (received == 0)
        {
            otbrLogInfo("Session socket closed by peer");
            Clear();
        }
        else
        {
            otbrLogWarning("CLI Daemon read: %s", strerror(errno));
            Clear();
        }
    }

exit:
    return;
}

} // namespace otbr
