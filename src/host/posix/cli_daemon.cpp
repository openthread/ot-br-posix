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
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
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

    // Keep trailing whitespace escaped with a backslash, which is part of the last argument.
    while (end > aLine && isspace(static_cast<unsigned char>(end[-1])))
    {
        size_t backslashes = 0;

        for (const char *p = end - 1; p > aLine && p[-1] == '\\'; p--)
        {
            backslashes++;
        }

        if ((backslashes & 1) != 0)
        {
            break;
        }

        end--;
    }

    *end = '\0';

    return aLine;
}

constexpr char kDefaultNetIfName[] = "wpan0";
constexpr char kSocketSuffix[]     = ".sock";
constexpr char kSocketLockSuffix[] = ".lock";

// Ends with a newline so that a truncated line still ends a line for clients (such as ot-ctl) parsing the output.
constexpr char kTruncatedMsg[] = "(truncated ...)\r\n";

// Bounds how long a client which does not read its output can block the mainloop.
constexpr struct timeval kSessionSendTimeout = {1, 0};

std::string GetSocketFilename(const std::string &aNetIfName, const char *aSuffix)
{
    char        fileName[sizeof(sockaddr_un::sun_path)];
    std::string netIfName = aNetIfName.empty() ? kDefaultNetIfName : aNetIfName;
    int         length;

    length =
        snprintf(fileName, sizeof(fileName), OTBR_CONFIG_CLI_DAEMON_SOCKET_BASENAME "%s", netIfName.c_str(), aSuffix);
    VerifyOrDie(length >= 0 && static_cast<size_t>(length) < sizeof(fileName),
                otbrErrorString(OTBR_ERROR_INVALID_ARGS));

    return fileName;
}

} // namespace

bool CliDaemon::sKeepSocketFile = false;

otbrError CliDaemon::Dependencies::InputCommandLine(const char *aLine)
{
    OTBR_UNUSED_VARIABLE(aLine);

    return OTBR_ERROR_NONE;
}

void CliDaemon::ProcessLine(char *aLine, int aOutputFd)
{
    aLine = TrimLine(aLine);
    VerifyOrExit(*aLine != '\0');

    mOutputFd = aOutputFd;

    if (mDeps.InputCommandLine(aLine) != OTBR_ERROR_NONE)
    {
        otbrLogWarning("Failed to process command line");
    }

exit:
    return;
}

int CliDaemon::OutputFormatV(const char *aFormat, va_list aArguments)
{
    int rval = 0;

    if (mOutputFd == STDOUT_FILENO)
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
    ret = OutputFormatV(aFormat, ap);
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
    size_t written = 0;
    int    rval    = -1;

    VerifyOrExit(mSessionSocket != -1);

    while (written < aLength)
    {
        ssize_t sent;

#ifdef MSG_NOSIGNAL
        // MSG_NOSIGNAL prevents send() from raising SIGPIPE in the case of a broken pipe.
        sent = send(mSessionSocket, aOutput + written, aLength - written, MSG_NOSIGNAL);
#else
        sent = write(mSessionSocket, aOutput + written, aLength - written);
#endif

        if (sent >= 0)
        {
            written += static_cast<size_t>(sent);
        }
        else if (errno != EINTR)
        {
            if (errno == EPIPE || errno == ECONNRESET)
            {
                otbrLogInfo("CLI session closed by peer");
            }
            else
            {
                otbrLogWarning("Failed to write CLI output: %s", strerror(errno));
            }

            UnixSocketSessionClear();
            ExitNow();
        }
    }

    rval = static_cast<int>(written);

exit:
    return rval;
}

void CliDaemon::UnixSocketSessionInit(void)
{
    int newSessionSocket;
    int rval = 0;

    VerifyOrExit((newSessionSocket = accept(mListenSocket, nullptr, nullptr)) != -1, rval = -1);

    VerifyOrExit((rval = fcntl(newSessionSocket, F_GETFD, 0)) != -1);
    VerifyOrExit((rval = fcntl(newSessionSocket, F_SETFD, rval | FD_CLOEXEC)) != -1);

    // BSD sockets inherit the file status flags of the listening socket on accept(), Linux sockets do not, and
    // accept(2) asks portable programs to set the flags of the new socket explicitly. Serve the session blocking
    // on every platform, as on Linux.
    VerifyOrExit((rval = fcntl(newSessionSocket, F_GETFL, 0)) != -1);
    VerifyOrExit((rval = fcntl(newSessionSocket, F_SETFL, rval & ~O_NONBLOCK)) != -1);

    VerifyOrExit((rval = setsockopt(newSessionSocket, SOL_SOCKET, SO_SNDTIMEO, &kSessionSendTimeout,
                                    sizeof(kSessionSendTimeout))) != -1);

#ifndef MSG_NOSIGNAL
    // some platforms (macOS, Solaris) don't have MSG_NOSIGNAL
    // SOME of those (macOS, but NOT Solaris) support SO_NOSIGPIPE
    // if we have SO_NOSIGPIPE, then set it. Otherwise, we're going
    // to simply ignore it.
#if defined(SO_NOSIGPIPE)
    {
        const int on = 1;

        VerifyOrExit((rval = setsockopt(newSessionSocket, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on))) != -1);
    }
#else
#warning "no support for MSG_NOSIGNAL or SO_NOSIGPIPE"
#endif
#endif // MSG_NOSIGNAL

    UnixSocketSessionClear();
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

void CliDaemon::UnixSocketSessionRead(void)
{
    static constexpr char kTooLongMessage[] = "Error 7: InvalidArgs\r\n";

    char   *line;
    char   *end;
    char   *newline;
    ssize_t received;

    // Always keep 1 byte for the null terminator. A full buffer never survives the previous call, so at least 1 byte
    // of space is available here.
    received =
        read(mSessionSocket, mSessionBuffer + mSessionBufferLength, sizeof(mSessionBuffer) - 1 - mSessionBufferLength);

    if (received == 0)
    {
        otbrLogInfo("Session socket closed by peer");

        // Process a trailing command sent without a newline before the peer shut down its sending side.
        if (mSessionBufferLength > 0 && !mSessionDiscarding)
        {
            mSessionBuffer[mSessionBufferLength] = '\0';
            ProcessLine(mSessionBuffer, mSessionSocket);
        }

        UnixSocketSessionClear();
        ExitNow();
    }

    if (received < 0)
    {
        VerifyOrExit(errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK);
        otbrLogWarning("CLI Daemon read: %s", strerror(errno));
        UnixSocketSessionClear();
        ExitNow();
    }

    mSessionBufferLength += static_cast<size_t>(received);

    line = mSessionBuffer;
    end  = mSessionBuffer + mSessionBufferLength;

    // Process each complete line as a separate command.
    while ((newline = static_cast<char *>(memchr(line, '\n', static_cast<size_t>(end - line)))) != nullptr)
    {
        *newline = '\0';

        if (mSessionDiscarding)
        {
            // This is the tail of a line which was too long, drop it.
            mSessionDiscarding = false;
        }
        else
        {
            ProcessLine(line, mSessionSocket);
        }

        // Writing the output may have failed and closed the session.
        VerifyOrExit(mSessionSocket != -1);

        line = newline + 1;
    }

    // Keep the partial line for the next read.
    mSessionBufferLength = static_cast<size_t>(end - line);
    memmove(mSessionBuffer, line, mSessionBufferLength);

    if (mSessionBufferLength == sizeof(mSessionBuffer) - 1)
    {
        // The buffer is full without a newline. Reject the command in the CLI error format (which also ends a
        // non-interactive ot-ctl) instead of running a truncated one, and drop the rest of the line.
        if (!mSessionDiscarding)
        {
            otbrLogWarning("CLI command line is too long, dropping it");
            UnixSocketOutput(kTooLongMessage, sizeof(kTooLongMessage) - 1);
            mSessionDiscarding = true;
        }

        mSessionBufferLength = 0;
    }

exit:
    return;
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
        std::string        socketFile = GetSocketFilename(aNetIfName, kSocketSuffix);

        sockname.sun_family = AF_UNIX;
        strncpy(sockname.sun_path, socketFile.c_str(), sizeof(sockname.sun_path) - 1);
        (void)unlink(sockname.sun_path);

        ret = bind(mListenSocket, reinterpret_cast<const struct sockaddr *>(&sockname), sizeof(struct sockaddr_un));

        if (ret != -1)
        {
            mSocketFilename = socketFile;
        }
    }

    if (ret == -1)
    {
        otbrLogErr("bind daemon socket: %s", strerror(errno));
        ExitNow(error = OTBR_ERROR_ERRNO);
    }

exit:
    return error;
}

CliDaemon::CliDaemon(Dependencies &aDependencies, uint8_t aMode)
    : mConsoleActive(false)
    , mListenSocket(-1)
    , mDaemonLock(-1)
    , mSessionSocket(-1)
    , mOutputFd(-1)
    , mSessionBufferLength(0)
    , mSessionDiscarding(false)
    , mDaemonMode(aMode)
    , mDeps(aDependencies)
{
}

CliDaemon::~CliDaemon(void)
{
    Deinit();
}

otbrError CliDaemon::Init(const std::string &aNetIfName)
{
    otbrError error = OTBR_ERROR_NONE;

    // Init() is called again on a software reset of the RCP host. Keep the existing session, unless the Thread
    // interface got another name (e.g. a kernel-assigned utun on macOS).
    if (mListenSocket != -1 && mSocketFilename != GetSocketFilename(aNetIfName, kSocketSuffix))
    {
        UnixSocketClose();
    }

    if ((mDaemonMode & OTBR_DAEMON_MODE_UNIX_SOCKET) && mListenSocket == -1)
    {
        SuccessOrExit(error = UnixSocketCreate(aNetIfName));

        //
        // only accept 1 connection.
        //
        VerifyOrExit(listen(mListenSocket, 1) != -1, error = OTBR_ERROR_ERRNO);
    }

    if ((mDaemonMode & OTBR_DAEMON_MODE_CONSOLE) && !mConsoleActive)
    {
        mConsole.Init(*this);
        mConsoleActive = true;

        // Show the console the output which is not a response to a command, such as the initial prompt.
        mOutputFd = STDOUT_FILENO;
    }

exit:
    if (error != OTBR_ERROR_NONE)
    {
        Deinit();
    }

    return error;
}

void CliDaemon::Deinit(void)
{
    StopConsole();
    UnixSocketClose();

    mOutputFd = -1;
}

void CliDaemon::UnixSocketClose(void)
{
    UnixSocketSessionClear();

    if (mListenSocket != -1)
    {
        close(mListenSocket);
        mListenSocket = -1;
    }

    if (!mSocketFilename.empty() && !sKeepSocketFile)
    {
        (void)unlink(mSocketFilename.c_str());
    }

    mSocketFilename.clear();

    if (mDaemonLock != -1)
    {
        (void)flock(mDaemonLock, LOCK_UN);
        close(mDaemonLock);
        mDaemonLock = -1;
    }
}

void CliDaemon::StopConsole(void)
{
    VerifyOrExit(mConsoleActive);

    mConsole.Deinit();
    mConsoleActive = false;

    if (mOutputFd == STDOUT_FILENO)
    {
        mOutputFd = -1;
    }

exit:
    return;
}

void CliDaemon::HandleConsoleClosed(void)
{
    otbrLogInfo("CLI console closed");

    StopConsole();

    // Without another CLI transport, closing the console terminates the agent, which then tears down cleanly.
    if (mListenSocket == -1)
    {
        raise(SIGTERM);
    }
}

void CliDaemon::UnixSocketSessionClear(void)
{
    if (mSessionSocket != -1)
    {
        close(mSessionSocket);

        // Do not send the remaining output of a pending command to the next session.
        if (mOutputFd == mSessionSocket)
        {
            mOutputFd = mConsoleActive ? STDOUT_FILENO : -1;
        }

        mSessionSocket = -1;
    }

    mSessionBufferLength = 0;
    mSessionDiscarding   = false;
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

    if (mConsoleActive)
    {
        mConsole.Update(aContext);
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
            UnixSocketSessionRead();
        }
    }

    if (mConsoleActive)
    {
        mConsole.Process(aContext);
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

    // The OpenThread CLI outputs the prompt itself.
    rl_callback_handler_install("", InputCallback);
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
        sDaemon->HandleConsoleClosed();
    }
    else if (FD_ISSET(STDIN_FILENO, &aContext.mReadFdSet))
    {
        rl_callback_read_char();
    }
}

void CliDaemon::Readline::InputCallback(char *aLine)
{
    if (aLine != nullptr)
    {
        if (aLine[0] != '\0' && !isspace(static_cast<unsigned char>(aLine[0])))
        {
            add_history(aLine);
        }

        sDaemon->ProcessLine(aLine, STDOUT_FILENO);

        free(aLine);
    }
    else
    {
        sDaemon->HandleConsoleClosed();
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
        mDaemon->HandleConsoleClosed();
    }
    else if (FD_ISSET(STDIN_FILENO, &aContext.mReadFdSet))
    {
        char buffer[kCliMaxLineLength];

        if (fgets(buffer, sizeof(buffer), stdin) != nullptr)
        {
            mDaemon->ProcessLine(buffer, STDOUT_FILENO);
        }
        else
        {
            mDaemon->HandleConsoleClosed();
        }
    }
}
#endif

} // namespace otbr
