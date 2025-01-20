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

#define OTBR_LOG_TAG "DAEMON"

#include "daemon.hpp"

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

#include <openthread/cli.h>

#include "utils/socket_utils.hpp"

#ifndef OTBR_POSIX_CONFIG_THREAD_NETIF_DEFAULT_NAME
#define OTBR_POSIX_CONFIG_THREAD_NETIF_DEFAULT_NAME "wpan0"
#endif

#ifndef OTBR_POSIX_CONFIG_DAEMON_SOCKET_BASENAME
#define OTBR_POSIX_CONFIG_DAEMON_SOCKET_BASENAME "/run/openthread-"
#endif

#ifndef OTBR_CONFIG_CLI_MAX_LINE_LENGTH
#define OTBR_CONFIG_CLI_MAX_LINE_LENGTH 640
#endif

namespace otbr {

static constexpr size_t kMaxSocketFilenameLength = sizeof(sockaddr_un::sun_path) - 1;

std::string Daemon::GetSocketFilename() const
{
    std::string fileName;
    std::string netIfName = mNetifName.empty() ? OTBR_POSIX_CONFIG_THREAD_NETIF_DEFAULT_NAME : mNetifName;

    fileName = OTBR_POSIX_CONFIG_DAEMON_SOCKET_BASENAME + netIfName + ".sock";
    if (fileName.size() > kMaxSocketFilenameLength)
    {
        DieNow(otbrErrorString(OTBR_ERROR_INVALID_ARGS));
    }

    return fileName;
}

std::string Daemon::GetSocketLockFilename() const
{
    std::string fileName;
    std::string netIfName = mNetifName.empty() ? OTBR_POSIX_CONFIG_THREAD_NETIF_DEFAULT_NAME : mNetifName;

    fileName = OTBR_POSIX_CONFIG_DAEMON_SOCKET_BASENAME + netIfName + ".lock";
    if (fileName.size() > kMaxSocketFilenameLength)
    {
        DieNow(otbrErrorString(OTBR_ERROR_INVALID_ARGS));
    }

    return fileName;
}

Daemon::Daemon()
    : mListenSocket(-1)
    , mDaemonLock(-1)
    , mSessionSocket(-1)
{
}

int Daemon::OutputFormat(const char *aFormat, ...)
{
    int     ret;
    va_list ap;

    va_start(ap, aFormat);
    ret = OutputFormatV(aFormat, ap);
    va_end(ap);

    return ret;
}

int Daemon::OutputFormatV(const char *aFormat, va_list aArguments)
{
    static constexpr char truncatedMsg[] = "(truncated ...)";
    char                  buf[OTBR_CONFIG_CLI_MAX_LINE_LENGTH];
    int                   rval;

    static_assert(sizeof(truncatedMsg) < OTBR_CONFIG_CLI_MAX_LINE_LENGTH,
                  "OTBR_CONFIG_CLI_MAX_LINE_LENGTH is too short!");

    rval = vsnprintf(buf, sizeof(buf), aFormat, aArguments);
    VerifyOrExit(rval >= 0, otbrLogWarning("Failed to format CLI output: %s", strerror(errno)));

    if (rval >= static_cast<int>(sizeof(buf)))
    {
        rval = static_cast<int>(sizeof(buf) - 1);
        memcpy(buf + sizeof(buf) - sizeof(truncatedMsg), truncatedMsg, sizeof(truncatedMsg));
    }

    VerifyOrExit(mSessionSocket != -1);

#ifdef __linux__
    // Don't die on SIGPIPE
    rval = send(mSessionSocket, buf, static_cast<size_t>(rval), MSG_NOSIGNAL);
#else
    rval = static_cast<int>(write(mSessionSocket, buf, static_cast<size_t>(rval)));
#endif

    if (rval < 0)
    {
        otbrLogWarning("Failed to write CLI output: %s", strerror(errno));
        close(mSessionSocket);
        mSessionSocket = -1;
    }

exit:
    return rval;
}

void Daemon::InitializeSessionSocket(void)
{
    int newSessionSocket;
    int rval;

    VerifyOrExit((newSessionSocket = accept(mListenSocket, nullptr, nullptr)) != -1, rval = -1);

    VerifyOrExit((rval = fcntl(newSessionSocket, F_GETFD, 0)) != -1);

    rval |= FD_CLOEXEC;

    VerifyOrExit((rval = fcntl(newSessionSocket, F_SETFD, rval)) != -1);

#ifndef __linux__
    // some platforms (macOS, Solaris) don't have MSG_NOSIGNAL
    // SOME of those (macOS, but NOT Solaris) support SO_NOSIGPIPE
    // if we have SO_NOSIGPIPE, then set it. Otherwise, we're going
    // to simply ignore it.
#if defined(SO_NOSIGPIPE)
    rval = setsockopt(newSessionSocket, SOL_SOCKET, SO_NOSIGPIPE, &rval, sizeof(rval));
    VerifyOrExit(rval != -1);
#else
#warning "no support for MSG_NOSIGNAL or SO_NOSIGPIPE"
#endif
#endif // __linux__

    if (mSessionSocket != -1)
    {
        close(mSessionSocket);
    }
    mSessionSocket = newSessionSocket;

exit:
    if (rval == -1)
    {
        otbrLogWarning("Failed to initialize session socket: %s", strerror(errno));
        if (newSessionSocket != -1)
        {
            close(newSessionSocket);
        }
    }
    else
    {
        otbrLogInfo("Session socket is ready");
    }
}

void Daemon::createListenSocketOrDie(void)
{
    struct sockaddr_un sockname;

    class AllowAllGuard
    {
    public:
        AllowAllGuard(void)
        {
            const char *allowAll = getenv("OT_DAEMON_ALLOW_ALL");
            mAllowAll            = (allowAll != nullptr && strcmp("1", allowAll) == 0);

            if (mAllowAll)
            {
                mMode = umask(0);
            }
        }
        ~AllowAllGuard(void)
        {
            if (mAllowAll)
            {
                umask(mMode);
            }
        }

    private:
        bool   mAllowAll = false;
        mode_t mMode     = 0;
    };

    mListenSocket = SocketWithCloseExec(AF_UNIX, SOCK_STREAM, 0, kSocketNonBlock);
    VerifyOrDie(mListenSocket != -1, strerror(errno));

    {
        std::string lockfile = GetSocketLockFilename();

        mDaemonLock = open(lockfile.c_str(), O_CREAT | O_RDONLY | O_CLOEXEC, 0600);
    }

    if (mDaemonLock == -1)
    {
        otbrLogCrit("open");
        DieNow(otbrErrorString(OTBR_ERROR_ERRNO));
    }

    if (flock(mDaemonLock, LOCK_EX | LOCK_NB) == -1)
    {
        otbrLogCrit("flock");
        DieNow(otbrErrorString(OTBR_ERROR_ERRNO));
    }

    std::string socketfile = GetSocketFilename();

    memset(&sockname, 0, sizeof(struct sockaddr_un));

    sockname.sun_family = AF_UNIX;
    strncpy(sockname.sun_path, socketfile.c_str(), sizeof(sockname.sun_path) - 1);
    (void)unlink(sockname.sun_path);

    {
        AllowAllGuard allowAllGuard;

        if (bind(mListenSocket, reinterpret_cast<const struct sockaddr *>(&sockname), sizeof(struct sockaddr_un)) == -1)
        {
            otbrLogCrit("bind");
            DieNow(otbrErrorString(OTBR_ERROR_ERRNO));
        }
    }
}

void Daemon::Init(const std::string &aNetIfName)
{
    // This allows implementing pseudo reset.
    VerifyOrExit(mListenSocket == -1);

    mNetifName = aNetIfName;
    createListenSocketOrDie();

    //
    // only accept 1 connection.
    //
    if (listen(mListenSocket, 1) == -1)
    {
        otbrLogCrit("listen");
        DieNow(otbrErrorString(OTBR_ERROR_ERRNO));
    }

    // TODO:: Send Spinel message to NCP to initialize CLI (otCliInit)

exit:
    return;
}

void Daemon::Deinit(void)
{
    if (mSessionSocket != -1)
    {
        close(mSessionSocket);
        mSessionSocket = -1;
    }
}

void Daemon::Process(const MainloopContext &aContext)
{
    ssize_t rval;

    VerifyOrExit(mListenSocket != -1);

    if (FD_ISSET(mListenSocket, &aContext.mErrorFdSet))
    {
        otbrLogCrit("daemon socket error");
        DieNow(otbrErrorString(OTBR_ERROR_ERRNO));
    }
    else if (FD_ISSET(mListenSocket, &aContext.mReadFdSet))
    {
        InitializeSessionSocket();
    }

    VerifyOrExit(mSessionSocket != -1);

    if (FD_ISSET(mSessionSocket, &aContext.mErrorFdSet))
    {
        close(mSessionSocket);
        mSessionSocket = -1;
    }
    else if (FD_ISSET(mSessionSocket, &aContext.mReadFdSet))
    {
        uint8_t buffer[OTBR_CONFIG_CLI_MAX_LINE_LENGTH];

        // leave 1 byte for the null terminator
        rval = read(mSessionSocket, buffer, sizeof(buffer) - 1);

        if (rval > 0)
        {
            buffer[rval] = '\0';

            otbrLogWarning("Received CLI command: %s", reinterpret_cast<char *>(buffer));
            // TODO:: Send Spinel message to NCP to process the command (otCliInputLine)
        }
        else
        {
            if (rval < 0)
            {
                otbrLogWarning("Daemon read: %s", strerror(errno));
            }
            close(mSessionSocket);
            mSessionSocket = -1;
        }
    }

exit:
    return;
}

void Daemon::UpdateFdSet(MainloopContext &aContext)
{
    if (mListenSocket != -1)
    {
        FD_SET(mListenSocket, &aContext.mReadFdSet);
        FD_SET(mListenSocket, &aContext.mErrorFdSet);

        if (aContext.mMaxFd < mListenSocket)
        {
            aContext.mMaxFd = mListenSocket;
        }
    }

    if (mSessionSocket != -1)
    {
        FD_SET(mSessionSocket, &aContext.mReadFdSet);
        FD_SET(mSessionSocket, &aContext.mErrorFdSet);

        if (aContext.mMaxFd < mSessionSocket)
        {
            aContext.mMaxFd = mSessionSocket;
        }
    }

    return;
}

} // namespace otbr
