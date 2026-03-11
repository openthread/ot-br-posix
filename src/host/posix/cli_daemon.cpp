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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#define OTBR_LOG_TAG "CLI_DAEMON"

#include "cli_daemon.hpp"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <string.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#if HAVE_LIBEDIT
#include <editline/readline.h>
#elif HAVE_LIBREADLINE
#include <readline/history.h>
#include <readline/readline.h>
#endif

#include "common/code_utils.hpp"
#include "common/logging.hpp"
#include "utils/socket_utils.hpp"

namespace otbr {

namespace {

char *TrimLine(char *aLine)
{
    char *end;

    while (isspace(static_cast<unsigned char>(*aLine)))
    {
        aLine++;
    }

    end = aLine + strlen(aLine);

    while (end > aLine && isspace(static_cast<unsigned char>(end[-1])))
    {
        end--;
    }

    *end = '\0';

    return aLine;
}

constexpr char kDefaultNetIfName[] = "wpan0";
constexpr char kSocketBaseName[]   = "/run/openthread-";
constexpr char kSocketSuffix[]     = ".sock";
constexpr char kSocketLockSuffix[] = ".lock";
constexpr char kTruncatedMsg[]     = "(truncated ...)";

constexpr size_t kMaxSocketFilenameLength = sizeof(sockaddr_un::sun_path) - 1;

std::string GetSocketFilename(const std::string &aNetIfName, const char *aSuffix)
{
    std::string netIfName = aNetIfName.empty() ? kDefaultNetIfName : aNetIfName;

    std::string fileName = kSocketBaseName + netIfName + aSuffix;

    VerifyOrDie(fileName.size() <= kMaxSocketFilenameLength, otbrErrorString(OTBR_ERROR_INVALID_ARGS));

    return fileName;
}

} // namespace

otbrError CliDaemon::Dependencies::InputCommandLine(const char *aLine)
{
    OTBR_UNUSED_VARIABLE(aLine);

    return OTBR_ERROR_NONE;
}

void CliDaemon::ProcessCommand(const char *aLine, void *aContext, otCliOutputCallback aCallback)
{
    char line[kCliMaxLineLength];

    mExternalOutputCallback        = aCallback;
    mExternalOutputCallbackContext = aContext;

    if (aLine != nullptr && aCallback != nullptr)
    {
        strncpy(line, aLine, sizeof(line) - 1);
        line[sizeof(line) - 1] = '\0';
        ProcessLine(line, -1);
    }
}

otbrError CliDaemon::SetOutputFd(int aFd)
{
    otbrError error = OTBR_ERROR_NONE;

    // TODO: check if CLI is busy if possible.
    // In NCP mode, we might not know.
    // In RCP mode, we can use otCliIsCommandPending().

    mOutputFd = aFd;
    if (aFd != -1)
    {
        mExternalOutputCallback = nullptr;
    }

    return error;
}

void CliDaemon::Reject(const char *aFormat, ...)
{
    va_list ap;

    va_start(ap, aFormat);
    if (mExternalOutputCallback != nullptr)
    {
        mExternalOutputCallback(mExternalOutputCallbackContext, aFormat, ap);
    }
    va_end(ap);
    mExternalOutputCallbackContext = nullptr;
    mExternalOutputCallback        = nullptr;
}

void CliDaemon::ProcessLine(char *aLine, int aOutputFd)
{
    static constexpr char kBusyMessage[] = "Error: CLI is busy, please try again later.\r\n> ";

    aLine = TrimLine(aLine);
    if (SetOutputFd(aOutputFd) == OTBR_ERROR_NONE)
    {
        if (mDeps.InputCommandLine(aLine) != OTBR_ERROR_NONE)
        {
            otbrLogWarning("Failed to process command line");
        }
    }
    else if (mExternalOutputCallback != nullptr)
    {
        Reject(kBusyMessage);
    }
    else if (aOutputFd == STDOUT_FILENO)
    {
        printf("%s", kBusyMessage);
        fflush(stdout);
    }
    else if (aOutputFd != -1)
    {
        UnixSocketOutput(kBusyMessage, sizeof(kBusyMessage) - 1);
    }
}

int CliDaemon::OutputFormatV(const char *aFormat, va_list aArguments)
{
    int rval = 0;

    if (mExternalOutputCallback != nullptr)
    {
        rval = mExternalOutputCallback(mExternalOutputCallbackContext, aFormat, aArguments);
        // We don't clear external callback here because we don't know if command is pending
        // The host implementation should handle it if possible, or we can add a method to Dependencies.
    }
    else if (mOutputFd == STDOUT_FILENO)
    {
        rval = vdprintf(STDOUT_FILENO, aFormat, aArguments);
    }
    else if (mOutputFd != -1)
    {
        rval = UnixSocketOutputV(aFormat, aArguments);
    }
    return rval;
}

int CliDaemon::OutputFormat(const char *aFormat, ...)
{
    int     ret;
    va_list ap;

    va_start(ap, aFormat);
    ret = UnixSocketOutputV(aFormat, ap);
    va_end(ap);

    return ret;
}

int CliDaemon::UnixSocketOutputV(const char *aFormat, va_list aArguments)
{
    char buf[kCliMaxLineLength];
    int  rval;

    static_assert(sizeof(kTruncatedMsg) < kCliMaxLineLength, "kCliMaxLineLength is too short!");

    rval = vsnprintf(buf, sizeof(buf), aFormat, aArguments);
    VerifyOrExit(rval >= 0, otbrLogWarning("Failed to format CLI output: %s", strerror(errno)));

    if (rval >= static_cast<int>(sizeof(buf)))
    {
        rval = static_cast<int>(sizeof(buf) - 1);
        memcpy(buf + sizeof(buf) - sizeof(kTruncatedMsg), kTruncatedMsg, sizeof(kTruncatedMsg));
    }

    rval = UnixSocketOutput(buf, static_cast<size_t>(rval));

exit:
    return rval;
}

int CliDaemon::UnixSocketOutput(const char *aOutput, size_t aLength)
{
    int rval = -1;

    VerifyOrExit(mSessionSocket != -1);
#ifdef __linux__
    // Don't die on SIGPIPE
    rval = send(mSessionSocket, aOutput, aLength, MSG_NOSIGNAL);
#else
    rval = static_cast<int>(write(mSessionSocket, aOutput, aLength));
#endif

    if (rval < 0)
    {
        otbrLogWarning("Failed to write CLI output: %s", strerror(errno));
        UnixSocketSessionClear();
    }

exit:
    return rval;
}

void CliDaemon::UnixSocketSessionInit(void)
{
    int newSessionSocket;
    int rval = 0;

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
    {
        int nosigpipe = 1;
        rval          = setsockopt(newSessionSocket, SOL_SOCKET, SO_NOSIGPIPE, &nosigpipe, sizeof(nosigpipe));
        VerifyOrExit(rval != -1);
    }
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

otbrError CliDaemon::UnixSocketCreate(const std::string &aNetIfName)
{
    otbrError error = OTBR_ERROR_NONE;
    int       ret;

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

    if (mListenSocket == -1)
    {
        ExitNow(error = OTBR_ERROR_ERRNO);
    }

    {
        std::string lockfile = GetSocketFilename(aNetIfName, kSocketLockSuffix);
        mDaemonLock          = open(lockfile.c_str(), O_CREAT | O_RDONLY | O_CLOEXEC, 0600);
    }

    if (mDaemonLock == -1)
    {
        otbrLogErr("open lock file: %s", strerror(errno));
        ExitNow(error = OTBR_ERROR_ERRNO);
    }

    if (flock(mDaemonLock, LOCK_EX | LOCK_NB) == -1)
    {
        otbrLogErr("flock lock file: %s", strerror(errno));
        ExitNow(error = OTBR_ERROR_ERRNO);
    }

    {
        struct sockaddr_un sockname{};
        AllowAllGuard      allowAllGuard;

        sockname.sun_family    = AF_UNIX;
        std::string socketFile = GetSocketFilename(aNetIfName, kSocketSuffix);
        strncpy(sockname.sun_path, socketFile.c_str(), sizeof(sockname.sun_path) - 1);
        (void)unlink(sockname.sun_path);

        ret = bind(mListenSocket, reinterpret_cast<const struct sockaddr *>(&sockname), sizeof(struct sockaddr_un));
    }

    if (ret == -1)
    {
        otbrLogErr("bind daemon socket: %s", strerror(errno));
        ExitNow(error = OTBR_ERROR_ERRNO);
    }

exit:
    if (error != OTBR_ERROR_NONE)
    {
        if (mListenSocket != -1)
        {
            close(mListenSocket);
            mListenSocket = -1;
        }
        if (mDaemonLock != -1)
        {
            close(mDaemonLock);
            mDaemonLock = -1;
        }
    }
    return error;
}

CliDaemon::CliDaemon(Dependencies &aDependencies, uint8_t aMode)
    : mListenSocket(-1)
    , mDaemonLock(-1)
    , mSessionSocket(-1)
    , mOutputFd(-1)
    , mDaemonMode(aMode)
    , mDeps(aDependencies)
{
}

otbrError CliDaemon::Init(const std::string &aNetIfName)
{
    otbrError error = OTBR_ERROR_NONE;

    if (mDaemonMode & OTBR_DAEMON_MODE_CONSOLE)
    {
#if defined(HAVE_LIBEDIT) || defined(HAVE_LIBREADLINE)
        mReadline.Init(*this);
#else
        mStdio.Init(*this);
#endif
    }

    if (mDaemonMode & OTBR_DAEMON_MODE_UNIX_SOCKET)
    {
        VerifyOrExit(mListenSocket == -1);
        SuccessOrExit(error = UnixSocketCreate(aNetIfName));

        //
        // only accept 1 connection.
        //
        VerifyOrExit(listen(mListenSocket, 1) != -1, error = OTBR_ERROR_ERRNO);
    }

exit:
    return error;
}

void CliDaemon::Deinit(void)
{
    if (mDaemonMode & OTBR_DAEMON_MODE_CONSOLE)
    {
#if defined(HAVE_LIBEDIT) || defined(HAVE_LIBREADLINE)
        mReadline.Deinit();
#else
        mStdio.Deinit();
#endif
    }

    UnixSocketSessionClear();

    if (mListenSocket != -1)
    {
        close(mListenSocket);
        mListenSocket = -1;
    }

    if (mDaemonLock != -1)
    {
        (void)flock(mDaemonLock, LOCK_UN);
        close(mDaemonLock);
        mDaemonLock = -1;
    }
}

void CliDaemon::UnixSocketSessionClear(void)
{
    if (mSessionSocket != -1)
    {
        close(mSessionSocket);
        mSessionSocket = -1;
    }
}

void CliDaemon::Update(MainloopContext &aContext)
{
    if (mListenSocket != -1)
    {
        aContext.AddFdToSet(mListenSocket, MainloopContext::kErrorFdSet | MainloopContext::kReadFdSet);
    }

    if (mSessionSocket != -1)
    {
        aContext.AddFdToSet(mSessionSocket, MainloopContext::kErrorFdSet | MainloopContext::kReadFdSet);
    }

    if (mDaemonMode & OTBR_DAEMON_MODE_CONSOLE)
    {
#if defined(HAVE_LIBEDIT) || defined(HAVE_LIBREADLINE)
        mReadline.Update(aContext);
#else
        mStdio.Update(aContext);
#endif
    }
}

void CliDaemon::Process(const MainloopContext &aContext)
{
    if (mListenSocket != -1)
    {
        if (FD_ISSET(mListenSocket, &aContext.mErrorFdSet))
        {
            DieNow("daemon socket error");
        }

        if (FD_ISSET(mListenSocket, &aContext.mReadFdSet))
        {
            UnixSocketSessionInit();
        }
    }

    if (mSessionSocket != -1)
    {
        if (FD_ISSET(mSessionSocket, &aContext.mErrorFdSet))
        {
            UnixSocketSessionClear();
        }
        else if (FD_ISSET(mSessionSocket, &aContext.mReadFdSet))
        {
            uint8_t buffer[kCliMaxLineLength];
            ssize_t received;

            // leave 1 byte for the null terminator
            received = read(mSessionSocket, buffer, sizeof(buffer) - 1);

            if (received > 0)
            {
                buffer[received] = '\0';
                ProcessLine(reinterpret_cast<char *>(buffer), mSessionSocket);
            }
            else if (received == 0)
            {
                otbrLogInfo("Session socket closed by peer");
                UnixSocketSessionClear();
            }
            else
            {
                otbrLogWarning("CLI Daemon read: %s", strerror(errno));
                UnixSocketSessionClear();
            }
        }
    }

    if (mDaemonMode & OTBR_DAEMON_MODE_CONSOLE)
    {
#if defined(HAVE_LIBEDIT) || defined(HAVE_LIBREADLINE)
        mReadline.Process(aContext);
#else
        mStdio.Process(aContext);
#endif
    }
}

#if defined(HAVE_LIBEDIT) || defined(HAVE_LIBREADLINE)
CliDaemon *CliDaemon::Readline::sDaemon = nullptr;

void CliDaemon::Readline::Init(CliDaemon &aDaemon)
{
    sDaemon               = &aDaemon;
    rl_instream           = stdin;
    rl_outstream          = stdout;
    rl_inhibit_completion = true;

    rl_set_screen_size(0, kCliMaxLineLength);

    rl_callback_handler_install("> ", InputCallback);
    rl_already_prompted = true;
}

void CliDaemon::Readline::Deinit(void)
{
    rl_callback_handler_remove();
    sDaemon = nullptr;
}

void CliDaemon::Readline::Update(MainloopContext &aContext)
{
    aContext.AddFdToSet(STDIN_FILENO, MainloopContext::kErrorFdSet | MainloopContext::kReadFdSet);
}

void CliDaemon::Readline::Process(const MainloopContext &aContext)
{
    if (FD_ISSET(STDIN_FILENO, &aContext.mErrorFdSet))
    {
        exit(EXIT_FAILURE);
    }

    if (FD_ISSET(STDIN_FILENO, &aContext.mReadFdSet))
    {
        rl_callback_read_char();
    }
}

void CliDaemon::Readline::InputCallback(char *aLine)
{
    if (aLine != nullptr)
    {
        if (!isspace(aLine[0]))
        {
            add_history(aLine);
        }

        sDaemon->ProcessLine(aLine, STDOUT_FILENO);

        free(aLine);
    }
    else
    {
        exit(EXIT_SUCCESS);
    }
}
#else
void CliDaemon::Stdio::Init(CliDaemon &aDaemon)
{
    mDaemon = &aDaemon;
}

void CliDaemon::Stdio::Deinit(void)
{
    mDaemon = nullptr;
}

void CliDaemon::Stdio::Update(MainloopContext &aContext)
{
    aContext.AddFdToSet(STDIN_FILENO, MainloopContext::kErrorFdSet | MainloopContext::kReadFdSet);
}

void CliDaemon::Stdio::Process(const MainloopContext &aContext)
{
    if (FD_ISSET(STDIN_FILENO, &aContext.mErrorFdSet))
    {
        exit(EXIT_FAILURE);
    }

    if (FD_ISSET(STDIN_FILENO, &aContext.mReadFdSet))
    {
        char buffer[kCliMaxLineLength];

        if (fgets(buffer, sizeof(buffer), stdin) != nullptr)
        {
            mDaemon->ProcessLine(buffer, STDOUT_FILENO);
        }
        else
        {
            exit(EXIT_SUCCESS);
        }
    }
}
#endif

} // namespace otbr
